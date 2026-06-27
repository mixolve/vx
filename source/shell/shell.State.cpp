#include "shell.EditorBellSection.h"
#include "shell.ShellState.h"

namespace
{
constexpr auto editorBellDisplayOrderStateKey = "editor_bell_display_order";
constexpr auto editorShellGlobalHostExpandedStateKey = "editor_shell_global_host_expanded";
constexpr auto editorVisualizerExpandedStateKey = "editor_visualizer_expanded";
constexpr auto editorVisualizerRangeLowStateKey = "editor_visualizer_range_low";
constexpr auto editorVisualizerRangeHighStateKey = "editor_visualizer_range_high";
constexpr auto editorVisualizerCursorEnabledStateKey = "editor_visualizer_cursor_enabled";
constexpr auto editorVisualizerShowStereoStateKey = "editor_visualizer_show_stereo";
constexpr auto editorVisualizerShowLeftStateKey = "editor_visualizer_show_left";
constexpr auto editorVisualizerShowRightStateKey = "editor_visualizer_show_right";
constexpr auto editorVisualizerShowMidStateKey = "editor_visualizer_show_mid";
constexpr auto editorVisualizerShowSideStateKey = "editor_visualizer_show_side";
constexpr auto editorLastCollapsedWidthStateKey = "editor_last_collapsed_width";

juce::Point<int> clampEditorSize(const int width, const int height) noexcept
{
    return { juce::jlimit(minimumEditorWidth, maximumEditorWidth, width),
             juce::jlimit(minimumEditorHeight, maximumEditorHeight, height) };
}

int clampCollapsedEditorWidth(const int width) noexcept
{
    return juce::jlimit(minimumEditorWidth,
                        juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                        width);
}

juce::String encodeBellDisplayOrder(const std::vector<int>& bellDisplayOrder, const int activeCount)
{
    juce::StringArray values;
    values.ensureStorageAllocated(juce::jmax(0, activeCount));

    for (int orderIndex = 0; orderIndex < activeCount; ++orderIndex)
    {
        if (! juce::isPositiveAndBelow(orderIndex, static_cast<int>(bellDisplayOrder.size())))
            break;

        values.add(juce::String(bellDisplayOrder[static_cast<size_t>(orderIndex)]));
    }

    return values.joinIntoString(",");
}

std::vector<int> decodeBellDisplayOrder(const juce::String& text, const int activeCount)
{
    std::vector<int> order;
    order.reserve(static_cast<size_t>(juce::jmax(0, activeCount)));

    std::array<bool, VxAudioProcessor::maxBellFilterCount> used {};
    const auto tokens = juce::StringArray::fromTokens(text, ",", "");

    for (const auto& token : tokens)
    {
        if (static_cast<int>(order.size()) >= activeCount)
            break;

        const auto trimmed = token.trim();

        if (! trimmed.containsOnly("-0123456789"))
            continue;

        const auto bellIndex = trimmed.getIntValue();

        if (! juce::isPositiveAndBelow(bellIndex, activeCount))
            continue;

        if (used[static_cast<size_t>(bellIndex)])
            continue;

        used[static_cast<size_t>(bellIndex)] = true;
        order.push_back(bellIndex);
    }

    for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
    {
        if (! used[static_cast<size_t>(bellIndex)])
            order.push_back(bellIndex);
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


EqeModuleProcessor* VxAudioProcessorEditor::getActiveEqeProcessor() noexcept
{
    return audioProcessor.getActiveEqeModuleProcessor();
}

const EqeModuleProcessor* VxAudioProcessorEditor::getActiveEqeProcessor() const noexcept
{
    return audioProcessor.getActiveEqeModuleProcessor();
}


void VxAudioProcessorEditor::openVisualizerSection()
{
    if (! speModuleLoaded)
        return;

    const auto preservedFilterScrollY = filterViewport.getViewPositionY();
    const auto clickedExpanded = visualizerExpanded;
    visualizerExpanded = ! clickedExpanded;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();

    const auto maxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
    filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, preservedFilterScrollY));
}

void VxAudioProcessorEditor::clearAllFilters()
{
    auto* eqeProcessor = getActiveEqeProcessor();

    if (eqeProcessor == nullptr || ! eqeProcessor->clearBellFilters())
        return;

    bellDisplayOrder.clear();
    bellDisplayOrder.reserve(VxAudioProcessor::maxBellFilterCount);

    for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        bellDisplayOrder.push_back(bellIndex);
        resetBellSectionStoredValues(bellIndex);
    }

