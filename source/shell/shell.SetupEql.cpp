#include "shell.EditorFilterSection.h"
#include "../modules/eql/module.eql.ProcessorSupport.h"

#include <utility>

void AvaAudioProcessorEditor::setupEqlControls(juce::AudioProcessorValueTreeState& initialEqlState)
{
    for (int filterIndex = 0; filterIndex < AvaAudioProcessor::maxEqlFilterCount; ++filterIndex)
    {
        auto section = std::make_unique<FilterSection>(initialEqlState, filterIndex);
        for (const auto filterType : AvaAudioProcessor::filterTypePresetOrder)
        {
            section->setStoredValues(filterType,
                                     defaultFilterFrequency(),
                                     defaultFilterBandwidth(),
                                     defaultFilterSlope(),
                                     0,
                                     false);
        }
        section->captureCurrentValuesForCurrentType(true);
        section->moveUpButton->onClick = [this, filterIndex]
        {
            const auto orderPosition = getFilterOrderPositionForIndex(filterIndex);

            if (orderPosition > 0)
                moveFilterSection(orderPosition, orderPosition - 1);

            clearKeyboardFocus(*this);
        };
        section->moveDownButton->onClick = [this, filterIndex]
        {
            const auto orderPosition = getFilterOrderPositionForIndex(filterIndex);

            if (orderPosition >= 0 && orderPosition + 1 < getActiveFilterCount())
                moveFilterSection(orderPosition, orderPosition + 1);

            clearKeyboardFocus(*this);
        };
        section->placeControl->onTitleClick = [this, filterIndex]
        {
            auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

            if (filterSection == nullptr)
                return;

            filterSection->placeControl->setSelectedChoiceIndex(0, true);
        };
        section->frequencyControl->onTitleClick = [this, filterIndex]
        {
            auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

            if (filterSection == nullptr)
                return;

            filterSection->frequencyControl->setValue(defaultFilterFrequency(), true);
        };
        section->bandwidthControl->onTitleClick = [this, filterIndex]
        {
            auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

            if (filterSection == nullptr)
                return;

            filterSection->bandwidthControl->setValue(defaultFilterBandwidth(), true);
        };
        section->slopeControl->onTitleClick = [this, filterIndex]
        {
            auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

            if (filterSection == nullptr)
                return;

            filterSection->slopeControl->setSelectedChoiceIndex(
                EqlModuleProcessor::getBellSlopeChoiceIndexForValue(defaultFilterSlope()),
                true);
        };
        section->gainControl->onTitleClick = [this, filterIndex]
        {
            auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

            if (filterSection == nullptr)
                return;

            filterSection->gainControl->setValue(0.0, true);
        };
        section->typeControl->onValueChanged = [this, filterIndex]
        {
            if (suppressFilterSectionValueChangeHandlers)
                return;

            auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

            if (filterSection == nullptr)
                return;

            const auto newType = filterSection->getFilterType();
            filterSection->lastFilterType = newType;
            filterSection->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(newType));
            filterSection->slopeControl->setChoiceEnabled(0, newType != EqlModuleProcessor::FilterType::bell);
            filterSection->updatePlaceChoicesForType(true);
            filterSection->captureCurrentValuesForCurrentType();
            updateSectionStates();
            resized();
        };
        section->frequencyControl->onValueChanged = [this, filterIndex]
        {
            if (suppressFilterSectionValueChangeHandlers)
                return;

            if (auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get())
            {
                filterSection->captureCurrentValuesForCurrentType();

                updateSectionStates();
            }
        };
        section->placeControl->onValueChanged = [this, filterIndex]
        {
            if (suppressFilterSectionValueChangeHandlers)
                return;

            if (auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get())
            {
                filterSection->captureCurrentValuesForCurrentType();
                updateSectionStates();
            }
        };
        section->bandwidthControl->onValueChanged = [this, filterIndex]
        {
            if (suppressFilterSectionValueChangeHandlers)
                return;

            if (auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get())
                filterSection->captureCurrentValuesForCurrentType();

            updateSectionStates();
        };
        section->slopeControl->onValueChanged = [this, filterIndex]
        {
            if (suppressFilterSectionValueChangeHandlers)
                return;

            normalizeSlopeForType(filterIndex);

            if (auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get())
                filterSection->captureCurrentValuesForCurrentType();

            updateSectionStates();
        };
        section->gainControl->onValueChanged = [this]
        {
            if (suppressFilterSectionValueChangeHandlers)
                return;

            updateSectionStates();
        };
        section->header->onClick = [this, filterIndex]
        {
            auto* filterSection = filterSections[static_cast<size_t>(filterIndex)].get();

            if (filterSection == nullptr)
                return;

            enforceSingleExpandedFilterSection(filterSection->expanded ? -1 : filterIndex);
            storeEditorStateToValueTree();
            updateSectionStates();
            resized();
            clearKeyboardFocus(*this);
        };
        section->bypassButton->onClick = [this]
        {
            updateSectionStates();
            clearKeyboardFocus(*this);
        };
        section->bypassButton->setLongPressAction([this, filterIndex]
        {
            const juce::ScopedValueSetter<bool> suppressHandlers(suppressFilterSectionValueChangeHandlers, true);
            auto* eqlProcessor = getActiveEqlProcessor();
            const auto previousCount = getActiveFilterCount();

            if (eqlProcessor != nullptr && eqlProcessor->removeFilter(filterIndex))
            {
                removeFilterSectionStoredValues(filterIndex, previousCount);
                enforceSingleExpandedFilterSection();

                storeEditorStateToValueTree();

                updateSectionStates();
                resized();
            }

            clearKeyboardFocus(*this);
        }, 500, "D");

        filterContent.addAndMakeVisible(*section->moveUpButton);
        filterContent.addAndMakeVisible(*section->header);
        filterContent.addAndMakeVisible(*section->moveDownButton);
        filterContent.addAndMakeVisible(*section->typeControl);
        filterContent.addAndMakeVisible(*section->placeControl);
        filterContent.addAndMakeVisible(*section->frequencyControl);
        filterContent.addAndMakeVisible(*section->bandwidthControl);
        filterContent.addAndMakeVisible(*section->slopeControl);
        filterContent.addAndMakeVisible(*section->gainControl);
        filterContent.addAndMakeVisible(*section->bypassButton);
        filterSections[static_cast<size_t>(filterIndex)] = std::move(section);

        normalizeSlopeForType(filterIndex);
    }

    addFilterButton = std::make_unique<BoxTextButton>(uiGrey500);
    addFilterButton->setButtonText("AD");
    addFilterButton->setTooltip("ADD FILTER");
    addFilterButton->onClick = [this]
    {
        const juce::ScopedValueSetter<bool> suppressHandlers(suppressFilterSectionValueChangeHandlers, true);
        auto* eqlProcessor = getActiveEqlProcessor();

        if (eqlProcessor != nullptr && eqlProcessor->addFilter())
        {
            const auto newFilterIndex = getActiveFilterCount() - 1;
            filterDisplayOrder[static_cast<size_t>(newFilterIndex)] = newFilterIndex;
            resetFilterSectionStoredValues(getActiveFilterCount() - 1);
            enforceSingleExpandedFilterSection(newFilterIndex);
            storeEditorStateToValueTree();
            selectFilterSection(newFilterIndex);
        }

        clearKeyboardFocus(*this);
    };
    addFilterButton->setLongPressAction([this]
    {
        clearAllFilters();
        clearKeyboardFocus(*this);
    }, 500, "SURE?");
    addAndMakeVisible(*addFilterButton);
}
