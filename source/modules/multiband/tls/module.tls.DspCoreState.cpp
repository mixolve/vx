#include "module.tls.DspCore.h"

#include <algorithm>
#include <utility>

namespace tls::dsp
{
namespace
{
constexpr double neutralEpsilon = 1.0e-9;

bool isNear(const double value, const double target) noexcept
{
    return std::abs(value - target) <= neutralEpsilon;
}
} // namespace

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
    const auto midGainDb = juce::jlimit(-99.0, 48.0, roundToParameterStep(parameters.gainMid));
    const auto sideGainDb = juce::jlimit(-99.0, 48.0, roundToParameterStep(parameters.gainSide));
    derived.midGain = parameters.gainMidMute ? 0.0 : dbToAmp(midGainDb);
    derived.sideGain = parameters.gainSideMute ? 0.0 : dbToAmp(sideGainDb);
    derived.leftGain = parameters.gainLMute
        ? 0.0
        : dbToAmp(juce::jlimit(-99.0, 48.0, roundToParameterStep(parameters.gainL)));
    derived.rightGain = parameters.gainRMute
        ? 0.0
        : dbToAmp(juce::jlimit(-99.0, 48.0, roundToParameterStep(parameters.gainR)));
    derived.linkedGain = parameters.gainLrMute
        ? 0.0
        : dbToAmp(juce::jlimit(-99.0, 48.0, roundToParameterStep(parameters.gainLr)));

    auto orderedGains = std::array<std::pair<int, GainOperation>, 4> {{
        { juce::jlimit(0, 3, parameters.gainLOrder), GainOperation::left },
        { juce::jlimit(0, 3, parameters.gainROrder), GainOperation::right },
        { juce::jlimit(0, 3, parameters.gainMidOrder), GainOperation::mid },
        { juce::jlimit(0, 3, parameters.gainSideOrder), GainOperation::side }
    }};
    std::stable_sort(orderedGains.begin(), orderedGains.end(), [] (const auto& first, const auto& second)
    {
        return first.first < second.first;
    });

    for (size_t index = 0; index < orderedGains.size(); ++index)
        derived.gainOrder[index] = orderedGains[index].second;
    const auto leftPosition = roundToParameterStep(parameters.left) * 0.01;
    const auto rightPosition = roundToParameterStep(parameters.right) * 0.01;
    const auto leftTheta = (leftPosition + 1.0) * juce::MathConstants<double>::pi * 0.25;
    const auto rightTheta = (rightPosition + 1.0) * juce::MathConstants<double>::pi * 0.25;
    const auto lawDb = juce::jlimit(0.0f, 6.0f, parameters.law);
    const auto lawGainForPosition = [lawDb] (const double position)
    {
        const auto distanceFromCentre = juce::jlimit(0.0, 1.0, std::abs(position));
        const auto centreWeight = std::cos(distanceFromCentre * juce::MathConstants<double>::pi * 0.5);
        return dbToAmp(-lawDb * centreWeight);
    };

    derived.gLL = std::cos(leftTheta);
    derived.gLR = std::sin(leftTheta);
    derived.gRL = std::cos(rightTheta);
    derived.gRR = std::sin(rightTheta);

    const auto leftLawGain = lawGainForPosition(leftPosition);
    derived.gLL *= leftLawGain;
    derived.gLR *= leftLawGain;

    const auto rightLawGain = lawGainForPosition(rightPosition);
    derived.gRL *= rightLawGain;
    derived.gRR *= rightLawGain;

    derived.impactAmount = roundToParameterStep(parameters.impact) * 0.01;
    derived.midAmount = roundToParameterStep(parameters.mid) * 0.01;
    derived.sideAmount = roundToParameterStep(parameters.side) * 0.01;

    const auto orthogonalTheta = roundToParameterStep(parameters.degree) * (juce::MathConstants<double>::pi / 180.0);
    const auto c = std::cos(orthogonalTheta);
    const auto s = std::sin(orthogonalTheta);
    derived.orthogonalM11 = c;
    derived.orthogonalM12 = parameters.flipRight ? s : -s;
    derived.orthogonalM21 = s;
    derived.orthogonalM22 = parameters.flipRight ? -c : c;

