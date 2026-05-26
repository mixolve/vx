#include "module.tse.TseProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace
{
juce::String formatDecibelValue(const float value)
{
    return juce::String::formatted("%.1f dB", static_cast<double>(value));
}

juce::String formatTimeValue(const float value)
{
    return juce::String::formatted("%.0f ms", static_cast<double>(value));
}

juce::String formatLookaheadValue(const float value)
{
    return juce::String::formatted("%.2f ms", static_cast<double>(value));
}

juce::String formatCurveValue(const float value)
{
    return juce::String::formatted("%.0f", static_cast<double>(value));
}

bool isEnabled(const std::atomic<float>* parameter) noexcept
{
    return parameter != nullptr && parameter->load(std::memory_order_relaxed) >= 0.5f;
}

float calculateThresholdAmount(const float levelDb, const float thresholdDb, const float kneeDb) noexcept
{
    if (kneeDb <= 0.0f)
        return levelDb >= thresholdDb ? 1.0f : 0.0f;

    const auto kneeStartDb = thresholdDb - kneeDb;

    if (levelDb <= kneeStartDb)
        return 0.0f;

    if (levelDb >= thresholdDb)
        return 1.0f;

    const auto normalized = juce::jlimit(0.0f, 1.0f, (levelDb - kneeStartDb) / kneeDb);
    return normalized * normalized * (3.0f - (2.0f * normalized));
}

constexpr auto gainMinDb = -48.0f;
constexpr auto gainMaxDb = 48.0f;
constexpr std::array<int, 9> hostSyncDenominators { 1, 2, 4, 8, 16, 32, 64, 128, 256 };

float getParameterValue(const std::atomic<float>* parameter, const float fallback) noexcept
{
    return parameter != nullptr ? parameter->load(std::memory_order_relaxed) : fallback;
}

float getHostSyncMilliseconds(const int choiceIndex, const double bpm) noexcept
{
    const auto safeBpm = bpm > 0.0 ? bpm : 120.0;
    const auto safeIndex = juce::jlimit(0, static_cast<int>(hostSyncDenominators.size()) - 1, choiceIndex);
    const auto denominator = hostSyncDenominators[static_cast<size_t>(safeIndex)];
    return static_cast<float>((60000.0 / safeBpm) * (4.0 / static_cast<double>(denominator)));
}
}

TseModuleProcessor::TseModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
    parameters(ownerProcessor, nullptr, "tse_state", createParameterLayout())
{
    transOnParam = parameters.getRawParameterValue(paramTransOnId);
    transGainParam = parameters.getRawParameterValue(paramTransGainId);
    sustainOnParam = parameters.getRawParameterValue(paramSusOnId);
    sustainGainParam = parameters.getRawParameterValue(paramSusGainId);
    bypassParam = parameters.getRawParameterValue(paramBypassId);
    bypassWithGainParam = parameters.getRawParameterValue(paramBypassWithGainId);
    holdParam = parameters.getRawParameterValue(paramTimeHoldId);
    holdModeParam = parameters.getRawParameterValue(paramTimeHoldModeId);
    holdSyncParam = parameters.getRawParameterValue(paramTimeHoldSyncId);
    releaseParam = parameters.getRawParameterValue(paramTimeReleaseId);
    releaseCurveParam = parameters.getRawParameterValue(paramTimeReleaseCurveId);
    releaseModeParam = parameters.getRawParameterValue(paramTimeReleaseModeId);
    releaseSyncParam = parameters.getRawParameterValue(paramTimeReleaseSyncId);
    thresholdParam = parameters.getRawParameterValue(paramSensLevelId);
    kneeParam = parameters.getRawParameterValue(paramSensKneeId);
    retriggerParam = parameters.getRawParameterValue(paramSensRetriggerId);
    inGainLrParam = parameters.getRawParameterValue(paramMiscInGainLrId);
    inGainLParam = parameters.getRawParameterValue(paramMiscInGainLId);
    inGainRParam = parameters.getRawParameterValue(paramMiscInGainRId);
    wideParam = parameters.getRawParameterValue(paramMiscWideId);
    lookaheadParam = parameters.getRawParameterValue(paramMiscLookaheadId);
    outGainParam = parameters.getRawParameterValue(paramMiscOutGainId);
}

