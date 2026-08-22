#include "Processor.h"
#include "../DspUtilities.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* label;
};

inline constexpr auto trsCrossoverOrder = std::to_array<ParameterOrderEntry>({
    { TrsModuleProcessor::paramTransOnId, "TRANSIENT / ENABLE" },
    { TrsModuleProcessor::paramSusOnId, "SUSTAIN / ENABLE" },
    { TrsModuleProcessor::paramTransGainId, "TRANSIENT / GAIN" },
    { TrsModuleProcessor::paramSusGainId, "SUSTAIN / GAIN" },
    { TrsModuleProcessor::paramTimeHoldId, "HOLD" },
    { TrsModuleProcessor::paramTimeHoldModeId, "HOLD MODE" },
    { TrsModuleProcessor::paramTimeHoldSyncId, "HOLD SYNC" },
    { TrsModuleProcessor::paramTimeReleaseId, "RELEASE" },
    { TrsModuleProcessor::paramTimeReleaseModeId, "RELEASE MODE" },
    { TrsModuleProcessor::paramTimeReleaseSyncId, "RELEASE SYNC" },
    { TrsModuleProcessor::paramTimeReleaseCurveId, "REL-CURVE" },
    { TrsModuleProcessor::paramSensLevelId, "SENS.LVL" },
    { TrsModuleProcessor::paramSensKneeId, "SENS.KNEE" },
    { TrsModuleProcessor::paramSensRetriggerId, "SENS.RETR" },
    { TrsModuleProcessor::paramLookaheadId, "LOOKAHEAD" },
});

constexpr auto gainMinDb = -48.0f;
constexpr auto gainMaxDb = 48.0f;
constexpr std::array<int, 9> hostSyncDenominators { 1, 2, 4, 8, 16, 32, 64, 128, 256 };

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

float getParameterValue(const std::atomic<float>* parameter, const float fallback) noexcept
{
    return parameter != nullptr ? parameter->load(std::memory_order_relaxed) : fallback;
}

bool isEnabled(const std::atomic<float>* parameter) noexcept
{
    return getParameterValue(parameter, 0.0f) >= 0.5f;
}

float getHostSyncMilliseconds(const int choiceIndex, const double bpm) noexcept
{
    const auto safeBpm = bpm > 0.0 ? bpm : 120.0;
    const auto safeIndex = juce::jlimit(0, static_cast<int>(hostSyncDenominators.size()) - 1, choiceIndex);
    const auto denominator = hostSyncDenominators[static_cast<size_t>(safeIndex)];
    return static_cast<float>((60000.0 / safeBpm) * (4.0 / static_cast<double>(denominator)));
}
} // namespace

TrsModuleProcessor::TrsModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
      parameters(parameterHost, nullptr, "trs_state", createParameterLayout())
{
    cacheParameterPointers();
}

TrsModuleProcessor::~TrsModuleProcessor() = default;

void TrsModuleProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    preparedBlockSize = juce::jmax(1, samplesPerBlock);
    processorBank.prepare(currentSampleRate, preparedBlockSize, ownerProcessor.getTotalNumOutputChannels());
    syncParameters();
    processorBank.reset();
}

void TrsModuleProcessor::releaseResources()
{
    processorBank.releaseResources();
}

void TrsModuleProcessor::resetProcessingState()
{
    processorBank.reset();
}

void TrsModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    ava::modules::dsp::clearOutputOnlyChannels(ownerProcessor, buffer);

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    syncParameters();
    processorBank.processRange(0, buffer);
}

juce::String TrsModuleProcessor::getStateXmlString() const
{
    auto state = const_cast<juce::AudioProcessorValueTreeState&>(parameters).copyState();

    if (auto stateXml = state.createXml())
        return stateXml->toString();

    return {};
}

void TrsModuleProcessor::setStateFromXmlString(const juce::String& stateXmlString)
{
    if (stateXmlString.trim().isEmpty())
        return;

    auto stateXml = juce::parseXML(stateXmlString);

    if (stateXml == nullptr || ! stateXml->hasTagName(parameters.state.getType()))
        return;

    parameters.replaceState(juce::ValueTree::fromXml(*stateXml));
    cacheParameterPointers();
    syncParameters();
}

int TrsModuleProcessor::getLatencySamples() const noexcept
{
    return moduleLatencySamples;
}

TrsModuleProcessor::RangeLatencies TrsModuleProcessor::getRangeLatencies() const noexcept
{
    return processorBank.getRangeLatencies();
}

