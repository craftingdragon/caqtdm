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
 *  Copyright (c) 2010 - 2020
 *
 *  Author:
 *    Helge Brands
 *  Contact details:
 *    helge.brands@psi.ch
 */
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#endif

#include "gps_plugin.h"
#include "caQtDM_Plugins_global.h"

Q_LOGGING_CATEGORY(gpsLog, "caqtdm.plugins.gps")

// as defined in knobDefines.h
//caType {caSTRING	= 0, caINT = 1, caFLOAT = 2, caENUM = 3, caCHAR = 4, caLONG = 5, caDOUBLE = 6};

// gives the plugin name back
QString gpsPlugin::pluginName()
{
    return "gps";
}

// constructor
gpsPlugin::gpsPlugin()
{
    qCDebug(gpsLog) << "GPS: Create";
    enable_gps_readout=false;
    timerValues = Q_NULLPTR;
    pos_data_source = Q_NULLPTR;
}

gpsPlugin::~gpsPlugin()
{
    if (timerValues) {
        timerValues->deleteLater();
    }
}

void gpsPlugin::publishValue(const Registration &registration, double value, bool increment, bool refreshMetadata)
{
    if (!mutexknobdataP->UpdateMutexKnobDataIfCurrent(registration.handle, [=](knobData *record) {
            if (increment) {
                record->edata.rvalue = value;
                ++record->edata.monitorCount;
            }
            if (refreshMetadata) {
                record->edata.fieldtype = caDOUBLE;
                record->edata.connected = true;
                record->edata.accessR = true;
                record->edata.accessW = false;
            }
        })) {
        return;
    }

    knobData snapshot;
    if (mutexknobdataP->GetMutexKnobDataIfCurrent(registration.handle, &snapshot)) {
        mutexknobdataP->SetMutexKnobDataReceivedIfCurrent(registration.handle, &snapshot);
    }
}

// In this GPS plugin we update our interface here; normally this comes from the control system.
void gpsPlugin::updateInterface()
{
    QMutexLocker locker(&mutex);

    for (const Registration &registration : registrations) {
        const double value = listOfDoubles.value(registration.pv, 0.0);
        publishValue(registration, value, true, true);
    }
}

void gpsPlugin::positionUpdated(const QGeoPositionInfo &info)
{
    qCDebug(gpsLog) << "Position updated:" << info;
    QMutexLocker locker(&mutex);
    if (!enable_gps_readout || !info.isValid()) {
        return;
    }

    for (const Registration &registration : registrations) {
        const QString &key = registration.pv;
        double value = 0.0;
        bool changed = false;

        if (key.compare("altitude", Qt::CaseInsensitive) == 0) {
            value = info.coordinate().altitude();
            changed = true;
        } else if (key.compare("latitude", Qt::CaseInsensitive) == 0) {
            value = info.coordinate().latitude();
            changed = true;
        } else if (key.compare("longitude", Qt::CaseInsensitive) == 0) {
            value = info.coordinate().longitude();
            changed = true;
        } else if (key.compare("distanceTo", Qt::CaseInsensitive) == 0) {
            value = info.coordinate().distanceTo(distancedata);
            changed = true;
        } else if (key.compare("GroundSpeed", Qt::CaseInsensitive) == 0 &&
                   info.hasAttribute(QGeoPositionInfo::GroundSpeed)) {
            value = info.attribute(QGeoPositionInfo::GroundSpeed);
            changed = true;
        } else if (key.compare("Direction", Qt::CaseInsensitive) == 0 &&
                   info.hasAttribute(QGeoPositionInfo::Direction)) {
            value = info.attribute(QGeoPositionInfo::Direction);
            changed = true;
        } else if (key.compare("HorizontalAccuracy", Qt::CaseInsensitive) == 0 &&
                   info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy)) {
            value = info.attribute(QGeoPositionInfo::HorizontalAccuracy);
            changed = true;
        }

        publishValue(registration, value, changed, false);
    }
}

// in this gps we update our values here
void gpsPlugin::updateValues()
{
    QMutexLocker locker(&mutex);
    if (!enable_gps_readout) return;
}


