#include "module.mie.DspCore.h"

namespace mie::dsp
{
DspCore::StereoSample DspCore::processSample(const double leftInput, const double rightInput)
{
    const auto gainedL = leftInput * derived.linkedGain * derived.leftGain;
    const auto gainedR = rightInput * derived.linkedGain * derived.rightGain;
    const auto mid = 0.5 * (gainedL + gainedR);
    const auto side = 0.5 * (gainedL - gainedR) * derived.wideAmount;
    auto left = mid + side;
    auto right = mid - side;

    const auto pannedLeft = (left * derived.gLL) + (right * derived.gRL);
    const auto pannedRight = (left * derived.gLR) + (right * derived.gRR);

    if (parameters.shearToRight)
    {
        left = pannedLeft;
        right = pannedRight + (derived.shearAmount * pannedLeft);
    }
    else
    {
        left = pannedLeft + (derived.shearAmount * pannedRight);
        right = pannedRight;
    }

    const auto linMid = (left + right) * 0.7071067811865476;
    const auto linSide = (left - right) * 0.7071067811865476;
    const auto midLeft = linMid * (derived.midBalance > 0.0 ? 1.0 - derived.midBalance : 1.0);
    const auto midRight = linMid * (derived.midBalance < 0.0 ? 1.0 + derived.midBalance : 1.0);
    const auto sideLeft = linSide * (derived.sideBalance > 0.0 ? 1.0 - derived.sideBalance : 1.0);
    const auto sideRight = linSide * (derived.sideBalance < 0.0 ? 1.0 + derived.sideBalance : 1.0);
    left = (midLeft + sideLeft) * 0.7071067811865476;
    right = (midRight - sideRight) * 0.7071067811865476;

    const auto rotatedLeft = (derived.ortM11 * left) + (derived.ortM12 * right);
    const auto rotatedRight = (derived.ortM21 * left) + (derived.ortM22 * right);
    left = rotatedLeft;
    right = rotatedRight;

    if (parameters.rectPlus)
    {
        left = juce::jmax(left, 0.0);
        right = juce::jmax(right, 0.0);
    }
    else if (parameters.rectMinus)
    {
        left = juce::jmin(left, 0.0);
        right = juce::jmin(right, 0.0);
    }
    else if (parameters.rectFoldPlus)
    {
        left = std::abs(left);
        right = std::abs(right);
    }
    else if (parameters.rectFoldMinus)
    {
        left = -std::abs(left);
        right = -std::abs(right);
    }

    if (derived.listenMode >= 0)
    {
        constexpr auto msNorm = 0.7071067811865476;
        const auto listenLeft = left;
        const auto listenRight = right;
        const auto listenMid = (left + right) * msNorm;
        const auto listenSide = (left - right) * msNorm;

        if (derived.listenMode == 0)
        {
            right = parameters.listenInPlace ? 0.0 : listenLeft;
            left = listenLeft;
        }
        else if (derived.listenMode == 1)
        {
            left = parameters.listenInPlace ? 0.0 : listenRight;
            right = listenRight;
        }
        else if (derived.listenMode == 2)
        {
            left = listenMid;
            right = listenMid;
        }
        else if (derived.listenMode == 3)
        {
            left = listenSide;
            right = parameters.listenInPlace ? -listenSide : listenSide;
        }
    }

    if (derived.depLookaheadSamples > 0 && ! depDelayLeft.empty() && ! depDelayRight.empty())
    {
        depDelayLeft[static_cast<size_t>(depDelayWritePosition)] = left;
        depDelayRight[static_cast<size_t>(depDelayWritePosition)] = right;

        const auto readLeftPosition = wrapIndex(depDelayWritePosition - derived.depDelayLeftSamples, depDelayBufferSize);
        const auto readRightPosition = wrapIndex(depDelayWritePosition - derived.depDelayRightSamples, depDelayBufferSize);
        left = depDelayLeft[static_cast<size_t>(readLeftPosition)];
        right = depDelayRight[static_cast<size_t>(readRightPosition)];
        depDelayWritePosition = wrapIndex(depDelayWritePosition + 1, depDelayBufferSize);
    }

    depPhaseLeft[static_cast<size_t>(depPhaseWritePosition)] = left;
    depPhaseRight[static_cast<size_t>(depPhaseWritePosition)] = right;

    const auto delayedLeft = depPhaseLeft[static_cast<size_t>(wrapIndex(depPhaseWritePosition - depPhaseMid, depPhaseBufferSize))];
    const auto delayedRight = depPhaseRight[static_cast<size_t>(wrapIndex(depPhaseWritePosition - depPhaseMid, depPhaseBufferSize))];
    auto hilbertLeft = 0.0;
    auto hilbertRight = 0.0;

    for (int tapIndex = 0; tapIndex < depPhaseTaps; ++tapIndex)
    {
        const auto readPosition = wrapIndex(depPhaseWritePosition - tapIndex, depPhaseBufferSize);
        const auto coefficient = depPhaseCoefficients[static_cast<size_t>(tapIndex)];
        hilbertLeft += coefficient * depPhaseLeft[static_cast<size_t>(readPosition)];
        hilbertRight += coefficient * depPhaseRight[static_cast<size_t>(readPosition)];
    }

    left = (delayedLeft * derived.depPhaseCosL) + (hilbertLeft * derived.depPhaseSinL);
    right = (delayedRight * derived.depPhaseCosR) + (hilbertRight * derived.depPhaseSinR);
    depPhaseWritePosition = wrapIndex(depPhaseWritePosition + 1, depPhaseBufferSize);

    return { left, right };
}
} // namespace mie::dsp
