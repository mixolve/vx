#include "shell.EditorFilterSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

#include <algorithm>


void VxAudioProcessorEditor::updateSectionStates()
{
    constexpr auto globalControlsVisible = true;

    const auto activeFilterCount = getActiveFilterCount();
    const auto activeModule = audioProcessor.getActiveModule();
    auto setActiveSpeFilterControlState = [] (auto* control, const bool visible, const bool active)
    {
        if (control == nullptr)
            return;

        control->setVisible(visible);
        control->setInteractionEnabled(active);

        if (active)
            control->clearOverrideText();
        else
            control->setOverrideText("OFF");
    };

    auto setSpeFilterControlsVisible = [setActiveSpeFilterControlState] (const bool shouldShow,
                                                                         auto* addButton,
                                                                         const int activeSpeFilterCount,
                                                                         auto& bypassButtons,
                                                                         auto& headerButtons,
                                                                         auto& typeControls,
                                                                         auto& placeControls,
                                                                         auto& slopeControls,
                                                                         auto& frequencyControls,
                                                                         auto& bandwidthControls,
                                                                         auto& impactControls,
                                                                         auto& expandedStates,
                                                                         auto getHeaderText,
                                                                         auto shouldEnableOrder,
                                                                         auto shouldEnableFrequency,
                                                                         auto shouldEnableBandwidth,
                                                                         auto shouldShowImpact)
    {
        if (addButton != nullptr)
        {
            addButton->setVisible(shouldShow);
            addButton->setEnabled(activeSpeFilterCount < speFilterControlCount);
            addButton->setAlpha(activeSpeFilterCount < speFilterControlCount ? 1.0f : 0.45f);
        }

        for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
        {
            const auto filterVisible = shouldShow && filterIndex < activeSpeFilterCount;
            const auto filterExpanded = filterVisible && expandedStates[static_cast<size_t>(filterIndex)];

            if (bypassButtons[static_cast<size_t>(filterIndex)] != nullptr)
                bypassButtons[static_cast<size_t>(filterIndex)]->setVisible(filterVisible);
            if (headerButtons[static_cast<size_t>(filterIndex)] != nullptr)
            {
                headerButtons[static_cast<size_t>(filterIndex)]->setVisible(filterVisible);
                headerButtons[static_cast<size_t>(filterIndex)]->setButtonText(filterVisible ? getHeaderText(filterIndex) : juce::String {});
                headerButtons[static_cast<size_t>(filterIndex)]->setToggleState(filterExpanded, juce::dontSendNotification);
            }

            if (typeControls[static_cast<size_t>(filterIndex)] != nullptr)
                typeControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);
            if (placeControls[static_cast<size_t>(filterIndex)] != nullptr)
                placeControls[static_cast<size_t>(filterIndex)]->setVisible(filterExpanded);

            setActiveSpeFilterControlState(slopeControls[static_cast<size_t>(filterIndex)].get(),
                                           filterExpanded,
                                           shouldEnableOrder(filterIndex));
            setActiveSpeFilterControlState(frequencyControls[static_cast<size_t>(filterIndex)].get(),
                                           filterExpanded,
                                           shouldEnableFrequency(filterIndex));
            setActiveSpeFilterControlState(bandwidthControls[static_cast<size_t>(filterIndex)].get(),
                                           filterExpanded,
                                           shouldEnableBandwidth(filterIndex));
            setActiveSpeFilterControlState(impactControls[static_cast<size_t>(filterIndex)].get(),
                                           filterExpanded,
                                           shouldShowImpact(filterIndex));
        }
    };

    auto setSpePhaseControlsVisible = [this, setSpeFilterControlsVisible] (const bool shouldShow)
    {
        setSpeFilterControlsVisible(shouldShow,
                                    spePhaseAddButton.get(),
                                    shouldShow ? getActiveSpePhaseFilterCount() : 0,
                                    spePhaseBypassButtons,
                                    spePhaseHeaderButtons,
                                    spePhaseTypeControls,
                                    spePhasePlaceControls,
                                    spePhaseSlopeControls,
                                    spePhaseFrequencyControls,
                                    spePhaseBandwidthControls,
                                    spePhaseImpactControls,
                                    spePhaseExpanded,
                                    [this] (const int filterIndex) { return getSpePhaseFilterHeaderText(filterIndex); },
                                    [this] (const int filterIndex) { return shouldEnableSpePhaseOrder(filterIndex); },
                                    [this] (const int filterIndex) { return shouldEnableSpePhaseFrequency(filterIndex); },
                                    [this] (const int filterIndex) { return shouldEnableSpePhaseBandwidth(filterIndex); },
                                    [this] (const int filterIndex) { return shouldShowSpePhaseImpact(filterIndex); });
    };

    auto setSpeAmplitudeControlsVisible = [this, setSpeFilterControlsVisible] (const bool shouldShow)
    {
        setSpeFilterControlsVisible(shouldShow,
                                    speAmplitudeAddButton.get(),
                                    shouldShow ? getActiveSpeAmplitudeFilterCount() : 0,
                                    speAmplitudeBypassButtons,
                                    speAmplitudeHeaderButtons,
                                    speAmplitudeTypeControls,
                                    speAmplitudePlaceControls,
                                    speAmplitudeSlopeControls,
                                    speAmplitudeFrequencyControls,
                                    speAmplitudeBandwidthControls,
                                    speAmplitudeImpactControls,
                                    speAmplitudeExpanded,
                                    [this] (const int filterIndex) { return getSpeAmplitudeFilterHeaderText(filterIndex); },
                                    [this] (const int filterIndex) { return shouldEnableSpeAmplitudeOrder(filterIndex); },
                                    [this] (const int filterIndex) { return shouldEnableSpeAmplitudeFrequency(filterIndex); },
                                    [this] (const int filterIndex) { return shouldEnableSpeAmplitudeBandwidth(filterIndex); },
                                    [this] (const int filterIndex) { return shouldShowSpeAmplitudeImpact(filterIndex); });
    };

    auto setComponentVisible = [] (auto* component, const bool shouldShow)
    {
        if (component != nullptr)
            component->setVisible(shouldShow);
    };

    auto setPresetsVisible = [this, setComponentVisible] (const bool shouldShow)
    {
        if (presetsSection == nullptr)
            return;

        setComponentVisible(&presetsSection->presetCombo, shouldShow);
        setComponentVisible(presetsSection->adButton.get(), shouldShow);
        setComponentVisible(presetsSection->saveButton.get(), shouldShow);
        setComponentVisible(presetsSection->renameButton.get(), shouldShow);
        setComponentVisible(presetsSection->defaultButton.get(), shouldShow);
        setComponentVisible(presetsSection->deleteButton.get(), shouldShow);
    };

    auto setEqeFilterSectionsVisible = [this] (const bool shouldShow)
    {
        for (auto& section : filterSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(shouldShow);
            section->moveDownButton->setVisible(shouldShow);
            section->header->setVisible(shouldShow);
            section->typeControl->setVisible(shouldShow);
            section->placeControl->setVisible(shouldShow);
            section->slopeControl->setVisible(shouldShow);
            section->frequencyControl->setVisible(shouldShow);
            section->bandwidthControl->setVisible(shouldShow);
            section->gainControl->setVisible(shouldShow);
            section->bypassButton->setVisible(shouldShow);
        }
    };

    auto setEqeControlsVisible = [this, setComponentVisible, setPresetsVisible, setEqeFilterSectionsVisible] (const bool shouldShow)
    {
        filterViewport.setVisible(shouldShow);
        setPresetsVisible(shouldShow);
        setComponentVisible(addFilterButton.get(), shouldShow);
        setComponentVisible(clearFiltersButton.get(), shouldShow);
        setComponentVisible(sortPlaceButton.get(), shouldShow);
        setComponentVisible(sortFreqButton.get(), shouldShow);
        setComponentVisible(sortDuoButton.get(), shouldShow);
        setEqeFilterSectionsVisible(shouldShow);
    };

    auto setSpeControlsVisible = [this, setComponentVisible, setSpePhaseControlsVisible, setSpeAmplitudeControlsVisible] (const bool shouldShow)
    {
        speAnalyserViewport.setVisible(shouldShow);
        setComponentVisible(speAnalyserComponent.get(), shouldShow);
        setComponentVisible(speAttackControl.get(), shouldShow);
        setComponentVisible(speReleaseControl.get(), shouldShow);
        setComponentVisible(speKneeControl.get(), shouldShow);
        setComponentVisible(speRatioControl.get(), shouldShow);
        setComponentVisible(speFftProcessorHeader.get(), shouldShow);
        setComponentVisible(speDspFftSizeControl.get(), shouldShow);
        setComponentVisible(speDspHopDivisorControl.get(), shouldShow);
        setComponentVisible(speDspSlopeControl.get(), shouldShow);
        setComponentVisible(speDeltaButton.get(), shouldShow);
        setComponentVisible(speDualMonoLeftThresholdControl.get(), shouldShow);
        setComponentVisible(speDualMonoLeftAdaptiveControl.get(), shouldShow);
        setComponentVisible(speDualMonoLeftAdaptiveOffsetControl.get(), shouldShow);
        setComponentVisible(speDualMonoRightThresholdControl.get(), shouldShow);
        setComponentVisible(speDualMonoRightAdaptiveControl.get(), shouldShow);
        setComponentVisible(speDualMonoRightAdaptiveOffsetControl.get(), shouldShow);
        setComponentVisible(speDynamicProcessorHeader.get(), shouldShow);
        setComponentVisible(speDualMonoLinkButton.get(), shouldShow);
        setComponentVisible(spePhaseProcessorHeader.get(), shouldShow);
        setSpePhaseControlsVisible(shouldShow);
        setComponentVisible(speAmplitudeProcessorHeader.get(), shouldShow);
        setSpeAmplitudeControlsVisible(shouldShow);
        setComponentVisible(speAnalyserSettingsHeader.get(), shouldShow);
        setComponentVisible(speAnalyserFftSizeControl.get(), shouldShow);
        setComponentVisible(speAnalyserOverlapControl.get(), shouldShow);
        setComponentVisible(speAnalyserLeftControl.get(), shouldShow);
        setComponentVisible(speAnalyserRightControl.get(), shouldShow);
        setComponentVisible(speAnalyserRangeLowControl.get(), shouldShow);
        setComponentVisible(speAnalyserRangeHighControl.get(), shouldShow);
        setComponentVisible(speAnalyserSlopeControl.get(), shouldShow);
        setComponentVisible(speAnalyserTimeControl.get(), shouldShow);
    };

    if (clipButton != nullptr)
    {
        clipButton->setVisible(globalControlsVisible);
        clipButton->setButtonText("C");
        clipButton->setToggleState(false, juce::dontSendNotification);
    }

    if (footerTab != nullptr)
        footerTab->setVisible(true);

    if (hostButton != nullptr)
    {
        hostButton->setVisible(globalControlsVisible);
        hostButton->setToggleState(hostParametersExpanded, juce::dontSendNotification);
    }

    refreshModuleTabButton();

    if (moduleTabButton != nullptr)
    {
        const auto module = audioProcessor.getActiveModule();
        const auto active = activeModule == module;
        const auto shouldShow = module != VxAudioProcessor::ActiveModule::none;

        moduleTabButton->setVisible(shouldShow);
        moduleTabButton->setButtonText(juce::String(VxAudioProcessor::stateIdForModule(module)).toUpperCase());
        moduleTabButton->setToggleState(active, juce::dontSendNotification);
    }

    const auto hostParametersVisible = hostParametersExpanded;

    hostParametersViewport.setVisible(hostParametersVisible);

    if (moduleAddButton != nullptr)
    {
        const auto noModuleLoaded = audioProcessor.getActiveModule() == VxAudioProcessor::ActiveModule::none;
        moduleAddButton->setVisible(noModuleLoaded && ! hostParametersExpanded);
        moduleAddButton->setEnabled(noModuleLoaded);
    }

    if (globalBypassButton != nullptr)
        globalBypassButton->setVisible(globalControlsVisible);

    if (undoButton != nullptr)
        undoButton->setVisible(globalControlsVisible);

    if (redoButton != nullptr)
        redoButton->setVisible(globalControlsVisible);

    if (abCompareButton != nullptr)
        abCompareButton->setVisible(globalControlsVisible);

    for (auto& hostSlotButton : hostSlotButtons)
        if (hostSlotButton != nullptr)
            hostSlotButton->setVisible(hostParametersVisible);

    if (mieModuleEditor != nullptr)
        mieModuleEditor->setVisible(mieModuleLoaded);

    if (mxeModuleEditor != nullptr)
        mxeModuleEditor->setVisible(mxeModuleLoaded);

    if (tseModuleEditor != nullptr)
        tseModuleEditor->setVisible(tseModuleLoaded);

    if (! eqeModuleLoaded && ! speModuleLoaded && ! mieModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
    {
        setEqeControlsVisible(false);
        setSpeControlsVisible(false);
        setComponentVisible(mieModuleEditor.get(), false);
        setComponentVisible(mxeModuleEditor.get(), false);
        setComponentVisible(tseModuleEditor.get(), false);

        return;
    }

    if (mieModuleLoaded || mxeModuleLoaded || tseModuleLoaded)
    {
        setEqeControlsVisible(false);
        setSpeControlsVisible(false);
        setComponentVisible(mieModuleEditor.get(), mieModuleLoaded);
        setComponentVisible(mxeModuleEditor.get(), mxeModuleLoaded);
        setComponentVisible(tseModuleEditor.get(), tseModuleLoaded);

        return;
    }

    if (speModuleLoaded)
    {
        setPresetsVisible(false);
        setSpeControlsVisible(true);
        filterViewport.setVisible(true);
        setComponentVisible(addFilterButton.get(), false);
        setComponentVisible(clearFiltersButton.get(), false);
        setComponentVisible(sortPlaceButton.get(), false);
        setComponentVisible(sortFreqButton.get(), false);
        setComponentVisible(sortDuoButton.get(), false);
        setEqeFilterSectionsVisible(false);

        return;
    }

    setSpeControlsVisible(false);

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
    setPresetsVisible(eqeModuleLoaded);

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
        const auto isPhasePlace = section->getPlace() >= 5 && section->getPlace() <= 7;
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
        if (auto* eqeProcessor = getActiveEqeProcessor())
            section->header->setButtonText(eqeProcessor->getFilterHeaderText(filterIndex, orderPosition));
        else
            section->header->setButtonText({});
        section->header->setVisible(isActive);
        section->header->setToggleState(sectionExpanded, juce::dontSendNotification);
        section->typeControl->setVisible(sectionExpanded);
        section->placeControl->setVisible(sectionExpanded);
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
        section->setGainDisplaysDegrees(isPhasePlace && ! gainInactive);
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
