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
 *    Erik Schwarz
 *  Contact details:
 *    erik.schwarz@psi.ch
 */

#include "tst_caqtdm_lib.h"

#include <caapplynumeric.h>
#include <cainclude.h>
#include <canumeric.h>
#include <caspinbox.h>
#include <cascript.h>
#include <MessageWindow.h>
#include <cawavetable.h>
#include <gensoftpv.h>
#include <QSignalSpy>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QMutexLocker>

#include <cfloat>
#include <climits>

class WaveWriteProbe : public InternalPlugin
{
public:
    knobData *source = Q_NULLPTR;
    bool unlocked = true;
    bool copied = true;
    bool typed = true;
    bool fallback = false;
    int calls = 0;
    QVector<double> values;

    bool pvSetWave(knobData *, float *f, double *d, int16_t *s, int32_t *l,
                   char *c, int count, char *, char *) override
    {
        record(f, d, s, l, c, count);
        return !fallback;
    }

    int pvSetWave(char *, float *f, double *d, int16_t *s, int32_t *l,
                  char *c, int count, char *, char *) override
    {
        record(f, d, s, l, c, count);
        return 1;
    }

private:
    void record(float *f, double *d, int16_t *s, int32_t *l, char *c, int count)
    {
        ++calls;
        QMutex *mutex = static_cast<QMutex *>(source->mutex);
        const bool acquired = mutex->tryLock();
        unlocked = unlocked && acquired;
        if(acquired) mutex->unlock();
        typed = typed && (int(f != Q_NULLPTR) + int(d != Q_NULLPTR)
                         + int(s != Q_NULLPTR) + int(l != Q_NULLPTR) + int(c != Q_NULLPTR) == 1);
        values.clear();
        for(int i = 0; i < count; ++i) {
            switch(source->edata.fieldtype) {
            case caDOUBLE: copied = copied && d != source->edata.dataB; values.append(d[i]); break;
            case caFLOAT: copied = copied && f != source->edata.dataB; values.append(f[i]); break;
            case caINT: copied = copied && s != source->edata.dataB; values.append(s[i]); break;
            case caLONG: copied = copied && l != source->edata.dataB; values.append(l[i]); break;
            case caCHAR: copied = copied && c != source->edata.dataB; values.append((unsigned char)c[i]); break;
            }
        }
    }
};

void TestCaQtDM_Lib::waveformWriteReleasesDataMutex_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("format");
    QTest::addColumn<double>("expected");
    QTest::addColumn<bool>("fallback");
    QTest::newRow("double") << int(genSoftPV::Double) << "7.5" << int(caWaveTable::decimal) << 7.5 << false;
    QTest::newRow("float") << int(genSoftPV::Float) << "7.5" << int(caWaveTable::decimal) << 7.5 << false;
    QTest::newRow("short") << int(genSoftPV::Int) << "7" << int(caWaveTable::decimal) << 7.0 << false;
    QTest::newRow("long") << int(genSoftPV::Long) << "7" << int(caWaveTable::decimal) << 7.0 << false;
    QTest::newRow("char") << int(genSoftPV::Char) << "65" << int(caWaveTable::decimal) << 65.0 << false;
    QTest::newRow("char-text") << int(genSoftPV::Char) << "A" << int(caWaveTable::string) << 65.0 << false;
    QTest::newRow("legacy-fallback") << int(genSoftPV::Double) << "7.5" << int(caWaveTable::decimal) << 7.5 << true;
}

void TestCaQtDM_Lib::waveformWriteReleasesDataMutex()
{
    QFETCH(int, type);
    QFETCH(QString, text);
    QFETCH(int, format);
    QFETCH(double, expected);
    QFETCH(bool, fallback);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV *definition = new genSoftPV(m_parentAS);
    definition->setVariable("waveWriteTest");
    definition->setDataType(static_cast<genSoftPV::DataType>(type));
    definition->setNelm(3);
    definition->setValue("1;2;3");
    m_caQtDM_Lib->HandleWidget(definition, "", true, true);
    caWaveTable *table = new caWaveTable(m_parentAS);
    table->setPV("internal://waveWriteTest");
    m_caQtDM_Lib->HandleWidget(table, "", false, false);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);

    WaveWriteProbe probe;
    probe.source = m_mutexKnobData->getMutexKnobDataPV(table, table->getPV());
    QVERIFY(probe.source);
    probe.fallback = fallback;
    const QVariant originalInterface = table->property("Interface");
    const QByteArray originalData(static_cast<const char *>(probe.source->edata.dataB), probe.source->edata.dataSize);
    table->setProperty("Interface", QVariantList{QVariant::fromValue(static_cast<void *>(qobject_cast<ControlsInterface *>(&probe)))});
    m_caQtDM_Lib->TreatRequestedWave(table->getPV(), text,
                                  static_cast<caWaveTable::FormatType>(format), 1, table);
    QCOMPARE(probe.calls, fallback ? 2 : 1);
    QVERIFY2(probe.unlocked, "Plugin called while waveform data mutex is held");
    QVERIFY2(probe.copied, "Plugin received the monitor buffer instead of a snapshot");
    QVERIFY2(probe.typed, "Only the matching waveform pointer must be supplied");
    QCOMPARE(probe.values, QVector<double>({1.0, expected, 3.0}));
    QCOMPARE(QByteArray(static_cast<const char *>(probe.source->edata.dataB), probe.source->edata.dataSize), originalData);

    // Exercise synchronous publication as well as the non-blocking lock probe.
    table->setProperty("Interface", originalInterface);
    if(type == genSoftPV::Char) return; // InternalPlugin::setWaveForPv has no char buffer support.
    m_caQtDM_Lib->TreatRequestedWave(table->getPV(), text,
                                  static_cast<caWaveTable::FormatType>(format), 1, table);
    QVector<double> published;
    for(int i = 0; i < 3; ++i) {
        switch(probe.source->edata.fieldtype) {
        case caDOUBLE: published.append(static_cast<double *>(probe.source->edata.dataB)[i]); break;
        case caFLOAT: published.append(static_cast<float *>(probe.source->edata.dataB)[i]); break;
        case caINT: published.append(static_cast<int16_t *>(probe.source->edata.dataB)[i]); break;
        case caLONG: published.append(static_cast<int32_t *>(probe.source->edata.dataB)[i]); break;
        }
    }
    QCOMPARE(published, QVector<double>({1.0, expected, 3.0}));
}

// registers a genSoftPV-defined internal channel and pumps one publish cycle
static genSoftPV *registerInternalGenSoftPV(CaQtDM_Lib *lib, InternalPlugin *plugin, QWidget *host,
                                            const QString &variable, const QString &hopr,
                                            const QString &lopr, const QString &drvl = QString(),
                                            const QString &drvh = QString(), int precision = -1)
{
    genSoftPV *softpv = new genSoftPV(host);
    softpv->setVariable(variable);
    softpv->setDataType(genSoftPV::Double);
    if(!hopr.isEmpty()) softpv->setHopr(hopr);
    if(!lopr.isEmpty()) softpv->setLopr(lopr);
    if(!drvl.isEmpty()) softpv->setDrvl(drvl);
    if(!drvh.isEmpty()) softpv->setDrvh(drvh);
    if(precision >= 0) softpv->setPrecision(precision);
    lib->HandleWidget(softpv, "", true, true);

    QMetaObject::invokeMethod(plugin, "updateChannels", Qt::DirectConnection);
    return softpv;
}

// writes a field of a running internal channel through the real plugin API
// (the caput equivalent) and lets it publish the resulting change
static void writeInternalField(InternalPlugin *plugin, const QString &variable, const QString &field,
                               double value)
{
    // no "internal://" scheme prefix: CaQtDM_Lib::addMonitor() strips that
    // before ever reaching the plugin interface, and InternalChannel::splitField()
    // expects the already-stripped form
    QByteArray pv = (variable + "." + field).toLatin1();
    plugin->pvSetValue(pv.data(), value, 0, Q_NULLPTR, Q_NULLPTR, Q_NULLPTR, 0);
}

// shared across caApplyNumeric/caNumeric/caSpinbox; each caller needs a unique
// variable name since InternalPlugin caches channels by base name
template<class WidgetT>
static void checkChannelDispLimits(CaQtDM_Lib *lib, InternalPlugin *plugin, QWidget *host,
                                    const QString &variable)
{
    registerInternalGenSoftPV(lib, plugin, host, variable, "90", "10");

    WidgetT *w = new WidgetT(host);
    w->setLimitsMode(WidgetT::Channel);
    w->setPV("internal://" + variable);
    lib->HandleWidget(w, "", false, false);

    QMetaObject::invokeMethod(plugin, "updateChannels", Qt::DirectConnection);

    QCOMPARE(w->getMinValue(), 10.0);
    QCOMPARE(w->getMaxValue(), 90.0);

    // the actual purpose of HOPR/LOPR: values outside the range are rejected
    w->setValue(50.0);
    QCOMPARE(w->value(), 50.0);
    w->setValue(500.0);   // above HOPR
    QCOMPARE(w->value(), 50.0);
    w->setValue(-500.0);  // below LOPR
    QCOMPARE(w->value(), 50.0);

    delete w;
}

template<class WidgetT>
static void checkFallbackWithoutLimits(CaQtDM_Lib *lib, InternalPlugin *plugin, QWidget *host,
                                        const QString &variable)
{
    // neither HOPR/LOPR nor DRVL/DRVH configured
    registerInternalGenSoftPV(lib, plugin, host, variable, "", "");

    WidgetT *w = new WidgetT(host);
    w->setLimitsMode(WidgetT::Channel);
    w->setPV("internal://" + variable);
    lib->HandleWidget(w, "", false, false);

    QMetaObject::invokeMethod(plugin, "updateChannels", Qt::DirectConnection);

    QCOMPARE(w->getMinValue(), -100000.0);
    QCOMPARE(w->getMaxValue(), 100000.0);

    delete w;
}

