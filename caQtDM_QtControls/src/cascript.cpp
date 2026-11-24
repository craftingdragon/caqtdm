/*
 *  This file is part of the caQtDM Framework, developed at the Paul Scherrer Institut,
 *  Villigen, Switzerland
 *
 *  The caQtDM Framework is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  The caQtDM Framework is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the caQtDM Framework.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (c) 2026
 *
 *  Author:
 *    Julian Houba
 *  Contact details:
 *    julian.houba@psi.ch
 */

#include "cascript.h"

// The Apple compatibility macro conflicts with std::isnan in QtQml headers.
#ifdef isnan
#undef isnan
#endif

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJSEngine>
#include <QMetaEnum>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QThread>
#include <QPainter>
#include <QApplication>
#include <QTimer>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <cmath>
#include <limits>

namespace {
using ScriptClock = std::chrono::steady_clock;
constexpr int maxWaveformSamples = 1024 * 1024;
constexpr auto warningAfter = std::chrono::milliseconds(100);
constexpr auto timeoutAfter = std::chrono::milliseconds(1000);

// One thread services deadlines for all widgets. A worker unregisters its engine
// under the same lock used to interrupt it, before it can finish or be destroyed.
class CaScriptWatchdog
{
public:
    CaScriptWatchdog() : m_thread([this]() { watch(); }) {}

    ~CaScriptWatchdog()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_changed.notify_one();
        m_thread.join();
    }

    static CaScriptWatchdog &instance()
    {
        static CaScriptWatchdog watchdog;
        return watchdog;
    }

    void arm(ScriptClock::time_point deadline, QJSEngine *engine)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_deadlines.emplace(deadline, engine);
        m_changed.notify_one();
    }

    void disarm(ScriptClock::time_point deadline, QJSEngine *engine)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto range = m_deadlines.equal_range(deadline);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == engine) {
                m_deadlines.erase(it);
                m_changed.notify_one();
                break;
            }
        }
    }

private:
    void watch()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_stopping) {
            if (m_deadlines.empty()) {
                m_changed.wait(lock);
                continue;
            }
            const auto deadline = m_deadlines.begin()->first;
            if (ScriptClock::now() < deadline) {
                m_changed.wait_until(lock, deadline);
                continue;
            }
            m_deadlines.begin()->second->setInterrupted(true);
            m_deadlines.erase(m_deadlines.begin());
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_changed;
    std::multimap<ScriptClock::time_point, QJSEngine *> m_deadlines;
    bool m_stopping = false;
    std::thread m_thread;
};

QJSValue newDictionary(QJSEngine *engine)
{
    QJSValue dictionary = engine->newObject();
    dictionary.setPrototype(QJSValue(QJSValue::NullValue));
    return dictionary;
}

QJSValue inputValueToScript(QJSEngine *engine, const QVariant &value, ScriptClock::time_point deadline)
{
    if (value.userType() != QMetaType::QVariantList && value.userType() != QMetaType::QStringList)
        return engine->toScriptValue(value);
    const QVariantList elements = value.toList();
    QJSValue array = engine->newArray(elements.size());
    for (int i = 0; i < elements.size(); ++i) {
        if ((i % 256) == 0 && ScriptClock::now() >= deadline) break;
        array.setProperty(i, inputValueToScript(engine, elements.at(i), deadline));
    }
    return array;
}

QVariantMap scalarConversion(const QVariant &value, caScript::EpicsType type)
{
    const auto failure = [](const QString &message) {
        return QVariantMap{{QStringLiteral("error"), message}};
    };
    if (type == caScript::String && value.userType() == QMetaType::QString) {
        return {{QStringLiteral("value"), value}};
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok) return failure(QStringLiteral("Input cannot be converted to a number"));
    QVariant result;
    switch (type) {
    case caScript::String:
        result = QString::number(number, 'g', 17);
        break;
    case caScript::Double:
        result = number;
        break;
    case caScript::Float:
        if (qIsFinite(number) && qAbs(number) > std::numeric_limits<float>::max()) {
            return failure(QStringLiteral("Input exceeds the float range"));
        }
        result = double(float(number));
        break;
    default: {
        if (!qIsFinite(number)) return failure(QStringLiteral("Integer conversion requires a finite number"));
        double minimum;
        double maximum;
        switch (type) {
        case caScript::Short: minimum = -32768; maximum = 32767; break;
        case caScript::Long: minimum = -2147483648.0; maximum = 2147483647; break;
        case caScript::Char: minimum = 0; maximum = 255; break;
        case caScript::Enum: minimum = 0; maximum = 65535; break;
        default: return failure(QStringLiteral("Unknown EPICS conversion type"));
        }
        const double rounded = std::round(number);
        if (rounded < minimum || rounded > maximum) {
            return failure(QStringLiteral("Input exceeds the target range"));
        }
        result = double(std::llround(number));
        break;
    }
    }
    return {{QStringLiteral("value"), result}};
}
}

