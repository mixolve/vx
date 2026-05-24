#include "module.mxe.DspCore.h"

#include <algorithm>

namespace mxe::dsp
{
DspCore::StereoSample DspCore::processSample(const double leftInput, const double rightInput)
{
    const auto inputGain = derived.inGain * autoInGain.current;
    const auto inputRightGain = derived.inRightGain * autoInRightGain.current;
    const auto inputLeftGain = derived.inLeftGain * autoInLeftGain.current;
    const auto widthGain = 1.0 + wideAmount.current;

    const auto gainedL = leftInput * inputGain * inputLeftGain;
    const auto gainedR = rightInput * inputGain * inputRightGain;
    const auto mid = 0.5 * (gainedL + gainedR);
    const auto side = 0.5 * (gainedL - gainedR) * widthGain;
    const auto inL = mid + side;
    const auto inR = mid - side;

    dryL[static_cast<size_t>(bufPosDry)] = inL;
    dryR[static_cast<size_t>(bufPosDry)] = inR;

    const auto posL = std::max(inL, 0.0);
    const auto negL = std::min(inL, 0.0);
    const auto posR = std::max(inR, 0.0);
    const auto negR = std::min(inR, 0.0);

    const auto morphHalfWave = [this] (const double value, const double threshold)
    {
        if (threshold <= epsilon)
            return 0.0;

        return threshold * satShape(value / threshold, derived.clipKneeDb);
    };

    auto halfL = morphHalfWave(posL, derived.thLU) * derived.mkLU
        + morphHalfWave(negL, derived.thLD) * derived.mkLD;
    auto halfR = morphHalfWave(posR, derived.thRU) * derived.mkRU
        + morphHalfWave(negR, derived.thRD) * derived.mkRD;

    if (derived.hwBypass)
    {
        halfL = inL;
        halfR = inR;
    }

    const auto ctrlBaseLL = safeAbs(halfL);
    if (ctrlBaseLL > envBaseLL)
    {
        envBaseLL = ctrlBaseLL;
        holdBaseLL = derived.holdSamples;
    }
    else if (holdBaseLL > 0)
    {
        --holdBaseLL;
    }
    else
    {
        envBaseLL = ctrlBaseLL + (envBaseLL - ctrlBaseLL) * derived.llReleaseCoeff;
    }

    const auto targetBaseLL = tensionTarget(envBaseLL,
                                            derived.llThresh,
                                            derived.tensionFloor,
                                            derived.tensionHysteresis,
                                            derived.llTension);
    const auto baseGainTargetLL = envBaseLL > epsilon ? (targetBaseLL / envBaseLL) : 1.0;
    baseGainStateLL = baseGainTargetLL;

    const auto baseNowLL = halfL * baseGainStateLL;

    const auto ctrlBaseRR = safeAbs(halfR);
    if (ctrlBaseRR > envBaseRR)
    {
        envBaseRR = ctrlBaseRR;
        holdBaseRR = derived.holdSamples;
    }
    else if (holdBaseRR > 0)
    {
        --holdBaseRR;
    }
    else
    {
        envBaseRR = ctrlBaseRR + (envBaseRR - ctrlBaseRR) * derived.rrReleaseCoeff;
    }

    const auto targetBaseRR = tensionTarget(envBaseRR,
                                            derived.rrThresh,
                                            derived.tensionFloor,
                                            derived.tensionHysteresis,
                                            derived.rrTension);
    const auto baseGainTargetRR = envBaseRR > epsilon ? (targetBaseRR / envBaseRR) : 1.0;
    baseGainStateRR = baseGainTargetRR;

    const auto baseNowRR = halfR * baseGainStateRR;

    dmBaseL[static_cast<size_t>(bufPos)] = baseNowLL;
    dmBaseR[static_cast<size_t>(bufPos)] = baseNowRR;
    dmDryL[static_cast<size_t>(bufPos)] = halfL;
    dmDryR[static_cast<size_t>(bufPos)] = halfR;

    const auto peakNowLL = safeAbs(baseNowLL);
    const auto peakNowRR = safeAbs(baseNowRR);

    if (peakNowLL > envLL)
    {
        envLL = peakNowLL;
        holdLL = derived.holdSamples;
    }
    else if (holdLL > 0)
    {
        --holdLL;
    }
    else
    {
        envLL = peakNowLL;
    }

    if (peakNowRR > envRR)
    {
        envRR = peakNowRR;
        holdRR = derived.holdSamples;
    }
    else if (holdRR > 0)
    {
        --holdRR;
    }
    else
    {
        envRR = peakNowRR;
    }

    const auto gainRedTargetLL = envLL > derived.llThresh ? (derived.llThresh / envLL) : 1.0;
    const auto gainRedTargetRR = envRR > derived.rrThresh ? (derived.rrThresh / envRR) : 1.0;

    if (gainRedTargetLL < gainReductionStateLL)
        gainReductionStateLL = gainRedTargetLL;
    else
        gainReductionStateLL = gainRedTargetLL + (gainReductionStateLL - gainRedTargetLL) * derived.llReleaseCoeff;

    if (gainRedTargetRR < gainReductionStateRR)
        gainReductionStateRR = gainRedTargetRR;
    else
        gainReductionStateRR = gainRedTargetRR + (gainReductionStateRR - gainRedTargetRR) * derived.rrReleaseCoeff;

    dmGainL[static_cast<size_t>(bufPos)] = gainReductionStateLL;
    dmGainR[static_cast<size_t>(bufPos)] = gainReductionStateRR;

    const auto readPos = wrapIndex(bufPos - derived.lookaheadSamples, derived.bufferSize);

    const auto delayBaseLL = dmBaseL[static_cast<size_t>(readPos)];
    const auto delayBaseRR = dmBaseR[static_cast<size_t>(readPos)];
    const auto delayedDmDryL = dmDryL[static_cast<size_t>(readPos)];
    const auto delayedDmDryR = dmDryR[static_cast<size_t>(readPos)];
    const auto delayedGainRedLL = dmGainL[static_cast<size_t>(readPos)];
    const auto delayedGainRedRR = dmGainR[static_cast<size_t>(readPos)];

    const auto reducedDmBaseL = delayBaseLL * delayedGainRedLL;
    const auto reducedDmBaseR = delayBaseRR * delayedGainRedRR;

    const auto dmClipL = derived.llThresh > epsilon
        ? derived.llThresh * satShape(delayBaseLL / derived.llThresh, derived.clipKneeDb)
        : satShape(delayBaseLL, derived.clipKneeDb);
    const auto dmClipR = derived.rrThresh > epsilon
        ? derived.rrThresh * satShape(delayBaseRR / derived.rrThresh, derived.clipKneeDb)
        : satShape(delayBaseRR, derived.clipKneeDb);

    const auto dmLimL = reducedDmBaseL;
    const auto dmLimR = reducedDmBaseR;
    const auto dmWetL = (dmClipL + (dmLimL - dmClipL) * derived.morph) * derived.llMakeup;
    const auto dmWetR = (dmClipR + (dmLimR - dmClipR) * derived.morph) * derived.rrMakeup;
    const auto dmOutL = derived.dmBypass ? delayedDmDryL : dmWetL;
    const auto dmOutR = derived.dmBypass ? delayedDmDryR : dmWetR;

    const auto ctrlBaseFF = std::max(safeAbs(dmOutL), safeAbs(dmOutR));
    if (ctrlBaseFF > envBaseFF)
    {
        envBaseFF = ctrlBaseFF;
        holdBaseFF = derived.holdSamples;
    }
    else if (holdBaseFF > 0)
    {
        --holdBaseFF;
    }
    else
    {
        envBaseFF = ctrlBaseFF + (envBaseFF - ctrlBaseFF) * derived.ffReleaseCoeff;
    }

    const auto targetBaseFF = tensionTarget(envBaseFF,
                                            derived.ffThresh,
                                            derived.tensionFloor,
                                            derived.tensionHysteresis,
                                            derived.ffTension);
    const auto baseGainTargetFF = envBaseFF > epsilon ? (targetBaseFF / envBaseFF) : 1.0;
    baseGainStateFF = baseGainTargetFF;

    const auto baseNowFFL = dmOutL * baseGainStateFF;
    const auto baseNowFFR = dmOutR * baseGainStateFF;

    ffBaseL[static_cast<size_t>(bufPos)] = dmOutL;
    ffBaseR[static_cast<size_t>(bufPos)] = dmOutR;
    ffDryL[static_cast<size_t>(bufPos)] = dmOutL;
    ffDryR[static_cast<size_t>(bufPos)] = dmOutR;
    ffBaseGain[static_cast<size_t>(bufPos)] = baseGainStateFF;

    const auto peakNowFF = std::max(safeAbs(baseNowFFL), safeAbs(baseNowFFR));

    if (peakNowFF > envFF)
    {
        envFF = peakNowFF;
        holdFF = derived.holdSamples;
    }
    else if (holdFF > 0)
    {
        --holdFF;
    }
    else
    {
        envFF = peakNowFF;
    }

    const auto gainRedTargetFF = envFF > derived.ffThresh ? (derived.ffThresh / envFF) : 1.0;

    if (gainRedTargetFF < gainReductionStateFF)
        gainReductionStateFF = gainRedTargetFF;
    else
        gainReductionStateFF = gainRedTargetFF + (gainReductionStateFF - gainRedTargetFF) * derived.ffReleaseCoeff;

    ffGain[static_cast<size_t>(bufPos)] = gainReductionStateFF;

    const auto delayBaseFFL = ffBaseL[static_cast<size_t>(readPos)];
    const auto delayBaseFFR = ffBaseR[static_cast<size_t>(readPos)];
    const auto delayedFfDryL = ffDryL[static_cast<size_t>(readPos)];
    const auto delayedFfDryR = ffDryR[static_cast<size_t>(readPos)];
    const auto delayedBaseGainFF = ffBaseGain[static_cast<size_t>(readPos)];
    const auto delayedGainRedFF = ffGain[static_cast<size_t>(readPos)];

    const auto baseDelayedFFL = delayBaseFFL * delayedBaseGainFF;
    const auto baseDelayedFFR = delayBaseFFR * delayedBaseGainFF;
    const auto reducedFfBaseL = baseDelayedFFL * delayedGainRedFF;
    const auto reducedFfBaseR = baseDelayedFFR * delayedGainRedFF;
    const auto ffClipL = derived.ffThresh > epsilon
        ? derived.ffThresh * satShape(baseDelayedFFL / derived.ffThresh, derived.clipKneeDb)
        : satShape(baseDelayedFFL, derived.clipKneeDb);
    const auto ffClipR = derived.ffThresh > epsilon
        ? derived.ffThresh * satShape(baseDelayedFFR / derived.ffThresh, derived.clipKneeDb)
        : satShape(baseDelayedFFR, derived.clipKneeDb);
    const auto ffLimL = reducedFfBaseL;
    const auto ffLimR = reducedFfBaseR;
    const auto ffWetL = (ffClipL + (ffLimL - ffClipL) * derived.morph) * derived.ffMakeup;
    const auto ffWetR = (ffClipR + (ffLimR - ffClipR) * derived.morph) * derived.ffMakeup;
    const auto outL = derived.ffBypass ? delayedFfDryL : ffWetL;
    const auto outR = derived.ffBypass ? delayedFfDryR : ffWetR;

    const auto readPosDry = wrapIndex(bufPosDry - derived.totalLookaheadSamples, derived.dryBufferSize);
    const auto delayedDryL = dryL[static_cast<size_t>(readPosDry)];
    const auto delayedDryR = dryR[static_cast<size_t>(readPosDry)];

    StereoSample output;
    output.left = (derived.delta ? (delayedDryL - outL) : outL) * derived.outGain;
    output.right = (derived.delta ? (delayedDryR - outR) : outR) * derived.outGain;

    bufPos = wrapIndex(bufPos + 1, derived.bufferSize);
    bufPosDry = wrapIndex(bufPosDry + 1, derived.dryBufferSize);
    advanceSmoothedParameters();

    return output;
}
} // namespace mxe::dsp