template<class WidgetT>
static void checkUserModeIgnoresChannel(CaQtDM_Lib *lib, InternalPlugin *plugin, QWidget *host,
                                        const QString &variable)
{
    registerInternalGenSoftPV(lib, plugin, host, variable, "90", "10");

    WidgetT *w = new WidgetT(host);
    w->setLimitsMode(WidgetT::User);
    w->setMinValue(1.0);
    w->setMaxValue(2.0);
    w->setPV("internal://" + variable);
    lib->HandleWidget(w, "", false, false);

    QMetaObject::invokeMethod(plugin, "updateChannels", Qt::DirectConnection);

    QCOMPARE(w->getMinValue(), 1.0);
    QCOMPARE(w->getMaxValue(), 2.0);

    delete w;
}

template<class WidgetT>
static void checkWidgetPicksUpFieldWrite(CaQtDM_Lib *lib, InternalPlugin *plugin, QWidget *host,
                                         const QString &variable)
{
    // HOPR/LOPR inside DRVL/DRVH: the widget follows HOPR/LOPR first
    registerInternalGenSoftPV(lib, plugin, host, variable, "90", "10", "0", "100");

    WidgetT *w = new WidgetT(host);
    w->setLimitsMode(WidgetT::Channel);
    w->setPV("internal://" + variable);
    lib->HandleWidget(w, "", false, false);

    QMetaObject::invokeMethod(plugin, "updateChannels", Qt::DirectConnection);

    QCOMPARE(w->getMinValue(), 10.0);
    QCOMPARE(w->getMaxValue(), 90.0);

    // reset HOPR/LOPR to 0 through the real plugin write path (like a caput
    // to NAME.HOPR/NAME.LOPR); the widget must recognize the change and fall
    // back to DRVL/DRVH
    writeInternalField(plugin, variable, "HOPR", 0.0);
    writeInternalField(plugin, variable, "LOPR", 0.0);

    QCOMPARE(w->getMinValue(), 0.0);
    QCOMPARE(w->getMaxValue(), 100.0);

    delete w;
}

// maxChannelPrecision caps the decimals taken from the channel; default 4 is
// what caQtDM_Lib had hard coded before the property existed
template<class WidgetT>
static void checkMaxChannelPrecision(CaQtDM_Lib *lib, InternalPlugin *plugin, QWidget *host,
                                     const QString &variable)
{
    // no HOPR/LOPR: the limits fall back to +-100000
    registerInternalGenSoftPV(lib, plugin, host, variable, "", "", QString(), QString(), 8);

    WidgetT *w = new WidgetT(host);
    w->setPrecisionMode(WidgetT::Channel);
    w->setPV("internal://" + variable);
    lib->HandleWidget(w, "", false, false);

    QMetaObject::invokeMethod(plugin, "updateChannels", Qt::DirectConnection);

    QCOMPARE(w->getMaxChannelPrecision(), 4);
    QCOMPARE(w->decDigits(), 4);

    // raising the cap lets the channel precision through on the next initialize
    w->setMaxChannelPrecision(10);
    writeInternalField(plugin, variable, "PREC", 8.0);
    QCOMPARE(w->decDigits(), 8);

    delete w;
}

void TestCaQtDM_Lib::initTestCase()
{
    // code to be executed before the first test function

    m_fakeFileOpenWindow = Q_NULLPTR;
    m_parentAS = Q_NULLPTR;
    m_mutexKnobData = Q_NULLPTR;
    m_internalPlugin = Q_NULLPTR;
    m_caQtDM_Lib = Q_NULLPTR;
}

void TestCaQtDM_Lib::init()
{
    // code to be executed before each test function

    m_fakeFileOpenWindow = new FakeFileOpenWindow(Q_NULLPTR);
    m_parentAS = new caInclude(m_fakeFileOpenWindow);
    m_mutexKnobData = new MutexKnobData();
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateDirect); // synchronous widget updates

    m_internalPlugin = new InternalPlugin();
    m_internalPlugin->initCommunicationLayer(m_mutexKnobData, Q_NULLPTR, QMap<QString, QString>());

    QMap<QString, ControlsInterface *> interfaces;
    interfaces.insert("internal", qobject_cast<ControlsInterface *>(m_internalPlugin));

    m_caQtDM_Lib = new CaQtDM_Lib(m_fakeFileOpenWindow,
                                  "",
                                  "",
                                  m_mutexKnobData,
                                  interfaces,
                                  Q_NULLPTR,
                                  false,
                                  m_parentAS,
                                  {});
}

void TestCaQtDM_Lib::cleanupTestCase()
{
    // code to be executed after the last test function
}

void TestCaQtDM_Lib::cleanup()
{
    // code to be executed after each test function

    delete m_caQtDM_Lib;
    delete m_internalPlugin;
    delete m_mutexKnobData;
    delete m_fakeFileOpenWindow;
}

void TestCaQtDM_Lib::scriptWritesExistingScalar_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QVariant>("expected");
    QTest::addColumn<bool>("warning");
    QTest::newRow("double") << int(genSoftPV::Double) << QStringLiteral("42.25") << QVariant(42.25) << false;
    QTest::newRow("float") << int(genSoftPV::Float) << QStringLiteral("-3.75") << QVariant(-3.75) << false;
    QTest::newRow("short-round-positive") << int(genSoftPV::Int) << QStringLiteral("3.5") << QVariant(4.0) << false;
    QTest::newRow("short-round-negative") << int(genSoftPV::Int) << QStringLiteral("-3.5") << QVariant(-4.0) << false;
    QTest::newRow("long-max") << int(genSoftPV::Long) << QStringLiteral("2147483647") << QVariant(2147483647.0) << false;
    QTest::newRow("long-min") << int(genSoftPV::Long) << QStringLiteral("-2147483648") << QVariant(-2147483648.0) << false;
    QTest::newRow("char") << int(genSoftPV::Char) << QStringLiteral("255") << QVariant(255.0) << false;
    QTest::newRow("enum") << int(genSoftPV::Enum) << QStringLiteral("2") << QVariant(2.0) << false;
    QTest::newRow("string") << int(genSoftPV::String) << QStringLiteral("'Grüezi'") << QVariant(QStringLiteral("Grüezi")) << false;
    QTest::newRow("empty-string") << int(genSoftPV::String) << QStringLiteral("''") << QVariant(QString()) << false;
    QTest::newRow("numeric-string") << int(genSoftPV::Double) << QStringLiteral("'42.25'") << QVariant(42.25) << false;
    QTest::newRow("overflow") << int(genSoftPV::Long) << QStringLiteral("2147483648") << QVariant(0.0) << true;
    QTest::newRow("char-negative") << int(genSoftPV::Char) << QStringLiteral("-1") << QVariant(0.0) << true;
    QTest::newRow("enum-range") << int(genSoftPV::Enum) << QStringLiteral("3") << QVariant(0.0) << true;
    QTest::newRow("float-overflow") << int(genSoftPV::Float) << QStringLiteral("1e100") << QVariant(0.0) << true;
    QTest::newRow("nan") << int(genSoftPV::Double) << QStringLiteral("NaN") << QVariant(0.0) << true;
    QTest::newRow("infinity") << int(genSoftPV::Double) << QStringLiteral("Infinity") << QVariant(0.0) << true;
    QTest::newRow("object") << int(genSoftPV::Double) << QStringLiteral("({bad: 42})") << QVariant(0.0) << true;
    QTest::newRow("unrepresentable-string") << int(genSoftPV::String) << QStringLiteral("'€'") << QVariant(QStringLiteral("0")) << true;
}

void TestCaQtDM_Lib::scriptWritesExistingScalar()
{
    QFETCH(int, type);
    QFETCH(QString, expression);
    QFETCH(QVariant, expected);
    QFETCH(bool, warning);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptOutputTarget"));
    source.setDataType(static_cast<genSoftPV::DataType>(type));
    source.setEnumStrings({QStringLiteral("OFF"), QStringLiteral("ON"), QStringLiteral("FAULT")});
    source.setValue(QStringLiteral("0"));
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    knobData *data = m_mutexKnobData->getMutexKnobDataPV(&source, QStringLiteral("scriptOutputTarget"));
    QVERIFY(data);
    QTRY_VERIFY(data->edata.connected);
    const short originalType = data->edata.fieldtype;
    const int nextIndex = m_mutexKnobData->GetMutexKnobDataIndex();

    caScript script(m_parentAS);
    script.setOutputChannels({{QStringLiteral("internal://scriptOutputTarget"), QStringLiteral("result")}});
    script.setScript(QStringLiteral("outputs.result.value = %1;").arg(expression));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    if (warning) QTest::ignoreMessage(QtWarningMsg, QRegularExpression("caScript .*output .* ignored: .*"));
    QSignalSpy finished(&script, &caScript::scriptFinished);
    script.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
    if (warning) QVERIFY(script.getLastOutputText().isEmpty());
    else QCOMPARE(script.getLastOutputText(), QStringLiteral("internal://scriptOutputTarget = ") + expected.toString());
    QCOMPARE(m_mutexKnobData->GetMutexKnobDataIndex(), nextIndex);
    QCOMPARE(data->edata.fieldtype, originalType);
    if (type == genSoftPV::String) {
        QMutexLocker lock(static_cast<QMutex *>(data->mutex));
        QCOMPARE(QString::fromLatin1(static_cast<const char *>(data->edata.dataB)), expected.toString());
    } else {
        QCOMPARE(data->edata.rvalue, expected.toDouble());
    }
}

void TestCaQtDM_Lib::scriptWritesInternalWaveform_data()
{
    QTest::addColumn<bool>("direct");
    QTest::newRow("timed") << false;
    QTest::newRow("direct") << true;
}