class CaScriptConsole : public QObject
{
    Q_OBJECT

public:
    explicit CaScriptConsole(QObject *parent) : QObject(parent) {}
    void reset() { m_messageCount = 0; }

public slots:
    void log(const QString &text) { send(QtInfoMsg, text); }
    void warn(const QString &text) { send(QtWarningMsg, text); }
    void error(const QString &text) { send(QtCriticalMsg, text); }

signals:
    void message(int level, const QString &text);

private:
    void send(int level, const QString &text)
    {
        // Bound queued GUI messages before they leave the worker thread.
        if (m_messageCount < 100) {
            ++m_messageCount;
            emit message(level, text);
        } else if (m_messageCount == 100) {
            ++m_messageCount;
            emit message(QtWarningMsg, tr("Console message limit (100 per execution) reached; further console messages suppressed."));
        }
    }

    int m_messageCount = 0;
};

class CaScriptWorker : public QObject
{
    Q_OBJECT

public:
    CaScriptWorker() : m_console(new CaScriptConsole(this))
    {
        connect(m_console, &CaScriptConsole::message, this, &CaScriptWorker::message);
    }

    bool executionIsSlow()
    {
        QMutexLocker lock(&m_mutex);
        return m_active && ScriptClock::now() - m_started >= warningAfter;
    }