size_t TrsModuleProcessor::ensureRangeCount(const size_t rangeCount)
{
    const auto createdRangeCount = processorBank.ensureRangeCount(rangeCount);
    processorBank.setRangeParameters(currentRangeParameters);
    return createdRangeCount;
}

size_t TrsModuleProcessor::getCreatedRangeCount() const noexcept
{
    return processorBank.getCreatedRangeCount();
}

void TrsModuleProcessor::processRange(const size_t rangeIndex, juce::AudioBuffer<float>& buffer)
{
    processorBank.processRange(rangeIndex, buffer);
}

bool TrsModuleProcessor::refreshLatencyState() noexcept
{
    const auto previousLatency = moduleLatencySamples;
    syncParameters();
    return previousLatency != moduleLatencySamples;
}

juce::AudioProcessorValueTreeState& TrsModuleProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& TrsModuleProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

juce::StringArray TrsModuleProcessor::getHostSyncChoices()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128", "1/256" };
}

int TrsModuleProcessor::getDefaultHostSyncChoiceIndex() noexcept
{
    return 4;
}

juce::String TrsModuleProcessor::makeCrossoverRangeParameterId(const size_t rangeIndex, const char* suffix)
{
    return "crossover" + juce::String(static_cast<int>(rangeIndex + 1)) + "_" + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout TrsModuleProcessor::createParameterLayout()
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using Parameter = std::unique_ptr<juce::RangedAudioParameter>;

    const juce::StringArray timeModeChoices { "M", "T" };

    auto boolParam = [] (const juce::String& id,
                         const juce::String& name,
                         const bool defaultValue,
                         const bool isMeta = false) -> Parameter
    {
        return std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { id, 1 },
            name,
            defaultValue,
            juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(isMeta));
    };

    auto floatParam = [] (const juce::String& id,
                          const juce::String& name,
                          const juce::NormalisableRange<float>& range,
                          const float defaultValue,
                          std::function<juce::String(float, int)> formatter,
                          const bool isMeta = false) -> Parameter
    {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 },
            name,
            range,
            defaultValue,
            juce::AudioParameterFloatAttributes()
                .withAutomatable(false)
                .withMeta(isMeta)
                .withStringFromValueFunction(std::move(formatter)));
    };

    auto choiceParam = [] (const juce::String& id,
                           const juce::String& name,
                           const juce::StringArray& choices,
                           const int defaultIndex,
                           const bool isMeta = false) -> Parameter
    {
        return std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { id, 1 },
            name,
            choices,
            defaultIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable(false).withMeta(isMeta));
    };

    Layout layout;

    for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
    {
        auto crossoverGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "crossover" + juce::String(static_cast<int>(rangeIndex + 1)),
            "Crossover " + juce::String(static_cast<int>(rangeIndex + 1)),
            " | ");

        for (const auto& entry : trsCrossoverOrder)
        {
            const auto key = juce::String(entry.key);
            const auto id = makeCrossoverRangeParameterId(rangeIndex, entry.key);
            const auto name = "TRS / CROSSOVER " + juce::String(static_cast<int>(rangeIndex + 1))
                + " / TRANSIENT PROCESSOR / " + juce::String(entry.label);

            if (key == paramTransOnId || key == paramSusOnId)
            {
                crossoverGroup->addChild(boolParam(id, name, true));
                continue;
            }

            if (key == paramTransGainId || key == paramSusGainId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.01f },
                                               0.0f,
                                               [] (float value, int) { return formatDecibelValue(value); }));
                continue;
            }

            if (key == paramTimeHoldId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 200.0f, 1.0f },
                                               0.0f,
                                               [] (float value, int) { return formatTimeValue(value); }));
                continue;
            }

            if (key == paramTimeHoldModeId || key == paramTimeReleaseModeId)
            {
                crossoverGroup->addChild(choiceParam(id, name, timeModeChoices, 0));
                continue;
            }

            if (key == paramTimeHoldSyncId || key == paramTimeReleaseSyncId)
            {
                crossoverGroup->addChild(choiceParam(id, name, getHostSyncChoices(), getDefaultHostSyncChoiceIndex()));
                continue;
            }

            if (key == paramTimeReleaseId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 1.0f, 500.0f, 1.0f },
                                               10.0f,
                                               [] (float value, int) { return formatTimeValue(value); }));
                continue;
            }

            if (key == paramTimeReleaseCurveId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { -100.0f, 100.0f, 1.0f },
                                               0.0f,
                                               [] (float value, int) { return formatCurveValue(value); }));
                continue;
            }

            if (key == paramSensLevelId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { -48.0f, 0.0f, 0.01f },
                                               -48.0f,
                                               [] (float value, int) { return formatDecibelValue(value); }));
                continue;
            }

            if (key == paramSensKneeId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 24.0f, 0.01f },
                                               0.0f,
                                               [] (float value, int) { return formatDecibelValue(value); }));
                continue;
            }

            if (key == paramSensRetriggerId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 250.0f, 1.0f },
                                               100.0f,
                                               [] (float value, int) { return formatTimeValue(value); }));
                continue;
            }

            if (key == paramLookaheadId)
            {
                crossoverGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 20.0f, 0.01f },
                                               1.0f,
                                               [] (float value, int) { return formatLookaheadValue(value); }));
            }
        }

        layout.add(std::move(crossoverGroup));
    }

    return layout;
}

