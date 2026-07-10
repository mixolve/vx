#include "shell.EditorFilterSection.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"

VxAudioProcessorEditor::FilterSection::FilterSection(juce::AudioProcessorValueTreeState& state, const int bandIndexIn)
    : moveUpButton(std::make_unique<BoxTextButton>(uiGrey500)),
      header(std::make_unique<BoxTextButton>(uiAccent)),
      moveDownButton(std::make_unique<BoxTextButton>(uiGrey500)),
      typeControl(std::make_unique<ChoiceControl>(state,
                                                  EqeModuleProcessor::getFilterTypeParamId(bandIndexIn),
                                                  "TYPE",
                                                  std::vector<int> { 0, 1, 2, 3, 4, 5, 6 })),
      placeControl(std::make_unique<ChoiceControl>(state,
                                                  EqeModuleProcessor::getFilterPlaceParamId(bandIndexIn),
                                                  "PLACE",
                                                  std::vector<int> { 0, 1, 2, 3, 4, 5, 6, 7 })),
      slopeControl(std::make_unique<ChoiceControl>(state,
                                                   EqeModuleProcessor::getFilterSlopeParamId(bandIndexIn),
                                                   "ORDER",
                                                   std::vector<int> { 0, 1, 2, 3, 4, 5 })),
      frequencyControl(std::make_unique<ParameterControl>(state,
                                                          EqeModuleProcessor::getFilterFrequencyParamId(bandIndexIn),
                                                          "FREQ",
                                                          2)),
      bandwidthControl(std::make_unique<ParameterControl>(state,
                                                          EqeModuleProcessor::getFilterBandwidthParamId(bandIndexIn),
                                                          "BW",
                                                          2)),
      gainControl(std::make_unique<ParameterControl>(state,
                                                     EqeModuleProcessor::getFilterGainParamId(bandIndexIn),
                                                     "GAIN",
                                                     2)),
      bypassButton(std::make_unique<BoxTextButton>(uiAccent)),
      bandIndex(bandIndexIn)
{
    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterTypeParamId(bandIndexIn)))
        typeParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterPlaceParamId(bandIndexIn)))
        placeParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterSlopeParamId(bandIndexIn)))
        slopeParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterFrequencyParamId(bandIndexIn)))
        frequencyParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterBandwidthParamId(bandIndexIn)))
        bandwidthParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterGainParamId(bandIndexIn)))
        gainParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    moveUpButton->setButtonText({});
    moveUpButton->setTooltip("MOVE FILTER UP");
    moveUpButton->setArrowDirection(BoxTextButton::ArrowDirection::up);
    moveUpButton->setCancelClickOnLeave(true);

    header->setButtonText({});
    header->setTextJustification(juce::Justification::centred);
    header->setEqeFilterHeaderColouringEnabled(true);
    header->setClickingTogglesState(true);
    header->setCancelClickOnLeave(true);

    moveDownButton->setButtonText({});
    moveDownButton->setTooltip("MOVE FILTER DOWN");
    moveDownButton->setArrowDirection(BoxTextButton::ArrowDirection::down);
    moveDownButton->setCancelClickOnLeave(true);

    bypassButton->setButtonText("B");
    bypassButton->setTextJustification(juce::Justification::centred);
    bypassButton->setClickingTogglesState(true);
    bypassButton->setCancelClickOnLeave(true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state,
        EqeModuleProcessor::getFilterBypassParamId(bandIndexIn),
        *bypassButton);
    bypassButton->onClickWithModifiers = [this] (const juce::ModifierKeys& modifiers)
    {
        if (! modifiers.isCtrlDown())
            return false;

        auto* editor = bypassButton != nullptr
            ? bypassButton->findParentComponentOfClass<VxAudioProcessorEditor>()
            : nullptr;

        if (editor == nullptr)
            return false;

        const auto parameterId = EqeModuleProcessor::getFilterBypassParamId(bandIndex);

        if (auto* parameter = editor->findHostAssignableParameter(parameterId))
            return editor->handleHostSlotAssignRequest(parameterId, "B", parameter->getValue());

        return false;
    };

    lastFilterType = getFilterType();
    slopeControl->setChoices(getBellSlopeDisplayChoicesForType(lastFilterType));
    slopeControl->setChoiceEnabled(0, lastFilterType != FilterType::bell);
    updatePlaceChoicesForType(true);
}

