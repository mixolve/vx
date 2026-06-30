#include "module.spe.SpeProcessor.h"
#include "module.spe.ProcessorConstants.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* label;
};

inline constexpr auto speDeltaOrder = std::to_array<ParameterOrderEntry>({
    { "delta", "DELTA" },
});

inline constexpr auto speMainOrder = std::to_array<ParameterOrderEntry>({
    { "attack", "ATTACK" },
    { "release", "RELEASE" },
    { "knee", "KNEE" },
    { "ratio", "RATIO" },
    { "window_size", "WINDOW-SIZE" },
    { "hop_divisor", "HOP-DIV" },
    { "slope", "SLOPE" },
    { "l_threshold", "L.THRESHOLD" },
    { "l_adaptive", "L.ADAPTIVE" },
    { "l_offset", "L.OFFSET" },
    { "r_threshold", "R.THRESHOLD" },
    { "r_adaptive", "R.ADAPTIVE" },
    { "r_offset", "R.OFFSET" },
    { "link_lr", "LINK-LR (STEREO)" },
});

juce::String formatDecibelValue(const float value)
{
    return juce::String::formatted("%.0f dB", static_cast<double>(value));
}

juce::String formatSlopeValue(const float value)
{
    return juce::String::formatted("%.2f dB/oct", static_cast<double>(value));
}

juce::String formatRatioValue(const float value)
{
    return juce::String::formatted("%.2f:1", static_cast<double>(value));
}

juce::String formatTimeValue(const float value)
{
    return juce::String::formatted("%.0f ms", static_cast<double>(value));
}

juce::String formatPercentValue(const float value)
{
    return juce::String::formatted("%.0f%%", static_cast<double>(value));
}

juce::String formatFrequencyValue(const float value)
{
    return juce::String::formatted("%.2f Hz", static_cast<double>(value));
}

juce::String formatBandwidthValue(const float value)
{
    return juce::String::formatted("%.2f oct", static_cast<double>(value));
}

juce::String makePhaseFilterParameterId(const int filterIndex, const char* suffix)
{
    return "spe_phase_filter_" + juce::String(filterIndex + 1) + "_" + suffix;
}

}

juce::String SpeModuleProcessor::getPhaseFilterTypeParamId(const int filterIndex)
{
    return makePhaseFilterParameterId(filterIndex, "type");
}

juce::String SpeModuleProcessor::getPhaseFilterPlaceParamId(const int filterIndex)
{
    return makePhaseFilterParameterId(filterIndex, "place");
}

juce::String SpeModuleProcessor::getPhaseFilterSlopeParamId(const int filterIndex)
{
    return makePhaseFilterParameterId(filterIndex, "slope");
}

juce::String SpeModuleProcessor::getPhaseFilterFrequencyParamId(const int filterIndex)
{
    return makePhaseFilterParameterId(filterIndex, "frequency");
}

juce::String SpeModuleProcessor::getPhaseFilterBandwidthParamId(const int filterIndex)
{
    return makePhaseFilterParameterId(filterIndex, "bandwidth");
}

juce::String SpeModuleProcessor::getPhaseFilterImpactParamId(const int filterIndex)
{
    return makePhaseFilterParameterId(filterIndex, "impact");
}