TseModuleProcessor::~TseModuleProcessor() = default;

void TseModuleProcessor::prepareToPlay(const double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    preparedBlockSize = juce::jmax(1, ownerProcessor.getBlockSize());
    maximumLookaheadSamples = juce::jmax(0, static_cast<int>(std::ceil(currentSampleRate * 0.02)));
    delayBufferLength = juce::jmax(1, maximumLookaheadSamples + 1);
    preparedChannels = juce::jmax(1, ownerProcessor.getTotalNumInputChannels());
    delayBuffers.resize(static_cast<size_t>(preparedChannels));

    for (auto& channelBuffer : delayBuffers)
        channelBuffer.assign(static_cast<size_t>(delayBufferLength), 0.0f);

    refreshLatencyState();
    resetDetector();
    delayWriteIndex = 0;
}

void TseModuleProcessor::releaseResources()
{
    resetDelayBuffers();
    resetDetector();
}

void TseModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const auto settings = getSettings(getHostBpm());
    const auto channelsToUse = juce::jmin(ownerProcessor.getTotalNumInputChannels(), buffer.getNumChannels());

    for (auto channel = ownerProcessor.getTotalNumInputChannels(); channel < ownerProcessor.getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (channelsToUse <= 0 || buffer.getNumSamples() <= 0)
        return;

    const auto lookaheadSamples = (settings.bypassed || settings.bypassedWithGain)
        ? 0
        : juce::jlimit(0,
                       maximumLookaheadSamples,
                       static_cast<int>(std::round(settings.lookaheadMs * 0.001 * currentSampleRate)));
    updateDelayConfiguration(channelsToUse, lookaheadSamples);

    if (settings.bypassed)
        return;

    const auto wideAmount = juce::jlimit(0.0f,
                                         4.0f,
                                         getParameterValue(wideParam, 100.0f) / 100.0f);

    if (channelsToUse > 1 && std::abs(wideAmount - 1.0f) > 1.0e-6f)
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);

        for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            const auto mid = 0.5f * (left[sampleIndex] + right[sampleIndex]);
            const auto side = 0.5f * (left[sampleIndex] - right[sampleIndex]) * wideAmount;
            left[sampleIndex] = mid + side;
            right[sampleIndex] = mid - side;
        }
    }

    const auto inGainLrDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(inGainLrParam, 0.0f));
    const auto inGainLDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(inGainLParam, 0.0f));
    const auto inGainRDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(inGainRParam, 0.0f));
    const auto effectiveInGainLDb = inGainLDb + inGainLrDb;
    const auto effectiveInGainRDb = inGainRDb + inGainLrDb;

    if (std::abs(effectiveInGainLDb) > 1.0e-6f)
        buffer.applyGain(0, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveInGainLDb));

    if (channelsToUse > 1 && std::abs(effectiveInGainRDb) > 1.0e-6f)
        buffer.applyGain(1, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveInGainRDb));

    if (settings.bypassedWithGain)
    {
        const auto outputGain = juce::Decibels::decibelsToGain(settings.outGainDb);

        if (std::abs(outputGain - 1.0f) > 1.0e-6f)
            buffer.applyGain(outputGain);

        return;
    }

    for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        auto detectorLevel = 0.0f;

        for (auto channel = 0; channel < channelsToUse; ++channel)
            detectorLevel = juce::jmax(detectorLevel, std::abs(buffer.getSample(channel, sampleIndex)));

        const auto transientAmount = processDetectorSample(detectorLevel, settings);
        const auto outputGain = juce::Decibels::decibelsToGain(settings.outGainDb);
        auto readIndex = delayWriteIndex - activeLatencySamples;

        if (readIndex < 0)
            readIndex += delayBufferLength;

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            auto& channelBuffer = delayBuffers[static_cast<size_t>(channel)];
            const auto inputSample = buffer.getSample(channel, sampleIndex);
            channelBuffer[static_cast<size_t>(delayWriteIndex)] = inputSample;
            const auto delayedSample = channelBuffer[static_cast<size_t>(readIndex)];
            const auto transientSample = delayedSample * transientAmount * settings.transientGain * outputGain;
            const auto sustainSample = delayedSample * (1.0f - transientAmount) * settings.sustainGain * outputGain;
            buffer.setSample(channel, sampleIndex, transientSample + sustainSample);
        }

        delayWriteIndex = (delayWriteIndex + 1) % delayBufferLength;
    }
}

