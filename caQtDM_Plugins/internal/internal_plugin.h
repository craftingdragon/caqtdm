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
 *  Copyright (c) 2010 - 2026
 *
 *  Author:
 *    Helge Brands
 *  Contact details:
 *    helge.brands@psi.ch
 */
#ifndef INTERNALPLUGIN_H
#define INTERNALPLUGIN_H

#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QVariant>

#include "controlsinterface.h"
#include "internal_channel.h"

// This needs to be included everywhere qCDebug, qCWarning etc are used
#include <QLoggingCategory>

// test/simulation plugin: channels live only inside caQtDM and are defined
// through genSoftPV widgets (see InternalChannel)
class Q_DECL_EXPORT InternalPlugin : public QObject, ControlsInterface
{
    Q_OBJECT
    Q_INTERFACES(ControlsInterface)
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "ch.psi.caqtdm.Plugin.ControlsInterface/1.0.internalcontrols")
#endif

public:
    QString pluginName();
    InternalPlugin();
    ~InternalPlugin();

    int initCommunicationLayer(MutexKnobData *data, MessageWindow *messageWindow, QMap<QString, QString> options);
    int pvAddMonitor(int index, knobData *kData, int rate, int skip);
    int pvClearMonitor(knobData *kData);
    int pvFreeAllocatedData(knobData *kData);
    int pvSetValue(char *pv, double rdata, int32_t idata, char *sdata, char *object, char *errmess, int forceType);
    bool pvSetValue(knobData *kData, double rdata, int32_t idata, char *sdata, char *object, char *errmess, int forceType);
    int pvSetWave(char *pv, float *fdata, double *ddata, int16_t *data16, int32_t *data32, char *sdata, int nelm, char *object, char *errmess);
    bool pvSetWave(knobData *kData, float *fdata, double *ddata, int16_t *data16, int32_t *data32, char *sdata, int nelm, char *object, char *errmess);
    int pvGetTimeStamp(char *pv, char *timestamp);
    int pvGetDescription(char *pv, char *description);
    int pvClearEvent(void * ptr);
    int pvAddEvent(void * ptr);
    int pvReconnect(knobData *kData);
    int pvDisconnect(knobData *kData);
    int FlushIO();
    int TerminateIO();

    // access to a channel by its base name (used by the unit tests)
    InternalChannel *channel(const QString &baseName);

private slots:
    void updateChannels();

private:
    int setValueForPv(const QString &pv, double rdata, int32_t idata, char *sdata);
    int setWaveForPv(const QString &pv, float *fdata, double *ddata, int16_t *data16, int32_t *data32, int nelm);
    void publishChannel(const QString &key, InternalChannel *channel);
    void publishIndex(InternalChannel *channel, int index);

    QMutex mutex;
    MutexKnobData *mutexknobdataP;
    MessageWindow *messagewindowP;
    QMap<QString, InternalChannel *> channels;
    QMap<QString, QList<int> > monitorIndexes;
    QMap<int, InternalChannel::Field> monitorFields;
    QMap<int, MutexKnobData::MonitorHandle> monitorHandles;
    QMap<int, QVariant> lastPublishedField;
    QTimer *timer;
};

#endif // INTERNALPLUGIN_H