    void stop()
    {
        QMutexLocker lock(&m_mutex);
        m_stopping = true;
        if (m_engine) {
            m_engine->setInterrupted(true);
        }
    }

public slots:
    void execute(const QString &script, const QVariantMap &inputs, const QVariantMap &outputs)
    {
        const auto started = ScriptClock::now();
        const auto deadline = started + timeoutAfter;
        {
            QMutexLocker lock(&m_mutex);
            if (m_stopping) return;
            if (!m_engine) m_engine = new QJSEngine(this);
            m_engine->setInterrupted(false);
            m_started = started;
            m_active = true;
        }
        auto &watchdog = CaScriptWatchdog::instance();
        watchdog.arm(deadline, m_engine);
        const auto complete = [this, &watchdog, started, deadline](bool success, const QVariantMap &values,
                                                                  const QString &diagnostic) {
            const auto completed = ScriptClock::now();
            watchdog.disarm(deadline, m_engine);
            {
                QMutexLocker lock(&m_mutex);
                m_active = false;
            }
            const bool timedOut = completed >= deadline;
            emit finished(success && !timedOut, timedOut ? QVariantMap() : values, diagnostic,
                          timedOut, completed - started >= warningAfter);
        };
        if (!m_runScript.isCallable()) initializeBindings();
        m_console->reset();
        const QJSValue console = m_consoleFactory.call({m_engine->newQObject(m_console)});
        QJSValue types = newDictionary(m_engine);
        const QMetaEnum typeEnum = QMetaEnum::fromType<caScript::EpicsType>();
        for (int i = 0; i < typeEnum.keyCount(); ++i) {
            types.setProperty(QString::fromLatin1(typeEnum.key(i)), typeEnum.value(i));
        }
        QJSValue scriptInputs = newDictionary(m_engine);
        for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
            if (ScriptClock::now() >= deadline) {
                complete(false, {}, {});
                return;
            }
            const QVariantMap channel = it.value().toMap();
            QJSValue scriptChannel = newDictionary(m_engine);
            for (auto property = channel.constBegin(); property != channel.constEnd(); ++property) {
                scriptChannel.setProperty(property.key(), inputValueToScript(m_engine, property.value(), deadline));
            }
            scriptInputs.setProperty(it.key(), scriptChannel);
            if (channel.contains(QStringLiteral("value"))) {
                // Convert the snapshot in C++; expose only the captured results to JavaScript.
                QVariantMap conversions;
                for (int i = 0; i < typeEnum.keyCount(); ++i) {
                    const auto type = static_cast<caScript::EpicsType>(typeEnum.value(i));
                    conversions.insert(QString::number(type), scalarConversion(channel.value(QStringLiteral("value")), type));
                }
                scriptChannel.setProperty(QStringLiteral("convert"),
                                          m_conversionFunction.call({m_engine->toScriptValue(conversions)}));
            }
            if (!channel.contains(QStringLiteral("rawBytes"))) continue;
            const QByteArray bytes = channel.value(QStringLiteral("rawBytes")).toByteArray();
            QJSValue array = m_engine->newArray(bytes.size());
            for (int i = 0; i < bytes.size(); ++i) {
                if ((i % 256) == 0 && ScriptClock::now() >= deadline) {
                    complete(false, {}, {});
                    return;
                }
                array.setProperty(i, static_cast<unsigned char>(bytes.at(i)));
            }
            scriptChannel.setProperty(QStringLiteral("rawBytes"), m_byteArrayConstructor.callAsConstructor({array}));
            // Decode the original snapshot in C++, independently of the displayed value.
            scriptChannel.setProperty(QStringLiteral("fromLatin1"),
                                      m_decodedTextFunction.call({QJSValue(QString::fromLatin1(bytes))}));
            scriptChannel.setProperty(QStringLiteral("fromUtf8"),
                                      m_decodedTextFunction.call({QJSValue(QString::fromUtf8(bytes))}));
        }
        QJSValue scriptOutputs = newDictionary(m_engine);
        QJSValue keys = m_engine->newArray(outputs.size());
        int index = 0;
        for (auto it = outputs.constBegin(); it != outputs.constEnd(); ++it) {
            keys.setProperty(index++, it.key());
            scriptOutputs.setProperty(it.key(), newDictionary(m_engine));
        }
        const QJSValue function = m_engine->evaluate(
            QStringLiteral("(function(inputs, outputs, console, EpicsType) {\n") + script
                + QStringLiteral("\n})"),
            QStringLiteral("caScript"), 0);
        const QJSValue result = function.isCallable()
            ? m_runScript.call({function, scriptInputs, scriptOutputs, console, types, keys}) : function;
        const bool success = !result.isError() && result.property(QStringLiteral("success")).toBool();
        QString diagnostic;
        if (!success) {
            diagnostic = result.isError() ? result.toString() : result.property(QStringLiteral("message")).toString();
            if (diagnostic.isEmpty()) diagnostic = tr("JavaScript execution failed.");
            const int line = result.property(QStringLiteral("lineNumber")).toInt();
            if (line > 0) diagnostic = tr("%1 (line %2)").arg(diagnostic).arg(line);
        }
        QVariantMap values;
        if (success) {
            const QJSValue scriptValues = result.property(QStringLiteral("values"));
            for (auto it = outputs.constBegin(); it != outputs.constEnd() && ScriptClock::now() < deadline; ++it) {
                if (!scriptValues.hasOwnProperty(it.key())) continue;
                const QJSValue output = scriptValues.property(it.key());
                const bool scalar = output.isBool() || output.isNumber() || output.isString();
                QVariant value;
                if (scalar) {
                    value = output.toVariant();
                } else if (output.isArray()) {
                    const QJSValue snapshot = m_waveformSnapshot.call({output, QJSValue(maxWaveformSamples)});
                    if (snapshot.isObject() && !snapshot.isError()) {
                        const int count = snapshot.property(QStringLiteral("length")).toInt();
                        QVector<double> samples;
                        samples.reserve(count);
                        for (int i = 0; i < count && ScriptClock::now() < deadline; ++i)
                            samples.append(snapshot.property(static_cast<quint32>(i)).toNumber());
                        if (samples.size() == count) value = QVariant::fromValue(samples);
                    }
                }
                values.insert(it.key(), value);
            }
        }
        complete(success, values, diagnostic);
    }

signals:
    void finished(bool success, const QVariantMap &values, const QString &diagnostic,
                  bool timedOut, bool slow);
    void message(int level, const QString &text);

