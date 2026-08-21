#include "module.tls.PluginProcessor.h"

#include "module.tls.ParameterIds.h"

#include <cmath>

namespace
{
using tls::parameters::makeActiveSplitCountParameterId;
using tls::parameters::makeBandParameterId;
using tls::parameters::makeFullbandParameterId;
using tls::parameters::makeSoloParameterId;
using tls::parameters::crossoverSpecs;
using tls::parameters::numCrossoverSlots;
using tls::parameters::numParameterSlots;
using tls::parameters::parameterSpecs;
using tls::parameters::toIndex;
using tls::parameters::ParameterSlot;
} // namespace

void TlsAudioProcessor::cacheParameterPointers()
{
    rawActiveSplitCountParameter = valueTreeState.getRawParameterValue(makeActiveSplitCountParameterId());

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        rawSoloParameters[bandIndex] = valueTreeState.getRawParameterValue(makeSoloParameterId(bandIndex));
        jassert(rawSoloParameters[bandIndex] != nullptr);
    }

    for (size_t parameterIndex = 0; parameterIndex < numCrossoverSlots; ++parameterIndex)
    {
        rawCrossoverParameters[parameterIndex] = valueTreeState.getRawParameterValue(makeFullbandParameterId(crossoverSpecs[parameterIndex].suffix));
        jassert(rawCrossoverParameters[parameterIndex] != nullptr);
    }

    for (size_t parameterIndex = 0; parameterIndex < numWidebandListenSlots; ++parameterIndex)
    {
        rawWidebandListenParameters[parameterIndex] = valueTreeState.getRawParameterValue(
            makeFullbandParameterId(tls::parameters::widebandListenSpecs[parameterIndex].suffix));
        jassert(rawWidebandListenParameters[parameterIndex] != nullptr);
    }

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        for (size_t parameterIndex = 0; parameterIndex < numParameterSlots; ++parameterIndex)
        {
            rawBandParameters[bandIndex][parameterIndex] = valueTreeState.getRawParameterValue(
                makeBandParameterId(bandIndex, parameterSpecs[parameterIndex].suffix));

            jassert(rawBandParameters[bandIndex][parameterIndex] != nullptr);
        }
    }
}

tls::dsp::DspCore::Parameters TlsAudioProcessor::readBandParameters(const size_t bandIndex) const
{
    const auto loadFloat = [this, bandIndex] (const ParameterSlot slot)
    {
        if (const auto* value = rawBandParameters[bandIndex][toIndex(slot)])
            return value->load();

        jassertfalse;
        return 0.0f;
    };
    const auto loadBool = [&loadFloat] (const ParameterSlot slot)
    {
        return loadFloat(slot) >= 0.5f;
    };

    tls::dsp::DspCore::Parameters parameters;
    parameters.gainMid = loadFloat(ParameterSlot::gainMid);
    parameters.gainMidMute = loadBool(ParameterSlot::gainMidMute);
    parameters.gainSide = loadFloat(ParameterSlot::gainSide);
    parameters.gainSideMute = loadBool(ParameterSlot::gainSideMute);
    parameters.gainL = loadFloat(ParameterSlot::gainL);
    parameters.gainLMute = loadBool(ParameterSlot::gainLMute);
    parameters.gainR = loadFloat(ParameterSlot::gainR);
    parameters.gainRMute = loadBool(ParameterSlot::gainRMute);
    parameters.gainLr = loadFloat(ParameterSlot::gainLr);
    parameters.gainLrMute = loadBool(ParameterSlot::gainLrMute);
    parameters.gainLOrder = juce::roundToInt(loadFloat(ParameterSlot::gainLOrder));
    parameters.gainROrder = juce::roundToInt(loadFloat(ParameterSlot::gainROrder));
    parameters.gainMidOrder = juce::roundToInt(loadFloat(ParameterSlot::gainMidOrder));
    parameters.gainSideOrder = juce::roundToInt(loadFloat(ParameterSlot::gainSideOrder));
    parameters.halfPositive = loadBool(ParameterSlot::halfPositive);
    parameters.halfNegative = loadBool(ParameterSlot::halfNegative);
    parameters.fullPositive = loadBool(ParameterSlot::fullPositive);
    parameters.fullNegative = loadBool(ParameterSlot::fullNegative);
    parameters.left = loadFloat(ParameterSlot::left);
    parameters.right = loadFloat(ParameterSlot::right);
    parameters.law = loadFloat(ParameterSlot::law);
    parameters.impact = loadFloat(ParameterSlot::impact);
    parameters.impactToRight = loadBool(ParameterSlot::impactDirection);
    parameters.mid = loadFloat(ParameterSlot::mid);
    parameters.side = loadFloat(ParameterSlot::side);
    parameters.degree = loadFloat(ParameterSlot::degree);
    parameters.flipRight = loadBool(ParameterSlot::flipRight);
    parameters.listenLc = loadBool(ParameterSlot::listenLc);
    parameters.listenRc = loadBool(ParameterSlot::listenRc);
    parameters.listenMc = loadBool(ParameterSlot::listenMc);
    parameters.listenSc = loadBool(ParameterSlot::listenSc);
    parameters.listenLl = loadBool(ParameterSlot::listenLl);
    parameters.listenRr = loadBool(ParameterSlot::listenRr);
    parameters.listenSs = loadBool(ParameterSlot::listenSs);
    parameters.depStereoMs = loadFloat(ParameterSlot::depStereo);
    parameters.depLeftMs = loadFloat(ParameterSlot::depLeft);
    parameters.depRightMs = loadFloat(ParameterSlot::depRight);
    parameters.depPhaseL = loadFloat(ParameterSlot::depPhaseL);
    parameters.depPhaseR = loadFloat(ParameterSlot::depPhaseR);

    return parameters;
}

