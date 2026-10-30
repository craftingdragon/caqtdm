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

#ifndef OPCUA_PLUGIN_H
#define OPCUA_PLUGIN_H

#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QtGlobal>
#include "controlsinterface.h"
#include "opcua_core.h"

//#define HARDWORK

#ifdef HARDWORK
#include <QtConcurrentRun>
#endif

// Holds the mutexKnobData indices for channels carrying EPICS waveform attributes corresponding to a channel (.NELM, .FTVL)
typedef struct
{
    int NELM_index;
    int FTVL_index;
} EpicsWaveformAttributePVs;

class Q_DECL_EXPORT OPCUAPlugin : public QObject, ControlsInterface
{
    Q_OBJECT
    Q_INTERFACES(ControlsInterface)
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "ch.psi.caqtdm.Plugin.ControlsInterface/1.0.opcua")
#endif

public:
    /**
     * @brief Creates plugin and disables open62541 logging
     */
    OPCUAPlugin();

    /**
     * @brief Returns the plugin name
     * @return "opcua"
     */
    QString pluginName();
    /**
     * @brief Initializes mutexKnobData and messageWindow pointers, loads translation map
     * @param data: mutexKnobData pointer
     * @param messageWindow: messageWindow pointer
     * @param options: optional options for the plugin
     * @return true
     */
    int initCommunicationLayer(MutexKnobData *data,
                               MessageWindow *messageWindow,
                               QMap<QString, QString> options);
    /**
     * @brief Subscribes to a new variable, connects to the endpoint if not already done
     * @param index: knobData index of the pv
     * @param kData: knobData for the pv, pv must contain valid connection string
     * @param rate: not used, update rate is taken directly from kData
     * @param skip: not used
     * @return true if the connection to the variable was initiated, else false (connection may still fail)
     */
    int pvAddMonitor(int index, knobData *kData, int rate, int skip);
    /**
     * @brief Unsubscribes from the given node
     * @param kData: knobData containing the pv with the connection string
     * @return true if the pv ends with .NELM/.FTVL or the node was unsubscribed
     */
    int pvClearMonitor(knobData *kData);
    /**
     * @brief Frees the data allocated to kData->edata.dataB, sets it to Q_NULLPTR
     * @param kData: knobData whose dataB should be freed
     * @return true
     */
    int pvFreeAllocatedData(knobData *kData);
    /**
     * @brief Updates a simple variable, auto-detects the correct type based on last OpcUa value
     * @param pv: has the connection string
     * @param rdata: double (8 bytes) data
     * @param idata: integer data (cast to int32_t or int16_t depending on previous OpcUa value)
     * @param sdata: string data
     * @param object: not used, can be null
     * @param errmess: output where an optional error message is copied to
     * @param forceType: not used
     * @return true if the write was successfully initiated, else false (write may still be rejected from OpcUa server)
     */
    int pvSetValue(char *pv,
                   double rdata,
                   int32_t idata,
                   char *sdata,
                   char *object,
                   char *errmess,
                   int forceType);
    /**
     * @brief Updates a waveform variable, auto-detects the correct type based on last OpcUa value
     * @param pv: has the connection string
     * @param fdata: float (4 byte per var) data
     * @param ddata: double (8 byte per var) data
     * @param data16: int16_t data
     * @param data32: int32_t data
     * @param sdata: string data
     * @param nelm: number of elements
     * @param object: not used, can be null
     * @param errmess: output where an optional error message is copied to
     * @return true if the write was successfully initiated, else false (write may still be rejected from OpcUa server)
     */
    int pvSetWave(char *pv,
                  float *fdata,
                  double *ddata,
                  int16_t *data16,
                  int32_t *data32,
                  char *sdata,
                  int nelm,
                  char *object,
                  char *errmess);
    /**
     * @brief Gets the OpcUa timestamp for the last received value of a variable
     * @param pv: has the connection string
     * @param timestamp: output where the description is copied to (MAX_STRING_LENGTH is maximum length copied)
     * @return true if the endpoint is registered (and the timestamp is set), else false
     */
    int pvGetTimeStamp(char *pv, char *timestamp);
    /**
     * @brief Gets the OpcUa description field
     * @param pv: has the connection string
     * @param description: output where the description is copied to (MAX_STRING_LENGTH is maximum length copied)
     * @return true if the endpoint is registered (and the description is set), else false
     */
    int pvGetDescription(char *pv, char *description);
    /**
     * @brief Disables the monitoring of a node
     * @param ptr: void pointer castable to a knobData* with pv to disable monitoring for
     * @return true if the pv is a valid connection string, else false
     */
    int pvClearEvent(void *ptr);
    /**
     * @brief Subscribes to a variable if not already subscribed
     * @param ptr: void pointer castable to a knobData* with pv to subscribe
     * @return true if pv is a valid connection string, else false
     */
    int pvAddEvent(void *ptr);
    /**
     * @brief Disconnects and reconnects the endpoint associate to the variable; (re-)subscribes to the variable
     * @param kData: knobData containing the pv with the endpoint encoded in it
     * @return true if the endpoint is already registered and the pv is a valid connection string, else false
     */
    int pvReconnect(knobData *kData);
    /**
     * @brief Disconnects the endpoint associated to a variable (not just the variable itself)
     * @param kData: knobData containing the pv with the endpoint encoded in it
     * @return false if the endpoint is not a valid connection string, else true
     */
    int pvDisconnect(knobData *kData);
    /**
     * @brief Not Implemented
     * @return true
     */
    int FlushIO();
    /**
     * @brief Stops all connections, deletes all associated variables
     * @return true
     */
    int TerminateIO();

