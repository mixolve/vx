#include "DspCore.h"

#include <algorithm>
#include <cmath>

namespace dyn::dsp
{
namespace
{
constexpr double maxLookaheadMs = 24.0;
} // namespace

void DspCore::resizeLookaheadBuffers()
{
    maxBuf = std::max(1, static_cast<int>(std::ceil(currentSampleRate * maxLookaheadMs * 0.001)) + 2);

    for (size_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
        dmBase[branchIndex].assign(static_cast<size_t>(maxBuf), 0.0);

    dryL.assign(static_cast<size_t>(maxBuf), 0.0);
    dryR.assign(static_cast<size_t>(maxBuf), 0.0);
}

void DspCore::clearState()
{
    for (size_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
        std::fill(dmBase[branchIndex].begin(), dmBase[branchIndex].end(), 0.0);

    std::fill(dryL.begin(), dryL.end(), 0.0);
    std::fill(dryR.begin(), dryR.end(), 0.0);

    envBase = { 0.0, 0.0, 0.0, 0.0 };
    baseGainState = { 1.0, 1.0, 1.0, 1.0 };
    cleanEnvPeak = { 0.0, 0.0 };
    cleanGainState = { 1.0, 1.0 };
    cleanHalfPeak = { 0.0, 0.0 };
    cleanInputPositive = { true, true };
    cleanHoldSamples = { 0, 0 };
    baseReleaseStates = {};
    cleanReleaseStates = {};
    bufPos = 0;
    bufPosDry = 0;
}

void DspCore::updateDerivedParameters()
{
    const auto sampleRate = std::max(1.0, currentSampleRate);

    derived.thresholds[branchLeftUp] = dbToAmp(roundToParameterStep(parameters.leftUpThreshold));
    derived.thresholds[branchLeftDown] = dbToAmp(roundToParameterStep(parameters.leftDownThreshold));
    derived.thresholds[branchRightUp] = dbToAmp(roundToParameterStep(parameters.rightUpThreshold));
    derived.thresholds[branchRightDown] = dbToAmp(roundToParameterStep(parameters.rightDownThreshold));

    derived.tensions[branchLeftUp] = roundToParameterStep(parameters.leftUpTension);
    derived.tensions[branchLeftDown] = roundToParameterStep(parameters.leftDownTension);
    derived.tensions[branchRightUp] = roundToParameterStep(parameters.rightUpTension);
    derived.tensions[branchRightDown] = roundToParameterStep(parameters.rightDownTension);

    const auto relLuMs = roundToParameterStep(parameters.leftUpRelease);
    const auto relLdMs = roundToParameterStep(parameters.leftDownRelease);
    const auto relRuMs = roundToParameterStep(parameters.rightUpRelease);
    const auto relRdMs = roundToParameterStep(parameters.rightDownRelease);
    const auto toReleaseSamples = [sampleRate] (const double milliseconds)
    {
        return milliseconds <= 0.0 ? 0 : static_cast<int>(std::round(milliseconds * 0.001 * sampleRate));
    };
    derived.releaseSamples[branchLeftUp] = toReleaseSamples(relLuMs);
    derived.releaseSamples[branchLeftDown] = toReleaseSamples(relLdMs);
    derived.releaseSamples[branchRightUp] = toReleaseSamples(relRuMs);
    derived.releaseSamples[branchRightDown] = toReleaseSamples(relRdMs);
    derived.peakHoldSamples = toReleaseSamples(roundToParameterStep(parameters.peakHoldMs));

    derived.branchOutGains[branchLeftUp] = dbToAmp(roundToParameterStep(parameters.leftUpOutput));
    derived.branchOutGains[branchLeftDown] = dbToAmp(roundToParameterStep(parameters.leftDownOutput));
    derived.branchOutGains[branchRightUp] = dbToAmp(roundToParameterStep(parameters.rightUpOutput));
    derived.branchOutGains[branchRightDown] = dbToAmp(roundToParameterStep(parameters.rightDownOutput));

    derived.morph = roundToParameterStep(parameters.morph) * 0.01;
    derived.clipKneeDb = 0.0;

    const auto lookaheadSamples = static_cast<int>(std::round(
        std::max(0.0, roundToParameterStep(parameters.lookaheadMs)) * 0.001 * sampleRate));

    derived.tensionFloor = dbToAmp(roundToParameterStep(parameters.tensionFloor));
    derived.tensionHysteresis = roundToParameterStep(parameters.tensionHysteresis) * 0.01;
    derived.releaseLogarithmic = parameters.releaseForm == 1;
    derived.releaseCurve = derived.releaseLogarithmic ? roundToParameterStep(parameters.releaseCurve) * 0.01 : 0.0;
    derived.delta = parameters.delta;

    derived.lookaheadSamples = juce::jlimit(
        0,
        std::max(0, maxBuf - 2),
        lookaheadSamples);
    derived.bufferSize = derived.lookaheadSamples + 1;
    derived.dryBufferSize = derived.lookaheadSamples + 1;
    derived.latencySamples = derived.lookaheadSamples;

    bufPos = wrapIndex(bufPos, derived.bufferSize);
    bufPosDry = wrapIndex(bufPosDry, derived.dryBufferSize);
}
} // namespace dyn::dsp
