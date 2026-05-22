#include "shell.EditorBellSection.h"
#include "shell.ShellState.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"

void VxAudioProcessorEditor::parameterChanged(const juce::String&, float)
{
    if (suppressHistorySnapshots)
        return;

    pendingHistorySnapshot.store(true, std::memory_order_relaxed);
    lastHistoryChangeTimeMs.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
}

void VxAudioProcessorEditor::registerParameterListeners()
{
    valueTreeState.addParameterListener(VxAudioProcessor::paramGlobalGainLId, this);
    valueTreeState.addParameterListener(VxAudioProcessor::paramGlobalGainRId, this);
    valueTreeState.addParameterListener(VxAudioProcessor::paramGlobalWideId, this);
    valueTreeState.addParameterListener(VxAudioProcessor::paramOutGainId, this);
    valueTreeState.addParameterListener(VxAudioProcessor::paramGlobalBypassId, this);
    valueTreeState.addParameterListener(VxAudioProcessor::paramGlobalBypassOutGainOnlyId, this);
    registerActiveEqeParameterListeners();
}

void VxAudioProcessorEditor::registerActiveEqeParameterListeners()
{
    unregisterActiveEqeParameterListeners();

    auto* eqeProcessor = audioProcessor.getActiveEqeModuleProcessor();

    if (eqeProcessor == nullptr)
        return;

    activeEqeValueTreeState = &eqeProcessor->getValueTreeState();
    activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::paramBypassId, this);
    activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::paramBypassWithGainId, this);

    for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getFilterTypeParamId(bellIndex), this);
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getFilterTtssParamId(bellIndex), this);
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getFilterLrmsParamId(bellIndex), this);
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getBellFrequencyParamId(bellIndex), this);
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getBellBandwidthParamId(bellIndex), this);
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getBellSlopeParamId(bellIndex), this);
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getBellGainParamId(bellIndex), this);
        activeEqeValueTreeState->addParameterListener(EqeModuleProcessor::getBellBypassParamId(bellIndex), this);
    }
}

void VxAudioProcessorEditor::unregisterParameterListeners()
{
    valueTreeState.removeParameterListener(VxAudioProcessor::paramGlobalGainLId, this);
    valueTreeState.removeParameterListener(VxAudioProcessor::paramGlobalGainRId, this);
    valueTreeState.removeParameterListener(VxAudioProcessor::paramGlobalWideId, this);
    valueTreeState.removeParameterListener(VxAudioProcessor::paramOutGainId, this);
    valueTreeState.removeParameterListener(VxAudioProcessor::paramGlobalBypassId, this);
    valueTreeState.removeParameterListener(VxAudioProcessor::paramGlobalBypassOutGainOnlyId, this);
    unregisterActiveEqeParameterListeners();
}

void VxAudioProcessorEditor::unregisterActiveEqeParameterListeners()
{
    if (activeEqeValueTreeState == nullptr)
        return;

    activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::paramBypassId, this);
    activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::paramBypassWithGainId, this);

    for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getFilterTypeParamId(bellIndex), this);
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getFilterTtssParamId(bellIndex), this);
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getFilterLrmsParamId(bellIndex), this);
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getBellFrequencyParamId(bellIndex), this);
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getBellBandwidthParamId(bellIndex), this);
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getBellSlopeParamId(bellIndex), this);
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getBellGainParamId(bellIndex), this);
        activeEqeValueTreeState->removeParameterListener(EqeModuleProcessor::getBellBypassParamId(bellIndex), this);
    }

    activeEqeValueTreeState = nullptr;
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
        bool shellGlobal = false;
        bool global = false;
        bool speMain = false;
        bool filters = false;
        bool presets = false;
        bool visualizer = false;
        int expandedBell = -1;
        int globalScrollY = 0;
        int filterScrollY = 0;
    };

    const PreservedUiState preservedUiState
    {
        shellGlobalExpanded,
        globalExpanded,
        speMainExpanded,
        filtersExpanded,
        presetsExpanded,
        visualizerExpanded,
        expandedBellIndex,
        globalViewport.getViewPositionY(),
        filterViewport.getViewPositionY()
    };

    const juce::ScopedValueSetter<bool> suppressHistory(suppressHistorySnapshots, true);
    pendingHistorySnapshot.store(false, std::memory_order_relaxed);
    audioProcessor.setStateInformation(mergedSnapshot.getData(), static_cast<int>(mergedSnapshot.getSize()));
    refreshFilterPresetList(getActiveEqeProcessor() != nullptr ? getActiveEqeProcessor()->getLastFilterPresetName()
                                                               : juce::String {});
    reloadFilterPresetFromProcessor();

    shellGlobalExpanded = preservedUiState.shellGlobal;
    globalExpanded = preservedUiState.global;
    speMainExpanded = preservedUiState.speMain;
    filtersExpanded = preservedUiState.filters;
    presetsExpanded = preservedUiState.presets;
    visualizerExpanded = preservedUiState.visualizer;
    expandedBellIndex = preservedUiState.expandedBell;

    if (! filtersExpanded || ! juce::isPositiveAndBelow(expandedBellIndex, getActiveBellCount()))
        expandedBellIndex = -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();

    const auto globalMaxOffset = juce::jmax(0, getActiveGlobalContentHeight() - globalViewport.getHeight());
    globalViewport.setViewPosition(0, juce::jlimit(0, globalMaxOffset, preservedUiState.globalScrollY));

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
