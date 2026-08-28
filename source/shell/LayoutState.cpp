#include "EditorFilterSection.h"
#include "../crossover/ModuleComponent.h"
#include "UiConstants.h"
#include "EditorPresetSections.h"

#include <algorithm>


void AvaAudioProcessorEditor::updateSectionStates()
{
    constexpr auto globalControlsVisible = true;

    const auto activeFilterCount = getActiveFilterCount();
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

    auto setEqlFilterSectionsVisible = [this] (const bool shouldShow)
    {
        for (auto& section : filterSections)
        {
            if (section == nullptr)
                continue;

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

    auto setEqlControlsVisible = [this, setComponentVisible, setPresetsVisible, setEqlFilterSectionsVisible] (const bool shouldShow)
    {
        filterViewport.setVisible(shouldShow);
        setPresetsVisible(shouldShow);
        setComponentVisible(addFilterButton.get(), shouldShow);
        setComponentVisible(sortPlaceButton.get(), shouldShow);
        setComponentVisible(sortFreqButton.get(), shouldShow);
        setComponentVisible(sortDuoButton.get(), shouldShow);
        setEqlFilterSectionsVisible(shouldShow);
    };

    auto setFftControlsVisible = [this, setComponentVisible] (const bool shouldShow)
    {
        const auto phaseMode = fftDynamicModeControl != nullptr
            && fftDynamicModeControl->getSelectedChoiceIndex() == 1;

        setComponentVisible(fftAnalyserComponent.get(), shouldShow);
        setComponentVisible(fftAnalyserRangeControl.get(), shouldShow);
        setComponentVisible(fftAttackControl.get(), shouldShow);
        setComponentVisible(fftReleaseControl.get(), shouldShow);
        setComponentVisible(fftKneeControl.get(), shouldShow);
        setComponentVisible(fftRatioControl.get(), shouldShow);
        setComponentVisible(fftFloorControl.get(), shouldShow && phaseMode);
        setComponentVisible(fftGeneralProcessorHeader.get(), shouldShow);
        setComponentVisible(fftDspFftSizeControl.get(), shouldShow);
        setComponentVisible(fftDspOverlapControl.get(), shouldShow);
        setComponentVisible(fftDspSlopeControl.get(), shouldShow);
        setComponentVisible(fftPhaseImpactControl.get(), shouldShow && phaseMode);
        setComponentVisible(fftDeltaButton.get(), shouldShow);
        setComponentVisible(fftDualMonoLeftThresholdControl.get(), shouldShow);
        setComponentVisible(fftDualMonoLeftAdaptiveControl.get(), shouldShow);
        setComponentVisible(fftDualMonoRightThresholdControl.get(), shouldShow && ! phaseMode);
        setComponentVisible(fftDualMonoRightAdaptiveControl.get(), shouldShow && ! phaseMode);
        setComponentVisible(fftDynamicProcessorHeader.get(), shouldShow);
        setComponentVisible(fftDynamicModeControl.get(), shouldShow);
        setComponentVisible(fftDualMonoLinkButton.get(), shouldShow && ! phaseMode);
        setComponentVisible(fftAdaptiveSettingsHeader.get(), shouldShow);
        setComponentVisible(fftAdaptiveOffsetControl.get(), shouldShow);
        setComponentVisible(fftAdaptiveAttackControl.get(), shouldShow);
        setComponentVisible(fftAdaptiveHoldControl.get(), shouldShow);
        setComponentVisible(fftAdaptiveReleaseControl.get(), shouldShow);
        setComponentVisible(fftAnalyserTimeControl.get(), shouldShow);

        if (fftDualMonoLeftThresholdControl != nullptr)
            fftDualMonoLeftThresholdControl->setTitleText(phaseMode ? "THRESH" : "L.THRESH");
        if (fftDualMonoLeftAdaptiveControl != nullptr)
            fftDualMonoLeftAdaptiveControl->setTitleText(phaseMode ? "ADAP" : "L.ADAP");
        if (fftDualMonoRightThresholdControl != nullptr)
            fftDualMonoRightThresholdControl->setTitleText("R.THRESH");
        if (fftDualMonoRightAdaptiveControl != nullptr)
            fftDualMonoRightAdaptiveControl->setTitleText("R.ADAP");

        if (fftAdaptiveOffsetControl != nullptr)
            fftAdaptiveOffsetControl->setTitleText("OFFSET");

        if (fftDualMonoLinkButton != nullptr)
        {
            fftDualMonoLinkButton->setButtonText("LINK-LR (STEREO)");
            fftDualMonoLinkButton->setEnabled(true);
        }
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

    ensureModuleTitle();

    const auto hostParametersVisible = hostParametersExpanded;
    const auto moduleContentVisible = crossoverEditor != nullptr
        && ! dynamic_cast<CrossoverModuleComponent*>(crossoverEditor.get())->isCrossoverSettingsSelected();

    if (moduleTitle != nullptr)
    {
        const juce::String moduleLabel = eqlModuleLoaded ? "EQL" : (fftModuleLoaded ? "FFT" : "");
        moduleTitle->setButtonText(moduleLabel);
        moduleTitle->setVisible(moduleContentVisible && moduleLabel.isNotEmpty());
    }

    hostParametersViewport.setVisible(hostParametersVisible);

    if (moduleAddButton != nullptr)
    {
        const auto noModuleLoaded = audioProcessor.getActiveModule() == AvaAudioProcessor::ActiveModule::none;
        moduleAddButton->setVisible(noModuleLoaded && moduleContentVisible && ! hostParametersExpanded);
        moduleAddButton->setEnabled(noModuleLoaded && moduleContentVisible);
    }

    if (globalBypassButton != nullptr)
        globalBypassButton->setVisible(globalControlsVisible);

    if (undoButton != nullptr)
        undoButton->setVisible(globalControlsVisible);

    if (redoButton != nullptr)
        redoButton->setVisible(globalControlsVisible);

    if (abSlotAButton != nullptr)
        abSlotAButton->setVisible(globalControlsVisible);

    if (abSwitchButton != nullptr)
        abSwitchButton->setVisible(globalControlsVisible);

    if (abSlotBButton != nullptr)
        abSlotBButton->setVisible(globalControlsVisible);

    for (auto& hostSlotNameField : hostSlotNameFields)
        if (hostSlotNameField != nullptr)
            hostSlotNameField->setVisible(hostParametersVisible);

    for (auto& hostSlotButton : hostSlotButtons)
        if (hostSlotButton != nullptr)
            hostSlotButton->setVisible(hostParametersVisible);

    if (tlsModuleEditor != nullptr)
        tlsModuleEditor->setVisible(tlsModuleLoaded && moduleContentVisible);

    if (dynModuleEditor != nullptr)
        dynModuleEditor->setVisible(dynModuleLoaded && moduleContentVisible);

    if (trsModuleEditor != nullptr)
        trsModuleEditor->setVisible(trsModuleLoaded && moduleContentVisible);

    if (! moduleContentVisible)
    {
        setEqlControlsVisible(false);
        setFftControlsVisible(false);
        return;
    }

    if (! eqlModuleLoaded && ! fftModuleLoaded && ! tlsModuleLoaded && ! dynModuleLoaded && ! trsModuleLoaded)
    {
        setEqlControlsVisible(false);
        setFftControlsVisible(false);
        setComponentVisible(tlsModuleEditor.get(), false);
        setComponentVisible(dynModuleEditor.get(), false);
        setComponentVisible(trsModuleEditor.get(), false);

        return;
    }

    if (tlsModuleLoaded || dynModuleLoaded || trsModuleLoaded)
    {
        setEqlControlsVisible(false);
        setFftControlsVisible(false);
        setComponentVisible(tlsModuleEditor.get(), tlsModuleLoaded);
        setComponentVisible(dynModuleEditor.get(), dynModuleLoaded);
        setComponentVisible(trsModuleEditor.get(), trsModuleLoaded);

        return;
    }

    if (fftModuleLoaded)
    {
        setPresetsVisible(false);
        setFftControlsVisible(true);
        filterViewport.setVisible(true);
        setComponentVisible(addFilterButton.get(), false);
        setComponentVisible(sortPlaceButton.get(), false);
        setComponentVisible(sortFreqButton.get(), false);
        setComponentVisible(sortDuoButton.get(), false);
        setEqlFilterSectionsVisible(false);

        return;
    }

    setFftControlsVisible(false);

    updateUndoRedoButtons();
    const auto canSortFilters = activeFilterCount > 1;

    if (sortPlaceButton != nullptr)
    {
        sortPlaceButton->setVisible(eqlModuleLoaded);
        sortPlaceButton->setEnabled(canSortFilters);
        sortPlaceButton->setAlpha(1.0f);
    }

    if (sortFreqButton != nullptr)
    {
        sortFreqButton->setVisible(eqlModuleLoaded);
        sortFreqButton->setEnabled(canSortFilters);
        sortFreqButton->setAlpha(1.0f);
    }

    if (sortDuoButton != nullptr)
    {
        sortDuoButton->setVisible(eqlModuleLoaded);
        sortDuoButton->setEnabled(canSortFilters);
        sortDuoButton->setAlpha(1.0f);
    }

    filterViewport.setVisible(eqlModuleLoaded);
    setPresetsVisible(eqlModuleLoaded);

    for (int orderPosition = 0; orderPosition < static_cast<int>(filterOrderLabels.size()); ++orderPosition)
        if (auto* label = filterOrderLabels[static_cast<size_t>(orderPosition)].get())
            label->setVisible(eqlModuleLoaded && orderPosition < activeFilterCount);

    for (int filterIndex = 0; filterIndex < AvaAudioProcessor::maxEqlFilterCount; ++filterIndex)
    {
        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            continue;

        const auto orderPosition = getFilterOrderPositionForIndex(filterIndex);
        const auto isActive = eqlModuleLoaded && orderPosition >= 0;
        const auto sectionExpanded = isActive && section->expanded;
        const auto filterType = section->getFilterType();
        const auto isVolume = filterType == EqlModuleProcessor::FilterType::volume;
        const auto isBell = filterType == EqlModuleProcessor::FilterType::bell;
        const auto isPhasePlace = section->getPlace() >= 5 && section->getPlace() <= 7;
        const auto bandwidthInactive = section->isBandwidthInactiveAtCurrentSlope();
        const auto slopeInactive = section->isSlopeInactive();
        const auto gainInactive = section->isGainInactive();
        const auto filterOrderOff = filterType == EqlModuleProcessor::FilterType::bell
            && section->slopeControl->getSelectedChoiceIndex() == 0;
        section->updatePlaceChoicesForType(false);
        if (auto* eqlProcessor = getActiveEqlProcessor())
            section->header->setButtonText(eqlProcessor->getFilterHeaderText(filterIndex, orderPosition));
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
        section->updateFrequencyRangeForType();
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
        addFilterButton->setVisible(eqlModuleLoaded);
        addFilterButton->setEnabled(true);
        addFilterButton->setAlpha(1.0f);
    }
}