void TrsModuleProcessor::cacheParameterPointers()
{
    for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
    {
        auto& crossover = rawRangeParameters[rangeIndex];
        crossover.transOn = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTransOnId));
        crossover.transGain = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTransGainId));
        crossover.sustainOn = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramSusOnId));
        crossover.sustainGain = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramSusGainId));
        crossover.hold = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTimeHoldId));
        crossover.holdMode = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTimeHoldModeId));
        crossover.holdSync = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTimeHoldSyncId));
        crossover.release = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTimeReleaseId));
        crossover.releaseCurve = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTimeReleaseCurveId));
        crossover.releaseMode = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTimeReleaseModeId));
        crossover.releaseSync = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramTimeReleaseSyncId));
        crossover.threshold = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramSensLevelId));
        crossover.knee = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramSensKneeId));
        crossover.retrigger = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramSensRetriggerId));
        crossover.lookahead = parameters.getRawParameterValue(makeCrossoverRangeParameterId(rangeIndex, paramLookaheadId));
    }

}

trs::dsp::DspCore::Parameters TrsModuleProcessor::readCrossoverRangeParameters(const size_t rangeIndex) const noexcept
{
    const auto& crossover = rawRangeParameters[juce::jmin(rangeIndex, numRanges - 1)];
    const auto hostBpm = getHostBpm();
    const auto holdHostSync = getParameterValue(crossover.holdMode, 0.0f) >= 0.5f;
    const auto releaseHostSync = getParameterValue(crossover.releaseMode, 0.0f) >= 0.5f;
    const auto holdSyncIndex = static_cast<int>(std::round(getParameterValue(crossover.holdSync,
                                                                              static_cast<float>(getDefaultHostSyncChoiceIndex()))));
    const auto releaseSyncIndex = static_cast<int>(std::round(getParameterValue(crossover.releaseSync,
                                                                                 static_cast<float>(getDefaultHostSyncChoiceIndex()))));

    trs::dsp::DspCore::Parameters result;
    result.transEnabled = isEnabled(crossover.transOn);
    result.sustainEnabled = isEnabled(crossover.sustainOn);
    result.transGainDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(crossover.transGain, 0.0f));
    result.sustainGainDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(crossover.sustainGain, 0.0f));
    result.holdMs = holdHostSync ? getHostSyncMilliseconds(holdSyncIndex, hostBpm)
                                 : juce::jlimit(0.0f, 200.0f, getParameterValue(crossover.hold, 0.0f));
    result.releaseMs = releaseHostSync ? getHostSyncMilliseconds(releaseSyncIndex, hostBpm)
                                       : juce::jlimit(1.0f, 500.0f, getParameterValue(crossover.release, 10.0f));
    result.releaseCurve = juce::jlimit(-100.0f, 100.0f, getParameterValue(crossover.releaseCurve, 0.0f));
    result.thresholdDb = juce::jlimit(-48.0f, 0.0f, getParameterValue(crossover.threshold, -48.0f));
    result.kneeDb = juce::jlimit(0.0f, 24.0f, getParameterValue(crossover.knee, 0.0f));
    result.retriggerMs = juce::jlimit(0.0f, 250.0f, getParameterValue(crossover.retrigger, 100.0f));
    result.lookaheadMs = juce::jlimit(0.0f, 20.0f, getParameterValue(crossover.lookahead, 1.0f));
    return result;
}

void TrsModuleProcessor::syncParameters()
{
    for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
        currentRangeParameters[rangeIndex] = readCrossoverRangeParameters(rangeIndex);

    processorBank.setRangeParameters(currentRangeParameters);
    const auto rangeLatencies = processorBank.getRangeLatencies();
    moduleLatencySamples = *std::max_element(rangeLatencies.begin(), rangeLatencies.end());
}

double TrsModuleProcessor::getHostBpm() const noexcept
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
