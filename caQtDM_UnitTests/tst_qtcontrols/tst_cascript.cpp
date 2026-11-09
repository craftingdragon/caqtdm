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

#include <QMetaProperty>
#include <QTest>

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
