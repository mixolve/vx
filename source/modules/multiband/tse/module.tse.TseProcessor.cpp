#include "module.tse.TseProcessor.h"

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

inline constexpr auto tseBandOrder = std::to_array<ParameterOrderEntry>({
    { TseModuleProcessor::paramTransOnId, "TRANS.ON/OFF" },
    { TseModuleProcessor::paramSusOnId, "SUS.ON/OFF" },
    { TseModuleProcessor::paramTransGainId, "TRANS.GAIN" },
    { TseModuleProcessor::paramSusGainId, "SUS.GAIN" },
    { TseModuleProcessor::paramTimeHoldId, "HOLD" },
    { TseModuleProcessor::paramTimeHoldModeId, "HOLD MODE" },
    { TseModuleProcessor::paramTimeHoldSyncId, "HOLD SYNC" },
    { TseModuleProcessor::paramTimeReleaseId, "RELEASE" },
    { TseModuleProcessor::paramTimeReleaseModeId, "RELEASE MODE" },
    { TseModuleProcessor::paramTimeReleaseSyncId, "RELEASE SYNC" },
    { TseModuleProcessor::paramTimeReleaseCurveId, "REL-CURVE" },
    { TseModuleProcessor::paramSensLevelId, "SENS.LVL" },
    { TseModuleProcessor::paramSensKneeId, "SENS.KNEE" },
    { TseModuleProcessor::paramSensRetriggerId, "SENS.RETR" },
    { TseModuleProcessor::paramLookaheadId, "LOOKAHEAD" },
});

inline constexpr auto tseCrossoverOrder = std::to_array<ParameterOrderEntry>({
    { "active_split_count", "SPLIT COUNT" },
    { "xover1", "CROSSOVER 1" },
    { "xover2", "CROSSOVER 2" },
    { "xover3", "CROSSOVER 3" },
    { "xover4", "CROSSOVER 4" },
    { "xover5", "CROSSOVER 5" },
});

constexpr auto gainMinDb = -48.0f;
constexpr auto gainMaxDb = 48.0f;
constexpr std::array<int, 9> hostSyncDenominators { 1, 2, 4, 8, 16, 32, 64, 128, 256 };
constexpr std::array<const char*, TseModuleProcessor::numCrossoverSlots> crossoverSuffixes {
    "xover1", "xover2", "xover3", "xover4", "xover5"
};

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

juce::String formatFrequencyValue(const float value)
{
    return juce::String::formatted("%.0f Hz", static_cast<double>(value));
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

TseModuleProcessor::TseModuleProcessor(juce::AudioProcessor& owner)
    : ownerProcessor(owner),
      parameters(internalParameterHost, nullptr, "tse_state", createParameterLayout())
{
    cacheParameterPointers();
}

TseModuleProcessor::~TseModuleProcessor() = default;

void TseModuleProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    preparedBlockSize = juce::jmax(1, samplesPerBlock);
    multibandProcessor.prepare(currentSampleRate, preparedBlockSize, ownerProcessor.getTotalNumOutputChannels());
    syncParameters();
    multibandProcessor.reset();
}

void TseModuleProcessor::releaseResources()
{
    multibandProcessor.reset();
}

void TseModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    vx::multiband::detail::clearOutputOnlyChannels(ownerProcessor, buffer);

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    syncParameters();
    multibandProcessor.process(buffer);
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
    cacheParameterPointers();
    syncParameters();
}

int TseModuleProcessor::getLatencySamples() const noexcept
{
    return moduleLatencySamples;
}

