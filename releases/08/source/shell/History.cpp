#include "EditorFilterSection.h"
#include "EditorState.h"
#include "../modules/eql/ProcessorSupport.h"

void AvaAudioProcessorEditor::scheduleHistorySnapshot()
{
    if (suppressHistorySnapshots)
        return;

    audioProcessor.notifyHostOfStateChange();
    pendingHistorySnapshot.store(true, std::memory_order_relaxed);
    lastHistoryChangeTimeMs.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
}

void AvaAudioProcessorEditor::commitPendingHistorySnapshot(const bool force)
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

void AvaAudioProcessorEditor::applyHistorySnapshot(const juce::MemoryBlock& snapshot)
{
    if (snapshot.isEmpty())
        return;

    auto mergedStateXml = AvaAudioProcessor::getXmlFromBinary(snapshot.getData(), static_cast<int>(snapshot.getSize()));

    if (mergedStateXml == nullptr || ! mergedStateXml->hasTagName(valueTreeState.state.getType().toString()))
        return;

    preserveEditorWindowState(*mergedStateXml, valueTreeState.state);

    juce::MemoryBlock mergedSnapshot;
    AvaAudioProcessor::copyXmlToBinary(*mergedStateXml, mergedSnapshot);

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
    auto* bypassParameter = valueTreeState.getParameter(AvaAudioProcessor::paramGlobalBypassId);
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
    ensureModuleTitle();
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

void AvaAudioProcessorEditor::refreshEqlFilterSectionsFromProcessor()
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

void AvaAudioProcessorEditor::updateUndoRedoButtons()
{
    if (undoButton != nullptr)
    {
        const auto canUndo = ! undoHistory.empty();
        undoButton->setEnabled(canUndo);
        undoButton->setAlpha(1.0f);
    }

    if (redoButton != nullptr)
    {
        const auto canRedo = ! redoHistory.empty();
        redoButton->setEnabled(canRedo);
        redoButton->setAlpha(1.0f);
    }

    refreshABCompareButton();
}

void AvaAudioProcessorEditor::resetFilterSectionStoredValues(const int filterIndex)
{
    if (! juce::isPositiveAndBelow(filterIndex, static_cast<int>(filterSections.size())))
        return;

    auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

    if (section == nullptr)
        return;

    for (const auto filterType : AvaAudioProcessor::filterTypePresetOrder)
    {
        section->setStoredValues(filterType,
                                 defaultFilterFrequency(),
                                 defaultFilterBandwidth(),
                                 defaultFilterSlope(),
                                 0,
                                 false);
    }

    section->lastFilterType = section->getFilterType();
    section->expanded = false;
    section->captureCurrentValuesForCurrentType(true);
}

void AvaAudioProcessorEditor::removeFilterSectionStoredValues(const int removedIndex, const int previousCount)
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