juce::String TseModuleProcessor::getStateXmlString() const
{
    auto state = const_cast<juce::AudioProcessorValueTreeState&>(parameters).copyState();

    if (auto stateXml = state.createXml())
        return stateXml->toString();

    return {};
}

void TseModuleProcessor::setStateFromXmlString(const juce::String& stateXmlString)
{
    if (stateXmlString.trim().isEmpty())
        return;

    auto stateXml = juce::parseXML(stateXmlString);

    if (stateXml == nullptr || ! stateXml->hasTagName(parameters.state.getType()))
        return;

    parameters.replaceState(juce::ValueTree::fromXml(*stateXml));
    refreshLatencyState();
}

int TseModuleProcessor::getLatencySamples() const noexcept
{
    return activeLatencySamples;
}

bool TseModuleProcessor::canProvideSplit() const noexcept
{
    return ! isEnabled(bypassParam) && ! isEnabled(bypassWithGainParam);
}

juce::AudioProcessorValueTreeState& TseModuleProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& TseModuleProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

juce::StringArray TseModuleProcessor::getHostSyncChoices()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128", "1/256" };
}

int TseModuleProcessor::getDefaultHostSyncChoiceIndex() noexcept
{
    return 4;
}

TransientSplitSettings TseModuleProcessor::getSplitSettings() const noexcept
{
    const auto hostBpm = getHostBpm();
    const auto holdHostSync = getParameterValue(holdModeParam, 0.0f) >= 0.5f;
    const auto releaseHostSync = getParameterValue(releaseModeParam, 0.0f) >= 0.5f;
    const auto holdSyncIndex = static_cast<int>(std::round(getParameterValue(holdSyncParam,
                                                                              static_cast<float>(getDefaultHostSyncChoiceIndex()))));
    const auto releaseSyncIndex = static_cast<int>(std::round(getParameterValue(releaseSyncParam,
                                                                                 static_cast<float>(getDefaultHostSyncChoiceIndex()))));

    return {
        holdHostSync ? getHostSyncMilliseconds(holdSyncIndex, hostBpm)
                     : juce::jlimit(0.0f, 200.0f, getParameterValue(holdParam, 0.0f)),
        releaseHostSync ? getHostSyncMilliseconds(releaseSyncIndex, hostBpm)
                        : juce::jlimit(1.0f, 500.0f, getParameterValue(releaseParam, 10.0f)),
        juce::jlimit(-100.0f, 100.0f, getParameterValue(releaseCurveParam, 0.0f)),
        juce::jlimit(-80.0f, 0.0f, getParameterValue(thresholdParam, -42.0f)),
        juce::jlimit(0.0f, 24.0f, getParameterValue(kneeParam, 0.0f)),
        juce::jlimit(0.0f, 250.0f, getParameterValue(retriggerParam, 100.0f)),
        juce::jlimit(0.0f, 20.0f, getParameterValue(lookaheadParam, 1.0f))
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout TseModuleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    const juce::StringArray timeModeChoices { "M", "T" };
    const auto makeTseName = [] (const juce::String& tabName, const juce::String& parameterName)
    {
        return "TSE - " + tabName + " - " + parameterName;
    };

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramBypassId, 1 },
        makeTseName("MISC", "BYPASS"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramBypassWithGainId, 1 },
        makeTseName("MISC", "BYPASS.WT-GAIN"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMiscInGainLrId, 1 },
        makeTseName("MISC", "IN-GAIN-LR"),
        juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMiscInGainLId, 1 },
        makeTseName("MISC", "IN-GAIN-L"),
        juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMiscInGainRId, 1 },
        makeTseName("MISC", "IN-GAIN-R"),
        juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMiscWideId, 1 },
        makeTseName("MISC", "WIDE"),
        juce::NormalisableRange<float> { 0.0f, 400.0f, 0.1f },
        100.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return juce::String::formatted("%.0f", static_cast<double>(value));
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMiscOutGainId, 1 },
        makeTseName("MISC", "OUT-GAIN"),
        juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramTransOnId, 1 },
        makeTseName("STEREO", "TRANS.ON/OFF"),
        true,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramSusOnId, 1 },
        makeTseName("STEREO", "SUS.ON/OFF"),
        true,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramTransGainId, 1 },
        makeTseName("STEREO", "TRANS.GAIN"),
        juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramSusGainId, 1 },
        makeTseName("STEREO", "SUS.GAIN"),
        juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramTimeHoldId, 1 },
        makeTseName("STEREO", "HOLD"),
        juce::NormalisableRange<float> { 0.0f, 200.0f, 1.0f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatTimeValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramTimeHoldModeId, 1 },
        makeTseName("STEREO", "HOLD MODE"),
        timeModeChoices,
        0,
        juce::AudioParameterChoiceAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramTimeHoldSyncId, 1 },
        makeTseName("STEREO", "HOLD SYNC"),
        getHostSyncChoices(),
        getDefaultHostSyncChoiceIndex(),
        juce::AudioParameterChoiceAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramTimeReleaseId, 1 },
        makeTseName("STEREO", "RELEASE"),
        juce::NormalisableRange<float> { 1.0f, 500.0f, 1.0f },
        10.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatTimeValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramTimeReleaseModeId, 1 },
        makeTseName("STEREO", "RELEASE MODE"),
        timeModeChoices,
        0,
        juce::AudioParameterChoiceAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramTimeReleaseSyncId, 1 },
        makeTseName("STEREO", "RELEASE SYNC"),
        getHostSyncChoices(),
        getDefaultHostSyncChoiceIndex(),
        juce::AudioParameterChoiceAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramTimeReleaseCurveId, 1 },
        makeTseName("STEREO", "REL-CURVE"),
        juce::NormalisableRange<float> { -100.0f, 100.0f, 1.0f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatCurveValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramSensLevelId, 1 },
        makeTseName("STEREO", "SENS.LVL"),
        juce::NormalisableRange<float> { -80.0f, 0.0f, 0.1f },
        -42.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramSensKneeId, 1 },
        makeTseName("STEREO", "SENS.KNEE"),
        juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramSensRetriggerId, 1 },
        makeTseName("STEREO", "SENS.RETR"),
        juce::NormalisableRange<float> { 0.0f, 250.0f, 1.0f },
        100.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatTimeValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMiscLookaheadId, 1 },
        makeTseName("STEREO", "LOOKAHEAD"),
        juce::NormalisableRange<float> { 0.0f, 20.0f, 0.01f },
        1.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatLookaheadValue(value);
            })));

    return { parameterLayout.begin(), parameterLayout.end() };
}

