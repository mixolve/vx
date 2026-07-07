#include "module.mie.PluginProcessor.h"

#include "module.mie.ParameterIds.h"

#include <cmath>

namespace
{
using mie::parameters::makeActiveSplitCountParameterId;
using mie::parameters::makeBandParameterId;
using mie::parameters::makeFullbandParameterId;
using mie::parameters::makeSoloParameterId;
using mie::parameters::crossoverSpecs;
using mie::parameters::numCrossoverSlots;
using mie::parameters::numParameterSlots;
using mie::parameters::parameterSpecs;
using mie::parameters::toIndex;
using mie::parameters::ParameterSlot;
} // namespace

void MieAudioProcessor::cacheParameterPointers()
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

mie::dsp::DspCore::Parameters MieAudioProcessor::readBandParameters(const size_t bandIndex) const
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

    mie::dsp::DspCore::Parameters parameters;
    parameters.gainMid = loadFloat(ParameterSlot::gainMid);
    parameters.gainSide = loadFloat(ParameterSlot::gainSide);
    parameters.gainL = loadFloat(ParameterSlot::gainL);
    parameters.gainR = loadFloat(ParameterSlot::gainR);
    parameters.gainLr = loadFloat(ParameterSlot::gainLr);
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
    parameters.listenL = loadBool(ParameterSlot::listenL);
    parameters.listenR = loadBool(ParameterSlot::listenR);
    parameters.listenM = loadBool(ParameterSlot::listenM);
    parameters.listenS = loadBool(ParameterSlot::listenS);
    parameters.listenInPlace = loadBool(ParameterSlot::listenInPlace);
    parameters.depStereoMs = loadFloat(ParameterSlot::depStereo);
    parameters.depRightMs = loadFloat(ParameterSlot::depRight);
    parameters.depBufferMs = loadFloat(ParameterSlot::depBuffer);
    parameters.depPhaseL = loadFloat(ParameterSlot::depPhaseL);
    parameters.depPhaseR = loadFloat(ParameterSlot::depPhaseR);

    return parameters;
}

mie::dsp::MultibandProcessor::CrossoverFrequencies MieAudioProcessor::readCrossoverFrequencies() const
{
    mie::dsp::MultibandProcessor::CrossoverFrequencies frequencies {};

    for (size_t parameterIndex = 0; parameterIndex < frequencies.size(); ++parameterIndex)
    {
        if (const auto* value = rawCrossoverParameters[parameterIndex])
            frequencies[parameterIndex] = static_cast<double>(value->load());
        else
            jassertfalse;
    }

    return frequencies;
}

size_t MieAudioProcessor::readActiveSplitCount() const
{
    if (const auto* value = rawActiveSplitCountParameter)
        return static_cast<size_t>(juce::jlimit(0, static_cast<int>(numCrossoverSlots), static_cast<int>(std::round(value->load()))));

    jassertfalse;
    return numCrossoverSlots;
}

mie::dsp::MultibandProcessor::SoloMask MieAudioProcessor::readSoloMask() const
{
    mie::dsp::MultibandProcessor::SoloMask soloMask {};

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (const auto* value = rawSoloParameters[bandIndex])
            soloMask[bandIndex] = value->load() >= 0.5f;
        else
            jassertfalse;
    }

    return soloMask;
}

bool MieAudioProcessor::syncParameters(const bool force)
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
    multibandProcessor.setBandParameters(currentBandParameters);
    multibandProcessor.setActiveSplitCount(currentActiveSplitCount);
    multibandProcessor.setCrossoverFrequencies(currentCrossoverFrequencies);
    multibandProcessor.setSoloMask(currentSoloMask);

    moduleLatencySamples = multibandProcessor.getLatencySamples();
    return true;
}
