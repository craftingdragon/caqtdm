/*
 *  This file is part of the caQtDM Framework, it was developed in collaboration with
 *  the University of Lucerne (HSLU) as an Economy Project and the Paul Scherrer Institut.
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
 *  Authors:
 *    Erik Schwarz - PSI
 *    Hrvat Leo - HSLU
 *    Joel Müller - HSLU
 */

#include "opcua_plugin.h"
#include <QDebug>
#include <QOpcUaMultiDimensionalArray>
#include <QSettings>
#include <QThread>
#include <QtConcurrent/QtConcurrent>
#include "alarmdefs.h"
#include "fileFunctions.h"
#include "caQtDM_Plugins_global.h"
#include "opcua_core.h"
#include "searchfile.h"

#define PASSWORD_PLACEHOLDER "[HIDDEN]"
#define PLUGIN_PREFIX "opcua://"
#define PROTOCOL_PREFIX "opc.tcp://"

Q_LOGGING_CATEGORY(opcuaLog, "caqtdm.plugins.opcua")

OPCUAPlugin::OPCUAPlugin()
    : m_generalPasswordCredentials({"", ""})
{
    qCDebug(opcuaLog) << "OPCUAPlugin: Create";
    //QLoggingCategory::setFilterRules("qt.opcua.plugins.open62541*=false");
    m_mutexKnobDataP = Q_NULLPTR;
    m_messageWindowP = Q_NULLPTR;
    m_pemPassword = "";
}

QString OPCUAPlugin::pluginName()
{
    return "opcua";
}

int OPCUAPlugin::initCommunicationLayer(MutexKnobData *data,
                                        MessageWindow *messageWindow,
                                        QMap<QString, QString> options)
{
    qCDebug(opcuaLog) << "Initialized with num options: " << options.size();

    m_mutexKnobDataP = data;
    m_messageWindowP = messageWindow;

    QStringList opcua_database_files;

    QString url = (QString) qgetenv("CAQTDM_URL_DISPLAY_PATH");
    QString database_file = (QString) qgetenv("CAQTDM_OPCUA_DATABASE");

    opcua_database_files.append(database_file.split(","));

    if (!options.value("OPCUA_DATABASE", "").isEmpty())
        opcua_database_files.append(options.value("OPCUA_DATABASE"));

    fileFunctions fileFunction;
    foreach (QString opcua_database_file, opcua_database_files) {
        if (opcua_database_file.startsWith("\"") || opcua_database_file.startsWith("\'")) {
            opcua_database_file.remove(0, 1);
        }

        if (opcua_database_file.endsWith("\"") || opcua_database_file.endsWith("\'")) {
            opcua_database_file.remove(opcua_database_file.length() - 1, 1);
        }

        if (!url.isEmpty()) {
            fileFunction.checkFileAndDownload(opcua_database_file, url);
        }
        qCDebug(opcuaLog) << "Search for database file" << opcua_database_file;
        searchFile *s = new searchFile(opcua_database_file);
        QString fileNameFound = s->findFile();
        delete s;

        if (fileNameFound.isEmpty()) {
            if (m_messageWindowP) {
                QString msg = "OPCUA: Couldn't find opcua database file: " + opcua_database_file;
                m_messageWindowP->postMsgEvent(QtDebugMsg, msg.toUtf8().data());
            }
            continue;
        }else{
            qCDebug(opcuaLog) << "found a database file" << fileNameFound;
        }

        QFile file(fileNameFound);
        if (file.open(QIODevice::ReadOnly)) {
            QString msg = "opcua translation found: ";
            msg.append(fileNameFound);
            if (m_messageWindowP != Q_NULLPTR)
                m_messageWindowP->postMsgEvent(QtDebugMsg, msg.toUtf8().data());

            QTextStream in(&file);

            while (!in.atEnd()) {
                QString line = in.readLine();
                if (!line.trimmed().startsWith("#")) {
                    int equalIndex = line.indexOf(
                        "="); // Since Node Id's have '=' in them we have to make sure to only split at the first equal sign.
                    if (equalIndex > 0) {
                        QString key = line.left(equalIndex).trimmed();
                        QString val = line.mid(equalIndex + 1).trimmed();
                        m_translationMap.insert(key, val);
                    }
                }
            }

            file.close();

            if (m_messageWindowP) {
                msg = "OPCUA: Loaded database file: " + opcua_database_file;
                m_messageWindowP->postMsgEvent(QtDebugMsg, msg.toUtf8().data());
            }
        }
    }

    if (m_messageWindowP) {
        for (auto it = m_translationMap.constBegin(); it != m_translationMap.constEnd(); ++it) {
            QString msg = QString("OPCUA: %1 => %2").arg(it.key(), it.value());
            m_messageWindowP->postMsgEvent(QtDebugMsg, msg.toUtf8().data());
        }
    }

    if (m_messageWindowP) {
        QString msg = "OPCUA: Make sure the channels dont have semicolons in them. If neccessary, "
                      "use CAQTDM_OPCUA_DATABASE";
        m_messageWindowP->postMsgEvent(QtWarningMsg, msg.toUtf8().data());
    }

    return true;
}

bool OPCUAPlugin::copyStringToDataB(knobData &kData, const QString &newValue) const
{
    QByteArray newValueBytes = newValue.toUtf8();
    if (kData.edata.dataB && kData.edata.dataSize != (newValueBytes.size() + 1)) {
        free(kData.edata.dataB);
        kData.edata.dataB = Q_NULLPTR;
    }
    if (!kData.edata.dataB) {
        kData.edata.dataSize = newValueBytes.size() + 1;
        kData.edata.dataB = malloc(static_cast<size_t>(kData.edata.dataSize));
        if (!kData.edata.dataB) return false;
    }
    memcpy(kData.edata.dataB, newValueBytes.constData(), static_cast<size_t>(kData.edata.dataSize));
    return true;
}

