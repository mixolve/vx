#include "shell.EditorBellSection.h"

#include <algorithm>

void VxAudioProcessorEditor::openBellSection(const int bellIndex)
{
    if (! eqeModuleLoaded)
        return;

    const auto clickedExpanded = filtersExpanded
        && ! globalExpanded
        && ! speMainExpanded
        && ! presetsExpanded
        && ! visualizerExpanded
        && expandedBellIndex == bellIndex;
    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = true;
    presetsExpanded = false;
    visualizerExpanded = false;
    expandedBellIndex = clickedExpanded ? -1 : bellIndex;
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::selectBellSection(const int bellIndex)
{
    if (! eqeModuleLoaded)
        return;

    if (! juce::isPositiveAndBelow(bellIndex, getActiveBellCount()))
        return;

    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = true;
    presetsExpanded = false;
    visualizerExpanded = false;
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

    refreshVisualizerResponse();
}

void VxAudioProcessorEditor::selectAdjacentBellSection(const int direction)
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

juce::Rectangle<int> VxAudioProcessorEditor::getVisibleBellSectionBounds(const int bellIndex) const
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
    includeVisibleComponent(section->ttssControl.get());
    includeVisibleComponent(section->lrmsControl.get());
    includeVisibleComponent(section->slopeControl.get());
    includeVisibleComponent(section->frequencyControl.get());
    includeVisibleComponent(section->bandwidthControl.get());
    includeVisibleComponent(section->gainControl.get());
    includeVisibleComponent(section->bypassButton.get());
    includeVisibleComponent(section->deleteButton.get());

    return bounds;
}

void VxAudioProcessorEditor::moveBellSection(const int sourceIndex, const int destinationIndex)
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
    speMainExpanded = false;
    filtersExpanded = true;
    presetsExpanded = false;
    visualizerExpanded = false;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::normalizeSlopeForType(const int bellIndex)
{
    if (! juce::isPositiveAndBelow(bellIndex, static_cast<int>(bellSections.size())))
        return;

    auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

    if (bellSection == nullptr || bellSection->getFilterType() != EqeModuleProcessor::FilterType::bell)
        return;

    const auto selectedChoiceIndex = bellSection->slopeControl->getSelectedChoiceIndex();

    if (selectedChoiceIndex != 0)
        return;

    const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);
    bellSection->slopeControl->setSelectedChoiceIndex(
        EqeModuleProcessor::getBellSlopeChoiceIndexForValue(EqeModuleProcessor::fixedSlopeDbPerOct),
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

void VxAudioProcessorEditor::sortBellSectionsByPlace()
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

void VxAudioProcessorEditor::sortBellSectionsByFrequency()
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

void VxAudioProcessorEditor::sortBellSectionsByDuo()
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

void VxAudioProcessorEditor::applyBellSortOrder(const std::vector<int>& orderedIndices)
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

int VxAudioProcessorEditor::getBellIndexForOrderPosition(const int orderIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(orderIndex, getActiveBellCount()))
        return -1;

    if (! juce::isPositiveAndBelow(orderIndex, static_cast<int>(bellDisplayOrder.size())))
        return -1;

    return bellDisplayOrder[static_cast<size_t>(orderIndex)];
}

int VxAudioProcessorEditor::getBellOrderPositionForIndex(const int bellIndex) const noexcept
{
    const auto activeCount = getActiveBellCount();

    for (int orderIndex = 0; orderIndex < activeCount; ++orderIndex)
    {
        if (bellDisplayOrder[static_cast<size_t>(orderIndex)] == bellIndex)
            return orderIndex;
    }

    return -1;
}
