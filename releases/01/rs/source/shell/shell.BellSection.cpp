#include "shell.EditorBellSection.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"

VxAudioProcessorEditor::BellSection::BellSection(juce::AudioProcessorValueTreeState& state, const int bandIndexIn)
    : moveUpButton(std::make_unique<BoxTextButton>(uiGrey500)),
      header(std::make_unique<BoxTextButton>(uiAccent)),
      moveDownButton(std::make_unique<BoxTextButton>(uiGrey500)),
      typeControl(std::make_unique<ChoiceControl>(state,
                                                  EqeModuleProcessor::getFilterTypeParamId(bandIndexIn),
                                                  "TYPE",
                                                  std::vector<int> { 0, 1, 2, 3, 4, 5 })),
      ttssControl(std::make_unique<ChoiceControl>(state,
                                                  EqeModuleProcessor::getFilterTtssParamId(bandIndexIn),
                                                  "TTSS",
                                                  std::vector<int> { 0, 1, 2 })),
      lrmsControl(std::make_unique<ChoiceControl>(state,
                                                  EqeModuleProcessor::getFilterLrmsParamId(bandIndexIn),
                                                  "PLACE",
                                                  std::vector<int> { 0, 1, 2, 3, 4 })),
      slopeControl(std::make_unique<ChoiceControl>(state,
                                                   EqeModuleProcessor::getBellSlopeParamId(bandIndexIn),
                                                                                                      "ORDER",
                                                                                                      std::vector<int> { 0, 1, 2, 3, 4, 5 })),
      frequencyControl(std::make_unique<ParameterControl>(state,
                                                          EqeModuleProcessor::getBellFrequencyParamId(bandIndexIn),
                                                          "FREQ",
                                                          2)),
      bandwidthControl(std::make_unique<ParameterControl>(state,
                                                  EqeModuleProcessor::getBellBandwidthParamId(bandIndexIn),
                                                  "BW",
                                                  2)),
      gainControl(std::make_unique<ParameterControl>(state,
                                                     EqeModuleProcessor::getBellGainParamId(bandIndexIn),
                                                     "GAIN",
                                                     2)),
      bypassButton(std::make_unique<BoxTextButton>(uiAccent)),
      deleteButton(std::make_unique<BoxTextButton>(uiAccent)),
      bandIndex(bandIndexIn)
{
    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterTypeParamId(bandIndexIn)))
        typeParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getFilterLrmsParamId(bandIndexIn)))
        lrmsParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getBellSlopeParamId(bandIndexIn)))
        slopeParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getBellFrequencyParamId(bandIndexIn)))
        frequencyParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getBellBandwidthParamId(bandIndexIn)))
        bandwidthParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);

    if (auto* parameter = state.getParameter(EqeModuleProcessor::getBellGainParamId(bandIndexIn)))
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
        EqeModuleProcessor::getBellBypassParamId(bandIndexIn),
        *bypassButton);

    deleteButton->setButtonText("DELETE");
    deleteButton->setCancelClickOnLeave(true);
    lastFilterType = getFilterType();
    slopeControl->setChoices(getBellSlopeDisplayChoicesForType(lastFilterType));
    slopeControl->setChoiceEnabled(0, lastFilterType != FilterType::bell);
}

void VxAudioProcessorEditor::BellSection::rebind(juce::AudioProcessorValueTreeState& state)
{
    typeControl->rebind(state);
    ttssControl->rebind(state);
    lrmsControl->rebind(state);
    slopeControl->rebind(state);
    frequencyControl->rebind(state);
    bandwidthControl->rebind(state);
    gainControl->rebind(state);

    typeParameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(EqeModuleProcessor::getFilterTypeParamId(bandIndex)));
    lrmsParameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(EqeModuleProcessor::getFilterLrmsParamId(bandIndex)));
    slopeParameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(EqeModuleProcessor::getBellSlopeParamId(bandIndex)));
    frequencyParameter = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(EqeModuleProcessor::getBellFrequencyParamId(bandIndex)));
    bandwidthParameter = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(EqeModuleProcessor::getBellBandwidthParamId(bandIndex)));
    gainParameter = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(EqeModuleProcessor::getBellGainParamId(bandIndex)));

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state,
        EqeModuleProcessor::getBellBypassParamId(bandIndex),
        *bypassButton);

    lastFilterType = getFilterType();
    slopeControl->setChoices(getBellSlopeDisplayChoicesForType(lastFilterType));
    slopeControl->setChoiceEnabled(0, lastFilterType != FilterType::bell);
}

VxAudioProcessorEditor::BellSection::FilterType VxAudioProcessorEditor::BellSection::getFilterType() const noexcept
{
    if (typeParameter == nullptr)
        return FilterType::bell;

    return EqeModuleProcessor::filterTypeFromChoiceIndex(typeParameter->getIndex());
}

int VxAudioProcessorEditor::BellSection::getPlace() const noexcept
{
    return lrmsParameter != nullptr ? lrmsParameter->getIndex()
                                    : 0;
}

double VxAudioProcessorEditor::BellSection::getFrequency() const noexcept
{
    return frequencyParameter != nullptr ? static_cast<double>(frequencyParameter->get())
                                         : 0.0;
}

bool VxAudioProcessorEditor::BellSection::isBandwidthInactiveAtCurrentSlope() const noexcept
{
    const auto filterType = getFilterType();
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

bool VxAudioProcessorEditor::BellSection::isSlopeInactive() const noexcept
{
    return getFilterType() == FilterType::tilt;
}

bool VxAudioProcessorEditor::BellSection::isGainInactive() const noexcept
{
    const auto filterType = getFilterType();
    return filterType == FilterType::lowCut
        || filterType == FilterType::highCut;
}

void VxAudioProcessorEditor::BellSection::setStoredValues(const FilterType type,
                                                           const double frequency,
                                                           const double bandwidth,
                                                           const double slope,
                                                           const int lrms,
                                                           const bool isCustom) noexcept
{
    const auto index = static_cast<size_t>(EqeModuleProcessor::choiceIndexFromFilterType(type));
    storedFrequencies[index] = frequency;
    storedBandwidths[index] = bandwidth;
    storedSlopes[index] = slope;
    storedLrms[index] = lrms;
    storedValuesCustom[index] = isCustom;
}

int VxAudioProcessorEditor::BellSection::getStoredLrms(const FilterType type) const noexcept
{
    return storedLrms[static_cast<size_t>(EqeModuleProcessor::choiceIndexFromFilterType(type))];
}

void VxAudioProcessorEditor::BellSection::captureCurrentValuesForType(const FilterType type,
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
                    lrmsParameter != nullptr ? lrmsParameter->getIndex() : getStoredLrms(type),
                    markCustom);
}

void VxAudioProcessorEditor::BellSection::captureCurrentValuesForCurrentType(const bool markCustom) noexcept
{
    captureCurrentValuesForType(getFilterType(), markCustom);
    lastFilterType = getFilterType();
}

void VxAudioProcessorEditor::BellSection::copyStoredValuesFrom(const BellSection& other) noexcept
{
    storedFrequencies = other.storedFrequencies;
    storedBandwidths = other.storedBandwidths;
    storedSlopes = other.storedSlopes;
    storedLrms = other.storedLrms;
    storedValuesCustom = other.storedValuesCustom;
    lastFilterType = other.lastFilterType;
}