bool OPCUAPlugin::publishCredentialsPV(MutexKnobData::MonitorHandle handle,
                                        const QString &value, bool initialize)
{
    QByteArray bytes = value.toUtf8();
    void *replacement = Q_NULLPTR;
    if (!value.isEmpty() || !initialize) {
        replacement = malloc(static_cast<size_t>(bytes.size() + 1));
        if (!replacement) return false;
        memcpy(replacement, bytes.constData(), static_cast<size_t>(bytes.size() + 1));
    }

    knobData lockData;
    if (!m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &lockData)) {
        free(replacement);
        return false;
    }
    m_mutexKnobDataP->DataLock(&lockData);
    bool updated = m_mutexKnobDataP->UpdateMutexKnobDataIfCurrent(handle, [&](knobData *stored) {
        if (initialize && !stored->edata.connected) {
            stored->edata.connected = true;
            stored->edata.valueCount = stored->edata.nelm = 0;
            stored->edata.dataSize = 0;
            stored->edata.fieldtype = caSTRING;
            stored->edata.accessR = true;
            stored->edata.accessW = true;
        }
        if (replacement) {
            free(stored->edata.dataB);
            stored->edata.dataB = replacement;
            stored->edata.dataSize = bytes.size() + 1;
            replacement = Q_NULLPTR;
        }
        if (!initialize) stored->edata.valueCount = stored->edata.nelm = 1;
        stored->edata.monitorCount++;
    });
    knobData published;
    bool delivered = updated && m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &published)
                     && m_mutexKnobDataP->SetMutexKnobDataReceivedIfCurrent(handle, &published);
    m_mutexKnobDataP->DataUnlock(&lockData);
    free(replacement);
    return delivered;
}

bool OPCUAPlugin::isGeneralUsernamePassword(const QString &pv) const
{
    return (pv == "username" || pv == "password");
}

bool OPCUAPlugin::isSpecificUsernamePassword(const QString &pv) const
{
    if (!pv.endsWith("/username") && !pv.endsWith("/password")) {
        return false;
    }

    if (pv.contains(PROTOCOL_PREFIX)) {
        return false;
    }

    return true;
}

bool OPCUAPlugin::isPemPassword(const QString &pv) const
{
    return pv == "pem_password";
}

int OPCUAPlugin::initializeCredentialsPV(int index)
{
    auto handle = m_mutexKnobDataP->GetMutexKnobDataHandle(index);
    knobData kData;
    if (!m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &kData)) return false;
    QString pv = QString::fromUtf8(kData.pv);
    QString newValue;

    if (isGeneralUsernamePassword(pv)) {
        if (pv == "username") {
            newValue = m_generalPasswordCredentials.username;
        } else if (pv == "password") {
            if (!m_generalPasswordCredentials.password.isEmpty()) {
                newValue = PASSWORD_PLACEHOLDER;
            }
        }
    } else if (isSpecificUsernamePassword(pv)) {
        QString host = pv;
        host.remove(pv.length() - 9, 9);
        host.remove(PLUGIN_PREFIX);
        if (pv.endsWith("username")) {
            if (m_passwordCredentialsForHost.contains(host)) {
                newValue = m_passwordCredentialsForHost[host].username;
            }
        } else if (pv.endsWith("password")) {
            if (m_passwordCredentialsForHost.contains(host)
                && !m_passwordCredentialsForHost[host].password.isEmpty()) {
                newValue = PASSWORD_PLACEHOLDER;
            }
        }
    } else if (isPemPassword(pv)) {
        if (!m_pemPassword.isEmpty()) {
            newValue = PASSWORD_PLACEHOLDER;
        }
    }

    if (!publishCredentialsPV(handle, newValue, true)) return false;
    if (pv == "username") m_usernameIndex = handle;
    else if (pv == "password") m_passwordIndex = handle;
    else if (isSpecificUsernamePassword(pv)) {
        QString host = getHostFromSpecificUsernamePassword(pv);
        if (!m_passwordCredentialsForHost.contains(host)) m_passwordCredentialsForHost[host] = {"", ""};
        if (pv.endsWith("username")) m_usernameIndexForHost[host] = handle;
        else m_passwordIndexForHost[host] = handle;
    } else if (isPemPassword(pv)) m_pemPasswordIndex = handle;

    return true;
}

bool OPCUAPlugin::isPasswordCredentialsValid(const PasswordCredentials &credentialsToCheck) const
{
    return (!credentialsToCheck.username.isEmpty() && !credentialsToCheck.password.isEmpty());
}