TseModuleProcessor::Settings TseModuleProcessor::getSettings(const double hostBpm) const noexcept
{
    juce::ignoreUnused(hostBpm);
    const auto splitSettings = getSplitSettings();
    const auto transEnabled = isEnabled(transOnParam);
    const auto sustainEnabled = isEnabled(sustainOnParam);
    const auto transGainDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(transGainParam, 0.0f));
    const auto sustainGainDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(sustainGainParam, 0.0f));
    const auto holdMs = splitSettings.holdMs;
    const auto releaseMs = splitSettings.releaseMs;

    return {
        isEnabled(bypassParam),
        isEnabled(bypassWithGainParam),
        transEnabled,
        transGainDb,
        sustainEnabled,
        sustainGainDb,
        holdMs,
        releaseMs,
        splitSettings.releaseCurve,
        splitSettings.thresholdDb,
        splitSettings.kneeDb,
        splitSettings.retriggerMs,
        splitSettings.lookaheadMs,
        juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(outGainParam, 0.0f)),
        makeReleaseCoefficient(5.0f, currentSampleRate),
        makeReleaseCoefficient(25.0f, currentSampleRate),
        makeReleaseCoefficient(juce::jmax(50.0f, holdMs + releaseMs), currentSampleRate),
        juce::jlimit(-1.0f, 1.0f, splitSettings.releaseCurve * 0.01f),
        juce::jmax(0, static_cast<int>(std::round(holdMs * 0.001 * currentSampleRate))),
        juce::jmax(0, static_cast<int>(std::round(splitSettings.retriggerMs * 0.001 * currentSampleRate))),
        juce::jmax(1, static_cast<int>(std::round(juce::jmax(1.0f, releaseMs) * 0.001 * currentSampleRate))),
        transEnabled ? juce::Decibels::decibelsToGain(transGainDb) : 0.0f,
        sustainEnabled ? juce::Decibels::decibelsToGain(sustainGainDb) : 0.0f
    };
}