// initialize our communicationlayer with everything you need
int gpsPlugin::initCommunicationLayer(MutexKnobData *data, MessageWindow *messageWindow, QMap<QString, QString> options)
{
    qCDebug(gpsLog) << "gpsPlugin: InitCommunicationLayer with options" << options;

    mutexknobdataP = data;
    messagewindowP = messageWindow;

    initValue = 0.0;

    // we want to update our internal doubles every second
    timerValues = new QTimer();
    connect(timerValues, SIGNAL(timeout()), this, SLOT(updateValues()));
    timerValues->start(500);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // since Qt 6.5 the app must request the location permission itself
    // (iOS/Android show no dialog and deliver no fixes without this)
    QLocationPermission locationPermission;
    locationPermission.setAccuracy(QLocationPermission::Precise);
    locationPermission.setAvailability(QLocationPermission::WhenInUse);
    switch (qApp->checkPermission(locationPermission)) {
    case Qt::PermissionStatus::Undetermined:
         qApp->requestPermission(locationPermission, this, [this](const QPermission &permission){
             if (permission.status() == Qt::PermissionStatus::Granted){
                 QMutexLocker locker(&mutex);
                 if (enable_gps_readout){
                    pos_data_source = QGeoPositionInfoSource::createDefaultSource(this);
                    if (pos_data_source) pos_data_source->startUpdates();
                 }
             } else if (messagewindowP != Q_NULLPTR){
                 messagewindowP->postMsgEvent(QtWarningMsg,(char*) "gps: location permission denied");
             }
         });
         break;
    case Qt::PermissionStatus::Denied:
         if (messagewindowP != Q_NULLPTR)
             messagewindowP->postMsgEvent(QtWarningMsg,(char*) "gps: location permission denied - enable it in the system settings");
         break;
    case Qt::PermissionStatus::Granted:
         pos_data_source = QGeoPositionInfoSource::createDefaultSource(this);
         if (pos_data_source) pos_data_source->startUpdates();
         break;
    }
#else
    pos_data_source = QGeoPositionInfoSource::createDefaultSource(this);
#endif

      if (pos_data_source) {
          connect(pos_data_source, SIGNAL(positionUpdated(QGeoPositionInfo)),
                  this, SLOT(positionUpdated(QGeoPositionInfo)));

         pos_data_source->setUpdateInterval(500);
         pos_data_source->setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);
         return true;
      }else {
         qCWarning(gpsLog) << "GPS could not been initialized, no default source";
         return false;
      }
}

// caQtDM_Lib will call this routine for defining a monitor
int gpsPlugin::pvAddMonitor(int index, knobData *kData, int rate, int skip) {
    Q_UNUSED(index);
    Q_UNUSED(rate);
    Q_UNUSED(skip);
    QMutexLocker locker(&mutex);
    QString key = kData->pv;

    qCDebug(gpsLog) << "gpsPlugin:pvAddMonitor" << kData->pv << kData->index;
    bool validkey=false;
    bool writeable=false;

    if (key.compare("altitude",Qt::CaseInsensitive)==0){
       validkey=true; }else
    if (key.compare("latitude",Qt::CaseInsensitive)==0){
        validkey=true;}else
    if (key.compare("longitude",Qt::CaseInsensitive)==0){
       validkey=true;}else
    if (key.compare("distanceTo",Qt::CaseInsensitive)==0){
       validkey=true;}else
    if (key.compare("setAltitude",Qt::CaseInsensitive)==0){
       validkey=true;
       writeable=true;}else
    if (key.compare("setLatitude",Qt::CaseInsensitive)==0){
       validkey=true;
       writeable=true;}else
    if (key.compare("setLongitude",Qt::CaseInsensitive)==0){
       validkey=true;
       writeable=true;}else
    if (key.compare("GroundSpeed",Qt::CaseInsensitive)==0){
        qstrncpy(kData->edata.units,QString("m/s").toLatin1().data(),39);
        validkey=true;}else
    if (key.compare("Direction",Qt::CaseInsensitive)==0){
        validkey=true;}
    if (key.compare("HorizontalAccuracy",Qt::CaseInsensitive)==0){
        qstrncpy(kData->edata.units,QString("m").toLatin1().data(),39);
        validkey=true;}

    if (!validkey) return false;
    const MutexKnobData::MonitorHandle handle = mutexknobdataP->GetMutexKnobDataHandle(kData->index);
    if (!handle.isValid()) return false;
    // initial values into the doubles list
    if (!listOfDoubles.contains(key)) {
        listOfDoubles.insert(key, 0.0);
    }

    kData->edata.fieldtype = caDOUBLE;
    kData->edata.precision = 6;
    kData->edata.connected = true;
    kData->edata.accessR = true;
    kData->edata.accessW = writeable;
    ++kData->edata.monitorCount;
    if (!mutexknobdataP->SetMutexKnobDataIfCurrent(handle, *kData)) {
        return false;
    }
    registrations.append({handle, key});
    knobData snapshot;
    if (mutexknobdataP->GetMutexKnobDataIfCurrent(handle, &snapshot)) {
        mutexknobdataP->SetMutexKnobDataReceivedIfCurrent(handle, &snapshot);
    }
    if (!enable_gps_readout && pos_data_source) {
        pos_data_source->startUpdates();
        enable_gps_readout = true;
    }

    return true;
}

// caQtDM_Lib will call this routine for getting rid of a monitor
int gpsPlugin::pvClearMonitor(knobData *kData) {
    QMutexLocker locker(&mutex);

    qCDebug(gpsLog) << "gpsPlugin:pvClearMonitor" << kData->pv << kData->index;
    QString key = kData->pv;
    for (int i = 0; i < registrations.size(); ++i) {
        if (registrations.at(i).handle.index == kData->index && registrations.at(i).pv == key) {
            registrations.removeAt(i);
            break;
        }
    }
    bool stillUsed = false;
    for (const Registration &registration : registrations) {
        if (registration.pv == key) {
            stillUsed = true;
            break;
        }
    }
    if (!stillUsed) {
        listOfDoubles.remove(key);
    }

    return true;
}

