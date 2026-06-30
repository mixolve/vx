#include "module.eqe.ProcessorSupport.h"

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

juce::StringArray EqeModuleProcessor::getBellSlopeChoices() noexcept
{
    return { "01", "02", "04", "08", "16", "++" };
}

float EqeModuleProcessor::getBellSlopeValueForChoiceIndex(const int choiceIndex) noexcept
{
    if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(bellSlopeValues.size())))
        return fixedSlopeDbPerOct;

    return bellSlopeValues[static_cast<size_t>(choiceIndex)];
}

int EqeModuleProcessor::getBellSlopeChoiceIndexForValue(const float slope) noexcept
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

juce::String EqeModuleProcessor::getFilterTypeParamId(const int filterIndex)
{
    return "filter_" + juce::String(filterIndex + 1) + "_type";
}

juce::String EqeModuleProcessor::getFilterLrmsParamId(const int filterIndex)
{
    return "filter_" + juce::String(filterIndex + 1) + "_lrms";
}

juce::String EqeModuleProcessor::getFilterFrequencyParamId(const int filterIndex)
{
    return makeFilterParameterId("frequency", filterIndex);
}

juce::String EqeModuleProcessor::getFilterBandwidthParamId(const int filterIndex)
{
    return makeFilterParameterId("bandwidth", filterIndex);
}

juce::String EqeModuleProcessor::getFilterSlopeParamId(const int filterIndex)
{
    return makeFilterParameterId("slope", filterIndex);
}

juce::String EqeModuleProcessor::getFilterGainParamId(const int filterIndex)
{
    return makeFilterParameterId("gain", filterIndex);
}

juce::String EqeModuleProcessor::getFilterBypassParamId(const int filterIndex)
{
    return makeFilterParameterId("bypass", filterIndex);
}

juce::String EqeModuleProcessor::getFilterHeaderText(const FilterType type, const int filterIndex)
{
    return juce::String::formatted("%02d %s",
                                   filterIndex + 1,
                                   filterTypeDisplayPrefix(type).toRawUTF8());
}

namespace
{
juce::String filterLrmsDisplayPrefix(const int choiceIndex)
{
    switch (juce::jlimit(0, 7, choiceIndex))
    {
        case 0: return "LR";
        case 1: return "LL";
        case 2: return "RR";
        case 3: return "MM";
        case 4: return "SS";
        case 5: return "PHS";
        case 6: return "PHL";
        case 7: return "PHR";
        default: return "LR";
    }
}

}

juce::String EqeModuleProcessor::getFilterHeaderText(const int filterIndex, const int displayIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(filterIndex, static_cast<int>(filterTypeParams.size())))
        return {};

    const auto bandIndex = static_cast<size_t>(filterIndex);
    const auto filterType = getFilterTypeForBand(bandIndex);
    const auto lrmsChoice = filterLrmsParams[bandIndex] != nullptr
        ? juce::jlimit(0, 7, static_cast<int>(std::lround(filterLrmsParams[bandIndex]->load(std::memory_order_relaxed))))
        : 0;
    const auto frequency = filterFrequencyParams[bandIndex] != nullptr
        ? filterFrequencyParams[bandIndex]->load(std::memory_order_relaxed)
        : defaultTiltFrequency;

    if (filterType == FilterType::volume)
    {
        return juce::String::formatted("%02d-%s-%s",
                                       displayIndex + 1,
                                       filterTypeDisplayPrefix(filterType).toRawUTF8(),
                                       filterLrmsDisplayPrefix(lrmsChoice).toRawUTF8())
            + "-00000";
    }

    return juce::String::formatted("%02d-%s-%s-%05d",
                                   displayIndex + 1,
                                   filterTypeDisplayPrefix(filterType).toRawUTF8(),
                                   filterLrmsDisplayPrefix(lrmsChoice).toRawUTF8(),
                                   static_cast<int>(std::lround(frequency)));
}

juce::StringArray getBellSlopeDisplayChoicesForType(const EqeModuleProcessor::FilterType type) noexcept
{
    if (type == EqeModuleProcessor::FilterType::bell)
        return { "OFF", "02", "04", "08", "16", "++" };

    if (type == EqeModuleProcessor::FilterType::volume)
        return { "OFF", "OFF", "OFF", "OFF", "OFF", "OFF" };

    return { "01", "02", "04", "08", "16", "++" };
}

EqeModuleProcessor::FilterType EqeModuleProcessor::filterTypeFromChoiceIndex(const int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case 0: return FilterType::lowCut;
        case 1: return FilterType::lowShelf;
        case 2: return FilterType::bell;
        case 3: return FilterType::tilt;
        case 4: return FilterType::highShelf;
        case 5: return FilterType::highCut;
        case 6: return FilterType::volume;
        default: return FilterType::bell;
    }
}

int EqeModuleProcessor::choiceIndexFromFilterType(const FilterType type) noexcept
{
    switch (type)
    {
        case FilterType::lowCut: return 0;
        case FilterType::lowShelf: return 1;
        case FilterType::bell: return 2;
        case FilterType::tilt: return 3;
        case FilterType::highShelf: return 4;
        case FilterType::highCut: return 5;
        case FilterType::volume: return 6;
        default: return 2;
    }
}