    derived.listenMode = ListenMode::neutral;
    if (parameters.listenLc)
        derived.listenMode = ListenMode::leftCenter;
    else if (parameters.listenRc)
        derived.listenMode = ListenMode::rightCenter;
    else if (parameters.listenMc)
        derived.listenMode = ListenMode::midCenter;
    else if (parameters.listenSc)
        derived.listenMode = ListenMode::sideCenter;
    else if (parameters.listenLl)
        derived.listenMode = ListenMode::leftLeft;
    else if (parameters.listenRr)
        derived.listenMode = ListenMode::rightRight;
    else if (parameters.listenSs)
        derived.listenMode = ListenMode::sideStereo;

    const auto stereoDelayMs = juce::jlimit(-100.0, 100.0, roundToParameterStep(parameters.depStereoMs));
    const auto leftDelayMs = juce::jlimit(-100.0, 100.0, roundToParameterStep(parameters.depLeftMs));
    const auto rightDelayMs = juce::jlimit(-100.0, 100.0, roundToParameterStep(parameters.depRightMs));
    const auto leftOffsetMs = stereoDelayMs + leftDelayMs;
    const auto rightOffsetMs = stereoDelayMs + rightDelayMs;
    const auto lookaheadMs = juce::jmax(0.0, -juce::jmin(leftOffsetMs, rightOffsetMs));
    const auto lookaheadSamples = msToSamples(lookaheadMs, currentSampleRate);
    derived.depDelayLeftSamples = msToSamples(lookaheadMs + leftOffsetMs, currentSampleRate);
    derived.depDelayRightSamples = msToSamples(lookaheadMs + rightOffsetMs, currentSampleRate);
    derived.depDelayEnabled = ! isNear(leftOffsetMs, 0.0) || ! isNear(rightOffsetMs, 0.0);

    if (! derived.depDelayEnabled)
    {
        derived.depDelayLeftSamples = 0;
        derived.depDelayRightSamples = 0;
    }

    const auto phaseL = juce::jlimit(-180.0f, 180.0f, parameters.depPhaseL) * (juce::MathConstants<double>::pi / 180.0);
    const auto phaseR = juce::jlimit(-180.0f, 180.0f, parameters.depPhaseR) * (juce::MathConstants<double>::pi / 180.0);
    derived.depPhaseEnabled = std::abs(phaseL) > neutralEpsilon || std::abs(phaseR) > neutralEpsilon;
    derived.latencySamples = lookaheadSamples + (derived.depPhaseEnabled ? depPhaseMid : 0);
    derived.depPhaseCosL = std::cos(phaseL);
    derived.depPhaseSinL = std::sin(phaseL);
    derived.depPhaseCosR = std::cos(phaseR);
    derived.depPhaseSinR = std::sin(phaseR);
}

bool DspCore::isNeutral() const noexcept
{
    return isNear(derived.midGain, 1.0)
        && isNear(derived.sideGain, 1.0)
        && isNear(derived.leftGain, 1.0)
        && isNear(derived.rightGain, 1.0)
        && isNear(derived.linkedGain, 1.0)
        && isNear(derived.gLL, 1.0)
        && isNear(derived.gLR, 0.0)
        && isNear(derived.gRL, 0.0)
        && isNear(derived.gRR, 1.0)
        && isNear(derived.impactAmount, 0.0)
        && isNear(derived.midAmount, 0.0)
        && isNear(derived.sideAmount, 0.0)
        && isNear(derived.orthogonalM11, 1.0)
        && isNear(derived.orthogonalM12, 0.0)
        && isNear(derived.orthogonalM21, 0.0)
        && isNear(derived.orthogonalM22, 1.0)
        && derived.listenMode == ListenMode::neutral
        && ! derived.depDelayEnabled
        && ! derived.depPhaseEnabled
        && ! parameters.halfPositive
        && ! parameters.halfNegative
        && ! parameters.fullPositive
        && ! parameters.fullNegative;
}
} // namespace tls::dsp
