#include "module.mie.DspCore.h"

namespace mie::dsp
{
void DspCore::clearState()
{
}

void DspCore::updateDerivedParameters()
{
    juce::ignoreUnused(currentSampleRate);
    derived.wideAmount = roundToJsfxStep(parameters.wide) / 100.0;
    derived.leftGain = dbToAmp(roundToJsfxStep(parameters.gainL));
    derived.rightGain = dbToAmp(roundToJsfxStep(parameters.gainR));
    derived.linkedGain = dbToAmp(roundToJsfxStep(parameters.gainLr));
}
} // namespace mie::dsp