int OPCUAPlugin::pvAddMonitor(int index, knobData *kData, int rate, int skip)
{
    Q_UNUSED(rate);
    Q_UNUSED(skip);

    QString rawPV = QString::fromUtf8(kData->pv);
    if (rawPV.endsWith(".FTVL")) {
        QString regularPV = rawPV;
        regularPV.remove(rawPV.length() - 5, 5);
        const auto handle = m_mutexKnobDataP->GetMutexKnobDataHandle(index);
        if (!handle.isValid()) return false;
        m_epicsWaveformAttributePVs[regularPV].FTVL = handle;
        return true;
    } else if (rawPV.endsWith(".NELM")) {
        QString regularPV = rawPV;
        regularPV.remove(rawPV.length() - 5, 5);
        const auto handle = m_mutexKnobDataP->GetMutexKnobDataHandle(index);
        if (!handle.isValid()) return false;
        m_epicsWaveformAttributePVs[regularPV].NELM = handle;
        return true;
    } else if (isGeneralUsernamePassword(rawPV) || isSpecificUsernamePassword(rawPV)
               || isPemPassword(rawPV)) {
        return initializeCredentialsPV(index);
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(kData->pv, endpoint, nodeId)) {
        if (m_messageWindowP) {
            QString msg = "Invalid OPCUA PV format. Expected <endpoint>/ns=...; got: "
                          + QString::fromUtf8(kData->pv);
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return false;
    };

    const QString URI = endpoint + "/" + nodeId;

    const auto handle = m_mutexKnobDataP->GetMutexKnobDataHandle(index);
    if (!m_mutexKnobDataP->UpdateMutexKnobDataIfCurrent(handle, [&](knobData *stored) {
            qstrncpy(stored->edata.fec, endpoint.toLatin1().constData(), caqtdm_string_t_length);
        })) return false;

    int samplingIntervalMs = getUpdateIntervalFromKnobData(kData);
    SubscriptionSettings pendingSubscription = {nodeId, samplingIntervalMs};

    const MonitorRegistration registration{handle, kData->mutex};
    m_channelCache.insert(URI, registration);

    OpcUaCore *core;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_cores.contains(endpoint)) {
            m_cores[endpoint] = new OpcUaCore(this);
            m_connectionState[endpoint] = ConnectionState::NotConnected;

            core = m_cores[endpoint];
            QObject::connect(core,
                             &OpcUaCore::valueRead,
                             this,
                             [=](const QString &URI, const QVariant &value) {
                                 if (m_cores.value(endpoint) != core) return;
                                 auto range = m_channelCache.equal_range(URI);
                                 for (auto it = range.first; it != range.second; ++it)
                                     publishValue(it.value().handle, value);
                             });

            QObject::connect(core,
                             &OpcUaCore::accessLevelRead,
                             this,
                             [=](const QString &URI,
                                 const bool &readAccess,
                                 const bool &writeAccess) {
                                 if (m_cores.value(endpoint) != core) return;
                                 auto range = m_channelCache.equal_range(URI);
                                 for (auto it = range.first; it != range.second; ++it) {
                                     const auto handle = it.value().handle;
                                     knobData kData;
                                     if (!m_mutexKnobDataP->UpdateMutexKnobDataIfCurrent(handle, [&](knobData *stored) {
                                             stored->edata.connected = true;
                                             stored->edata.accessR = readAccess;
                                             stored->edata.accessW = writeAccess;
                                         }) || !m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &kData))
                                         continue;
                                     updateEpicsWaveformAttributePVs(handle);
                                     m_mutexKnobDataP->SetMutexKnobDataReceivedIfCurrent(handle, &kData);
                                 }
                             });

            QObject::connect(core, &OpcUaCore::disconnected, this, [=]() {
                if (m_cores.value(endpoint) != core) return;
                m_connectionState[endpoint] = ConnectionState::NotConnected;
                for (const auto &registration : m_knobDataIndicesForEndpoint[endpoint]) {
                    const auto handle = registration.handle;
                    if (!m_mutexKnobDataP->SetMutexKnobDataConnectedIfCurrent(handle, false))
                        continue;
                    updateEpicsWaveformAttributePVs(handle);
                }

                if (m_knobDataIndicesForEndpoint[endpoint].length() > 0) {
                    if (m_messageWindowP) {
                        QString err = QString("OPCUA: Connection failed for %1").arg(endpoint);
                        m_messageWindowP->postMsgEvent(QtCriticalMsg, err.toUtf8().data());
                    }
                }
            });

            QObject::connect(core, &OpcUaCore::connected, this, [=]() {
                if (m_cores.value(endpoint) != core) return;
                m_connectionState[endpoint] = ConnectionState::Connected;
                if (m_messageWindowP) {
                    QString info = QString("OPCUA: Connected to %1").arg(endpoint);
                    m_messageWindowP->postMsgEvent(QtInfoMsg, info.toUtf8().data());
                }

                // Subscribe to all pending nodeIds
                for (const auto &pendingSubscription : m_pendingSubscriptions[endpoint]) {
                    core->subscribeToNode(pendingSubscription);
                }
                m_pendingSubscriptions[endpoint].clear();
            });

            QObject::connect(core,
                             &OpcUaCore::attributeGotError,
                             this,
                             [=](const QString &URI, const QString &errorMsg) {
                                 if (m_cores.value(endpoint) != core) return;
                                 auto range = m_channelCache.equal_range(URI);
                                 for (auto it = range.first; it != range.second; ++it) {
                                     const auto handle = it.value().handle;
                                     qCCritical(opcuaLog)
                                         << "nodeId: " << nodeId << " got error: " << errorMsg;
                                     if (!m_mutexKnobDataP->UpdateMutexKnobDataIfCurrent(handle, [](knobData *stored) {
                                             stored->edata.severity = INVALID_ALARM;
                                             stored->edata.status = 1; // READ_ALARM
                                         })) continue;
                                     knobData kData;
                                     if (m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &kData))
                                         m_mutexKnobDataP->SetMutexKnobDataReceivedIfCurrent(handle, &kData);
                                 }
                             });

            QObject::connect(core,
                             &OpcUaCore::userMessage,
                             this,
                             [&](QtMsgType type, const QString &message) {
                                 if (m_messageWindowP) {
                                     m_messageWindowP->postMsgEvent(type, message.toUtf8().data());
                                 }
                             });
        } else {
            core = m_cores[endpoint];
        }
    }
    if (isPasswordCredentialsValid(m_generalPasswordCredentials)) {
        core->updatePasswordCredentials(m_generalPasswordCredentials);
    }

    m_knobDataIndicesForEndpoint[endpoint].push_back(registration);

    QMutexLocker lock(&m_mutex);
    ConnectionState state = m_connectionState[endpoint];

    if (state == ConnectionState::Connected) {
        core->subscribeToNode(pendingSubscription);
    } else {
        // Store subscription to do later
        m_pendingSubscriptions[endpoint].append(pendingSubscription);

        if (state == ConnectionState::NotConnected) {
            m_connectionState[endpoint] = ConnectionState::Connecting;

            // Start connection
            core->connectOpc(endpoint);
        }
        // Else the core is already connecting, meaning once it finishes it will subscribe to the stored subscription
    }
    return true;
}

