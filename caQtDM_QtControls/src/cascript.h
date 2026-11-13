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

#ifndef CASCRIPT_H
#define CASCRIPT_H

#include <QList>
#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>
#include <qtcontrols_global.h>

class CaScriptWorker;
class QThread;

class QTCON_EXPORT caScript : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QString script READ getScript WRITE setScript)
    Q_PROPERTY(QString channels READ getChannels WRITE setChannels)
    Q_PROPERTY(QString outputChannels READ getOutputChannelsJson WRITE setOutputChannelsJson)

public:
    enum EpicsType { String = 0, Short = 1, Float = 2, Enum = 3, Char = 4, Long = 5, Double = 6 };
    Q_ENUM(EpicsType)

    struct InputChannel
    {
        QString channel;
        QString alias;
    };

    explicit caScript(QWidget *parent = nullptr);
    ~caScript() override;

    QString getScript() const;
    void setScript(const QString &script);

    QString getChannels() const;
    void setChannels(const QString &json);
    QList<InputChannel> getInputChannels() const;
    void setInputChannels(const QList<InputChannel> &inputs);
    QStringList inputConfigurationErrors() const;
    QString getOutputChannelsJson() const;
    void setOutputChannelsJson(const QString &json);
    QList<InputChannel> getOutputChannels() const;
    void setOutputChannels(const QList<InputChannel> &outputs);
    QStringList outputConfigurationErrors() const;

    static QStringList validateInputChannels(const QList<InputChannel> &inputs);

public slots:
    void executeScript();
    void updateInputValue(int index, const QVariant &value);
    void updateInputRawBytes(int index, const QByteArray &bytes);
    void updateInputMetadata(int index, const QVariantMap &metadata);

signals:
    void scriptFinished(bool success);
    void outputValuesReady(const QVariantMap &values);

private:
    QString m_script;
    QList<InputChannel> m_inputChannels;
    QList<InputChannel> m_outputChannels;
    QMap<int, QVariant> m_inputValues;
    QMap<int, QByteArray> m_inputRawBytes;
    QMap<int, QVariantMap> m_inputMetadata;
    QString m_inputJsonError;
    QString m_outputJsonError;
    QThread *m_executionThread = nullptr;
    CaScriptWorker *m_worker = nullptr;
    bool m_running = false;
};

#endif // CASCRIPT_H
