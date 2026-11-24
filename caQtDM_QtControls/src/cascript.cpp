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
#include <cmath>
#include <limits>

namespace {
QJSValue inputValueToScript(QJSEngine *engine, const QVariant &value)
{
    if (value.userType() != QMetaType::QVariantList && value.userType() != QMetaType::QStringList)
        return engine->toScriptValue(value);
    const QVariantList elements = value.toList();
    QJSValue array = engine->newArray(elements.size());
    for (int i = 0; i < elements.size(); ++i) {
        array.setProperty(i, inputValueToScript(engine, elements.at(i)));
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

    void prepareExecution()
    {
        QMutexLocker lock(&m_mutex);
        m_interruptRequested = false;
        if (m_engine) m_engine->setInterrupted(false);
    }

    void interrupt()
    {
        QMutexLocker lock(&m_mutex);
        m_interruptRequested = true;
        if (m_engine) m_engine->setInterrupted(true);
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
        {
            QMutexLocker lock(&m_mutex);
            if (m_stopping) return;
            if (!m_engine) m_engine = new QJSEngine(this);
            m_engine->setInterrupted(m_interruptRequested);
        }
        m_console->reset();
        QJSValue consoleFactory = m_engine->evaluate(QStringLiteral(
            "(function(sink) { function text(args) { return Array.prototype.map.call(args, String).join(' '); }"
            "return {log: function() { sink.log(text(arguments)); },"
            "warn: function() { sink.warn(text(arguments)); },"
            "error: function() { sink.error(text(arguments)); }}; })"));
        m_engine->globalObject().setProperty(QStringLiteral("console"),
            consoleFactory.call({m_engine->newQObject(m_console)}));
        QJSValue types = m_engine->newObject();
        const QMetaEnum typeEnum = QMetaEnum::fromType<caScript::EpicsType>();
        for (int i = 0; i < typeEnum.keyCount(); ++i) {
            types.setProperty(QString::fromLatin1(typeEnum.key(i)), typeEnum.value(i));
        }
        m_engine->globalObject().setProperty(QStringLiteral("EpicsType"), types);
        QJSValue scriptInputs = m_engine->toScriptValue(inputs);
        QJSValue byteArrayConstructor = m_engine->globalObject().property(QStringLiteral("Uint8Array"));
        QJSValue decodedTextFunction = m_engine->evaluate(QStringLiteral(
            "(function(text) { return function() { return text; }; })"));
        QJSValue conversionFunction = m_engine->evaluate(QStringLiteral(
            "(function(results) { return function(type) {\n"
            "if (typeof type !== 'number' || !Object.prototype.hasOwnProperty.call(results, type))\n"
            "    throw new TypeError('Unknown EPICS conversion type');\n"
            "const result = results[type];\n"
            "if (result.error !== undefined) throw new RangeError(result.error);\n"
            "return result.value; }; })"));
        for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
            const QVariantMap channel = it.value().toMap();
            QJSValue scriptChannel = scriptInputs.property(it.key());
            for (auto property = channel.constBegin(); property != channel.constEnd(); ++property) {
                if (property.value().userType() == QMetaType::QVariantList
                    || property.value().userType() == QMetaType::QStringList) {
                    scriptChannel.setProperty(property.key(), inputValueToScript(m_engine, property.value()));
                }
            }
            if (channel.contains(QStringLiteral("value"))) {
                // Convert the snapshot in C++; expose only the captured results to JavaScript.
                QVariantMap conversions;
                for (int i = 0; i < typeEnum.keyCount(); ++i) {
                    const auto type = static_cast<caScript::EpicsType>(typeEnum.value(i));
                    conversions.insert(QString::number(type), scalarConversion(channel.value(QStringLiteral("value")), type));
                }
                scriptChannel.setProperty(QStringLiteral("convert"),
                                          conversionFunction.call({m_engine->toScriptValue(conversions)}));
            }
            if (!channel.contains(QStringLiteral("rawBytes"))) continue;
            const QByteArray bytes = channel.value(QStringLiteral("rawBytes")).toByteArray();
            QJSValue array = m_engine->newArray(bytes.size());
            for (int i = 0; i < bytes.size(); ++i) {
                array.setProperty(i, static_cast<unsigned char>(bytes.at(i)));
            }
            scriptChannel.setProperty(QStringLiteral("rawBytes"), byteArrayConstructor.callAsConstructor({array}));
            // Decode the original snapshot in C++, independently of the displayed value.
            scriptChannel.setProperty(QStringLiteral("fromLatin1"),
                                      decodedTextFunction.call({QJSValue(QString::fromLatin1(bytes))}));
            scriptChannel.setProperty(QStringLiteral("fromUtf8"),
                                      decodedTextFunction.call({QJSValue(QString::fromUtf8(bytes))}));
        }
        m_engine->globalObject().setProperty(QStringLiteral("inputs"), scriptInputs);
        m_engine->globalObject().setProperty(QStringLiteral("outputs"), m_engine->toScriptValue(outputs));
        const QString keys = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(outputs.keys()))
                                                   .toJson(QJsonDocument::Compact));
        const QJSValue result = m_engine->evaluate(
            QStringLiteral("(function() { try { (function() {\n") + script
                + QStringLiteral("\n})(); const values = Object.create(null);\n"
                                 "for (const key of ") + keys + QStringLiteral(
                    ") { const channel = outputs[key];\n"
                    "if (channel && Object.prototype.hasOwnProperty.call(channel, 'value'))\n"
                    "values[key] = channel.value; }\n"
                    "return {success: true, values: values};\n"
                    "} catch (error) { var message = 'JavaScript exception'; var line = 0;\n"
                    "try { message = String(error);\n"
                    "if (error != null && typeof error.lineNumber === 'number') line = error.lineNumber;\n"
                    "} catch (ignored) {}\n"
                    "return {success: false, message: message, lineNumber: line}; } })()"),
            QStringLiteral("caScript"), 0);
        const bool success = !result.isError() && result.property(QStringLiteral("success")).toBool();
        QString diagnostic;
        if (!success) {
            diagnostic = result.isError() ? result.toString() : result.property(QStringLiteral("message")).toString();
            if (diagnostic.isEmpty()) diagnostic = tr("JavaScript execution failed.");
            const int line = result.property(QStringLiteral("lineNumber")).toInt();
            if (line > 0) diagnostic = tr("%1 (line %2)").arg(diagnostic).arg(line);
        }
        emit finished(success, success ? result.property(QStringLiteral("values")).toVariant().toMap()
                                       : QVariantMap(), diagnostic);
    }