private:
    void initializeBindings()
    {
        // Capture the host's built-ins before user code can replace them.
        m_byteArrayConstructor = m_engine->globalObject().property(QStringLiteral("Uint8Array"));
        m_consoleFactory = m_engine->evaluate(QStringLiteral(
            "(function(asString) { return function(sink) {"
            "function text(args) { let result = ''; for (let i = 0; i < args.length; ++i) {"
            "if (i) result += ' '; result += asString(args[i]); } return result; }"
            "return {log: function() { sink.log(text(arguments)); },"
            "warn: function() { sink.warn(text(arguments)); },"
            "error: function() { sink.error(text(arguments)); }}; }; })(String)"));
        m_decodedTextFunction = m_engine->evaluate(QStringLiteral(
            "(function(text) { return function() { return text; }; })"));
        m_conversionFunction = m_engine->evaluate(QStringLiteral(
            "(function(hasOwn, unknown, range) { return function(results) { return function(type) {\n"
            "if (typeof type !== 'number' || !hasOwn(results, type))\n"
            "    throw new unknown('Unknown EPICS conversion type');\n"
            "const result = results[type];\n"
            "if (hasOwn(result, 'error')) throw new range(result.error);\n"
            "return result.value; }; }; })"
            "(Function.prototype.call.bind(Object.prototype.hasOwnProperty), TypeError, RangeError)"));
        m_waveformSnapshot = m_engine->evaluate(QStringLiteral(
            "(function(create, descriptor, hasOwn, isFinite) { return function(array, maximum) {"
            "try {"
            "const length = array.length;"
            "if (typeof length !== 'number' || length !== (length | 0) || length < 1 || length > maximum) return;"
            "const snapshot = create(null); snapshot.length = length;"
            "for (let i = 0; i < length; ++i) {"
            "const element = descriptor(array, i);"
            "if (!element || !hasOwn(element, 'value') || !isFinite(element.value)) return;"
            "snapshot[i] = element.value;"
            "} return snapshot;"
            "} catch (ignored) {}"
            "}; })(Object.create, Object.getOwnPropertyDescriptor,"
            "Function.prototype.call.bind(Object.prototype.hasOwnProperty), Number.isFinite)"));
        m_runScript = m_engine->evaluate(QStringLiteral(
            "(function(create, hasOwn, asString) {"
            "return function(script, inputs, outputs, console, types, keys) {"
            "try { script(inputs, outputs, console, types);"
            "const values = create(null);"
            "for (let i = 0; i < keys.length; ++i) {"
            "const key = keys[i]; const channel = outputs[key];"
            "if (channel && hasOwn(channel, 'value')) values[key] = channel.value; }"
            "return {success: true, values: values};"
            "} catch (error) { var message = 'JavaScript exception'; var line = 0;"
            "try { message = asString(error);"
            "if (error != null && typeof error.lineNumber === 'number') line = error.lineNumber;"
            "} catch (ignored) {}"
            "return {success: false, message: message, lineNumber: line}; } }; })"
            "(Object.create, Function.prototype.call.bind(Object.prototype.hasOwnProperty), String)"));
    }

    CaScriptConsole *m_console;
    QMutex m_mutex;
    QJSEngine *m_engine = nullptr;
    QJSValue m_runScript;
    QJSValue m_waveformSnapshot;
    QJSValue m_consoleFactory;
    QJSValue m_decodedTextFunction;
    QJSValue m_conversionFunction;
    QJSValue m_byteArrayConstructor;
    bool m_stopping = false;
    ScriptClock::time_point m_started;
    bool m_active = false;
};

caScript::caScript(QWidget *parent) : QWidget(parent), m_triggerTimer(new QTimer(this)),
    m_warningTimer(new QTimer(this))
{
    connect(m_triggerTimer, &QTimer::timeout, this, &caScript::executeScript);
    connect(this, &caScript::inputValueUpdated, this, &caScript::handleInputTrigger);
    m_warningTimer->setSingleShot(true);
    m_warningTimer->setTimerType(Qt::PreciseTimer);
    connect(m_warningTimer, &QTimer::timeout, this, [this]() {
        if (m_worker->executionIsSlow()) {
            m_slowWarningEmitted = true;
            emit scriptWarning(tr("Script execution exceeded 100 ms."));
        }
    });
}

caScript::~caScript()
{
    m_triggerTimer->stop();
    m_warningTimer->stop();
    if (m_executionThread) {
        m_worker->stop();
        m_executionThread->quit();
        m_executionThread->wait();
    }
}

