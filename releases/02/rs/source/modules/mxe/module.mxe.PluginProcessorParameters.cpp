#include "module.mxe.PluginProcessor.h"

#include "module.mxe.ParameterIds.h"

#include <cmath>

namespace
{
using mxe::parameters::makeActiveSplitCountParameterId;
using mxe::parameters::makeBandParameterId;
using mxe::parameters::makeFullbandParameterId;
using mxe::parameters::makeModuleBypassParameterId;
using mxe::parameters::makeModuleBypassWithGainParameterId;
using mxe::parameters::makeSoloParameterId;
using mxe::parameters::crossoverSpecs;
using mxe::parameters::fullbandAutomationSpecs;
using mxe::parameters::fullbandVisibleSpecs;
using mxe::parameters::numCrossoverSlots;
using mxe::parameters::numFullbandAutomationSlots;
using mxe::parameters::numFullbandVisibleSlots;
using mxe::parameters::numParameterSlots;
using mxe::parameters::parameterSpecs;
using mxe::parameters::toIndex;
using mxe::parameters::FullbandAutomationSlot;
using mxe::parameters::FullbandVisibleSlot;
using mxe::parameters::ParameterSlot;
} // namespace

void MxeAudioProcessor::cacheParameterPointers()
{
    rawActiveSplitCountParameter = valueTreeState.getRawParameterValue(makeActiveSplitCountParameterId());
    jassert(rawActiveSplitCountParameter != nullptr);
    rawModuleBypassParameter = valueTreeState.getRawParameterValue(makeModuleBypassParameterId());
    jassert(rawModuleBypassParameter != nullptr);
    rawModuleBypassWithGainParameter = valueTreeState.getRawParameterValue(makeModuleBypassWithGainParameterId());
    jassert(rawModuleBypassWithGainParameter != nullptr);

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        rawSoloParameters[bandIndex] = valueTreeState.getRawParameterValue(makeSoloParameterId(bandIndex));
        jassert(rawSoloParameters[bandIndex] != nullptr);
    }

    for (size_t parameterIndex = 0; parameterIndex < numFullbandAutomationSlots; ++parameterIndex)
    {
        rawFullbandParameters[parameterIndex] = valueTreeState.getRawParameterValue(makeFullbandParameterId(fullbandAutomationSpecs[parameterIndex].suffix));
        jassert(rawFullbandParameters[parameterIndex] != nullptr);
    }

    for (size_t parameterIndex = 0; parameterIndex < numFullbandVisibleSlots; ++parameterIndex)
    {
        rawFullbandVisibleParameters[parameterIndex] = valueTreeState.getRawParameterValue(makeFullbandParameterId(fullbandVisibleSpecs[parameterIndex].suffix));
        jassert(rawFullbandVisibleParameters[parameterIndex] != nullptr);
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
    parameters.inGn = loadFloat(ParameterSlot::inGn);
    parameters.inRight = loadFloat(ParameterSlot::inRight);
    parameters.inLeft = loadFloat(ParameterSlot::inLeft);
    parameters.autoInGn = loadFloat(ParameterSlot::autoInGn);
    parameters.autoInRight = loadFloat(ParameterSlot::autoInRight);
    parameters.autoInLeft = loadFloat(ParameterSlot::autoInLeft);
    parameters.wide = loadFloat(ParameterSlot::wide);
    parameters.outGn = loadFloat(ParameterSlot::outGn);
    parameters.thLU = loadFloat(ParameterSlot::thLU);
    parameters.mkLU = loadFloat(ParameterSlot::mkLU);
    parameters.thLD = loadFloat(ParameterSlot::thLD);
    parameters.mkLD = loadFloat(ParameterSlot::mkLD);
    parameters.thRU = loadFloat(ParameterSlot::thRU);
    parameters.mkRU = loadFloat(ParameterSlot::mkRU);
    parameters.thRD = loadFloat(ParameterSlot::thRD);
    parameters.mkRD = loadFloat(ParameterSlot::mkRD);
    parameters.hwBypass = loadBool(ParameterSlot::hwBypass);
    parameters.LLThResh = loadFloat(ParameterSlot::LLThResh);
    parameters.LLTension = loadFloat(ParameterSlot::LLTension);
    parameters.LLRelease = loadFloat(ParameterSlot::LLRelease);
    parameters.LLmk = loadFloat(ParameterSlot::LLmk);
    parameters.RRThResh = loadFloat(ParameterSlot::RRThResh);
    parameters.RRTension = loadFloat(ParameterSlot::RRTension);
    parameters.RRRelease = loadFloat(ParameterSlot::RRRelease);
    parameters.RRmk = loadFloat(ParameterSlot::RRmk);
    parameters.DMbypass = loadBool(ParameterSlot::DMbypass);

    parameters.FFThResh = loadFloat(ParameterSlot::FFThResh);
    parameters.FFTension = loadFloat(ParameterSlot::FFTension);
    parameters.FFRelease = loadFloat(ParameterSlot::FFRelease);
    parameters.FFmk = loadFloat(ParameterSlot::FFmk);
    parameters.FFbypass = loadBool(ParameterSlot::FFbypass);
    parameters.moRph = loadFloat(ParameterSlot::moRph);
    parameters.peakHoldHz = loadFloat(ParameterSlot::peakHoldHz);
    parameters.TensionFlooR = loadFloat(ParameterSlot::TensionFlooR);
    parameters.TensionHysT = loadFloat(ParameterSlot::TensionHysT);
    parameters.delTa = loadBool(ParameterSlot::delTa);

    if (hasExternalParameterConnection())
    {
        const auto& externalBand = externalParameterConnection.bandParameters[bandIndex];
        parameters.inGn = loadExternalFloat(externalBand.inGainLr, parameters.inGn);
        parameters.inLeft = loadExternalFloat(externalBand.inGainL, parameters.inLeft);
        parameters.inRight = loadExternalFloat(externalBand.inGainR, parameters.inRight);
        parameters.wide = loadExternalFloat(externalBand.wide, parameters.wide);
        parameters.outGn = loadExternalFloat(externalBand.outGain, parameters.outGn);
    }

    return parameters;
}