void TestCaQtDM_Lib::scriptWritesInternalWaveform()
{
    QFETCH(bool, direct);
    m_mutexKnobData->UpdateMechanism(direct ? MutexKnobData::UpdateDirect : MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable("scriptWaveformOutput");
    source.setDataType(genSoftPV::Char);
    source.setNelm(4);
    source.setValue("17;18;19;20");
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);
    auto *scalar = registerInternalGenSoftPV(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                            QStringLiteral("scriptWaveformScalar"), "", "");
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    auto *data = m_mutexKnobData->getMutexKnobDataPV(&source, QStringLiteral("scriptWaveformOutput"));
    QVERIFY(data);
    QVERIFY(data->edata.connected);
    QCOMPARE(data->edata.valueCount, 4);
    bool delivered = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &source,
            [&source, &delivered](int, QWidget *widget, const QString &, const QString &,
                                  const QString &, const knobData &sample) {
        if (widget != &source) return;
        auto *lock = static_cast<QMutex *>(sample.mutex);
        delivered = lock->tryLock();
        if (delivered) lock->unlock();
    });

    caScript script(m_parentAS);
    script.setOutputChannels({{"internal://scriptWaveformOutput.VAL", "image"},
                              {"internal://scriptWaveformScalar", "scalar"}});
    script.setScript("outputs.image.value = [0, 127.5, 255];");
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QSignalSpy finished(&script, &caScript::scriptFinished);
    script.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.last().first().toBool());
    QTRY_VERIFY(delivered);
    QCOMPARE(data->edata.valueCount, 3);
    QCOMPARE(QByteArray(static_cast<const char *>(data->edata.dataB), 3), QByteArray::fromHex("0080ff"));
    QCOMPARE(script.getLastOutputText(), QStringLiteral("internal://scriptWaveformOutput = 3 samples"));

    const QStringList rejected = {
        "[]", "[, 2]", "[[0]]", "[1, Infinity]", "[1, 256]", "[1, 2, 3, 4, 5]",
        "new Array(1048577)", "Object.defineProperty([0], '0', {get: function() { return 1; }})"
    };
    for (const QString &value : rejected) {
        script.setScript(QStringLiteral("outputs.image.value = %1; outputs.scalar.value = 42;").arg(value));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("caScript .*output .* ignored: .*"));
        const int count = finished.count();
        script.executeScript();
        QTRY_COMPARE(finished.count(), count + 1);
        QVERIFY(finished.last().first().toBool());
        QCOMPARE(data->edata.valueCount, 3);
        QCOMPARE(QByteArray(static_cast<const char *>(data->edata.dataB), 3), QByteArray::fromHex("0080ff"));
        QCOMPARE(m_mutexKnobData->getMutexKnobDataPV(scalar, QStringLiteral("scriptWaveformScalar"))->edata.rvalue, 42.0);
    }
}

void TestCaQtDM_Lib::scriptKeepsInternalOutputBinding()
{
    genSoftPV target(m_parentAS);
    target.setVariable("scriptCollision");
    m_caQtDM_Lib->HandleWidget(&target, "", true, true);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    knobData *record = m_mutexKnobData->getMutexKnobDataPV(&target, "scriptCollision");
    QVERIFY(record);
    QTRY_VERIFY(record->edata.connected);
    const int targetIndex = record->index;

    caScript script(m_parentAS);
    script.setOutputChannels({{"internal://scriptCollision.val", "result"}});
    script.setScript("outputs.result.value = 42;");
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);

    // A widget-local monitor from another plugin must not shadow the internal target.
    knobData collision = *record;
    collision.index = m_mutexKnobData->GetMutexKnobDataIndex();
    collision.dispW = &script;
    collision.thisW = nullptr;
    collision.mutex = nullptr;
    qstrncpy(collision.pluginName, "epics3", sizeof(collision.pluginName));
    m_mutexKnobData->SetMutexKnobData(collision.index, collision);

    QSignalSpy finished(&script, &caScript::scriptFinished);
    script.executeScript();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("caScript .*Channel bindings are fixed.*"));
    script.setOutputChannels({{"internal://differentTarget", "result"}});
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(script.getLastOutputText(), QStringLiteral("internal://scriptCollision = 42"));
    QCOMPARE(m_mutexKnobData->GetMutexKnobData(targetIndex).edata.rvalue, 42.0);

    // The target represents a monitor in a different panel and survives this close.
    knobData survivingTarget = m_mutexKnobData->GetMutexKnobData(targetIndex);
    survivingTarget.thisW = nullptr;
    m_mutexKnobData->SetMutexKnobData(targetIndex, survivingTarget);
    script.setScript("outputs.result.value = 99;");
    script.executeScript();
    QCloseEvent close;
    QCoreApplication::sendEvent(m_caQtDM_Lib, &close);
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(m_mutexKnobData->GetMutexKnobData(targetIndex).edata.rvalue, 42.0);
}

void TestCaQtDM_Lib::scriptInvalidWaveformClearsReadiness()
{
    genSoftPV source(m_parentAS);
    source.setVariable("scriptInvalidWave");
    source.setNelm(3);
    source.setValue("1;2;3");
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setExecutionMode(caScript::DataDriven);
    script.setInputChannels({{"internal://scriptInvalidWave", "sample"}});
    script.setScript("if (inputs.sample.value.length !== 3) throw Error('bad sample');");
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QSignalSpy updates(&script, &caScript::inputValueUpdated);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(!updates.isEmpty());

    QSignalSpy finished(&script, &caScript::scriptFinished);
    script.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.last().first().toBool());

    const knobData *record = m_mutexKnobData->getMutexKnobDataPV(&script, "scriptInvalidWave");
    QVERIFY(record);
    knobData malformed = *record;
    malformed.edata.valueCount = malformed.edata.dataSize / sizeof(double) + 1;
    m_caQtDM_Lib->Callback_UpdateWidget(malformed.index, &script, "", "", "", malformed);
    script.executeScript(); // Would set the widget busy immediately if the old sample were still ready.
    QSignalSpy warnings(&script, &caScript::scriptWarning);
    const knobData valid = *record;
    m_caQtDM_Lib->Callback_UpdateWidget(valid.index, &script, "", "", "", valid);
    script.executeScript();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(finished.last().first().toBool());
    QVERIFY(warnings.isEmpty());
}

void TestCaQtDM_Lib::scriptReceivesUntruncatedText()
{
    const QByteArray latin1 = QByteArray(9000, 'a') + QByteArray::fromHex("fc");
    const QByteArray utf8 = QByteArray(9000, 'a') + QByteArray::fromHex("c3bc");
    genSoftPV latinSource(m_parentAS), utfSource(m_parentAS);
    latinSource.setVariable("scriptLongLatin1");
    utfSource.setVariable("scriptLongUtf8");
    latinSource.setDataType(genSoftPV::String);
    utfSource.setDataType(genSoftPV::String);
    latinSource.setValue(QString::fromLatin1(latin1));
    utfSource.setValue(QString::fromLatin1(utf8));
    m_caQtDM_Lib->HandleWidget(&latinSource, "", true, true);
    m_caQtDM_Lib->HandleWidget(&utfSource, "", true, true);

    caScript script(m_parentAS);
    script.setExecutionMode(caScript::DataDriven);
    script.setInputChannels({{"internal://scriptLongLatin1", "latin"}, {"internal://scriptLongUtf8", "utf"}});
    script.setScript("if (inputs.latin.rawBytes.length !== 9001 || inputs.utf.rawBytes.length !== 9002"
                     " || inputs.utf.value.length !== 9001"
                     " || inputs.latin.fromLatin1() !== inputs.utf.fromUtf8()"
                     " || inputs.utf.fromUtf8().charCodeAt(9000) !== 252)"
                     " throw Error('wrong text or bytes');");
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QSignalSpy updates(&script, &caScript::inputValueUpdated);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updates.count() >= 2);
    QSignalSpy finished(&script, &caScript::scriptFinished);
    script.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.last().first().toBool());
}

void TestCaQtDM_Lib::scriptSkipsInvalidOutputs_data()
{
    QTest::addColumn<QString>("badChannel");
    QTest::addColumn<bool>("duplicate");
    QTest::addColumn<QString>("badValue");
    QTest::newRow("missing") << QStringLiteral("internal://missing") << false << QStringLiteral("99");
    QTest::newRow("external") << QStringLiteral("epics3://notAllowed") << false << QStringLiteral("99");
    QTest::newRow("duplicate-val") << QStringLiteral("internal://duplicate") << true << QStringLiteral("99");
    QTest::newRow("duplicate-same-value") << QStringLiteral("internal://duplicate") << true << QStringLiteral("100");
    QTest::newRow("invalid-value") << QStringLiteral("internal://duplicate") << false << QStringLiteral("NaN");
}

void TestCaQtDM_Lib::scriptSkipsInvalidOutputs()
{
    QFETCH(QString, badChannel);
    QFETCH(bool, duplicate);
    QFETCH(QString, badValue);
    genSoftPV *good = registerInternalGenSoftPV(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                               QStringLiteral("scriptGoodOutput"), "", "");
    genSoftPV *bad = registerInternalGenSoftPV(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                              QStringLiteral("duplicate"), "", "");
    const int nextIndex = m_mutexKnobData->GetMutexKnobDataIndex();
    caScript script(m_parentAS);
    QList<caScript::InputChannel> outputs = {
        {QStringLiteral("internal://scriptGoodOutput"), QStringLiteral("good")},
        {badChannel, QStringLiteral("bad")}
    };
    if (duplicate) outputs.append({badChannel + QStringLiteral(".VAL"), QStringLiteral("again")});
    script.setOutputChannels(outputs);
    script.setScript(QStringLiteral("outputs.good.value = 42; outputs.bad.value = %1;").arg(badValue)
        + (duplicate ? QStringLiteral("outputs.again.value = 100;") : QString()));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("caScript .*output .* ignored: .*"));
    if (duplicate) QTest::ignoreMessage(QtWarningMsg, QRegularExpression("caScript .*output .* ignored: .*"));
    QSignalSpy finished(&script, &caScript::scriptFinished);
    script.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
    QCOMPARE(m_mutexKnobData->getMutexKnobDataPV(good, QStringLiteral("scriptGoodOutput"))->edata.rvalue, 42.0);
    QCOMPARE(m_mutexKnobData->getMutexKnobDataPV(bad, QStringLiteral("duplicate"))->edata.rvalue, 0.0);
    QCOMPARE(m_mutexKnobData->GetMutexKnobDataIndex(), nextIndex);
    QVERIFY(!m_mutexKnobData->getMutexKnobDataPV(&script, QStringLiteral("missing")));
}

void TestCaQtDM_Lib::scriptPublishesScaledInput_data()
{
    QTest::addColumn<bool>("useMonitor");
    QTest::newRow("manual") << false;
    QTest::newRow("selected-monitor") << true;
}