SpeModuleProcessor::DisplaySettings SpeModuleProcessor::getDisplaySettings() const noexcept
{
    auto left = analyserLeftValue.load(std::memory_order_relaxed);
    auto right = analyserRightValue.load(std::memory_order_relaxed);
    auto low = analyserRangeLowValue.load(std::memory_order_relaxed);
    auto high = analyserRangeHighValue.load(std::memory_order_relaxed);

    left = juce::jlimit(0.0f, 1000.0f, left);
    right = juce::jlimit(1000.0f, analyserMaxFrequency, right);

    if (right <= left)
        right = juce::jmin(analyserMaxFrequency, left + 1.0f);

    if (high < low)
        std::swap(low, high);

    if ((high - low) < 6.0f)
    {
        const auto centre = 0.5f * (low + high);
        low = centre - 3.0f;
        high = centre + 3.0f;
    }

    return {
        left,
        right,
        juce::jlimit(analyserMinDecibels, analyserMaxDecibels - 6.0f, low),
        juce::jlimit(analyserMinDecibels + 6.0f, analyserMaxDecibels, high),
        juce::jlimit(analyserMinDecibels,
                     analyserMaxDecibels,
                     (dualMonoLeftAdaptiveParam != nullptr && dualMonoLeftAdaptiveParam->load(std::memory_order_relaxed) > 0.0f)
                         ? spectralCompressor.getPublishedDualMonoThresholdDb(0)
                         : (dualMonoLeftThresholdParam != nullptr ? dualMonoLeftThresholdParam->load(std::memory_order_relaxed) : 0.0f)),
        juce::jlimit(analyserMinDecibels,
                     analyserMaxDecibels,
                     (dualMonoRightAdaptiveParam != nullptr && dualMonoRightAdaptiveParam->load(std::memory_order_relaxed) > 0.0f)
                         ? spectralCompressor.getPublishedDualMonoThresholdDb(1)
                         : (dualMonoRightThresholdParam != nullptr ? dualMonoRightThresholdParam->load(std::memory_order_relaxed) : 0.0f)),
        juce::jlimit(0.0f, 6.0f, analyserSlopeValue.load(std::memory_order_relaxed))
    };
}

SpeModuleProcessor::AnalysisSettings SpeModuleProcessor::getAnalysisSettings() const noexcept
{
    return { getSelectedAnalyserFftSize(), getSelectedOverlapFactor(), getSelectedAveragingTimeMs() };
}

SpeModuleProcessor::CompressorSettings SpeModuleProcessor::getCompressorSettings() const noexcept
{
    CompressorSettings settings;
    settings.fftSize = getSelectedDspFftSize();
    settings.overlapFactor = getSelectedDspHopDivisor();
    settings.leftThresholdDb = juce::jlimit(-48.0f, 0.0f, dualMonoLeftThresholdParam != nullptr ? dualMonoLeftThresholdParam->load(std::memory_order_relaxed) : 0.0f);
    settings.rightThresholdDb = juce::jlimit(-48.0f, 0.0f, dualMonoRightThresholdParam != nullptr ? dualMonoRightThresholdParam->load(std::memory_order_relaxed) : 0.0f);
    settings.leftAdaptiveAmount = juce::jlimit(0.0f, 100.0f, dualMonoLeftAdaptiveParam != nullptr ? dualMonoLeftAdaptiveParam->load(std::memory_order_relaxed) : 0.0f);
    settings.rightAdaptiveAmount = juce::jlimit(0.0f, 100.0f, dualMonoRightAdaptiveParam != nullptr ? dualMonoRightAdaptiveParam->load(std::memory_order_relaxed) : 0.0f);
    settings.leftAdaptiveOffsetDb = juce::jlimit(0.0f, 48.0f, dualMonoLeftAdaptiveOffsetParam != nullptr ? dualMonoLeftAdaptiveOffsetParam->load(std::memory_order_relaxed) : 0.0f);
    settings.rightAdaptiveOffsetDb = juce::jlimit(0.0f, 48.0f, dualMonoRightAdaptiveOffsetParam != nullptr ? dualMonoRightAdaptiveOffsetParam->load(std::memory_order_relaxed) : 0.0f);
    settings.slopeDbPerOct = juce::jlimit(0.0f, 6.0f, dspSlopeParam != nullptr ? dspSlopeParam->load(std::memory_order_relaxed) : 4.5f);
    settings.attackMs = juce::jlimit(0.0f, 200.0f, attackParam != nullptr ? attackParam->load(std::memory_order_relaxed) : 0.0f);
    settings.releaseMs = juce::jlimit(0.0f, 2000.0f, releaseParam != nullptr ? releaseParam->load(std::memory_order_relaxed) : 0.0f);
    settings.kneeDb = juce::jlimit(0.0f, 24.0f, kneeParam != nullptr ? kneeParam->load(std::memory_order_relaxed) : 0.0f);
    settings.ratio = juce::jlimit(1.0f, 100.0f, ratioParam != nullptr ? ratioParam->load(std::memory_order_relaxed) : 100.0f);
    settings.phaseFilterCount = getActivePhaseFilterCount();

    for (auto filterIndex = 0; filterIndex < settings.phaseFilterCount; ++filterIndex)
    {
        auto& phaseFilter = settings.phaseFilters[static_cast<size_t>(filterIndex)];
        phaseFilter.type = juce::jlimit(0, 4, phaseTypeParams[static_cast<size_t>(filterIndex)] != nullptr ? juce::roundToInt(phaseTypeParams[static_cast<size_t>(filterIndex)]->load(std::memory_order_relaxed)) : 1);
        phaseFilter.place = juce::jlimit(0, 2, phasePlaceParams[static_cast<size_t>(filterIndex)] != nullptr ? juce::roundToInt(phasePlaceParams[static_cast<size_t>(filterIndex)]->load(std::memory_order_relaxed)) : 0);
        phaseFilter.slope = juce::jlimit(0, 5, phaseSlopeParams[static_cast<size_t>(filterIndex)] != nullptr ? juce::roundToInt(phaseSlopeParams[static_cast<size_t>(filterIndex)]->load(std::memory_order_relaxed)) : 1);
        phaseFilter.frequency = juce::jlimit(20.0f, 20000.0f, phaseFrequencyParams[static_cast<size_t>(filterIndex)] != nullptr ? phaseFrequencyParams[static_cast<size_t>(filterIndex)]->load(std::memory_order_relaxed) : 632.0f);
        phaseFilter.bandwidth = juce::jlimit(0.05f, 5.0f, phaseBandwidthParams[static_cast<size_t>(filterIndex)] != nullptr ? phaseBandwidthParams[static_cast<size_t>(filterIndex)]->load(std::memory_order_relaxed) : 1.0f);
        phaseFilter.impactPercent = juce::jlimit(-100.0f, 100.0f, phaseImpactParams[static_cast<size_t>(filterIndex)] != nullptr ? phaseImpactParams[static_cast<size_t>(filterIndex)]->load(std::memory_order_relaxed) : 0.0f);
    }

    return settings;
}