int OPCUAPlugin::pvClearMonitor(knobData *kData)
{
    QMutexLocker lock(&m_mutex);
    auto matchesRegistration = [&](MutexKnobData::MonitorHandle handle) {
        knobData current;
        return m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &current)
               && current.index == kData->index && current.mutex == kData->mutex
               && qstrcmp(current.pv, kData->pv) == 0;
    };

    QString rawPV = QString::fromUtf8(kData->pv);
    if (rawPV.endsWith(".FTVL")) {
        QString regularPV = rawPV;
        regularPV.remove(rawPV.length() - 5, 5);
        auto it = m_epicsWaveformAttributePVs.find(regularPV);
        if (it != m_epicsWaveformAttributePVs.end() && matchesRegistration(it->FTVL)) {
            it->FTVL = {};
            if (!it->NELM.isValid()) m_epicsWaveformAttributePVs.erase(it);
        }
        return true;
    } else if (rawPV.endsWith(".NELM")) {
        QString regularPV = rawPV;
        regularPV.remove(rawPV.length() - 5, 5);
        auto it = m_epicsWaveformAttributePVs.find(regularPV);
        if (it != m_epicsWaveformAttributePVs.end() && matchesRegistration(it->NELM)) {
            it->NELM = {};
            if (!it->FTVL.isValid()) m_epicsWaveformAttributePVs.erase(it);
        }
        return true;
    } else if (rawPV == "username") {
        if (matchesRegistration(m_usernameIndex)) m_usernameIndex = {};
        return true;
    } else if (rawPV == "password") {
        if (matchesRegistration(m_passwordIndex)) m_passwordIndex = {};
        return true;
    } else if (isSpecificUsernamePassword(rawPV)) {
        QString host = getHostFromSpecificUsernamePassword(rawPV);
        if (rawPV.endsWith("/username")) {
            if (matchesRegistration(m_usernameIndexForHost.value(host))) m_usernameIndexForHost.remove(host);
        } else if (rawPV.endsWith("/password")) {
            if (matchesRegistration(m_passwordIndexForHost.value(host))) m_passwordIndexForHost.remove(host);
        }
        return true;
    } else if (isPemPassword(rawPV)) {
        if (matchesRegistration(m_pemPasswordIndex)) m_pemPasswordIndex = {};
        return true;
    }

    int index = kData->index;
    QString endpoint, nodeId;
    if (!resolveConnectionString(kData->pv, endpoint, nodeId)) {
        if (m_messageWindowP) {
            QString msg = "Invalid OPCUA PV format. Expected <endpoint>/ns=...; got: "
                          + QString::fromUtf8(kData->pv);
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return false;
    };

    const QString URI = endpoint + "/" + nodeId;

    if (nodeId.isEmpty()) {
        if (m_messageWindowP) {
            QString msg = QString("OPCUA: No nodeId found for index %1").arg(index);
            m_messageWindowP->postMsgEvent(QtDebugMsg, msg.toUtf8().data());
        }
        return false;
    }

    const auto matchesOrdinaryRegistration = [&](const MonitorRegistration &registration) {
        return registration.handle.index == kData->index && registration.mutex == kData->mutex;
    };
    bool removed = false;
    auto &endpointHandles = m_knobDataIndicesForEndpoint[endpoint];
    for (int i = endpointHandles.size() - 1; i >= 0; --i) {
        if (matchesOrdinaryRegistration(endpointHandles.at(i))) {
            endpointHandles.removeAt(i);
            removed = true;
        }
    }
    auto range = m_channelCache.equal_range(URI);
    for (auto it = range.first; it != range.second;) {
        if (matchesOrdinaryRegistration(it.value())) {
            it = m_channelCache.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (!removed) return true;

    // In case other knobDatas still use this node, return
    if (m_channelCache.contains(URI))
        return true;

    // Else unsubscribe from the node
    if (m_cores.contains(endpoint)) {
        OpcUaCore *core = m_cores[endpoint];
        if (core->hasSubscription(nodeId)) {
            core->unsubscribeFromNode(nodeId);
        }

        // Check if any other node is still connected to this core
        // Remove the core if its not used anymore
        if (!core->hasAnySubscriptions()) {
            core->disconnectOpc();
            core->deleteLater();
            m_cores.remove(endpoint);
        }
    }

    return true;
}

int OPCUAPlugin::pvFreeAllocatedData(knobData *kData)
{
    QMutexLocker locker(static_cast<QMutex *>(kData->mutex));
    if (kData->edata.dataB) {
        free(kData->edata.dataB);
        kData->edata.dataB = Q_NULLPTR;
    }
    return true;
}

QString OPCUAPlugin::getHostFromSpecificUsernamePassword(const QString &pv) const
{
    QString host = pv;
    host.remove(pv.length() - 9, 9);
    host.remove(PLUGIN_PREFIX);
    return host;
}

bool OPCUAPlugin::setCredentialsPV(const QString &pvString, const char *sdata)
{
    if (isGeneralUsernamePassword(pvString)) {
        MutexKnobData::MonitorHandle handle;
        QString newValue;
        if (pvString == "username") {
            handle = m_usernameIndex;
            newValue = QString::fromUtf8(sdata);
        } else if (pvString == "password") {
            handle = m_passwordIndex;
            newValue = PASSWORD_PLACEHOLDER;
        }
        if (!publishCredentialsPV(handle, newValue, false)) return false;
        if (pvString == "username") m_generalPasswordCredentials.username = QString::fromUtf8(sdata);
        else m_generalPasswordCredentials.password = QString::fromUtf8(sdata);

        if (isPasswordCredentialsValid(m_generalPasswordCredentials)) {
            for (auto it = m_cores.constKeyValueBegin(); it != m_cores.constKeyValueEnd(); it++) {
                QString host = it->first;
                host.remove(PROTOCOL_PREFIX);
                const PasswordCredentials &specificCredentials = m_passwordCredentialsForHost[host];
                if (isPasswordCredentialsValid(specificCredentials)) {
                    // Don't overwrite credentials for endpoints that already have endpoint-specific credentials
                    continue;
                }
                m_cores[it->first]->updatePasswordCredentials(m_generalPasswordCredentials);
            }
        }
        return true;
    } else if (isSpecificUsernamePassword(pvString)) {
        MutexKnobData::MonitorHandle handle;
        QString newValue;
        QString host = getHostFromSpecificUsernamePassword(pvString);
        if (pvString.endsWith("username")) {
            handle = m_usernameIndexForHost.value(host);
            newValue = QString::fromUtf8(sdata);
        } else if (pvString.endsWith("password")) {
            handle = m_passwordIndexForHost.value(host);
            newValue = PASSWORD_PLACEHOLDER;
        }
        if (!publishCredentialsPV(handle, newValue, false)) return false;
        if (pvString.endsWith("username")) m_passwordCredentialsForHost[host].username = QString::fromUtf8(sdata);
        else m_passwordCredentialsForHost[host].password = QString::fromUtf8(sdata);

        if (isPasswordCredentialsValid(m_passwordCredentialsForHost[host])) {
            QString endpoint = PROTOCOL_PREFIX + host;
            if (m_cores.contains(endpoint)) {
                m_cores[endpoint]->updatePasswordCredentials(m_passwordCredentialsForHost[host]);
            }
        }
        return true;
    } else if (isPemPassword(pvString)) {
        if (!publishCredentialsPV(m_pemPasswordIndex, PASSWORD_PLACEHOLDER, false)) return false;
        m_pemPassword = QString::fromUtf8(sdata);

        for (auto &core : m_cores) {
            core->setPemPassword(m_pemPassword);
        }

        return true;
    }

    return false;
}

int OPCUAPlugin::pvSetValue(
    char *pv, double rdata, int32_t idata, char *sdata, char *object, char *errmess, int forceType)
{
    Q_UNUSED(object);
    Q_UNUSED(forceType);

    QMutexLocker locker(&m_mutex);

    QString pvString = QString::fromUtf8(pv);

    if (isGeneralUsernamePassword(pvString) || isSpecificUsernamePassword(pvString)
        || isPemPassword(pvString)) {
        return setCredentialsPV(pvString, sdata);
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(pv, endpoint, nodeId)) {
        return false;
    }

    if (!m_cores.contains(endpoint)) {
        if (m_messageWindowP) {
            QString msg = "Tried writing to pv that's not connected correctly: " + endpoint;
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return false;
    }

    auto &core = m_cores[endpoint];
    return core->writeValue(nodeId, rdata, idata, sdata, errmess);
}

int OPCUAPlugin::pvSetWave(char *pv,
                           float *fdata,
                           double *ddata,
                           int16_t *data16,
                           int32_t *data32,
                           char *sdata,
                           int nelm,
                           char *object,
                           char *errmess)
{
    Q_UNUSED(object);

    QMutexLocker locker(&m_mutex);

    QString endpoint, nodeId;
    if (!resolveConnectionString(pv, endpoint, nodeId)) {
        return false;
    }

    if (!m_cores.contains(endpoint)) {
        if (m_messageWindowP) {
            QString msg = "Tried writing to pv that's not connected correctly: " + endpoint;
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return false;
    }

    auto &core = m_cores[endpoint];
    return core->writeValues(nodeId, fdata, ddata, data16, data32, sdata, nelm, errmess);
}

int OPCUAPlugin::pvGetTimeStamp(char *pv, char *timestamp)
{
    Q_UNUSED(pv);
    QString rawPV = QString::fromUtf8(pv);
    if (rawPV.endsWith(".FTVL") || rawPV.endsWith(".NELM")) {
        rawPV.remove(rawPV.length() - 5, 5);
    } else if (isGeneralUsernamePassword(rawPV) || isSpecificUsernamePassword(rawPV)
               || isPemPassword(rawPV)) {
        qstrncpy(timestamp, "Timestamp: N/A", MAX_STRING_LENGTH);
        return true;
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(rawPV.toUtf8().data(), endpoint, nodeId)) {
        return false;
    }

    if (m_cores.contains(endpoint)) {
        auto &core = m_cores[endpoint];
        QString nodeTimestamp = core->getTimestamp(nodeId);
        qstrncpy(timestamp, nodeTimestamp.toUtf8().data(), MAX_STRING_LENGTH);
    } else {
        if (m_messageWindowP) {
            QString msg = "[pvGetTimeStamp]: endpoint not configured: " + endpoint;
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return false;
    }

    return true;
}

int OPCUAPlugin::pvGetDescription(char *pv, char *description)
{
    QString rawPV = QString::fromUtf8(pv);
    if (rawPV.endsWith(".FTVL") || rawPV.endsWith(".NELM")) {
        qstrcpy(description, "I'm just here for compatibility reasons :)");
        return true;
    } else if (isGeneralUsernamePassword(rawPV)) {
        qstrncpy(description,
                 "Global value used across all opcua endpoints, unless overwritten per endpoint",
                 MAX_STRING_LENGTH);
        return true;
    } else if (isSpecificUsernamePassword(rawPV)) {
        qstrncpy(description,
                 "Endpoint specific value, overwrites all other definitions",
                 MAX_STRING_LENGTH);
        return true;
    } else if (isPemPassword(rawPV)) {
        qstrncpy(
            description,
            "Used to decrypt private key for OPCUA communication, overwrites all other definitions",
            MAX_STRING_LENGTH);
        return true;
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(pv, endpoint, nodeId)) {
        return false;
    }

    if (m_cores.contains(endpoint)) {
        auto &core = m_cores[endpoint];
        QString nodeDescription = core->getDescription(nodeId);
        qstrncpy(description, nodeDescription.toUtf8().data(), MAX_STRING_LENGTH);
    } else {
        if (m_messageWindowP) {
            QString msg = "[pvGetDescription]: endpoint not configured: " + endpoint;
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return false;
    }

    return true;
}

int OPCUAPlugin::pvClearEvent(void *ptr)
{
    knobData *kData = static_cast<knobData *>(ptr);

    QString plainKey = QString::fromUtf8(kData->pv);
    if (plainKey.endsWith(".FTVL") || plainKey.endsWith(".NELM")
        || isSpecificUsernamePassword(plainKey) || isGeneralUsernamePassword(plainKey)
        || isPemPassword(plainKey)) {
        return true;
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(kData->pv, endpoint, nodeId)) {
        return false;
    };

    QMutexLocker locker(&m_mutex);
    if (m_cores.contains(endpoint)) {
        m_cores[endpoint]->unsubscribeFromNode(nodeId);
    }

    return true;
}

int OPCUAPlugin::pvAddEvent(void *ptr)
{
    knobData *kData = static_cast<knobData *>(ptr);

    QString plainKey = QString::fromUtf8(kData->pv);
    if (plainKey.endsWith(".FTVL") || plainKey.endsWith(".NELM")
        || isGeneralUsernamePassword(plainKey) || isSpecificUsernamePassword(plainKey)
        || isPemPassword(plainKey)) {
        return true;
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(kData->pv, endpoint, nodeId)) {
        return false;
    };

    int samplingIntervalMs = getUpdateIntervalFromKnobData(kData);
    SubscriptionSettings pendingSubscription = {nodeId, samplingIntervalMs};

    QMutexLocker locker(&m_mutex);
    if (m_cores.contains(endpoint)) {
        m_cores[endpoint]->subscribeToNode(pendingSubscription);
    }

    return true;
}

int OPCUAPlugin::pvReconnect(knobData *kData)
{
    QString plainKey = QString::fromUtf8(kData->pv);
    if (plainKey.endsWith(".FTVL") || plainKey.endsWith(".NELM")
        || isGeneralUsernamePassword(plainKey) || isSpecificUsernamePassword(plainKey)
        || isPemPassword(plainKey)) {
        return true;
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(kData->pv, endpoint, nodeId)) {
        if (m_messageWindowP) {
            QString msg = "Invalid OPCUA PV format. Expected <endpoint>/ns=...; got: "
                          + QString::fromUtf8(kData->pv);
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return false;
    };

    int samplingIntervalMs = getUpdateIntervalFromKnobData(kData);
    SubscriptionSettings pendingSubscription = {nodeId, samplingIntervalMs};

    QMutexLocker lock(&m_mutex);
    if (!m_cores.contains(endpoint))
        return false;

    auto core = m_cores[endpoint];
    core->disconnectOpc();
    m_connectionState[endpoint] = ConnectionState::NotConnected;
    m_pendingSubscriptions[endpoint].append(pendingSubscription);

    core->connectOpc(endpoint);
    return true;
}

int OPCUAPlugin::pvDisconnect(knobData *kData)
{
    QString plainKey = QString::fromUtf8(kData->pv);
    if (plainKey.endsWith(".FTVL") || plainKey.endsWith(".NELM")
        || isGeneralUsernamePassword(plainKey) || isSpecificUsernamePassword(plainKey)
        || isPemPassword(plainKey)) {
        return true;
    }

    QString endpoint, nodeId;
    if (!resolveConnectionString(kData->pv, endpoint, nodeId)) {
        return false;
    };

    QMutexLocker lock(&m_mutex);
    if (m_cores.contains(endpoint)) {
        m_cores[endpoint]->disconnectOpc();
        m_connectionState[endpoint] = ConnectionState::NotConnected;
        qCDebug(opcuaLog) << "Disconnected from" << endpoint;
    }
    return true;
}

int OPCUAPlugin::FlushIO()
{
    return true;
}

int OPCUAPlugin::TerminateIO()
{
    QMutexLocker locker(&m_mutex);

    for (auto &core : m_cores) {
        if (core) {
            core->clearAllSubscriptions();
            core->disconnectOpc();
            core->deleteLater();
        }
    }

    m_cores.clear();
    m_connectionState.clear();
    m_pendingSubscriptions.clear();
    m_channelCache.clear();

    return true;
}

bool OPCUAPlugin::resolveConnectionString(char *pv, QString &endpoint, QString &nodeId) const
{
    QString plainKey = QString::fromUtf8(pv);

    if (plainKey.endsWith(".FTVL") || plainKey.endsWith(".NELM")
        || isGeneralUsernamePassword(plainKey) || isSpecificUsernamePassword(plainKey)) {
        qCWarning(opcuaLog) << "Invalid connection string, cannot write opcua to EPICS extensions: "
                            << plainKey;
        return false;
    }

    QString logicalKey = plainKey;
    logicalKey.remove(PLUGIN_PREFIX);
    QString fullConnection = m_translationMap.value(plainKey,
                                                    m_translationMap.value(logicalKey, ""));
    if (fullConnection.isEmpty()) {
        fullConnection = plainKey;
    }

    QString raw = fullConnection.remove(PLUGIN_PREFIX);

    if (!raw.startsWith(PROTOCOL_PREFIX)) {
        qCWarning(opcuaLog) << "Invalid connection string: " << fullConnection;
        return false;
    }

    int splitPos = raw.lastIndexOf("/ns=");
    if (splitPos < 0) {
        splitPos = raw.lastIndexOf("/i=");
        if (splitPos < 0) {
            qCWarning(opcuaLog) << "Invalid connection string: " << fullConnection;
            return false;
        }
    }

    endpoint = raw.left(splitPos);
    nodeId = raw.mid(splitPos + 1).trimmed();
    return true;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define QT_VARIANT_TYPE(value) value.typeId()
#else
#define QT_VARIANT_TYPE(value) value.type()
#endif

caType OPCUAPlugin::generateCaTypeFromVariant(const QVariant &value,
                                              bool &isArray,
                                              bool &isMatrix) const
{
    isArray = false;
    QVariant valueToCheck = value;
    isMatrix = valueToCheck.canConvert<QOpcUaMultiDimensionalArray>();

    if ((valueToCheck.canConvert<QVariantList>() || isMatrix)
        && !valueToCheck.canConvert<QString>()) {
        if (isMatrix) {
            valueToCheck = valueToCheck.value<QOpcUaMultiDimensionalArray>().valueArray();
        }
        QList<QVariant> list = valueToCheck.toList();
        if (list.length() > 0) {
            valueToCheck = list.constFirst();
            isArray = true;
        }
    }

    switch (QT_VARIANT_TYPE(valueToCheck)) {
    case QMetaType::Double:
        return caDOUBLE;
    case QMetaType::Float:
        return caFLOAT;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Long:
    case QMetaType::ULong:
        return caLONG;
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Bool:
        return caINT;
    case QMetaType::QString:
        return caSTRING;
    case QMetaType::QDateTime:
        return caSTRING;
    default:
        return caDOUBLE;
    }
}

bool OPCUAPlugin::updateKnobDataFromVariantSingle(knobData &kData,
                                                  const QVariant &value,
                                                  const caType &detectedType)
{
    kData.edata.valueCount = kData.edata.nelm = 1;
    switch (detectedType) {
    case caDOUBLE:
        kData.edata.rvalue = value.toDouble();
        kData.edata.ivalue = kData.edata.rvalue;
        kData.edata.precision = 8;
        break;
    case caFLOAT:
        kData.edata.rvalue = value.toFloat();
        kData.edata.ivalue = kData.edata.rvalue;
        kData.edata.precision = 4;
        break;
    case caLONG:
        kData.edata.ivalue = value.toInt();
        kData.edata.rvalue = kData.edata.ivalue;
        kData.edata.precision = 0;
        break;
    case caINT:
        kData.edata.ivalue = static_cast<int16_t>(value.toInt());
        kData.edata.rvalue = kData.edata.ivalue;
        kData.edata.precision = 0;
        break;
    case caSTRING:
        if (QT_VARIANT_TYPE(value)==QMetaType::QDateTime) {
            QDateTime now = QDateTime::currentDateTime();
            QString utcInfo = QTimeZone::systemTimeZone().displayName(now, QTimeZone::OffsetName);
            return copyStringToDataB(kData, value.toDateTime().toString(Qt::ISODateWithMs)+" ("+utcInfo+")" );
        } else return copyStringToDataB(kData, value.toString());
        break;
    default:
        kData.edata.rvalue = 0.0;
        kData.edata.ivalue = 0;
        break;
    }
    return true;
}

void OPCUAPlugin::updateEpicsWaveformAttributePVs(MutexKnobData::MonitorHandle referenceHandle)
{
    knobData reference;
    if (!m_mutexKnobDataP->GetMutexKnobDataIfCurrent(referenceHandle, &reference)) return;
    const QString rawPV = QString::fromUtf8(reference.pv);
    const auto it = m_epicsWaveformAttributePVs.constFind(rawPV);
    if (it == m_epicsWaveformAttributePVs.constEnd()) return;

    const auto publishAttribute = [&](MutexKnobData::MonitorHandle handle, int value) {
        if (!m_mutexKnobDataP->UpdateMutexKnobDataIfCurrent(handle, [&](knobData *stored) {
                stored->edata.connected = reference.edata.connected;
                stored->edata.ivalue = value;
                stored->edata.rvalue = value;
                stored->edata.fieldtype = caINT;
                stored->edata.accessR = true;
                stored->edata.accessW = false;
                stored->edata.monitorCount++;
            })) return;
        knobData published;
        if (m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &published))
            m_mutexKnobDataP->SetMutexKnobDataReceivedIfCurrent(handle, &published);
    };
    publishAttribute(it->FTVL, reference.edata.fieldtype);
    publishAttribute(it->NELM, reference.edata.valueCount);
}

bool OPCUAPlugin::updateKnobDataFromVariantArray(knobData &kData,
                                                 const QVariant &value,
                                                 const caType &detectedType)
{
    QList<QVariant> list = value.toList();
    int num_values = list.count();
    kData.edata.valueCount = kData.edata.nelm = num_values;

    switch (detectedType) {
    case caDOUBLE: {
        if (kData.edata.dataB && kData.edata.dataSize != num_values * sizeof(double)) {
            free(kData.edata.dataB);
            kData.edata.dataB = Q_NULLPTR;
        }

        if (!kData.edata.dataB) {
            kData.edata.dataSize = num_values * sizeof(double);
            kData.edata.dataB = malloc(static_cast<size_t>(kData.edata.dataSize));
            if (!kData.edata.dataB) return false;
        }

        double *intBuffer = static_cast<double *>(kData.edata.dataB);
        for (int i = 0; i < num_values; ++i) {
            intBuffer[i] = list[i].toDouble();
        }

        kData.edata.precision = 8;
        break;
    }
    case caFLOAT: {
        if (kData.edata.dataB && kData.edata.dataSize != num_values * sizeof(float)) {
            free(kData.edata.dataB);
            kData.edata.dataB = Q_NULLPTR;
        }

        if (!kData.edata.dataB) {
            kData.edata.dataSize = num_values * sizeof(float);
            kData.edata.dataB = malloc(static_cast<size_t>(kData.edata.dataSize));
            if (!kData.edata.dataB) return false;
        }

        float *intBuffer = static_cast<float *>(kData.edata.dataB);
        for (int i = 0; i < num_values; ++i) {
            intBuffer[i] = list[i].toFloat();
        }

        kData.edata.precision = 4;
        break;
    }
    case caLONG: {
        if (kData.edata.dataB && kData.edata.dataSize != num_values * sizeof(int32_t)) {
            free(kData.edata.dataB);
            kData.edata.dataB = Q_NULLPTR;
        }

        if (!kData.edata.dataB) {
            kData.edata.dataSize = num_values * sizeof(int32_t);
            kData.edata.dataB = malloc(static_cast<size_t>(kData.edata.dataSize));
            if (!kData.edata.dataB) return false;
        }

        int32_t *intBuffer = static_cast<int32_t *>(kData.edata.dataB);
        for (int i = 0; i < num_values; ++i) {
            intBuffer[i] = list[i].toInt();
        }

        kData.edata.precision = 0;
        break;
    }
    case caINT: {
        if (kData.edata.dataB && kData.edata.dataSize != num_values * sizeof(int16_t)) {
            free(kData.edata.dataB);
            kData.edata.dataB = Q_NULLPTR;
        }

        if (!kData.edata.dataB) {
            kData.edata.dataSize = num_values * sizeof(int16_t);
            kData.edata.dataB = malloc(static_cast<size_t>(kData.edata.dataSize));
            if (!kData.edata.dataB) return false;
        }

        int16_t *intBuffer = static_cast<int16_t *>(kData.edata.dataB);
        for (int i = 0; i < num_values; ++i) {
            intBuffer[i] = static_cast<int16_t>(list[i].toInt());
        }

        kData.edata.precision = 0;
        break;
    }
    case caSTRING: {
        QVariant firstValue = list.constFirst();
        return copyStringToDataB(kData, firstValue.toString());
    }
    default:
        kData.edata.rvalue = 0.0;
        kData.edata.ivalue = 0;
        break;
    }
    return true;
}

bool OPCUAPlugin::updateKnobDataFromVariant(knobData &kData, QVariant value, bool *isArrayResult)
{
    bool isArray = false;
    bool isMatrix = false;
    caType detectedType = generateCaTypeFromVariant(value, isArray, isMatrix);
    if (isMatrix) {
        value = value.value<QOpcUaMultiDimensionalArray>().valueArray();
    }
    kData.edata.fieldtype = detectedType;
    kData.edata.connected = 1;

    *isArrayResult = isArray;
    if (isArray)
        return updateKnobDataFromVariantArray(kData, value, detectedType);
    return updateKnobDataFromVariantSingle(kData, value, detectedType);
}

bool OPCUAPlugin::publishValue(MutexKnobData::MonitorHandle handle, const QVariant &value)
{
    knobData lockData;
    if (!m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &lockData)) return false;

    // Decode against an independent buffer. A knobData copy only borrows dataB;
    // decoding directly into that copy could free another registration's data.
    knobData decoded = lockData;
    decoded.edata.dataB = Q_NULLPTR;
    decoded.edata.dataSize = 0;
    bool isArray = false;
    if (!updateKnobDataFromVariant(decoded, value, &isArray)) {
        free(decoded.edata.dataB);
        return false;
    }
    const bool hasPayload = decoded.edata.dataB != Q_NULLPTR;

    m_mutexKnobDataP->DataLock(&lockData);
    const bool updated = m_mutexKnobDataP->UpdateMutexKnobDataIfCurrent(handle, [&](knobData *stored) {
        stored->edata.fieldtype = decoded.edata.fieldtype;
        stored->edata.connected = decoded.edata.connected;
        stored->edata.monitorCount++;
        stored->edata.valueCount = decoded.edata.valueCount;
        stored->edata.nelm = decoded.edata.nelm;
        stored->edata.rvalue = decoded.edata.rvalue;
        stored->edata.ivalue = decoded.edata.ivalue;
        stored->edata.precision = decoded.edata.precision;
        if (hasPayload) {
            free(stored->edata.dataB);
            stored->edata.dataB = decoded.edata.dataB;
            stored->edata.dataSize = decoded.edata.dataSize;
            decoded.edata.dataB = Q_NULLPTR;
        }
    });
    knobData published;
    const bool delivered = updated && m_mutexKnobDataP->GetMutexKnobDataIfCurrent(handle, &published)
                           && m_mutexKnobDataP->SetMutexKnobDataReceivedIfCurrent(handle, &published);
    m_mutexKnobDataP->DataUnlock(&lockData);
    free(decoded.edata.dataB);
    if (delivered && isArray) updateEpicsWaveformAttributePVs(handle);
    return delivered;
}

int OPCUAPlugin::getUpdateIntervalFromKnobData(knobData *kData) const
{
    if (!kData) {
        if (m_messageWindowP) {
            QString msg = "Received invalid KnobData for unknown PV";
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
        return 1;
    }
    int updateRateHz = kData->edata.repRate;
    if (updateRateHz == 0) {
        updateRateHz = 1;
        if (m_messageWindowP) {
            QString msg = "Invalid OPCUA PV refresh rate of 0 Hz. Defaulted to 1 Hz. PV: "
                          + QString::fromUtf8(kData->pv);
            m_messageWindowP->postMsgEvent(QtCriticalMsg, msg.toUtf8().data());
        }
    }
    int samplingIntervalMs = 1000 / updateRateHz;
    return samplingIntervalMs;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#else
Q_EXPORT_PLUGIN2(DemoPlugin, DemoPlugin)
#endif