void caScript::executeScript()
{
    if (m_executionDisabled) return;
    if (m_running) {
        emit scriptWarning(tr("Trigger ignored: script is already running."));
        return;
    }
    const QStringList errors = inputConfigurationErrors() + outputConfigurationErrors()
                               + triggerConfigurationErrors();
    if (!errors.isEmpty()) {
        m_running = true;
        QTimer::singleShot(0, this, [this, errors]() {
            for (const QString &error : errors) emit scriptError(error);
            m_running = false;
            emit scriptFinished(false);
        });
        return;
    }

    if (m_executionMode == DataDriven) {
        for (int i = 0; i < m_inputChannels.size(); ++i) {
            if (!m_readyInputs.contains(i)) return;
        }
    }
    m_running = true;

    if (!m_executionThread) {
        m_executionThread = new QThread(this);
        m_worker = new CaScriptWorker;
        m_worker->moveToThread(m_executionThread);
        connect(m_executionThread, &QThread::finished, m_worker, &QObject::deleteLater);
        connect(m_worker, &CaScriptWorker::message, this, [this](int level, const QString &text) {
            if (level == QtWarningMsg) emit scriptWarning(text);
            else if (level == QtCriticalMsg) emit scriptError(text);
            else emit scriptInfo(text);
        });
        connect(m_worker, &CaScriptWorker::finished, this, [this](bool success, const QVariantMap &values,
                                                              const QString &diagnostic,
                                                              bool timedOut, bool slow) {
            m_warningTimer->stop();
            if (slow && !m_slowWarningEmitted) emit scriptWarning(tr("Script execution exceeded 100 ms."));
            if (timedOut) {
                emit scriptError(tr("Script execution interrupted after the 1000 ms time limit."));
                if (++m_consecutiveTimeouts >= 5) {
                    m_executionDisabled = true;
                    m_triggerTimer->stop();
                    emit scriptError(tr("Script disabled after 5 consecutive timeouts. Reload the panel to enable it again."));
                }
            } else {
                m_consecutiveTimeouts = 0;
                if (!success) emit scriptError(diagnostic);
            }
            m_running = false;
            if (success && !values.isEmpty()) emit outputValuesReady(values);
            emit scriptFinished(success);
        });
        m_executionThread->start();
    }
    QVariantMap snapshot;
    for (int i = 0; i < m_inputChannels.size(); ++i) {
        const InputChannel &input = m_inputChannels.at(i);
        QVariantMap channel = m_inputMetadata.value(i);
        if (m_inputValues.contains(i)) {
            channel.insert(QStringLiteral("value"), m_inputValues.value(i));
        }
        if (m_inputRawBytes.contains(i)) {
            channel.insert(QStringLiteral("rawBytes"), m_inputRawBytes.value(i));
        }
        snapshot.insert(input.alias.isEmpty() ? input.channel : input.alias, channel);
    }
    QVariantMap outputs;
    for (const InputChannel &output : m_outputChannels) {
        outputs.insert(output.alias.isEmpty() ? output.channel : output.alias, QVariantMap());
    }
    m_slowWarningEmitted = false;
    m_lastExecution.start();
    m_warningTimer->start(100);
    QMetaObject::invokeMethod(m_worker, "execute", Qt::QueuedConnection,
                              Q_ARG(QString, m_script), Q_ARG(QVariantMap, snapshot),
                              Q_ARG(QVariantMap, outputs));
}

void caScript::updateInputSample(int index, const QVariantMap &metadata, const QVariant &value,
                                 const QVariant &rawBytes)
{
    if (index < 0 || index >= m_inputChannels.size()) return;
    updateInputMetadata(index, metadata);
    if (!metadata.value(QStringLiteral("connected")).toBool()) return;

    if (rawBytes.isValid()) m_inputRawBytes.insert(index, rawBytes.toByteArray());
    else m_inputRawBytes.remove(index);
    if (value.isValid()) {
        updateInputValue(index, value);
    } else {
        m_inputValues.remove(index);
        m_readyInputs.remove(index);
    }
}

void caScript::updateInputValue(int index, const QVariant &value)
{
    if (index >= 0 && index < m_inputChannels.size()) {
        m_inputValues.insert(index, value);
        if (value.isValid() && m_inputMetadata.value(index).value(QStringLiteral("connected")).toBool()) {
            m_readyInputs.insert(index);
        } else {
            m_readyInputs.remove(index);
        }
        emit inputValueUpdated(index);
    }
}

void caScript::updateInputRawBytes(int index, const QByteArray &bytes)
{
    if (index >= 0 && index < m_inputChannels.size()) {
        m_inputRawBytes.insert(index, bytes);
    }
}