void TestCaQtDM_Lib::scriptPublishesScaledInput()
{
    QFETCH(bool, useMonitor);
    genSoftPV *score = registerInternalGenSoftPV(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                                QStringLiteral("scriptScore"), "", "");
    genSoftPV *factor = registerInternalGenSoftPV(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                                 QStringLiteral("scriptFactor"), "", "");
    genSoftPV *result = registerInternalGenSoftPV(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                                 QStringLiteral("scriptScaledScore"), "", "");
    Q_UNUSED(score);
    Q_UNUSED(factor);
    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptScore"), QStringLiteral("score")},
                             {QStringLiteral("internal://scriptFactor"), QStringLiteral("factor")}});
    script.setOutputChannels({{QStringLiteral("internal://scriptScaledScore"), QStringLiteral("result")}});
    script.setScript(QStringLiteral("outputs.result.value = inputs.score.value * inputs.factor.value;"));
    if (useMonitor) {
        script.setExecutionMode(caScript::DataDriven);
        script.setTriggerMode(caScript::SelectedInput);
        script.setTriggerInput(QStringLiteral("factor"));
    }
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QSignalSpy finished(&script, &caScript::scriptFinished);
    const auto run = [&](double input, double scale, double expected, int count) {
        writeInternalField(m_internalPlugin, QStringLiteral("scriptScore"), QStringLiteral("VAL"), input);
        writeInternalField(m_internalPlugin, QStringLiteral("scriptFactor"), QStringLiteral("VAL"), scale);
        if (!useMonitor) script.executeScript();
        QTRY_COMPARE(finished.count(), count);
        QVERIFY(finished.last().first().toBool());
        QCOMPARE(m_mutexKnobData->getMutexKnobDataPV(result, QStringLiteral("scriptScaledScore"))->edata.rvalue, expected);
    };
    run(200, 2, 400, 1);
    run(300, 3, 900, 2);
    run(400, 4, 1600, 3);
    script.setScript(QStringLiteral("outputs.result.value = 123; throw new Error('Discard');"));
    script.executeScript();
    QTRY_COMPARE(finished.count(), 4);
    QVERIFY(!finished.last().first().toBool());
    QCOMPARE(m_mutexKnobData->getMutexKnobDataPV(result, QStringLiteral("scriptScaledScore"))->edata.rvalue, 1600.0);
    QCOMPARE(script.getLastOutputText(), QStringLiteral("internal://scriptScaledScore = 1600"));
}

void TestCaQtDM_Lib::scriptReceivesNumericWaveform_data()
{
    QTest::addColumn<int>("sourceType");
    QTest::addColumn<QString>("typeName");
    QTest::addColumn<int>("used");
    QTest::addColumn<QString>("expected");
    for (const auto type : {genSoftPV::Double, genSoftPV::Float, genSoftPV::Int}) {
        const QString name = type == genSoftPV::Double ? QStringLiteral("Double")
            : type == genSoftPV::Float ? QStringLiteral("Float") : QStringLiteral("Short");
        const QString values = type == genSoftPV::Int ? QStringLiteral("[-2,0,3]") : QStringLiteral("[-2,0,3.75]");
        QTest::newRow(qPrintable(name + "-used-length")) << int(type) << name << 3 << values;
        QTest::newRow(qPrintable(name + "-one-element")) << int(type) << name << 1 << QStringLiteral("[-2]");
        QTest::newRow(qPrintable(name + "-empty")) << int(type) << name << 0 << QStringLiteral("[]");
    }
    QTest::newRow("Long-used-length") << int(genSoftPV::Long) << QStringLiteral("Long") << 3
        << QStringLiteral("[-2147483648,0,2147483647]");
    QTest::newRow("Long-one-element") << int(genSoftPV::Long) << QStringLiteral("Long") << 1
        << QStringLiteral("[-2147483648]");
    QTest::newRow("Long-empty") << int(genSoftPV::Long) << QStringLiteral("Long") << 0 << QStringLiteral("[]");
}

void TestCaQtDM_Lib::scriptReceivesNumericWaveform()
{
    QFETCH(int, sourceType);
    QFETCH(QString, typeName);
    QFETCH(int, used);
    QFETCH(QString, expected);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptFloatingPointWaveform"));
    source.setDataType(static_cast<genSoftPV::DataType>(sourceType));
    source.setNelm(5);
    source.setNord(used);
    source.setValue(sourceType == genSoftPV::Long
        ? QStringLiteral("-2147483648;0;2147483647;99;100") : sourceType == genSoftPV::Int
        ? QStringLiteral("-2;0;3;99;100") : QStringLiteral("-2;0;3.75;99;100"));
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptFloatingPointWaveform"), QStringLiteral("samples")}});
    script.setScript(QStringLiteral(
        "const values = inputs.samples.value;\n"
        "if (!Array.isArray(values)) throw new Error('Expected an array');\n"
        "if (inputs.samples.fieldtype !== EpicsType.%2) throw new Error('Incorrect type');\n"
        "if (JSON.stringify(values) !== JSON.stringify(%1)) throw new Error('Incorrect values or length');\n"
        "if (values.length) values[0] = 999;").arg(expected, typeName));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);
    const knobData *input = m_mutexKnobData->getMutexKnobDataPV(&script, QStringLiteral("scriptFloatingPointWaveform"));
    QVERIFY(input);
    QCOMPARE(input->edata.nelm, 5);
    QCOMPARE(input->edata.valueCount, used);
    QCOMPARE(input->edata.dimCount, 0);
    const int elementSize = sourceType == genSoftPV::Double ? sizeof(double)
        : sourceType == genSoftPV::Float ? sizeof(float)
        : sourceType == genSoftPV::Int ? sizeof(int16_t) : sizeof(int32_t);
    QByteArray originalBytes;
    {
        QMutexLocker lock(static_cast<QMutex *>(input->mutex));
        if(used > 0) originalBytes = QByteArray(static_cast<const char *>(input->edata.dataB), used * elementSize);
    }
    QVariantList expectedBytes;
    for(unsigned char byte : originalBytes) expectedBytes.append(int(byte));
    const QString expectedJson = QString::fromUtf8(QJsonDocument::fromVariant(expectedBytes).toJson(QJsonDocument::Compact));
    script.setScript(script.getScript() + QStringLiteral(
        "\nconst bytes = inputs.samples.rawBytes;\n"
        "if (!(bytes instanceof Uint8Array)) throw new Error('Missing waveform raw bytes');\n"
        "if (JSON.stringify(Array.from(bytes)) !== JSON.stringify(%1))\n"
        "    throw new Error('Incorrect waveform bytes or used length');\n"
        "if (bytes.length) bytes[0] ^= 255;\n").arg(expectedJson));
    QSignalSpy finished(&script, &caScript::scriptFinished);
    for (int run = 1; run <= 2; ++run) {
        script.executeScript();
        QTRY_COMPARE(finished.count(), run);
        QVERIFY(finished.last().first().toBool());
    }
    if(used > 0) {
        QMutexLocker lock(static_cast<QMutex *>(input->mutex));
        QCOMPARE(QByteArray(static_cast<const char *>(input->edata.dataB), used * elementSize), originalBytes);
    }
}

void TestCaQtDM_Lib::scriptReceivesCharWaveform_data()
{
    QTest::addColumn<int>("used");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<QString>("sourceValues");
    QTest::addColumn<QByteArray>("expectedBytes");
    QTest::newRow("used-length") << 3 << QStringLiteral("ABC")
        << QStringLiteral("65;66;67;88;89") << QByteArray("ABC");
    QTest::newRow("one-element") << 1 << QStringLiteral("A")
        << QStringLiteral("65;66;67;88;89") << QByteArray("A");
    QTest::newRow("empty") << 0 << QString()
        << QStringLiteral("65;66;67;88;89") << QByteArray();
    QTest::newRow("embedded-null-and-high-byte") << 3 << QStringLiteral("A")
        << QStringLiteral("65;0;255;88;89") << QByteArray::fromHex("4100ff");
}

void TestCaQtDM_Lib::scriptReceivesCharWaveform()
{
    QFETCH(int, used);
    QFETCH(QString, expected);
    QFETCH(QString, sourceValues);
    QFETCH(QByteArray, expectedBytes);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptCharWaveform"));
    source.setDataType(genSoftPV::Char);
    source.setNelm(5);
    source.setNord(used);
    source.setValue(sourceValues);
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptCharWaveform"), QStringLiteral("text")}});
    script.setScript(QStringLiteral(
        "if (typeof inputs.text.value !== 'string' || inputs.text.value !== '%1')\n"
        "    throw new Error('Incorrect char waveform text or length');\n"
        "if (inputs.text.fieldtype !== EpicsType.Char) throw new Error('Incorrect source type');\n"
        "inputs.text.value = 'changed';").arg(expected));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);
    QVariantList bytes;
    for(unsigned char byte : expectedBytes) bytes.append(int(byte));
    const QString expectedJson = QString::fromUtf8(QJsonDocument::fromVariant(bytes).toJson(QJsonDocument::Compact));
    script.setScript(script.getScript() + QStringLiteral(
        "\nconst bytes = inputs.text.rawBytes;\n"
        "if (!(bytes instanceof Uint8Array)) throw new Error('Missing char waveform raw bytes');\n"
        "if (JSON.stringify(Array.from(bytes)) !== JSON.stringify(%1))\n"
        "    throw new Error('Incorrect char waveform bytes or used length');\n"
        "if (bytes.length) bytes[0] ^= 255;\n").arg(expectedJson));
    QSignalSpy finished(&script, &caScript::scriptFinished);
    for (int run = 1; run <= 2; ++run) {
        script.executeScript();
        QTRY_COMPARE(finished.count(), run);
        QVERIFY(finished.last().first().toBool());
    }
    const knobData *input = m_mutexKnobData->getMutexKnobDataPV(&script, QStringLiteral("scriptCharWaveform"));
    QVERIFY(input);
    QMutexLocker lock(static_cast<QMutex *>(input->mutex));
    QCOMPARE(QByteArray(static_cast<const char *>(input->edata.dataB), used), expectedBytes);
}

void TestCaQtDM_Lib::scriptReceivesStringWaveform_data()
{
    QTest::addColumn<int>("used");
    QTest::addColumn<QString>("expected");
    QTest::newRow("used-length-with-empty-text-and-umlaut") << 3 << QStringLiteral("[\"A\",\"\",\"Gr\\uFFFDezi\"]");
    QTest::newRow("one-element") << 1 << QStringLiteral("[\"A\"]");
    QTest::newRow("empty") << 0 << QStringLiteral("[]");
}

