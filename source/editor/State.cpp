#include "EditorBellSection.h"
#include "EditorPresetSections.h"
#include "ProcessorSupport.h"

#include <algorithm>
#include <numeric>
#include <cmath>

namespace
{
constexpr auto editorBellDisplayOrderStateKey = "editor_bell_display_order";
constexpr auto editorGlobalExpandedStateKey = "editor_global_expanded";
constexpr auto editorFiltersExpandedStateKey = "editor_filters_expanded";
constexpr auto editorPresetsExpandedStateKey = "editor_presets_expanded";
#if ! JUCE_IOS
constexpr auto editorVisualizerExpandedStateKey = "editor_visualizer_expanded";
constexpr auto editorVisualizerVisibleStateKey = "editor_visualizer_visible";
constexpr auto editorVisualizerRangeLowStateKey = "editor_visualizer_range_low";
constexpr auto editorVisualizerRangeHighStateKey = "editor_visualizer_range_high";
constexpr auto editorVisualizerCursorEnabledStateKey = "editor_visualizer_cursor_enabled";
constexpr auto editorVisualizerShowStereoStateKey = "editor_visualizer_show_stereo";
constexpr auto editorVisualizerShowLeftStateKey = "editor_visualizer_show_left";
constexpr auto editorVisualizerShowRightStateKey = "editor_visualizer_show_right";
constexpr auto editorVisualizerShowMidStateKey = "editor_visualizer_show_mid";
constexpr auto editorVisualizerShowSideStateKey = "editor_visualizer_show_side";
constexpr auto editorLastCollapsedWidthStateKey = "editor_last_collapsed_width";
constexpr auto editorLastVisualizerWidthStateKey = "editor_last_visualizer_width";
#endif
constexpr auto editorExpandedBellIndexStateKey = "editor_expanded_bell_index";

juce::Point<int> clampEditorSize(const int width, const int height) noexcept
{
    return { juce::jlimit(minimumEditorWidth, maximumEditorWidth, width),
             juce::jlimit(minimumEditorHeight, maximumEditorHeight, height) };
}

#if ! JUCE_IOS
int clampCollapsedEditorWidth(const int width) noexcept
{
    return juce::jlimit(minimumEditorWidth,
                        juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                        width);
}
#endif

struct NumberedNameSeed
{
    juce::String prefix;
    bool hasSeparatorBeforeNumber = false;
    int nextNumber = 2;
};

NumberedNameSeed makeNumberedNameSeed(const juce::String& name)
{
    const auto trimmedName = name.trim();
    auto splitIndex = trimmedName.length();

    while (splitIndex > 0 && juce::CharacterFunctions::isDigit(trimmedName[splitIndex - 1]))
        --splitIndex;

    if (splitIndex == trimmedName.length())
        return { trimmedName, true, 2 };

    const auto numberText = trimmedName.substring(splitIndex);
    const auto prefix = trimmedName.substring(0, splitIndex);

    return { prefix, false, numberText.getIntValue() + 1 };
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

    std::array<bool, EqeAudioProcessor::maxBellFilterCount> used {};
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

juce::String buildNumberedName(const juce::String& prefix,
                               const int number,
                               const bool insertSeparator)
{
    if (prefix.isEmpty())
        return juce::String(number);

    return insertSeparator ? prefix + " " + juce::String(number)
                           : prefix + juce::String(number);
}

juce::String makeUniquePresetName(const juce::String& requestedName,
                                  const juce::StringArray& existingNames,
                                  const juce::String& fallbackPrefix)
{
    auto baseName = requestedName.trim();

    if (baseName.isEmpty())
    {
        auto nextIndex = existingNames.size() + 1;
        auto generatedName = fallbackPrefix + " " + juce::String(nextIndex);

        while (existingNames.contains(generatedName, true))
            generatedName = fallbackPrefix + " " + juce::String(++nextIndex);

        return generatedName;
    }

    if (! existingNames.contains(baseName, true))
        return baseName;

    const auto numberedSeed = makeNumberedNameSeed(baseName);
    auto candidateNumber = numberedSeed.nextNumber;
    auto candidate = buildNumberedName(numberedSeed.prefix, candidateNumber, numberedSeed.hasSeparatorBeforeNumber);

    while (existingNames.contains(candidate, true))
        candidate = buildNumberedName(numberedSeed.prefix,
                                       ++candidateNumber,
                                       numberedSeed.hasSeparatorBeforeNumber);

    return candidate;
}

void preserveEditorWindowAndVisualizerState(juce::XmlElement& targetStateElement,
                                            const juce::ValueTree& sourceState)
{
    const auto copyStateProperty = [&targetStateElement, &sourceState] (const juce::Identifier& propertyId)
    {
        if (sourceState.hasProperty(propertyId))
            targetStateElement.setAttribute(propertyId.toString(), sourceState.getProperty(propertyId).toString());
        else
            targetStateElement.removeAttribute(propertyId.toString());
    };

    copyStateProperty(EqeAudioProcessor::editorWidthStateKey);
    copyStateProperty(EqeAudioProcessor::editorHeightStateKey);
    copyStateProperty(EqeAudioProcessor::activeModuleStateKey);

#if ! JUCE_IOS
    copyStateProperty(editorVisualizerExpandedStateKey);
    copyStateProperty(editorVisualizerVisibleStateKey);
    copyStateProperty(editorVisualizerRangeLowStateKey);
    copyStateProperty(editorVisualizerRangeHighStateKey);
    copyStateProperty(editorVisualizerCursorEnabledStateKey);
    copyStateProperty(editorVisualizerShowStereoStateKey);
    copyStateProperty(editorVisualizerShowLeftStateKey);
    copyStateProperty(editorVisualizerShowRightStateKey);
    copyStateProperty(editorVisualizerShowMidStateKey);
    copyStateProperty(editorVisualizerShowSideStateKey);
    copyStateProperty(editorLastCollapsedWidthStateKey);
    copyStateProperty(editorLastVisualizerWidthStateKey);
#endif
}
}

void EqeAudioProcessorEditor::showModulePicker()
{
    if (moduleAddButton == nullptr)
        return;

    auto anchorBounds = moduleAddButton->getBounds();
    anchorBounds.setSize(juce::jmax(120, anchorBounds.getWidth()), anchorBounds.getHeight());
    anchorBounds.setCentre(moduleAddButton->getBounds().getCentre());

    showChoicePrompt(anchorBounds,
                     { "EQE" },
                     -1,
                     { true },
                     juce::Justification::centred,
                     [this] (const int selectedIndex)
                     {
                         if (selectedIndex == 0)
                             loadEqeModule();
                     });
}

void EqeAudioProcessorEditor::loadEqeModule()
{
    eqeModuleLoaded = true;
    audioProcessor.setEqeModuleLoaded(true);

    globalExpanded = false;
    filtersExpanded = true;
    presetsExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif
    expandedBellIndex = getActiveBellCount() > 0 ? 0 : -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void EqeAudioProcessorEditor::openGlobalSection()
{
    if (! eqeModuleLoaded)
        return;

    const auto clickedExpanded = globalExpanded
        && ! filtersExpanded
        && ! presetsExpanded
#if ! JUCE_IOS
        && ! visualizerExpanded
#endif
        ;
    globalExpanded = ! clickedExpanded;
    filtersExpanded = false;
    presetsExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif

    if (globalExpanded)
        expandedBellIndex = -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void EqeAudioProcessorEditor::openBellSection(const int bellIndex)
{
    if (! eqeModuleLoaded)
        return;

    const auto clickedExpanded = filtersExpanded
        && ! globalExpanded
        && ! presetsExpanded
#if ! JUCE_IOS
        && ! visualizerExpanded
#endif
        && expandedBellIndex == bellIndex;
    globalExpanded = false;
    filtersExpanded = true;
    presetsExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif
    expandedBellIndex = clickedExpanded ? -1 : bellIndex;
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void EqeAudioProcessorEditor::selectBellSection(const int bellIndex)
{
    if (! eqeModuleLoaded)
        return;

    if (! juce::isPositiveAndBelow(bellIndex, getActiveBellCount()))
        return;

    globalExpanded = false;
    filtersExpanded = true;
    presetsExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif
    expandedBellIndex = bellIndex;
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();

    if (auto* section = bellSections[static_cast<size_t>(bellIndex)].get())
    {
        juce::ignoreUnused(section);
        const auto sectionBounds = getVisibleBellSectionBounds(bellIndex);
        const auto currentY = filterViewport.getViewPositionY();
        const auto viewportHeight = filterViewport.getHeight();
        auto targetY = currentY;

        if (sectionBounds.getY() < currentY)
            targetY = sectionBounds.getY();
        else if (sectionBounds.getBottom() > currentY + viewportHeight)
            targetY = sectionBounds.getBottom() - viewportHeight;

        const auto maxOffset = juce::jmax(0, getFilterContentHeight() - viewportHeight);
        filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, targetY));
    }

#if ! JUCE_IOS
    refreshVisualizerResponse();
#endif
}

void EqeAudioProcessorEditor::selectAdjacentBellSection(const int direction)
{
    const auto activeCount = getActiveBellCount();

    if (activeCount <= 0 || direction == 0)
        return;

    auto currentOrderIndex = expandedBellIndex >= 0 ? getBellOrderPositionForIndex(expandedBellIndex)
                                                     : -1;

    if (currentOrderIndex < 0)
        currentOrderIndex = direction > 0 ? -1 : activeCount;

    const auto targetOrderIndex = juce::jlimit(0, activeCount - 1, currentOrderIndex + (direction > 0 ? 1 : -1));
    const auto targetBellIndex = getBellIndexForOrderPosition(targetOrderIndex);

    if (targetBellIndex >= 0)
        selectBellSection(targetBellIndex);
}

juce::Rectangle<int> EqeAudioProcessorEditor::getVisibleBellSectionBounds(const int bellIndex) const
{
    if (! juce::isPositiveAndBelow(bellIndex, getActiveBellCount()))
        return {};

    const auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

    if (section == nullptr || section->header == nullptr)
        return {};

    auto bounds = filterContent.getLocalArea(section->header.get(), section->header->getLocalBounds());

    auto includeVisibleComponent = [this, &bounds] (const juce::Component* component)
    {
        if (component == nullptr || ! component->isVisible() || component->getBounds().isEmpty())
            return;

        bounds = bounds.getUnion(filterContent.getLocalArea(component, component->getLocalBounds()));
    };

    includeVisibleComponent(section->typeControl.get());
    includeVisibleComponent(section->lrmsControl.get());
    includeVisibleComponent(section->slopeControl.get());
    includeVisibleComponent(section->frequencyControl.get());
    includeVisibleComponent(section->bandwidthControl.get());
    includeVisibleComponent(section->gainControl.get());
    includeVisibleComponent(section->bypassButton.get());
    includeVisibleComponent(section->deleteButton.get());

    return bounds;
}

void EqeAudioProcessorEditor::openFiltersSection()
{
    if (! eqeModuleLoaded)
        return;

    const auto clickedExpanded = filtersExpanded
        && ! globalExpanded
        && ! presetsExpanded
#if ! JUCE_IOS
        && ! visualizerExpanded
#endif
        ;
    filtersExpanded = ! clickedExpanded;
    globalExpanded = false;
    presetsExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif

    if (! filtersExpanded)
        expandedBellIndex = -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void EqeAudioProcessorEditor::openPresetsSection()
{
    if (! eqeModuleLoaded)
        return;

    const auto clickedExpanded = presetsExpanded
        && ! globalExpanded
        && ! filtersExpanded
#if ! JUCE_IOS
        && ! visualizerExpanded
#endif
        ;
    presetsExpanded = ! clickedExpanded;
    globalExpanded = false;
    filtersExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif
    expandedBellIndex = -1;

    if (presetsExpanded)
        refreshFilterPresetList(audioProcessor.getLastFilterPresetName());

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

#if ! JUCE_IOS
void EqeAudioProcessorEditor::openVisualizerSection()
{
    if (! eqeModuleLoaded)
        return;

    const auto clickedExpanded = visualizerExpanded
        && ! globalExpanded
        && ! filtersExpanded
        && ! presetsExpanded;
    visualizerExpanded = ! clickedExpanded;
    globalExpanded = false;
    filtersExpanded = false;
    presetsExpanded = false;
    expandedBellIndex = -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}
#endif

void EqeAudioProcessorEditor::moveBellSection(const int sourceIndex, const int destinationIndex)
{
    if (sourceIndex == destinationIndex)
        return;

    const auto activeCount = getActiveBellCount();

    if (! juce::isPositiveAndBelow(sourceIndex, activeCount)
        || ! juce::isPositiveAndBelow(destinationIndex, activeCount))
        return;

    std::swap(bellDisplayOrder[static_cast<size_t>(sourceIndex)],
              bellDisplayOrder[static_cast<size_t>(destinationIndex)]);

    globalExpanded = false;
    filtersExpanded = true;
    presetsExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void EqeAudioProcessorEditor::clearAllFilters()
{
    if (! audioProcessor.clearBellFilters())
        return;

    bellDisplayOrder.clear();
    bellDisplayOrder.reserve(EqeAudioProcessor::maxBellFilterCount);

    for (int bellIndex = 0; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        bellDisplayOrder.push_back(bellIndex);
        resetBellSectionStoredValues(bellIndex);
    }

    globalExpanded = true;
    filtersExpanded = false;
    presetsExpanded = false;
#if ! JUCE_IOS
    visualizerExpanded = false;
#endif
    expandedBellIndex = -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void EqeAudioProcessorEditor::performUndo()
{
    commitPendingHistorySnapshot(true);

    if (undoHistory.empty())
        return;

    redoHistory.push_back(committedHistorySnapshot);
    const auto snapshot = undoHistory.back();
    undoHistory.pop_back();
    applyHistorySnapshot(snapshot);
}

void EqeAudioProcessorEditor::performRedo()
{
    commitPendingHistorySnapshot(true);

    if (redoHistory.empty())
        return;

    undoHistory.push_back(committedHistorySnapshot);
    const auto snapshot = redoHistory.back();
    redoHistory.pop_back();
    applyHistorySnapshot(snapshot);
}

void EqeAudioProcessorEditor::refreshFilterPresetList(const juce::String& preferredSelection)
{
    if (presetsSection == nullptr)
        return;

    const auto presetNames = audioProcessor.getFilterPresetNames();
    presetsSection->setPresetNames(presetNames, preferredSelection);

    const auto hasPresetSelection = presetNames.size() > 0
        && presetsSection->getSelectedPresetName().isNotEmpty();
    const auto selectedPresetName = presetsSection->getSelectedPresetName();
    const auto defaultPresetName = audioProcessor.getDefaultFilterPresetName();
    const auto selectedPresetIsDefault = hasPresetSelection
        && selectedPresetName.equalsIgnoreCase(defaultPresetName);
    const auto canDeletePreset = presetNames.size() > 1 && hasPresetSelection;
    presetsSection->deleteButton->setEnabled(canDeletePreset);
    presetsSection->deleteButton->setAlpha(canDeletePreset ? 1.0f : 0.45f);
    presetsSection->renameButton->setEnabled(hasPresetSelection);
    presetsSection->renameButton->setAlpha(hasPresetSelection ? 1.0f : 0.45f);
    presetsSection->defaultButton->setEnabled(hasPresetSelection);
    presetsSection->defaultButton->setAlpha(hasPresetSelection ? 1.0f : 0.45f);
    presetsSection->defaultButton->setAlwaysAccentOutline(selectedPresetIsDefault);
}

void EqeAudioProcessorEditor::reloadFilterPresetFromProcessor()
{
    if (presetsSection == nullptr)
        return;

    restoreEditorStateFromValueTree();

    for (auto& sectionPtr : bellSections)
    {
        auto* section = sectionPtr.get();

        if (section == nullptr)
            continue;

        const auto loadedType = section->getFilterType();
        section->lastFilterType = loadedType;
        section->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(loadedType));
        section->slopeControl->setChoiceEnabled(0, loadedType != EqeAudioProcessor::FilterType::bell);

        for (const auto filterType : EqeAudioProcessor::filterTypePresetOrder)
        {
            section->setStoredValues(filterType,
                                     defaultFilterFrequencyForType(filterType),
                                     defaultFilterBandwidthForType(filterType),
                                     defaultFilterSlopeForType(filterType),
                                     0,
                                     false);
        }

        section->captureCurrentValuesForCurrentType(true);
    }

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void EqeAudioProcessorEditor::addFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    const auto presetName = makeUniquePresetName(presetsSection->getEnteredPresetName(),
                                                 audioProcessor.getFilterPresetNames(),
                                                 "PRESET");

    if (audioProcessor.saveFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

void EqeAudioProcessorEditor::saveFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    auto presetName = presetsSection->getEnteredPresetName();

    if (presetName.isEmpty())
    {
        presetName = makeUniquePresetName({}, audioProcessor.getFilterPresetNames(), "PRESET");
    }

    if (presetName.isEmpty())
        return;

    if (audioProcessor.saveFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

bool EqeAudioProcessorEditor::renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName)
{
    const auto trimmedOldName = oldPresetName.trim();
    const auto trimmedNewName = newPresetName.trim();

    if (trimmedOldName.isEmpty() || trimmedNewName.isEmpty())
        return false;

    if (! audioProcessor.renameFilterPreset(trimmedOldName, trimmedNewName))
        return false;

    refreshFilterPresetList(trimmedNewName);
    return true;
}

void EqeAudioProcessorEditor::setDefaultFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    const auto presetName = presetsSection->getSelectedPresetName();

    if (presetName.isEmpty())
        return;

    if (audioProcessor.setDefaultFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

void EqeAudioProcessorEditor::deleteSelectedFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    if (audioProcessor.getFilterPresetNames().size() <= 1)
        return;

    const auto presetName = presetsSection->getSelectedPresetName();

    if (presetName.isEmpty())
        return;

    if (audioProcessor.deleteFilterPreset(presetName))
        refreshFilterPresetList();
}

void EqeAudioProcessorEditor::normalizeSlopeForType(const int bellIndex)
{
    if (! juce::isPositiveAndBelow(bellIndex, static_cast<int>(bellSections.size())))
        return;

    auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

    if (bellSection == nullptr || bellSection->getFilterType() != EqeAudioProcessor::FilterType::bell)
        return;

    const auto selectedChoiceIndex = bellSection->slopeControl->getSelectedChoiceIndex();

    if (selectedChoiceIndex != 0)
        return;

    const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);
    bellSection->slopeControl->setSelectedChoiceIndex(
        EqeAudioProcessor::getBellSlopeChoiceIndexForValue(EqeAudioProcessor::fixedSlopeDbPerOct),
        true);
}

namespace
{
struct BellSortKey
{
    int index = 0;
    int place = 0;
    double frequency = 0.0;
};
}

void EqeAudioProcessorEditor::sortBellSectionsByPlace()
{
    const auto activeCount = getActiveBellCount();

    if (activeCount <= 1)
        return;

    std::vector<BellSortKey> sortKeys;
    sortKeys.reserve(static_cast<size_t>(activeCount));

    for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
    {
        auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

        if (section == nullptr)
            return;

        sortKeys.push_back({ bellIndex, juce::jlimit(0, 4, section->getPlace()), section->getFrequency() });
    }

    std::stable_sort(sortKeys.begin(),
                     sortKeys.end(),
                     [] (const BellSortKey& left, const BellSortKey& right)
                     {
                         return left.place < right.place;
                     });

    std::vector<int> orderedIndices;
    orderedIndices.reserve(sortKeys.size());

    for (const auto& key : sortKeys)
        orderedIndices.push_back(key.index);

    applyBellSortOrder(orderedIndices);
}

void EqeAudioProcessorEditor::sortBellSectionsByFrequency()
{
    const auto activeCount = getActiveBellCount();

    if (activeCount <= 1)
        return;

    std::vector<BellSortKey> sortKeys;
    sortKeys.reserve(static_cast<size_t>(activeCount));

    for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
    {
        auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

        if (section == nullptr)
            return;

        sortKeys.push_back({ bellIndex, juce::jlimit(0, 4, section->getPlace()), section->getFrequency() });
    }

    std::stable_sort(sortKeys.begin(),
                     sortKeys.end(),
                     [] (const BellSortKey& left, const BellSortKey& right)
                     {
                         return left.frequency < right.frequency;
                     });

    std::vector<int> orderedIndices;
    orderedIndices.reserve(sortKeys.size());

    for (const auto& key : sortKeys)
        orderedIndices.push_back(key.index);

    applyBellSortOrder(orderedIndices);
}

void EqeAudioProcessorEditor::sortBellSectionsByDuo()
{
    const auto activeCount = getActiveBellCount();

    if (activeCount <= 1)
        return;

    std::vector<BellSortKey> sortKeys;
    sortKeys.reserve(static_cast<size_t>(activeCount));

    for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
    {
        auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

        if (section == nullptr)
            return;

        sortKeys.push_back({ bellIndex, juce::jlimit(0, 4, section->getPlace()), section->getFrequency() });
    }

    std::stable_sort(sortKeys.begin(),
                     sortKeys.end(),
                     [] (const BellSortKey& left, const BellSortKey& right)
                     {
                         if (left.place != right.place)
                             return left.place < right.place;

                         return left.frequency < right.frequency;
                     });

    std::vector<int> orderedIndices;
    orderedIndices.reserve(sortKeys.size());

    for (const auto& key : sortKeys)
        orderedIndices.push_back(key.index);

    applyBellSortOrder(orderedIndices);
}

void EqeAudioProcessorEditor::applyBellSortOrder(const std::vector<int>& orderedIndices)
{
    const auto activeCount = getActiveBellCount();

    if (activeCount <= 1 || static_cast<int>(orderedIndices.size()) != activeCount)
        return;

    for (int destinationIndex = 0; destinationIndex < activeCount; ++destinationIndex)
        bellDisplayOrder[static_cast<size_t>(destinationIndex)] = orderedIndices[static_cast<size_t>(destinationIndex)];

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void EqeAudioProcessorEditor::parameterChanged(const juce::String&, float)
{
    if (suppressHistorySnapshots)
        return;

    pendingHistorySnapshot.store(true, std::memory_order_relaxed);
    lastHistoryChangeTimeMs.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
}

void EqeAudioProcessorEditor::registerParameterListeners()
{
    valueTreeState.addParameterListener(EqeAudioProcessor::paramGlobalGainLId, this);
    valueTreeState.addParameterListener(EqeAudioProcessor::paramGlobalGainRId, this);
    valueTreeState.addParameterListener(EqeAudioProcessor::paramGlobalWideId, this);
    valueTreeState.addParameterListener(EqeAudioProcessor::paramOutGainId, this);
    valueTreeState.addParameterListener(EqeAudioProcessor::paramGlobalBypassId, this);
    valueTreeState.addParameterListener(EqeAudioProcessor::paramGlobalBypassOutGainOnlyId, this);

    for (int bellIndex = 0; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        valueTreeState.addParameterListener(EqeAudioProcessor::getFilterTypeParamId(bellIndex), this);
        valueTreeState.addParameterListener(EqeAudioProcessor::getFilterLrmsParamId(bellIndex), this);
        valueTreeState.addParameterListener(EqeAudioProcessor::getBellFrequencyParamId(bellIndex), this);
        valueTreeState.addParameterListener(EqeAudioProcessor::getBellBandwidthParamId(bellIndex), this);
        valueTreeState.addParameterListener(EqeAudioProcessor::getBellSlopeParamId(bellIndex), this);
        valueTreeState.addParameterListener(EqeAudioProcessor::getBellGainParamId(bellIndex), this);
        valueTreeState.addParameterListener(EqeAudioProcessor::getBellBypassParamId(bellIndex), this);
    }
}

void EqeAudioProcessorEditor::unregisterParameterListeners()
{
    valueTreeState.removeParameterListener(EqeAudioProcessor::paramGlobalGainLId, this);
    valueTreeState.removeParameterListener(EqeAudioProcessor::paramGlobalGainRId, this);
    valueTreeState.removeParameterListener(EqeAudioProcessor::paramGlobalWideId, this);
    valueTreeState.removeParameterListener(EqeAudioProcessor::paramOutGainId, this);
    valueTreeState.removeParameterListener(EqeAudioProcessor::paramGlobalBypassId, this);
    valueTreeState.removeParameterListener(EqeAudioProcessor::paramGlobalBypassOutGainOnlyId, this);

    for (int bellIndex = 0; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        valueTreeState.removeParameterListener(EqeAudioProcessor::getFilterTypeParamId(bellIndex), this);
        valueTreeState.removeParameterListener(EqeAudioProcessor::getFilterLrmsParamId(bellIndex), this);
        valueTreeState.removeParameterListener(EqeAudioProcessor::getBellFrequencyParamId(bellIndex), this);
        valueTreeState.removeParameterListener(EqeAudioProcessor::getBellBandwidthParamId(bellIndex), this);
        valueTreeState.removeParameterListener(EqeAudioProcessor::getBellSlopeParamId(bellIndex), this);
        valueTreeState.removeParameterListener(EqeAudioProcessor::getBellGainParamId(bellIndex), this);
        valueTreeState.removeParameterListener(EqeAudioProcessor::getBellBypassParamId(bellIndex), this);
    }
}

void EqeAudioProcessorEditor::scheduleHistorySnapshot()
{
    if (suppressHistorySnapshots)
        return;

    pendingHistorySnapshot.store(true, std::memory_order_relaxed);
    lastHistoryChangeTimeMs.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
}

void EqeAudioProcessorEditor::commitPendingHistorySnapshot(const bool force)
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

void EqeAudioProcessorEditor::applyHistorySnapshot(const juce::MemoryBlock& snapshot)
{
    if (snapshot.isEmpty())
        return;

    auto mergedStateXml = EqeAudioProcessor::getXmlFromBinary(snapshot.getData(), static_cast<int>(snapshot.getSize()));

    if (mergedStateXml == nullptr || ! mergedStateXml->hasTagName(valueTreeState.state.getType().toString()))
        return;

    preserveEditorWindowAndVisualizerState(*mergedStateXml, valueTreeState.state);

    juce::MemoryBlock mergedSnapshot;
    EqeAudioProcessor::copyXmlToBinary(*mergedStateXml, mergedSnapshot);

    struct PreservedUiState
    {
        bool global = false;
        bool filters = false;
        bool presets = false;
#if ! JUCE_IOS
        bool visualizer = false;
#endif
        int expandedBell = -1;
        int globalScrollY = 0;
        int filterScrollY = 0;
    };

    const PreservedUiState preservedUiState
    {
        globalExpanded,
        filtersExpanded,
        presetsExpanded,
#if ! JUCE_IOS
        visualizerExpanded,
#endif
        expandedBellIndex,
        globalViewport.getViewPositionY(),
        filterViewport.getViewPositionY()
    };

    const juce::ScopedValueSetter<bool> suppressHistory(suppressHistorySnapshots, true);
    pendingHistorySnapshot.store(false, std::memory_order_relaxed);
    audioProcessor.setStateInformation(mergedSnapshot.getData(), static_cast<int>(mergedSnapshot.getSize()));
    refreshFilterPresetList(audioProcessor.getLastFilterPresetName());
    reloadFilterPresetFromProcessor();

    globalExpanded = preservedUiState.global;
    filtersExpanded = preservedUiState.filters;
    presetsExpanded = preservedUiState.presets;
#if ! JUCE_IOS
    visualizerExpanded = preservedUiState.visualizer;
#endif
    expandedBellIndex = preservedUiState.expandedBell;

    if (! filtersExpanded || ! juce::isPositiveAndBelow(expandedBellIndex, getActiveBellCount()))
        expandedBellIndex = -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();

    const auto globalMaxOffset = juce::jmax(0, getGlobalContentHeight() - globalViewport.getHeight());
    globalViewport.setViewPosition(0, juce::jlimit(0, globalMaxOffset, preservedUiState.globalScrollY));

    const auto filterMaxOffset = juce::jmax(0, getFilterContentHeight() - filterViewport.getHeight());
    filterViewport.setViewPosition(0, juce::jlimit(0, filterMaxOffset, preservedUiState.filterScrollY));

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
}

void EqeAudioProcessorEditor::updateUndoRedoButtons()
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

void EqeAudioProcessorEditor::resetBellSectionStoredValues(const int bellIndex)
{
    if (! juce::isPositiveAndBelow(bellIndex, static_cast<int>(bellSections.size())))
        return;

    auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

    if (section == nullptr)
        return;

    for (const auto filterType : EqeAudioProcessor::filterTypePresetOrder)
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

void EqeAudioProcessorEditor::moveBellSectionStoredValues(const int sourceIndex, const int destinationIndex)
{
    if (sourceIndex == destinationIndex)
        return;

    const auto activeCount = getActiveBellCount();

    if (! juce::isPositiveAndBelow(sourceIndex, activeCount)
        || ! juce::isPositiveAndBelow(destinationIndex, activeCount))
        return;

    auto copySectionState = [this] (const int destination, const int source)
    {
        auto* destinationSection = bellSections[static_cast<size_t>(destination)].get();
        auto* sourceSection = bellSections[static_cast<size_t>(source)].get();

        if (destinationSection == nullptr || sourceSection == nullptr)
            return;

        destinationSection->copyStoredValuesFrom(*sourceSection);
    };

    auto tempSection = std::make_unique<BellSection>(valueTreeState, 0);
    tempSection->copyStoredValuesFrom(*bellSections[static_cast<size_t>(sourceIndex)]);

    if (sourceIndex < destinationIndex)
    {
        for (int index = sourceIndex; index < destinationIndex; ++index)
            copySectionState(index, index + 1);
    }
    else
    {
        for (int index = sourceIndex; index > destinationIndex; --index)
            copySectionState(index, index - 1);
    }

    bellSections[static_cast<size_t>(destinationIndex)]->copyStoredValuesFrom(*tempSection);
}

void EqeAudioProcessorEditor::removeBellSectionStoredValues(const int removedIndex, const int previousCount)
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

void EqeAudioProcessorEditor::restoreEditorStateFromValueTree()
{
    const auto activeCount = getActiveBellCount();
    auto& state = valueTreeState.state;

    eqeModuleLoaded = state.getProperty(EqeAudioProcessor::activeModuleStateKey).toString().equalsIgnoreCase(EqeAudioProcessor::eqeModuleId)
        || audioProcessor.isEqeModuleLoaded();
    audioProcessor.setEqeModuleLoaded(eqeModuleLoaded);

    globalExpanded = state.getProperty(editorGlobalExpandedStateKey, false);
    filtersExpanded = state.getProperty(editorFiltersExpandedStateKey, true);
    presetsExpanded = state.getProperty(editorPresetsExpandedStateKey, false);
#if ! JUCE_IOS
    visualizerExpanded = state.getProperty(editorVisualizerExpandedStateKey, false);
    visualizerVisible = state.getProperty(editorVisualizerVisibleStateKey, false);
    visualizerCursorEnabled = state.getProperty(editorVisualizerCursorEnabledStateKey, true);
    visualizerShowStereo = state.getProperty(editorVisualizerShowStereoStateKey, true);
    visualizerShowLeft = state.getProperty(editorVisualizerShowLeftStateKey, true);
    visualizerShowRight = state.getProperty(editorVisualizerShowRightStateKey, true);
    visualizerShowMid = state.getProperty(editorVisualizerShowMidStateKey, true);
    visualizerShowSide = state.getProperty(editorVisualizerShowSideStateKey, true);
#endif
    expandedBellIndex = static_cast<int>(state.getProperty(editorExpandedBellIndexStateKey,
                                                           activeCount > 0 ? 0 : -1));
#if ! JUCE_IOS
    visualizerRangeLowDb = static_cast<float>(state.getProperty(editorVisualizerRangeLowStateKey, -24.0f));
    visualizerRangeHighDb = static_cast<float>(state.getProperty(editorVisualizerRangeHighStateKey, 24.0f));
#endif

    const auto storedGlobalWide = static_cast<float>(state.getProperty("global_wide", 100.0f));

    if (auto* wideParam = audioProcessor.getValueTreeState().getRawParameterValue(EqeAudioProcessor::paramGlobalWideId))
        wideParam->store(storedGlobalWide, std::memory_order_relaxed);

#if ! JUCE_IOS
    lastCollapsedEditorWidth = clampCollapsedEditorWidth(static_cast<int>(state.getProperty(editorLastCollapsedWidthStateKey,
                                                                                            minimumEditorWidth)));
    lastVisualizerWidth = juce::jmax(minimumVisualizerWidth,
                                     static_cast<int>(state.getProperty(editorLastVisualizerWidthStateKey,
                                                                        defaultVisualizerWidth)));
#endif

#if ! JUCE_IOS
    if (globalExpanded)
    {
        filtersExpanded = false;
        presetsExpanded = false;
        visualizerExpanded = false;
    }
    else if (filtersExpanded)
    {
        presetsExpanded = false;
        visualizerExpanded = false;
    }
    else if (presetsExpanded)
    {
        filtersExpanded = false;
        visualizerExpanded = false;
    }
    else if (visualizerExpanded)
    {
        filtersExpanded = false;
        presetsExpanded = false;
    }
#else
    if (globalExpanded)
    {
        filtersExpanded = false;
        presetsExpanded = false;
    }
    else if (filtersExpanded)
    {
        presetsExpanded = false;
    }
    else if (presetsExpanded)
    {
        filtersExpanded = false;
    }
#endif

    if (! filtersExpanded || ! juce::isPositiveAndBelow(expandedBellIndex, activeCount))
        expandedBellIndex = -1;

#if ! JUCE_IOS
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
#endif

    const auto savedOrder = state.getProperty(editorBellDisplayOrderStateKey).toString().trim();

    if (savedOrder.isNotEmpty())
        bellDisplayOrder = decodeBellDisplayOrder(savedOrder, activeCount);
    else
    {
        bellDisplayOrder.clear();
        bellDisplayOrder.reserve(EqeAudioProcessor::maxBellFilterCount);

        for (int bellIndex = 0; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
            bellDisplayOrder.push_back(bellIndex);
    }

    if (static_cast<int>(bellDisplayOrder.size()) < EqeAudioProcessor::maxBellFilterCount)
    {
        const auto previousSize = static_cast<int>(bellDisplayOrder.size());
        bellDisplayOrder.reserve(EqeAudioProcessor::maxBellFilterCount);

        for (int bellIndex = previousSize; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
            bellDisplayOrder.push_back(bellIndex);
    }
}

void EqeAudioProcessorEditor::storeEditorStateToValueTree() noexcept
{
    auto& state = valueTreeState.state;

    if (eqeModuleLoaded)
        state.setProperty(EqeAudioProcessor::activeModuleStateKey, EqeAudioProcessor::eqeModuleId, nullptr);
    else
        state.removeProperty(EqeAudioProcessor::activeModuleStateKey, nullptr);

    state.setProperty(editorGlobalExpandedStateKey, globalExpanded, nullptr);
    state.setProperty(editorFiltersExpandedStateKey, filtersExpanded, nullptr);
    state.setProperty(editorPresetsExpandedStateKey, presetsExpanded, nullptr);
#if ! JUCE_IOS
    state.setProperty(editorVisualizerExpandedStateKey, visualizerExpanded, nullptr);
    state.setProperty(editorVisualizerVisibleStateKey, visualizerVisible, nullptr);
    state.setProperty(editorVisualizerRangeLowStateKey, visualizerRangeLowDb, nullptr);
    state.setProperty(editorVisualizerRangeHighStateKey, visualizerRangeHighDb, nullptr);
    state.setProperty(editorVisualizerCursorEnabledStateKey, visualizerCursorEnabled, nullptr);
    state.setProperty(editorVisualizerShowStereoStateKey, visualizerShowStereo, nullptr);
    state.setProperty(editorVisualizerShowLeftStateKey, visualizerShowLeft, nullptr);
    state.setProperty(editorVisualizerShowRightStateKey, visualizerShowRight, nullptr);
    state.setProperty(editorVisualizerShowMidStateKey, visualizerShowMid, nullptr);
    state.setProperty(editorVisualizerShowSideStateKey, visualizerShowSide, nullptr);
    state.setProperty(editorLastCollapsedWidthStateKey, lastCollapsedEditorWidth, nullptr);
    state.setProperty(editorLastVisualizerWidthStateKey, lastVisualizerWidth, nullptr);
#endif
    state.setProperty(editorExpandedBellIndexStateKey, expandedBellIndex, nullptr);
    state.setProperty(editorBellDisplayOrderStateKey,
                      encodeBellDisplayOrder(bellDisplayOrder, getActiveBellCount()),
                      nullptr);

    if (! suppressEditorSizeStateSave && getWidth() > 0 && getHeight() > 0)
    {
        const auto size = clampEditorSize(getWidth(), getHeight());
        audioProcessor.setLastEditorSize(size.x, size.y);
        state.setProperty(EqeAudioProcessor::editorWidthStateKey, size.x, nullptr);
        state.setProperty(EqeAudioProcessor::editorHeightStateKey, size.y, nullptr);
    }
}

juce::Point<int> EqeAudioProcessorEditor::getRestoredEditorSize() const noexcept
{
    const auto& state = valueTreeState.state;

    if (state.hasProperty(EqeAudioProcessor::editorWidthStateKey)
        && state.hasProperty(EqeAudioProcessor::editorHeightStateKey))
    {
        return clampEditorSize(static_cast<int>(state.getProperty(EqeAudioProcessor::editorWidthStateKey, initialEditorWidth)),
                               static_cast<int>(state.getProperty(EqeAudioProcessor::editorHeightStateKey, initialEditorHeight)));
    }

    const auto lastSize = audioProcessor.getLastEditorSize();

    if (lastSize.x > 0 && lastSize.y > 0)
        return clampEditorSize(lastSize.x, lastSize.y);

    return { initialEditorWidth, initialEditorHeight };
}

int EqeAudioProcessorEditor::getBellIndexForOrderPosition(const int orderIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(orderIndex, getActiveBellCount()))
        return -1;

    if (! juce::isPositiveAndBelow(orderIndex, static_cast<int>(bellDisplayOrder.size())))
        return -1;

    return bellDisplayOrder[static_cast<size_t>(orderIndex)];
}

int EqeAudioProcessorEditor::getBellOrderPositionForIndex(const int bellIndex) const noexcept
{
    const auto activeCount = getActiveBellCount();

    for (int orderIndex = 0; orderIndex < activeCount; ++orderIndex)
    {
        if (bellDisplayOrder[static_cast<size_t>(orderIndex)] == bellIndex)
            return orderIndex;
    }

    return -1;
}

int EqeAudioProcessorEditor::getActiveBellCount() const noexcept
{
    return audioProcessor.getActiveBellCount();
}

#if ! JUCE_IOS
int EqeAudioProcessorEditor::getCurrentMainPanelWidth() const noexcept
{
    return visualizerVisible ? lastCollapsedEditorWidth
                             : getWidth();
}

void EqeAudioProcessorEditor::updateEditorWidthForVisualizerVisibility()
{
    if (visualizerVisible)
    {
        lastCollapsedEditorWidth = clampCollapsedEditorWidth(juce::jmax(minimumEditorWidth, lastCollapsedEditorWidth));
        lastVisualizerWidth = juce::jmax(minimumVisualizerWidth, lastVisualizerWidth);
        const auto minimumVisibleWidth = lastCollapsedEditorWidth + visualizerPanelGap + minimumVisualizerWidth;
        const auto restoredWidth = juce::jmax(minimumVisibleWidth,
                                              lastCollapsedEditorWidth + visualizerPanelGap + lastVisualizerWidth);
        setResizeLimits(minimumVisibleWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
        setSize(restoredWidth, getHeight());
        return;
    }

    lastVisualizerWidth = juce::jmax(minimumVisualizerWidth,
                                     getWidth() - visualizerPanelGap - lastCollapsedEditorWidth);
    const auto restoredWidth = juce::jmax(minimumEditorWidth, lastCollapsedEditorWidth);
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
    setSize(restoredWidth, getHeight());
    lastCollapsedEditorWidth = restoredWidth;
}
#endif
