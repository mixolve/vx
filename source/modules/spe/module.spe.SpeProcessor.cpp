#include "module.spe.SpeProcessor.h"

#include <cmath>

SpeModuleProcessor::SpeModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
      parameters(ownerProcessor, nullptr, "spe_state", createParameterLayout())
{
    fftSizeParam = parameters.getRawParameterValue(paramFftSizeId);
    overlapParam = parameters.getRawParameterValue(paramOverlapId);
    leftParam = parameters.getRawParameterValue(paramLeftId);
    rightParam = parameters.getRawParameterValue(paramRightId);
    rangeLowParam = parameters.getRawParameterValue(paramRangeLowId);
    rangeHighParam = parameters.getRawParameterValue(paramRangeHighId);
    slopeParam = parameters.getRawParameterValue(paramSlopeId);
    timeParam = parameters.getRawParameterValue(paramTimeId);
    thresholdParam = parameters.getRawParameterValue(paramThresholdId);
    stereoAdaptiveParam = parameters.getRawParameterValue(paramStereoAdaptiveId);
    stereoAdaptiveOffsetParam = parameters.getRawParameterValue(paramStereoAdaptiveOffsetId);
    dualMonoLeftThresholdParam = parameters.getRawParameterValue(paramDualMonoLeftThresholdId);
    dualMonoRightThresholdParam = parameters.getRawParameterValue(paramDualMonoRightThresholdId);
    dualMonoLeftAdaptiveParam = parameters.getRawParameterValue(paramDualMonoLeftAdaptiveId);
    dualMonoRightAdaptiveParam = parameters.getRawParameterValue(paramDualMonoRightAdaptiveId);
    dualMonoLeftAdaptiveOffsetParam = parameters.getRawParameterValue(paramDualMonoLeftAdaptiveOffsetId);
    dualMonoRightAdaptiveOffsetParam = parameters.getRawParameterValue(paramDualMonoRightAdaptiveOffsetId);
    inputGainParam = parameters.getRawParameterValue(paramInputGainId);
    attackParam = parameters.getRawParameterValue(paramAttackId);
    releaseParam = parameters.getRawParameterValue(paramReleaseId);
    kneeParam = parameters.getRawParameterValue(paramKneeId);
    ratioParam = parameters.getRawParameterValue(paramRatioId);
    makeupParam = parameters.getRawParameterValue(paramMakeupId);
    bypassParam = parameters.getRawParameterValue(paramBypassId);
    miscBypassParam = parameters.getRawParameterValue(paramMiscBypassId);
    miscBypassWithGainParam = parameters.getRawParameterValue(paramMiscBypassWithGainId);
    dualMonoBypassParam = parameters.getRawParameterValue(paramDualMonoBypassId);
    deltaParam = parameters.getRawParameterValue(paramDeltaId);
    dspFftSizeParam = parameters.getRawParameterValue(paramDspFftSizeId);
    dspSlopeParam = parameters.getRawParameterValue(paramDspSlopeId);
}

SpeModuleProcessor::~SpeModuleProcessor() = default;

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
    const auto moduleBypass = isModuleBypassEnabled();
    const auto moduleBypassWithGain = isModuleBypassWithGainEnabled();

    const auto desiredLatencySamples = (moduleBypass || moduleBypassWithGain) ? 0
                                                                              : compressorSettings.fftSize;

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

    if (moduleBypass || moduleBypassWithGain)
    {
        if (! moduleBypass && moduleBypassWithGain)
            applyMakeupGain(buffer, channelsToUse);

        outputAnalyser.pushBuffer(buffer,
                                  channelsToUse,
                                  analysisSettings.fftSize,
                                  analysisSettings.overlapFactor,
                                  analysisSettings.averagingTimeMs);
        return;
    }

    const auto inputGainDb = juce::jlimit(-24.0f, 24.0f, inputGainParam != nullptr ? inputGainParam->load(std::memory_order_relaxed) : 0.0f);
    const auto inputGainLinear = juce::Decibels::decibelsToGain(inputGainDb);

    if (std::abs(inputGainDb) > 1.0e-6f)
    {
        buffer.applyGain(inputGainLinear);

        if (deltaEnabled)
            deltaDryBuffer.applyGain(inputGainLinear);
    }

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

void SpeModuleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = parameters.copyState().createXml())
        juce::AudioProcessor::copyXmlToBinary(*stateXml, destData);
}

void SpeModuleProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto stateXml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes))
        if (stateXml->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(juce::ValueTree::fromXml(*stateXml));
            refreshLatencyState();
        }
}

juce::String SpeModuleProcessor::getStateXmlString() const
{
    auto state = const_cast<juce::AudioProcessorValueTreeState&>(parameters).copyState();

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

    parameters.replaceState(juce::ValueTree::fromXml(*stateXml));
    refreshLatencyState();
}

int SpeModuleProcessor::getLatencySamples() const noexcept
{
    return activeLatencySamples;
}

bool SpeModuleProcessor::refreshLatencyState() noexcept
{
    const auto newLatencySamples = (isModuleBypassEnabled() || isModuleBypassWithGainEnabled()) ? 0
                                                                                                 : getSelectedDspFftSize();
    const auto changed = activeLatencySamples != newLatencySamples;
    activeLatencySamples = newLatencySamples;
    return changed;
}

void SpeModuleProcessor::copyAnalyserData(std::array<float, analyserScopeSize>& destination,
                                          double& currentSampleRate) const
{
    outputAnalyser.copyScope(destination, currentSampleRate);
}

void SpeModuleProcessor::copyGainReductionData(std::array<float, analyserScopeSize>& destination) const
{
    spectralCompressor.copyReductionScope(destination);
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

    const auto delaySamples = juce::jlimit(0, deltaDelayBufferSize - 1, juce::jmax(0, latencySamples - 1));

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