void TestCaQtDM_Lib::scriptReceivesStringWaveform()
{
    QFETCH(int, used);
    QFETCH(QString, expected);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptStringWaveform"));
    source.setDataType(genSoftPV::String);
    source.setNelm(5);
    source.setNord(used);
    source.setValue(QStringLiteral("A;;Grüezi;unused;unused"));
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptStringWaveform"), QStringLiteral("texts")}});
    script.setScript(QStringLiteral(
        "if (!Array.isArray(inputs.texts.value) || JSON.stringify(inputs.texts.value) !== '%1')\n"
        "    throw new Error('Incorrect string waveform values or length');\n"
        "if (inputs.texts.fieldtype !== EpicsType.String) throw new Error('Incorrect source type');\n"
        "inputs.texts.value[0] = 'changed';").arg(expected));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);
    QSignalSpy finished(&script, &caScript::scriptFinished);
    for (int run = 1; run <= 2; ++run) {
        script.executeScript();
        QTRY_COMPARE(finished.count(), run);
        QVERIFY(finished.last().first().toBool());
    }
}

void TestCaQtDM_Lib::scriptReceivesMatrix_data()
{
    QTest::addColumn<int>("sourceType");
    QTest::addColumn<QString>("typeName");
    QTest::addColumn<QString>("sourceValues");
    QTest::addColumn<int>("rows");
    QTest::addColumn<int>("columns");
    QTest::addColumn<QString>("expected");
    const auto addShapes = [](genSoftPV::DataType type, const QString &name,
                              const QStringList &values, const QStringList &expected) {
        QTest::newRow(qPrintable(name + "-two-by-three"))
            << int(type) << name << values.join(';') << 2 << 3 << expected.at(0);
        QTest::newRow(qPrintable(name + "-single-row"))
            << int(type) << name << values.mid(0, 3).join(';') << 1 << 3 << expected.at(1);
        QTest::newRow(qPrintable(name + "-single-column"))
            << int(type) << name << values.mid(0, 3).join(';') << 3 << 1 << expected.at(2);
        QTest::newRow(qPrintable(name + "-single-element"))
            << int(type) << name << values.first() << 1 << 1 << expected.at(3);
    };
    addShapes(genSoftPV::Double, "Double", {"1", "2", "3", "4", "5", "6"},
              {"[[1,2,3],[4,5,6]]", "[[1,2,3]]", "[[1],[2],[3]]", "[[1]]"});
    addShapes(genSoftPV::Float, "Float", {"-3.75", "0", "2.5", "4", "5", "6"},
              {"[[-3.75,0,2.5],[4,5,6]]", "[[-3.75,0,2.5]]", "[[-3.75],[0],[2.5]]", "[[-3.75]]"});
    addShapes(genSoftPV::Int, "Short", {"-32768", "0", "32767", "4", "5", "6"},
              {"[[-32768,0,32767],[4,5,6]]", "[[-32768,0,32767]]", "[[-32768],[0],[32767]]", "[[-32768]]"});
    addShapes(genSoftPV::Long, "Long", {"-2147483648", "0", "2147483647", "4", "5", "6"},
              {"[[-2147483648,0,2147483647],[4,5,6]]", "[[-2147483648,0,2147483647]]",
               "[[-2147483648],[0],[2147483647]]", "[[-2147483648]]"});
    addShapes(genSoftPV::Char, "Char", {"0", "65", "255", "4", "5", "6"},
              {"[[0,65,255],[4,5,6]]", "[[0,65,255]]", "[[0],[65],[255]]", "[[0]]"});
}

void TestCaQtDM_Lib::scriptReceivesMatrix()
{
    QFETCH(int, sourceType);
    QFETCH(QString, typeName);
    QFETCH(QString, sourceValues);
    QFETCH(int, rows);
    QFETCH(int, columns);
    QFETCH(QString, expected);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptMatrix"));
    source.setDataType(static_cast<genSoftPV::DataType>(sourceType));
    source.setMatrixRows(rows);
    source.setMatrixColumns(columns);
    source.setValue(sourceValues);
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptMatrix"), QStringLiteral("matrix")}});
    script.setScript(QStringLiteral(
        "const values = inputs.matrix.value;\n"
        "if (!Array.isArray(values) || !values.every(Array.isArray))\n"
        "    throw new Error('Expected nested arrays');\n"
        "if (JSON.stringify(values) !== '%1') throw new Error('Incorrect matrix shape or order');\n"
        "if (inputs.matrix.fieldtype !== EpicsType.%2) throw new Error('Incorrect source type');\n"
        "values[0][0] = 999;").arg(expected, typeName));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);
    const knobData *input = m_mutexKnobData->getMutexKnobDataPV(&script, QStringLiteral("scriptMatrix"));
    QVERIFY(input);
    QCOMPARE(input->edata.dimCount, 2);
    QCOMPARE(input->edata.dim[0], rows);
    QCOMPARE(input->edata.dim[1], columns);
    QCOMPARE(input->edata.valueCount, rows * columns);
    const int elementSize = sourceType == genSoftPV::Double ? sizeof(double)
        : sourceType == genSoftPV::Float ? sizeof(float)
        : sourceType == genSoftPV::Int ? sizeof(int16_t)
        : sourceType == genSoftPV::Long ? sizeof(int32_t) : sizeof(uint8_t);
    QByteArray originalBytes;
    {
        QMutexLocker lock(static_cast<QMutex *>(input->mutex));
        if(input->edata.dataB) {
            originalBytes = QByteArray(static_cast<const char *>(input->edata.dataB), rows * columns * elementSize);
        }
    }
    QVariantList expectedBytes;
    for(unsigned char byte : originalBytes) expectedBytes.append(int(byte));
    const QString expectedJson = QString::fromUtf8(QJsonDocument::fromVariant(expectedBytes).toJson(QJsonDocument::Compact));
    script.setScript(script.getScript() + QStringLiteral(
        "\nconst bytes = inputs.matrix.rawBytes;\n"
        "if (!(bytes instanceof Uint8Array)) throw new Error('Missing matrix raw bytes');\n"
        "if (JSON.stringify(Array.from(bytes)) !== JSON.stringify(%1))\n"
        "    throw new Error('Incorrect matrix bytes');\n"
        "if (bytes.length) bytes[0] ^= 255;\n").arg(expectedJson));
    QSignalSpy finished(&script, &caScript::scriptFinished);
    for (int run = 1; run <= 2; ++run) {
        script.executeScript();
        QTRY_COMPARE(finished.count(), run);
        QVERIFY(finished.last().first().toBool());
    }
    if(!originalBytes.isEmpty()) {
        QMutexLocker lock(static_cast<QMutex *>(input->mutex));
        QCOMPARE(QByteArray(static_cast<const char *>(input->edata.dataB), originalBytes.size()), originalBytes);
    }
}

void TestCaQtDM_Lib::scriptReceivesMetadata_data()
{
    QTest::addColumn<QString>("shape");
    QTest::addColumn<double>("value");
    QTest::addColumn<int>("status");
    QTest::addColumn<int>("severity");
    QTest::newRow("scalar-no-alarm") << QStringLiteral("scalar") << 0.0 << 0 << 0;
    QTest::newRow("scalar-high") << QStringLiteral("scalar") << 8.0 << 4 << 1;
    QTest::newRow("scalar-hihi") << QStringLiteral("scalar") << 10.0 << 3 << 2;
    QTest::newRow("scalar-low") << QStringLiteral("scalar") << -8.0 << 6 << 1;
    QTest::newRow("scalar-lolo") << QStringLiteral("scalar") << -10.0 << 5 << 2;
    QTest::newRow("waveform") << QStringLiteral("waveform") << 0.0 << 0 << 0;
    QTest::newRow("matrix") << QStringLiteral("matrix") << 0.0 << 0 << 0;
}

void TestCaQtDM_Lib::scriptReceivesMetadata()
{
    QFETCH(QString, shape);
    QFETCH(double, value);
    QFETCH(int, status);
    QFETCH(int, severity);
    const bool waveform = shape == QStringLiteral("waveform");
    const bool matrix = shape == QStringLiteral("matrix");
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptMetadata"));
    source.setDataType(genSoftPV::Double);
    source.setValue(matrix ? QStringLiteral("0;1;2;3;4;5") : waveform
        ? QStringLiteral("0;1;2;99;100") : QString::number(value));
    source.setUnits(QStringLiteral("V"));
    source.setPrecision(2);
    source.setDrvl(QStringLiteral("-12"));
    source.setDrvh(QStringLiteral("12"));
    source.setLopr(QStringLiteral("-10"));
    source.setHopr(QStringLiteral("10"));
    source.setLolo(QStringLiteral("-9"));
    source.setLow(QStringLiteral("-7"));
    source.setHigh(QStringLiteral("7"));
    source.setHihi(QStringLiteral("9"));
    if (waveform) { source.setNelm(5); source.setNord(3); }
    if (matrix) { source.setMatrixRows(2); source.setMatrixColumns(3); }
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    QVariantMap expected = {
        {"connected", true}, {"fieldtype", int(caScript::Double)},
        {"units", "V"}, {"precision", 2}, {"status", status}, {"severity", severity},
        {"lower_disp_limit", -10}, {"upper_disp_limit", 10},
        {"lower_ctrl_limit", -12}, {"upper_ctrl_limit", 12},
        {"lower_alarm_limit", -9}, {"lower_warning_limit", -7},
        {"upper_warning_limit", 7}, {"upper_alarm_limit", 9},
        {"nelm", matrix ? 6 : waveform ? 5 : 1},
        {"valueCount", matrix ? 6 : waveform ? 3 : 1}, {"enumCount", 0},
        {"dimCount", matrix ? 2 : 0}, {"ntType", matrix ? int(NT_MATRIX) : int(NT_NONE)},
        {"dim", matrix ? QVariantList{2, 3} : QVariantList{0, 0}},
        {"accessR", true}, {"accessW", true}
    };
    const QString json = QString::fromUtf8(QJsonDocument::fromVariant(expected).toJson(QJsonDocument::Compact));
    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptMetadata"), QStringLiteral("pv")}});
    script.setScript(QStringLiteral(
        "const expected = %1;\n"
        "for (const key of Object.keys(expected)) {\n"
        "    if (JSON.stringify(inputs.pv[key]) !== JSON.stringify(expected[key]))\n"
        "        throw new Error('Incorrect metadata: ' + key);\n"
        "}\n"
        "if (!Array.isArray(inputs.pv.dim)) throw new Error('Expected dimension array');\n"
        "inputs.pv.units = 'changed'; inputs.pv.dim[0] = 999;").arg(json));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);
    const knobData *input = m_mutexKnobData->getMutexKnobDataPV(&script, QStringLiteral("scriptMetadata"));
    QVERIFY(input);
    QCOMPARE(input->edata.status, short(status));
    QCOMPARE(input->edata.severity, short(severity));
    QCOMPARE(input->edata.upper_ctrl_limit, 12.0);
    QSignalSpy finished(&script, &caScript::scriptFinished);
    for (int run = 1; run <= 2; ++run) {
        script.executeScript();
        QTRY_COMPARE(finished.count(), run);
        QVERIFY(finished.last().first().toBool());
    }
    QCOMPARE(QString::fromLatin1(input->edata.units), QStringLiteral("V"));
    QCOMPARE(input->edata.dim[0], matrix ? 2 : 0);
}