bool SpeModuleProcessor::isDeltaEnabled() const noexcept
{
    return deltaParam != nullptr
        && juce::roundToInt(deltaParam->load(std::memory_order_relaxed)) != 0;
}

int SpeModuleProcessor::getActivePhaseFilterCount() const noexcept
{
    return juce::jlimit(0,
                        maxPhaseFilterCount,
                        phaseFilterCountParam != nullptr ? juce::roundToInt(phaseFilterCountParam->load(std::memory_order_relaxed)) : 0);
}

bool SpeModuleProcessor::addPhaseFilter() noexcept
{
    const auto currentCount = getActivePhaseFilterCount();

    if (currentCount >= maxPhaseFilterCount)
        return false;

    const auto setParameterValue = [this] (const juce::String& parameterId, const float value)
    {
        if (auto* parameter = parameters.getParameter(parameterId))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };

    setParameterValue(getPhaseFilterTypeParamId(currentCount), 1.0f);
    setParameterValue(getPhaseFilterPlaceParamId(currentCount), 0.0f);
    setParameterValue(getPhaseFilterSlopeParamId(currentCount), 1.0f);
    setParameterValue(getPhaseFilterFrequencyParamId(currentCount), 632.0f);
    setParameterValue(getPhaseFilterBandwidthParamId(currentCount), 1.0f);
    setParameterValue(getPhaseFilterImpactParamId(currentCount), 0.0f);
    setParameterValue(paramPhaseFilterCountId, static_cast<float>(currentCount + 1));
    return true;
}

