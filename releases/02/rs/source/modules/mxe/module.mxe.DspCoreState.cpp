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
    maxTotalBuf = std::max(2, maxBuf * 2);

    dmBaseL.assign(static_cast<size_t>(maxBuf), 0.0);
    dmBaseR.assign(static_cast<size_t>(maxBuf), 0.0);
    dmDryL.assign(static_cast<size_t>(maxBuf), 0.0);
    dmDryR.assign(static_cast<size_t>(maxBuf), 0.0);
    dmGainL.assign(static_cast<size_t>(maxBuf), 1.0);
    dmGainR.assign(static_cast<size_t>(maxBuf), 1.0);
    ffBaseL.assign(static_cast<size_t>(maxBuf), 0.0);
    ffBaseR.assign(static_cast<size_t>(maxBuf), 0.0);
    ffDryL.assign(static_cast<size_t>(maxBuf), 0.0);
    ffDryR.assign(static_cast<size_t>(maxBuf), 0.0);
    ffBaseGain.assign(static_cast<size_t>(maxBuf), 1.0);
    ffGain.assign(static_cast<size_t>(maxBuf), 1.0);
    dryL.assign(static_cast<size_t>(maxTotalBuf), 0.0);
    dryR.assign(static_cast<size_t>(maxTotalBuf), 0.0);
}

void DspCore::clearState()
{
    std::fill(dmBaseL.begin(), dmBaseL.end(), 0.0);
    std::fill(dmBaseR.begin(), dmBaseR.end(), 0.0);
    std::fill(dmDryL.begin(), dmDryL.end(), 0.0);
    std::fill(dmDryR.begin(), dmDryR.end(), 0.0);
    std::fill(dmGainL.begin(), dmGainL.end(), 1.0);
    std::fill(dmGainR.begin(), dmGainR.end(), 1.0);
    std::fill(ffBaseL.begin(), ffBaseL.end(), 0.0);
    std::fill(ffBaseR.begin(), ffBaseR.end(), 0.0);
    std::fill(ffDryL.begin(), ffDryL.end(), 0.0);
    std::fill(ffDryR.begin(), ffDryR.end(), 0.0);
    std::fill(ffBaseGain.begin(), ffBaseGain.end(), 1.0);
    std::fill(ffGain.begin(), ffGain.end(), 1.0);
    std::fill(dryL.begin(), dryL.end(), 0.0);
    std::fill(dryR.begin(), dryR.end(), 0.0);

    holdLL = 0;
    holdRR = 0;
    holdFF = 0;
    holdBaseLL = 0;
    holdBaseRR = 0;
    holdBaseFF = 0;
    envLL = 0.0;
    envRR = 0.0;
    envFF = 0.0;
    envBaseLL = 0.0;
    envBaseRR = 0.0;
    envBaseFF = 0.0;
    baseGainStateLL = 1.0;
    baseGainStateRR = 1.0;
    baseGainStateFF = 1.0;
    gainReductionStateLL = 1.0;
    gainReductionStateRR = 1.0;
    gainReductionStateFF = 1.0;
    bufPos = 0;
    bufPosDry = 0;
}

void DspCore::updateDerivedParameters()
{
    const auto sampleRate = std::max(1.0, currentSampleRate);

    derived.inGain = dbToAmp(roundToJsfxStep(parameters.inGn));
    derived.inRightGain = dbToAmp(roundToJsfxStep(parameters.inRight));
    derived.inLeftGain = dbToAmp(roundToJsfxStep(parameters.inLeft));
    derived.autoInGain = dbToAmp(roundToJsfxStep(parameters.autoInGn));
    derived.autoInRightGain = dbToAmp(roundToJsfxStep(parameters.autoInRight));
    derived.autoInLeftGain = dbToAmp(roundToJsfxStep(parameters.autoInLeft));
    derived.wideAmount = roundToJsfxStep(parameters.wide) / 100.0;
    derived.outGain = dbToAmp(roundToJsfxStep(parameters.outGn));
    derived.thLU = dbToAmp(roundToJsfxStep(parameters.thLU));
    derived.mkLU = dbToAmp(roundToJsfxStep(parameters.mkLU));
    derived.thLD = dbToAmp(roundToJsfxStep(parameters.thLD));
    derived.mkLD = dbToAmp(roundToJsfxStep(parameters.mkLD));
    derived.thRU = dbToAmp(roundToJsfxStep(parameters.thRU));
    derived.mkRU = dbToAmp(roundToJsfxStep(parameters.mkRU));
    derived.thRD = dbToAmp(roundToJsfxStep(parameters.thRD));
    derived.mkRD = dbToAmp(roundToJsfxStep(parameters.mkRD));
    derived.hwBypass = parameters.hwBypass;

    derived.llThresh = dbToAmp(roundToJsfxStep(parameters.LLThResh));
    derived.llTension = roundToJsfxStep(parameters.LLTension);
    const auto llReleaseMs = roundToJsfxStep(parameters.LLRelease);
    derived.llReleaseCoeff = llReleaseMs <= 0.0 ? 0.0 : std::exp(-1.0 / std::max(1.0, llReleaseMs * 0.001 * sampleRate));
    derived.llMakeup = dbToAmp(roundToJsfxStep(parameters.LLmk));

    derived.rrThresh = dbToAmp(roundToJsfxStep(parameters.RRThResh));
    derived.rrTension = roundToJsfxStep(parameters.RRTension);
    const auto rrReleaseMs = roundToJsfxStep(parameters.RRRelease);
    derived.rrReleaseCoeff = rrReleaseMs <= 0.0 ? 0.0 : std::exp(-1.0 / std::max(1.0, rrReleaseMs * 0.001 * sampleRate));
    derived.rrMakeup = dbToAmp(roundToJsfxStep(parameters.RRmk));
    derived.dmBypass = parameters.DMbypass;

    derived.ffThresh = dbToAmp(roundToJsfxStep(parameters.FFThResh));
    derived.ffTension = roundToJsfxStep(parameters.FFTension);
    const auto ffReleaseMs = roundToJsfxStep(parameters.FFRelease);
    derived.ffReleaseCoeff = ffReleaseMs <= 0.0 ? 0.0 : std::exp(-1.0 / std::max(1.0, ffReleaseMs * 0.001 * sampleRate));
    derived.ffMakeup = dbToAmp(roundToJsfxStep(parameters.FFmk));
    derived.ffBypass = parameters.FFbypass;

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
    derived.totalLookaheadSamples = juce::jlimit(0, std::max(0, maxTotalBuf - 2), derived.lookaheadSamples * 2);
    derived.bufferSize = derived.lookaheadSamples + 1;
    derived.dryBufferSize = derived.totalLookaheadSamples + 1;
    derived.latencySamples = derived.totalLookaheadSamples;

    bufPos = wrapIndex(bufPos, derived.bufferSize);
    bufPosDry = wrapIndex(bufPosDry, derived.dryBufferSize);
}
} // namespace mxe::dsp