void VxAudioProcessorEditor::FilterSection::detach() noexcept
{
    if (typeControl != nullptr)
        typeControl->detach();

    if (placeControl != nullptr)
        placeControl->detach();

    if (slopeControl != nullptr)
        slopeControl->detach();

    if (frequencyControl != nullptr)
        frequencyControl->detach();

    if (bandwidthControl != nullptr)
        bandwidthControl->detach();

    if (gainControl != nullptr)
        gainControl->detach();

    bypassAttachment.reset();
    typeParameter = nullptr;
    placeParameter = nullptr;
    slopeParameter = nullptr;
    frequencyParameter = nullptr;
    bandwidthParameter = nullptr;
    gainParameter = nullptr;
}

void VxAudioProcessorEditor::FilterSection::rebind(juce::AudioProcessorValueTreeState& state)
{
    typeControl->rebind(state);
    placeControl->rebind(state);
    slopeControl->rebind(state);
    frequencyControl->rebind(state);
    bandwidthControl->rebind(state);
    gainControl->rebind(state);

    typeParameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(EqeModuleProcessor::getFilterTypeParamId(bandIndex)));
    placeParameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(EqeModuleProcessor::getFilterPlaceParamId(bandIndex)));
    slopeParameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(EqeModuleProcessor::getFilterSlopeParamId(bandIndex)));
    frequencyParameter = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(EqeModuleProcessor::getFilterFrequencyParamId(bandIndex)));
    bandwidthParameter = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(EqeModuleProcessor::getFilterBandwidthParamId(bandIndex)));
    gainParameter = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(EqeModuleProcessor::getFilterGainParamId(bandIndex)));

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state,
        EqeModuleProcessor::getFilterBypassParamId(bandIndex),
        *bypassButton);

    lastFilterType = getFilterType();
    slopeControl->setChoices(getBellSlopeDisplayChoicesForType(lastFilterType));
    slopeControl->setChoiceEnabled(0, lastFilterType != FilterType::bell);
    updatePlaceChoicesForType(true);
}

VxAudioProcessorEditor::FilterSection::FilterType VxAudioProcessorEditor::FilterSection::getFilterType() const noexcept
{
    if (typeParameter == nullptr)
        return FilterType::bell;

    return EqeModuleProcessor::filterTypeFromChoiceIndex(typeParameter->getIndex());
}

int VxAudioProcessorEditor::FilterSection::getPlace() const noexcept
{
    return placeParameter != nullptr ? placeParameter->getIndex()
                                    : 0;
}

double VxAudioProcessorEditor::FilterSection::getFrequency() const noexcept
{
    return frequencyParameter != nullptr ? static_cast<double>(frequencyParameter->get())
                                         : 0.0;
}

bool VxAudioProcessorEditor::FilterSection::isBandwidthInactiveAtCurrentSlope() const noexcept
{
    const auto filterType = getFilterType();
    if (filterType == FilterType::volume)
        return true;

    if (filterType == FilterType::bell)
        return slopeParameter != nullptr && slopeParameter->getIndex() == 0;

    const auto slope = slopeParameter != nullptr
        ? EqeModuleProcessor::getBellSlopeValueForChoiceIndex(slopeParameter->getIndex())
        : EqeModuleProcessor::fixedSlopeDbPerOct;

    if (filterType == FilterType::tilt)
        return true;

    return filterType != FilterType::bell
        && (slope <= 6.05f || slope > 96.0f);
}

bool VxAudioProcessorEditor::FilterSection::isSlopeInactive() const noexcept
{
    const auto filterType = getFilterType();
    return filterType == FilterType::tilt
        || filterType == FilterType::volume;
}

bool VxAudioProcessorEditor::FilterSection::isGainInactive() const noexcept
{
    const auto filterType = getFilterType();
    return filterType == FilterType::lowCut
        || filterType == FilterType::highCut;
}

