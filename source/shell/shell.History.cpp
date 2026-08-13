#include "shell.EditorFilterSection.h"
#include "shell.ShellState.h"
#include "shell.SetupSupport.h"
#include "../modules/eql/module.eql.ProcessorSupport.h"
#include "../modules/multiband/tls/module.tls.PluginProcessor.h"
#include "../modules/multiband/dyn/module.dyn.PluginProcessor.h"
#include "../modules/fft/module.fft.FftProcessor.h"
#include "../modules/multiband/trs/module.trs.TrsProcessor.h"

juce::RangedAudioParameter* VxAudioProcessorEditor::findHostAssignableParameter(const juce::String& parameterId) const noexcept
{
    const auto trimmedParameterId = parameterId.trim();

    if (trimmedParameterId.isEmpty())
        return nullptr;

    if (auto* parameter = valueTreeState.getParameter(trimmedParameterId))
        return parameter;

    const auto findInModule = [&trimmedParameterId] (auto* processor) -> juce::RangedAudioParameter*
    {
        if (processor == nullptr)
            return nullptr;

        return processor->getValueTreeState().getParameter(trimmedParameterId);
    };

    switch (audioProcessor.getActiveModule())
    {
        case VxAudioProcessor::ActiveModule::eql: return findInModule(audioProcessor.getEqlModuleProcessor());
        case VxAudioProcessor::ActiveModule::fft: return findInModule(audioProcessor.getFftModuleProcessor());
        case VxAudioProcessor::ActiveModule::tls: return findInModule(audioProcessor.getTlsModuleProcessor());
        case VxAudioProcessor::ActiveModule::dyn: return findInModule(audioProcessor.getDynModuleProcessor());
        case VxAudioProcessor::ActiveModule::trs: return findInModule(audioProcessor.getTrsModuleProcessor());
        case VxAudioProcessor::ActiveModule::none: break;
    }

    return nullptr;
}

void VxAudioProcessorEditor::syncHostSlotAssignmentValue(const int slotIndex, const float normalizedValue)
{
    if (! juce::isPositiveAndBelow(slotIndex, static_cast<int>(hostSlotAssignments.size())))
        return;

    const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

    if (assignment.parameterId.isEmpty())
        return;

    if (auto* assignedParameter = findHostAssignableParameter(assignment.parameterId))
    {
        assignedParameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
    }
}

void VxAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == FftModuleProcessor::paramDynamicModeId)
    {
        juce::Component::SafePointer<VxAudioProcessorEditor> safeEditor(this);
        juce::MessageManager::callAsync([safeEditor]
        {
            if (safeEditor == nullptr)
                return;

            if (auto* processor = safeEditor->audioProcessor.getFftModuleProcessor())
                safeEditor->refreshFftAnalyserControls(*processor);

            safeEditor->updateSectionStates();
            safeEditor->resized();
            safeEditor->repaint();
        });
    }

    if (! suppressHostSlotAutomationSync)
    {
        juce::ScopedValueSetter<bool> scopedSyncGuard(suppressHostSlotAutomationSync, true);

        for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
        {
            const auto slotParameterId = VxAudioProcessor::getHostSlotParameterId(slotIndex);

            if (parameterID == slotParameterId)
            {
                syncHostSlotAssignmentValue(slotIndex, newValue);
                scheduleHistorySnapshot();
                return;
            }
        }

        for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
        {
            const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

            if (assignment.parameterId != parameterID)
                continue;

            auto* assignedParameter = findHostAssignableParameter(parameterID);
            auto* slotParameter = valueTreeState.getParameter(VxAudioProcessor::getHostSlotParameterId(slotIndex));

            if (assignedParameter == nullptr || slotParameter == nullptr)
                continue;

            const auto normalizedSourceValue = juce::jlimit(0.0f, 1.0f, assignedParameter->convertTo0to1(newValue));
            slotParameter->setValueNotifyingHost(normalizedSourceValue);
            break;
        }
    }

    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::resyncEditorFromProcessorState()
{
    restoreEditorStateFromValueTree();
    refreshModuleStateListeners();
    refreshModuleTabButton();
    updateSectionStates();
    syncFocusedParameterControl();
    resized();
    repaint();
}