bool SpeModuleProcessor::removePhaseFilter(const int filterIndex) noexcept
{
    const auto currentCount = getActivePhaseFilterCount();

    if (filterIndex < 0 || filterIndex >= currentCount)
        return false;

    const auto setParameterValue = [this] (const juce::String& parameterId, const float value)
    {
        if (auto* parameter = parameters.getParameter(parameterId))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };

    const auto getParameterValue = [this] (const juce::String& parameterId, const float fallback) noexcept
    {
        if (const auto* value = parameters.getRawParameterValue(parameterId))
            return value->load(std::memory_order_relaxed);

        return fallback;
    };

    for (auto sourceIndex = filterIndex + 1; sourceIndex < currentCount; ++sourceIndex)
    {
        const auto destinationIndex = sourceIndex - 1;
        setParameterValue(getPhaseFilterTypeParamId(destinationIndex), getParameterValue(getPhaseFilterTypeParamId(sourceIndex), 1.0f));
        setParameterValue(getPhaseFilterPlaceParamId(destinationIndex), getParameterValue(getPhaseFilterPlaceParamId(sourceIndex), 0.0f));
        setParameterValue(getPhaseFilterSlopeParamId(destinationIndex), getParameterValue(getPhaseFilterSlopeParamId(sourceIndex), 1.0f));
        setParameterValue(getPhaseFilterFrequencyParamId(destinationIndex), getParameterValue(getPhaseFilterFrequencyParamId(sourceIndex), 632.0f));
        setParameterValue(getPhaseFilterBandwidthParamId(destinationIndex), getParameterValue(getPhaseFilterBandwidthParamId(sourceIndex), 1.0f));
        setParameterValue(getPhaseFilterImpactParamId(destinationIndex), getParameterValue(getPhaseFilterImpactParamId(sourceIndex), 0.0f));
    }

    const auto lastIndex = currentCount - 1;
    setParameterValue(getPhaseFilterTypeParamId(lastIndex), 1.0f);
    setParameterValue(getPhaseFilterPlaceParamId(lastIndex), 0.0f);
    setParameterValue(getPhaseFilterSlopeParamId(lastIndex), 1.0f);
    setParameterValue(getPhaseFilterFrequencyParamId(lastIndex), 632.0f);
    setParameterValue(getPhaseFilterBandwidthParamId(lastIndex), 1.0f);
    setParameterValue(getPhaseFilterImpactParamId(lastIndex), 0.0f);
    setParameterValue(paramPhaseFilterCountId, static_cast<float>(currentCount - 1));
    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout SpeModuleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    const auto makeSpeName = [] (const juce::String& tabName, const juce::String& parameterName)
    {
        return tabName + " - " + parameterName;
    };

    for (const auto& entry : speDeltaOrder)
    {
        const auto key = juce::String(entry.key);
        if (key == "delta")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramDeltaId, 1 },
                entry.label,
                false,
                juce::AudioParameterBoolAttributes()));
        }
    }

    for (const auto& entry : speMainOrder)
    {
        const auto key = juce::String(entry.key);
        const auto name = makeSpeName("SPE", entry.label);

        if (key == "attack")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramAttackId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 200.0f, 1.0f },
                0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatTimeValue(value);
                    })));
            continue;
        }

        if (key == "release")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramReleaseId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 2000.0f, 1.0f },
                0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatTimeValue(value);
                    })));
            continue;
        }

        if (key == "knee")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramKneeId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f },
                0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "ratio")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramRatioId, 1 },
                name,
                juce::NormalisableRange<float> { 1.0f, 100.0f, 0.1f },
                100.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatRatioValue(value);
                    })));
            continue;
        }

        if (key == "window_size")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { paramDspFftSizeId, 1 },
                name,
                juce::StringArray { "1024", "2048", "4096", "8192", "16384" },
                2,
                juce::AudioParameterChoiceAttributes()));
            continue;
        }

        if (key == "hop_divisor")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { paramDspHopDivisorId, 1 },
                name,
                juce::StringArray { "/2", "/4", "/8", "/16", "/32" },
                4,
                juce::AudioParameterChoiceAttributes()));
            continue;
        }

        if (key == "slope")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDspSlopeId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 6.0f, 0.01f },
                4.5f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatSlopeValue(value);
                    })));
            continue;
        }

        if (key == "l_threshold")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDualMonoLeftThresholdId, 1 },
                name,
                juce::NormalisableRange<float> { -48.0f, 0.0f, 0.1f },
                0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "l_adaptive")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDualMonoLeftAdaptiveId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f },
                0.0f,
                juce::AudioParameterFloatAttributes()));
            continue;
        }

        if (key == "l_offset")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDualMonoLeftAdaptiveOffsetId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 48.0f, 0.1f },
                0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "r_threshold")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDualMonoRightThresholdId, 1 },
                name,
                juce::NormalisableRange<float> { -48.0f, 0.0f, 0.1f },
                0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "r_adaptive")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDualMonoRightAdaptiveId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f },
                0.0f,
                juce::AudioParameterFloatAttributes()));
            continue;
        }

        if (key == "r_offset")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDualMonoRightAdaptiveOffsetId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 48.0f, 0.1f },
                0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "link_lr")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramDualMonoLinkId, 1 },
                name,
                true,
                juce::AudioParameterBoolAttributes()));
            continue;
        }

    }

    parameterLayout.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { paramPhaseFilterCountId, 1 },
        "SPE - PHASE FILTER COUNT",
        0,
        maxPhaseFilterCount,
        0,
        juce::AudioParameterIntAttributes()));

    for (auto filterIndex = 0; filterIndex < maxPhaseFilterCount; ++filterIndex)
    {
        const auto prefix = "SPE - PHASE " + juce::String(filterIndex + 1) + " - ";

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getPhaseFilterTypeParamId(filterIndex), 1 },
            prefix + "TYPE",
            juce::StringArray { "LSH", "BEL", "FTL", "HSH", "FUL" },
            1,
            juce::AudioParameterChoiceAttributes()));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getPhaseFilterPlaceParamId(filterIndex), 1 },
            prefix + "PLACE",
            juce::StringArray { "RTL", "LTR", "50" },
            0,
            juce::AudioParameterChoiceAttributes()));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getPhaseFilterSlopeParamId(filterIndex), 1 },
            prefix + "ORDER",
            juce::StringArray { "01", "02", "04", "08", "16", "++" },
            1,
            juce::AudioParameterChoiceAttributes()));

        auto frequencyRange = juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.01f };
        frequencyRange.setSkewForCentre(632.0f);
        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getPhaseFilterFrequencyParamId(filterIndex), 1 },
            prefix + "FREQ",
            frequencyRange,
            632.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatFrequencyValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getPhaseFilterBandwidthParamId(filterIndex), 1 },
            prefix + "BW",
            juce::NormalisableRange<float> { 0.05f, 5.0f, 0.01f },
            1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatBandwidthValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getPhaseFilterImpactParamId(filterIndex), 1 },
            prefix + "IMPACT",
            juce::NormalisableRange<float> { -100.0f, 100.0f, 1.0f },
            0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatPercentValue(value);
                })));
    }

    return { parameterLayout.begin(), parameterLayout.end() };
}