void VxAudioProcessorEditor::FilterSection::setGainDisplaysDegrees(const bool shouldDisplayDegrees)
{
    if (gainControl == nullptr)
        return;

    if (gainDisplaysDegrees == shouldDisplayDegrees)
        return;

    gainDisplaysDegrees = shouldDisplayDegrees;

    if (gainDisplaysDegrees)
    {
        static constexpr auto degreesPerDb = 7.5;
        const auto formatDegrees = [] (const double value)
        {
            auto degrees = juce::jlimit(-180.0, 180.0, value * degreesPerDb);
            if (std::abs(degrees) < 0.005)
                degrees = 0.0;

            return juce::String::formatted("%.2f", degrees);
        };

        gainControl->setTitleText("DEG");
        gainControl->setValueTextTransform(
            formatDegrees,
            formatDegrees,
            [] (const juce::String& text)
            {
                const auto degrees = text.retainCharacters("0123456789+-.").getDoubleValue();
                return juce::jlimit(-180.0, 180.0, degrees) / degreesPerDb;
            });

        if (gainControl->getValue() > 24.0)
            gainControl->setValue(24.0, true);
        else if (gainControl->getValue() < -24.0)
            gainControl->setValue(-24.0, true);
    }
    else
    {
        gainControl->setTitleText("GAIN");
        gainControl->clearValueTextTransform();
    }
}

void VxAudioProcessorEditor::FilterSection::updatePlaceChoicesForType(const bool normalizeSelection)
{
    if (placeControl == nullptr)
        return;

    const auto phasePlaceAllowed = ! isCutFilterType(getFilterType());
    placeControl->setChoiceEnabled(5, phasePlaceAllowed);
    placeControl->setChoiceEnabled(6, phasePlaceAllowed);
    placeControl->setChoiceEnabled(7, phasePlaceAllowed);

    if (normalizeSelection && ! phasePlaceAllowed && isPhasePlaceChoice(getPlace()))
        placeControl->setSelectedChoiceIndex(0, true);
}

void VxAudioProcessorEditor::FilterSection::setStoredValues(const FilterType type,
                                                           const double frequency,
                                                           const double bandwidth,
                                                           const double slope,
                                                           const int place,
                                                           const bool isCustom) noexcept
{
    const auto index = static_cast<size_t>(EqeModuleProcessor::choiceIndexFromFilterType(type));
    storedFrequencies[index] = frequency;
    storedBandwidths[index] = bandwidth;
    storedSlopes[index] = slope;
    storedPlace[index] = isCutFilterType(type) && isPhasePlaceChoice(place) ? 0 : place;
    storedValuesCustom[index] = isCustom;
}

int VxAudioProcessorEditor::FilterSection::getStoredPlace(const FilterType type) const noexcept
{
    return storedPlace[static_cast<size_t>(EqeModuleProcessor::choiceIndexFromFilterType(type))];
}

void VxAudioProcessorEditor::FilterSection::captureCurrentValuesForType(const FilterType type,
                                                                        const bool markCustom) noexcept
{
    if (suppressStoredValueCapture)
        return;

    if (frequencyParameter == nullptr || bandwidthParameter == nullptr || slopeParameter == nullptr)
        return;

    setStoredValues(type,
                    frequencyParameter->get(),
                    bandwidthParameter->get(),
                    EqeModuleProcessor::getBellSlopeValueForChoiceIndex(slopeParameter->getIndex()),
                    placeParameter != nullptr ? placeParameter->getIndex() : getStoredPlace(type),
                    markCustom);
}

void VxAudioProcessorEditor::FilterSection::captureCurrentValuesForCurrentType(const bool markCustom) noexcept
{
    captureCurrentValuesForType(getFilterType(), markCustom);
    lastFilterType = getFilterType();
}

void VxAudioProcessorEditor::FilterSection::copyStoredValuesFrom(const FilterSection& other) noexcept
{
    storedFrequencies = other.storedFrequencies;
    storedBandwidths = other.storedBandwidths;
    storedSlopes = other.storedSlopes;
    storedPlace = other.storedPlace;
    storedValuesCustom = other.storedValuesCustom;
    lastFilterType = other.lastFilterType;
    expanded = other.expanded;
}
