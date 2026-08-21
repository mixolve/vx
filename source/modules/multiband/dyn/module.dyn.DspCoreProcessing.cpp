#include "module.dyn.DspCore.h"

#include <algorithm>
#include <cmath>

namespace dyn::dsp
{
double DspCore::releaseTowards(ReleaseState& state,
                               const double current,
                               const double target,
                               const int durationSamples,
                               const bool logarithmic,
                               const double curve)
{
    if (durationSamples <= 0 || target >= current)
    {
        state.active = false;
        return target;
    }

    if (! state.active)
    {
        state.start = current;
        state.target = target;
        state.elapsedSamples = 0;
        state.durationSamples = durationSamples;
        state.active = true;
    }

    const auto progress = juce::jlimit(0.0,
                                        1.0,
                                        static_cast<double>(++state.elapsedSamples)
                                            / static_cast<double>(state.durationSamples));
    const auto curveAmount = logarithmic ? juce::jlimit(-1.0, 1.0, curve) : 0.0;
    const auto curvature = std::abs(curveAmount) * 99.0;
    const auto shapedProgress = curveAmount > 0.0
        ? std::log1p(curvature * progress) / std::log1p(curvature)
        : curveAmount < 0.0
            ? std::expm1(std::log1p(curvature) * progress) / curvature
            : progress;
    const auto released = state.start + (state.target - state.start) * shapedProgress;

    if (progress >= 1.0)
        state.active = false;

    return std::max(state.target, released);
}

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
            baseReleaseStates[branchIndex].active = false;
        }
        else
        {
            envBase[branchIndex] = releaseTowards(baseReleaseStates[branchIndex],
                                                   envBase[branchIndex],
                                                   0.0,
                                                   derived.releaseSamples[branchIndex],
                                                   derived.releaseLogarithmic,
                                                   derived.releaseCurve);
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

    }

    const std::array<double, channelCount> cleanDetectorInput {
        dmBase[branchLu][static_cast<size_t>(bufPos)] + dmBase[branchLd][static_cast<size_t>(bufPos)],
        dmBase[branchRu][static_cast<size_t>(bufPos)] + dmBase[branchRd][static_cast<size_t>(bufPos)]
    };
    const std::array<double, channelCount> cleanPositiveThresholds {
        derived.thresholds[branchLu],
        derived.thresholds[branchRu]
    };
    const std::array<double, channelCount> cleanNegativeThresholds {
        derived.thresholds[branchLd],
        derived.thresholds[branchRd]
    };
    const std::array<int, channelCount> cleanPositiveReleaseSamples {
        derived.releaseSamples[branchLu],
        derived.releaseSamples[branchRu]
    };
    const std::array<int, channelCount> cleanNegativeReleaseSamples {
        derived.releaseSamples[branchLd],
        derived.releaseSamples[branchRd]
    };

    for (size_t channelIndex = 0; channelIndex < channelCount; ++channelIndex)
    {
        const auto peakNow = safeAbs(cleanDetectorInput[channelIndex]);
        const auto inputPositive = cleanDetectorInput[channelIndex] >= 0.0;
        const auto releaseTarget = inputPositive
            ? cleanPositiveThresholds[channelIndex]
            : cleanNegativeThresholds[channelIndex];
        const auto releaseSamples = inputPositive
            ? cleanPositiveReleaseSamples[channelIndex]
            : cleanNegativeReleaseSamples[channelIndex];

        if (peakNow > cleanEnvPeak[channelIndex])
        {
            cleanEnvPeak[channelIndex] = peakNow;
            cleanHoldSamples[channelIndex] = derived.peakHoldSamples;
            cleanReleaseStates[channelIndex].active = false;
        }
        else if (cleanHoldSamples[channelIndex] > 0)
        {
            --cleanHoldSamples[channelIndex];
            cleanReleaseStates[channelIndex].active = false;
        }
        else
        {
            cleanEnvPeak[channelIndex] = releaseTowards(cleanReleaseStates[channelIndex],
                                                         cleanEnvPeak[channelIndex],
                                                         releaseTarget,
                                                         releaseSamples,
                                                         derived.releaseLogarithmic,
                                                         derived.releaseCurve);
        }

        if (inputPositive != cleanInputPositive[channelIndex])
        {
            cleanInputPositive[channelIndex] = inputPositive;
            const auto threshold = inputPositive
                ? cleanPositiveThresholds[channelIndex]
                : cleanNegativeThresholds[channelIndex];
            const auto gainReference = std::max(cleanHalfPeak[channelIndex], cleanEnvPeak[channelIndex]);
            cleanGainState[channelIndex] = gainReference > epsilon
                ? juce::jlimit(0.0, 1.0, threshold / gainReference)
                : 1.0;
            cleanHalfPeak[channelIndex] = peakNow;
        }
        else
        {
            cleanHalfPeak[channelIndex] = std::max(cleanHalfPeak[channelIndex], peakNow);
        }

    }

    const auto readPos = wrapIndex(bufPos - derived.lookaheadSamples, derived.bufferSize);
    std::array<double, branchCount> branchProcessed {};
    std::array<double, branchCount> branchOutput {};

    for (size_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
    {
        const auto delayedBase = dmBase[branchIndex][static_cast<size_t>(readPos)];
        const auto clipped = derived.thresholds[branchIndex] > epsilon
            ? derived.thresholds[branchIndex] * satShape(delayedBase / derived.thresholds[branchIndex], derived.clipKneeDb)
            : satShape(delayedBase, derived.clipKneeDb);

        branchProcessed[branchIndex] = clipped;
        branchOutput[branchIndex] = branchProcessed[branchIndex] * derived.branchOutGains[branchIndex];
    }

    const auto clippedOutL = branchOutput[branchLu] + branchOutput[branchLd];
    const auto clippedOutR = branchOutput[branchRu] + branchOutput[branchRd];
    const auto clippedDeltaL = branchProcessed[branchLu] + branchProcessed[branchLd];
    const auto clippedDeltaR = branchProcessed[branchRu] + branchProcessed[branchRd];
    const auto cleanGainL = cleanGainState[0];
    const auto cleanGainR = cleanGainState[1];
    const auto cleanDeltaL = (dmBase[branchLu][static_cast<size_t>(readPos)]
        + dmBase[branchLd][static_cast<size_t>(readPos)]) * cleanGainL;
    const auto cleanDeltaR = (dmBase[branchRu][static_cast<size_t>(readPos)]
        + dmBase[branchRd][static_cast<size_t>(readPos)]) * cleanGainR;
    const auto cleanOutL = (dmBase[branchLu][static_cast<size_t>(readPos)] * derived.branchOutGains[branchLu]
        + dmBase[branchLd][static_cast<size_t>(readPos)] * derived.branchOutGains[branchLd]) * cleanGainL;
    const auto cleanOutR = (dmBase[branchRu][static_cast<size_t>(readPos)] * derived.branchOutGains[branchRu]
        + dmBase[branchRd][static_cast<size_t>(readPos)] * derived.branchOutGains[branchRd]) * cleanGainR;
    const auto outL = clippedOutL + (cleanOutL - clippedOutL) * derived.morph;
    const auto outR = clippedOutR + (cleanOutR - clippedOutR) * derived.morph;
    const auto deltaProcessedL = clippedDeltaL + (cleanDeltaL - clippedDeltaL) * derived.morph;
    const auto deltaProcessedR = clippedDeltaR + (cleanDeltaR - clippedDeltaR) * derived.morph;

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
} // namespace dyn::dsp