mxe::dsp::MultibandProcessor::FullbandParameters MxeAudioProcessor::readFullbandParameters() const
{
    const auto loadAutomationFloat = [this] (const FullbandAutomationSlot slot)
    {
        if (const auto* value = rawFullbandParameters[static_cast<size_t>(slot)])
            return value->load();

        jassertfalse;
        return 0.0f;
    };

    const auto loadVisibleFloat = [this] (const FullbandVisibleSlot slot)
    {
        if (const auto* value = rawFullbandVisibleParameters[static_cast<size_t>(slot)])
            return value->load();

        jassertfalse;
        return 0.0f;
    };

    mxe::dsp::MultibandProcessor::FullbandParameters parameters;
    parameters.inGn = loadVisibleFloat(FullbandVisibleSlot::inGn);
    parameters.outGn = loadVisibleFloat(FullbandVisibleSlot::outGn);
    parameters.autoInGn = loadAutomationFloat(FullbandAutomationSlot::inGn);
    parameters.autoInRight = loadAutomationFloat(FullbandAutomationSlot::inRight);
    parameters.autoInLeft = loadAutomationFloat(FullbandAutomationSlot::inLeft);
    parameters.wide = loadVisibleFloat(FullbandVisibleSlot::wide);

    if (hasExternalParameterConnection())
    {
        parameters.inGn = loadExternalFloat(externalParameterConnection.inGainLr, parameters.inGn);
        parameters.autoInLeft = loadExternalFloat(externalParameterConnection.inGainL, parameters.autoInLeft);
        parameters.autoInRight = loadExternalFloat(externalParameterConnection.inGainR, parameters.autoInRight);
        parameters.wide = loadExternalFloat(externalParameterConnection.wide, parameters.wide);
        parameters.outGn = loadExternalFloat(externalParameterConnection.outGain, parameters.outGn);
    }

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

bool MxeAudioProcessor::isModuleBypassEnabled() const noexcept
{
    if (hasExternalParameterConnection())
        return loadExternalBool(externalParameterConnection.moduleBypass);

    return rawModuleBypassParameter != nullptr && rawModuleBypassParameter->load(std::memory_order_relaxed) >= 0.5f;
}

bool MxeAudioProcessor::isModuleBypassWithGainEnabled() const noexcept
{
    if (hasExternalParameterConnection())
        return loadExternalBool(externalParameterConnection.moduleBypassWithGain);

    return rawModuleBypassWithGainParameter != nullptr
        && rawModuleBypassWithGainParameter->load(std::memory_order_relaxed) >= 0.5f;
}

void MxeAudioProcessor::applyFullbandOutGain(juce::AudioBuffer<float>& buffer) const noexcept
{
    const auto outGain = juce::Decibels::decibelsToGain(juce::jlimit(-24.0f,
                                                                     24.0f,
                                                                     currentFullbandParameters.outGn));

    if (std::abs(outGain - 1.0f) <= 1.0e-6f)
        return;

    buffer.applyGain(outGain);
}

bool MxeAudioProcessor::hasExternalParameterConnection() const noexcept
{
    return externalParameterConnection.pushMirroredParameterValue != nullptr;
}

float MxeAudioProcessor::loadExternalFloat(std::atomic<float>* value, const float defaultValue) const noexcept
{
    return value != nullptr ? value->load(std::memory_order_relaxed) : defaultValue;
}

bool MxeAudioProcessor::loadExternalBool(std::atomic<float>* value, const bool defaultValue) const noexcept
{
    return value != nullptr ? value->load(std::memory_order_relaxed) >= 0.5f : defaultValue;
}

bool MxeAudioProcessor::syncParameters(const bool force)
{
    if (! hasExternalParameterConnection())
    {
        if (! force && ! parametersDirty.exchange(false, std::memory_order_acq_rel))
            return false;

        if (force)
            parametersDirty.store(false, std::memory_order_relaxed);
    }
    else
    {
        parametersDirty.store(false, std::memory_order_relaxed);
    }

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
        currentBandParameters[bandIndex] = readBandParameters(bandIndex);

    currentFullbandParameters = readFullbandParameters();
    currentCrossoverFrequencies = readCrossoverFrequencies();
    currentActiveSplitCount = readActiveSplitCount();
    currentSoloMask = readSoloMask();
    multibandProcessor.setBandParameters(currentBandParameters);
    multibandProcessor.setActiveSplitCount(currentActiveSplitCount);
    multibandProcessor.setCrossoverFrequencies(currentCrossoverFrequencies);
    multibandProcessor.setFullbandParameters(currentFullbandParameters);
    multibandProcessor.setSoloMask(currentSoloMask);

    moduleLatencySamples = (isModuleBypassEnabled() || isModuleBypassWithGainEnabled()) ? 0
                                                                                        : multibandProcessor.getLatencySamples();
    return true;
}
