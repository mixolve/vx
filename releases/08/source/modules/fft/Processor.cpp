#include "Processor.h"
#include "Constants.h"

#include <cmath>

namespace
{
juce::ValueTree buildCombinedFftState(juce::AudioProcessorValueTreeState& parameters,
                                      const juce::ValueTree& analyserState)
{
    auto state = parameters.copyState();
    state.appendChild(analyserState.createCopy(), nullptr);
    return state;
}

constexpr std::array<const char*, 4> dualMonoLinkedParameterIds {
    FftModuleProcessor::paramDualMonoLeftThresholdId,
    FftModuleProcessor::paramDualMonoRightThresholdId,
    FftModuleProcessor::paramDualMonoLeftAdaptiveId,
    FftModuleProcessor::paramDualMonoRightAdaptiveId
};

}

FftModuleProcessor::FftModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
    parameters(parameterHost, nullptr, "fft_state", createParameterLayout())
{
    resetAnalyserState();
    dualMonoLeftThresholdParam = parameters.getRawParameterValue(paramDualMonoLeftThresholdId);
    dualMonoRightThresholdParam = parameters.getRawParameterValue(paramDualMonoRightThresholdId);
    phaseThresholdParam = parameters.getRawParameterValue(paramPhaseThresholdId);
    phaseAdaptiveParam = parameters.getRawParameterValue(paramPhaseAdaptiveId);
    phaseSlopeParam = parameters.getRawParameterValue(paramPhaseSlopeId);
    phaseImpactParam = parameters.getRawParameterValue(paramPhaseImpactId);
    dualMonoLeftAdaptiveParam = parameters.getRawParameterValue(paramDualMonoLeftAdaptiveId);
    dualMonoRightAdaptiveParam = parameters.getRawParameterValue(paramDualMonoRightAdaptiveId);
    spectralAdaptiveOffsetParam = parameters.getRawParameterValue(paramSpectralAdaptiveOffsetId);
    spectralAdaptiveAttackParam = parameters.getRawParameterValue(paramSpectralAdaptiveAttackId);
    spectralAdaptiveHoldParam = parameters.getRawParameterValue(paramSpectralAdaptiveHoldId);
    spectralAdaptiveReleaseParam = parameters.getRawParameterValue(paramSpectralAdaptiveReleaseId);
    phaseAdaptiveOffsetParam = parameters.getRawParameterValue(paramPhaseAdaptiveOffsetId);
    phaseAdaptiveAttackParam = parameters.getRawParameterValue(paramPhaseAdaptiveAttackId);
    phaseAdaptiveHoldParam = parameters.getRawParameterValue(paramPhaseAdaptiveHoldId);
    phaseAdaptiveReleaseParam = parameters.getRawParameterValue(paramPhaseAdaptiveReleaseId);
    dualMonoLinkParam = parameters.getRawParameterValue(paramDualMonoLinkId);
    dynamicBypassParam = parameters.getRawParameterValue(paramDynamicBypassId);
    dynamicModeParam = parameters.getRawParameterValue(paramDynamicModeId);
    floorParam = parameters.getRawParameterValue(paramFloorId);
    attackParam = parameters.getRawParameterValue(paramAttackId);
    releaseParam = parameters.getRawParameterValue(paramReleaseId);
    kneeParam = parameters.getRawParameterValue(paramKneeId);
    ratioParam = parameters.getRawParameterValue(paramRatioId);
    deltaParam = parameters.getRawParameterValue(paramDeltaId);
    dspFftSizeParam = parameters.getRawParameterValue(paramDspFftSizeId);
    dspOverlapParam = parameters.getRawParameterValue(paramDspOverlapId);
    dspSlopeParam = parameters.getRawParameterValue(paramDspSlopeId);
    for (const auto* parameterId : dualMonoLinkedParameterIds)
        parameters.addParameterListener(parameterId, this);
}

FftModuleProcessor::~FftModuleProcessor()
{
    for (const auto* parameterId : dualMonoLinkedParameterIds)
        parameters.removeParameterListener(parameterId, this);
}

void FftModuleProcessor::prepareToPlay(double sampleRate, int)
{
    preparedBlockSize = juce::jmax(1, ownerProcessor.getBlockSize());
    dynamicProcessor.prepare(sampleRate, ownerProcessor.getTotalNumInputChannels());
    refreshLatencyState();
    resetDeltaDelay();
    deltaDryBuffer.setSize(ownerProcessor.getTotalNumInputChannels(), preparedBlockSize);
}

void FftModuleProcessor::releaseResources()
{
    resetDeltaDelay();
}

void FftModuleProcessor::resetProcessingState() noexcept
{
    dynamicProcessor.reset();
    resetDeltaDelay();
}

void FftModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

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

    dynamicProcessor.processBuffer(buffer, channelsToUse, compressorSettings);

    if (deltaEnabled)
    {
        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            buffer.applyGain(channel, 0, buffer.getNumSamples(), -1.0f);
            buffer.addFrom(channel, 0, deltaDryBuffer, channel, 0, buffer.getNumSamples());
        }
    }

}

void FftModuleProcessor::parameterChanged(const juce::String& parameterID, float)
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
    }

    linkedDualMonoPropagationInProgress.store(false, std::memory_order_release);
}

void FftModuleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = buildCombinedFftState(parameters, createAnalyserStateSnapshot());

    if (auto stateXml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary(*stateXml, destData);
}

void FftModuleProcessor::setStateInformation(const void* data, int sizeInBytes)
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

juce::String FftModuleProcessor::getStateXmlString() const
{
    auto state = buildCombinedFftState(const_cast<juce::AudioProcessorValueTreeState&>(parameters),
                                       createAnalyserStateSnapshot());

    if (auto stateXml = state.createXml())
        return stateXml->toString();

    return {};
}

void FftModuleProcessor::setStateFromXmlString(const juce::String& stateXmlString)
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

int FftModuleProcessor::getLatencySamples() const noexcept
{
    return activeLatencySamples;
}

bool FftModuleProcessor::refreshLatencyState() noexcept
{
    const auto newLatencySamples = juce::jmax(0, getSelectedDspFftSize() - 1);
    const auto changed = activeLatencySamples != newLatencySamples;
    activeLatencySamples = newLatencySamples;
    return changed;
}

void FftModuleProcessor::copyGainReductionData(std::array<float, analyserScopeSize>& leftDestination,
                                               std::array<float, analyserScopeSize>& rightDestination) const
{
    dynamicProcessor.copyReductionScope(leftDestination, rightDestination);
}

bool FftModuleProcessor::isPhaseCorrMode() const noexcept
{
    return dynamicModeParam != nullptr
        && dynamicModeParam->load(std::memory_order_relaxed) >= 0.5f;
}

float FftModuleProcessor::getReductionDisplayFloor() const noexcept
{
    return isPhaseCorrMode()
        ? -2.0f * phaseReductionRangeValue.load(std::memory_order_relaxed) * 0.01f
        : spectralReductionRangeValue.load(std::memory_order_relaxed);
}

void FftModuleProcessor::resetDeltaDelay() noexcept
{
    deltaDelayWriteIndex = 0;
    deltaDryBuffer.clear();

    for (auto& channelBuffer : deltaDelayBuffers)
        channelBuffer.fill(0.0f);
}

void FftModuleProcessor::ensureDeltaDryBufferSize(const int channels, const int samples)
{
    const auto requiredChannels = juce::jmax(0, channels);
    const auto requiredSamples = juce::jmax(0, samples);

    if (deltaDryBuffer.getNumChannels() < requiredChannels
        || deltaDryBuffer.getNumSamples() < requiredSamples)
    {
        deltaDryBuffer.setSize(requiredChannels, requiredSamples, false, false, true);
    }
}

void FftModuleProcessor::populateAlignedDryBuffer(const juce::AudioBuffer<float>& inputBuffer,
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

juce::AudioProcessorValueTreeState& FftModuleProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& FftModuleProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

juce::ValueTree FftModuleProcessor::createAnalyserStateSnapshot() const
{
    auto state = juce::ValueTree(analyserState.getType());
    state.setProperty(paramTimeId, analyserTimeValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramSpectralReductionRangeId, spectralReductionRangeValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramPhaseReductionRangeId, phaseReductionRangeValue.load(std::memory_order_relaxed), nullptr);
    return state;
}

float FftModuleProcessor::getAnalyserParameterValue(const juce::String& parameterId) const noexcept
{
    if (parameterId == paramTimeId)
        return analyserTimeValue.load(std::memory_order_relaxed);

    if (parameterId == paramSpectralReductionRangeId)
        return spectralReductionRangeValue.load(std::memory_order_relaxed);

    if (parameterId == paramPhaseReductionRangeId)
        return phaseReductionRangeValue.load(std::memory_order_relaxed);

    return 0.0f;
}

void FftModuleProcessor::setAnalyserParameterValue(const juce::String& parameterId, float value)
{
    if (parameterId == paramTimeId)
    {
        const auto clamped = juce::jlimit(0.0f, 1000.0f, value);
        analyserTimeValue.store(clamped, std::memory_order_relaxed);
    }

    if (parameterId == paramSpectralReductionRangeId)
    {
        spectralReductionRangeValue.store(juce::jlimit(-99.0f, 0.0f, value), std::memory_order_relaxed);
        return;
    }

    if (parameterId == paramPhaseReductionRangeId)
        phaseReductionRangeValue.store(juce::jlimit(0.0f, 100.0f, value), std::memory_order_relaxed);
}

void FftModuleProcessor::resetAnalyserState()
{
    setAnalyserParameterValue(paramTimeId, 50.0f);
    setAnalyserParameterValue(paramSpectralReductionRangeId, -36.0f);
    setAnalyserParameterValue(paramPhaseReductionRangeId, 50.0f);
}

void FftModuleProcessor::applyAnalyserState(juce::ValueTree state)
{
    if (! state.isValid())
    {
        resetAnalyserState();
        return;
    }

    setAnalyserParameterValue(paramTimeId, static_cast<float>(state.getProperty(paramTimeId, 50.0f)));
    setAnalyserParameterValue(paramSpectralReductionRangeId,
                              static_cast<float>(state.getProperty(paramSpectralReductionRangeId, -36.0f)));
    setAnalyserParameterValue(paramPhaseReductionRangeId,
                              static_cast<float>(state.getProperty(paramPhaseReductionRangeId, 50.0f)));
}
