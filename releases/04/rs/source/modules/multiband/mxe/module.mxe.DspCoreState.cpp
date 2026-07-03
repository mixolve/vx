#include "module.mxe.DspCore.h"

#include <algorithm>
#include <cmath>

namespace mxe::dsp
{
namespace
{
constexpr double maxLookaheadMs = 24.0;
constexpr double kneeRangeDb = 24.0;
} // namespace

void DspCore::resizeLookaheadBuffers()
{
    maxBuf = std::max(1, static_cast<int>(std::ceil(currentSampleRate * maxLookaheadMs * 0.001)) + 2);

    for (size_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
    {
        dmBase[branchIndex].assign(static_cast<size_t>(maxBuf), 0.0);
        dmGain[branchIndex].assign(static_cast<size_t>(maxBuf), 1.0);
    }

    dryL.assign(static_cast<size_t>(maxBuf), 0.0);
    dryR.assign(static_cast<size_t>(maxBuf), 0.0);
}

void DspCore::clearState()
{
    for (size_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
    {
        std::fill(dmBase[branchIndex].begin(), dmBase[branchIndex].end(), 0.0);
        std::fill(dmGain[branchIndex].begin(), dmGain[branchIndex].end(), 1.0);
    }

    std::fill(dryL.begin(), dryL.end(), 0.0);
    std::fill(dryR.begin(), dryR.end(), 0.0);

    holdPeak = { 0, 0, 0, 0 };
    holdBase = { 0, 0, 0, 0 };
    envPeak = { 0.0, 0.0, 0.0, 0.0 };
    envBase = { 0.0, 0.0, 0.0, 0.0 };
    baseGainState = { 1.0, 1.0, 1.0, 1.0 };
    gainReductionState = { 1.0, 1.0, 1.0, 1.0 };
    bufPos = 0;
    bufPosDry = 0;
}

void DspCore::updateDerivedParameters()
{
    const auto sampleRate = std::max(1.0, currentSampleRate);

    derived.thresholds[branchLu] = dbToAmp(roundToJsfxStep(parameters.thLU));
    derived.thresholds[branchLd] = dbToAmp(roundToJsfxStep(parameters.thLD));
    derived.thresholds[branchRu] = dbToAmp(roundToJsfxStep(parameters.thRU));
    derived.thresholds[branchRd] = dbToAmp(roundToJsfxStep(parameters.thRD));

    derived.tensions[branchLu] = roundToJsfxStep(parameters.tensLU);
    derived.tensions[branchLd] = roundToJsfxStep(parameters.tensLD);
    derived.tensions[branchRu] = roundToJsfxStep(parameters.tensRU);
    derived.tensions[branchRd] = roundToJsfxStep(parameters.tensRD);

    const auto relLuMs = roundToJsfxStep(parameters.relLU);
    const auto relLdMs = roundToJsfxStep(parameters.relLD);
    const auto relRuMs = roundToJsfxStep(parameters.relRU);
    const auto relRdMs = roundToJsfxStep(parameters.relRD);
    derived.releaseCoeffs[branchLu] = relLuMs <= 0.0 ? 0.0 : std::exp(-1.0 / std::max(1.0, relLuMs * 0.001 * sampleRate));
    derived.releaseCoeffs[branchLd] = relLdMs <= 0.0 ? 0.0 : std::exp(-1.0 / std::max(1.0, relLdMs * 0.001 * sampleRate));
    derived.releaseCoeffs[branchRu] = relRuMs <= 0.0 ? 0.0 : std::exp(-1.0 / std::max(1.0, relRuMs * 0.001 * sampleRate));
    derived.releaseCoeffs[branchRd] = relRdMs <= 0.0 ? 0.0 : std::exp(-1.0 / std::max(1.0, relRdMs * 0.001 * sampleRate));

    derived.branchOutGains[branchLu] = dbToAmp(roundToJsfxStep(parameters.outLU));
    derived.branchOutGains[branchLd] = dbToAmp(roundToJsfxStep(parameters.outLD));
    derived.branchOutGains[branchRu] = dbToAmp(roundToJsfxStep(parameters.outRU));
    derived.branchOutGains[branchRd] = dbToAmp(roundToJsfxStep(parameters.outRD));

    derived.morph = roundToJsfxStep(parameters.moRph) * 0.01;
    derived.clipKneeDb = kneeRangeDb * derived.morph;

    auto holdHz = roundToJsfxStep(parameters.peakHoldHz);
    holdHz = std::max(21.0, holdHz);

    derived.tensionFloor = dbToAmp(roundToJsfxStep(parameters.TensionFlooR));
    derived.tensionHysteresis = roundToJsfxStep(parameters.TensionHysT) * 0.01;
    derived.delta = parameters.delTa;

    const auto holdTotalMs = 500.0 / holdHz;
    auto holdSamples = static_cast<int>(std::floor(holdTotalMs * 0.001 * sampleRate));
    holdSamples = std::max(0, holdSamples - 5);

    derived.holdSamples = holdSamples;
    derived.lookaheadSamples = juce::jlimit(0, std::max(0, maxBuf - 2), holdSamples);
    derived.bufferSize = derived.lookaheadSamples + 1;
    derived.dryBufferSize = derived.lookaheadSamples + 1;
    derived.latencySamples = derived.lookaheadSamples;

    bufPos = wrapIndex(bufPos, derived.bufferSize);
    bufPosDry = wrapIndex(bufPosDry, derived.dryBufferSize);
}
} // namespace mxe::dsp