tls::dsp::MultibandProcessor::CrossoverFrequencies TlsAudioProcessor::readCrossoverFrequencies() const
{
    tls::dsp::MultibandProcessor::CrossoverFrequencies frequencies {};

    for (size_t parameterIndex = 0; parameterIndex < frequencies.size(); ++parameterIndex)
    {
        if (const auto* value = rawCrossoverParameters[parameterIndex])
            frequencies[parameterIndex] = static_cast<double>(value->load());
        else
            jassertfalse;
    }

    return frequencies;
}

size_t TlsAudioProcessor::readActiveSplitCount() const
{
    if (const auto* value = rawActiveSplitCountParameter)
        return static_cast<size_t>(juce::jlimit(0, static_cast<int>(numCrossoverSlots), static_cast<int>(std::round(value->load()))));

    jassertfalse;
    return numCrossoverSlots;
}

tls::dsp::MultibandProcessor::SoloMask TlsAudioProcessor::readSoloMask() const
{
    tls::dsp::MultibandProcessor::SoloMask soloMask {};

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (const auto* value = rawSoloParameters[bandIndex])
            soloMask[bandIndex] = value->load() >= 0.5f;
        else
            jassertfalse;
    }

    return soloMask;
}

TlsAudioProcessor::WidebandListenMode TlsAudioProcessor::readWidebandListenMode() const noexcept
{
    const auto isEnabled = [this] (const size_t index)
    {
        const auto* parameter = rawWidebandListenParameters[index];
        return parameter != nullptr && parameter->load(std::memory_order_relaxed) >= 0.5f;
    };

    if (isEnabled(0)) return WidebandListenMode::leftCenter;
    if (isEnabled(1)) return WidebandListenMode::rightCenter;
    if (isEnabled(2)) return WidebandListenMode::midCenter;
    if (isEnabled(3)) return WidebandListenMode::sideCenter;
    if (isEnabled(4)) return WidebandListenMode::leftLeft;
    if (isEnabled(5)) return WidebandListenMode::rightRight;
    if (isEnabled(6)) return WidebandListenMode::sideStereo;
    return WidebandListenMode::neutral;
}

bool TlsAudioProcessor::syncParameters(const bool force)
{
    if (! force && ! parametersDirty.exchange(false, std::memory_order_acq_rel))
        return false;

    if (force)
        parametersDirty.store(false, std::memory_order_release);

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
        currentBandParameters[bandIndex] = readBandParameters(bandIndex);

    currentCrossoverFrequencies = readCrossoverFrequencies();
    currentActiveSplitCount = readActiveSplitCount();
    currentSoloMask = readSoloMask();
    currentWidebandListenMode = readWidebandListenMode();
    multibandProcessor.setBandParameters(currentBandParameters);
    multibandProcessor.setActiveSplitCount(currentActiveSplitCount);
    multibandProcessor.setCrossoverFrequencies(currentCrossoverFrequencies);
    multibandProcessor.setSoloMask(currentSoloMask);

    moduleLatencySamples.store(multibandProcessor.getLatencySamples(), std::memory_order_release);
    return true;
}
