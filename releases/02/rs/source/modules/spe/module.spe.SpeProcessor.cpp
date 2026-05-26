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
}

SpeModuleProcessor::SpeModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
      parameters(ownerProcessor, nullptr, "spe_state", createParameterLayout())
{
    resetAnalyserState();
    thresholdParam = parameters.getRawParameterValue(paramThresholdId);
    stereoAdaptiveParam = parameters.getRawParameterValue(paramStereoAdaptiveId);
    stereoAdaptiveOffsetParam = parameters.getRawParameterValue(paramStereoAdaptiveOffsetId);
    dualMonoLeftThresholdParam = parameters.getRawParameterValue(paramDualMonoLeftThresholdId);
    dualMonoRightThresholdParam = parameters.getRawParameterValue(paramDualMonoRightThresholdId);
    dualMonoLeftAdaptiveParam = parameters.getRawParameterValue(paramDualMonoLeftAdaptiveId);
    dualMonoRightAdaptiveParam = parameters.getRawParameterValue(paramDualMonoRightAdaptiveId);
    dualMonoLeftAdaptiveOffsetParam = parameters.getRawParameterValue(paramDualMonoLeftAdaptiveOffsetId);
    dualMonoRightAdaptiveOffsetParam = parameters.getRawParameterValue(paramDualMonoRightAdaptiveOffsetId);
    inputGainLrParam = parameters.getRawParameterValue(paramInputGainLrId);
    inputGainLParam = parameters.getRawParameterValue(paramInputGainLId);
    inputGainRParam = parameters.getRawParameterValue(paramInputGainRId);
    wideParam = parameters.getRawParameterValue(paramWideId);
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

    if (moduleBypass)
    {
        outputAnalyser.pushBuffer(buffer,
                                  channelsToUse,
                                  analysisSettings.fftSize,
                                  analysisSettings.overlapFactor,
                                  analysisSettings.averagingTimeMs);
        return;
    }

    const auto wideAmount = wideParam != nullptr
        ? juce::jlimit(0.0f, 4.0f, wideParam->load(std::memory_order_relaxed) / 100.0f)
        : 1.0f;

    if (channelsToUse > 1 && std::abs(wideAmount - 1.0f) > 1.0e-6f)
    {
        const auto numSamples = buffer.getNumSamples();
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            const auto mid = 0.5f * (left[sampleIndex] + right[sampleIndex]);
            const auto side = 0.5f * (left[sampleIndex] - right[sampleIndex]) * wideAmount;
            left[sampleIndex] = mid + side;
            right[sampleIndex] = mid - side;
        }
    }

    const auto gainLrDb = juce::jlimit(-24.0f, 24.0f, inputGainLrParam != nullptr ? inputGainLrParam->load(std::memory_order_relaxed) : 0.0f);
    const auto gainLDb = juce::jlimit(-24.0f, 24.0f, inputGainLParam != nullptr ? inputGainLParam->load(std::memory_order_relaxed) : 0.0f);
    const auto gainRDb = juce::jlimit(-24.0f, 24.0f, inputGainRParam != nullptr ? inputGainRParam->load(std::memory_order_relaxed) : 0.0f);
    const auto effectiveGainLDb = gainLDb + gainLrDb;
    const auto effectiveGainRDb = gainRDb + gainLrDb;
    const auto applyInputGain = std::abs(effectiveGainLDb) > 1.0e-6f || std::abs(effectiveGainRDb) > 1.0e-6f;

    if (applyInputGain)
    {
        if (channelsToUse > 0)
            buffer.applyGain(0, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveGainLDb));

        if (channelsToUse > 1)
            buffer.applyGain(1, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveGainRDb));

        if (deltaEnabled)
        {
            if (channelsToUse > 0)
                deltaDryBuffer.applyGain(0, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveGainLDb));

            if (channelsToUse > 1)
                deltaDryBuffer.applyGain(1, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveGainRDb));
        }
    }

    if (moduleBypassWithGain)
    {
        applyMakeupGain(buffer, channelsToUse);

        outputAnalyser.pushBuffer(buffer,
                                  channelsToUse,
                                  analysisSettings.fftSize,
                                  analysisSettings.overlapFactor,
                                  analysisSettings.averagingTimeMs);
        return;
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
    auto state = buildCombinedSpeState(parameters, analyserState);

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
    auto state = buildCombinedSpeState(const_cast<juce::AudioProcessorValueTreeState&>(parameters), analyserState);

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

juce::ValueTree SpeModuleProcessor::getAnalyserState() const noexcept
{
    return analyserState;
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
    auto setProperty = [this] (const char* id, const float propertyValue)
    {
        analyserState.setProperty(id, propertyValue, nullptr);
    };

    if (parameterId == paramFftSizeId)
    {
        const auto clamped = static_cast<float>(juce::jlimit(0, 4, juce::roundToInt(value)));
        analyserFftSizeValue.store(clamped, std::memory_order_relaxed);
        setProperty(paramFftSizeId, clamped);
        return;
    }

    if (parameterId == paramOverlapId)
    {
        const auto clamped = static_cast<float>(juce::jlimit(0, 4, juce::roundToInt(value)));
        analyserOverlapValue.store(clamped, std::memory_order_relaxed);
        setProperty(paramOverlapId, clamped);
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
        setProperty(paramLeftId, left);
        setProperty(paramRightId, right);
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
        setProperty(paramRangeLowId, low);
        setProperty(paramRangeHighId, high);
        return;
    }

    if (parameterId == paramSlopeId)
    {
        const auto clamped = juce::jlimit(0.0f, 6.0f, value);
        analyserSlopeValue.store(clamped, std::memory_order_relaxed);
        setProperty(paramSlopeId, clamped);
        return;
    }

    if (parameterId == paramTimeId)
    {
        const auto clamped = juce::jlimit(0.0f, 1000.0f, value);
        analyserTimeValue.store(clamped, std::memory_order_relaxed);
        setProperty(paramTimeId, clamped);
    }
}

void SpeModuleProcessor::resetAnalyserState()
{
    setAnalyserParameterValue(paramFftSizeId, 3.0f);
    setAnalyserParameterValue(paramOverlapId, 4.0f);
    setAnalyserParameterValue(paramLeftId, 21.0f);
    setAnalyserParameterValue(paramRightId, 20000.0f);
    setAnalyserParameterValue(paramRangeLowId, -60.0f);
    setAnalyserParameterValue(paramRangeHighId, 10.0f);
    setAnalyserParameterValue(paramSlopeId, 4.0f);
    setAnalyserParameterValue(paramTimeId, 50.0f);
}

void SpeModuleProcessor::applyAnalyserState(juce::ValueTree state)
{
    if (! state.isValid())
    {
        resetAnalyserState();
        return;
    }

    setAnalyserParameterValue(paramFftSizeId, static_cast<float>(state.getProperty(paramFftSizeId, 3.0f)));
    setAnalyserParameterValue(paramOverlapId, static_cast<float>(state.getProperty(paramOverlapId, 4.0f)));
    setAnalyserParameterValue(paramLeftId, static_cast<float>(state.getProperty(paramLeftId, 21.0f)));
    setAnalyserParameterValue(paramRightId, static_cast<float>(state.getProperty(paramRightId, 20000.0f)));
    setAnalyserParameterValue(paramRangeLowId, static_cast<float>(state.getProperty(paramRangeLowId, -60.0f)));
    setAnalyserParameterValue(paramRangeHighId, static_cast<float>(state.getProperty(paramRangeHighId, 10.0f)));
    setAnalyserParameterValue(paramSlopeId, static_cast<float>(state.getProperty(paramSlopeId, 4.0f)));
    setAnalyserParameterValue(paramTimeId, static_cast<float>(state.getProperty(paramTimeId, 50.0f)));
}
