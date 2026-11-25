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
#include <QElapsedTimer>
#include <QByteArray>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>
#include <qtcontrols_global.h>

class CaScriptWorker;
class QThread;
class QTimer;

class QTCON_EXPORT caScript : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(DisplayMode displayMode READ getDisplayMode WRITE setDisplayMode)
    Q_PROPERTY(TriggerMode triggerMode READ getTriggerMode WRITE setTriggerMode DESIGNABLE true)
    Q_PROPERTY(QString triggerInput READ getTriggerInput WRITE setTriggerInput DESIGNABLE true)
    Q_PROPERTY(int triggerInterval READ getTriggerInterval WRITE setTriggerInterval DESIGNABLE true)
    Q_PROPERTY(ExecutionMode executionMode READ getExecutionMode WRITE setExecutionMode DESIGNABLE true)
    Q_PROPERTY(QString script READ getScript WRITE setScript)
    Q_PROPERTY(QString channels READ getChannels WRITE setChannels)
    Q_PROPERTY(QString outputChannels READ getOutputChannelsJson WRITE setOutputChannelsJson)

public:
    enum DisplayMode { Hidden, LastOutput, Status, Diagnostics };
    Q_ENUM(DisplayMode)

    enum TriggerMode { Manual, AnyInput, SelectedInput, MinimumInterval, Periodic };
    Q_ENUM(TriggerMode)

    enum ExecutionMode { Constant, DataDriven };
    Q_ENUM(ExecutionMode)

    enum EpicsType { String = 0, Short = 1, Float = 2, Enum = 3, Char = 4, Long = 5, Double = 6 };
    Q_ENUM(EpicsType)

    struct InputChannel
    {
        QString channel;
        QString alias;
    };

    explicit caScript(QWidget *parent = nullptr);
    ~caScript() override;

    DisplayMode getDisplayMode() const;
    void setDisplayMode(DisplayMode mode);
    QString getDisplayText() const;
    QString getLastOutputText() const;
    void updateWriteOutcome(int written, int rejected, const QString &firstRejection);
    void reportDiagnostic(QtMsgType level, const QString &message);
    void reportConfigurationErrors(const QStringList &errors);

    TriggerMode getTriggerMode() const;
    void setTriggerMode(TriggerMode mode);
    QString getTriggerInput() const;
    void setTriggerInput(const QString &input);
    int getTriggerInterval() const;
    void setTriggerInterval(int milliseconds);
    QStringList triggerConfigurationErrors() const;

    ExecutionMode getExecutionMode() const;
    void setExecutionMode(ExecutionMode mode);

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

    void freezeChannelBindings();
    void updateInputSample(int index, const QVariantMap &metadata, const QVariant &value,
                           const QVariant &rawBytes = QVariant());

    static QStringList validateInputChannels(const QList<InputChannel> &inputs);

public slots:
    void updateLastOutput(const QString &channel, const QVariant &value);
    void startTriggers();
    void executeScript();
    void updateInputValue(int index, const QVariant &value);
    void updateInputRawBytes(int index, const QByteArray &bytes);
    void updateInputMetadata(int index, const QVariantMap &metadata);

signals:
    void inputValueUpdated(int index);
    void scriptInfo(const QString &message);
    void scriptWarning(const QString &message);
    void scriptError(const QString &message);
    void scriptFinished(bool success);
    void outputValuesReady(const QVariantMap &values);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    DisplayMode m_displayMode = Hidden;
    QString m_lastOutputText;
    QString m_statusText = tr("Idle");
    QString m_diagnosticText;
    void setStatusText(const QString &text);
    void handleInputTrigger(int index);
    void refreshTriggers();
    bool channelBindingsEditable();

    TriggerMode m_triggerMode = Manual;
    QString m_triggerInput;
    int m_triggerInterval = 100;
    bool m_triggersStarted = false;
    bool m_channelBindingsFrozen = false;
    QTimer *m_triggerTimer;
    QTimer *m_warningTimer;
    bool m_slowWarningEmitted = false;
    int m_consecutiveTimeouts = 0;
    bool m_executionDisabled = false;
    QElapsedTimer m_lastExecution;
    ExecutionMode m_executionMode = Constant;
    QSet<int> m_readyInputs;
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
