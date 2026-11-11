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
#include <QTimer>
#include <cmath>
#include <limits>

namespace {
QJSValue inputValueToScript(QJSEngine *engine, const QVariant &value)
{
    if (value.userType() != QMetaType::QVariantList) return engine->toScriptValue(value);
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

class CaScriptWorker : public QObject
{
    Q_OBJECT

public:
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
        }
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
            if (channel.contains(QStringLiteral("value"))) {
                if (channel.value(QStringLiteral("value")).userType() == QMetaType::QVariantList) {
                    scriptChannel.setProperty(QStringLiteral("value"),
                        inputValueToScript(m_engine, channel.value(QStringLiteral("value"))));
                }
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
                    "} catch (error) { return {success: false}; } })()"),
            QStringLiteral("caScript"), 0);
        const bool success = !result.isError() && result.property(QStringLiteral("success")).toBool();
        emit finished(success, success ? result.property(QStringLiteral("values")).toVariant().toMap()
                                       : QVariantMap());
    }

signals:
    void finished(bool success, const QVariantMap &values);

private:
    QMutex m_mutex;
    QJSEngine *m_engine = nullptr;
    bool m_stopping = false;
};

caScript::caScript(QWidget *parent) : QWidget(parent)
{}

caScript::~caScript()
{
    if (m_executionThread) {
        m_worker->stop();
        m_executionThread->quit();
        m_executionThread->wait();
    }
}

void caScript::executeScript()
{
    if (m_running) return;
    m_running = true;
    if (!inputConfigurationErrors().isEmpty() || !outputConfigurationErrors().isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            m_running = false;
            emit scriptFinished(false);
        });
        return;
    }

    if (!m_executionThread) {
        m_executionThread = new QThread(this);
        m_worker = new CaScriptWorker;
        m_worker->moveToThread(m_executionThread);
        connect(m_executionThread, &QThread::finished, m_worker, &QObject::deleteLater);
        connect(m_worker, &CaScriptWorker::finished, this, [this](bool success, const QVariantMap &values) {
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
    QMetaObject::invokeMethod(m_worker, "execute", Qt::QueuedConnection,
                              Q_ARG(QString, m_script), Q_ARG(QVariantMap, snapshot),
                              Q_ARG(QVariantMap, outputs));
}

void caScript::updateInputValue(int index, const QVariant &value)
{
    if (index >= 0 && index < m_inputChannels.size()) {
        m_inputValues.insert(index, value);
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
    }
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
    m_inputChannels = inputs;
    m_inputValues.clear();
    m_inputRawBytes.clear();
    m_inputMetadata.clear();
    m_inputJsonError.clear();
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