void TestCaQtDM_Lib::scriptReceivesConnectionAndAccessChanges()
{
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptAccessMetadata"));
    source.setDataType(genSoftPV::Double);
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);
    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptAccessMetadata"), QStringLiteral("pv")}});
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    const knobData *input = m_mutexKnobData->getMutexKnobDataPV(&script, QStringLiteral("scriptAccessMetadata"));
    QVERIFY(input);
    QTRY_VERIFY(input->edata.connected);
    knobData snapshot = *input;
    QSignalSpy finished(&script, &caScript::scriptFinished);
    for (int step = 0; step < 3; ++step) {
        snapshot.edata.connected = step != 1;
        snapshot.edata.accessR = step != 1;
        snapshot.edata.accessW = step == 2;
        m_caQtDM_Lib->Callback_UpdateWidget(snapshot.index, &script, "", "", "", snapshot);
        script.setScript(QStringLiteral(
            "if (inputs.pv.connected !== %1 || inputs.pv.accessR !== %1 || inputs.pv.accessW !== %2)\n"
            "    throw new Error('Incorrect connection or access state');")
            .arg(step != 1 ? "true" : "false", step == 2 ? "true" : "false"));
        script.executeScript();
        QTRY_COMPARE(finished.count(), step + 1);
        QVERIFY(finished.last().first().toBool());
    }
}

void TestCaQtDM_Lib::scriptReceivesTypedScalar_data()
{
    QTest::addColumn<int>("sourceType");
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("expectedType");
    QTest::addColumn<QString>("expectedValue");
    QTest::newRow("string") << int(genSoftPV::String) << QStringLiteral("ABC")
                            << QStringLiteral("String") << QStringLiteral("'ABC'");
    QTest::newRow("short-min") << int(genSoftPV::Int) << QStringLiteral("-32768")
                               << QStringLiteral("Short") << QStringLiteral("-32768");
    QTest::newRow("short-max") << int(genSoftPV::Int) << QStringLiteral("32767")
                               << QStringLiteral("Short") << QStringLiteral("32767");
    QTest::newRow("float") << int(genSoftPV::Float) << QStringLiteral("-3.75")
                           << QStringLiteral("Float") << QStringLiteral("-3.75");
    QTest::newRow("double") << int(genSoftPV::Double) << QStringLiteral("3.75")
                            << QStringLiteral("Double") << QStringLiteral("3.75");
    QTest::newRow("long") << int(genSoftPV::Long) << QStringLiteral("2147483647")
                          << QStringLiteral("Long") << QStringLiteral("2147483647");
    for (int value : {0, 1, 2}) {
        QTest::newRow(qPrintable(QStringLiteral("enum-%1").arg(value)))
            << int(genSoftPV::Enum) << QString::number(value)
            << QStringLiteral("Enum") << QString::number(value);
    }
    for (int value : {0, 65, 255}) {
        QTest::newRow(qPrintable(QStringLiteral("char-%1").arg(value)))
            << int(genSoftPV::Char) << QString::number(value)
            << QStringLiteral("Char") << QString::number(value);
    }
}

void TestCaQtDM_Lib::scriptReceivesTypedScalar()
{
    QFETCH(int, sourceType);
    QFETCH(QString, value);
    QFETCH(QString, expectedType);
    QFETCH(QString, expectedValue);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);

    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptTypedInput"));
    source.setDataType(static_cast<genSoftPV::DataType>(sourceType));
    source.setValue(value);
    source.setEnumStrings({QStringLiteral("OFF"), QStringLiteral("ON"), QStringLiteral("FAULT")});
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({{QStringLiteral("internal://scriptTypedInput"), QStringLiteral("sample")}});
    script.setScript(QStringLiteral(
        "const channel = inputs.sample;\n"
        "if (channel.fieldtype !== EpicsType.%1) throw new Error('Incorrect source type');\n"
        "if (channel.value !== %2) throw new Error('Incorrect scalar value');\n"
        "if (channel.fieldtype === EpicsType.Enum) {\n"
        "    const labels = ['OFF', 'ON', 'FAULT'];\n"
        "    if (channel.label !== labels[channel.value]) throw new Error('Incorrect enum label');\n"
        "    if (JSON.stringify(channel.enumStrings) !== JSON.stringify(labels))\n"
        "        throw new Error('Incorrect enum strings');\n"
        "}").arg(expectedType, expectedValue));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);

    QSignalSpy finished(&script, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    script.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaQtDM_Lib::scriptReceivesMonitoredScalarThroughAlias()
{
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptScalarInput"));
    source.setDataType(genSoftPV::Double);
    source.setValue(QStringLiteral("2.5"));
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({
        {QStringLiteral("internal://scriptScalarInput"), QStringLiteral("factor")}
    });
    script.setScript(QStringLiteral(
        "if (inputs.factor.value !== 2.5) throw new Error('Incorrect input value');"));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);

    QSignalSpy finished(&script, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    script.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaQtDM_Lib::scriptReceivesMonitoredShortThroughAlias_data()
{
    QTest::addColumn<int>("value");
    QTest::newRow("zero") << 0;
    QTest::newRow("positive") << 42;
    QTest::newRow("negative") << -42;
}

void TestCaQtDM_Lib::scriptReceivesMonitoredShortThroughAlias()
{
    QFETCH(int, value);

    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptShortInput"));
    source.setDataType(genSoftPV::Int);
    source.setValue(QString::number(value));
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({
        {QStringLiteral("internal://scriptShortInput"), QStringLiteral("number")}
    });
    script.setScript(QStringLiteral(
        "if (inputs.number.value !== %1) throw new Error('Incorrect short input value');")
        .arg(value));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);

    QSignalSpy finished(&script, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    script.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaQtDM_Lib::scriptReceivesMonitoredLongThroughAlias_data()
{
    QTest::addColumn<int>("value");
    QTest::newRow("zero") << 0;
    QTest::newRow("positive") << 42;
    QTest::newRow("negative") << -42;
    QTest::newRow("minimum") << (-2147483647 - 1);
    QTest::newRow("maximum") << 2147483647;
}

void TestCaQtDM_Lib::scriptReceivesMonitoredLongThroughAlias()
{
    QFETCH(int, value);

    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptLongInput"));
    source.setDataType(genSoftPV::Long);
    source.setValue(QString::number(value));
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({
        {QStringLiteral("internal://scriptLongInput"), QStringLiteral("number")}
    });
    script.setScript(QStringLiteral(
        "if (inputs.number.value !== %1) throw new Error('Incorrect long input value');")
        .arg(value));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);

    QSignalSpy finished(&script, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    script.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaQtDM_Lib::scriptReceivesMonitoredStringThroughAlias_data()
{
    QTest::addColumn<QString>("value");
    QTest::newRow("umlaut") << QStringLiteral("Grüezi");
    QTest::newRow("empty") << QString();
}

void TestCaQtDM_Lib::scriptReceivesMonitoredStringThroughAlias()
{
    QFETCH(QString, value);
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);

    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptStringInput"));
    source.setDataType(genSoftPV::String);
    source.setValue(value);
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({
        {QStringLiteral("internal://scriptStringInput"), QStringLiteral("text")}
    });
    script.setScript(value.isEmpty()
        ? QStringLiteral("if (inputs.text.value !== '') throw new Error('Missing empty string');")
        : QStringLiteral("if (inputs.text.value !== 'Gr\\uFFFDezi') throw new Error('Incorrect string');"));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);

    QSignalSpy finished(&script, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    script.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaQtDM_Lib::scriptReceivesOriginalStringBytes()
{
    m_mutexKnobData->UpdateMechanism(MutexKnobData::UpdateTimed);
    genSoftPV source(m_parentAS);
    source.setVariable(QStringLiteral("scriptRawStringInput"));
    source.setDataType(genSoftPV::String);
    source.setValue(QStringLiteral("Grüezi"));
    m_caQtDM_Lib->HandleWidget(&source, "", true, true);

    caScript script(m_parentAS);
    script.setInputChannels({
        {QStringLiteral("internal://scriptRawStringInput"), QStringLiteral("text")}
    });
    script.setScript(QStringLiteral(
        "const bytes = inputs.text.rawBytes;\n"
        "if (!(bytes instanceof Uint8Array)) throw new Error('Missing raw bytes');\n"
        "const expected = [0x47, 0x72, 0xfc, 0x65, 0x7a, 0x69];\n"
        "if (bytes.length !== expected.length) throw new Error('Incorrect byte count');\n"
        "for (let i = 0; i < expected.length; ++i) {\n"
        "    if (bytes[i] !== expected[i]) throw new Error('Changed original byte');\n"
        "}"));
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    bool updated = false;
    connect(m_mutexKnobData, &MutexKnobData::Signal_UpdateWidget, &script,
            [&script, &updated](int, QWidget *widget, const QString &, const QString &,
                               const QString &, const knobData &) {
        if (widget == &script) updated = true;
    });
    QMetaObject::invokeMethod(m_internalPlugin, "updateChannels", Qt::DirectConnection);
    QTRY_VERIFY(updated);

    QSignalSpy finished(&script, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    script.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaQtDM_Lib::checkJsonStringWorks()
{
    QString emptyJson = "{}";
    QCOMPARE(m_caQtDM_Lib->checkJsonString(emptyJson), true);

    QString validJson
        = R"({"root":{"children":[{"id":"node-1","metadata":{"tags":["alpha","beta"],"priority":7}},{"id":"node-2","metadata":{"tags":[],"priority":0}}],"configuration":{"featureFlags":{"enableQuantumMode":false,"useLegacyFluxCapacitor":true},"thresholds":[0.1,0.5,0.9]},"auditTrail":[{"timestamp":"1970-01-01T00:00:00Z","action":"INITIALIZE"},{"timestamp":"2099-12-31T23:59:59Z","action":"TERMINATE"}],"nullableField":null,"emptyObject":{},"emptyArray":[]}})";
    QCOMPARE(m_caQtDM_Lib->checkJsonString(validJson), true);

    QString validMultilineJson = R"({
        "a": 1,
        "caqtdm_monitor": {
            "maxdisplayrate": 42
        },
        "b": 2
    })";
    QCOMPARE(m_caQtDM_Lib->checkJsonString(validMultilineJson), true);

    QString emptyString = "";
    QCOMPARE(m_caQtDM_Lib->checkJsonString(emptyString), false);

    QString invalidJson
        = R"({"root":{"children":[{"id":"node-1","metadata":{"tags":["alpha","beta"],"priority":7}},{"id":"node-2","metadata":{"tags":[],"priority":0}}],"configuration":{"featureFlags":{"enableQuantumMode":false,"useLegacyFluxCapacitor":true},"thresholds":[0.1,0.5,0.9]},"auditTrail":[{"timestamp":"1970-01-01T00:00:00Z","action"2099-12-31T23:59:59Z","action":"TERMINATE"}],"nullableField":null,"emptyObject":{},"emptyArray":[]}})";
    QCOMPARE(m_caQtDM_Lib->checkJsonString(invalidJson), false);

    QString invalidJsonMissingBracket
        = R"({"root":{"children":[{"id":"node-1","metadata":{"tags":["alpha","beta"],"priority":7}},{"id":"node-2","metadata":{"tags":[],"priority":0}}],"configuration":{"featureFlags":{"enableQuantumMode":false,"useLegacyFluxCapacitor":true},"thresholds":[0.1,0.5,0.9]},"auditTrail":[{"timestamp":"1970-01-01T00:00:00Z","action":"INITIALIZE"},{"timestamp":"2099-12-31T23:59:59Z","action":"TERMINATE"}],"nullableField":null,"emptyObject":{},"emptyArray":[]})";
    QCOMPARE(m_caQtDM_Lib->checkJsonString(invalidJsonMissingBracket), false);
}

