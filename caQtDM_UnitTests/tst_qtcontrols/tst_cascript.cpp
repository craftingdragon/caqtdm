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

#include "tst_cascript.h"
#include "cascript.h"

#include <QJsonDocument>
#include <QFile>
#include <QElapsedTimer>
#include <QFormBuilder>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

namespace {
class ScriptFormBuilder : public QFormBuilder
{
protected:
    QWidget *createWidget(const QString &widgetName, QWidget *parent, const QString &name) override
    {
        if (widgetName == QStringLiteral("caScript")) {
            caScript *widget = new caScript(parent);
            widget->setObjectName(name);
            return widget;
        }
        return QFormBuilder::createWidget(widgetName, parent, name);
    }
};
}

void TestCaScript::scriptPropertyIsInitiallyEmptyAndWritable()
{
    caScript widget;
    const int index = widget.metaObject()->indexOfProperty("script");
    QVERIFY(index >= 0);

    const QMetaProperty property = widget.metaObject()->property(index);
    QCOMPARE(property.userType(), int(QMetaType::QString));
    QVERIFY(property.isReadable());
    QVERIFY(property.isWritable());
    QCOMPARE(property.read(&widget).toString(), QString());

    const QString script = QStringLiteral("// Grüezi\noutputs.result.value = 42;\n");
    QVERIFY(property.write(&widget, script));
    QCOMPARE(property.read(&widget).toString(), script);
}

void TestCaScript::duplicateInputAliasesAreRejected()
{
    const QStringList errors = caScript::validateInputChannels({
        {QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"), QStringLiteral("score")},
        {QStringLiteral("internal://scriptFactor"), QStringLiteral("score")}
    });

    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.first().contains(QStringLiteral("score")));
}

void TestCaScript::distinctInputAliasesAreAccepted()
{
    const QStringList errors = caScript::validateInputChannels({
        {QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"), QStringLiteral("score")},
        {QStringLiteral("internal://scriptFactor"), QStringLiteral("factor")}
    });

    QVERIFY(errors.isEmpty());
}

void TestCaScript::inputAliasCollidingWithChannelNameIsRejected()
{
    const QStringList errors = caScript::validateInputChannels({
        {QStringLiteral("internal://scriptFactor"), QString()},
        {QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"), QStringLiteral("internal://scriptFactor")}
    });

    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.first().contains(QStringLiteral("internal://scriptFactor")));
}

void TestCaScript::emptyInputChannelWithAliasIsRejected()
{
    const QStringList errors = caScript::validateInputChannels({
        {QString(), QStringLiteral("score")}
    });

    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.first().contains(QStringLiteral("score")));
}