bool TseModuleProcessor::refreshLatencyState() noexcept
{
    const auto newLatencySamples = (isEnabled(bypassParam) || isEnabled(bypassWithGainParam))
        ? 0
        : juce::jlimit(0,
                       maximumLookaheadSamples,
                       static_cast<int>(std::round(getSplitSettings().lookaheadMs * 0.001 * currentSampleRate)));
    const auto changed = activeLatencySamples != newLatencySamples;
    activeLatencySamples = newLatencySamples;
    return changed;
}

double TseModuleProcessor::getHostBpm() const noexcept
{
    if (auto* playHead = ownerProcessor.getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
            {
                if (*bpm > 0.0)
                    return *bpm;
            }
        }
    }

    return 120.0;
}

void TseModuleProcessor::updateDelayConfiguration(const int channelsToUse, const int lookaheadSamples)
{
    if (channelsToUse != preparedChannels || static_cast<int>(delayBuffers.size()) < channelsToUse)
    {
        preparedChannels = juce::jmax(1, channelsToUse);
        delayBuffers.resize(static_cast<size_t>(preparedChannels));

        for (auto& channelBuffer : delayBuffers)
            channelBuffer.assign(static_cast<size_t>(delayBufferLength), 0.0f);

        delayWriteIndex = 0;
    }

    activeLatencySamples = lookaheadSamples;
}

void TseModuleProcessor::resetDelayBuffers() noexcept
{
    delayWriteIndex = 0;

    for (auto& channelBuffer : delayBuffers)
        std::fill(channelBuffer.begin(), channelBuffer.end(), 0.0f);
}

void TseModuleProcessor::resetDetector() noexcept
{
    detector = {};
    detector.samplesSinceTrigger = std::numeric_limits<int>::max() / 2;
}

