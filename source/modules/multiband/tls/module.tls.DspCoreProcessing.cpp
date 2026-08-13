#include "module.tls.DspCore.h"

namespace tls::dsp
{
DspCore::StereoSample DspCore::processSample(const double leftInput, const double rightInput)
{
    auto left = leftInput * derived.linkedGain;
    auto right = rightInput * derived.linkedGain;

    for (const auto operation : derived.gainOrder)
    {
        if (operation == GainOperation::left)
        {
            left *= derived.leftGain;
        }
        else if (operation == GainOperation::right)
        {
            right *= derived.rightGain;
        }
        else
        {
            auto mid = 0.5 * (left + right);
            auto side = 0.5 * (left - right);

            if (operation == GainOperation::mid)
                mid *= derived.midGain;
            else
                side *= derived.sideGain;

            left = mid + side;
            right = mid - side;
        }
    }

    const auto panoramaLeft = (left * derived.gLL) + (right * derived.gRL);
    const auto panoramaRight = (left * derived.gLR) + (right * derived.gRR);

    if (parameters.impactToRight)
    {
        left = panoramaLeft;
        right = panoramaRight + (derived.impactAmount * panoramaLeft);
    }
    else
    {
        left = panoramaLeft + (derived.impactAmount * panoramaRight);
        right = panoramaRight;
    }

    const auto linMid = (left + right) * 0.7071067811865476;
    const auto linSide = (left - right) * 0.7071067811865476;
    const auto midLeft = linMid * (derived.midAmount > 0.0 ? 1.0 - derived.midAmount : 1.0);
    const auto midRight = linMid * (derived.midAmount < 0.0 ? 1.0 + derived.midAmount : 1.0);
    const auto sideLeft = linSide * (derived.sideAmount > 0.0 ? 1.0 - derived.sideAmount : 1.0);
    const auto sideRight = linSide * (derived.sideAmount < 0.0 ? 1.0 + derived.sideAmount : 1.0);
    left = (midLeft + sideLeft) * 0.7071067811865476;
    right = (midRight - sideRight) * 0.7071067811865476;

    const auto orthogonalLeft = (derived.orthogonalM11 * left) + (derived.orthogonalM12 * right);
    const auto orthogonalRight = (derived.orthogonalM21 * left) + (derived.orthogonalM22 * right);
    left = orthogonalLeft;
    right = orthogonalRight;

    if (parameters.halfPositive)
    {
        left = juce::jmax(left, 0.0);
        right = juce::jmax(right, 0.0);
    }
    else if (parameters.halfNegative)
    {
        left = juce::jmin(left, 0.0);
        right = juce::jmin(right, 0.0);
    }
    else if (parameters.fullPositive)
    {
        left = std::abs(left);
        right = std::abs(right);
    }
    else if (parameters.fullNegative)
    {
        left = -std::abs(left);
        right = -std::abs(right);
    }

    if (derived.listenMode != ListenMode::neutral)
    {
        const auto listenLeft = left;
        const auto listenRight = right;
        const auto listenMid = 0.5 * (left + right);
        const auto listenSide = 0.5 * (left - right);

        if (derived.listenMode == ListenMode::leftCenter)
        {
            left = listenLeft;
            right = listenLeft;
        }
        else if (derived.listenMode == ListenMode::rightCenter)
        {
            left = listenRight;
            right = listenRight;
        }
        else if (derived.listenMode == ListenMode::midCenter)
        {
            left = listenMid;
            right = listenMid;
        }
        else if (derived.listenMode == ListenMode::sideCenter)
        {
            left = listenSide;
            right = listenSide;
        }
        else if (derived.listenMode == ListenMode::leftLeft)
        {
            left = listenLeft;
            right = 0.0;
        }
        else if (derived.listenMode == ListenMode::rightRight)
        {
            left = 0.0;
            right = listenRight;
        }
        else if (derived.listenMode == ListenMode::sideStereo)
        {
            left = listenSide;
            right = -listenSide;
        }
    }

    if (derived.depDelayEnabled && ! depDelayLeft.empty() && ! depDelayRight.empty())
    {
        depDelayLeft[static_cast<size_t>(depDelayWritePosition)] = left;
        depDelayRight[static_cast<size_t>(depDelayWritePosition)] = right;

        const auto readLeftPosition = wrapIndex(depDelayWritePosition - derived.depDelayLeftSamples, depDelayBufferSize);
        const auto readRightPosition = wrapIndex(depDelayWritePosition - derived.depDelayRightSamples, depDelayBufferSize);
        left = depDelayLeft[static_cast<size_t>(readLeftPosition)];
        right = depDelayRight[static_cast<size_t>(readRightPosition)];
        depDelayWritePosition = wrapIndex(depDelayWritePosition + 1, depDelayBufferSize);
    }

    if (derived.depPhaseEnabled)
    {
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
    }

    return { left, right };
}
} // namespace tls::dsp
