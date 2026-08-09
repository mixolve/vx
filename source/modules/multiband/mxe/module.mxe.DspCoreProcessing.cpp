#include "module.mxe.DspCore.h"

#include <algorithm>

namespace mxe::dsp
{
DspCore::StereoSample DspCore::processSample(const double leftInput, const double rightInput)
{
    const auto inL = leftInput;
    const auto inR = rightInput;

    dryL[static_cast<size_t>(bufPosDry)] = inL;
    dryR[static_cast<size_t>(bufPosDry)] = inR;

    const auto posL = std::max(inL, 0.0);
    const auto negL = std::min(inL, 0.0);
    const auto posR = std::max(inR, 0.0);
    const auto negR = std::min(inR, 0.0);

    // Feed detector/release from pre-clip half-waves; clipped path is only for output blend.
    std::array<double, branchCount> branchDetectorInput {
        posL,
        negL,
        posR,
        negR
    };

    for (size_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
    {
        const auto controlBase = safeAbs(branchDetectorInput[branchIndex]);

        if (controlBase > envBase[branchIndex])
        {
            envBase[branchIndex] = controlBase;
            holdBase[branchIndex] = derived.holdSamples;
        }
        else if (holdBase[branchIndex] > 0)
        {
            --holdBase[branchIndex];
        }
        else
        {
            envBase[branchIndex] = controlBase
                + (envBase[branchIndex] - controlBase) * derived.releaseCoeffs[branchIndex];
        }

        const auto targetBase = tensionTarget(envBase[branchIndex],
                                              derived.thresholds[branchIndex],
                                              derived.tensionFloor,
                                              derived.tensionHysteresis,
                                              derived.tensions[branchIndex]);
        const auto baseGainTarget = envBase[branchIndex] > epsilon ? (targetBase / envBase[branchIndex]) : 1.0;
        baseGainState[branchIndex] = baseGainTarget;

        const auto baseNow = branchDetectorInput[branchIndex] * baseGainState[branchIndex];
        dmBase[branchIndex][static_cast<size_t>(bufPos)] = baseNow;

        const auto peakNow = safeAbs(baseNow);

        if (peakNow > envPeak[branchIndex])
        {
            envPeak[branchIndex] = peakNow;
            holdPeak[branchIndex] = derived.holdSamples;
        }
        else if (holdPeak[branchIndex] > 0)
        {
            --holdPeak[branchIndex];
        }
        else
        {
            envPeak[branchIndex] = peakNow
                + (envPeak[branchIndex] - peakNow) * derived.releaseCoeffs[branchIndex];
        }

        const auto gainReductionTarget = envPeak[branchIndex] > derived.thresholds[branchIndex]
            ? (derived.thresholds[branchIndex] / envPeak[branchIndex])
            : 1.0;

        if (gainReductionTarget < gainReductionState[branchIndex])
            gainReductionState[branchIndex] = gainReductionTarget;
        else
            gainReductionState[branchIndex] = gainReductionTarget
                + (gainReductionState[branchIndex] - gainReductionTarget) * derived.releaseCoeffs[branchIndex];

        dmGain[branchIndex][static_cast<size_t>(bufPos)] = gainReductionState[branchIndex];
    }

    const auto readPos = wrapIndex(bufPos - derived.lookaheadSamples, derived.bufferSize);
    std::array<double, branchCount> branchProcessed {};
    std::array<double, branchCount> branchOutput {};

    for (size_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
    {
        const auto delayedBase = dmBase[branchIndex][static_cast<size_t>(readPos)];
        const auto delayedGainReduction = dmGain[branchIndex][static_cast<size_t>(readPos)];
        const auto reducedBase = delayedBase * delayedGainReduction;

        const auto clipped = derived.thresholds[branchIndex] > epsilon
            ? derived.thresholds[branchIndex] * satShape(delayedBase / derived.thresholds[branchIndex], derived.clipKneeDb)
            : satShape(delayedBase, derived.clipKneeDb);

        branchProcessed[branchIndex] = clipped + (reducedBase - clipped) * derived.morph;
        branchOutput[branchIndex] = branchProcessed[branchIndex] * derived.branchOutGains[branchIndex];
    }

    const auto outL = branchOutput[branchLu] + branchOutput[branchLd];
    const auto outR = branchOutput[branchRu] + branchOutput[branchRd];
    const auto deltaProcessedL = branchProcessed[branchLu] + branchProcessed[branchLd];
    const auto deltaProcessedR = branchProcessed[branchRu] + branchProcessed[branchRd];

    const auto readPosDry = wrapIndex(bufPosDry - derived.latencySamples, derived.dryBufferSize);
    const auto delayedDryL = dryL[static_cast<size_t>(readPosDry)];
    const auto delayedDryR = dryR[static_cast<size_t>(readPosDry)];

    StereoSample output;
    output.left = derived.delta ? (delayedDryL - deltaProcessedL) : outL;
    output.right = derived.delta ? (delayedDryR - deltaProcessedR) : outR;

    bufPos = wrapIndex(bufPos + 1, derived.bufferSize);
    bufPosDry = wrapIndex(bufPosDry + 1, derived.dryBufferSize);

    return output;
}
} // namespace mxe::dsp
