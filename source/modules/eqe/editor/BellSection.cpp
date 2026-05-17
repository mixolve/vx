#include "EditorBellSection.h"
#include "ProcessorSupport.h"

EqeAudioProcessorEditor::BellSection::BellSection(juce::AudioProcessorValueTreeState& state, const int bandIndex)
    : moveUpButton(std::make_unique<BoxTextButton>(uiGrey500)),
      header(std::make_unique<BoxTextButton>(uiAccent)),
      moveDownButton(std::make_unique<BoxTextButton>(uiGrey500)),
      typeControl(std::make_unique<ChoiceControl>(state,
                                                  EqeAudioProcessor::getFilterTypeParamId(bandIndex),
                                                  "TYPE",
                                                  std::vector<int> { 0, 1, 2, 3, 4, 5 })),
      lrmsControl(std::make_unique<ChoiceControl>(state,
                                                  EqeAudioProcessor::getFilterLrmsParamId(bandIndex),
                                                  "PLACE",
                                                  std::vector<int> { 0, 1, 2, 3, 4 })),
      slopeControl(std::make_unique<ChoiceControl>(state,
                                                   EqeAudioProcessor::getBellSlopeParamId(bandIndex),
                                                                                                     "ORDER",
                                                                                                     std::vector<int> { 0, 1, 2, 3, 4, 5 })),
      frequencyControl(std::make_unique<ParameterControl>(state,
                                                          EqeAudioProcessor::getBellFrequencyParamId(bandIndex),
                                                          "FREQ",
                                                          2)),
      bandwidthControl(std::make_unique<ParameterControl>(state,
                                                  EqeAudioProcessor::getBellBandwidthParamId(bandIndex),
                                                  "BW",
                                                  2)),
      gainControl(std::make_unique<ParameterControl>(state,
                                                     EqeAudioProcessor::getBellGainParamId(bandIndex),
                                                     "GAIN",
                                                     2)),
      bypassButton(std::make_unique<BoxTextButton>(uiAccent)),
      deleteButton(std::make_unique<BoxTextButton>(uiAccent))
{
    if (auto* parameter = state.getParameter(EqeAudioProcessor::getFilterTypeParamId(bandIndex)))
        typeParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeAudioProcessor::getFilterLrmsParamId(bandIndex)))
        lrmsParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeAudioProcessor::getBellSlopeParamId(bandIndex)))
        slopeParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeAudioProcessor::getBellFrequencyParamId(bandIndex)))
        frequencyParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    if (auto* parameter = state.getParameter(EqeAudioProcessor::getBellBandwidthParamId(bandIndex)))
        bandwidthParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    if (auto* parameter = state.getParameter(EqeAudioProcessor::getBellGainParamId(bandIndex)))
        gainParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    moveUpButton->setButtonText({});
    moveUpButton->setTooltip("Move filter up");
    moveUpButton->setArrowDirection(BoxTextButton::ArrowDirection::up);
    moveUpButton->setCancelClickOnLeave(true);

    header->setButtonText({});
    header->setTextJustification(juce::Justification::centred);
    header->setClickingTogglesState(true);
    header->setCancelClickOnLeave(true);

    moveDownButton->setButtonText({});
    moveDownButton->setTooltip("Move filter down");
    moveDownButton->setArrowDirection(BoxTextButton::ArrowDirection::down);
    moveDownButton->setCancelClickOnLeave(true);

    bypassButton->setButtonText("BYPASS");
    bypassButton->setTextJustification(juce::Justification::centred);
    bypassButton->setClickingTogglesState(true);
    bypassButton->setCancelClickOnLeave(true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state,
        EqeAudioProcessor::getBellBypassParamId(bandIndex),
        *bypassButton);

    deleteButton->setButtonText("DELETE");
    deleteButton->setCancelClickOnLeave(true);
    lastFilterType = getFilterType();
    slopeControl->setChoices(getBellSlopeDisplayChoicesForType(lastFilterType));
    slopeControl->setChoiceEnabled(0, lastFilterType != EqeAudioProcessor::FilterType::bell);
}

EqeAudioProcessor::FilterType EqeAudioProcessorEditor::BellSection::getFilterType() const noexcept
{
    if (typeParameter == nullptr)
        return EqeAudioProcessor::FilterType::bell;

    return EqeAudioProcessor::filterTypeFromChoiceIndex(typeParameter->getIndex());
}

int EqeAudioProcessorEditor::BellSection::getPlace() const noexcept
{
    return lrmsParameter != nullptr ? lrmsParameter->getIndex()
                                    : 0;
}

