#include "module.tls.ParameterIds.h"

namespace tls::parameters
{
juce::String makeBandParameterId(const size_t bandIndex, const char* suffix)
{
    return "band" + juce::String(static_cast<int>(bandIndex + 1)) + "_" + suffix;
}

juce::String makeFullbandParameterId(const char* suffix)
{
    return "fullband_" + juce::String(suffix);
}

juce::String makeBandGroupId(const size_t bandIndex)
{
    return "band" + juce::String(static_cast<int>(bandIndex + 1));
}

juce::String makeBandGroupName(const size_t bandIndex)
{
    return "Band " + juce::String(static_cast<int>(bandIndex + 1));
}

juce::String makeFullbandGroupId()
{
    return "fullband";
}

juce::String makeFullbandGroupName()
{
    return "Fullband";
}

juce::String makeSoloParameterId(const size_t bandIndex)
{
    return "soloBand" + juce::String(static_cast<int>(bandIndex + 1));
}

juce::String makeActiveSplitCountParameterId()
{
    return "fullband_activeXovers";
}
} // namespace tls::parameters