void VxAudioProcessorEditor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                                       const juce::Identifier& property)
{
    if (treeWhosePropertyHasChanged == valueTreeState.state
        && property == juce::Identifier(VxAudioProcessor::activeModuleStateKey)
        && ! suppressProcessorStateResync)
        resyncEditorFromProcessorState();

    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::valueTreeRedirected(juce::ValueTree& treeWhichHasBeenChanged)
{
    if (treeWhichHasBeenChanged == valueTreeState.state)
        resyncEditorFromProcessorState();

    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::registerParameterListeners()
{
    for (int slotIndex = 0; slotIndex < VxAudioProcessor::hostAutomationSlotCount; ++slotIndex)
        valueTreeState.addParameterListener(VxAudioProcessor::getHostSlotParameterId(slotIndex), this);

    if (! shellStateListenerRegistered)
    {
        valueTreeState.state.addListener(this);
        shellStateListenerRegistered = true;
    }

    refreshModuleStateListeners();
}

void VxAudioProcessorEditor::registerObservedModuleParameterListeners(juce::AudioProcessorValueTreeState& moduleValueTreeState)
{
    auto observedListeners = ObservedModuleParameterListeners {};
    observedListeners.valueTreeState = &moduleValueTreeState;
    observedListeners.parameterIds.reserve(static_cast<size_t>(moduleValueTreeState.state.getNumChildren()));

    for (const auto parameterState : moduleValueTreeState.state)
    {
        const auto parameterId = parameterState.getProperty("id").toString();

        if (parameterId.isEmpty())
            continue;

        moduleValueTreeState.addParameterListener(parameterId, this);
        observedListeners.parameterIds.push_back(parameterId);
    }

    observedModuleParameterListeners.push_back(std::move(observedListeners));
}

void VxAudioProcessorEditor::unregisterParameterListeners()
{
    for (int slotIndex = 0; slotIndex < VxAudioProcessor::hostAutomationSlotCount; ++slotIndex)
        valueTreeState.removeParameterListener(VxAudioProcessor::getHostSlotParameterId(slotIndex), this);

    if (shellStateListenerRegistered)
    {
        valueTreeState.state.removeListener(this);
        shellStateListenerRegistered = false;
    }

    clearModuleStateListeners();
}

void VxAudioProcessorEditor::refreshModuleStateListeners()
{
    clearModuleStateListeners();

    observedModuleStates.reserve(1);
    observedModuleParameterListeners.reserve(1);

    juce::ValueTree moduleState;

    auto observeModule = [this, &moduleState] (auto* processor)
    {
        if (processor == nullptr)
            return;

        auto& moduleValueTreeState = processor->getValueTreeState();
        moduleState = moduleValueTreeState.state;
        registerObservedModuleParameterListeners(moduleValueTreeState);
    };

    switch (audioProcessor.getActiveModule())
    {
        case VxAudioProcessor::ActiveModule::eql:
            observeModule(audioProcessor.getEqlModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::fft:
            observeModule(audioProcessor.getFftModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::tls:
            observeModule(audioProcessor.getTlsModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::dyn:
            observeModule(audioProcessor.getDynModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::trs:
            observeModule(audioProcessor.getTrsModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::none:
            break;
    }

    if (moduleState.isValid())
    {
        moduleState.addListener(this);
        observedModuleStates.push_back(moduleState);
    }
}

void VxAudioProcessorEditor::clearModuleStateListeners()
{
    for (auto& observedListeners : observedModuleParameterListeners)
    {
        if (observedListeners.valueTreeState == nullptr)
            continue;

        for (const auto& parameterId : observedListeners.parameterIds)
            observedListeners.valueTreeState->removeParameterListener(parameterId, this);
    }

    observedModuleParameterListeners.clear();

    for (auto& state : observedModuleStates)
        if (state.isValid())
            state.removeListener(this);

    observedModuleStates.clear();
}

void VxAudioProcessorEditor::detachModuleEditorBindings()
{
    clearModuleStateListeners();

    for (auto& section : filterSections)
        if (section != nullptr)
            section->detach();

    fftDeltaAttachment.reset();
    fftDualMonoLinkAttachment.reset();
    fftDynamicBypassAttachment.reset();
    fftDynamicModeAttachment.reset();

    if (fftAttackControl != nullptr) fftAttackControl->detach();
    if (fftReleaseControl != nullptr) fftReleaseControl->detach();
    if (fftKneeControl != nullptr) fftKneeControl->detach();
    if (fftRatioControl != nullptr) fftRatioControl->detach();
    if (fftFloorControl != nullptr) fftFloorControl->detach();
    if (fftDspFftSizeControl != nullptr) fftDspFftSizeControl->detach();
    if (fftDspHopDivisorControl != nullptr) fftDspHopDivisorControl->detach();
    if (fftDspSlopeControl != nullptr) fftDspSlopeControl->detach();
    if (fftPhaseImpactControl != nullptr) fftPhaseImpactControl->detach();
    if (fftDualMonoLeftThresholdControl != nullptr) fftDualMonoLeftThresholdControl->detach();
    if (fftDualMonoLeftAdaptiveControl != nullptr) fftDualMonoLeftAdaptiveControl->detach();
    if (fftDualMonoRightThresholdControl != nullptr) fftDualMonoRightThresholdControl->detach();
    if (fftDualMonoRightAdaptiveControl != nullptr) fftDualMonoRightAdaptiveControl->detach();
    if (fftAdaptiveOffsetControl != nullptr) fftAdaptiveOffsetControl->detach();
    if (fftAdaptiveAttackControl != nullptr) fftAdaptiveAttackControl->detach();
    if (fftAdaptiveHoldControl != nullptr) fftAdaptiveHoldControl->detach();
    if (fftAdaptiveReleaseControl != nullptr) fftAdaptiveReleaseControl->detach();
    shell_setup_support::removeOwnedChild(fftAnalyserContent, fftAnalyserComponent);
    shell_setup_support::removeOwnedChild(*this, tlsModuleEditor);
    shell_setup_support::removeOwnedChild(*this, dynModuleEditor);
    shell_setup_support::removeOwnedChild(*this, trsModuleEditor);
}

void VxAudioProcessorEditor::scheduleHistorySnapshot()
{
    if (suppressHistorySnapshots)
        return;

    audioProcessor.notifyHostOfStateChange();
    pendingHistorySnapshot.store(true, std::memory_order_relaxed);
    lastHistoryChangeTimeMs.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
}

void VxAudioProcessorEditor::commitPendingHistorySnapshot(const bool force)
{
    if (! pendingHistorySnapshot.load(std::memory_order_relaxed) || suppressHistorySnapshots)
        return;

    constexpr uint32 snapshotDebounceMs = 300;
    const auto now = juce::Time::getMillisecondCounter();
    const auto lastChange = lastHistoryChangeTimeMs.load(std::memory_order_relaxed);

    if (! force && now - lastChange < snapshotDebounceMs)
        return;

    juce::MemoryBlock snapshot;
    audioProcessor.getStateInformationForABCompareSnapshot(snapshot);
    pendingHistorySnapshot.store(false, std::memory_order_relaxed);

    if (snapshot == committedHistorySnapshot)
    {
        updateUndoRedoButtons();
        return;
    }

    if (! committedHistorySnapshot.isEmpty())
    {
        undoHistory.push_back(committedHistorySnapshot);

        constexpr size_t maximumHistoryDepth = 128;
        if (undoHistory.size() > maximumHistoryDepth)
            undoHistory.erase(undoHistory.begin());
    }

    committedHistorySnapshot = snapshot;
    redoHistory.clear();
    updateUndoRedoButtons();
}

void VxAudioProcessorEditor::applyHistorySnapshot(const juce::MemoryBlock& snapshot)
{
    if (snapshot.isEmpty())
        return;

    auto mergedStateXml = VxAudioProcessor::getXmlFromBinary(snapshot.getData(), static_cast<int>(snapshot.getSize()));

    if (mergedStateXml == nullptr || ! mergedStateXml->hasTagName(valueTreeState.state.getType().toString()))
        return;

    preserveEditorWindowState(*mergedStateXml, valueTreeState.state);

    juce::MemoryBlock mergedSnapshot;
    VxAudioProcessor::copyXmlToBinary(*mergedStateXml, mergedSnapshot);

    struct PreservedUiState
    {
        bool hostParameters = false;
        int filterScrollY = 0;
    };

    const PreservedUiState preservedUiState
    {
        hostParametersExpanded,
        filterViewport.getViewPositionY()
    };
    auto* bypassParameter = valueTreeState.getParameter(VxAudioProcessor::paramGlobalBypassId);
    const auto preservedBypassValue = bypassParameter != nullptr ? bypassParameter->getValue() : 0.0f;

    const juce::ScopedValueSetter<bool> suppressHistory(suppressHistorySnapshots, true);
    const juce::ScopedValueSetter<bool> suppressHostSlotSync(suppressHostSlotAutomationSync, true);
    pendingHistorySnapshot.store(false, std::memory_order_relaxed);
    detachModuleEditorBindings();
    if (! audioProcessor.setStateInformationPreservingLoadedModule(mergedSnapshot.getData(),
                                                                    static_cast<int>(mergedSnapshot.getSize())))
    {
        audioProcessor.setStateInformation(mergedSnapshot.getData(), static_cast<int>(mergedSnapshot.getSize()));
    }
    if (bypassParameter != nullptr)
        bypassParameter->setValueNotifyingHost(preservedBypassValue);

    restoreEditorStateFromValueTree();
    refreshModuleTabButton();
    if (auto* eqlProcessor = getActiveEqlProcessor())
        refreshFilterPresetList(eqlProcessor->getLastFilterPresetName());
    else
        refreshFilterPresetList({});
    reloadFilterPresetFromProcessor();

    hostParametersExpanded = preservedUiState.hostParameters;

    storeEditorStateToValueTree();
    syncEditorWidthToBounds();
    updateSectionStates();
    resized();

    const auto filterMaxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
    filterViewport.setViewPosition(0, juce::jlimit(0, filterMaxOffset, preservedUiState.filterScrollY));

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
}

void VxAudioProcessorEditor::refreshEqlFilterSectionsFromProcessor()
{
    for (auto& sectionPtr : filterSections)
    {
        auto* section = sectionPtr.get();

        if (section == nullptr)
            continue;

        const auto loadedType = section->getFilterType();
        section->lastFilterType = loadedType;
        section->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(loadedType));
        section->slopeControl->setChoiceEnabled(0, loadedType != EqlModuleProcessor::FilterType::bell);
        section->updatePlaceChoicesForType(true);
        section->captureCurrentValuesForCurrentType(true);
    }
}

void VxAudioProcessorEditor::captureCurrentABState()
{
    const auto activeSlot = audioProcessor.getABCompareActiveSlot();

    juce::MemoryBlock snapshot;
    audioProcessor.getStateInformationForABCompareSnapshot(snapshot);
    audioProcessor.setABCompareSnapshot(activeSlot, snapshot);
}

void VxAudioProcessorEditor::restoreABStateSnapshot(const juce::MemoryBlock& snapshot)
{
    if (snapshot.isEmpty())
        return;

    auto stateXml = VxAudioProcessor::getXmlFromBinary(snapshot.getData(), static_cast<int>(snapshot.getSize()));

    if (stateXml == nullptr || ! stateXml->hasTagName(valueTreeState.state.getType().toString()))
        return;

    preserveEditorWindowState(*stateXml, valueTreeState.state);

    juce::MemoryBlock restoredSnapshot;
    VxAudioProcessor::copyXmlToBinary(*stateXml, restoredSnapshot);

    struct PreservedUiState
    {
        bool hostParameters = false;
        int filterScrollY = 0;
    };

    const PreservedUiState preservedUiState
    {
        hostParametersExpanded,
        filterViewport.getViewPositionY()
    };

    const juce::ScopedValueSetter<bool> suppressHistory(suppressHistorySnapshots, true);
    const juce::ScopedValueSetter<bool> suppressHostSlotSync(suppressHostSlotAutomationSync, true);
    pendingHistorySnapshot.store(false, std::memory_order_relaxed);

    {
        // Restoring root properties fires synchronous ValueTree callbacks before the
        // old processor is destroyed. Rebind only after the replacement is complete.
        const juce::ScopedValueSetter<bool> suppressResync(suppressProcessorStateResync, true);
        detachModuleEditorBindings();
        audioProcessor.applyStateInformationForABCompare(restoredSnapshot.getData(),
                                                          static_cast<int>(restoredSnapshot.getSize()));
    }

    restoreEditorStateFromValueTree();
    refreshModuleTabButton();

    if (auto* eqlProcessor = getActiveEqlProcessor())
    {
        refreshFilterPresetList(eqlProcessor->getLastFilterPresetName());
        refreshEqlFilterSectionsFromProcessor();
    }
    else
    {
        refreshFilterPresetList({});
    }

    hostParametersExpanded = preservedUiState.hostParameters;

    storeEditorStateToValueTree();
    syncEditorWidthToBounds();
    updateSectionStates();
    resized();

    const auto filterMaxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
    filterViewport.setViewPosition(0, juce::jlimit(0, filterMaxOffset, preservedUiState.filterScrollY));

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
    refreshABCompareButton();
}

void VxAudioProcessorEditor::switchABState()
{
    captureCurrentABState();

    const auto currentSlot = audioProcessor.getABCompareActiveSlot();
    const auto nextSlot = currentSlot == 0 ? 1 : 0;
    const auto currentSnapshot = audioProcessor.getABCompareSnapshot(currentSlot);

    audioProcessor.setABCompareActiveSlot(nextSlot);
    const auto nextSnapshot = audioProcessor.getABCompareSnapshot(nextSlot);

    if (nextSnapshot != currentSnapshot)
    {
        restoreABStateSnapshot(nextSnapshot);
    }
    else
    {
        updateUndoRedoButtons();
        refreshABCompareButton();
    }

    clearKeyboardFocus(*this);
}

void VxAudioProcessorEditor::copyCurrentABStateToOtherSlot()
{
    captureCurrentABState();

    const auto currentSlot = audioProcessor.getABCompareActiveSlot();
    const auto targetSlot = currentSlot == 0 ? 1 : 0;
    const auto currentSnapshot = audioProcessor.getABCompareSnapshot(currentSlot);

    if (! currentSnapshot.isEmpty())
        audioProcessor.setABCompareSnapshot(targetSlot, currentSnapshot);

    refreshABCompareButton();
    clearKeyboardFocus(*this);
}

void VxAudioProcessorEditor::refreshABCompareButton()
{
    if (abCompareButton == nullptr)
        return;

    const auto activeABSlot = audioProcessor.getABCompareActiveSlot();
    abCompareButton->setToggleState(false, juce::dontSendNotification);
    abCompareButton->setButtonText("AB");
    abCompareButton->setABCompareHighlightIndex(activeABSlot);
    abCompareButton->setTooltip(activeABSlot == 0 ? "A/B COMPARE: A" : "A/B COMPARE: B");
}

void VxAudioProcessorEditor::updateUndoRedoButtons()
{
    if (undoButton != nullptr)
    {
        const auto canUndo = ! undoHistory.empty();
        undoButton->setEnabled(canUndo);
        undoButton->setAlpha(canUndo ? 1.0f : 0.45f);
    }

    if (redoButton != nullptr)
    {
        const auto canRedo = ! redoHistory.empty();
        redoButton->setEnabled(canRedo);
        redoButton->setAlpha(canRedo ? 1.0f : 0.45f);
    }

    refreshABCompareButton();
}

void VxAudioProcessorEditor::resetFilterSectionStoredValues(const int filterIndex)
{
    if (! juce::isPositiveAndBelow(filterIndex, static_cast<int>(filterSections.size())))
        return;

    auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

    if (section == nullptr)
        return;

    for (const auto filterType : VxAudioProcessor::filterTypePresetOrder)
    {
        section->setStoredValues(filterType,
                                 defaultFilterFrequencyForType(filterType),
                                 defaultFilterBandwidth(),
                                 defaultFilterSlope(),
                                 0,
                                 false);
    }

    section->lastFilterType = section->getFilterType();
    section->expanded = false;
    section->captureCurrentValuesForCurrentType(true);
}

void VxAudioProcessorEditor::removeFilterSectionStoredValues(const int removedIndex, const int previousCount)
{
    if (previousCount <= 0)
        return;

    if (previousCount == 1)
    {
        resetFilterSectionStoredValues(0);
        return;
    }

    for (int sourceIndex = removedIndex + 1; sourceIndex < previousCount; ++sourceIndex)
        filterSections[static_cast<size_t>(sourceIndex - 1)]->copyStoredValuesFrom(*filterSections[static_cast<size_t>(sourceIndex)]);

    std::vector<int> reorderedOrder;
    reorderedOrder.reserve(static_cast<size_t>(previousCount - 1));

    for (int orderIndex = 0; orderIndex < previousCount; ++orderIndex)
    {
        const auto orderFilterIndex = filterDisplayOrder[static_cast<size_t>(orderIndex)];

        if (orderFilterIndex == removedIndex)
            continue;

        reorderedOrder.push_back(orderFilterIndex > removedIndex ? orderFilterIndex - 1
                                                               : orderFilterIndex);
    }

    for (size_t orderIndex = 0; orderIndex < reorderedOrder.size(); ++orderIndex)
        filterDisplayOrder[orderIndex] = reorderedOrder[orderIndex];

    for (int orderIndex = static_cast<int>(reorderedOrder.size()); orderIndex < previousCount; ++orderIndex)
    {
        const auto orderFilterIndex = filterDisplayOrder[static_cast<size_t>(orderIndex)];

        filterDisplayOrder[static_cast<size_t>(orderIndex)] = orderFilterIndex > removedIndex
            ? orderFilterIndex - 1
            : orderFilterIndex;
    }

    resetFilterSectionStoredValues(previousCount - 1);
    storeEditorStateToValueTree();
}
