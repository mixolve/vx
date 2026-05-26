#include "shell.EditorBellSection.h"
#include "shell.Constants.h"
#include "shell.EditorPresetSections.h"
#include "../modules/mxe/module.mxe.ModuleComponent.h"
#include "module.tse.Component.h"

#include <algorithm>


void VxAudioProcessorEditor::updateSectionStates()
{
    const auto activeBellCount = getActiveBellCount();
    const auto loadedModuleCount = audioProcessor.getLoadedModuleCount();
    const auto activeModule = audioProcessor.getActiveModule();
    const auto eqeTabActive = eqeModuleExpanded && activeModule == VxAudioProcessor::ActiveModule::eqe;
    const auto speTabActive = eqeModuleExpanded && activeModule == VxAudioProcessor::ActiveModule::spe;
    const auto mxeTabActive = eqeModuleExpanded && activeModule == VxAudioProcessor::ActiveModule::mxe;
    const auto tseTabActive = eqeModuleExpanded && activeModule == VxAudioProcessor::ActiveModule::tse;

    auto updateModuleMoveButton = [] (BoxTextButton* button, const bool shouldShow, const bool shouldEnable)
    {
        if (button == nullptr)
            return;

        button->setVisible(shouldShow);
        button->setEnabled(shouldEnable);
        button->setAlpha(shouldEnable ? 1.0f : 0.45f);
    };

    if (shellGlobalHeader != nullptr)
    {
        shellGlobalHeader->setVisible(true);
        shellGlobalHeader->setButtonText("GLOBAL");
        shellGlobalHeader->setToggleState(shellGlobalExpanded, juce::dontSendNotification);
    }

    if (footerTab != nullptr)
        footerTab->setVisible(true);

    if (shellGlobalMiscHeader != nullptr)
    {
        shellGlobalMiscHeader->setVisible(shellGlobalExpanded);
        shellGlobalMiscHeader->setToggleState(shellGlobalMiscExpanded, juce::dontSendNotification);
    }

    rebuildModuleTabRows();

    for (int rowIndex = 0; rowIndex < static_cast<int>(moduleTabRows.size()); ++rowIndex)
    {
        auto& row = *moduleTabRows[static_cast<size_t>(rowIndex)];
        const auto slot = audioProcessor.getLoadedModuleSlotAtPosition(rowIndex);
        const auto active = eqeModuleExpanded
            && activeModule == slot.module
            && audioProcessor.getActiveModuleInstanceIndex() == slot.instanceIndex;
        const auto shouldShow = slot.module != VxAudioProcessor::ActiveModule::none
            && (active || ! eqeModuleExpanded);

        row.tabButton->setVisible(shouldShow);
        row.tabButton->setButtonText(audioProcessor.getLoadedModuleLabelAtPosition(rowIndex));
        row.tabButton->setToggleState(active, juce::dontSendNotification);
        updateModuleMoveButton(row.moveUpButton.get(), shouldShow, rowIndex > 0);
        updateModuleMoveButton(row.moveDownButton.get(), shouldShow, rowIndex + 1 < loadedModuleCount);
    }

    if (eqeModuleFrame != nullptr)
        eqeModuleFrame->setVisible(shellGlobalExpanded || eqeTabActive || speTabActive || mxeTabActive || tseTabActive);

    if (shellGlobalSectionFrame != nullptr)
        shellGlobalSectionFrame->setVisible(shellGlobalExpanded && shellGlobalMiscExpanded);

    const auto shellGlobalsVisible = shellGlobalExpanded && shellGlobalMiscExpanded;

    if (moduleAddButton != nullptr)
        moduleAddButton->setVisible(shellGlobalsVisible);

    if (clipControl != nullptr)
        clipControl->setVisible(shellGlobalsVisible);

    if (outGainControl != nullptr)
        outGainControl->setVisible(shellGlobalsVisible);

    if (gainLControl != nullptr)
        gainLControl->setVisible(shellGlobalsVisible);

    if (gainRControl != nullptr)
        gainRControl->setVisible(shellGlobalsVisible);

    if (globalInGainLrControl != nullptr)
        globalInGainLrControl->setVisible(shellGlobalsVisible);

    if (wideControl != nullptr)
        wideControl->setVisible(shellGlobalsVisible);

    if (globalBypassButton != nullptr)
        globalBypassButton->setVisible(shellGlobalsVisible);

    if (globalBypassOutGainOnlyButton != nullptr)
        globalBypassOutGainOnlyButton->setVisible(shellGlobalsVisible);

    if (undoButton != nullptr)
        undoButton->setVisible(shellGlobalsVisible);

    if (redoButton != nullptr)
        redoButton->setVisible(shellGlobalsVisible);

    if (moduleCloseButton != nullptr)
        moduleCloseButton->setVisible(globalExpanded && eqeModuleLoaded);

    if (eqeBypassButton != nullptr)
        eqeBypassButton->setVisible(globalExpanded && eqeModuleLoaded);

    if (eqeBypassWithGainButton != nullptr)
        eqeBypassWithGainButton->setVisible(globalExpanded && eqeModuleLoaded);

    if (eqeInGainLrControl != nullptr)
        eqeInGainLrControl->setVisible(globalExpanded && eqeModuleLoaded);

    if (eqeInGainLControl != nullptr)
        eqeInGainLControl->setVisible(globalExpanded && eqeModuleLoaded);

    if (eqeInGainRControl != nullptr)
        eqeInGainRControl->setVisible(globalExpanded && eqeModuleLoaded);

    if (eqeWideControl != nullptr)
        eqeWideControl->setVisible(globalExpanded && eqeModuleLoaded);

    if (eqeOutGainControl != nullptr)
        eqeOutGainControl->setVisible(globalExpanded && eqeModuleLoaded);

    if (speModuleCloseButton != nullptr)
        speModuleCloseButton->setVisible(globalExpanded && speModuleLoaded);

    if (mxeModuleEditor != nullptr)
        mxeModuleEditor->setVisible(eqeModuleExpanded && mxeModuleLoaded);

    if (tseModuleEditor != nullptr)
        tseModuleEditor->setVisible(eqeModuleExpanded && tseModuleLoaded);

    if ((! eqeModuleLoaded && ! speModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded) || ! eqeModuleExpanded)
    {
        if (globalHeader != nullptr)
            globalHeader->setVisible(false);

        if (speMainHeader != nullptr)
            speMainHeader->setVisible(false);

        if (filtersHeader != nullptr)
            filtersHeader->setVisible(false);

        if (visualizerHeader != nullptr)
            visualizerHeader->setVisible(false);

        globalViewport.setVisible(false);
        filterViewport.setVisible(false);

        if (presetsSection != nullptr)
        {
            presetsSection->header->setVisible(false);
            presetsSection->presetCombo.setVisible(false);
            presetsSection->adButton->setVisible(false);
            presetsSection->saveButton->setVisible(false);
            presetsSection->renameButton->setVisible(false);
            presetsSection->defaultButton->setVisible(false);
            presetsSection->deleteButton->setVisible(false);
        }

        if (addFilterButton != nullptr)
            addFilterButton->setVisible(false);

        if (globalSectionFrame != nullptr)
            globalSectionFrame->setVisible(false);

        if (speMainSectionFrame != nullptr)
            speMainSectionFrame->setVisible(false);

        if (filtersSectionFrame != nullptr)
            filtersSectionFrame->setVisible(false);

        if (presetsSectionFrame != nullptr)
            presetsSectionFrame->setVisible(false);

        if (visualizerSectionFrame != nullptr)
            visualizerSectionFrame->setVisible(false);

        if (visualizerComponent != nullptr)
            visualizerComponent->setVisible(false);

        if (visualizerRangeLowControl != nullptr)
            visualizerRangeLowControl->setVisible(false);

        if (visualizerRangeHighControl != nullptr)
            visualizerRangeHighControl->setVisible(false);

        if (visualizerCursorButton != nullptr)
            visualizerCursorButton->setVisible(false);

        if (visualizerShowStereoButton != nullptr)
            visualizerShowStereoButton->setVisible(false);

        if (visualizerShowLeftButton != nullptr)
            visualizerShowLeftButton->setVisible(false);

        if (visualizerShowRightButton != nullptr)
            visualizerShowRightButton->setVisible(false);

        if (visualizerShowMidButton != nullptr)
            visualizerShowMidButton->setVisible(false);

        if (visualizerShowSideButton != nullptr)
            visualizerShowSideButton->setVisible(false);

        if (visualizerVisibilityButton != nullptr)
            visualizerVisibilityButton->setVisible(false);

        for (auto& section : bellSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(false);
            section->moveDownButton->setVisible(false);
            section->header->setVisible(false);
            section->typeControl->setVisible(false);
            section->ttssControl->setVisible(false);
            section->lrmsControl->setVisible(false);
            section->slopeControl->setVisible(false);
            section->frequencyControl->setVisible(false);
            section->bandwidthControl->setVisible(false);
            section->gainControl->setVisible(false);
            section->bypassButton->setVisible(false);
            section->deleteButton->setVisible(false);
        }

        if (speInputGainControl != nullptr) speInputGainControl->setVisible(false);
        if (speInputGainLControl != nullptr) speInputGainLControl->setVisible(false);
        if (speInputGainRControl != nullptr) speInputGainRControl->setVisible(false);
        if (speWideControl != nullptr) speWideControl->setVisible(false);
        if (speAttackControl != nullptr) speAttackControl->setVisible(false);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
        if (speKneeControl != nullptr) speKneeControl->setVisible(false);
        if (speRatioControl != nullptr) speRatioControl->setVisible(false);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
        if (speMakeupControl != nullptr) speMakeupControl->setVisible(false);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
        if (speThresholdControl != nullptr) speThresholdControl->setVisible(false);
        if (speStereoAdaptiveControl != nullptr) speStereoAdaptiveControl->setVisible(false);
        if (speStereoAdaptiveOffsetControl != nullptr) speStereoAdaptiveOffsetControl->setVisible(false);
        if (speStereoBypassButton != nullptr) speStereoBypassButton->setVisible(false);
        if (speBypassButton != nullptr) speBypassButton->setVisible(false);
        if (speBypassWithGainButton != nullptr) speBypassWithGainButton->setVisible(false);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoBypassButton != nullptr) speDualMonoBypassButton->setVisible(false);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
        if (moduleCloseButton != nullptr) moduleCloseButton->setVisible(false);
        if (eqeBypassButton != nullptr) eqeBypassButton->setVisible(false);
        if (eqeBypassWithGainButton != nullptr) eqeBypassWithGainButton->setVisible(false);
        if (eqeInGainLrControl != nullptr) eqeInGainLrControl->setVisible(false);
        if (eqeInGainLControl != nullptr) eqeInGainLControl->setVisible(false);
        if (eqeInGainRControl != nullptr) eqeInGainRControl->setVisible(false);
        if (eqeWideControl != nullptr) eqeWideControl->setVisible(false);
        if (eqeOutGainControl != nullptr) eqeOutGainControl->setVisible(false);
        if (speModuleCloseButton != nullptr) speModuleCloseButton->setVisible(false);
        if (mxeModuleEditor != nullptr) mxeModuleEditor->setVisible(false);
        if (tseModuleEditor != nullptr) tseModuleEditor->setVisible(false);
        if (speAnalyserVisibilityButton != nullptr) speAnalyserVisibilityButton->setVisible(false);
        if (speAnalyserComponent != nullptr) speAnalyserComponent->setVisible(false);

        return;
    }

    if (mxeModuleLoaded || tseModuleLoaded)
    {
        if (globalHeader != nullptr)
            globalHeader->setVisible(false);

        if (speMainHeader != nullptr)
            speMainHeader->setVisible(false);

        if (filtersHeader != nullptr)
            filtersHeader->setVisible(false);

        if (presetsSection != nullptr)
        {
            presetsSection->header->setVisible(false);
            presetsSection->presetCombo.setVisible(false);
            presetsSection->adButton->setVisible(false);
            presetsSection->saveButton->setVisible(false);
            presetsSection->renameButton->setVisible(false);
            presetsSection->defaultButton->setVisible(false);
            presetsSection->deleteButton->setVisible(false);
        }

        if (visualizerHeader != nullptr)
            visualizerHeader->setVisible(false);
        if (visualizerComponent != nullptr)
            visualizerComponent->setVisible(false);
        if (speAnalyserComponent != nullptr)
            speAnalyserComponent->setVisible(false);
        if (visualizerSectionFrame != nullptr)
            visualizerSectionFrame->setVisible(false);
        if (speAnalyserVisibilityButton != nullptr)
            speAnalyserVisibilityButton->setVisible(false);
        if (visualizerRangeLowControl != nullptr) visualizerRangeLowControl->setVisible(false);
        if (visualizerRangeHighControl != nullptr) visualizerRangeHighControl->setVisible(false);
        if (visualizerCursorButton != nullptr) visualizerCursorButton->setVisible(false);
        if (visualizerShowStereoButton != nullptr) visualizerShowStereoButton->setVisible(false);
        if (visualizerShowLeftButton != nullptr) visualizerShowLeftButton->setVisible(false);
        if (visualizerShowRightButton != nullptr) visualizerShowRightButton->setVisible(false);
        if (visualizerShowMidButton != nullptr) visualizerShowMidButton->setVisible(false);
        if (visualizerShowSideButton != nullptr) visualizerShowSideButton->setVisible(false);
        if (visualizerVisibilityButton != nullptr) visualizerVisibilityButton->setVisible(false);

        globalViewport.setVisible(false);
        filterViewport.setVisible(false);
        if (addFilterButton != nullptr) addFilterButton->setVisible(false);
        if (moduleCloseButton != nullptr) moduleCloseButton->setVisible(false);
        if (eqeBypassButton != nullptr) eqeBypassButton->setVisible(false);
        if (eqeBypassWithGainButton != nullptr) eqeBypassWithGainButton->setVisible(false);
        if (eqeInGainLrControl != nullptr) eqeInGainLrControl->setVisible(false);
        if (eqeInGainLControl != nullptr) eqeInGainLControl->setVisible(false);
        if (eqeInGainRControl != nullptr) eqeInGainRControl->setVisible(false);
        if (eqeWideControl != nullptr) eqeWideControl->setVisible(false);
        if (eqeOutGainControl != nullptr) eqeOutGainControl->setVisible(false);
        if (speModuleCloseButton != nullptr) speModuleCloseButton->setVisible(false);
        if (clearFiltersButton != nullptr) clearFiltersButton->setVisible(false);
        if (sortPlaceButton != nullptr) sortPlaceButton->setVisible(false);
        if (sortFreqButton != nullptr) sortFreqButton->setVisible(false);
        if (sortDuoButton != nullptr) sortDuoButton->setVisible(false);
        if (globalSectionFrame != nullptr) globalSectionFrame->setVisible(false);
        if (speMainSectionFrame != nullptr) speMainSectionFrame->setVisible(false);
        if (filtersSectionFrame != nullptr) filtersSectionFrame->setVisible(false);
        if (presetsSectionFrame != nullptr) presetsSectionFrame->setVisible(false);

        for (auto& section : bellSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(false);
            section->moveDownButton->setVisible(false);
            section->header->setVisible(false);
            section->typeControl->setVisible(false);
            section->ttssControl->setVisible(false);
            section->lrmsControl->setVisible(false);
            section->slopeControl->setVisible(false);
            section->frequencyControl->setVisible(false);
            section->bandwidthControl->setVisible(false);
            section->gainControl->setVisible(false);
            section->bypassButton->setVisible(false);
            section->deleteButton->setVisible(false);
        }

        if (speInputGainControl != nullptr) speInputGainControl->setVisible(false);
        if (speInputGainLControl != nullptr) speInputGainLControl->setVisible(false);
        if (speInputGainRControl != nullptr) speInputGainRControl->setVisible(false);
        if (speWideControl != nullptr) speWideControl->setVisible(false);
        if (speAttackControl != nullptr) speAttackControl->setVisible(false);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
        if (speKneeControl != nullptr) speKneeControl->setVisible(false);
        if (speRatioControl != nullptr) speRatioControl->setVisible(false);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
        if (speMakeupControl != nullptr) speMakeupControl->setVisible(false);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
        if (speThresholdControl != nullptr) speThresholdControl->setVisible(false);
        if (speStereoAdaptiveControl != nullptr) speStereoAdaptiveControl->setVisible(false);
        if (speStereoAdaptiveOffsetControl != nullptr) speStereoAdaptiveOffsetControl->setVisible(false);
        if (speStereoBypassButton != nullptr) speStereoBypassButton->setVisible(false);
        if (speBypassButton != nullptr) speBypassButton->setVisible(false);
        if (speBypassWithGainButton != nullptr) speBypassWithGainButton->setVisible(false);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoBypassButton != nullptr) speDualMonoBypassButton->setVisible(false);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
        if (tseModuleEditor != nullptr) tseModuleEditor->setVisible(eqeModuleExpanded && tseModuleLoaded);

        return;
    }

    if (speModuleLoaded)
    {
        if (globalHeader != nullptr)
        {
            globalHeader->setVisible(true);
            globalHeader->setButtonText("MISC");
            globalHeader->setToggleState(globalExpanded, juce::dontSendNotification);
        }

        if (speMainHeader != nullptr)
        {
            speMainHeader->setVisible(true);
            speMainHeader->setButtonText("MAIN");
            speMainHeader->setToggleState(speMainExpanded, juce::dontSendNotification);
        }

        if (filtersHeader != nullptr)
        {
            filtersHeader->setVisible(true);
            filtersHeader->setButtonText("STEREO");
            filtersHeader->setToggleState(filtersExpanded, juce::dontSendNotification);
        }

        if (presetsSection != nullptr)
        {
            presetsSection->header->setVisible(true);
            presetsSection->header->setButtonText("DUAL-MONO");
            presetsSection->header->setToggleState(presetsExpanded, juce::dontSendNotification);
            presetsSection->presetCombo.setVisible(false);
            presetsSection->adButton->setVisible(false);
            presetsSection->saveButton->setVisible(false);
            presetsSection->renameButton->setVisible(false);
            presetsSection->defaultButton->setVisible(false);
            presetsSection->deleteButton->setVisible(false);
        }

        if (visualizerHeader != nullptr)
        {
            visualizerHeader->setVisible(true);
            visualizerHeader->setButtonText("ANALYSER");
            visualizerHeader->setToggleState(visualizerExpanded, juce::dontSendNotification);
        }

        if (speAnalyserVisibilityButton != nullptr)
        {
            speAnalyserVisibilityButton->setVisible(visualizerExpanded);
            speAnalyserVisibilityButton->setToggleState(visualizerVisible, juce::dontSendNotification);
            speAnalyserVisibilityButton->setButtonText(visualizerVisible ? "HIDE" : "SHOW");
        }

        if (speAnalyserComponent != nullptr)
            speAnalyserComponent->setVisible(eqeModuleExpanded && visualizerVisible);

        if (visualizerRangeLowControl != nullptr) visualizerRangeLowControl->setVisible(false);
        if (visualizerRangeHighControl != nullptr) visualizerRangeHighControl->setVisible(false);
        if (visualizerCursorButton != nullptr) visualizerCursorButton->setVisible(false);
        if (visualizerShowStereoButton != nullptr) visualizerShowStereoButton->setVisible(false);
        if (visualizerShowLeftButton != nullptr) visualizerShowLeftButton->setVisible(false);
        if (visualizerShowRightButton != nullptr) visualizerShowRightButton->setVisible(false);
        if (visualizerShowMidButton != nullptr) visualizerShowMidButton->setVisible(false);
        if (visualizerShowSideButton != nullptr) visualizerShowSideButton->setVisible(false);
        if (visualizerVisibilityButton != nullptr) visualizerVisibilityButton->setVisible(false);

        globalViewport.setVisible(globalExpanded);
        filterViewport.setVisible(visualizerExpanded);
        if (addFilterButton != nullptr)
            addFilterButton->setVisible(false);
        if (clearFiltersButton != nullptr)
            clearFiltersButton->setVisible(false);
        if (sortPlaceButton != nullptr)
            sortPlaceButton->setVisible(false);
        if (sortFreqButton != nullptr)
            sortFreqButton->setVisible(false);
        if (sortDuoButton != nullptr)
            sortDuoButton->setVisible(false);
        if (moduleCloseButton != nullptr)
            moduleCloseButton->setVisible(false);
        if (speModuleCloseButton != nullptr)
            speModuleCloseButton->setVisible(globalExpanded);

        if (speInputGainControl != nullptr) speInputGainControl->setVisible(globalExpanded);
        if (speInputGainLControl != nullptr) speInputGainLControl->setVisible(globalExpanded);
        if (speInputGainRControl != nullptr) speInputGainRControl->setVisible(globalExpanded);
        if (speWideControl != nullptr) speWideControl->setVisible(globalExpanded);
        if (speAttackControl != nullptr) speAttackControl->setVisible(speMainExpanded);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(speMainExpanded);
        if (speKneeControl != nullptr) speKneeControl->setVisible(speMainExpanded);
        if (speRatioControl != nullptr) speRatioControl->setVisible(speMainExpanded);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(speMainExpanded);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(speMainExpanded);
        if (speMakeupControl != nullptr) speMakeupControl->setVisible(globalExpanded);
        if (speBypassButton != nullptr) speBypassButton->setVisible(globalExpanded);
        if (speBypassWithGainButton != nullptr) speBypassWithGainButton->setVisible(globalExpanded);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(globalExpanded);
        if (speThresholdControl != nullptr) speThresholdControl->setVisible(filtersExpanded);
        if (speStereoAdaptiveControl != nullptr) speStereoAdaptiveControl->setVisible(filtersExpanded);
        if (speStereoAdaptiveOffsetControl != nullptr) speStereoAdaptiveOffsetControl->setVisible(filtersExpanded);
        if (speStereoBypassButton != nullptr) speStereoBypassButton->setVisible(filtersExpanded);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(presetsExpanded);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(presetsExpanded);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(presetsExpanded);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(presetsExpanded);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(presetsExpanded);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(presetsExpanded);
        if (speDualMonoBypassButton != nullptr) speDualMonoBypassButton->setVisible(presetsExpanded);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(visualizerExpanded);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(visualizerExpanded);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(visualizerExpanded);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(visualizerExpanded);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(visualizerExpanded);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(visualizerExpanded);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(visualizerExpanded);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(visualizerExpanded);

        if (globalSectionFrame != nullptr)
            globalSectionFrame->setVisible(globalExpanded);
        if (speMainSectionFrame != nullptr)
            speMainSectionFrame->setVisible(speMainExpanded);
        if (filtersSectionFrame != nullptr)
            filtersSectionFrame->setVisible(filtersExpanded);
        if (presetsSectionFrame != nullptr)
            presetsSectionFrame->setVisible(presetsExpanded);
        if (visualizerSectionFrame != nullptr)
            visualizerSectionFrame->setVisible(visualizerExpanded);

        for (auto& section : bellSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(false);
            section->moveDownButton->setVisible(false);
            section->header->setVisible(false);
            section->typeControl->setVisible(false);
            section->ttssControl->setVisible(false);
            section->lrmsControl->setVisible(false);
            section->slopeControl->setVisible(false);
            section->frequencyControl->setVisible(false);
            section->bandwidthControl->setVisible(false);
            section->gainControl->setVisible(false);
            section->bypassButton->setVisible(false);
            section->deleteButton->setVisible(false);
        }

        return;
    }

    if (speInputGainControl != nullptr) speInputGainControl->setVisible(false);
    if (speInputGainLControl != nullptr) speInputGainLControl->setVisible(false);
    if (speInputGainRControl != nullptr) speInputGainRControl->setVisible(false);
    if (speWideControl != nullptr) speWideControl->setVisible(false);
    if (speAttackControl != nullptr) speAttackControl->setVisible(false);
    if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
    if (speKneeControl != nullptr) speKneeControl->setVisible(false);
    if (speRatioControl != nullptr) speRatioControl->setVisible(false);
    if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
    if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
    if (speMakeupControl != nullptr) speMakeupControl->setVisible(false);
    if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
    if (speThresholdControl != nullptr) speThresholdControl->setVisible(false);
    if (speStereoAdaptiveControl != nullptr) speStereoAdaptiveControl->setVisible(false);
    if (speStereoAdaptiveOffsetControl != nullptr) speStereoAdaptiveOffsetControl->setVisible(false);
    if (speStereoBypassButton != nullptr) speStereoBypassButton->setVisible(false);
    if (speBypassButton != nullptr) speBypassButton->setVisible(false);
    if (speBypassWithGainButton != nullptr) speBypassWithGainButton->setVisible(false);
    if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
    if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
    if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
    if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
    if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
    if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
    if (speDualMonoBypassButton != nullptr) speDualMonoBypassButton->setVisible(false);
    if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
    if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
    if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
    if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
    if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
    if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
    if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
    if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
    if (speAnalyserVisibilityButton != nullptr) speAnalyserVisibilityButton->setVisible(false);
    if (speAnalyserComponent != nullptr) speAnalyserComponent->setVisible(false);

    if (globalHeader != nullptr)
        globalHeader->setVisible(true);

    if (speMainHeader != nullptr)
        speMainHeader->setVisible(false);

    if (filtersHeader != nullptr)
        filtersHeader->setVisible(true);

    if (presetsSection != nullptr)
        presetsSection->header->setVisible(true);

    if (visualizerHeader != nullptr)
        visualizerHeader->setVisible(true);

    if (globalHeader != nullptr)
        globalHeader->setButtonText("MISC");

    if (globalHeader != nullptr)
        globalHeader->setToggleState(globalExpanded, juce::dontSendNotification);

    if (filtersHeader != nullptr)
    {
        filtersHeader->setButtonText(juce::String::formatted("FILTERS (%d)", activeBellCount));
        filtersHeader->setToggleState(filtersExpanded, juce::dontSendNotification);
    }

    if (visualizerHeader != nullptr)
    {
        visualizerHeader->setButtonText("VISUALIZER");
        visualizerHeader->setToggleState(visualizerExpanded, juce::dontSendNotification);
    }

    if (clearFiltersButton != nullptr)
    {
        clearFiltersButton->setVisible(globalExpanded);
        clearFiltersButton->setEnabled(activeBellCount > 0);
        clearFiltersButton->setAlpha(activeBellCount > 0 ? 1.0f : 0.45f);
    }

    updateUndoRedoButtons();
    globalViewport.setVisible(globalExpanded);

    if (moduleCloseButton != nullptr)
        moduleCloseButton->setVisible(globalExpanded);

    if (speModuleCloseButton != nullptr)
        speModuleCloseButton->setVisible(false);

    const auto canSortFilters = activeBellCount > 1;

    if (sortPlaceButton != nullptr)
    {
        sortPlaceButton->setVisible(globalExpanded);
        sortPlaceButton->setEnabled(canSortFilters);
        sortPlaceButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    if (sortFreqButton != nullptr)
    {
        sortFreqButton->setVisible(globalExpanded);
        sortFreqButton->setEnabled(canSortFilters);
        sortFreqButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    if (sortDuoButton != nullptr)
    {
        sortDuoButton->setVisible(globalExpanded);
        sortDuoButton->setEnabled(canSortFilters);
        sortDuoButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    filterViewport.setVisible(filtersExpanded);

    if (presetsSection != nullptr)
    {
        presetsSection->header->setButtonText("PRESETS");
        presetsSection->header->setToggleState(presetsExpanded, juce::dontSendNotification);
        presetsSection->presetCombo.setVisible(presetsExpanded);
        presetsSection->adButton->setVisible(presetsExpanded);
        presetsSection->saveButton->setVisible(presetsExpanded);
        presetsSection->renameButton->setVisible(presetsExpanded);
        presetsSection->defaultButton->setVisible(presetsExpanded);
        presetsSection->deleteButton->setVisible(presetsExpanded);
    }

    if (visualizerRangeLowControl != nullptr)
        visualizerRangeLowControl->setVisible(visualizerExpanded);

    if (visualizerRangeHighControl != nullptr)
        visualizerRangeHighControl->setVisible(visualizerExpanded);

    if (visualizerCursorButton != nullptr)
        visualizerCursorButton->setVisible(visualizerExpanded);

    if (visualizerShowStereoButton != nullptr)
        visualizerShowStereoButton->setVisible(visualizerExpanded);

    if (visualizerShowLeftButton != nullptr)
        visualizerShowLeftButton->setVisible(visualizerExpanded);

    if (visualizerShowRightButton != nullptr)
        visualizerShowRightButton->setVisible(visualizerExpanded);

    if (visualizerShowMidButton != nullptr)
        visualizerShowMidButton->setVisible(visualizerExpanded);

    if (visualizerShowSideButton != nullptr)
        visualizerShowSideButton->setVisible(visualizerExpanded);

    if (visualizerVisibilityButton != nullptr)
    {
        visualizerVisibilityButton->setVisible(visualizerExpanded);
        visualizerVisibilityButton->setToggleState(visualizerVisible, juce::dontSendNotification);
        visualizerVisibilityButton->setButtonText(visualizerVisible ? "HIDE" : "SHOW");
    }

    if (visualizerCursorButton != nullptr)
        visualizerCursorButton->setButtonText(visualizerCursorEnabled ? "CURSOR-OFF" : "CURSOR-ON");

    if (globalSectionFrame != nullptr)
        globalSectionFrame->setVisible(globalExpanded);

    if (speMainSectionFrame != nullptr)
        speMainSectionFrame->setVisible(false);

    if (filtersSectionFrame != nullptr)
        filtersSectionFrame->setVisible(filtersExpanded);

    if (presetsSectionFrame != nullptr)
        presetsSectionFrame->setVisible(presetsExpanded);

    if (visualizerSectionFrame != nullptr)
        visualizerSectionFrame->setVisible(visualizerExpanded);

    if (visualizerComponent != nullptr)
        visualizerComponent->setVisible(eqeModuleExpanded && visualizerVisible);

    for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

        if (section == nullptr)
            continue;

        const auto orderPosition = getBellOrderPositionForIndex(bellIndex);
        const auto isActive = filtersExpanded && orderPosition >= 0;
        const auto isExpanded = isActive && ! globalExpanded && ! presetsExpanded && expandedBellIndex == bellIndex;
        const auto isBell = section->getFilterType() == EqeModuleProcessor::FilterType::bell;
        const auto bandwidthInactive = section->isBandwidthInactiveAtCurrentSlope();
        const auto slopeInactive = section->isSlopeInactive();
        const auto gainInactive = section->isGainInactive();
        const auto bellOrderOff = section->getFilterType() == EqeModuleProcessor::FilterType::bell
            && section->slopeControl->getSelectedChoiceIndex() == 0;
        const auto canMoveUp = isActive && orderPosition > 0;
        const auto canMoveDown = isActive && orderPosition + 1 < activeBellCount;

        section->moveUpButton->setVisible(isActive);
        section->moveUpButton->setEnabled(canMoveUp);
        section->moveUpButton->setAlpha(canMoveUp ? 1.0f : 0.45f);
        section->moveDownButton->setVisible(isActive);
        section->moveDownButton->setEnabled(canMoveDown);
        section->moveDownButton->setAlpha(canMoveDown ? 1.0f : 0.45f);
        if (auto* eqeProcessor = audioProcessor.getActiveEqeModuleProcessor())
            section->header->setButtonText(eqeProcessor->getBellHeaderText(bellIndex, orderPosition));
        else
            section->header->setButtonText({});
        section->header->setVisible(isActive);
        section->header->setToggleState(isExpanded, juce::dontSendNotification);
        section->typeControl->setVisible(isExpanded);
        section->ttssControl->setVisible(isExpanded);
        const auto ttssEnabled = audioProcessor.hasTtssSourceForActiveEqeModule();
        section->ttssControl->setInteractionEnabled(ttssEnabled);
        if (ttssEnabled)
            section->ttssControl->clearOverrideText();
        else
            section->ttssControl->setOverrideText("OFF");
        section->lrmsControl->setVisible(isExpanded);
        section->slopeControl->setVisible(isExpanded);
        section->slopeControl->setInteractionEnabled(! slopeInactive);
        if (slopeInactive || bellOrderOff)
            section->slopeControl->setOverrideText("OFF");
        else
            section->slopeControl->clearOverrideText();
        section->frequencyControl->setVisible(isExpanded);
        section->bandwidthControl->setVisible(isExpanded);
        section->bandwidthControl->setInteractionEnabled(isBell && ! bandwidthInactive);
        if (isBell && ! bandwidthInactive)
            section->bandwidthControl->clearOverrideText();
        else
            section->bandwidthControl->setOverrideText("OFF");
        section->gainControl->setVisible(isExpanded);
        section->gainControl->setInteractionEnabled(! gainInactive);
        if (gainInactive)
            section->gainControl->setOverrideText("OFF");
        else
            section->gainControl->clearOverrideText();
        section->bypassButton->setVisible(isExpanded);
        section->deleteButton->setVisible(isExpanded);
        section->deleteButton->setEnabled(activeBellCount > 0);
        section->deleteButton->setAlpha(activeBellCount > 0 ? 1.0f : 0.45f);
    }

    if (addFilterButton != nullptr)
    {
        const auto canAddFilter = activeBellCount < VxAudioProcessor::maxBellFilterCount;
        addFilterButton->setVisible(filtersExpanded);
        addFilterButton->setEnabled(canAddFilter);
        addFilterButton->setAlpha(canAddFilter ? 1.0f : 0.45f);
    }
}
