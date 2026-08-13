#include "shell.EditorFilterSection.h"
#include "shell.ShellState.h"

namespace
{
constexpr auto editorFilterDisplayOrderStateKey = "editor_filter_display_order";
constexpr auto editorHostParametersExpandedStateKey = "editor_host_parameters_expanded";
constexpr auto editorFftAdaptiveSettingsExpandedStateKey = "editor_fft_adaptive_settings_expanded";

juce::Point<int> clampEditorSize(const int width, const int height) noexcept
{
    return { juce::jlimit(minimumEditorWidth, maximumEditorWidth, width),
             juce::jlimit(minimumEditorHeight, maximumEditorHeight, height) };
}

juce::String encodeFilterDisplayOrder(const std::vector<int>& filterDisplayOrder, const int activeCount)
{
    juce::StringArray values;
    values.ensureStorageAllocated(juce::jmax(0, activeCount));

    for (int orderIndex = 0; orderIndex < activeCount; ++orderIndex)
    {
        if (! juce::isPositiveAndBelow(orderIndex, static_cast<int>(filterDisplayOrder.size())))
            break;

        values.add(juce::String(filterDisplayOrder[static_cast<size_t>(orderIndex)]));
    }

    return values.joinIntoString(",");
}

std::vector<int> decodeFilterDisplayOrder(const juce::String& text, const int activeCount)
{
    std::vector<int> order;
    order.reserve(static_cast<size_t>(juce::jmax(0, activeCount)));

    std::array<bool, VxAudioProcessor::maxEqlFilterCount> used {};
    const auto tokens = juce::StringArray::fromTokens(text, ",", "");

    for (const auto& token : tokens)
    {
        if (static_cast<int>(order.size()) >= activeCount)
            break;

        const auto trimmed = token.trim();

        if (! trimmed.containsOnly("-0123456789"))
            continue;

        const auto filterIndex = trimmed.getIntValue();

        if (! juce::isPositiveAndBelow(filterIndex, activeCount))
            continue;

        if (used[static_cast<size_t>(filterIndex)])
            continue;

        used[static_cast<size_t>(filterIndex)] = true;
        order.push_back(filterIndex);
    }

    for (int filterIndex = 0; filterIndex < activeCount; ++filterIndex)
    {
        if (! used[static_cast<size_t>(filterIndex)])
            order.push_back(filterIndex);
    }

    return order;
}

juce::String makeHostSlotParameterIdStateKey(const int slotIndex)
{
    return "editor_host_slot_param_" + juce::String::formatted("%02d", slotIndex + 1);
}

juce::String makeHostSlotNameStateKey(const int slotIndex)
{
    return "editor_host_slot_name_" + juce::String::formatted("%02d", slotIndex + 1);
}

}


EqlModuleProcessor* VxAudioProcessorEditor::getActiveEqlProcessor() noexcept
{
    if (audioProcessor.getActiveModule() != VxAudioProcessor::ActiveModule::eql)
        return nullptr;

    return audioProcessor.getEqlModuleProcessor();
}

const EqlModuleProcessor* VxAudioProcessorEditor::getActiveEqlProcessor() const noexcept
{
    if (audioProcessor.getActiveModule() != VxAudioProcessor::ActiveModule::eql)
        return nullptr;

    return audioProcessor.getEqlModuleProcessor();
}


void VxAudioProcessorEditor::clearAllFilters()
{
    auto* eqlProcessor = getActiveEqlProcessor();

    if (eqlProcessor == nullptr || ! eqlProcessor->clearFilters())
        return;

    filterDisplayOrder.clear();
    filterDisplayOrder.reserve(VxAudioProcessor::maxEqlFilterCount);

    for (int filterIndex = 0; filterIndex < VxAudioProcessor::maxEqlFilterCount; ++filterIndex)
    {
        filterDisplayOrder.push_back(filterIndex);
        resetFilterSectionStoredValues(filterIndex);
    }

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::performUndo()
{
    commitPendingHistorySnapshot(true);

    if (undoHistory.empty())
        return;

    redoHistory.push_back(committedHistorySnapshot);
    const auto snapshot = undoHistory.back();
    undoHistory.pop_back();
    applyHistorySnapshot(snapshot);
}

void VxAudioProcessorEditor::performRedo()
{
    commitPendingHistorySnapshot(true);

    if (redoHistory.empty())
        return;

    undoHistory.push_back(committedHistorySnapshot);
    const auto snapshot = redoHistory.back();
    redoHistory.pop_back();
    applyHistorySnapshot(snapshot);
}

void VxAudioProcessorEditor::restoreEditorStateFromValueTree()
{
    const auto activeCount = getActiveFilterCount();
    auto& state = valueTreeState.state;

    setLoadedModuleFlags(audioProcessor.getActiveModule());

    hostParametersExpanded = static_cast<bool>(state.getProperty(editorHostParametersExpandedStateKey, false));
    fftAdaptiveSettingsExpanded = static_cast<bool>(state.getProperty(editorFftAdaptiveSettingsExpandedStateKey, false));

    const auto savedOrder = state.getProperty(editorFilterDisplayOrderStateKey).toString().trim();

    if (savedOrder.isNotEmpty())
        filterDisplayOrder = decodeFilterDisplayOrder(savedOrder, activeCount);
    else
    {
        filterDisplayOrder.clear();
        filterDisplayOrder.reserve(VxAudioProcessor::maxEqlFilterCount);

        for (int filterIndex = 0; filterIndex < VxAudioProcessor::maxEqlFilterCount; ++filterIndex)
            filterDisplayOrder.push_back(filterIndex);
    }

    if (static_cast<int>(filterDisplayOrder.size()) < VxAudioProcessor::maxEqlFilterCount)
    {
        const auto previousSize = static_cast<int>(filterDisplayOrder.size());
        filterDisplayOrder.reserve(VxAudioProcessor::maxEqlFilterCount);

        for (int filterIndex = previousSize; filterIndex < VxAudioProcessor::maxEqlFilterCount; ++filterIndex)
            filterDisplayOrder.push_back(filterIndex);
    }

    for (int filterIndex = 0; filterIndex < VxAudioProcessor::maxEqlFilterCount; ++filterIndex)
    {
        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            continue;

        section->expanded = false;
    }

    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
    {
        auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];
        assignment.parameterId = state.getProperty(makeHostSlotParameterIdStateKey(slotIndex)).toString().trim();
        assignment.parameterName = state.getProperty(makeHostSlotNameStateKey(slotIndex)).toString().trim();
    }

    refreshHostSlotButtons();

    rebindActiveModuleEditors();
}