    visualizerExpanded = false;

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
    const auto activeCount = getActiveBellCount();
    auto& state = valueTreeState.state;

    const auto activeModule = audioProcessor.getActiveModule();
    eqeModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::eqe;
    speModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::spe;
    mieModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::mie;
    mxeModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::mxe;
    tseModuleLoaded = activeModule == VxAudioProcessor::ActiveModule::tse;

    if (! eqeModuleLoaded && ! speModuleLoaded && ! mieModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
    {
        if (audioProcessor.isEqeModuleLoaded())
            eqeModuleLoaded = true;
        else if (audioProcessor.isSpeModuleLoaded())
            speModuleLoaded = true;
        else if (audioProcessor.isMieModuleLoaded())
            mieModuleLoaded = true;
        else if (audioProcessor.isMxeModuleLoaded())
            mxeModuleLoaded = true;
        else if (audioProcessor.isTseModuleLoaded())
            tseModuleLoaded = true;
    }

    shellGlobalHostExpanded = static_cast<bool>(state.getProperty(editorShellGlobalHostExpandedStateKey, false));

    visualizerExpanded = state.getProperty(editorVisualizerExpandedStateKey, false);
    visualizerCursorEnabled = state.getProperty(editorVisualizerCursorEnabledStateKey, true);
    visualizerShowStereo = state.getProperty(editorVisualizerShowStereoStateKey, true);
    visualizerShowLeft = state.getProperty(editorVisualizerShowLeftStateKey, true);
    visualizerShowRight = state.getProperty(editorVisualizerShowRightStateKey, true);
    visualizerShowMid = state.getProperty(editorVisualizerShowMidStateKey, true);
    visualizerShowSide = state.getProperty(editorVisualizerShowSideStateKey, true);

    if (! speModuleLoaded)
    {
        visualizerExpanded = false;
    }

    visualizerRangeLowDb = static_cast<float>(state.getProperty(editorVisualizerRangeLowStateKey, -24.0f));
    visualizerRangeHighDb = static_cast<float>(state.getProperty(editorVisualizerRangeHighStateKey, 24.0f));

    lastCollapsedEditorWidth = clampCollapsedEditorWidth(static_cast<int>(state.getProperty(editorLastCollapsedWidthStateKey,
                                                                                            minimumEditorWidth)));

    if (eqeModuleLoaded)
    {
        visualizerExpanded = false;
    }

    if (visualizerRangeHighDb < visualizerRangeLowDb + 6.0f)
        visualizerRangeHighDb = visualizerRangeLowDb + 6.0f;

    if (visualizerRangeLowControl != nullptr && visualizerRangeHighControl != nullptr)
    {
        const juce::ScopedValueSetter<bool> scopedIgnore(suppressVisualizerControlChangeHandlers, true);
        visualizerRangeLowControl->setValue(visualizerRangeLowDb, false);
        visualizerRangeHighControl->setValue(visualizerRangeHighDb, false);
    }

    if (visualizerCursorButton != nullptr)
    {
        visualizerCursorButton->setToggleState(visualizerCursorEnabled, juce::dontSendNotification);
        visualizerCursorButton->setButtonText(visualizerCursorEnabled ? "CURSOR-OFF" : "CURSOR-ON");
    }

    if (visualizerShowStereoButton != nullptr)
        visualizerShowStereoButton->setToggleState(visualizerShowStereo, juce::dontSendNotification);

    if (visualizerShowLeftButton != nullptr)
        visualizerShowLeftButton->setToggleState(visualizerShowLeft, juce::dontSendNotification);

    if (visualizerShowRightButton != nullptr)
        visualizerShowRightButton->setToggleState(visualizerShowRight, juce::dontSendNotification);

    if (visualizerShowMidButton != nullptr)
        visualizerShowMidButton->setToggleState(visualizerShowMid, juce::dontSendNotification);

    if (visualizerShowSideButton != nullptr)
        visualizerShowSideButton->setToggleState(visualizerShowSide, juce::dontSendNotification);

    const auto savedOrder = state.getProperty(editorBellDisplayOrderStateKey).toString().trim();

    if (savedOrder.isNotEmpty())
        bellDisplayOrder = decodeBellDisplayOrder(savedOrder, activeCount);
    else
    {
        bellDisplayOrder.clear();
        bellDisplayOrder.reserve(VxAudioProcessor::maxBellFilterCount);

        for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
            bellDisplayOrder.push_back(bellIndex);
    }

    if (static_cast<int>(bellDisplayOrder.size()) < VxAudioProcessor::maxBellFilterCount)
    {
        const auto previousSize = static_cast<int>(bellDisplayOrder.size());
        bellDisplayOrder.reserve(VxAudioProcessor::maxBellFilterCount);

        for (int bellIndex = previousSize; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
            bellDisplayOrder.push_back(bellIndex);
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

void VxAudioProcessorEditor::storeEditorStateToValueTree() noexcept
{
    auto& state = valueTreeState.state;

    if (eqeModuleLoaded)
    {
        state.setProperty(VxAudioProcessor::activeModuleStateKey, VxAudioProcessor::eqeModuleId, nullptr);
    }
    else if (speModuleLoaded)
    {
        state.setProperty(VxAudioProcessor::activeModuleStateKey, VxAudioProcessor::speModuleId, nullptr);
    }
    else if (mieModuleLoaded)
    {
        state.setProperty(VxAudioProcessor::activeModuleStateKey, VxAudioProcessor::mieModuleId, nullptr);
    }
    else if (mxeModuleLoaded)
    {
        state.setProperty(VxAudioProcessor::activeModuleStateKey, VxAudioProcessor::mxeModuleId, nullptr);
    }
    else if (tseModuleLoaded)
    {
        state.setProperty(VxAudioProcessor::activeModuleStateKey, VxAudioProcessor::tseModuleId, nullptr);
    }
    else
    {
        state.removeProperty(VxAudioProcessor::activeModuleStateKey, nullptr);
    }

    state.setProperty(editorShellGlobalHostExpandedStateKey, shellGlobalHostExpanded, nullptr);
    state.setProperty(editorVisualizerExpandedStateKey, visualizerExpanded, nullptr);
    state.setProperty(editorVisualizerRangeLowStateKey, visualizerRangeLowDb, nullptr);
    state.setProperty(editorVisualizerRangeHighStateKey, visualizerRangeHighDb, nullptr);
    state.setProperty(editorVisualizerCursorEnabledStateKey, visualizerCursorEnabled, nullptr);
    state.setProperty(editorVisualizerShowStereoStateKey, visualizerShowStereo, nullptr);
    state.setProperty(editorVisualizerShowLeftStateKey, visualizerShowLeft, nullptr);
    state.setProperty(editorVisualizerShowRightStateKey, visualizerShowRight, nullptr);
    state.setProperty(editorVisualizerShowMidStateKey, visualizerShowMid, nullptr);
    state.setProperty(editorVisualizerShowSideStateKey, visualizerShowSide, nullptr);
    state.setProperty(editorLastCollapsedWidthStateKey, lastCollapsedEditorWidth, nullptr);
    state.setProperty(editorBellDisplayOrderStateKey,
                      encodeBellDisplayOrder(bellDisplayOrder, getActiveBellCount()),
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

int VxAudioProcessorEditor::getActiveBellCount() const noexcept
{
    if (const auto* eqeProcessor = getActiveEqeProcessor())
        return eqeProcessor->getActiveBellCount();

    return 0;
}

void VxAudioProcessorEditor::updateEditorWidthForVisualizerVisibility()
{
    const auto restoredWidth = juce::jmax(minimumEditorWidth, getWidth());
    setResizeLimits(minimumEditorWidth,
                    minimumEditorHeight,
                    maximumEditorWidth,
                    maximumEditorHeight);

    if (restoredWidth != getWidth())
        setSize(restoredWidth, getHeight());

    lastCollapsedEditorWidth = restoredWidth;
}
