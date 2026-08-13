#include "module.tls.DspCore.h"

namespace tls::dsp
{
void DspCore::prepare(const double sampleRate, const int, const int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto maxDelaySamples = msToSamples(depMaxCombinedDelayMs, currentSampleRate);
    auto requiredDelaySize = juce::jmax(1, maxDelaySamples + 2);
    depDelayBufferSize = 1;
    while (depDelayBufferSize < requiredDelaySize)
        depDelayBufferSize *= 2;
    depDelayLeft.assign(static_cast<size_t>(depDelayBufferSize), 0.0);
    depDelayRight.assign(static_cast<size_t>(depDelayBufferSize), 0.0);
    initialiseDepPhaseCoefficients();
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

int DspCore::getLatencySamples() const noexcept
{
    return derived.latencySamples;
}

int DspCore::getMaximumLatencySamples(const double sampleRate) noexcept
{
    const auto safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    return msToSamples(depMaxCombinedDelayMs, safeSampleRate) + depPhaseMid;
}
} // namespace tls::dsp
