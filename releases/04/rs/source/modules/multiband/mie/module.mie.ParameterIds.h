#pragma once

#include <JuceHeader.h>

#include <cstddef>

namespace mie::parameters
{
juce::String makeBandParameterId(size_t bandIndex, const char* suffix);
juce::String makeFullbandParameterId(const char* suffix);
juce::String makeBandGroupId(size_t bandIndex);
juce::String makeBandGroupName(size_t bandIndex);
juce::String makeFullbandGroupId();
juce::String makeFullbandGroupName();
juce::String makeSoloParameterId(size_t bandIndex);
juce::String makeActiveSplitCountParameterId();
} // namespace mie::parameters