double EqeAudioProcessorEditor::BellSection::getFrequency() const noexcept
{
    return frequencyParameter != nullptr ? static_cast<double>(frequencyParameter->get())
                                         : 0.0;
}

bool EqeAudioProcessorEditor::BellSection::isBandwidthInactiveAtCurrentSlope() const noexcept
{
    const auto filterType = getFilterType();
    if (filterType == EqeAudioProcessor::FilterType::bell)
        return slopeParameter != nullptr && slopeParameter->getIndex() == 0;

    const auto slope = slopeParameter != nullptr
        ? EqeAudioProcessor::getBellSlopeValueForChoiceIndex(slopeParameter->getIndex())
        : EqeAudioProcessor::fixedSlopeDbPerOct;

    if (filterType == EqeAudioProcessor::FilterType::tilt)
        return true;

    return filterType != EqeAudioProcessor::FilterType::bell
        && (slope <= 6.05f || slope > 96.0f);
}

bool EqeAudioProcessorEditor::BellSection::isSlopeInactive() const noexcept
{
    return getFilterType() == EqeAudioProcessor::FilterType::tilt;
}

bool EqeAudioProcessorEditor::BellSection::isGainInactive() const noexcept
{
    const auto filterType = getFilterType();
    return filterType == EqeAudioProcessor::FilterType::lowCut
        || filterType == EqeAudioProcessor::FilterType::highCut;
}

void EqeAudioProcessorEditor::BellSection::setStoredValues(const EqeAudioProcessor::FilterType type,
                                                           const double frequency,
                                                           const double bandwidth,
                                                           const double slope,
                                                           const int lrms,
                                                           const bool isCustom) noexcept
{
    const auto index = static_cast<size_t>(EqeAudioProcessor::choiceIndexFromFilterType(type));
    storedFrequencies[index] = frequency;
    storedBandwidths[index] = bandwidth;
    storedSlopes[index] = slope;
    storedLrms[index] = lrms;
    storedValuesCustom[index] = isCustom;
}

double EqeAudioProcessorEditor::BellSection::getStoredFrequency(const EqeAudioProcessor::FilterType type) const noexcept
{
    return storedFrequencies[static_cast<size_t>(EqeAudioProcessor::choiceIndexFromFilterType(type))];
}

double EqeAudioProcessorEditor::BellSection::getStoredBandwidth(const EqeAudioProcessor::FilterType type) const noexcept
{
    return storedBandwidths[static_cast<size_t>(EqeAudioProcessor::choiceIndexFromFilterType(type))];
}

double EqeAudioProcessorEditor::BellSection::getStoredSlope(const EqeAudioProcessor::FilterType type) const noexcept
{
    return storedSlopes[static_cast<size_t>(EqeAudioProcessor::choiceIndexFromFilterType(type))];
}

int EqeAudioProcessorEditor::BellSection::getStoredLrms(const EqeAudioProcessor::FilterType type) const noexcept
{
    return storedLrms[static_cast<size_t>(EqeAudioProcessor::choiceIndexFromFilterType(type))];
}

bool EqeAudioProcessorEditor::BellSection::isStoredValuesCustom(const EqeAudioProcessor::FilterType type) const noexcept
{
    return storedValuesCustom[static_cast<size_t>(EqeAudioProcessor::choiceIndexFromFilterType(type))];
}

void EqeAudioProcessorEditor::BellSection::captureCurrentValuesForType(const EqeAudioProcessor::FilterType type,
                                                                       const bool markCustom) noexcept
{
    if (suppressStoredValueCapture)
        return;

    if (frequencyParameter == nullptr || bandwidthParameter == nullptr || slopeParameter == nullptr)
        return;

    setStoredValues(type,
                    frequencyParameter->get(),
                    bandwidthParameter->get(),
                    EqeAudioProcessor::getBellSlopeValueForChoiceIndex(slopeParameter->getIndex()),
                    lrmsParameter != nullptr ? lrmsParameter->getIndex() : getStoredLrms(type),
                    markCustom);
}

void EqeAudioProcessorEditor::BellSection::captureCurrentValuesForCurrentType(const bool markCustom) noexcept
{
    captureCurrentValuesForType(getFilterType(), markCustom);
    lastFilterType = getFilterType();
}

void EqeAudioProcessorEditor::BellSection::copyStoredValuesFrom(const BellSection& other) noexcept
{
    storedFrequencies = other.storedFrequencies;
    storedBandwidths = other.storedBandwidths;
    storedSlopes = other.storedSlopes;
    storedLrms = other.storedLrms;
    storedValuesCustom = other.storedValuesCustom;
    lastFilterType = other.lastFilterType;
}
