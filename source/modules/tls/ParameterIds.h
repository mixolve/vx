#pragma once

#include <JuceHeader.h>

#include <cstddef>

namespace tls::parameters
{
juce::String makeCrossoverRangeParameterId(size_t rangeIndex, const char* suffix);
juce::String makeCrossoverGroupId(size_t rangeIndex);
juce::String makeCrossoverGroupName(size_t rangeIndex);
} // namespace tls::parameters