private:
    enum class ConnectionState { NotConnected, Connecting, Connected };
    struct MonitorRegistration {
        MutexKnobData::MonitorHandle handle;
        void *mutex;
    };

    QMutex m_mutex;
    MutexKnobData *m_mutexKnobDataP;
    MessageWindow *m_messageWindowP;
    PasswordCredentials m_generalPasswordCredentials;
    QString m_pemPassword;
    // Registration handles for synthetic credential channels.
    MutexKnobData::MonitorHandle m_pemPasswordIndex;
    // knobData index for the general username for all channels
    MutexKnobData::MonitorHandle m_usernameIndex;
    // knobData index for the general password for all channels
    MutexKnobData::MonitorHandle m_passwordIndex;
    // Maps host to knobData index of respective username
    QMap<QString, MutexKnobData::MonitorHandle> m_usernameIndexForHost;
    // Maps host to knobData index of respective password
    QMap<QString, MutexKnobData::MonitorHandle> m_passwordIndexForHost;
    // Maps host to respective PasswordCredentials
    QMap<QString, PasswordCredentials> m_passwordCredentialsForHost;
    // Maps URI (endpoint + "/" + nodeId) to the original monitor registration.
    QMultiMap<QString, MonitorRegistration> m_channelCache;
    // Maps endpoint to core
    QMap<QString, OpcUaCore *> m_cores;
    // Maps endpoint to all monitor registrations handled by it.
    QMap<QString, QList<MonitorRegistration>> m_knobDataIndicesForEndpoint;
    // Maps endpoint to connectionstate
    QMap<QString, ConnectionState> m_connectionState;
    // Maps endpoint to list of SubscriptionSettings to subscribe to
    QMap<QString, QList<SubscriptionSettings>> m_pendingSubscriptions;
    // Maps pv (without .NELM /.FTVL) to struct containing knobData indices of those channel extensions
    QMap<QString, EpicsWaveformAttributePVs> m_epicsWaveformAttributePVs;
    // Maps pvs (as keys)) to values that should be used instead of them in the OpcUa context
    QMap<QString, QString> m_translationMap;

    /**
     * @brief Copies a string into the dataB container in knobdata safely
     * @param knobData: knobData to copy into
     * @param value: value to copy
     */
    bool copyStringToDataB(knobData &kData, const QString &value) const;
    bool publishCredentialsPV(MutexKnobData::MonitorHandle handle,
                              const QString &value, bool initialize);

    /**
     * @brief Checks if a pv is for a general username or password for all hosts
     * @param pv: string to check
     * @return true if it is, else false
     */
    bool isGeneralUsernamePassword(const QString &pv) const;

    /**
     * @brief Checks if a pv is a specific username or password for some host
     * @param pv: string to check
     * @return true if it is, else false
     */
    bool isSpecificUsernamePassword(const QString &pv) const;

    /**
     * @brief Checks if a pv is for the password of the pem key
     * @param pv: string ot check
     * @return true if it is, else false
     */
    bool isPemPassword(const QString &pv) const;

    /**
     * @brief Initialized either a general or a specific username or password pv or a pv containing the password to the pem, updating its value if anything is already stored
     * @param index: knobData index of the pv to initialize
     * @return true for success, false for failure
     */
    int initializeCredentialsPV(int index);

    /**
     * @brief Checks if a given PasswordCredentials is valid in a way it can be used to start a connection
     * This Check does not guarantee or even check correctness of the credentials on any system.
     * @param credentialsToCheck: credentials to check
     * @return true if credentials are valid, else false
     */
    bool isPasswordCredentialsValid(const PasswordCredentials &credentialsToCheck) const;

    /**
     * @brief Extracts the host from a pv for a host-specific username or password
     * @param pv: the pv to extract the host from
     * @return The extracted host
     */
    QString getHostFromSpecificUsernamePassword(const QString &pv) const;

    /**
     * @brief Sets either the username or the password for general or endpoint-specific credentials or for the pem key
     * @param pvString: pv whose value is to be used, and whose name defines what exactly needs to be set
     * @param sdata: string data containing the new value
     * @return true if the value has been set, else false
     */
    bool setCredentialsPV(const QString &pvString, const char *sdata);

    /**
     * @brief Creates a caType corresponding to a QVariant value
     * @param value: the QVariant value to check
     * @param isArray: output boolean that will be set to true if value is an array, else to false
     * @param isMatrix: output boolean that will be set to true if value is a matrix, else false
     * @return caType corresponding to the QVariant value
     */
    caType generateCaTypeFromVariant(const QVariant &value, bool &isArray, bool &isMatrix) const;
    /**
     * @brief Converts the update interval specified in a knobData (in Hz) and returns the milliseconds)
     * @param kData: knobData pointer to check
     * @return Update interval calculated in milliseconds, or 1000 milliseconds if invalid Hz specified (e.g. 0)
     */
    int getUpdateIntervalFromKnobData(knobData *kData) const;
    /**
     * @brief Resolves a connection string to an endpoint and a nodeId
     * @param pv: pv string to resolve
     * @param endpoint: output where the resolved endpoint will be copied to
     * @param nodeId: output where the resolved nodeId will be copied to
     * @return true if the outputs were both successfully extracted, else false
     */
    bool resolveConnectionString(char *pv, QString &endpoint, QString &nodeId) const;
    /**
     * @brief Updates knobData with a single value
     * @param kData: knobData to update
     * @param value: Value to store
     * @param detectedType: caType detected for the value
     */
    bool updateKnobDataFromVariantSingle(knobData &kData,
                                         const QVariant &value,
                                         const caType &detectedType);
    /**
     * @brief Updates knobData with a 1D-array of values (waveform)
     * @param kData: knobData to update
     * @param value: Value (waveform) to store
     * @param detectedType: caType detected for each variable in the 1D-array
     */
    bool updateKnobDataFromVariantArray(knobData &kData,
                                        const QVariant &value,
                                        const caType &detectedType);
    /**
     * @brief Updates knobData with a value, auto detects if it's a waveform and the type the element holds
     * @param kData: knobData to update
     * @param value: Value to store
     */
    bool updateKnobDataFromVariant(knobData &kData, QVariant value, bool *isArray);
    bool publishValue(MutexKnobData::MonitorHandle handle, const QVariant &value);
    /**
     * @brief Updates extension PVs holding EPICS-record data associated with the pv (.FTVL/.NELM)
     * @param rawPV: Base pv for which the extension PVs should be updated
     * @param referenceKnobData: knobData of the base pv
     */
    void updateEpicsWaveformAttributePVs(QString rawPV, const knobData &referenceKnobData);

#ifdef HARDWORK
    void updateHardwork();
#endif
};

#endif // OPCUA_PLUGIN_H