void VxAudioProcessorEditor::setLoadedModuleFlags(const VxAudioProcessor::ActiveModule activeModule) noexcept
{
    eqlModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::eql;
    fftModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::fft;
    tlsModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::tls;
    dynModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::dyn;
    trsModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::trs;
}

void VxAudioProcessorEditor::storeEditorStateToValueTree() noexcept
{
    auto& state = valueTreeState.state;
    const auto activeModuleId = juce::String(VxAudioProcessor::stateIdForModule(audioProcessor.getActiveModule()));

    if (activeModuleId.isNotEmpty())
        state.setProperty(VxAudioProcessor::activeModuleStateKey, activeModuleId, nullptr);
    else
        state.removeProperty(VxAudioProcessor::activeModuleStateKey, nullptr);

    state.setProperty(editorHostParametersExpandedStateKey, hostParametersExpanded, nullptr);
    state.setProperty(editorFftAdaptiveSettingsExpandedStateKey, fftAdaptiveSettingsExpanded, nullptr);
    state.setProperty(editorFilterDisplayOrderStateKey,
                      encodeFilterDisplayOrder(filterDisplayOrder, getActiveFilterCount()),
                      nullptr);

    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
    {
        const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

        if (assignment.parameterId.isEmpty())
        {
            state.removeProperty(makeHostSlotParameterIdStateKey(slotIndex), nullptr);
            state.removeProperty(makeHostSlotNameStateKey(slotIndex), nullptr);
            continue;
        }

        state.setProperty(makeHostSlotParameterIdStateKey(slotIndex), assignment.parameterId, nullptr);
        state.setProperty(makeHostSlotNameStateKey(slotIndex), assignment.parameterName, nullptr);
    }

    if (! suppressEditorSizeStateSave && getWidth() > 0 && getHeight() > 0)
    {
        const auto size = clampEditorSize(getWidth(), getHeight());
        audioProcessor.setLastEditorSize(size.x, size.y);
        state.setProperty(VxAudioProcessor::editorWidthStateKey, size.x, nullptr);
        state.setProperty(VxAudioProcessor::editorHeightStateKey, size.y, nullptr);
    }
}

juce::Point<int> VxAudioProcessorEditor::getRestoredEditorSize() const noexcept
{
    const auto& state = valueTreeState.state;

    if (state.hasProperty(VxAudioProcessor::editorWidthStateKey)
        && state.hasProperty(VxAudioProcessor::editorHeightStateKey))
    {
        return clampEditorSize(static_cast<int>(state.getProperty(VxAudioProcessor::editorWidthStateKey, initialEditorWidth)),
                               static_cast<int>(state.getProperty(VxAudioProcessor::editorHeightStateKey, initialEditorHeight)));
    }

    const auto lastSize = audioProcessor.getLastEditorSize();

    if (lastSize.x > 0 && lastSize.y > 0)
        return clampEditorSize(lastSize.x, lastSize.y);

    return { initialEditorWidth, initialEditorHeight };
}

int VxAudioProcessorEditor::getActiveFilterCount() const noexcept
{
    if (const auto* eqlProcessor = getActiveEqlProcessor())
        return eqlProcessor->getActiveFilterCount();

    return 0;
}

void VxAudioProcessorEditor::syncEditorWidthToBounds()
{
    const auto restoredWidth = juce::jmax(minimumEditorWidth, getWidth());
    setResizeLimits(minimumEditorWidth,
                    minimumEditorHeight,
                    maximumEditorWidth,
                    maximumEditorHeight);

    if (restoredWidth != getWidth())
        setSize(restoredWidth, getHeight());
}
