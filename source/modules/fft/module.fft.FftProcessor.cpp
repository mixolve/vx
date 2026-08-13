#include "module.fft.FftProcessor.h"
#include "module.fft.ProcessorConstants.h"

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

constexpr auto phaseRangeLowProperty = "fft_phase_range_low";
constexpr auto phaseRangeHighProperty = "fft_phase_range_high";
}

FftModuleProcessor::FftModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
    parameters(internalParameterHost, nullptr, "fft_state", createParameterLayout())
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
    dspHopDivisorParam = parameters.getRawParameterValue(paramDspHopDivisorId);
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
    outputAnalyser.prepare(sampleRate);
    deltaDryBuffer.setSize(ownerProcessor.getTotalNumInputChannels(), preparedBlockSize);
}

void FftModuleProcessor::releaseResources()
{
    resetDeltaDelay();
}

void FftModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
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

    dynamicProcessor.processBuffer(buffer, channelsToUse, compressorSettings);

    if (deltaEnabled)
    {
        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            buffer.applyGain(channel, 0, buffer.getNumSamples(), -1.0f);
            buffer.addFrom(channel, 0, deltaDryBuffer, channel, 0, buffer.getNumSamples());
        }
    }

    if (compressorSettings.phaseMode)
    {
        std::array<float, analyserScopeSize> leftReduction {};
        std::array<float, analyserScopeSize> rightReduction {};
        dynamicProcessor.copyReductionScope(leftReduction, rightReduction);
        outputAnalyser.pushPhaseBuffer(buffer,
                                       channelsToUse,
                                       analysisSettings.fftSize,
                                       analysisSettings.overlapFactor,
                                       analysisSettings.averagingTimeMs,
                                       leftReduction,
                                       rightReduction);
    }
    else
    {
        outputAnalyser.pushBuffer(buffer,
                                  channelsToUse,
                                  analysisSettings.fftSize,
                                  analysisSettings.overlapFactor,
                                  analysisSettings.averagingTimeMs);
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

void FftModuleProcessor::copyAnalyserData(std::array<float, analyserScopeSize>& destination,
                                          double& currentSampleRate) const
{
    outputAnalyser.copyScope(destination, currentSampleRate);
}

void FftModuleProcessor::copyGainReductionData(std::array<float, analyserScopeSize>& leftDestination,
                                               std::array<float, analyserScopeSize>& rightDestination) const
{
    dynamicProcessor.copyReductionScope(leftDestination, rightDestination);
}

void FftModuleProcessor::copyPhaseAnalysisData(
    std::array<float, analyserScopeSize>& detectorDestination,
    std::array<float, analyserScopeSize>& leftReductionDestination,
    std::array<float, analyserScopeSize>& rightReductionDestination) const
{
    outputAnalyser.copyPhaseAnalysisScopes(detectorDestination,
                                           leftReductionDestination,
                                           rightReductionDestination);
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
    state.setProperty(paramFftSizeId, analyserFftSizeValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramOverlapId, analyserOverlapValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramLeftId, analyserLeftValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramRightId, analyserRightValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramRangeLowId, analyserRangeLowValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramRangeHighId, analyserRangeHighValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(phaseRangeLowProperty, analyserPhaseRangeLowValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(phaseRangeHighProperty, analyserPhaseRangeHighValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramSlopeId, analyserSlopeValue.load(std::memory_order_relaxed), nullptr);
    state.setProperty(paramTimeId, analyserTimeValue.load(std::memory_order_relaxed), nullptr);
    return state;
}

float FftModuleProcessor::getAnalyserParameterValue(const juce::String& parameterId) const noexcept
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
        return dynamicModeParam != nullptr && dynamicModeParam->load(std::memory_order_relaxed) >= 0.5f
            ? analyserPhaseRangeLowValue.load(std::memory_order_relaxed)
            : analyserRangeLowValue.load(std::memory_order_relaxed);

    if (parameterId == paramRangeHighId)
        return dynamicModeParam != nullptr && dynamicModeParam->load(std::memory_order_relaxed) >= 0.5f
            ? analyserPhaseRangeHighValue.load(std::memory_order_relaxed)
            : analyserRangeHighValue.load(std::memory_order_relaxed);

    if (parameterId == paramSlopeId)
        return analyserSlopeValue.load(std::memory_order_relaxed);

    if (parameterId == paramTimeId)
        return analyserTimeValue.load(std::memory_order_relaxed);

    return 0.0f;
}

void FftModuleProcessor::setAnalyserParameterValue(const juce::String& parameterId, float value)
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
        const auto phaseMode = dynamicModeParam != nullptr
            && dynamicModeParam->load(std::memory_order_relaxed) >= 0.5f;

        if (phaseMode)
        {
            auto low = analyserPhaseRangeLowValue.load(std::memory_order_relaxed);
            auto high = analyserPhaseRangeHighValue.load(std::memory_order_relaxed);

            if (parameterId == paramRangeLowId)
            {
                low = juce::jlimit(-1.0f, 1.0f, value);

                if (low > high)
                    high = low;
            }
            else
            {
                high = juce::jlimit(-1.0f, 1.0f, value);

                if (high < low)
                    low = high;
            }

            analyserPhaseRangeLowValue.store(low, std::memory_order_relaxed);
            analyserPhaseRangeHighValue.store(high, std::memory_order_relaxed);
            return;
        }

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

void FftModuleProcessor::resetAnalyserState()
{
    setAnalyserParameterValue(paramFftSizeId, 2.0f);
    setAnalyserParameterValue(paramOverlapId, 4.0f);
    setAnalyserParameterValue(paramLeftId, 21.0f);
    setAnalyserParameterValue(paramRightId, 20000.0f);
    analyserRangeLowValue.store(-60.0f, std::memory_order_relaxed);
    analyserRangeHighValue.store(10.0f, std::memory_order_relaxed);
    analyserPhaseRangeLowValue.store(-1.0f, std::memory_order_relaxed);
    analyserPhaseRangeHighValue.store(1.0f, std::memory_order_relaxed);
    setAnalyserParameterValue(paramSlopeId, 4.5f);
    setAnalyserParameterValue(paramTimeId, 50.0f);
}

void FftModuleProcessor::applyAnalyserState(juce::ValueTree state)
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
    analyserRangeLowValue.store(juce::jlimit(analyserMinDecibels,
                                             analyserMaxDecibels - 6.0f,
                                             static_cast<float>(state.getProperty(paramRangeLowId, -60.0f))),
                                std::memory_order_relaxed);
    analyserRangeHighValue.store(juce::jlimit(analyserMinDecibels + 6.0f,
                                              analyserMaxDecibels,
                                              static_cast<float>(state.getProperty(paramRangeHighId, 10.0f))),
                                 std::memory_order_relaxed);
    auto phaseLow = juce::jlimit(-1.0f,
                                 1.0f,
                                 static_cast<float>(state.getProperty(phaseRangeLowProperty, -1.0f)));
    auto phaseHigh = juce::jlimit(-1.0f,
                                  1.0f,
                                  static_cast<float>(state.getProperty(phaseRangeHighProperty, 1.0f)));

    if (phaseHigh < phaseLow)
        std::swap(phaseLow, phaseHigh);

    analyserPhaseRangeLowValue.store(phaseLow, std::memory_order_relaxed);
    analyserPhaseRangeHighValue.store(phaseHigh, std::memory_order_relaxed);
    setAnalyserParameterValue(paramSlopeId, static_cast<float>(state.getProperty(paramSlopeId, 4.5f)));
    setAnalyserParameterValue(paramTimeId, static_cast<float>(state.getProperty(paramTimeId, 50.0f)));
}
