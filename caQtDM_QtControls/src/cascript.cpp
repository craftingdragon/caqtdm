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
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QThread>
#include <QTimer>

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
    void execute(const QString &script, const QVariantMap &inputs)
    {
        {
            QMutexLocker lock(&m_mutex);
            if (m_stopping) return;
            if (!m_engine) m_engine = new QJSEngine(this);
        }
        QJSValue scriptInputs = m_engine->toScriptValue(inputs);
        QJSValue byteArrayConstructor = m_engine->globalObject().property(QStringLiteral("Uint8Array"));
        QJSValue decodedTextFunction = m_engine->evaluate(QStringLiteral(
            "(function(text) { return function() { return text; }; })"));
        for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
            const QVariantMap channel = it.value().toMap();
            if (!channel.contains(QStringLiteral("rawBytes"))) continue;
            const QByteArray bytes = channel.value(QStringLiteral("rawBytes")).toByteArray();
            QJSValue array = m_engine->newArray(bytes.size());
            for (int i = 0; i < bytes.size(); ++i) {
                array.setProperty(i, static_cast<unsigned char>(bytes.at(i)));
            }
            QJSValue scriptChannel = scriptInputs.property(it.key());
            scriptChannel.setProperty(QStringLiteral("rawBytes"), byteArrayConstructor.callAsConstructor({array}));
            // Decode the original snapshot in C++, independently of the displayed value.
            scriptChannel.setProperty(QStringLiteral("fromLatin1"),
                                      decodedTextFunction.call({QJSValue(QString::fromLatin1(bytes))}));
        }
        m_engine->globalObject().setProperty(QStringLiteral("inputs"), scriptInputs);
        const QJSValue result = m_engine->evaluate(
            QStringLiteral("(function() { try { (function() {\n") + script
                + QStringLiteral("\n})(); return true; } catch (error) { return false; } })()"),
            QStringLiteral("caScript"), 0);
        emit finished(result.isBool() && result.toBool());
    }

signals:
    void finished(bool success);

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
    if (!inputConfigurationErrors().isEmpty()) {
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
        connect(m_worker, &CaScriptWorker::finished, this, [this](bool success) {
            m_running = false;
            emit scriptFinished(success);
        });
        m_executionThread->start();
    }
    QVariantMap snapshot;
    for (int i = 0; i < m_inputChannels.size(); ++i) {
        const InputChannel &input = m_inputChannels.at(i);
        QVariantMap channel;
        if (m_inputValues.contains(i)) {
            channel.insert(QStringLiteral("value"), m_inputValues.value(i));
        }
        if (m_inputRawBytes.contains(i)) {
            channel.insert(QStringLiteral("rawBytes"), m_inputRawBytes.value(i));
        }
        snapshot.insert(input.alias.isEmpty() ? input.channel : input.alias, channel);
    }
    QMetaObject::invokeMethod(m_worker, "execute", Qt::QueuedConnection,
                              Q_ARG(QString, m_script), Q_ARG(QVariantMap, snapshot));
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