void caScript::updateInputMetadata(int index, const QVariantMap &metadata)
{
    if (index >= 0 && index < m_inputChannels.size()) {
        m_inputMetadata.insert(index, metadata);
        if (!metadata.value(QStringLiteral("connected")).toBool()) {
            m_readyInputs.remove(index);
        }
    }
}

bool caScript::getShow() const
{
    return m_show;
}

void caScript::setShow(bool show)
{
    m_show = show;
    update();
}

QString caScript::getLastOutputText() const
{
    return m_lastOutputText;
}

void caScript::updateLastOutput(const QString &channel, const QVariant &value)
{
    m_lastOutputText = channel + QStringLiteral(" = ") + value.toString();
    update();
}

void caScript::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    const bool designer = qApp->property("APP_SOURCE").toString() == QStringLiteral("DESIGNER");
    if (!designer && !m_show) return;
    QPainter painter(this);
    painter.fillRect(rect(), designer ? QColor(Qt::blue) : palette().color(QPalette::Window));
    painter.setPen(designer ? QColor(Qt::white) : palette().color(QPalette::WindowText));
    const QString text = designer ? objectName() : m_lastOutputText;
    painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void caScript::startTriggers()
{
    m_triggersStarted = true;
    refreshTriggers();
}

void caScript::refreshTriggers()
{
    if (!m_triggersStarted) return;
    m_triggerTimer->stop();
    if (m_executionDisabled) return;
    const QStringList errors = triggerConfigurationErrors();
    for (const QString &error : errors) emit scriptError(error);
    if (errors.isEmpty() && m_triggerMode == Periodic) {
        m_triggerTimer->start(m_triggerInterval);
    }
}

void caScript::handleInputTrigger(int index)
{
    if (!m_triggersStarted || !triggerConfigurationErrors().isEmpty()) return;
    switch (m_triggerMode) {
    case AnyInput:
        break;
    case SelectedInput: {
        const InputChannel &input = m_inputChannels.at(index);
        if ((input.alias.isEmpty() ? input.channel : input.alias) != m_triggerInput) return;
        break;
    }
    case MinimumInterval:
        if (!m_running && m_lastExecution.isValid() && m_lastExecution.elapsed() < m_triggerInterval) return;
        break;
    default:
        return;
    }
    executeScript();
}

caScript::TriggerMode caScript::getTriggerMode() const
{
    return m_triggerMode;
}

void caScript::setTriggerMode(TriggerMode mode)
{
    m_triggerMode = mode;
    m_lastExecution.invalidate();
    refreshTriggers();
}

QString caScript::getTriggerInput() const
{
    return m_triggerInput;
}

void caScript::setTriggerInput(const QString &input)
{
    m_triggerInput = input;
    refreshTriggers();
}

int caScript::getTriggerInterval() const
{
    return m_triggerInterval;
}

void caScript::setTriggerInterval(int milliseconds)
{
    m_triggerInterval = milliseconds;
    refreshTriggers();
}

QStringList caScript::triggerConfigurationErrors() const
{
    if ((m_triggerMode == Periodic || m_triggerMode == MinimumInterval) && m_triggerInterval <= 0) {
        return {tr("Trigger interval must be greater than zero milliseconds.")};
    }
    if (m_triggerMode == SelectedInput) {
        for (const InputChannel &input : m_inputChannels) {
            if ((input.alias.isEmpty() ? input.channel : input.alias) == m_triggerInput) return {};
        }
        return {tr("Trigger input must match a configured input alias or unaliased channel name.")};
    }
    return {};
}

caScript::ExecutionMode caScript::getExecutionMode() const
{
    return m_executionMode;
}

void caScript::setExecutionMode(ExecutionMode mode)
{
    m_executionMode = mode;
}

QString caScript::getScript() const
{
    return m_script;
}

void caScript::setScript(const QString &script)
{
    m_script = script;
}

QString caScript::getChannels() const
{
    QJsonArray channels;
    for (const InputChannel &input : m_inputChannels) {
        QJsonObject entry;
        entry.insert(QStringLiteral("channel"), input.channel);
        entry.insert(QStringLiteral("alias"), input.alias);
        channels.append(entry);
    }
    return QString::fromUtf8(QJsonDocument(channels).toJson(QJsonDocument::Compact));
}