void TestCaQtDM_Lib::parseForDisplayRateWorks()
{
    QString json = R"({"a": 1,"b": 2})";
    int rate = -1;

    int result = m_caQtDM_Lib->parseForDisplayRate(json, rate);
    QCOMPARE(result, 0);
    QCOMPARE(rate, -1);

    json = R"({
        "a": 1,
        "caqtdm_monitor": {
            "maxdisplayrate": 42
        },
        "b": 2
    })";
    rate = 0;

    result = m_caQtDM_Lib->parseForDisplayRate(json, rate);
    QCOMPARE(result, 1);
    QCOMPARE(rate, 42);

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    QCOMPARE(doc.isObject(), true);

    QJsonObject root = doc.object();
    QCOMPARE(root.contains("caqtdm_monitor"), false);
    QCOMPARE(root["a"].toInt(), 1);
    QCOMPARE(root["b"].toInt(), 2);

    json = R"({"arr":{"e":-1,"i":1,"s":1},"caqtdm_monitor":{"maxdisplayrate":1},"dbnd":{"abs":0},"sync":{"before":""}})";
    rate = 0;

    result = m_caQtDM_Lib->parseForDisplayRate(json, rate);
    qDebug() << json;
    QCOMPARE(result, 1);
    QCOMPARE(rate, 1);

    doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    QCOMPARE(doc.isObject(), true);

    root = doc.object();
    QCOMPARE(root.contains("caqtdm_monitor"), false);
    QCOMPARE(root["arr"].toObject(), QJsonObject({{"e", -1}, {"i", 1}, {"s", 1}}));
    QCOMPARE(root["dbnd"].toObject(), QJsonObject({{"abs", 0}}));
    QCOMPARE(root["sync"].toObject(), QJsonObject({{"before", ""}}));
}

void TestCaQtDM_Lib::parseForQRectConstWorks()
{
    QString json = R"({"badkey":[1,2,3]})";
    double valueArray[3] = {0.0, 0.0, 0.0};

    QCOMPARE(m_caQtDM_Lib->parseForQRectConst(json, valueArray), false);
    QCOMPARE(valueArray[0], 0.0);
    QCOMPARE(valueArray[1], 0.0);
    QCOMPARE(valueArray[2], 0.0);
    QCOMPARE(json, QString(R"({"badkey":[1,2,3]})"));

    json = R"({"valueconst":[1,2,3]})";
    valueArray[0] = 1.0;
    valueArray[1] = 2.0;
    valueArray[2] = 3.0;

    QCOMPARE(m_caQtDM_Lib->parseForQRectConst(json, valueArray), true);
    QCOMPARE(valueArray[0], 1.0);
    QCOMPARE(valueArray[1], 2.0);
    QCOMPARE(valueArray[2], 3.0);
    QCOMPARE(json, QString(R"({"valueconst":[1,2,3]})"));
}

void TestCaQtDM_Lib::treatMacroWorks()
{
    m_caQtDM_Lib->unknownMacrosList.clear();
    m_caQtDM_Lib->level = 0;
    m_caQtDM_Lib->savedFile[0] = "test.ui";

    {
        QMap<QString, QString> map;
        map["A"] = "$(B)";
        map["B"] = "$(C)";
        map["C"] = "final";

        QString text = "prefix $(A) suffix";
        bool doNothing = true;

        QCOMPARE(m_caQtDM_Lib->treatMacro(map, text, &doNothing, "widget"),
                 QString("prefix final suffix"));
        QCOMPARE(doNothing, false);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.isEmpty(), true);
    }

    m_caQtDM_Lib->unknownMacrosList.clear();

    {
        QMap<QString, QString> map;
        map["A"] = "literal";

        QString text = R"(pv $(A{"regex":"literal","value":"JSON"}))";
        bool doNothing = true;

        QCOMPARE(m_caQtDM_Lib->treatMacro(map, text, &doNothing, "widget"), QString("pv JSON"));
        QCOMPARE(doNothing, false);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.isEmpty(), true);
    }

    m_caQtDM_Lib->unknownMacrosList.clear();

    {
        QMap<QString, QString> map;
        map["A"] = "prefix-123-suffix";

        QString text = R"(pv $(A{"regex":"[0-9]+","value":"456"}))";
        bool doNothing = true;

        QCOMPARE(m_caQtDM_Lib->treatMacro(map, text, &doNothing, "widget"),
                 QString("pv prefix-456-suffix"));
        QCOMPARE(doNothing, false);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.isEmpty(), true);
    }

    m_caQtDM_Lib->unknownMacrosList.clear();

    {
        QMap<QString, QString> map;
        map["A"] = "prefix-123-suffix";
        map["B"] = "456";

        QString text = R"lim(pv $(A{"regex":"[0-9]+","value":"$(B)"}))lim";
        bool doNothing = true;

        QCOMPARE(m_caQtDM_Lib->treatMacro(map, text, &doNothing, "widget"),
                 QString("pv prefix-456-suffix"));
        QCOMPARE(doNothing, false);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.isEmpty(), true);
    }

    m_caQtDM_Lib->unknownMacrosList.clear();

    {
        QMap<QString, QString> map;
        map["A"] = "one";

        QString text = "pv $(MISSING)";
        bool doNothing = false;

        QCOMPARE(m_caQtDM_Lib->treatMacro(map, text, &doNothing, "widget42"),
                 QString("pv $(MISSING)"));
        QCOMPARE(doNothing, false);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.size(), 1);

        QString key = "(MISSING)###widget42###test.ui";
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.contains(key), true);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.value(key), QString("test.ui"));
    }

    {
        QMap<QString, QString> map;
        QString text = "pv $(A)";
        bool doNothing = false;

        QCOMPARE(m_caQtDM_Lib->treatMacro(map, text, &doNothing, "widget"), QString("pv $(A)"));
        QCOMPARE(doNothing, true);
    }

    m_caQtDM_Lib->unknownMacrosList.clear();

    {
        // Cases taken from the manual display test caQtDM_Tests/macrotest.ui,
        // which pairs each expression with its expected result for the macros
        // TEST=12345, TEST1=abcd, TESTLEER=<empty>; TEST2..TEST5 are undefined.
        QMap<QString, QString> map;
        map["TEST"] = "12345";
        map["TEST1"] = "abcd";
        map["TESTLEER"] = "";

        const struct {
            const char *input;
            const char *expected;
        } cases[] = {
            {R"lim($(TEST))lim", "12345"},
            {R"lim($(TEST)$(TEST1)$(TEST)$(TEST1)$(TEST)$(TEST1))lim",
             "12345abcd12345abcd12345abcd"},
            {R"lim(xyz$$876_$(TEST{"regex":"([1,2]+)([3])","value":"A\\1"})______)lim",
             "xyz$$876_A1245______"},
            {R"lim($(TEST{Bla})$(TEST1)$(TEST))lim", "12345abcd12345"},
            {R"lim($(TEST{"regex":"\\S+","value":"HUI2HUIHUI"}) $(TEST{Bla}) $(TEST1)$(TEST))lim",
             "HUI2HUIHUI 12345 abcd12345"},
            {R"lim($(TEST{"regex":"([1,2]+)([3])","value":"A"}))lim", "A45"},
            {R"lim($(TEST{"regex":"([1,2]+)([3])","value":"A\\2"}))lim", "A345"},
            {R"lim($(TEST5=This) $(TEST5=is) $(TEST5=a) $(TEST5=macro) $(TEST5=test))lim",
             "This is a macro test"},
            {R"lim( $(TEST=Error!!))lim", " 12345"},
            {R"lim($(TEST5{"regex":"\\S+","value":"$(TEST1)"}=RegExConst))lim", "RegExConst"},
            {R"lim($(TEST5{"regex":"\\S+","value":"$(TEST1)"}=$(TEST)))lim", "12345"},
            {R"lim($(TEST5=$(TEST4=$(TEST3=chain constant test))))lim", "chain constant test"},
            {R"lim($(TEST5=$(TEST4=$(TEST3=$(TEST1)))))lim", "abcd"},
            {R"lim($(TEST5=->$(TEST=$(TEST3=$(TEST4)))<-))lim", "->12345<-"},
            {R"lim($(TESTLEER=1$(TEST=2$(TEST3=3$(TEST4)3)2)1))lim", ""},
            {R"lim($(TESTLEER)<-LEER)lim", "<-LEER"},
            {R"lim(->$(TESTLEER=not OK)$(TEST=$(TEST3=Boing))$(TEST4=)<-)lim", "->12345<-"},
            {R"lim($(TESTLEER)-$(TEST))lim", "-12345"},
            {R"lim($(TEST)-$(TESTLEER)$(TEST)=0)lim", "12345-12345=0"},
            {R"lim(->$(TESTLEER=Error)<-)lim", "-><-"},
            {R"lim($(TEST2=1);$(TEST2=2);$(TEST2=3);$(TEST2=4);$(TEST2=5);$(TEST2=6))lim",
             "1;2;3;4;5;6"},
        };

        for (const auto &testCase : cases) {
            bool doNothing = true;
            QString result = m_caQtDM_Lib->treatMacro(map,
                                                      QString::fromUtf8(testCase.input),
                                                      &doNothing,
                                                      "widget");
            QVERIFY2(result == QString::fromUtf8(testCase.expected),
                     qPrintable(QString("input '%1' gave '%2', expected '%3'")
                                    .arg(testCase.input, result, testCase.expected)));
            QCOMPARE(doNothing, false);
        }
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.isEmpty(), true);
    }

    m_caQtDM_Lib->unknownMacrosList.clear();

    {
        // also from macrotest.ui: an undefined macro without default value
        // stays untouched and is registered as unknown
        QMap<QString, QString> map;
        map["TEST"] = "12345";
        map["TEST1"] = "abcd";
        map["TESTLEER"] = "";

        QString text = R"lim($(TEST2),not Resolved Macro=OK)lim";
        bool doNothing = true;

        QCOMPARE(m_caQtDM_Lib->treatMacro(map, text, &doNothing, "widget42"), text);
        QCOMPARE(doNothing, false);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.size(), 1);
        QCOMPARE(m_caQtDM_Lib->unknownMacrosList.contains("(TEST2)###widget42###test.ui"), true);
    }
}

