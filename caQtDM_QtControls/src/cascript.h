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
#include <QStringList>
#include <QWidget>
#include <qtcontrols_global.h>

class CaScriptWorker;
class QThread;

class QTCON_EXPORT caScript : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QString script READ getScript WRITE setScript)
    Q_PROPERTY(QString channels READ getChannels WRITE setChannels)

public:
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

    static QStringList validateInputChannels(const QList<InputChannel> &inputs);

public slots:
    void executeScript();

signals:
    void scriptFinished(bool success);

private:
    QString m_script;
    QList<InputChannel> m_inputChannels;
    QString m_inputJsonError;
    QThread *m_executionThread = nullptr;
    CaScriptWorker *m_worker = nullptr;
    bool m_running = false;
};

#endif // CASCRIPT_H