float TseModuleProcessor::processDetectorSample(const float level, const Settings& settings) noexcept
{
    detector.fastEnvelope = level >= detector.fastEnvelope
        ? level
        : level + ((detector.fastEnvelope - level) * settings.fastReleaseCoefficient);

    const auto bodyCoefficient = level >= detector.bodyEnvelope ? settings.bodyAttackCoefficient
                                                                : settings.bodyReleaseCoefficient;
    detector.bodyEnvelope = level + ((detector.bodyEnvelope - level) * bodyCoefficient);

    const auto levelDb = juce::Decibels::gainToDecibels(detector.fastEnvelope, -120.0f);
    const auto bodyDb = juce::Decibels::gainToDecibels(detector.bodyEnvelope, -120.0f);
    const auto onsetDb = levelDb - bodyDb;
    const auto thresholdAmount = calculateThresholdAmount(levelDb, settings.thresholdDb, settings.kneeDb);
    const auto aboveThreshold = thresholdAmount > 1.0e-4f;
    const auto thresholdRisingEdge = aboveThreshold && ! detector.wasAboveThreshold;
    const auto retriggerElapsed = detector.samplesSinceTrigger >= settings.holdSamples + settings.retriggerSamples;
    const auto shouldTrigger = aboveThreshold
        && retriggerElapsed
        && (thresholdRisingEdge || onsetDb >= 6.0f || settings.retriggerSamples > 0);

    if (shouldTrigger)
    {
        detector.heldTransientAmount = thresholdAmount;
        detector.transientEnvelope = juce::jmax(detector.transientEnvelope, detector.heldTransientAmount);
        detector.releaseStartAmount = detector.transientEnvelope;
        detector.holdSamplesRemaining = settings.holdSamples;
        detector.releaseSamplesRemaining = 0;
        detector.releaseSamplesTotal = 0;
        detector.samplesSinceTrigger = 0;
    }
    else
    {
        detector.samplesSinceTrigger = detector.samplesSinceTrigger < (std::numeric_limits<int>::max() / 4)
            ? detector.samplesSinceTrigger + 1
            : detector.samplesSinceTrigger;

        if (detector.holdSamplesRemaining > 0)
        {
            --detector.holdSamplesRemaining;
            detector.heldTransientAmount = juce::jmax(detector.heldTransientAmount, thresholdAmount);
            detector.transientEnvelope = detector.heldTransientAmount;
            detector.releaseStartAmount = detector.transientEnvelope;
            detector.releaseSamplesRemaining = 0;
            detector.releaseSamplesTotal = 0;
        }
        else
        {
            if (detector.releaseSamplesTotal <= 0)
            {
                detector.releaseStartAmount = detector.transientEnvelope;
                detector.releaseSamplesTotal = settings.releaseSamples;
                detector.releaseSamplesRemaining = detector.releaseSamplesTotal;
            }

            if (detector.releaseSamplesRemaining > 0)
            {
                const auto completedSamples = detector.releaseSamplesTotal - detector.releaseSamplesRemaining + 1;
                auto progress = juce::jlimit(0.0f,
                                             1.0f,
                                             static_cast<float>(completedSamples) / static_cast<float>(detector.releaseSamplesTotal));
                const auto shapedProgress = settings.normalizedReleaseCurve >= 0.0f
                    ? std::pow(progress, 1.0f + (settings.normalizedReleaseCurve * 3.0f))
                    : 1.0f - std::pow(1.0f - progress, 1.0f + (-settings.normalizedReleaseCurve * 3.0f));

                detector.transientEnvelope = detector.releaseStartAmount * (1.0f - shapedProgress);
                --detector.releaseSamplesRemaining;
            }
            else
            {
                detector.transientEnvelope = 0.0f;
            }
        }
    }

    if (detector.transientEnvelope < 1.0e-4f)
    {
        detector.transientEnvelope = 0.0f;
        detector.heldTransientAmount = 0.0f;
    }

    detector.wasAboveThreshold = aboveThreshold;

    return juce::jlimit(0.0f, 1.0f, detector.transientEnvelope);
}

float TseModuleProcessor::makeReleaseCoefficient(const float timeMs, const double sampleRate) noexcept
{
    const auto timeSeconds = juce::jmax(0.001f, timeMs * 0.001f);
    const auto samples = juce::jmax(1.0, static_cast<double>(timeSeconds) * sampleRate);
    return static_cast<float>(std::exp(-1.0 / samples));
}
