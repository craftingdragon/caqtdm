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

#ifndef TST_CASCRIPT_H
#define TST_CASCRIPT_H

#include <QObject>

class TestCaScript : public QObject
{
    Q_OBJECT

private slots:
    void scriptPropertyIsInitiallyEmptyAndWritable();
    void duplicateInputAliasesAreRejected();
    void distinctInputAliasesAreAccepted();
    void inputAliasCollidingWithChannelNameIsRejected();
    void emptyInputChannelWithAliasIsRejected();
    void inputConfigurationErrorsReflectCurrentInputs();
    void inputConfigurationSurvivesJsonRoundTrip();
    void invalidInputJsonPreservesInputsAndCanBeCorrected();
    void scriptAndInputsSurviveUiFileRoundTrip();
    void executionModesRespectConnectionFreshness();
    void constantModeAllowsMissingInputs();
    void monitorTriggerPolicies_data();
    void monitorTriggerPolicies();
    void periodicTriggerRespectsMode_data();
    void periodicTriggerRespectsMode();
    void busyMonitorTriggersAreWarnedAndNotQueued();
    void timeoutsDiscardOutputsAndDisableAfterFiveConsecutiveRuns();
    void slowSuccessfulScriptOnlyWarns();
    void consoleMessagesPreserveLevelsAndSuccess();
    void consoleFloodIsBoundedAndStillTimesOut();
    void debugDisplayCanBeToggledWithoutLosingLastOutput();
    void scriptDiagnostics_data();
    void scriptDiagnostics();
    void closingBusyWidgetLeavesOtherWidgetResponsive();
    void simpleScriptCompletesAsynchronously();
    void runtimeErrorIsReportedAndNextRunSucceeds();
    void thrownStringIsReportedAsFailure();
    void scriptExecutionKeepsGuiEventLoopResponsive();
    void latin1DecodingPreservesInputSnapshot();
    void utf8DecodingPreservesInputSnapshot();
    void inputReconfigurationClearsSnapshot();
    void outputsAreResetBetweenRuns();
    void scriptBindingsSurviveModifiedGlobals();
    void failedScriptDiscardsOutputs_data();
    void failedScriptDiscardsOutputs();
    void invalidOutputJsonPreservesConfiguration();
    void explicitScalarConversions_data();
    void explicitScalarConversions();
    void conversionsPreserveInputSnapshot();
    void waveformSnapshotUsesJavaScriptArray();
    void channelDialogShowsConfigurationAndCancelPreservesIt_data();
    void channelDialogShowsConfigurationAndCancelPreservesIt();
    void channelDialogAddsAndRemovesRows_data();
    void channelDialogAddsAndRemovesRows();
    void channelDialogSavesThroughDesignerCursor_data();
    void channelDialogSavesThroughDesignerCursor();
    void channelDialogRejectsInvalidConfiguration_data();
    void channelDialogRejectsInvalidConfiguration();
};

#endif // TST_CASCRIPT_H
