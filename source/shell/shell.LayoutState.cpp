#include "shell.EditorBellSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

#include <algorithm>


void VxAudioProcessorEditor::updateSectionStates()
{
    constexpr auto showShellGlobalStrip = true;

    const auto activeBellCount = getActiveBellCount();
    const auto activeModule = audioProcessor.getActiveModule();

    if (shellGlobalHeader != nullptr)
    {
        shellGlobalHeader->setVisible(showShellGlobalStrip);
        shellGlobalHeader->setButtonText("CLIP");
        shellGlobalHeader->setToggleState(false, juce::dontSendNotification);
    }

    if (footerTab != nullptr)
        footerTab->setVisible(true);

    if (shellGlobalHostHeader != nullptr)
    {
        shellGlobalHostHeader->setVisible(showShellGlobalStrip);
        shellGlobalHostHeader->setToggleState(shellGlobalHostExpanded, juce::dontSendNotification);
    }

    rebuildModuleTabRows();

    for (int rowIndex = 0; rowIndex < static_cast<int>(moduleTabRows.size()); ++rowIndex)
    {
        auto& row = *moduleTabRows[static_cast<size_t>(rowIndex)];
        const auto slot = audioProcessor.getLoadedModuleSlotAtPosition(rowIndex);
        const auto active = activeModule == slot.module
            && audioProcessor.getActiveModuleInstanceIndex() == slot.instanceIndex;
        const auto shouldShow = slot.module != VxAudioProcessor::ActiveModule::none;

        row.tabButton->setVisible(shouldShow);
        row.tabButton->setButtonText(audioProcessor.getLoadedModuleLabelAtPosition(rowIndex));
        row.tabButton->setToggleState(active, juce::dontSendNotification);
    }

    const auto shellHostVisible = shellGlobalHostExpanded;

    shellGlobalHostViewport.setVisible(shellHostVisible);

    if (moduleAddButton != nullptr)
    {
        const auto noModuleLoaded = audioProcessor.getLoadedModuleCount() == 0;
        moduleAddButton->setVisible(noModuleLoaded && ! shellGlobalHostExpanded);
        moduleAddButton->setEnabled(noModuleLoaded);
    }

    if (globalBypassButton != nullptr)
        globalBypassButton->setVisible(showShellGlobalStrip);

    if (undoButton != nullptr)
        undoButton->setVisible(showShellGlobalStrip);

    if (redoButton != nullptr)
        redoButton->setVisible(showShellGlobalStrip);

    for (auto& hostSlotButton : hostSlotButtons)
        if (hostSlotButton != nullptr)
            hostSlotButton->setVisible(shellHostVisible);

    if (mieModuleEditor != nullptr)
        mieModuleEditor->setVisible(mieModuleLoaded);

    if (mxeModuleEditor != nullptr)
        mxeModuleEditor->setVisible(mxeModuleLoaded);

    if (tseModuleEditor != nullptr)
        tseModuleEditor->setVisible(tseModuleLoaded);

    if (! eqeModuleLoaded && ! speModuleLoaded && ! mieModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
    {

        if (visualizerHeader != nullptr)
            visualizerHeader->setVisible(false);

        filterViewport.setVisible(false);

        if (presetsSection != nullptr)
        {
            presetsSection->presetCombo.setVisible(false);
            presetsSection->adButton->setVisible(false);
            presetsSection->saveButton->setVisible(false);
            presetsSection->renameButton->setVisible(false);
            presetsSection->defaultButton->setVisible(false);
            presetsSection->deleteButton->setVisible(false);
        }

        if (addFilterButton != nullptr)
            addFilterButton->setVisible(false);

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

        for (auto& section : bellSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(false);
            section->moveDownButton->setVisible(false);
            section->header->setVisible(false);
            section->typeControl->setVisible(false);
            section->lrmsControl->setVisible(false);
            section->slopeControl->setVisible(false);
            section->frequencyControl->setVisible(false);
            section->bandwidthControl->setVisible(false);
            section->gainControl->setVisible(false);
            section->bypassButton->setVisible(false);
            section->deleteButton->setVisible(false);
        }

        if (speAttackControl != nullptr) speAttackControl->setVisible(false);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
        if (speKneeControl != nullptr) speKneeControl->setVisible(false);
        if (speRatioControl != nullptr) speRatioControl->setVisible(false);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoLinkButton != nullptr) speDualMonoLinkButton->setVisible(false);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
        if (mieModuleEditor != nullptr) mieModuleEditor->setVisible(false);
        if (mxeModuleEditor != nullptr) mxeModuleEditor->setVisible(false);
        if (tseModuleEditor != nullptr) tseModuleEditor->setVisible(false);
        if (speAnalyserComponent != nullptr) speAnalyserComponent->setVisible(false);

        return;
    }

    if (mieModuleLoaded || mxeModuleLoaded || tseModuleLoaded)
    {

        if (presetsSection != nullptr)
        {
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
        if (visualizerRangeLowControl != nullptr) visualizerRangeLowControl->setVisible(false);
        if (visualizerRangeHighControl != nullptr) visualizerRangeHighControl->setVisible(false);
        if (visualizerCursorButton != nullptr) visualizerCursorButton->setVisible(false);
        if (visualizerShowStereoButton != nullptr) visualizerShowStereoButton->setVisible(false);
        if (visualizerShowLeftButton != nullptr) visualizerShowLeftButton->setVisible(false);
        if (visualizerShowRightButton != nullptr) visualizerShowRightButton->setVisible(false);
        if (visualizerShowMidButton != nullptr) visualizerShowMidButton->setVisible(false);
        if (visualizerShowSideButton != nullptr) visualizerShowSideButton->setVisible(false);
        filterViewport.setVisible(false);
        if (addFilterButton != nullptr) addFilterButton->setVisible(false);
        if (clearFiltersButton != nullptr) clearFiltersButton->setVisible(false);
        if (sortPlaceButton != nullptr) sortPlaceButton->setVisible(false);
        if (sortFreqButton != nullptr) sortFreqButton->setVisible(false);
        if (sortDuoButton != nullptr) sortDuoButton->setVisible(false);
        for (auto& section : bellSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(false);
            section->moveDownButton->setVisible(false);
            section->header->setVisible(false);
            section->typeControl->setVisible(false);
            section->lrmsControl->setVisible(false);
            section->slopeControl->setVisible(false);
            section->frequencyControl->setVisible(false);
            section->bandwidthControl->setVisible(false);
            section->gainControl->setVisible(false);
            section->bypassButton->setVisible(false);
            section->deleteButton->setVisible(false);
        }

        if (speAttackControl != nullptr) speAttackControl->setVisible(false);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
        if (speKneeControl != nullptr) speKneeControl->setVisible(false);
        if (speRatioControl != nullptr) speRatioControl->setVisible(false);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoLinkButton != nullptr) speDualMonoLinkButton->setVisible(false);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
        if (mieModuleEditor != nullptr) mieModuleEditor->setVisible(mieModuleLoaded);
        if (mxeModuleEditor != nullptr) mxeModuleEditor->setVisible(mxeModuleLoaded);
        if (tseModuleEditor != nullptr) tseModuleEditor->setVisible(tseModuleLoaded);

        return;
    }

    if (speModuleLoaded)
    {
        if (presetsSection != nullptr)
        {
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

        if (speAnalyserComponent != nullptr)
            speAnalyserComponent->setVisible(true);

        if (visualizerRangeLowControl != nullptr) visualizerRangeLowControl->setVisible(false);
        if (visualizerRangeHighControl != nullptr) visualizerRangeHighControl->setVisible(false);
        if (visualizerCursorButton != nullptr) visualizerCursorButton->setVisible(false);
        if (visualizerShowStereoButton != nullptr) visualizerShowStereoButton->setVisible(false);
        if (visualizerShowLeftButton != nullptr) visualizerShowLeftButton->setVisible(false);
        if (visualizerShowRightButton != nullptr) visualizerShowRightButton->setVisible(false);
        if (visualizerShowMidButton != nullptr) visualizerShowMidButton->setVisible(false);
        if (visualizerShowSideButton != nullptr) visualizerShowSideButton->setVisible(false);
        filterViewport.setVisible(true);
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
        if (speDualMonoLinkButton != nullptr) speDualMonoLinkButton->setVisible(true);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(true);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(true);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(true);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(true);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(true);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(true);
        if (speAttackControl != nullptr) speAttackControl->setVisible(true);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(true);
        if (speKneeControl != nullptr) speKneeControl->setVisible(true);
        if (speRatioControl != nullptr) speRatioControl->setVisible(true);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(true);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(true);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(true);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(visualizerExpanded);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(visualizerExpanded);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(visualizerExpanded);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(visualizerExpanded);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(visualizerExpanded);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(visualizerExpanded);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(visualizerExpanded);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(visualizerExpanded);


        for (auto& section : bellSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(false);
            section->moveDownButton->setVisible(false);
            section->header->setVisible(false);
            section->typeControl->setVisible(false);
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

    if (speAttackControl != nullptr) speAttackControl->setVisible(false);
    if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
    if (speKneeControl != nullptr) speKneeControl->setVisible(false);
    if (speRatioControl != nullptr) speRatioControl->setVisible(false);
    if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
    if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
    if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
    if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
    if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
    if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
    if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
    if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
    if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
    if (speDualMonoLinkButton != nullptr) speDualMonoLinkButton->setVisible(false);
    if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
    if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
    if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
    if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
    if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
    if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
    if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
    if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
    if (speAnalyserComponent != nullptr) speAnalyserComponent->setVisible(false);


    if (visualizerHeader != nullptr)
        visualizerHeader->setVisible(false);

    if (clearFiltersButton != nullptr)
    {
        clearFiltersButton->setVisible(eqeModuleLoaded);
        clearFiltersButton->setEnabled(activeBellCount > 0);
        clearFiltersButton->setAlpha(activeBellCount > 0 ? 1.0f : 0.45f);
    }

    updateUndoRedoButtons();
    const auto canSortFilters = activeBellCount > 1;

    if (sortPlaceButton != nullptr)
    {
        sortPlaceButton->setVisible(eqeModuleLoaded);
        sortPlaceButton->setEnabled(canSortFilters);
        sortPlaceButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    if (sortFreqButton != nullptr)
    {
        sortFreqButton->setVisible(eqeModuleLoaded);
        sortFreqButton->setEnabled(canSortFilters);
        sortFreqButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    if (sortDuoButton != nullptr)
    {
        sortDuoButton->setVisible(eqeModuleLoaded);
        sortDuoButton->setEnabled(canSortFilters);
        sortDuoButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    filterViewport.setVisible(eqeModuleLoaded);

    if (presetsSection != nullptr)
    {
        presetsSection->presetCombo.setVisible(eqeModuleLoaded);
        presetsSection->adButton->setVisible(eqeModuleLoaded);
        presetsSection->saveButton->setVisible(eqeModuleLoaded);
        presetsSection->renameButton->setVisible(eqeModuleLoaded);
        presetsSection->defaultButton->setVisible(eqeModuleLoaded);
        presetsSection->deleteButton->setVisible(eqeModuleLoaded);
    }

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

    if (visualizerComponent != nullptr)
        visualizerComponent->setVisible(false);

    for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

        if (section == nullptr)
            continue;

        const auto orderPosition = getBellOrderPositionForIndex(bellIndex);
        const auto isActive = eqeModuleLoaded && orderPosition >= 0;
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
        section->header->setToggleState(false, juce::dontSendNotification);
        section->typeControl->setVisible(isActive);
        section->lrmsControl->setVisible(isActive);
        section->slopeControl->setVisible(isActive);
        section->slopeControl->setInteractionEnabled(! slopeInactive);
        if (slopeInactive || bellOrderOff)
            section->slopeControl->setOverrideText("OFF");
        else
            section->slopeControl->clearOverrideText();
        section->frequencyControl->setVisible(isActive);
        section->bandwidthControl->setVisible(isActive);
        section->bandwidthControl->setInteractionEnabled(isBell && ! bandwidthInactive);
        if (isBell && ! bandwidthInactive)
            section->bandwidthControl->clearOverrideText();
        else
            section->bandwidthControl->setOverrideText("OFF");
        section->gainControl->setVisible(isActive);
        section->gainControl->setInteractionEnabled(! gainInactive);
        if (gainInactive)
            section->gainControl->setOverrideText("OFF");
        else
            section->gainControl->clearOverrideText();
        section->bypassButton->setVisible(isActive);
        section->deleteButton->setVisible(isActive);
        section->deleteButton->setEnabled(activeBellCount > 0);
        section->deleteButton->setAlpha(activeBellCount > 0 ? 1.0f : 0.45f);
    }

    if (addFilterButton != nullptr)
    {
        const auto canAddFilter = activeBellCount < VxAudioProcessor::maxBellFilterCount;
        addFilterButton->setVisible(eqeModuleLoaded);
        addFilterButton->setEnabled(canAddFilter);
        addFilterButton->setAlpha(canAddFilter ? 1.0f : 0.45f);
    }
}
