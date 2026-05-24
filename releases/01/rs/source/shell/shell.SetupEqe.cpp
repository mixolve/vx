#include "shell.EditorBellSection.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"

#include <utility>

void VxAudioProcessorEditor::setupEqeControls(juce::AudioProcessorValueTreeState& initialEqeState)
{
    eqeBypassButton = std::make_unique<BoxTextButton>(uiAccent);
    eqeBypassButton->setButtonText("BYPASS");
    eqeBypassButton->setTextJustification(juce::Justification::centred);
    eqeBypassButton->setClickingTogglesState(true);
    eqeBypassAttachment = std::make_unique<ButtonAttachment>(initialEqeState,
                                                             EqeModuleProcessor::paramBypassId,
                                                             *eqeBypassButton);
    eqeBypassButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*eqeBypassButton);

    eqeBypassWithGainButton = std::make_unique<BoxTextButton>(uiAccent);
    eqeBypassWithGainButton->setButtonText("BYPASS.WT-GAIN");
    eqeBypassWithGainButton->setTextJustification(juce::Justification::centred);
    eqeBypassWithGainButton->setClickingTogglesState(true);
    eqeBypassWithGainAttachment = std::make_unique<ButtonAttachment>(initialEqeState,
                                                                     EqeModuleProcessor::paramBypassWithGainId,
                                                                     *eqeBypassWithGainButton);
    eqeBypassWithGainButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*eqeBypassWithGainButton);

    for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        auto section = std::make_unique<BellSection>(initialEqeState, bellIndex);
        for (const auto filterType : VxAudioProcessor::filterTypePresetOrder)
        {
            section->setStoredValues(filterType,
                                     defaultFilterFrequencyForType(filterType),
                                     defaultFilterBandwidthForType(filterType),
                                     defaultFilterSlopeForType(filterType),
                                     0,
                                     false);
        }
        section->captureCurrentValuesForCurrentType(true);
        section->moveUpButton->onClick = [this, bellIndex]
        {
            const auto orderPosition = getBellOrderPositionForIndex(bellIndex);

            if (orderPosition > 0)
                moveBellSection(orderPosition, orderPosition - 1);

            clearKeyboardFocus(*this);
        };
        section->moveDownButton->onClick = [this, bellIndex]
        {
            const auto orderPosition = getBellOrderPositionForIndex(bellIndex);

            if (orderPosition >= 0 && orderPosition + 1 < getActiveBellCount())
                moveBellSection(orderPosition, orderPosition + 1);

            clearKeyboardFocus(*this);
        };
        section->typeControl->setTitleMouseEnabled(false);
        section->ttssControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            bellSection->ttssControl->setSelectedChoiceIndex(0, true);
        };
        section->lrmsControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            bellSection->lrmsControl->setSelectedChoiceIndex(0, true);
        };
        section->frequencyControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto filterType = bellSection->getFilterType();
            bellSection->frequencyControl->setValue(defaultFilterFrequencyForType(filterType), true);
        };
        section->bandwidthControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto filterType = bellSection->getFilterType();
            bellSection->bandwidthControl->setValue(defaultFilterBandwidthForType(filterType), true);
        };
        section->slopeControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto filterType = bellSection->getFilterType();
            bellSection->slopeControl->setSelectedChoiceIndex(
                EqeModuleProcessor::getBellSlopeChoiceIndexForValue(defaultFilterSlopeForType(filterType)),
                true);
        };
        section->gainControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            bellSection->gainControl->setValue(0.0, true);
        };
        section->typeControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto newType = bellSection->getFilterType();
            bellSection->lastFilterType = newType;
            bellSection->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(newType));
            bellSection->slopeControl->setChoiceEnabled(0, newType != EqeModuleProcessor::FilterType::bell);
            bellSection->captureCurrentValuesForCurrentType();
            updateSectionStates();
            resized();
        };
        section->frequencyControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
            {
                bellSection->captureCurrentValuesForCurrentType();

                updateSectionStates();
            }
        };
        section->lrmsControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
            {
                bellSection->captureCurrentValuesForCurrentType();
                updateSectionStates();
            }
        };
        section->ttssControl->onValueChanged = [this]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            updateSectionStates();
        };
        section->bandwidthControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
                bellSection->captureCurrentValuesForCurrentType();

            updateSectionStates();
        };
        section->slopeControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            normalizeSlopeForType(bellIndex);

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
                bellSection->captureCurrentValuesForCurrentType();

            updateSectionStates();
        };
        section->gainControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            juce::ignoreUnused(bellIndex);

            updateSectionStates();
        };
        section->header->onClick = [this, bellIndex]
        {
            openBellSection(bellIndex);
            clearKeyboardFocus(*this);
        };
        section->bypassButton->onClick = [this]
        {
            updateSectionStates();
            clearKeyboardFocus(*this);
        };
        section->deleteButton->onClick = [this, bellIndex]
        {
            const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);
            auto* eqeProcessor = getActiveEqeProcessor();
            const auto previousCount = getActiveBellCount();

            if (eqeProcessor != nullptr && eqeProcessor->removeBellFilter(bellIndex))
            {
                removeBellSectionStoredValues(bellIndex, previousCount);
                globalExpanded = false;
                speMainExpanded = false;
                filtersExpanded = true;
                presetsExpanded = false;
                visualizerExpanded = false;

                const auto newCount = getActiveBellCount();

                if (newCount <= 0)
                {
                    expandedBellIndex = -1;
                }
                else if (expandedBellIndex > bellIndex)
                {
                    --expandedBellIndex;
                }
                else if (expandedBellIndex == bellIndex)
                {
                    expandedBellIndex = juce::jlimit(0, newCount - 1, bellIndex);
                }

                storeEditorStateToValueTree();

                updateSectionStates();
                resized();
            }

            clearKeyboardFocus(*this);
        };

        filterContent.addAndMakeVisible(*section->moveUpButton);
        filterContent.addAndMakeVisible(*section->header);
        filterContent.addAndMakeVisible(*section->moveDownButton);
        filterContent.addAndMakeVisible(*section->typeControl);
        filterContent.addAndMakeVisible(*section->ttssControl);
        filterContent.addAndMakeVisible(*section->lrmsControl);
        filterContent.addAndMakeVisible(*section->frequencyControl);
        filterContent.addAndMakeVisible(*section->bandwidthControl);
        filterContent.addAndMakeVisible(*section->slopeControl);
        filterContent.addAndMakeVisible(*section->gainControl);
        filterContent.addAndMakeVisible(*section->bypassButton);
        filterContent.addAndMakeVisible(*section->deleteButton);
        bellSections[static_cast<size_t>(bellIndex)] = std::move(section);

        normalizeSlopeForType(bellIndex);
    }

    addFilterButton = std::make_unique<BoxTextButton>(uiGrey500);
    addFilterButton->setButtonText("ADD");
    addFilterButton->onClick = [this]
    {
        const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);
        auto* eqeProcessor = getActiveEqeProcessor();

        if (eqeProcessor != nullptr && eqeProcessor->addBellFilter())
        {
            const auto newBellIndex = getActiveBellCount() - 1;
            bellDisplayOrder[static_cast<size_t>(newBellIndex)] = newBellIndex;
            resetBellSectionStoredValues(getActiveBellCount() - 1);
            globalExpanded = false;
            speMainExpanded = false;
            filtersExpanded = true;
            presetsExpanded = false;
            visualizerExpanded = false;
            expandedBellIndex = getActiveBellCount() - 1;
            storeEditorStateToValueTree();
            updateSectionStates();
            resized();
        }

        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*addFilterButton);
}