bool TseModuleProcessor::refreshLatencyState() noexcept
{
    const auto previousLatency = moduleLatencySamples;
    syncParameters();
    return previousLatency != moduleLatencySamples;
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

juce::String TseModuleProcessor::makeBandParameterId(const size_t bandIndex, const char* suffix)
{
    return "band" + juce::String(static_cast<int>(bandIndex + 1)) + "_" + suffix;
}

juce::String TseModuleProcessor::makeFullbandParameterId(const char* suffix)
{
    return "fullband_" + juce::String(suffix);
}

juce::String TseModuleProcessor::makeSoloParameterId(const size_t bandIndex)
{
    return "soloBand" + juce::String(static_cast<int>(bandIndex + 1));
}

juce::String TseModuleProcessor::makeActiveSplitCountParameterId()
{
    return "fullband_activeXovers";
}

juce::AudioProcessorValueTreeState::ParameterLayout TseModuleProcessor::createParameterLayout()
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
    auto soloGroup = std::make_unique<juce::AudioProcessorParameterGroup>("monitor", "Monitor", " | ");

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        soloGroup->addChild(boolParam(makeSoloParameterId(bandIndex),
                                      "Solo Band " + juce::String(static_cast<int>(bandIndex + 1)),
                                      false,
                                      true));
    }

    layout.add(std::move(soloGroup));

    auto fullbandGroup = std::make_unique<juce::AudioProcessorParameterGroup>("fullband", "Fullband", " | ");
    auto crossoverGroup = std::make_unique<juce::AudioProcessorParameterGroup>("fullband_crossover", "CROSSOVER", " | ");

    for (const auto& entry : tseCrossoverOrder)
    {
        const auto key = juce::String(entry.key);

        if (key == "active_split_count")
        {
            crossoverGroup->addChild(floatParam(makeActiveSplitCountParameterId(),
                                                "GLOBAL - CROSSOVER - " + juce::String(entry.label),
                                                juce::NormalisableRange<float> { 0.0f, 5.0f, 1.0f },
                                                0.0f,
                                                [] (float value, int)
                                                {
                                                    return juce::String(static_cast<int>(std::round(value)));
                                                },
                                                true));
            continue;
        }

        crossoverGroup->addChild(floatParam(makeFullbandParameterId(entry.key),
                                            "GLOBAL - CROSSOVER - " + juce::String(entry.label),
                                            juce::NormalisableRange<float> { 20.0f, 20000.0f, 1.0f },
                                            key == "xover1" ? 134.0f
                                                : key == "xover2" ? 523.0f
                                                : key == "xover3" ? 2093.0f
                                                : key == "xover4" ? 5000.0f
                                                : 10000.0f,
                                            [] (float value, int)
                                            {
                                                return formatFrequencyValue(value);
                                            },
                                            true));
    }

    fullbandGroup->addChild(std::move(crossoverGroup));
    layout.add(std::move(fullbandGroup));

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto bandGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "band" + juce::String(static_cast<int>(bandIndex + 1)),
            "Band " + juce::String(static_cast<int>(bandIndex + 1)),
            " | ");

        for (const auto& entry : tseBandOrder)
        {
            const auto key = juce::String(entry.key);
            const auto id = makeBandParameterId(bandIndex, entry.key);
            const auto name = "BAND " + juce::String(static_cast<int>(bandIndex + 1)) + " - TSE - " + juce::String(entry.label);

            if (key == paramTransOnId || key == paramSusOnId)
            {
                bandGroup->addChild(boolParam(id, name, true));
                continue;
            }

            if (key == paramTransGainId || key == paramSusGainId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { gainMinDb, gainMaxDb, 0.1f },
                                               0.0f,
                                               [] (float value, int) { return formatDecibelValue(value); }));
                continue;
            }

            if (key == paramTimeHoldId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 200.0f, 1.0f },
                                               0.0f,
                                               [] (float value, int) { return formatTimeValue(value); }));
                continue;
            }

            if (key == paramTimeHoldModeId || key == paramTimeReleaseModeId)
            {
                bandGroup->addChild(choiceParam(id, name, timeModeChoices, 0));
                continue;
            }

            if (key == paramTimeHoldSyncId || key == paramTimeReleaseSyncId)
            {
                bandGroup->addChild(choiceParam(id, name, getHostSyncChoices(), getDefaultHostSyncChoiceIndex()));
                continue;
            }

            if (key == paramTimeReleaseId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 1.0f, 500.0f, 1.0f },
                                               10.0f,
                                               [] (float value, int) { return formatTimeValue(value); }));
                continue;
            }

            if (key == paramTimeReleaseCurveId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { -100.0f, 100.0f, 1.0f },
                                               0.0f,
                                               [] (float value, int) { return formatCurveValue(value); }));
                continue;
            }

            if (key == paramSensLevelId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { -48.0f, 0.0f, 0.1f },
                                               -48.0f,
                                               [] (float value, int) { return formatDecibelValue(value); }));
                continue;
            }

            if (key == paramSensKneeId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f },
                                               0.0f,
                                               [] (float value, int) { return formatDecibelValue(value); }));
                continue;
            }

            if (key == paramSensRetriggerId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 250.0f, 1.0f },
                                               100.0f,
                                               [] (float value, int) { return formatTimeValue(value); }));
                continue;
            }

            if (key == paramLookaheadId)
            {
                bandGroup->addChild(floatParam(id,
                                               name,
                                               juce::NormalisableRange<float> { 0.0f, 20.0f, 0.01f },
                                               1.0f,
                                               [] (float value, int) { return formatLookaheadValue(value); }));
            }
        }

        layout.add(std::move(bandGroup));
    }

    return layout;
}

