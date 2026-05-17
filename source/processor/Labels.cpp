#include "ProcessorSupport.h"

#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr std::array<float, 6> bellSlopeValues
{
    6.0f,
    12.0f,
    24.0f,
    48.0f,
    96.0f,
    96.1f
};
}

juce::StringArray EqeAudioProcessor::getBellSlopeChoices() noexcept
{
    return { "01", "02", "04", "08", "16", "++" };
}

float EqeAudioProcessor::getBellSlopeValueForChoiceIndex(const int choiceIndex) noexcept
{
    if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(bellSlopeValues.size())))
        return fixedSlopeDbPerOct;

    return bellSlopeValues[static_cast<size_t>(choiceIndex)];
}

int EqeAudioProcessor::getBellSlopeChoiceIndexForValue(const float slope) noexcept
{
    if (! std::isfinite(slope))
        return juce::jlimit(0, static_cast<int>(bellSlopeValues.size()) - 1, 3);

    if (slope > 96.0f)
        return static_cast<int>(bellSlopeValues.size()) - 1;

    auto bestIndex = 0;
    auto bestDistance = std::numeric_limits<float>::max();

    for (int index = 0; index < static_cast<int>(bellSlopeValues.size()) - 1; ++index)
    {
        const auto distance = std::abs(bellSlopeValues[static_cast<size_t>(index)] - slope);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

juce::String EqeAudioProcessor::getFilterTypeParamId(const int filterIndex)
{
    return "filter_" + juce::String(filterIndex + 1) + "_type";
}

juce::String EqeAudioProcessor::getFilterLrmsParamId(const int filterIndex)
{
    return "filter_" + juce::String(filterIndex + 1) + "_lrms";
}

juce::String EqeAudioProcessor::getBellFrequencyParamId(const int bellIndex)
{
    return makeFilterParameterId("frequency", bellIndex);
}

juce::String EqeAudioProcessor::getBellBandwidthParamId(const int bellIndex)
{
    return makeFilterParameterId("bandwidth", bellIndex);
}

juce::String EqeAudioProcessor::getBellSlopeParamId(const int bellIndex)
{
    return makeFilterParameterId("slope", bellIndex);
}

juce::String EqeAudioProcessor::getBellGainParamId(const int bellIndex)
{
    return makeFilterParameterId("gain", bellIndex);
}

juce::String EqeAudioProcessor::getBellBypassParamId(const int bellIndex)
{
    return makeFilterParameterId("bypass", bellIndex);
}

juce::String EqeAudioProcessor::getFilterHeaderText(const FilterType type, const int filterIndex)
{
    return juce::String::formatted("%02d %s",
                                   filterIndex + 1,
                                   filterTypeDisplayPrefix(type).toRawUTF8());
}

namespace
{
juce::String filterLrmsDisplayPrefix(const int choiceIndex)
{
    switch (juce::jlimit(0, 4, choiceIndex))
    {
        case 0: return "LR";
        case 1: return "LL";
        case 2: return "RR";
        case 3: return "MM";
        case 4: return "SS";
        default: return "LR";
    }
}
}

juce::String EqeAudioProcessor::getBellHeaderText(const int bellIndex, const int displayIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(bellIndex, static_cast<int>(filterTypeParams.size())))
        return {};

    const auto bandIndex = static_cast<size_t>(bellIndex);
    const auto filterType = getFilterTypeForBand(bandIndex);
    const auto lrmsChoice = filterLrmsParams[bandIndex] != nullptr
        ? juce::jlimit(0, 4, static_cast<int>(std::lround(filterLrmsParams[bandIndex]->load(std::memory_order_relaxed))))
        : 0;
    const auto frequency = bellFrequencyParams[bandIndex] != nullptr
        ? bellFrequencyParams[bandIndex]->load(std::memory_order_relaxed)
        : defaultTiltFrequency;

    return juce::String::formatted("%02d-%s-%s-%05d",
                                   displayIndex + 1,
                                   filterTypeDisplayPrefix(filterType).toRawUTF8(),
                                   filterLrmsDisplayPrefix(lrmsChoice).toRawUTF8(),
                                   static_cast<int>(std::lround(frequency)));
}

juce::StringArray getBellSlopeDisplayChoicesForType(const EqeAudioProcessor::FilterType type) noexcept
{
    if (type == EqeAudioProcessor::FilterType::bell)
        return { "OFF", "02", "04", "08", "16", "++" };

    return { "01", "02", "04", "08", "16", "++" };
}

EqeAudioProcessor::FilterType EqeAudioProcessor::filterTypeFromChoiceIndex(const int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case 0: return FilterType::lowCut;
        case 1: return FilterType::lowShelf;
        case 2: return FilterType::bell;
        case 3: return FilterType::tilt;
        case 4: return FilterType::highShelf;
        case 5: return FilterType::highCut;
        default: return FilterType::bell;
    }
}

int EqeAudioProcessor::choiceIndexFromFilterType(const FilterType type) noexcept
{
    switch (type)
    {
        case FilterType::lowCut: return 0;
        case FilterType::lowShelf: return 1;
        case FilterType::bell: return 2;
        case FilterType::tilt: return 3;
        case FilterType::highShelf: return 4;
        case FilterType::highCut: return 5;
        default: return 2;
    }
}
