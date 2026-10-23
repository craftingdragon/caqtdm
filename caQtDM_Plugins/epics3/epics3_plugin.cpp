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
 *  Copyright (c) 2010 - 2014
 *
 *  Author:
 *    Anton Mezger
 *  Contact details:
 *    anton.mezger@psi.ch
 */
#include <QDebug>
#include <stdint.h>
#include <cadef.h>
#include "epics3_plugin.h"
#include "caQtDM_Plugins_global.h"

Q_LOGGING_CATEGORY(epics3Log, "caqtdm.plugins.epics.3")

typedef struct _connectInfo {
    int connected;
    int index;
    uint64_t generation;
    int event;
    pv_string pv;    // channel name
    chid ch;         // read channel
    evid evID;       // epics event id
    int  evAdded;    // epics event added yes/no
} connectInfo;


// global variables defined here for access through c routines in epicsSubs.c
extern "C" {
 MutexKnobData* mutexKnobdataPtr;
 MessageWindow *messageWindowPtr;
}

QString Epics3Plugin::pluginName()
{
    return "epics3";
}

Epics3Plugin::Epics3Plugin()
{
    qCDebug(epics3Log) << "Epics3: Create";
}

int Epics3Plugin::initCommunicationLayer(MutexKnobData *data, MessageWindow *messageWindow, QMap<QString, QString> options)
{
    qCDebug(epics3Log) << "InitCommunicationLayer with options" << options;
    QString msg=QString("Epics3Plugin: epics version: %1").arg(EPICS_VERSION_STRING);
    mutexknobdataP = data;
    messagewindowP = messageWindow;
    if(messagewindowP != (MessageWindow *) Q_NULLPTR) messagewindowP->postMsgEvent(QtDebugMsg,(char*) msg.toLatin1().constData());
    mutexknobdataP = data;
    mutexKnobdataPtr = data;
    messageWindowPtr = messageWindow;
    Channelcache.clear();
    PrepareDeviceIO();
    return true;
}

int Epics3Plugin::pvAddMonitor(int index, knobData *kData, int rate, int skip) {
    qCDebug(epics3Log) << "Epics3Plugin:pvAddMonitor" << kData->pv;
    if (!Channelcache.contains(kData->pv,index)){
        qCDebug(epics3Log) << "Epics3Plugin:first" << kData->pv << kData;
        Channelcache.insert(kData->pv,index);
    }else{
        qCDebug(epics3Log) << "duplicated: " << kData->pv << Channelcache.value(kData->pv) ;
    }

    return CreateAndConnect(index, kData, rate, skip);
}

int Epics3Plugin::pvClearMonitor(knobData *kData) {
    qCDebug(epics3Log) << "Epics3Plugin:pvClearMonitor" << kData->pv;
    Channelcache.remove(kData->pv,kData->index);
    ClearMonitor(kData);
    return true;
}

int Epics3Plugin::pvFreeAllocatedData(knobData *kData)
{
    qCDebug(epics3Log) << "Epics3Plugin:pvFreeAllocatedData";
    QMutexLocker locker((QMutex *)kData->mutex);
    if (kData->edata.info != (void *) Q_NULLPTR) {
        free(kData->edata.info);
        kData->edata.info = (void*) Q_NULLPTR;
    }
    if(kData->edata.dataB != (void*) Q_NULLPTR) {
        free(kData->edata.dataB);
        kData->edata.dataB = (void*) Q_NULLPTR;
    }

    return true;
}

int Epics3Plugin::pvSetValue(char *pv, double rdata, int32_t idata, char *sdata, char *object, char *errmess, int forceType) {

    qCDebug(epics3Log) << "Epics3Plugin:pvSetValue";
    if (Channelcache.contains(pv)){
        qCDebug(epics3Log) << "CacheEntry Found" << pv;
        QMultiMap<QString, int>::iterator i = Channelcache.find(pv);
        while (i != Channelcache.end() && i.key() == pv) {

            knobData kData=mutexKnobdataPtr->GetMutexKnobData(i.value());

            qCDebug(epics3Log) << "CacheEntry Found" << kData.pv;
            if (kData.edata.connected){
                if (kData.edata.info){
                    chid ch=((connectInfo *)kData.edata.info)->ch;
                    if (ch){
                        qCDebug(epics3Log) << "Epics3Plugin:use cached Ch" << pv << ":" << kData.pv << ":" << kData.edata.connected;
                        return EpicsSetValue_Connected(ch, pv, rdata, idata, sdata, object, errmess, forceType);
                    }
                }
            }
            i++;
        }
    }
    return EpicsSetValue(pv, rdata, idata, sdata, object, errmess, forceType);
}