void caScript::setChannels(const QString &json)
{
    if (!channelBindingsEditable()) return;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        m_inputJsonError = tr("Invalid input channel JSON: %1").arg(error.errorString());
        return;
    }
    if (!document.isArray()) {
        m_inputJsonError = tr("Input channel JSON must be an array.");
        return;
    }

    QList<InputChannel> inputs;
    const QJsonArray entries = document.array();
    for (int i = 0; i < entries.size(); ++i) {
        const QJsonValue entry = entries.at(i);
        const QJsonObject object = entry.toObject();
        const QJsonValue channel = object.value(QStringLiteral("channel"));
        const QJsonValue alias = object.value(QStringLiteral("alias"));
        if (!entry.isObject() || !channel.isString()
            || (!alias.isUndefined() && !alias.isString())) {
            m_inputJsonError = tr("Input entry %1 requires a channel string and an optional alias string.").arg(i + 1);
            return;
        }
        inputs.append({channel.toString(), alias.toString()});
    }
    setInputChannels(inputs);
}

QList<caScript::InputChannel> caScript::getInputChannels() const
{
    return m_inputChannels;
}

void caScript::setInputChannels(const QList<InputChannel> &inputs)
{
    if (!channelBindingsEditable()) return;
    m_inputChannels = inputs;
    m_inputValues.clear();
    m_readyInputs.clear();
    m_inputRawBytes.clear();
    m_inputMetadata.clear();
    m_inputJsonError.clear();
    refreshTriggers();
}

QStringList caScript::inputConfigurationErrors() const
{
    QStringList errors = validateInputChannels(m_inputChannels);
    if (!m_inputJsonError.isEmpty()) {
        errors.prepend(m_inputJsonError);
    }
    return errors;
}

QString caScript::getOutputChannelsJson() const
{
    QJsonArray channels;
    for (const InputChannel &output : m_outputChannels) {
        QJsonObject entry;
        entry.insert(QStringLiteral("channel"), output.channel);
        entry.insert(QStringLiteral("alias"), output.alias);
        channels.append(entry);
    }
    return QString::fromUtf8(QJsonDocument(channels).toJson(QJsonDocument::Compact));
}

void caScript::setOutputChannelsJson(const QString &json)
{
    if (!channelBindingsEditable()) return;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        m_outputJsonError = tr("Output channels require a JSON array.");
        return;
    }
    QList<InputChannel> outputs;
    for (const QJsonValue &entry : document.array()) {
        const QJsonObject object = entry.toObject();
        if (!entry.isObject() || !object.value(QStringLiteral("channel")).isString()
            || (object.contains(QStringLiteral("alias")) && !object.value(QStringLiteral("alias")).isString())) {
            m_outputJsonError = tr("Output entries require a channel string and an optional alias string.");
            return;
        }
        outputs.append({object.value(QStringLiteral("channel")).toString(),
                        object.value(QStringLiteral("alias")).toString()});
    }
    setOutputChannels(outputs);
}

QList<caScript::InputChannel> caScript::getOutputChannels() const
{
    return m_outputChannels;
}

void caScript::setOutputChannels(const QList<InputChannel> &outputs)
{
    if (!channelBindingsEditable()) return;
    m_outputChannels = outputs;
    m_outputJsonError.clear();
}

QStringList caScript::outputConfigurationErrors() const
{
    QStringList errors = validateInputChannels(m_outputChannels);
    for (QString &error : errors) error.prepend(tr("Output: "));
    if (!m_outputJsonError.isEmpty()) errors.prepend(m_outputJsonError);
    return errors;
}

void caScript::freezeChannelBindings()
{
    m_channelBindingsFrozen = true;
}

bool caScript::channelBindingsEditable()
{
    if (!m_channelBindingsFrozen) return true;
    emit scriptWarning(tr("Channel bindings are fixed for this panel. Reload it to apply changes."));
    return false;
}

#include "cascript.moc"

QStringList caScript::validateInputChannels(const QList<InputChannel> &inputs)
{
    QStringList errors;
    QSet<QString> keys;

    for (const InputChannel &input : inputs) {
        if (input.channel.isEmpty()) {
            errors.append(tr("Input channel for alias \"%1\" is empty.").arg(input.alias));
            continue;
        }

        const QString key = input.alias.isEmpty() ? input.channel : input.alias;
        if (keys.contains(key)) {
            errors.append(tr("Duplicate input key \"%1\".").arg(key));
        } else {
            keys.insert(key);
        }
    }

    return errors;
}