signals:
    void finished(bool success, const QVariantMap &values, const QString &diagnostic);
    void message(int level, const QString &text);

private:
    CaScriptConsole *m_console;
    QMutex m_mutex;
    QJSEngine *m_engine = nullptr;
    bool m_stopping = false;
    bool m_interruptRequested = false;
};

caScript::caScript(QWidget *parent) : QWidget(parent), m_triggerTimer(new QTimer(this)),
    m_warningTimer(new QTimer(this)), m_timeoutTimer(new QTimer(this))
{
    connect(m_triggerTimer, &QTimer::timeout, this, &caScript::executeScript);
    connect(this, &caScript::inputValueUpdated, this, &caScript::handleInputTrigger);
    m_warningTimer->setSingleShot(true);
    m_timeoutTimer->setSingleShot(true);
    m_warningTimer->setTimerType(Qt::PreciseTimer);
    m_timeoutTimer->setTimerType(Qt::PreciseTimer);
    connect(m_warningTimer, &QTimer::timeout, this, [this]() {
        emit scriptWarning(tr("Script execution exceeded 100 ms."));
    });
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        m_timedOut = true;
        m_worker->interrupt();
        emit scriptError(tr("Script execution interrupted after the 1000 ms time limit."));
    });
}

caScript::~caScript()
{
    m_triggerTimer->stop();
    m_warningTimer->stop();
    m_timeoutTimer->stop();
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
                                                              const QString &diagnostic) {
            m_warningTimer->stop();
            m_timeoutTimer->stop();
            if (m_timedOut) {
                success = false;
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
    m_worker->prepareExecution();
    m_timedOut = false;
    m_lastExecution.start();
    m_warningTimer->start(100);
    m_timeoutTimer->start(1000);
    QMetaObject::invokeMethod(m_worker, "execute", Qt::QueuedConnection,
                              Q_ARG(QString, m_script), Q_ARG(QVariantMap, snapshot),
                              Q_ARG(QVariantMap, outputs));
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
