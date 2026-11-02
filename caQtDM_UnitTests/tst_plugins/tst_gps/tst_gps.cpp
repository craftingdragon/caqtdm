#include <QtTest>
#include <QGeoPositionInfo>
#include "gps_plugin.h"

class FakePositionSource : public QGeoPositionInfoSource {
public:
    FakePositionSource() : QGeoPositionInfoSource(nullptr) {}
    void startUpdates() override {}
    void stopUpdates() override {}
    void requestUpdate(int = 0) override {}
    QGeoPositionInfo lastKnownPosition(bool = false) const override { return {}; }
    PositioningMethods supportedPositioningMethods() const override { return AllPositioningMethods; }
    int minimumUpdateInterval() const override { return 0; }
    Error error() const override { return NoError; }
};

class TestGpsPlugin : public QObject {
    Q_OBJECT
private slots:
    void registrationLifecycle();
};

void TestGpsPlugin::registrationLifecycle()
{
    MutexKnobData store;
    gpsPlugin plugin;
    FakePositionSource source;
    plugin.mutexknobdataP = &store;
    plugin.pos_data_source = &source;

    const int index = store.GetMutexKnobDataIndex();
    knobData first = {};
    first.index = index;
    qstrncpy(first.pv, "altitude", sizeof(first.pv));
    store.SetMutexKnobData(index, first);
    QVERIFY(plugin.pvAddMonitor(index, &first, 0, 0));
    QCOMPARE(store.GetMutexKnobData(index).edata.monitorCount, 1);

    QGeoPositionInfo fix(QGeoCoordinate(47.0, 8.0, 123.0), QDateTime::currentDateTimeUtc());
    plugin.positionUpdated(fix);
    QCOMPARE(store.GetMutexKnobData(index).edata.rvalue, 123.0);

    const int optionalIndex = store.GetMutexKnobDataIndex();
    knobData optional = {};
    optional.index = optionalIndex;
    optional.edata.rvalue = 17.0;
    qstrncpy(optional.pv, "GroundSpeed", sizeof(optional.pv));
    store.SetMutexKnobData(optionalIndex, optional);
    QVERIFY(plugin.pvAddMonitor(optionalIndex, &optional, 0, 0));
    const int optionalCount = store.GetMutexKnobData(optionalIndex).edata.monitorCount;
    plugin.positionUpdated(fix);
    QCOMPARE(store.GetMutexKnobData(optionalIndex).edata.rvalue, 17.0);
    QCOMPARE(store.GetMutexKnobData(optionalIndex).edata.monitorCount, optionalCount);

    QVERIFY(plugin.pvClearMonitor(&first));
    first.index = -1;
    store.SetMutexKnobData(index, first);
    knobData replacement = {};
    replacement.index = index;
    replacement.edata.rvalue = 999.0;
    qstrncpy(replacement.pv, "unrelated", sizeof(replacement.pv));
    store.SetMutexKnobData(index, replacement);
    plugin.positionUpdated(fix);
    QCOMPARE(store.GetMutexKnobData(index).edata.rvalue, 999.0);

    // Simulate a registration retained while its store slot is recycled.
    first.index = -1;
    store.SetMutexKnobData(index, first);
    first.index = index;
    store.SetMutexKnobData(index, first);
    QVERIFY(plugin.pvAddMonitor(index, &first, 0, 0));
    first.index = -1;
    store.SetMutexKnobData(index, first);
    store.SetMutexKnobData(index, replacement);
    plugin.positionUpdated(fix);
    QCOMPARE(store.GetMutexKnobData(index).edata.rvalue, 999.0);
    QMetaObject::invokeMethod(&plugin, "updateInterface", Qt::DirectConnection);
    QCOMPARE(store.GetMutexKnobData(index).edata.rvalue, 999.0);
    plugin.pos_data_source = nullptr;
}

QTEST_MAIN(TestGpsPlugin)
#include "tst_gps.moc"
