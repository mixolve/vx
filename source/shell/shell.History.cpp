#include "shell.EditorBellSection.h"
#include "shell.ShellState.h"
#include "shell.SetupSupport.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"

void VxAudioProcessorEditor::syncHostSlotAssignmentValue(const int slotIndex, const float normalizedValue)
{
    if (! juce::isPositiveAndBelow(slotIndex, static_cast<int>(hostSlotAssignments.size())))
        return;

    const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

    if (assignment.parameterId.isEmpty())
        return;

    if (auto* assignedParameter = valueTreeState.getParameter(assignment.parameterId);
        assignedParameter != nullptr)
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

            auto* assignedParameter = valueTreeState.getParameter(parameterID);
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

void VxAudioProcessorEditor::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&)
{
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::registerParameterListeners()
{
    for (int slotIndex = 0; slotIndex < VxAudioProcessor::hostAutomationSlotCount; ++slotIndex)
        valueTreeState.addParameterListener(VxAudioProcessor::getHostSlotParameterId(slotIndex), this);

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

    clearModuleStateListeners();
}

void VxAudioProcessorEditor::refreshModuleStateListeners()
{
    clearModuleStateListeners();

    observedModuleStates.reserve(static_cast<size_t>(audioProcessor.getLoadedModuleCount()) * 2);
    observedModuleParameterListeners.reserve(static_cast<size_t>(audioProcessor.getLoadedModuleCount()) * 2);

    for (int slotIndex = 0; slotIndex < audioProcessor.getLoadedModuleCount(); ++slotIndex)
    {
        const auto slot = audioProcessor.getLoadedModuleSlotAtPosition(slotIndex);
        juce::ValueTree moduleState;

        auto observeModule = [this, &moduleState] (auto* processor)
        {
            if (processor == nullptr)
                return;

            auto& moduleValueTreeState = processor->getValueTreeState();
            moduleState = moduleValueTreeState.state;
            registerObservedModuleParameterListeners(moduleValueTreeState);
        };

        switch (slot.module)
        {
            case VxAudioProcessor::ActiveModule::eqe:
                observeModule(audioProcessor.getEqeModuleProcessor(slot.instanceIndex));
                break;

            case VxAudioProcessor::ActiveModule::spe:
                if (auto* processor = audioProcessor.getSpeModuleProcessor(slot.instanceIndex))
                {
                    auto& moduleValueTreeState = processor->getValueTreeState();
                    moduleState = moduleValueTreeState.state;
                    registerObservedModuleParameterListeners(moduleValueTreeState);
                }
                break;

            case VxAudioProcessor::ActiveModule::mie:
                observeModule(audioProcessor.getMieModuleProcessor(slot.instanceIndex));
                break;

            case VxAudioProcessor::ActiveModule::mxe:
                observeModule(audioProcessor.getMxeModuleProcessor(slot.instanceIndex));
                break;

            case VxAudioProcessor::ActiveModule::tse:
                observeModule(audioProcessor.getTseModuleProcessor(slot.instanceIndex));
                break;

            case VxAudioProcessor::ActiveModule::none:
                break;
        }

        if (! moduleState.isValid())
            continue;

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

    for (auto& section : bellSections)
        if (section != nullptr)
            section->detach();

    speDeltaAttachment.reset();
    speDualMonoLinkAttachment.reset();

    if (speAttackControl != nullptr) speAttackControl->detach();
    if (speReleaseControl != nullptr) speReleaseControl->detach();
    if (speKneeControl != nullptr) speKneeControl->detach();
    if (speRatioControl != nullptr) speRatioControl->detach();
    if (speDspFftSizeControl != nullptr) speDspFftSizeControl->detach();
    if (speDspSlopeControl != nullptr) speDspSlopeControl->detach();
    if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->detach();
    if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->detach();
    if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->detach();
    if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->detach();
    if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->detach();
    if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->detach();

    shell_setup_support::removeOwnedChild(*this, speAnalyserComponent);
    shell_setup_support::removeOwnedChild(*this, mieModuleEditor);
    shell_setup_support::removeOwnedChild(*this, mxeModuleEditor);
    shell_setup_support::removeOwnedChild(*this, tseModuleEditor);
}

void VxAudioProcessorEditor::scheduleHistorySnapshot()
{
    if (suppressHistorySnapshots)
        return;

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

    preserveEditorWindowAndVisualizerState(*mergedStateXml, valueTreeState.state);

    juce::MemoryBlock mergedSnapshot;
    VxAudioProcessor::copyXmlToBinary(*mergedStateXml, mergedSnapshot);

    struct PreservedUiState
    {
        bool shellGlobalHost = false;
        bool visualizer = false;
        int filterScrollY = 0;
    };

    const PreservedUiState preservedUiState
    {
        shellGlobalHostExpanded,
        visualizerExpanded,
        filterViewport.getViewPositionY()
    };
    auto* bypassParameter = valueTreeState.getParameter(VxAudioProcessor::paramGlobalBypassId);
    const auto preservedBypassValue = bypassParameter != nullptr ? bypassParameter->getValue() : 0.0f;

    const juce::ScopedValueSetter<bool> suppressHistory(suppressHistorySnapshots, true);
    pendingHistorySnapshot.store(false, std::memory_order_relaxed);
    detachModuleEditorBindings();
    audioProcessor.setStateInformation(mergedSnapshot.getData(), static_cast<int>(mergedSnapshot.getSize()));
    if (bypassParameter != nullptr)
        bypassParameter->setValueNotifyingHost(preservedBypassValue);

    restoreEditorStateFromValueTree();
    rebuildModuleTabRows();
    refreshFilterPresetList(getActiveEqeProcessor() != nullptr ? getActiveEqeProcessor()->getLastFilterPresetName()
                                                               : juce::String {});
    reloadFilterPresetFromProcessor();

    shellGlobalHostExpanded = preservedUiState.shellGlobalHost;

    visualizerExpanded = preservedUiState.visualizer;

    if (eqeModuleLoaded)
    {
        visualizerExpanded = false;
    }

    storeEditorStateToValueTree();
    updateEditorWidthForVisualizerVisibility();
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

void VxAudioProcessorEditor::resetBellSectionStoredValues(const int bellIndex)
{
    if (! juce::isPositiveAndBelow(bellIndex, static_cast<int>(bellSections.size())))
        return;

    auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

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
    section->captureCurrentValuesForCurrentType(true);
}

void VxAudioProcessorEditor::removeBellSectionStoredValues(const int removedIndex, const int previousCount)
{
    if (previousCount <= 0)
        return;

    if (previousCount == 1)
    {
        resetBellSectionStoredValues(0);
        return;
    }

    for (int sourceIndex = removedIndex + 1; sourceIndex < previousCount; ++sourceIndex)
        bellSections[static_cast<size_t>(sourceIndex - 1)]->copyStoredValuesFrom(*bellSections[static_cast<size_t>(sourceIndex)]);

    std::vector<int> reorderedOrder;
    reorderedOrder.reserve(static_cast<size_t>(previousCount - 1));

    for (int orderIndex = 0; orderIndex < previousCount; ++orderIndex)
    {
        const auto orderBellIndex = bellDisplayOrder[static_cast<size_t>(orderIndex)];

        if (orderBellIndex == removedIndex)
            continue;

        reorderedOrder.push_back(orderBellIndex > removedIndex ? orderBellIndex - 1
                                                               : orderBellIndex);
    }

    for (size_t orderIndex = 0; orderIndex < reorderedOrder.size(); ++orderIndex)
        bellDisplayOrder[orderIndex] = reorderedOrder[orderIndex];

    for (int orderIndex = static_cast<int>(reorderedOrder.size()); orderIndex < previousCount; ++orderIndex)
    {
        const auto orderBellIndex = bellDisplayOrder[static_cast<size_t>(orderIndex)];

        bellDisplayOrder[static_cast<size_t>(orderIndex)] = orderBellIndex > removedIndex
            ? orderBellIndex - 1
            : orderBellIndex;
    }

    resetBellSectionStoredValues(previousCount - 1);
    storeEditorStateToValueTree();
}
