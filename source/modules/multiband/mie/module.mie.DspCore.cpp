#include "module.mie.DspCore.h"

namespace mie::dsp
{
void DspCore::prepare(const double sampleRate, const int, const int)
{
    currentSampleRate = sampleRate;
    clearState();
    updateDerivedParameters();
}

void DspCore::reset()
{
    clearState();
    updateDerivedParameters();
}

void DspCore::setParameters(const Parameters& newParameters)
{
    parameters = newParameters;
    updateDerivedParameters();
}

void DspCore::beginBlock(const int numSamples)
{
    juce::ignoreUnused(numSamples);
}

int DspCore::getLatencySamples() const noexcept
{
    return 0;
}

int DspCore::getMaximumLatencySamples(const double) noexcept
{
    return 0;
}
} // namespace mie::dsp
