#include "module.mie.DspCore.h"

namespace mie::dsp
{
int DspCore::msToSamples(const double ms, const double sampleRate) noexcept
{
    return static_cast<int>(std::floor(ms * sampleRate / 1000.0 + 0.5));
}

void DspCore::initialiseDepPhaseCoefficients()
{
    constexpr auto pi = juce::MathConstants<double>::pi;

    for (int index = 0; index < depPhaseTaps; ++index)
    {
        const auto k = index - depPhaseMid;
        auto coefficient = 0.0;

        if (k != 0 && (std::abs(k) & 1) != 0)
            coefficient = 2.0 / (pi * static_cast<double>(k));

        const auto window = 0.42
            - (0.5 * std::cos(2.0 * pi * index / (depPhaseTaps - 1)))
            + (0.08 * std::cos(4.0 * pi * index / (depPhaseTaps - 1)));
        depPhaseCoefficients[static_cast<size_t>(index)] = coefficient * window;
    }
}

void DspCore::clearState()
{
    depDelayWritePosition = 0;
    depPhaseWritePosition = 0;
    std::fill(depDelayLeft.begin(), depDelayLeft.end(), 0.0);
    std::fill(depDelayRight.begin(), depDelayRight.end(), 0.0);
    depPhaseLeft.fill(0.0);
    depPhaseRight.fill(0.0);
}

void DspCore::updateDerivedParameters()
{
    juce::ignoreUnused(currentSampleRate);
    derived.wideAmount = roundToJsfxStep(parameters.wide) / 100.0;
    derived.leftGain = dbToAmp(roundToJsfxStep(parameters.gainL));
    derived.rightGain = dbToAmp(roundToJsfxStep(parameters.gainR));
    derived.linkedGain = dbToAmp(roundToJsfxStep(parameters.gainLr));
    const auto panLPos = roundToJsfxStep(parameters.panL) * 0.01;
    const auto panRPos = roundToJsfxStep(parameters.panR) * 0.01;
    const auto panLTheta = (panLPos + 1.0) * juce::MathConstants<double>::pi * 0.25;
    const auto panRTheta = (panRPos + 1.0) * juce::MathConstants<double>::pi * 0.25;
    const auto law = dbToAmp(-juce::jlimit(0.0f, 6.0f, parameters.law));

    derived.gLL = std::cos(panLTheta);
    derived.gLR = std::sin(panLTheta);
    derived.gRL = std::cos(panRTheta);
    derived.gRR = std::sin(panRTheta);

    if (std::abs(panLPos) < 0.0000001)
    {
        derived.gLL *= law;
        derived.gLR *= law;
    }

    if (std::abs(panRPos) < 0.0000001)
    {
        derived.gRL *= law;
        derived.gRR *= law;
    }

    derived.shearAmount = roundToJsfxStep(parameters.shear) * 0.01;
    derived.midBalance = roundToJsfxStep(parameters.midBal) * 0.01;
    derived.sideBalance = roundToJsfxStep(parameters.sideBal) * 0.01;

    const auto ortTheta = roundToJsfxStep(parameters.ortDegRotation) * (juce::MathConstants<double>::pi / 180.0);
    const auto c = std::cos(ortTheta);
    const auto s = std::sin(ortTheta);
    derived.ortM11 = c;
    derived.ortM12 = parameters.ortFlipR ? s : -s;
    derived.ortM21 = s;
    derived.ortM22 = parameters.ortFlipR ? -c : c;

    derived.listenMode = -1;
    if (parameters.listenL)
        derived.listenMode = 0;
    if (parameters.listenR)
        derived.listenMode = 1;
    if (parameters.listenM)
        derived.listenMode = 2;
    if (parameters.listenS)
        derived.listenMode = 3;

    const auto lookaheadMs = juce::jlimit(0.0f, 200.0f, parameters.depBufferMs);
    const auto lookaheadSamples = msToSamples(lookaheadMs, currentSampleRate);
    derived.depLookaheadSamples = lookaheadSamples;

    if (lookaheadSamples == 0)
    {
        derived.depDelayLeftSamples = 0;
        derived.depDelayRightSamples = 0;
    }
    else
    {
        const auto maxDelaySamples = lookaheadSamples * 2;
        const auto stereoDelaySamples = msToSamples(juce::jlimit(-100.0f, 100.0f, parameters.depStereoMs), currentSampleRate);
        const auto rightDelaySamples = msToSamples(juce::jlimit(-100.0f, 100.0f, parameters.depRightMs), currentSampleRate);
        derived.depDelayLeftSamples = juce::jlimit(0, maxDelaySamples, lookaheadSamples + stereoDelaySamples);
        derived.depDelayRightSamples = juce::jlimit(0, maxDelaySamples, lookaheadSamples + stereoDelaySamples + rightDelaySamples);
    }

    derived.latencySamples = lookaheadSamples + depPhaseMid;

    const auto phaseL = juce::jlimit(-180.0f, 180.0f, parameters.depPhaseL) * (juce::MathConstants<double>::pi / 180.0);
    const auto phaseR = juce::jlimit(-180.0f, 180.0f, parameters.depPhaseR) * (juce::MathConstants<double>::pi / 180.0);
    derived.depPhaseCosL = std::cos(phaseL);
    derived.depPhaseSinL = std::sin(phaseL);
    derived.depPhaseCosR = std::cos(phaseR);
    derived.depPhaseSinR = std::sin(phaseR);
}
} // namespace mie::dsp