void TseModuleProcessor::cacheParameterPointers()
{
    rawActiveSplitCountParameter = parameters.getRawParameterValue(makeActiveSplitCountParameterId());

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        rawSoloParameters[bandIndex] = parameters.getRawParameterValue(makeSoloParameterId(bandIndex));
        auto& band = rawBandParameters[bandIndex];
        band.transOn = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTransOnId));
        band.transGain = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTransGainId));
        band.sustainOn = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramSusOnId));
        band.sustainGain = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramSusGainId));
        band.hold = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTimeHoldId));
        band.holdMode = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTimeHoldModeId));
        band.holdSync = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTimeHoldSyncId));
        band.release = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTimeReleaseId));
        band.releaseCurve = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTimeReleaseCurveId));
        band.releaseMode = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTimeReleaseModeId));
        band.releaseSync = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramTimeReleaseSyncId));
        band.threshold = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramSensLevelId));
        band.knee = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramSensKneeId));
        band.retrigger = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramSensRetriggerId));
        band.lookahead = parameters.getRawParameterValue(makeBandParameterId(bandIndex, paramLookaheadId));
    }

    for (size_t crossoverIndex = 0; crossoverIndex < numCrossoverSlots; ++crossoverIndex)
        rawCrossoverParameters[crossoverIndex] = parameters.getRawParameterValue(makeFullbandParameterId(crossoverSuffixes[crossoverIndex]));
}

tse::dsp::DspCore::Parameters TseModuleProcessor::readBandParameters(const size_t bandIndex) const noexcept
{
    const auto& band = rawBandParameters[juce::jmin(bandIndex, numBands - 1)];
    const auto hostBpm = getHostBpm();
    const auto holdHostSync = getParameterValue(band.holdMode, 0.0f) >= 0.5f;
    const auto releaseHostSync = getParameterValue(band.releaseMode, 0.0f) >= 0.5f;
    const auto holdSyncIndex = static_cast<int>(std::round(getParameterValue(band.holdSync,
                                                                              static_cast<float>(getDefaultHostSyncChoiceIndex()))));
    const auto releaseSyncIndex = static_cast<int>(std::round(getParameterValue(band.releaseSync,
                                                                                 static_cast<float>(getDefaultHostSyncChoiceIndex()))));

    tse::dsp::DspCore::Parameters result;
    result.transEnabled = isEnabled(band.transOn);
    result.sustainEnabled = isEnabled(band.sustainOn);
    result.transGainDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(band.transGain, 0.0f));
    result.sustainGainDb = juce::jlimit(gainMinDb, gainMaxDb, getParameterValue(band.sustainGain, 0.0f));
    result.holdMs = holdHostSync ? getHostSyncMilliseconds(holdSyncIndex, hostBpm)
                                 : juce::jlimit(0.0f, 200.0f, getParameterValue(band.hold, 0.0f));
    result.releaseMs = releaseHostSync ? getHostSyncMilliseconds(releaseSyncIndex, hostBpm)
                                       : juce::jlimit(1.0f, 500.0f, getParameterValue(band.release, 10.0f));
    result.releaseCurve = juce::jlimit(-100.0f, 100.0f, getParameterValue(band.releaseCurve, 0.0f));
    result.thresholdDb = juce::jlimit(-48.0f, 0.0f, getParameterValue(band.threshold, -48.0f));
    result.kneeDb = juce::jlimit(0.0f, 24.0f, getParameterValue(band.knee, 0.0f));
    result.retriggerMs = juce::jlimit(0.0f, 250.0f, getParameterValue(band.retrigger, 100.0f));
    result.lookaheadMs = juce::jlimit(0.0f, 20.0f, getParameterValue(band.lookahead, 1.0f));
    return result;
}

TseModuleProcessor::MultibandProcessor::CrossoverFrequencies TseModuleProcessor::readCrossoverFrequencies() const noexcept
{
    MultibandProcessor::CrossoverFrequencies frequencies { 134.0, 523.0, 2093.0, 5000.0, 10000.0 };

    for (size_t index = 0; index < numCrossoverSlots; ++index)
        frequencies[index] = static_cast<double>(getParameterValue(rawCrossoverParameters[index], static_cast<float>(frequencies[index])));

    return frequencies;
}

size_t TseModuleProcessor::readActiveSplitCount() const noexcept
{
    return static_cast<size_t>(juce::jlimit(0,
                                           static_cast<int>(numCrossoverSlots),
                                           static_cast<int>(std::round(getParameterValue(rawActiveSplitCountParameter, 0.0f)))));
}

TseModuleProcessor::MultibandProcessor::SoloMask TseModuleProcessor::readSoloMask() const noexcept
{
    MultibandProcessor::SoloMask soloMask {};

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
        soloMask[bandIndex] = getParameterValue(rawSoloParameters[bandIndex], 0.0f) >= 0.5f;

    return soloMask;
}

void TseModuleProcessor::syncParameters()
{
    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
        currentBandParameters[bandIndex] = readBandParameters(bandIndex);

    currentCrossoverFrequencies = readCrossoverFrequencies();
    currentActiveSplitCount = readActiveSplitCount();
    currentSoloMask = readSoloMask();
    multibandProcessor.setActiveSplitCount(currentActiveSplitCount);
    multibandProcessor.setCrossoverFrequencies(currentCrossoverFrequencies);
    multibandProcessor.setBandParameters(currentBandParameters);
    multibandProcessor.setSoloMask(currentSoloMask);
    moduleLatencySamples = multibandProcessor.getLatencySamples();
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