int Epics3Plugin::pvSetWave(char *pv, float *fdata, double *ddata, int16_t *data16, int32_t *data32, char *sdata, int nelm, char *object, char *errmess) {
    qCDebug(epics3Log) << "Epics3Plugin:pvSetWave";

    if (Channelcache.contains(pv)){
        qCDebug(epics3Log) << "CacheEntry Found" << pv;
        QMultiMap<QString, int>::iterator i = Channelcache.find(pv);
        while (i != Channelcache.end() && i.key() == pv) {

            knobData kData=mutexKnobdataPtr->GetMutexKnobData(i.value());

            qCDebug(epics3Log) << "CacheEntry Found" << kData.pv;
            if (kData.edata.connected){
                if (kData.edata.info){
                    chid ch=((connectInfo *)kData.edata.info)->ch;
                    if (ch){
                        qCDebug(epics3Log) << "Epics3Plugin:use cached Ch" << pv << ":" << kData.pv << ":" << kData.edata.connected;
                        return EpicsSetWave_Connected(ch, pv, fdata, ddata, data16, data32, sdata, nelm, object, errmess);
                    }
                }
            }
            i++;
        }
    }
    return EpicsSetWave(pv, fdata, ddata, data16, data32, sdata, nelm, object, errmess);
}

int Epics3Plugin::pvGetTimeStamp(char *pv, char *timestamp) {
    qCDebug(epics3Log) << "Epics3Plugin:pvgetTimeStamp";
    if (Channelcache.contains(pv)){
        qCDebug(epics3Log) << "CacheEntry Found" << pv;
        QMultiMap<QString, int>::iterator i = Channelcache.find(pv);
        while (i != Channelcache.end() && i.key() == pv) {

            knobData kData=mutexKnobdataPtr->GetMutexKnobData(i.value());

            qCDebug(epics3Log) << "CacheEntry Found" << kData.pv;
            if (kData.edata.connected){
                if (kData.edata.info){
                    chid ch=((connectInfo *)kData.edata.info)->ch;
                    if (ch){
                        qCDebug(epics3Log) << "Epics3Plugin:use cached Ch" << pv << ":" << kData.pv << ":" << kData.edata.connected;
                        return EpicsGetTimeStamp_Connected(ch, pv, timestamp);
                    }
                }
            }
            i++;
        }
    }
    return EpicsGetTimeStamp(pv, timestamp);
}

int Epics3Plugin::pvGetDescription(char *pv, char *description) {
    qCDebug(epics3Log) << "Epics3Plugin:pvGetDescription";
    return EpicsGetDescription(pv, description);
}

int Epics3Plugin::pvClearEvent(void * ptr) {
    qCDebug(epics3Log) << "Epics3Plugin:pvClearEvent";
    clearEvent(ptr);
    return true;
}

int Epics3Plugin::pvAddEvent(void * ptr) {
    qCDebug(epics3Log) << "Epics3Plugin:pvAddEvent";
    addEvent(ptr);
    return true;
}

int Epics3Plugin::pvReconnect(knobData *kData) {
    qCDebug(epics3Log) << "Epics3Plugin:pvReconnect";
    EpicsReconnect(kData);
    return true;
}

int Epics3Plugin::pvDisconnect(knobData *kData) {
    qCDebug(epics3Log) << "Epics3Plugin:pvDisconnect";
    EpicsDisconnect(kData);
    return true;
}

int Epics3Plugin::FlushIO() {
    qCDebug(epics3Log) << "Epics3Plugin:FlushIO";
    EpicsFlushIO();
    return true;
}

int Epics3Plugin::TerminateIO() {
    qCDebug(epics3Log) << "Epics3Plugin:TerminateIO";
    TerminateDeviceIO();
    return true;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#else
    Q_EXPORT_PLUGIN2(Epics3Plugin, Epics3Plugin)
#endif