int gpsPlugin::pvFreeAllocatedData(knobData *kData)
{
    QMutexLocker locker(&mutex);
    qCDebug(gpsLog) << "gpsPlugin:pvFreeAllocatedData";
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

// caQtDM_Lib will call this routine for setting data (see for more detail the epics3 plugin)
int gpsPlugin::pvSetValue(char *pv, double rdata, int32_t idata, char *sdata, char *object, char *errmess, int forceType) {
    Q_UNUSED(object)
    Q_UNUSED(errmess)
    Q_UNUSED(forceType)
    Q_UNUSED(idata)
    Q_UNUSED(sdata)

     QString key = pv;
     if (key.compare("setAltitude",Qt::CaseInsensitive)){
        distancedata.setAltitude(rdata); }else
     if (key.compare("setLatitude",Qt::CaseInsensitive)){
         distancedata.setLatitude(rdata);}else
     if (key.compare("setLongitude",Qt::CaseInsensitive)){
         distancedata.setLongitude(rdata);}

    return true;
}

// caQtDM_Lib will call this routine for setting waveforms data (see for more detail the epics3 plugin)
int gpsPlugin::pvSetWave(char *pv, float *fdata, double *ddata, int16_t *data16, int32_t *data32, char *sdata, int nelm, char *object, char *errmess) {
    Q_UNUSED(pv);
    Q_UNUSED(fdata);
    Q_UNUSED(ddata);
    Q_UNUSED(data16);
    Q_UNUSED(data32);
    Q_UNUSED(sdata);
    Q_UNUSED(nelm);
    Q_UNUSED(object);
    Q_UNUSED(errmess);
    return true;
}

// caQtDM_Lib will call this routine for getting a description of the monitor
int gpsPlugin::pvGetTimeStamp(char *pv, char *timestamp) {
    Q_UNUSED(pv);
    Q_UNUSED(timestamp);
    qCDebug(gpsLog) << "gpsPlugin:pvgetTimeStamp";
    qstrncpy(timestamp, "timestamp in epics format", TIMESTAMP_STRING_LENGTH);
    return true;
}

// caQtDM_Lib will call this routine for getting the timestamp for this monitor
int gpsPlugin::pvGetDescription(char *pv, char *description) {
    Q_UNUSED(pv);
    Q_UNUSED(description);
    qCDebug(gpsLog) << "gpsPlugin:pvGetDescription";

    QString data=pos_data_source->sourceName();
    data="Datasource: "+data;
    qstrncpy(description,  data.toLatin1().data(), MAX_STRING_LENGTH);
    return true;
}

// next two routines are used to stop and restart the monitoring (used in case of tabWidgets in the display)
int gpsPlugin::pvClearEvent(void * ptr) {
    Q_UNUSED(ptr);
    qCDebug(gpsLog) << "gpsPlugin:pvClearEvent";
    return true;
}

int gpsPlugin::pvAddEvent(void * ptr) {
    Q_UNUSED(ptr);
    qCDebug(gpsLog) << "gpsPlugin:pvAddEvent";
    return true;
}

// next two routines are used to connect and disconnect monitors when the application gest suspended and reactivated
int gpsPlugin::pvReconnect(knobData *kData) {
    Q_UNUSED(kData);
    qCDebug(gpsLog) << "gpsPlugin:pvReconnect";
    if(!enable_gps_readout){
        qCDebug(gpsLog) << "gpsPlugin:startUpdates";
        pos_data_source->startUpdates();
        enable_gps_readout=true;
    }

    return true;
}

int gpsPlugin::pvDisconnect(knobData *kData) {
    Q_UNUSED(kData);
    qCDebug(gpsLog) << "gpsPlugin:pvDisconnect";
    return true;
}

// flush any io is periodically called (1s timer) in order to flush the disconnection and reconnection
// used for pv's that will be hidden and shown in case of tabwidgets
int gpsPlugin::FlushIO() {
    qCDebug(gpsLog) << "gpsPlugin:FlushIO";
    return true;
}

// termination (in case of epics3, this is used to destroy the context when the application gest deactivated
// otherwise probably no meaning; in this gps, we stop the simulation, however it will not be reactivated
// any more (you may do that through pvReconnect)
int gpsPlugin::TerminateIO() {
    qCDebug(gpsLog) << "gpsPlugin:TerminateIO";

    if(enable_gps_readout){
        qCDebug(gpsLog) << "gpsPlugin:stopUpdates";
        pos_data_source->stopUpdates();
        enable_gps_readout=false;
    }

    //timerValues->stop();
    //timer->stop();
    return true;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#else
    Q_EXPORT_PLUGIN2(gpsPlugin, gpsPlugin)
#endif