void TestCaScript::inputConfigurationErrorsReflectCurrentInputs()
{
    caScript widget;
    widget.setInputChannels({
        {QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"), QStringLiteral("score")},
        {QStringLiteral("internal://scriptFactor"), QStringLiteral("score")}
    });

    const QStringList errors = widget.inputConfigurationErrors();
    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.first().contains(QStringLiteral("score")));

    widget.setInputChannels({
        {QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"), QStringLiteral("score")},
        {QStringLiteral("internal://scriptFactor"), QStringLiteral("factor")}
    });

    QVERIFY(widget.inputConfigurationErrors().isEmpty());
}

void TestCaScript::invalidInputJsonPreservesInputsAndCanBeCorrected()
{
    caScript widget;
    widget.setInputChannels({
        {QStringLiteral("internal://scriptFactor"), QStringLiteral("factor")}
    });
    const QString previousJson = widget.getChannels();

    widget.setChannels(QStringLiteral("[{"));
    QVERIFY(!widget.inputConfigurationErrors().isEmpty());
    QCOMPARE(widget.getChannels(), previousJson);
    const QList<caScript::InputChannel> previousInputs = widget.getInputChannels();
    QCOMPARE(previousInputs.size(), 1);
    QCOMPARE(previousInputs.first().channel, QStringLiteral("internal://scriptFactor"));
    QCOMPARE(previousInputs.first().alias, QStringLiteral("factor"));

    widget.setChannels(QStringLiteral(
        "[{\"channel\":\"epics3://PACMAN:PACMAN_USER_SCORE\",\"alias\":\"score\"}]"));
    QVERIFY(widget.inputConfigurationErrors().isEmpty());
    const QList<caScript::InputChannel> correctedInputs = widget.getInputChannels();
    QCOMPARE(correctedInputs.size(), 1);
    QCOMPARE(correctedInputs.first().channel, QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"));
    QCOMPARE(correctedInputs.first().alias, QStringLiteral("score"));
}

void TestCaScript::simpleScriptCompletesAsynchronously()
{
    caScript widget;
    widget.setScript(QStringLiteral(
        "const answer = 6 * 7;\n"
        "if (answer !== 42) throw new Error('Unexpected result');\n"));
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    QVERIFY(finished.isValid());

    widget.executeScript();

    QCOMPARE(finished.count(), 0);
    QTRY_COMPARE(finished.count(), 1);
    const QList<QVariant> arguments = finished.first();
    QCOMPARE(arguments.size(), 1);
    QCOMPARE(arguments.first().userType(), int(QMetaType::Bool));
    QVERIFY(arguments.first().toBool());
}

void TestCaScript::runtimeErrorIsReportedAndNextRunSucceeds()
{
    caScript widget;
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    widget.setScript(QStringLiteral("throw new Error('Test failure');"));

    widget.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    const QList<QVariant> failed = finished.takeFirst();
    QCOMPARE(failed.size(), 1);
    QCOMPARE(failed.first().userType(), int(QMetaType::Bool));
    QVERIFY(!failed.first().toBool());

    widget.setScript(QStringLiteral("const answer = 6 * 7;"));
    widget.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    const QList<QVariant> succeeded = finished.first();
    QCOMPARE(succeeded.size(), 1);
    QCOMPARE(succeeded.first().userType(), int(QMetaType::Bool));
    QVERIFY(succeeded.first().toBool());
}

void TestCaScript::thrownStringIsReportedAsFailure()
{
    caScript widget;
    widget.setScript(QStringLiteral("throw 'Fehler';"));
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    QVERIFY(finished.isValid());

    widget.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    const QList<QVariant> arguments = finished.first();
    QCOMPARE(arguments.size(), 1);
    QCOMPARE(arguments.first().userType(), int(QMetaType::Bool));
    QVERIFY(!arguments.first().toBool());
}

void TestCaScript::scriptExecutionKeepsGuiEventLoopResponsive()
{
    caScript widget;
    widget.setScript(QStringLiteral(
        "const end = Date.now() + 400;\n"
        "while (Date.now() < end) {}\n"));
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    QVERIFY(finished.isValid());

    bool timerFired = false;
    bool timerFiredBeforeCompletion = false;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &widget, [&]() {
        timerFired = true;
        timerFiredBeforeCompletion = finished.isEmpty();
    });

    QElapsedTimer elapsed;
    elapsed.start();
    timer.start(20);
    widget.executeScript();
    QVERIFY2(elapsed.elapsed() < 300, "Starting the script blocked the GUI thread.");

    QTRY_VERIFY(timerFired);
    QVERIFY(timerFiredBeforeCompletion);
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaScript::scriptAndInputsSurviveUiFileRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath(QStringLiteral("cascript.ui")));

    QWidget panel;
    panel.setObjectName(QStringLiteral("panel"));
    QVBoxLayout *layout = new QVBoxLayout(&panel);
    caScript *scriptWidget = new caScript(&panel);
    scriptWidget->setObjectName(QStringLiteral("scriptWidget"));
    layout->addWidget(scriptWidget);
    const QString script = QStringLiteral("// Grüezi <>&\noutputs.result.value = inputs.score.value;\n");
    scriptWidget->setScript(script);
    const QList<caScript::InputChannel> inputs = {
        {QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"), QStringLiteral("score")},
        {QStringLiteral("internal://scriptFactor"), QString()}
    };
    scriptWidget->setInputChannels(inputs);
    scriptWidget->setOutputChannels({{QStringLiteral("internal://scriptResult"), QStringLiteral("result")}});

    ScriptFormBuilder builder;
    builder.clearPluginPaths();
    QVERIFY(file.open(QIODevice::WriteOnly));
    builder.save(&file, &panel);
    QVERIFY(file.flush());
    QVERIFY(file.size() > 0);
    file.close();

    QVERIFY(file.open(QIODevice::ReadOnly));
    std::unique_ptr<QWidget> restoredPanel(builder.load(&file));
    QVERIFY(restoredPanel != nullptr);
    caScript *restored = restoredPanel->findChild<caScript *>(QStringLiteral("scriptWidget"));
    QVERIFY(restored != nullptr);
    QCOMPARE(restored->getScript(), script);
    QCOMPARE(restored->getOutputChannelsJson(), scriptWidget->getOutputChannelsJson());
    const QList<caScript::InputChannel> restoredInputs = restored->getInputChannels();
    QCOMPARE(restoredInputs.size(), inputs.size());
    for (int i = 0; i < inputs.size(); ++i) {
        QCOMPARE(restoredInputs.at(i).channel, inputs.at(i).channel);
        QCOMPARE(restoredInputs.at(i).alias, inputs.at(i).alias);
    }
    QVERIFY(restored->inputConfigurationErrors().isEmpty());
}

void TestCaScript::latin1DecodingPreservesInputSnapshot()
{
    caScript widget;
    widget.setInputChannels({
        {QStringLiteral("internal://text"), QStringLiteral("text")}
    });
    widget.updateInputValue(0, QStringLiteral("Gr\uFFFDezi"));
    widget.updateInputRawBytes(0, QByteArray::fromHex("4772fc657a69"));
    widget.setScript(QStringLiteral(
        "const text = inputs.text;\n"
        "const before = Array.from(text.rawBytes);\n"
        "if (text.fromLatin1() !== 'Grüezi') throw new Error('Incorrect Latin-1 decoding');\n"
        "if (text.value !== 'Gr\\uFFFDezi') throw new Error('Changed standard value');\n"
        "if (text.rawBytes.length !== before.length) throw new Error('Changed byte count');\n"
        "for (let i = 0; i < before.length; ++i) {\n"
        "    if (text.rawBytes[i] !== before[i]) throw new Error('Changed raw byte');\n"
        "}"));

    QSignalSpy finished(&widget, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    widget.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaScript::utf8DecodingPreservesInputSnapshot()
{
    caScript widget;
    widget.setInputChannels({
        {QStringLiteral("internal://text"), QStringLiteral("text")}
    });
    widget.updateInputValue(0, QStringLiteral("Grüezi"));
    widget.updateInputRawBytes(0, QByteArray::fromHex("4772c3bc657a69"));
    widget.setScript(QStringLiteral(
        "const text = inputs.text;\n"
        "const before = Array.from(text.rawBytes);\n"
        "if (text.fromUtf8() !== 'Grüezi') throw new Error('Incorrect UTF-8 decoding');\n"
        "if (text.value !== 'Grüezi') throw new Error('Changed standard value');\n"
        "if (text.rawBytes.length !== before.length) throw new Error('Changed byte count');\n"
        "for (let i = 0; i < before.length; ++i) {\n"
        "    if (text.rawBytes[i] !== before[i]) throw new Error('Changed raw byte');\n"
        "}"));

    QSignalSpy finished(&widget, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    widget.executeScript();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaScript::inputReconfigurationClearsSnapshot()
{
    caScript widget;
    widget.setInputChannels({{QStringLiteral("internal://old"), QStringLiteral("sample")}});
    widget.updateInputValue(0, 1);
    widget.updateInputRawBytes(0, QByteArray("old"));
    widget.updateInputMetadata(0, {
        {QStringLiteral("fieldtype"), int(caScript::Enum)},
        {QStringLiteral("label"), QStringLiteral("ON")},
        {QStringLiteral("enumStrings"), QStringList({QStringLiteral("OFF"), QStringLiteral("ON")})}
    });
    widget.setInputChannels({{QStringLiteral("internal://new"), QStringLiteral("sample")}});
    widget.setScript(QStringLiteral(
        "if (Object.keys(inputs.sample).length !== 0) throw new Error('Stale input snapshot');"));

    QSignalSpy finished(&widget, &caScript::scriptFinished);
    QVERIFY(finished.isValid());
    widget.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaScript::outputsAreResetBetweenRuns()
{
    caScript widget;
    widget.setOutputChannels({{QStringLiteral("internal://result"), QStringLiteral("result")}});
    widget.setScript(QStringLiteral("outputs.result.value = 42;"));
    QSignalSpy values(&widget, &caScript::outputValuesReady);
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    widget.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.first().first().toMap().value(QStringLiteral("result")).toInt(), 42);

    widget.setScript(QStringLiteral("if ('value' in outputs.result) throw new Error('Stale output');"));
    widget.executeScript();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(finished.last().first().toBool());
    QCOMPARE(values.count(), 1);
}

void TestCaScript::failedScriptDiscardsOutputs_data()
{
    QTest::addColumn<QString>("script");
    QTest::newRow("error") << QStringLiteral("outputs.result.value = 42; throw new Error('Failed');");
    QTest::newRow("primitive") << QStringLiteral("outputs.result.value = 42; throw 'Failed';");
    QTest::newRow("syntax") << QStringLiteral("outputs.result.value = 42; const = ;");
    QTest::newRow("getter") << QStringLiteral(
        "Object.defineProperty(outputs.result, 'value', {get() { throw new Error('Failed'); }});");
}

void TestCaScript::failedScriptDiscardsOutputs()
{
    QFETCH(QString, script);
    caScript widget;
    widget.setOutputChannels({{QStringLiteral("internal://result"), QStringLiteral("result")}});
    widget.setScript(script);
    QSignalSpy values(&widget, &caScript::outputValuesReady);
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    widget.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!finished.first().first().toBool());
    QCOMPARE(values.count(), 0);
}

void TestCaScript::invalidOutputJsonPreservesConfiguration()
{
    caScript widget;
    widget.setOutputChannels({{QStringLiteral("internal://result"), QStringLiteral("result")}});
    const QString original = widget.getOutputChannelsJson();
    widget.setOutputChannelsJson(QStringLiteral("[{\"channel\": 42}]"));
    QCOMPARE(widget.getOutputChannelsJson(), original);
    QVERIFY(!widget.outputConfigurationErrors().isEmpty());
    widget.setOutputChannelsJson(original);
    QVERIFY(widget.outputConfigurationErrors().isEmpty());
    widget.setOutputChannels({{QStringLiteral("internal://one"), QStringLiteral("same")},
                              {QStringLiteral("internal://two"), QStringLiteral("same")}});
    QVERIFY(!widget.outputConfigurationErrors().isEmpty());
}

void TestCaScript::explicitScalarConversions_data()
{
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("target");
    QTest::addColumn<QVariant>("expected");
    QTest::newRow("positive-half") << QVariant(3.5) << QStringLiteral("Short") << QVariant(4.0);
    QTest::newRow("negative-half") << QVariant(-3.5) << QStringLiteral("Long") << QVariant(-4.0);
    QTest::newRow("zero") << QVariant(0) << QStringLiteral("Short") << QVariant(0.0);
    QTest::newRow("short-min") << QVariant(-32768) << QStringLiteral("Short") << QVariant(-32768.0);
    QTest::newRow("short-max") << QVariant(32767) << QStringLiteral("Short") << QVariant(32767.0);
    QTest::newRow("long-min") << QVariant(-2147483648.0) << QStringLiteral("Long") << QVariant(-2147483648.0);
    QTest::newRow("long-max") << QVariant(2147483647.0) << QStringLiteral("Long") << QVariant(2147483647.0);
    QTest::newRow("char") << QVariant(65) << QStringLiteral("Char") << QVariant(65.0);
    QTest::newRow("enum") << QVariant(2) << QStringLiteral("Enum") << QVariant(2.0);
    QTest::newRow("float") << QVariant(0.1) << QStringLiteral("Float") << QVariant(double(float(0.1)));
    QTest::newRow("double") << QVariant(QStringLiteral("3.75")) << QStringLiteral("Double") << QVariant(3.75);
    QTest::newRow("numeric-string") << QVariant(QStringLiteral("-3.5")) << QStringLiteral("Short") << QVariant(-4.0);
    QTest::newRow("number-as-string") << QVariant(3.75) << QStringLiteral("String") << QVariant(QStringLiteral("3.75"));
    QTest::newRow("string") << QVariant(QStringLiteral("Grüezi")) << QStringLiteral("String") << QVariant(QStringLiteral("Grüezi"));
    QTest::newRow("empty-string") << QVariant(QString()) << QStringLiteral("String") << QVariant(QString());
    QTest::newRow("short-overflow") << QVariant(32767.5) << QStringLiteral("Short") << QVariant();
    QTest::newRow("long-overflow") << QVariant(2147483648.0) << QStringLiteral("Long") << QVariant();
    QTest::newRow("char-negative") << QVariant(-1) << QStringLiteral("Char") << QVariant();
    QTest::newRow("char-overflow") << QVariant(256) << QStringLiteral("Char") << QVariant();
    QTest::newRow("enum-overflow") << QVariant(65536) << QStringLiteral("Enum") << QVariant();
    QTest::newRow("float-overflow") << QVariant(1e100) << QStringLiteral("Float") << QVariant();
    QTest::newRow("invalid-text") << QVariant(QStringLiteral("not a number")) << QStringLiteral("Double") << QVariant();
    QTest::newRow("nan-integer") << QVariant(qQNaN()) << QStringLiteral("Long") << QVariant();
    QTest::newRow("infinity-integer") << QVariant(qInf()) << QStringLiteral("Long") << QVariant();
    QTest::newRow("unknown-type") << QVariant(42) << QStringLiteral("Missing") << QVariant();
}

void TestCaScript::explicitScalarConversions()
{
    QFETCH(QVariant, value);
    QFETCH(QString, target);
    QFETCH(QVariant, expected);
    caScript widget;
    widget.setInputChannels({{QStringLiteral("internal://sample"), QStringLiteral("sample")}});
    widget.setOutputChannels({{QStringLiteral("internal://result"), QStringLiteral("result")}});
    widget.updateInputValue(0, value);
    widget.setScript(QStringLiteral(
        "outputs.result.value = 99;\n"
        "outputs.result.value = inputs.sample.convert(EpicsType.%1);").arg(target));
    QSignalSpy values(&widget, &caScript::outputValuesReady);
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    widget.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), expected.isValid());
    QCOMPARE(values.count(), expected.isValid() ? 1 : 0);
    if (!expected.isValid()) return;
    const QVariant actual = values.first().first().toMap().value(QStringLiteral("result"));
    QCOMPARE(actual.userType() == QMetaType::QString, expected.userType() == QMetaType::QString);
    if (expected.userType() == QMetaType::QString) QCOMPARE(actual.toString(), expected.toString());
    else QCOMPARE(actual.toDouble(), expected.toDouble());
}

void TestCaScript::conversionsPreserveInputSnapshot()
{
    caScript widget;
    widget.setInputChannels({{QStringLiteral("internal://text"), QStringLiteral("text")}});
    widget.updateInputValue(0, QStringLiteral("3.5"));
    widget.updateInputRawBytes(0, QByteArray("3.5"));
    widget.setScript(QStringLiteral(
        "const channel = inputs.text;\n"
        "if (channel.convert(EpicsType.Short) !== 4) throw new Error('Incorrect integer');\n"
        "if (channel.convert(EpicsType.Double) !== 3.5) throw new Error('Incorrect number');\n"
        "if (channel.value !== '3.5') throw new Error('Changed input');\n"
        "if (channel.rawBytes.length !== 3 || channel.rawBytes[0] !== 51\n"
        "    || channel.rawBytes[1] !== 46 || channel.rawBytes[2] !== 53)\n"
        "    throw new Error('Changed bytes');"));
    QSignalSpy finished(&widget, &caScript::scriptFinished);
    widget.executeScript();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
}

void TestCaScript::inputConfigurationSurvivesJsonRoundTrip()
{
    const QList<caScript::InputChannel> inputs = {
        {QStringLiteral("epics3://PACMAN:PACMAN_USER_SCORE"), QStringLiteral("score")},
        {QStringLiteral("internal://scriptFactor"), QString()}
    };
    caScript widget;
    widget.setInputChannels(inputs);

    const int index = widget.metaObject()->indexOfProperty("channels");
    QVERIFY(index >= 0);
    QVERIFY(widget.metaObject()->property(index).isStored());

    const QString json = widget.property("channels").toString();
    const QJsonDocument expected = QJsonDocument::fromJson(R"([
        {"channel": "epics3://PACMAN:PACMAN_USER_SCORE", "alias": "score"},
        {"channel": "internal://scriptFactor", "alias": ""}
    ])");
    QVERIFY(QJsonDocument::fromJson(json.toUtf8()) == expected);

    caScript restored;
    QVERIFY(restored.setProperty("channels", json));
    const QList<caScript::InputChannel> restoredInputs = restored.getInputChannels();
    QCOMPARE(restoredInputs.size(), inputs.size());
    for (int i = 0; i < inputs.size(); ++i) {
        QCOMPARE(restoredInputs.at(i).channel, inputs.at(i).channel);
        QCOMPARE(restoredInputs.at(i).alias, inputs.at(i).alias);
    }
    QVERIFY(restored.inputConfigurationErrors().isEmpty());
}
