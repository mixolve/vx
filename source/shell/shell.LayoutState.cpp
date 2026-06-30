#include "shell.EditorFilterSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

#include <algorithm>


void VxAudioProcessorEditor::updateSectionStates()
{
    constexpr auto showShellGlobalStrip = true;

    const auto activeFilterCount = getActiveFilterCount();
    const auto activeModule = audioProcessor.getActiveModule();
    auto setSpePhaseControlsVisible = [this] (const bool shouldShow)
    {
        const auto activePhaseFilterCount = shouldShow ? getActiveSpePhaseFilterCount() : 0;

        if (spePhaseAddButton != nullptr)
        {
            spePhaseAddButton->setVisible(shouldShow);
            spePhaseAddButton->setEnabled(activePhaseFilterCount < spePhaseFilterControlCount);
            spePhaseAddButton->setAlpha(activePhaseFilterCount < spePhaseFilterControlCount ? 1.0f : 0.45f);
        }

        for (auto filterIndex = 0; filterIndex < spePhaseFilterControlCount; ++filterIndex)
        {
            const auto filterVisible = shouldShow && filterIndex < activePhaseFilterCount;
            const auto filterExpanded = filterVisible && spePhaseExpanded[static_cast<size_t>(filterIndex)];

            if (spePhaseRemoveButtons[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseRemoveButtons[static_cast<size_t>(filterIndex)]->setVisible(filterVisible);
            if (spePhaseHeaderButtons[static_cast<size_t>(filterIndex)] != nullptr)
            {
                spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->setVisible(filterVisible);
                spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->setButtonText(filterVisible ? getSpePhaseFilterHeaderText(filterIndex) : juce::String {});
                spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->setToggleState(filterExpanded, juce::dontSendNotification);
            }
            if (spePhaseTypeControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseTypeControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);
            if (spePhasePlaceControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhasePlaceControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);
            if (spePhaseSlopeControls[static_cast<size_t>(filterIndex)] != nullptr)
            {
                const auto orderActive = shouldEnableSpePhaseOrder(filterIndex);
                spePhaseSlopeControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);
                spePhaseSlopeControls[static_cast<size_t>(filterIndex)]->setInteractionEnabled(orderActive);

                if (orderActive)
                    spePhaseSlopeControls[static_cast<size_t>(filterIndex)]->clearOverrideText();
                else
                    spePhaseSlopeControls[static_cast<size_t>(filterIndex)]->setOverrideText("OFF");
            }
            if (spePhaseFrequencyControls[static_cast<size_t>(filterIndex)] != nullptr)
            {
                const auto frequencyActive = shouldEnableSpePhaseFrequency(filterIndex);
                spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);
                spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->setInteractionEnabled(frequencyActive);

                if (frequencyActive)
                    spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->clearOverrideText();
                else
                    spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->setOverrideText("OFF");
            }
            if (spePhaseBandwidthControls[static_cast<size_t>(filterIndex)] != nullptr)
            {
                const auto bandwidthActive = shouldEnableSpePhaseBandwidth(filterIndex);
                spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);
                spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]->setInteractionEnabled(bandwidthActive);

                if (bandwidthActive)
                    spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]->clearOverrideText();
                else
                    spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]->setOverrideText("OFF");
            }
            if (spePhaseImpactControls[static_cast<size_t>(filterIndex)] != nullptr)
            {
                const auto impactActive = shouldShowSpePhaseImpact(filterIndex);
                spePhaseImpactControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);
                spePhaseImpactControls[static_cast<size_t>(filterIndex)]->setInteractionEnabled(impactActive);

                if (impactActive)
                    spePhaseImpactControls[static_cast<size_t>(filterIndex)]->clearOverrideText();
                else
                    spePhaseImpactControls[static_cast<size_t>(filterIndex)]->setOverrideText("OFF");
            }
        }
    };

    if (shellGlobalHeader != nullptr)
    {
        shellGlobalHeader->setVisible(showShellGlobalStrip);
        shellGlobalHeader->setButtonText("C");
        shellGlobalHeader->setToggleState(false, juce::dontSendNotification);
    }

    if (footerTab != nullptr)
        footerTab->setVisible(true);

    if (shellGlobalHostHeader != nullptr)
    {
        shellGlobalHostHeader->setVisible(showShellGlobalStrip);
        shellGlobalHostHeader->setToggleState(shellGlobalHostExpanded, juce::dontSendNotification);
    }

    refreshModuleTabButton();

    if (moduleTabButton != nullptr)
    {
        const auto module = audioProcessor.getActiveModule();
        const auto active = activeModule == module;
        const auto shouldShow = module != VxAudioProcessor::ActiveModule::none;

        moduleTabButton->setVisible(shouldShow);
        moduleTabButton->setButtonText(audioProcessor.getLoadedModuleLabel());
        moduleTabButton->setToggleState(active, juce::dontSendNotification);
    }

    const auto shellHostVisible = shellGlobalHostExpanded;

    shellGlobalHostViewport.setVisible(shellHostVisible);

    if (moduleAddButton != nullptr)
    {
        const auto noModuleLoaded = ! audioProcessor.isModuleLoaded();
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


        speAnalyserViewport.setVisible(false);
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

        if (clearFiltersButton != nullptr)
            clearFiltersButton->setVisible(false);

        if (sortPlaceButton != nullptr)
            sortPlaceButton->setVisible(false);

        if (sortFreqButton != nullptr)
            sortFreqButton->setVisible(false);

        if (sortDuoButton != nullptr)
            sortDuoButton->setVisible(false);










        for (auto& section : filterSections)
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
        }

        if (speAttackControl != nullptr) speAttackControl->setVisible(false);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
        if (speKneeControl != nullptr) speKneeControl->setVisible(false);
        if (speRatioControl != nullptr) speRatioControl->setVisible(false);
        if (speFftProcessorHeader != nullptr) speFftProcessorHeader->setVisible(false);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
        if (speDspHopDivisorControl != nullptr) speDspHopDivisorControl->setVisible(false);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
        if (speDynamicProcessorHeader != nullptr) speDynamicProcessorHeader->setVisible(false);
        if (speDualMonoLinkButton != nullptr) speDualMonoLinkButton->setVisible(false);
        if (spePhaseProcessorHeader != nullptr) spePhaseProcessorHeader->setVisible(false);
        setSpePhaseControlsVisible(false);
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

        speAnalyserViewport.setVisible(false);
        if (speAnalyserComponent != nullptr)
            speAnalyserComponent->setVisible(false);
        filterViewport.setVisible(false);
        if (addFilterButton != nullptr) addFilterButton->setVisible(false);
        if (clearFiltersButton != nullptr) clearFiltersButton->setVisible(false);
        if (sortPlaceButton != nullptr) sortPlaceButton->setVisible(false);
        if (sortFreqButton != nullptr) sortFreqButton->setVisible(false);
        if (sortDuoButton != nullptr) sortDuoButton->setVisible(false);
        for (auto& section : filterSections)
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
        }

        if (speAttackControl != nullptr) speAttackControl->setVisible(false);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
        if (speKneeControl != nullptr) speKneeControl->setVisible(false);
        if (speRatioControl != nullptr) speRatioControl->setVisible(false);
        if (speFftProcessorHeader != nullptr) speFftProcessorHeader->setVisible(false);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
        if (speDspHopDivisorControl != nullptr) speDspHopDivisorControl->setVisible(false);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
        if (speDynamicProcessorHeader != nullptr) speDynamicProcessorHeader->setVisible(false);
        if (speDualMonoLinkButton != nullptr) speDualMonoLinkButton->setVisible(false);
        if (spePhaseProcessorHeader != nullptr) spePhaseProcessorHeader->setVisible(false);
        setSpePhaseControlsVisible(false);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
        speAnalyserViewport.setVisible(false);
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


        speAnalyserViewport.setVisible(true);

        if (speAnalyserComponent != nullptr)
            speAnalyserComponent->setVisible(true);

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
        if (speFftProcessorHeader != nullptr) speFftProcessorHeader->setVisible(true);
        if (speDynamicProcessorHeader != nullptr) speDynamicProcessorHeader->setVisible(true);
        if (spePhaseProcessorHeader != nullptr) spePhaseProcessorHeader->setVisible(true);
        setSpePhaseControlsVisible(true);
        if (speAttackControl != nullptr) speAttackControl->setVisible(true);
        if (speReleaseControl != nullptr) speReleaseControl->setVisible(true);
        if (speKneeControl != nullptr) speKneeControl->setVisible(true);
        if (speRatioControl != nullptr) speRatioControl->setVisible(true);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(true);
        if (speDspHopDivisorControl != nullptr) speDspHopDivisorControl->setVisible(true);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(true);
        if (speDeltaButton != nullptr) speDeltaButton->setVisible(true);
        if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(true);
        if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(true);
        if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(true);
        if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(true);
        if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(true);
        if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(true);
        if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(true);
        if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(true);


        for (auto& section : filterSections)
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
        }

        return;
    }

    if (speAttackControl != nullptr) speAttackControl->setVisible(false);
    if (speReleaseControl != nullptr) speReleaseControl->setVisible(false);
    if (speKneeControl != nullptr) speKneeControl->setVisible(false);
    if (speRatioControl != nullptr) speRatioControl->setVisible(false);
    if (speFftProcessorHeader != nullptr) speFftProcessorHeader->setVisible(false);
    if (speDspFftSizeControl != nullptr) speDspFftSizeControl->setVisible(false);
    if (speDspHopDivisorControl != nullptr) speDspHopDivisorControl->setVisible(false);
    if (speDspSlopeControl != nullptr) speDspSlopeControl->setVisible(false);
    if (speDeltaButton != nullptr) speDeltaButton->setVisible(false);
    if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->setVisible(false);
    if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->setVisible(false);
    if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->setVisible(false);
    if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->setVisible(false);
    if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->setVisible(false);
    if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->setVisible(false);
    if (speDynamicProcessorHeader != nullptr) speDynamicProcessorHeader->setVisible(false);
    if (speDualMonoLinkButton != nullptr) speDualMonoLinkButton->setVisible(false);
    if (spePhaseProcessorHeader != nullptr) spePhaseProcessorHeader->setVisible(false);
    setSpePhaseControlsVisible(false);
    if (speAnalyserFftSizeControl != nullptr) speAnalyserFftSizeControl->setVisible(false);
    if (speAnalyserOverlapControl != nullptr) speAnalyserOverlapControl->setVisible(false);
    if (speAnalyserLeftControl != nullptr) speAnalyserLeftControl->setVisible(false);
    if (speAnalyserRightControl != nullptr) speAnalyserRightControl->setVisible(false);
    if (speAnalyserRangeLowControl != nullptr) speAnalyserRangeLowControl->setVisible(false);
    if (speAnalyserRangeHighControl != nullptr) speAnalyserRangeHighControl->setVisible(false);
    if (speAnalyserSlopeControl != nullptr) speAnalyserSlopeControl->setVisible(false);
    if (speAnalyserTimeControl != nullptr) speAnalyserTimeControl->setVisible(false);
    if (speAnalyserComponent != nullptr) speAnalyserComponent->setVisible(false);
    speAnalyserViewport.setVisible(false);



    if (clearFiltersButton != nullptr)
    {
        clearFiltersButton->setVisible(eqeModuleLoaded);
        clearFiltersButton->setEnabled(activeFilterCount > 0);
        clearFiltersButton->setAlpha(activeFilterCount > 0 ? 1.0f : 0.45f);
    }

    updateUndoRedoButtons();
    const auto canSortFilters = activeFilterCount > 1;

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










    for (int filterIndex = 0; filterIndex < VxAudioProcessor::maxEqeFilterCount; ++filterIndex)
    {
        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            continue;

        const auto orderPosition = getFilterOrderPositionForIndex(filterIndex);
        const auto isActive = eqeModuleLoaded && orderPosition >= 0;
        const auto sectionExpanded = isActive && section->expanded;
        const auto filterType = section->getFilterType();
        const auto isVolume = filterType == EqeModuleProcessor::FilterType::volume;
        const auto isBell = filterType == EqeModuleProcessor::FilterType::bell;
        const auto bandwidthInactive = section->isBandwidthInactiveAtCurrentSlope();
        const auto slopeInactive = section->isSlopeInactive();
        const auto gainInactive = section->isGainInactive();
        const auto filterOrderOff = filterType == EqeModuleProcessor::FilterType::bell
            && section->slopeControl->getSelectedChoiceIndex() == 0;
        const auto canMoveUp = isActive && orderPosition > 0;
        const auto canMoveDown = isActive && orderPosition + 1 < activeFilterCount;

        section->updatePlaceChoicesForType(false);
        section->moveUpButton->setVisible(isActive);
        section->moveUpButton->setEnabled(canMoveUp);
        section->moveUpButton->setAlpha(canMoveUp ? 1.0f : 0.45f);
        section->moveDownButton->setVisible(isActive);
        section->moveDownButton->setEnabled(canMoveDown);
        section->moveDownButton->setAlpha(canMoveDown ? 1.0f : 0.45f);
        if (auto* eqeProcessor = audioProcessor.getActiveEqeModuleProcessor())
            section->header->setButtonText(eqeProcessor->getFilterHeaderText(filterIndex, orderPosition));
        else
            section->header->setButtonText({});
        section->header->setVisible(isActive);
        section->header->setToggleState(sectionExpanded, juce::dontSendNotification);
        section->typeControl->setVisible(sectionExpanded);
        section->lrmsControl->setVisible(sectionExpanded);
        section->slopeControl->setVisible(sectionExpanded);
        section->slopeControl->setInteractionEnabled(! slopeInactive);
        if (slopeInactive || filterOrderOff)
            section->slopeControl->setOverrideText("OFF");
        else
            section->slopeControl->clearOverrideText();
        section->frequencyControl->setVisible(sectionExpanded);
        section->frequencyControl->setInteractionEnabled(! isVolume);
        if (isVolume)
            section->frequencyControl->setOverrideText("OFF");
        else
            section->frequencyControl->clearOverrideText();
        section->bandwidthControl->setVisible(sectionExpanded);
        section->bandwidthControl->setInteractionEnabled(isBell && ! bandwidthInactive);
        if (isBell && ! bandwidthInactive)
            section->bandwidthControl->clearOverrideText();
        else
            section->bandwidthControl->setOverrideText("OFF");
        section->gainControl->setVisible(sectionExpanded);
        section->gainControl->setInteractionEnabled(! gainInactive);
        if (gainInactive)
            section->gainControl->setOverrideText("OFF");
        else
            section->gainControl->clearOverrideText();
        section->bypassButton->setVisible(isActive);
    }

    if (addFilterButton != nullptr)
    {
        const auto canAddFilter = activeFilterCount < VxAudioProcessor::maxEqeFilterCount;
        addFilterButton->setVisible(eqeModuleLoaded);
        addFilterButton->setEnabled(canAddFilter);
        addFilterButton->setAlpha(canAddFilter ? 1.0f : 0.45f);
    }
}
