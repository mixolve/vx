#include "module.spe.SpeProcessor.h"
#include "module.spe.ProcessorConstants.h"

#include <cmath>

namespace
{
juce::ValueTree buildCombinedSpeState(juce::AudioProcessorValueTreeState& parameters,
                                      const juce::ValueTree& analyserState)
{
    auto state = parameters.copyState();
    state.appendChild(analyserState.createCopy(), nullptr);
    return state;
}

constexpr std::array<const char*, 6> dualMonoLinkedParameterIds {
    SpeModuleProcessor::paramDualMonoLeftThresholdId,
    SpeModuleProcessor::paramDualMonoRightThresholdId,
    SpeModuleProcessor::paramDualMonoLeftAdaptiveId,
    SpeModuleProcessor::paramDualMonoRightAdaptiveId,
    SpeModuleProcessor::paramDualMonoLeftAdaptiveOffsetId,
    SpeModuleProcessor::paramDualMonoRightAdaptiveOffsetId
};
}

SpeModuleProcessor::SpeModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
    parameters(internalParameterHost, nullptr, "spe_state", createParameterLayout())
{
    resetAnalyserState();
    dualMonoLeftThresholdParam = parameters.getRawParameterValue(paramDualMonoLeftThresholdId);
    dualMonoRightThresholdParam = parameters.getRawParameterValue(paramDualMonoRightThresholdId);
    dualMonoLeftAdaptiveParam = parameters.getRawParameterValue(paramDualMonoLeftAdaptiveId);
    dualMonoRightAdaptiveParam = parameters.getRawParameterValue(paramDualMonoRightAdaptiveId);
    dualMonoLeftAdaptiveOffsetParam = parameters.getRawParameterValue(paramDualMonoLeftAdaptiveOffsetId);
    dualMonoRightAdaptiveOffsetParam = parameters.getRawParameterValue(paramDualMonoRightAdaptiveOffsetId);
    dualMonoLinkParam = parameters.getRawParameterValue(paramDualMonoLinkId);
    attackParam = parameters.getRawParameterValue(paramAttackId);
    releaseParam = parameters.getRawParameterValue(paramReleaseId);
    kneeParam = parameters.getRawParameterValue(paramKneeId);
    ratioParam = parameters.getRawParameterValue(paramRatioId);
    deltaParam = parameters.getRawParameterValue(paramDeltaId);
    dspFftSizeParam = parameters.getRawParameterValue(paramDspFftSizeId);
    dspHopDivisorParam = parameters.getRawParameterValue(paramDspHopDivisorId);
    dspSlopeParam = parameters.getRawParameterValue(paramDspSlopeId);
    phaseFilterCountParam = parameters.getRawParameterValue(paramPhaseFilterCountId);
    amplitudeFilterCountParam = parameters.getRawParameterValue(paramAmplitudeFilterCountId);

    for (auto filterIndex = 0; filterIndex < maxSpeFilterCount; ++filterIndex)
    {
        phaseTypeParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getPhaseFilterTypeParamId(filterIndex));
        phasePlaceParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getPhaseFilterPlaceParamId(filterIndex));
        phaseSlopeParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getPhaseFilterSlopeParamId(filterIndex));
        phaseFrequencyParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getPhaseFilterFrequencyParamId(filterIndex));
        phaseBandwidthParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getPhaseFilterBandwidthParamId(filterIndex));
        phaseImpactParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getPhaseFilterImpactParamId(filterIndex));
        phaseBypassParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getPhaseFilterBypassParamId(filterIndex));
        amplitudeTypeParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getAmplitudeFilterTypeParamId(filterIndex));
        amplitudePlaceParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getAmplitudeFilterPlaceParamId(filterIndex));
        amplitudeSlopeParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getAmplitudeFilterSlopeParamId(filterIndex));
        amplitudeFrequencyParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getAmplitudeFilterFrequencyParamId(filterIndex));
        amplitudeBandwidthParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getAmplitudeFilterBandwidthParamId(filterIndex));
        amplitudeImpactParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getAmplitudeFilterImpactParamId(filterIndex));
        amplitudeBypassParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getAmplitudeFilterBypassParamId(filterIndex));
    }

    for (const auto* parameterId : dualMonoLinkedParameterIds)
        parameters.addParameterListener(parameterId, this);
}