int SpeModuleProcessor::getSelectedAnalyserFftSize() const noexcept
{
    static constexpr std::array<int, 5> fftSizes { 1024, 2048, 4096, 8192, 16384 };
    const auto choiceIndex = juce::jlimit(0,
                                          static_cast<int>(fftSizes.size()) - 1,
                                          juce::roundToInt(analyserFftSizeValue.load(std::memory_order_relaxed)));
    return fftSizes[static_cast<size_t>(choiceIndex)];
}

int SpeModuleProcessor::getSelectedDspFftSize() const noexcept
{
    static constexpr std::array<int, 5> fftSizes { 1024, 2048, 4096, 8192, 16384 };
    const auto choiceIndex = dspFftSizeParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(fftSizes.size()) - 1,
                                          juce::roundToInt(dspFftSizeParam->load(std::memory_order_relaxed)))
                           : 3;
    return fftSizes[static_cast<size_t>(choiceIndex)];
}

int SpeModuleProcessor::getSelectedDspHopDivisor() const noexcept
{
    static constexpr std::array<int, 5> hopDivisors { 2, 4, 8, 16, 32 };
    const auto choiceIndex = dspHopDivisorParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(hopDivisors.size()) - 1,
                                          juce::roundToInt(dspHopDivisorParam->load(std::memory_order_relaxed)))
                           : 4;
    return hopDivisors[static_cast<size_t>(choiceIndex)];
}

int SpeModuleProcessor::getSelectedOverlapFactor() const noexcept
{
    static constexpr std::array<int, 5> overlapFactors { 2, 4, 8, 16, 32 };
    const auto choiceIndex = juce::jlimit(0,
                                          static_cast<int>(overlapFactors.size()) - 1,
                                          juce::roundToInt(analyserOverlapValue.load(std::memory_order_relaxed)));
    return overlapFactors[static_cast<size_t>(choiceIndex)];
}

float SpeModuleProcessor::getSelectedAveragingTimeMs() const noexcept
{
    return juce::jlimit(0.0f, 1000.0f, analyserTimeValue.load(std::memory_order_relaxed));
}
