#include "shell.EditorFilterSection.h"

#include <algorithm>

void VxAudioProcessorEditor::selectFilterSection(const int filterIndex)
{
    if (! eqeModuleLoaded)
        return;

    if (! juce::isPositiveAndBelow(filterIndex, getActiveFilterCount()))
        return;

    updateSectionStates();
    resized();

    if (auto* section = filterSections[static_cast<size_t>(filterIndex)].get())
    {
        juce::ignoreUnused(section);
        const auto sectionBounds = getFilterSectionBounds(filterIndex);
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

    refreshSpeAnalyserResponse();
}

juce::Rectangle<int> VxAudioProcessorEditor::getFilterSectionBounds(const int filterIndex) const
{
    if (! juce::isPositiveAndBelow(filterIndex, getActiveFilterCount()))
        return {};

    const auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

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
    includeVisibleComponent(section->placeControl.get());
    includeVisibleComponent(section->slopeControl.get());
    includeVisibleComponent(section->frequencyControl.get());
    includeVisibleComponent(section->bandwidthControl.get());
    includeVisibleComponent(section->gainControl.get());
    includeVisibleComponent(section->bypassButton.get());

    return bounds;
}

void VxAudioProcessorEditor::moveFilterSection(const int sourceIndex, const int destinationIndex)
{
    if (sourceIndex == destinationIndex)
        return;

    const auto activeCount = getActiveFilterCount();

    if (! juce::isPositiveAndBelow(sourceIndex, activeCount)
        || ! juce::isPositiveAndBelow(destinationIndex, activeCount))
        return;

    std::swap(filterDisplayOrder[static_cast<size_t>(sourceIndex)],
              filterDisplayOrder[static_cast<size_t>(destinationIndex)]);


    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::enforceSingleExpandedFilterSection(const int preferredFilterIndex)
{
    const auto activeCount = getActiveFilterCount();
    const auto targetFilterIndex = juce::isPositiveAndBelow(preferredFilterIndex, activeCount) ? preferredFilterIndex : -1;

    for (int filterIndex = 0; filterIndex < static_cast<int>(filterSections.size()); ++filterIndex)
    {
        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            continue;

        section->expanded = filterIndex == targetFilterIndex;
    }
}

void VxAudioProcessorEditor::normalizeSlopeForType(const int filterIndex)
{
    if (! juce::isPositiveAndBelow(filterIndex, static_cast<int>(filterSections.size())))
        return;

    auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

    if (filterSection == nullptr || filterSection->getFilterType() != EqeModuleProcessor::FilterType::bell)
        return;

    const auto selectedChoiceIndex = filterSection->slopeControl->getSelectedChoiceIndex();

    if (selectedChoiceIndex != 0)
        return;

    const juce::ScopedValueSetter<bool> suppressHandlers(suppressFilterSectionValueChangeHandlers, true);
    filterSection->slopeControl->setSelectedChoiceIndex(
        EqeModuleProcessor::getBellSlopeChoiceIndexForValue(EqeModuleProcessor::fixedSlopeDbPerOct),
        true);
}

namespace
{
struct FilterSortKey
{
    int index = 0;
    int place = 0;
    double frequency = 0.0;
};
}

void VxAudioProcessorEditor::sortFilterSectionsByPlace()
{
    const auto activeCount = getActiveFilterCount();

    if (activeCount <= 1)
        return;

    std::vector<FilterSortKey> sortKeys;
    sortKeys.reserve(static_cast<size_t>(activeCount));

    for (int filterIndex = 0; filterIndex < activeCount; ++filterIndex)
    {
        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            return;

        sortKeys.push_back({ filterIndex, juce::jlimit(0, 7, section->getPlace()), section->getFrequency() });
    }

    std::stable_sort(sortKeys.begin(),
                     sortKeys.end(),
                     [] (const FilterSortKey& left, const FilterSortKey& right)
                     {
                         return left.place < right.place;
                     });

    std::vector<int> orderedIndices;
    orderedIndices.reserve(sortKeys.size());

    for (const auto& key : sortKeys)
        orderedIndices.push_back(key.index);

    applyFilterSortOrder(orderedIndices);
}

void VxAudioProcessorEditor::sortFilterSectionsByFrequency()
{
    const auto activeCount = getActiveFilterCount();

    if (activeCount <= 1)
        return;

    std::vector<FilterSortKey> sortKeys;
    sortKeys.reserve(static_cast<size_t>(activeCount));

    for (int filterIndex = 0; filterIndex < activeCount; ++filterIndex)
    {
        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            return;

        sortKeys.push_back({ filterIndex, juce::jlimit(0, 7, section->getPlace()), section->getFrequency() });
    }

    std::stable_sort(sortKeys.begin(),
                     sortKeys.end(),
                     [] (const FilterSortKey& left, const FilterSortKey& right)
                     {
                         return left.frequency < right.frequency;
                     });

    std::vector<int> orderedIndices;
    orderedIndices.reserve(sortKeys.size());

    for (const auto& key : sortKeys)
        orderedIndices.push_back(key.index);

    applyFilterSortOrder(orderedIndices);
}

void VxAudioProcessorEditor::sortFilterSectionsByDuo()
{
    const auto activeCount = getActiveFilterCount();

    if (activeCount <= 1)
        return;

    std::vector<FilterSortKey> sortKeys;
    sortKeys.reserve(static_cast<size_t>(activeCount));

    for (int filterIndex = 0; filterIndex < activeCount; ++filterIndex)
    {
        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            return;

        sortKeys.push_back({ filterIndex, juce::jlimit(0, 7, section->getPlace()), section->getFrequency() });
    }

    std::stable_sort(sortKeys.begin(),
                     sortKeys.end(),
                     [] (const FilterSortKey& left, const FilterSortKey& right)
                     {
                         if (left.place != right.place)
                             return left.place < right.place;

                         return left.frequency < right.frequency;
                     });

    std::vector<int> orderedIndices;
    orderedIndices.reserve(sortKeys.size());

    for (const auto& key : sortKeys)
        orderedIndices.push_back(key.index);

    applyFilterSortOrder(orderedIndices);
}

void VxAudioProcessorEditor::applyFilterSortOrder(const std::vector<int>& orderedIndices)
{
    const auto activeCount = getActiveFilterCount();

    if (activeCount <= 1 || static_cast<int>(orderedIndices.size()) != activeCount)
        return;

    for (int destinationIndex = 0; destinationIndex < activeCount; ++destinationIndex)
        filterDisplayOrder[static_cast<size_t>(destinationIndex)] = orderedIndices[static_cast<size_t>(destinationIndex)];

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

int VxAudioProcessorEditor::getFilterIndexForOrderPosition(const int orderIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(orderIndex, getActiveFilterCount()))
        return -1;

    if (! juce::isPositiveAndBelow(orderIndex, static_cast<int>(filterDisplayOrder.size())))
        return -1;

    return filterDisplayOrder[static_cast<size_t>(orderIndex)];
}

int VxAudioProcessorEditor::getFilterOrderPositionForIndex(const int filterIndex) const noexcept
{
    const auto activeCount = getActiveFilterCount();

    for (int orderIndex = 0; orderIndex < activeCount; ++orderIndex)
    {
        if (filterDisplayOrder[static_cast<size_t>(orderIndex)] == filterIndex)
            return orderIndex;
    }

    return -1;
}
