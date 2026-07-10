#include "shell.EditorFilterSection.h"
#include "shell.ShellState.h"
#include "shell.SetupSupport.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"

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
        case VxAudioProcessor::ActiveModule::eqe: return findInModule(audioProcessor.getEqeModuleProcessor());
        case VxAudioProcessor::ActiveModule::spe: return findInModule(audioProcessor.getSpeModuleProcessor());
        case VxAudioProcessor::ActiveModule::mie: return findInModule(audioProcessor.getMieModuleProcessor());
        case VxAudioProcessor::ActiveModule::mxe: return findInModule(audioProcessor.getMxeModuleProcessor());
        case VxAudioProcessor::ActiveModule::tse: return findInModule(audioProcessor.getTseModuleProcessor());
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
        && property == juce::Identifier(VxAudioProcessor::activeModuleStateKey))
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
        case VxAudioProcessor::ActiveModule::eqe:
            observeModule(audioProcessor.getEqeModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::spe:
            observeModule(audioProcessor.getSpeModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::mie:
            observeModule(audioProcessor.getMieModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::mxe:
            observeModule(audioProcessor.getMxeModuleProcessor());
            break;

        case VxAudioProcessor::ActiveModule::tse:
            observeModule(audioProcessor.getTseModuleProcessor());
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

    speDeltaAttachment.reset();
    speDualMonoLinkAttachment.reset();
    for (auto& attachment : spePhaseBypassAttachments)
        attachment.reset();
    for (auto& attachment : speAmplitudeBypassAttachments)
        attachment.reset();

    if (speAttackControl != nullptr) speAttackControl->detach();
    if (speReleaseControl != nullptr) speReleaseControl->detach();
    if (speKneeControl != nullptr) speKneeControl->detach();
    if (speRatioControl != nullptr) speRatioControl->detach();
    if (speDspFftSizeControl != nullptr) speDspFftSizeControl->detach();
    if (speDspHopDivisorControl != nullptr) speDspHopDivisorControl->detach();
    if (speDspSlopeControl != nullptr) speDspSlopeControl->detach();
    if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->detach();
    if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->detach();
    if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->detach();
    if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->detach();
    if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->detach();
    if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->detach();
    for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
    {
        if (spePhaseTypeControls[static_cast<size_t>(filterIndex)] != nullptr)
            spePhaseTypeControls[static_cast<size_t>(filterIndex)]->detach();
        if (spePhasePlaceControls[static_cast<size_t>(filterIndex)] != nullptr)
            spePhasePlaceControls[static_cast<size_t>(filterIndex)]->detach();
        if (spePhaseSlopeControls[static_cast<size_t>(filterIndex)] != nullptr)
            spePhaseSlopeControls[static_cast<size_t>(filterIndex)]->detach();
        if (spePhaseFrequencyControls[static_cast<size_t>(filterIndex)] != nullptr)
            spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->detach();
        if (spePhaseBandwidthControls[static_cast<size_t>(filterIndex)] != nullptr)
            spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]->detach();
        if (spePhaseImpactControls[static_cast<size_t>(filterIndex)] != nullptr)
            spePhaseImpactControls[static_cast<size_t>(filterIndex)]->detach();
        if (speAmplitudeTypeControls[static_cast<size_t>(filterIndex)] != nullptr)
            speAmplitudeTypeControls[static_cast<size_t>(filterIndex)]->detach();
        if (speAmplitudePlaceControls[static_cast<size_t>(filterIndex)] != nullptr)
            speAmplitudePlaceControls[static_cast<size_t>(filterIndex)]->detach();
        if (speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)] != nullptr)
            speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)]->detach();
        if (speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)] != nullptr)
            speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)]->detach();
        if (speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)] != nullptr)
            speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)]->detach();
        if (speAmplitudeImpactControls[static_cast<size_t>(filterIndex)] != nullptr)
            speAmplitudeImpactControls[static_cast<size_t>(filterIndex)]->detach();
    }

    shell_setup_support::removeOwnedChild(speAnalyserContent, speAnalyserComponent);
    shell_setup_support::removeOwnedChild(*this, mieModuleEditor);
    shell_setup_support::removeOwnedChild(*this, mxeModuleEditor);
    shell_setup_support::removeOwnedChild(*this, tseModuleEditor);
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
    audioProcessor.getStateInformation(snapshot);
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
    refreshFilterPresetList(getActiveEqeProcessor() != nullptr ? getActiveEqeProcessor()->getLastFilterPresetName()
                                                               : juce::String {});
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
                                 defaultFilterBandwidthForType(filterType),
                                 defaultFilterSlopeForType(filterType),
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
