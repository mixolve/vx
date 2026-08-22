#include "ParameterIds.h"

namespace tls::parameters
{
juce::String makeCrossoverRangeParameterId(const size_t rangeIndex, const char* suffix)
{
    return "crossover" + juce::String(static_cast<int>(rangeIndex + 1)) + "_" + suffix;
}

juce::String makeCrossoverGroupId(const size_t rangeIndex)
{
    return "crossover" + juce::String(static_cast<int>(rangeIndex + 1));
}

juce::String makeCrossoverGroupName(const size_t rangeIndex)
{
    return "Crossover " + juce::String(static_cast<int>(rangeIndex + 1));
}

} // namespace tls::parameters
