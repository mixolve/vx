#include "module.dyn.PluginProcessor.h"

#include "module.dyn.ParameterIds.h"

#include <cmath>

namespace
{
using dyn::parameters::makeActiveSplitCountParameterId;
using dyn::parameters::makeBandParameterId;
using dyn::parameters::makeFullbandParameterId;
using dyn::parameters::makeSoloParameterId;
using dyn::parameters::crossoverSpecs;
using dyn::parameters::numCrossoverSlots;
using dyn::parameters::numParameterSlots;
using dyn::parameters::parameterSpecs;
using dyn::parameters::toIndex;
using dyn::parameters::ParameterSlot;
} // namespace

void DynAudioProcessor::cacheParameterPointers()
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

dyn::dsp::DspCore::Parameters DynAudioProcessor::readBandParameters(const size_t bandIndex) const
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

    dyn::dsp::DspCore::Parameters parameters;
    parameters.morph = loadFloat(ParameterSlot::morph);
    parameters.peakHoldMs = loadFloat(ParameterSlot::peakHoldMs);
    parameters.lookaheadMs = loadFloat(ParameterSlot::lookahead);
    parameters.tensionFloor = loadFloat(ParameterSlot::tensionFloor);
    parameters.tensionHysteresis = loadFloat(ParameterSlot::tensionHysteresis);
    parameters.releaseForm = juce::roundToInt(loadFloat(ParameterSlot::releaseForm));
    parameters.releaseCurve = loadFloat(ParameterSlot::releaseCurve);
    parameters.thLU = loadFloat(ParameterSlot::thLU);
    parameters.tensLU = loadFloat(ParameterSlot::tensLU);
    parameters.relLU = loadFloat(ParameterSlot::relLU);
    parameters.outLU = loadFloat(ParameterSlot::outLU);
    parameters.thLD = loadFloat(ParameterSlot::thLD);
    parameters.tensLD = loadFloat(ParameterSlot::tensLD);
    parameters.relLD = loadFloat(ParameterSlot::relLD);
    parameters.outLD = loadFloat(ParameterSlot::outLD);
    parameters.thRU = loadFloat(ParameterSlot::thRU);
    parameters.tensRU = loadFloat(ParameterSlot::tensRU);
    parameters.relRU = loadFloat(ParameterSlot::relRU);
    parameters.outRU = loadFloat(ParameterSlot::outRU);
    parameters.thRD = loadFloat(ParameterSlot::thRD);
    parameters.tensRD = loadFloat(ParameterSlot::tensRD);
    parameters.relRD = loadFloat(ParameterSlot::relRD);
    parameters.outRD = loadFloat(ParameterSlot::outRD);
    parameters.delta = loadBool(ParameterSlot::delta);

    return parameters;
}

dyn::dsp::MultibandProcessor::CrossoverFrequencies DynAudioProcessor::readCrossoverFrequencies() const
{
    dyn::dsp::MultibandProcessor::CrossoverFrequencies frequencies {};

    for (size_t parameterIndex = 0; parameterIndex < frequencies.size(); ++parameterIndex)
    {
        if (const auto* value = rawCrossoverParameters[parameterIndex])
            frequencies[parameterIndex] = static_cast<double>(value->load());
        else
            jassertfalse;
    }

    return frequencies;
}

size_t DynAudioProcessor::readActiveSplitCount() const
{
    if (const auto* value = rawActiveSplitCountParameter)
        return static_cast<size_t>(juce::jlimit(0, static_cast<int>(numCrossoverSlots), static_cast<int>(std::round(value->load()))));

    jassertfalse;
    return numCrossoverSlots;
}

dyn::dsp::MultibandProcessor::SoloMask DynAudioProcessor::readSoloMask() const
{
    dyn::dsp::MultibandProcessor::SoloMask soloMask {};

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (const auto* value = rawSoloParameters[bandIndex])
            soloMask[bandIndex] = value->load() >= 0.5f;
        else
            jassertfalse;
    }

    return soloMask;
}

bool DynAudioProcessor::syncParameters(const bool force)
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
