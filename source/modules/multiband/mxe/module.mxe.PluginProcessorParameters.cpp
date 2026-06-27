#include "module.mxe.PluginProcessor.h"

#include "module.mxe.ParameterIds.h"

#include <cmath>

namespace
{
using mxe::parameters::makeActiveSplitCountParameterId;
using mxe::parameters::makeBandParameterId;
using mxe::parameters::makeFullbandParameterId;
using mxe::parameters::makeSoloParameterId;
using mxe::parameters::crossoverSpecs;
using mxe::parameters::numCrossoverSlots;
using mxe::parameters::numParameterSlots;
using mxe::parameters::parameterSpecs;
using mxe::parameters::toIndex;
using mxe::parameters::ParameterSlot;
} // namespace

void MxeAudioProcessor::cacheParameterPointers()
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

mxe::dsp::DspCore::Parameters MxeAudioProcessor::readBandParameters(const size_t bandIndex) const
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

    mxe::dsp::DspCore::Parameters parameters;
    parameters.moRph = loadFloat(ParameterSlot::moRph);
    parameters.peakHoldHz = loadFloat(ParameterSlot::peakHoldHz);
    parameters.TensionFlooR = loadFloat(ParameterSlot::TensionFlooR);
    parameters.TensionHysT = loadFloat(ParameterSlot::TensionHysT);
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
    parameters.delTa = loadBool(ParameterSlot::delTa);

    return parameters;
}

mxe::dsp::MultibandProcessor::CrossoverFrequencies MxeAudioProcessor::readCrossoverFrequencies() const
{
    mxe::dsp::MultibandProcessor::CrossoverFrequencies frequencies {};

    for (size_t parameterIndex = 0; parameterIndex < frequencies.size(); ++parameterIndex)
    {
        if (const auto* value = rawCrossoverParameters[parameterIndex])
            frequencies[parameterIndex] = static_cast<double>(value->load());
        else
            jassertfalse;
    }

    return frequencies;
}

size_t MxeAudioProcessor::readActiveSplitCount() const
{
    if (const auto* value = rawActiveSplitCountParameter)
        return static_cast<size_t>(juce::jlimit(0, static_cast<int>(numCrossoverSlots), static_cast<int>(std::round(value->load()))));

    jassertfalse;
    return numCrossoverSlots;
}

mxe::dsp::MultibandProcessor::SoloMask MxeAudioProcessor::readSoloMask() const
{
    mxe::dsp::MultibandProcessor::SoloMask soloMask {};

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (const auto* value = rawSoloParameters[bandIndex])
            soloMask[bandIndex] = value->load() >= 0.5f;
        else
            jassertfalse;
    }

    return soloMask;
}

bool MxeAudioProcessor::syncParameters(const bool force)
{
    juce::ignoreUnused(force);
    parametersDirty.store(false, std::memory_order_relaxed);

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