SpeModuleProcessor::~SpeModuleProcessor()
{
    for (const auto* parameterId : dualMonoLinkedParameterIds)
        parameters.removeParameterListener(parameterId, this);
}

void SpeModuleProcessor::prepareToPlay(double sampleRate, int)
{
    preparedBlockSize = juce::jmax(1, ownerProcessor.getBlockSize());
    spectralCompressor.prepare(sampleRate, ownerProcessor.getTotalNumInputChannels());
    refreshLatencyState();
    resetDeltaDelay();
    outputAnalyser.prepare(sampleRate);
    deltaDryBuffer.setSize(ownerProcessor.getTotalNumInputChannels(), preparedBlockSize);
}

void SpeModuleProcessor::releaseResources()
{
    resetDeltaDelay();
}

void SpeModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const auto analysisSettings = getAnalysisSettings();
    auto compressorSettings = getCompressorSettings();
    const auto deltaEnabled = isDeltaEnabled();
    const auto channelsToUse = juce::jmin(ownerProcessor.getTotalNumInputChannels(), buffer.getNumChannels());
    const auto desiredLatencySamples = juce::jmax(0, compressorSettings.fftSize - 1);

    if (desiredLatencySamples != activeLatencySamples)
    {
        activeLatencySamples = desiredLatencySamples;
        resetDeltaDelay();
        ensureDeltaDryBufferSize(channelsToUse, buffer.getNumSamples());
    }

    jassert(deltaDryBuffer.getNumChannels() >= channelsToUse && deltaDryBuffer.getNumSamples() >= buffer.getNumSamples());
    populateAlignedDryBuffer(buffer, deltaDryBuffer, channelsToUse, activeLatencySamples);

    for (auto channel = ownerProcessor.getTotalNumInputChannels(); channel < ownerProcessor.getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (deltaEnabled)
        compressorSettings.makeupDb = 0.0f;

    spectralCompressor.processBuffer(buffer, channelsToUse, compressorSettings);

    if (deltaEnabled)
    {
        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            buffer.applyGain(channel, 0, buffer.getNumSamples(), -1.0f);
            buffer.addFrom(channel, 0, deltaDryBuffer, channel, 0, buffer.getNumSamples());
        }
    }

    outputAnalyser.pushBuffer(buffer,
                              channelsToUse,
                              analysisSettings.fftSize,
                              analysisSettings.overlapFactor,
                              analysisSettings.averagingTimeMs);
}

void SpeModuleProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (linkedDualMonoPropagationInProgress.exchange(true, std::memory_order_acq_rel))
        return;

    const auto linkActive = dualMonoLinkParam != nullptr
        && dualMonoLinkParam->load(std::memory_order_relaxed) >= 0.5f;

    const auto mirrorParameter = [this] (const char* sourceParameterId, const char* targetParameterId)
    {
        const auto* source = parameters.getRawParameterValue(sourceParameterId);

        if (source == nullptr)
            return;

        auto* target = parameters.getParameter(targetParameterId);

        if (target == nullptr)
            return;

        const auto targetValue = source->load(std::memory_order_relaxed);
        const auto normalizedValue = target->convertTo0to1(targetValue);

        if (std::abs(target->getValue() - normalizedValue) <= 1.0e-6f)
            return;

        target->setValueNotifyingHost(normalizedValue);
    };

    if (linkActive)
    {
        if (parameterID == paramDualMonoLeftThresholdId)
            mirrorParameter(paramDualMonoLeftThresholdId, paramDualMonoRightThresholdId);
        else if (parameterID == paramDualMonoRightThresholdId)
            mirrorParameter(paramDualMonoRightThresholdId, paramDualMonoLeftThresholdId);
        else if (parameterID == paramDualMonoLeftAdaptiveId)
            mirrorParameter(paramDualMonoLeftAdaptiveId, paramDualMonoRightAdaptiveId);
        else if (parameterID == paramDualMonoRightAdaptiveId)
            mirrorParameter(paramDualMonoRightAdaptiveId, paramDualMonoLeftAdaptiveId);
        else if (parameterID == paramDualMonoLeftAdaptiveOffsetId)
            mirrorParameter(paramDualMonoLeftAdaptiveOffsetId, paramDualMonoRightAdaptiveOffsetId);
        else if (parameterID == paramDualMonoRightAdaptiveOffsetId)
            mirrorParameter(paramDualMonoRightAdaptiveOffsetId, paramDualMonoLeftAdaptiveOffsetId);
    }

    linkedDualMonoPropagationInProgress.store(false, std::memory_order_release);
}

void SpeModuleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = buildCombinedSpeState(parameters, createAnalyserStateSnapshot());

    if (auto stateXml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary(*stateXml, destData);
}

void SpeModuleProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto stateXml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes))
        if (stateXml->hasTagName(parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml(*stateXml);
            auto restoredAnalyserState = state.getChildWithName(analyserState.getType());

            if (restoredAnalyserState.isValid())
                state.removeChild(state.indexOf(restoredAnalyserState), nullptr);

            parameters.replaceState(state);
            applyAnalyserState(restoredAnalyserState);
            refreshLatencyState();
        }
}

juce::String SpeModuleProcessor::getStateXmlString() const
{
    auto state = buildCombinedSpeState(const_cast<juce::AudioProcessorValueTreeState&>(parameters),
                                       createAnalyserStateSnapshot());

    if (auto stateXml = state.createXml())
        return stateXml->toString();

    return {};
}

void SpeModuleProcessor::setStateFromXmlString(const juce::String& stateXmlString)
{
    if (stateXmlString.trim().isEmpty())
        return;

    auto stateXml = juce::parseXML(stateXmlString);

    if (stateXml == nullptr || ! stateXml->hasTagName(parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml(*stateXml);
    auto restoredAnalyserState = state.getChildWithName(analyserState.getType());

    if (restoredAnalyserState.isValid())
        state.removeChild(state.indexOf(restoredAnalyserState), nullptr);

    parameters.replaceState(state);
    applyAnalyserState(restoredAnalyserState);
    refreshLatencyState();
}

int SpeModuleProcessor::getLatencySamples() const noexcept
{
    return activeLatencySamples;
}

bool SpeModuleProcessor::refreshLatencyState() noexcept
{
    const auto newLatencySamples = juce::jmax(0, getSelectedDspFftSize() - 1);
    const auto changed = activeLatencySamples != newLatencySamples;
    activeLatencySamples = newLatencySamples;
    return changed;
}

void SpeModuleProcessor::copyAnalyserData(std::array<float, analyserScopeSize>& destination,
                                          double& currentSampleRate) const
{
    outputAnalyser.copyScope(destination, currentSampleRate);
}

void SpeModuleProcessor::copyGainReductionData(std::array<float, analyserScopeSize>& leftDestination,
                                               std::array<float, analyserScopeSize>& rightDestination) const
{
    spectralCompressor.copyReductionScope(leftDestination, rightDestination);
}

void SpeModuleProcessor::resetDeltaDelay() noexcept
{
    deltaDelayWriteIndex = 0;
    deltaDryBuffer.clear();

    for (auto& channelBuffer : deltaDelayBuffers)
        channelBuffer.fill(0.0f);
}

void SpeModuleProcessor::ensureDeltaDryBufferSize(const int channels, const int samples)
{
    const auto requiredChannels = juce::jmax(0, channels);
    const auto requiredSamples = juce::jmax(0, samples);

    if (deltaDryBuffer.getNumChannels() < requiredChannels
        || deltaDryBuffer.getNumSamples() < requiredSamples)
    {
        deltaDryBuffer.setSize(requiredChannels, requiredSamples, false, false, true);
    }
}

void SpeModuleProcessor::populateAlignedDryBuffer(const juce::AudioBuffer<float>& inputBuffer,
                                                  juce::AudioBuffer<float>& delayedDryBuffer,
                                                  int channelsToUse,
                                                  int latencySamples) noexcept
{
    delayedDryBuffer.clear();

    if (channelsToUse <= 0)
        return;

    const auto delaySamples = juce::jlimit(0, deltaDelayBufferSize - 1, juce::jmax(0, latencySamples));

    if (delaySamples == 0)
    {
        for (auto channel = 0; channel < channelsToUse; ++channel)
            delayedDryBuffer.copyFrom(channel, 0, inputBuffer, channel, 0, inputBuffer.getNumSamples());

        return;
    }

    for (auto sampleIndex = 0; sampleIndex < inputBuffer.getNumSamples(); ++sampleIndex)
    {
        auto readIndex = deltaDelayWriteIndex - delaySamples;

        if (readIndex < 0)
            readIndex += deltaDelayBufferSize;

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            auto& delayBuffer = deltaDelayBuffers[static_cast<size_t>(channel)];
            delayedDryBuffer.setSample(channel, sampleIndex, delayBuffer[static_cast<size_t>(readIndex)]);
            delayBuffer[static_cast<size_t>(deltaDelayWriteIndex)] = inputBuffer.getSample(channel, sampleIndex);
        }

        deltaDelayWriteIndex = (deltaDelayWriteIndex + 1) % deltaDelayBufferSize;
    }
}

juce::AudioProcessorValueTreeState& SpeModuleProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& SpeModuleProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

juce::ValueTree SpeModuleProcessor::getAnalyserState() const
{
    return createAnalyserStateSnapshot();
}

juce::ValueTree SpeModuleProcessor::createAnalyserStateSnapshot() const
{
    auto state = juce::ValueTree(analyserState.getType());
    state.setProperty(paramFftSizeId, analyserFftSizeValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramOverlapId, analyserOverlapValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramLeftId, analyserLeftValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramRightId, analyserRightValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramRangeLowId, analyserRangeLowValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramRangeHighId, analyserRangeHighValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramSlopeId, analyserSlopeValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramTimeId, analyserTimeValue.load(std::memory_order_relaxed), nullptr);
    return state;
}

float SpeModuleProcessor::getAnalyserParameterValue(const juce::String& parameterId) const noexcept
{
    if (parameterId == paramFftSizeId)
        return analyserFftSizeValue.load(std::memory_order_relaxed);

    if (parameterId == paramOverlapId)
        return analyserOverlapValue.load(std::memory_order_relaxed);

    if (parameterId == paramLeftId)
        return analyserLeftValue.load(std::memory_order_relaxed);

    if (parameterId == paramRightId)
        return analyserRightValue.load(std::memory_order_relaxed);

    if (parameterId == paramRangeLowId)
        return analyserRangeLowValue.load(std::memory_order_relaxed);

    if (parameterId == paramRangeHighId)
        return analyserRangeHighValue.load(std::memory_order_relaxed);

    if (parameterId == paramSlopeId)
        return analyserSlopeValue.load(std::memory_order_relaxed);

    if (parameterId == paramTimeId)
        return analyserTimeValue.load(std::memory_order_relaxed);

    return 0.0f;
}

void SpeModuleProcessor::setAnalyserParameterValue(const juce::String& parameterId, float value)
{
    if (parameterId == paramFftSizeId)
    {
        const auto clamped = static_cast<float>(juce::jlimit(0, 4, juce::roundToInt(value)));
        analyserFftSizeValue.store(clamped, std::memory_order_relaxed);
        return;
    }

    if (parameterId == paramOverlapId)
    {
        const auto clamped = static_cast<float>(juce::jlimit(0, 4, juce::roundToInt(value)));
        analyserOverlapValue.store(clamped, std::memory_order_relaxed);
        return;
    }

    if (parameterId == paramLeftId || parameterId == paramRightId)
    {
        auto left = analyserLeftValue.load(std::memory_order_relaxed);
        auto right = analyserRightValue.load(std::memory_order_relaxed);

        if (parameterId == paramLeftId)
            left = juce::jlimit(0.0f, 1000.0f, value);
        else
            right = juce::jlimit(1000.0f, analyserMaxFrequency, value);

        if (right <= left)
        {
            if (parameterId == paramLeftId)
                right = juce::jmin(analyserMaxFrequency, left + 1.0f);
            else
                left = juce::jmax(0.0f, juce::jmin(1000.0f, right - 1.0f));
        }

        analyserLeftValue.store(left, std::memory_order_relaxed);
        analyserRightValue.store(right, std::memory_order_relaxed);
        return;
    }

    if (parameterId == paramRangeLowId || parameterId == paramRangeHighId)
    {
        auto low = analyserRangeLowValue.load(std::memory_order_relaxed);
        auto high = analyserRangeHighValue.load(std::memory_order_relaxed);

        if (parameterId == paramRangeLowId)
            low = juce::jlimit(analyserMinDecibels, analyserMaxDecibels - 6.0f, value);
        else
            high = juce::jlimit(analyserMinDecibels + 6.0f, analyserMaxDecibels, value);

        if (high < low + 6.0f)
        {
            if (parameterId == paramRangeLowId)
                high = juce::jmin(analyserMaxDecibels, low + 6.0f);
            else
                low = juce::jmax(analyserMinDecibels, high - 6.0f);
        }

        analyserRangeLowValue.store(low, std::memory_order_relaxed);
        analyserRangeHighValue.store(high, std::memory_order_relaxed);
        return;
    }

    if (parameterId == paramSlopeId)
    {
        const auto clamped = juce::jlimit(0.0f, 6.0f, value);
        analyserSlopeValue.store(clamped, std::memory_order_relaxed);
        return;
    }

    if (parameterId == paramTimeId)
    {
        const auto clamped = juce::jlimit(0.0f, 1000.0f, value);
        analyserTimeValue.store(clamped, std::memory_order_relaxed);
    }
}

void SpeModuleProcessor::resetAnalyserState()
{
    setAnalyserParameterValue(paramFftSizeId, 2.0f);
    setAnalyserParameterValue(paramOverlapId, 4.0f);
    setAnalyserParameterValue(paramLeftId, 21.0f);
    setAnalyserParameterValue(paramRightId, 20000.0f);
    setAnalyserParameterValue(paramRangeLowId, -60.0f);
    setAnalyserParameterValue(paramRangeHighId, 10.0f);
    setAnalyserParameterValue(paramSlopeId, 4.5f);
    setAnalyserParameterValue(paramTimeId, 50.0f);
}

void SpeModuleProcessor::applyAnalyserState(juce::ValueTree state)
{
    if (! state.isValid())
    {
        resetAnalyserState();
        return;
    }

    setAnalyserParameterValue(paramFftSizeId, static_cast<float>(state.getProperty(paramFftSizeId, 2.0f)));
    setAnalyserParameterValue(paramOverlapId, static_cast<float>(state.getProperty(paramOverlapId, 4.0f)));
    setAnalyserParameterValue(paramLeftId, static_cast<float>(state.getProperty(paramLeftId, 21.0f)));
    setAnalyserParameterValue(paramRightId, static_cast<float>(state.getProperty(paramRightId, 20000.0f)));
    setAnalyserParameterValue(paramRangeLowId, static_cast<float>(state.getProperty(paramRangeLowId, -60.0f)));
    setAnalyserParameterValue(paramRangeHighId, static_cast<float>(state.getProperty(paramRangeHighId, 10.0f)));
    setAnalyserParameterValue(paramSlopeId, static_cast<float>(state.getProperty(paramSlopeId, 4.5f)));
    setAnalyserParameterValue(paramTimeId, static_cast<float>(state.getProperty(paramTimeId, 50.0f)));
}