void TestCaQtDM_Lib::getLongValueFromStringWorks()
{
    // the function may modify the buffer ('O' -> '0'), so writable buffers are required
    char buffer[SMALL_STRING_LENGTH];
    char *end = Q_NULLPTR;

    qstrncpy(buffer, "17", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::octal, &end), 15L);
    QCOMPARE(*end, '\0');

    // caQtDM represents octal values with an 'O' prefix
    qstrncpy(buffer, "O17", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::octal, &end), 15L);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "ff", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::hexadecimal, &end), 255L);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "0x1F", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::hexadecimal, &end), 31L);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "42", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 42L);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "-42", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), -42L);
    QCOMPARE(*end, '\0');

    // hexadecimal and octal notations are auto-detected in decimal mode
    qstrncpy(buffer, "0x1F", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 31L);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "O17", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 15L);
    QCOMPARE(*end, '\0');

    // trailing garbage: end points to the first unparsed character
    qstrncpy(buffer, "42abc", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 42L);
    QCOMPARE(*end, 'a');

    // nothing parsed at all: end stays at the beginning of the buffer
    qstrncpy(buffer, "abc", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 0L);
    QCOMPARE(end, buffer);

    // limits of long (size is platform dependent, so the inputs are generated)
    qstrncpy(buffer, QString::number(LONG_MAX).toLatin1().constData(), sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             LONG_MAX);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, QString::number(LONG_MIN).toLatin1().constData(), sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             LONG_MIN);
    QCOMPARE(*end, '\0');

    // decimal overflow saturates at the limits (strtol semantics)
    qstrncpy(buffer, "9999999999999999999999999", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             LONG_MAX);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "-9999999999999999999999999", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             LONG_MIN);
    QCOMPARE(*end, '\0');

    // all bits set in hexadecimal is ULONG_MAX (strtoul), which maps to -1 as long
    QString allF(int(2 * sizeof(long)), QChar('f'));
    qstrncpy(buffer, allF.toLatin1().constData(), sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getLongValueFromString(buffer, CaQtDM_Lib_Interface::hexadecimal, &end),
             -1L);
    QCOMPARE(*end, '\0');
}

void TestCaQtDM_Lib::getDoubleValueFromStringWorks()
{
    char buffer[SMALL_STRING_LENGTH];
    char *end = Q_NULLPTR;

    qstrncpy(buffer, "17", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::octal, &end), 15.0);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "ff", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::hexadecimal, &end), 255.0);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "3.14", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 3.14);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "-2.5e3", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), -2500.0);
    QCOMPARE(*end, '\0');

    // hexadecimal notation is auto-detected in decimal mode
    qstrncpy(buffer, "0x10", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 16.0);
    QCOMPARE(*end, '\0');

    // nothing parsed at all: end stays at the beginning of the buffer
    qstrncpy(buffer, "abc", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end), 0.0);
    QCOMPARE(end, buffer);

    // limits of double survive a round trip through their string representation
    qstrncpy(buffer, QString::number(DBL_MAX, 'g', 17).toLatin1().constData(), sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             DBL_MAX);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, QString::number(-DBL_MAX, 'g', 17).toLatin1().constData(), sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             -DBL_MAX);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, QString::number(DBL_MIN, 'g', 17).toLatin1().constData(), sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             DBL_MIN);
    QCOMPARE(*end, '\0');

    // overflow yields infinity (strtod semantics)
    qstrncpy(buffer, "1e309", sizeof(buffer));
    double result = m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal,
                                                           &end);
    QVERIFY(qIsInf(result) && result > 0.0);
    QCOMPARE(*end, '\0');

    qstrncpy(buffer, "-1e309", sizeof(buffer));
    result = m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end);
    QVERIFY(qIsInf(result) && result < 0.0);
    QCOMPARE(*end, '\0');

    // underflow yields zero
    qstrncpy(buffer, "1e-999", sizeof(buffer));
    QCOMPARE(m_caQtDM_Lib->getDoubleValueFromString(buffer, CaQtDM_Lib_Interface::decimal, &end),
             0.0);
    QCOMPARE(*end, '\0');
}

void TestCaQtDM_Lib::computeNumericMaxMinPrecUsesChannelDispLimits()
{
    checkChannelDispLimits<caApplyNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                            "T_ApplyNumeric_Channel");
    checkChannelDispLimits<caNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                       "T_Numeric_Channel");
    checkChannelDispLimits<caSpinbox>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                       "T_Spinbox_Channel");
}

void TestCaQtDM_Lib::computeNumericMaxMinPrecFallsBackWithoutLimits()
{
    checkFallbackWithoutLimits<caApplyNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                                "T_ApplyNumeric_Fallback");
    checkFallbackWithoutLimits<caNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                           "T_Numeric_Fallback");
    checkFallbackWithoutLimits<caSpinbox>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                           "T_Spinbox_Fallback");
}

void TestCaQtDM_Lib::computeNumericMaxMinPrecIgnoresChannelInUserMode()
{
    checkUserModeIgnoresChannel<caApplyNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                                 "T_ApplyNumeric_User");
    checkUserModeIgnoresChannel<caNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                            "T_Numeric_User");
    checkUserModeIgnoresChannel<caSpinbox>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                            "T_Spinbox_User");
}

void TestCaQtDM_Lib::computeNumericMaxMinPrecHonoursMaxChannelPrecision()
{
    checkMaxChannelPrecision<caApplyNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                              "T_ApplyNumeric_Prec");
    checkMaxChannelPrecision<caNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                         "T_Numeric_Prec");
    checkMaxChannelPrecision<caSpinbox>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                         "T_Spinbox_Prec");
}

void TestCaQtDM_Lib::computeNumericMaxMinPrecUpdatesWhenChannelChanges()
{
    checkWidgetPicksUpFieldWrite<caApplyNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                                  "T_ApplyNumeric_Live");
    checkWidgetPicksUpFieldWrite<caNumeric>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                             "T_Numeric_Live");
    checkWidgetPicksUpFieldWrite<caSpinbox>(m_caQtDM_Lib, m_internalPlugin, m_parentAS,
                                             "T_Spinbox_Live");
}

void TestCaQtDM_Lib::scriptMessagesReachLogAndMessageWindow()
{
    MessageWindow *window = new MessageWindow(m_fakeFileOpenWindow);
    m_caQtDM_Lib->messageWindowP = window;
    caScript script(m_parentAS);
    script.setObjectName(QStringLiteral("loggingTest"));
    script.setScript("console.log('info-marker'); console.warn('warning-marker'); console.error('error-marker'); throw new Error('exception-marker');");
    m_caQtDM_Lib->HandleWidget(&script, "", false, false);
    QSignalSpy finished(&script, &caScript::scriptFinished);
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression("caScript loggingTest: info-marker"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("caScript loggingTest: warning-marker"));
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("caScript loggingTest: error-marker"));
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("caScript loggingTest: .*exception-marker"));
    script.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!finished.first().first().toBool());
    QTRY_VERIFY(window->getMessageBoxContents().contains("exception-marker"));
    const QString text = window->getMessageBoxContents();
    QVERIFY(text.contains("info-marker"));
    QVERIFY(text.contains("warning-marker"));
    QVERIFY(text.contains("error-marker"));

    window->clearText();
    caScript invalid(m_parentAS);
    invalid.setObjectName(QStringLiteral("invalidConfiguration"));
    invalid.setInputChannels({{"first", "duplicate"}, {"second", "duplicate"}});
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("caScript invalidConfiguration: Duplicate input key"));
    m_caQtDM_Lib->HandleWidget(&invalid, "", false, false);
    QTRY_VERIFY(window->getMessageBoxContents().contains("Duplicate input key"));
}
