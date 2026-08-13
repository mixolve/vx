#include "module.fft.FftProcessor.h"
#include "module.fft.ProcessorConstants.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* label;
};

inline constexpr auto fftDeltaOrder = std::to_array<ParameterOrderEntry>({
    { "delta", "DELTA" },
});

inline constexpr auto fftMainOrder = std::to_array<ParameterOrderEntry>({
    { "dynamic_mode", "MODE" },
    { "attack", "ATTACK" },
    { "release", "RELEASE" },
    { "knee", "KNEE" },
    { "ratio", "RATIO" },
    { "floor", "FLOOR" },
    { "window_size", "WINDOW-SIZE" },
    { "hop_divisor", "HOP-DIV" },
    { "slope", "SLOPE" },
    { "l_threshold", "L.THRESHOLD" },
    { "l_adaptive", "L.ADAPTIVE" },
    { "r_threshold", "R.THRESHOLD" },
    { "r_adaptive", "R.ADAPTIVE" },
    { "link_lr", "LINK-LR (STEREO)" },
    { "dynamic_bypass", "BYPASS" },
});

juce::String formatDecibelValue(const float value)
{
    return juce::String::formatted("%.0f dB", static_cast<double>(value));
}

juce::String formatFloorValue(const float value)
{
    return value <= -100.0f ? "FULL" : formatDecibelValue(value);
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

juce::String formatCorrelationValue(const float value)
{
    return juce::String::formatted("%.2f", static_cast<double>(value));
}

juce::String formatImpactValue(const float value)
{
    if (value <= -99.995f)
        return "LEFT";
    if (value >= 99.995f)
        return "RIGHT";
    if (std::abs(value) <= 0.005f)
        return "BOTH";

    return formatCorrelationValue(value);
}

}

float FftModuleProcessor::phaseThresholdToCorrelation(const float threshold) noexcept
{
    return juce::jmap(juce::jlimit(0.0f, 100.0f, threshold),
                      0.0f,
                      100.0f,
                      -1.0f,
                      1.0f);
}

FftModuleProcessor::DisplaySettings FftModuleProcessor::getDisplaySettings() const noexcept
{
    const auto phaseMode = dynamicModeParam != nullptr
        && dynamicModeParam->load(std::memory_order_relaxed) >= 0.5f;
    auto left = analyserLeftValue.load(std::memory_order_relaxed);
    auto right = analyserRightValue.load(std::memory_order_relaxed);
    auto low = phaseMode
        ? analyserPhaseRangeLowValue.load(std::memory_order_relaxed)
        : analyserRangeLowValue.load(std::memory_order_relaxed);
    auto high = phaseMode
        ? analyserPhaseRangeHighValue.load(std::memory_order_relaxed)
        : analyserRangeHighValue.load(std::memory_order_relaxed);

    left = juce::jlimit(0.0f, 1000.0f, left);
    right = juce::jlimit(1000.0f, analyserMaxFrequency, right);

    if (right <= left)
        right = juce::jmin(analyserMaxFrequency, left + 1.0f);

    if (high < low)
        std::swap(low, high);

    const auto minimumRange = phaseMode ? 0.0f : 6.0f;

    if ((high - low) < minimumRange)
    {
        const auto centre = 0.5f * (low + high);
        low = centre - (minimumRange * 0.5f);
        high = centre + (minimumRange * 0.5f);
    }

    const auto phaseThreshold = phaseThresholdToCorrelation(
        phaseThresholdParam != nullptr ? phaseThresholdParam->load(std::memory_order_relaxed) : 0.0f);
    const auto displayedPhaseThreshold = phaseAdaptiveParam != nullptr
                                          && phaseAdaptiveParam->load(std::memory_order_relaxed) > 0.0f
        ? dynamicProcessor.getPublishedThreshold(0)
        : phaseThreshold;
    return {
        left,
        right,
        phaseMode ? juce::jlimit(-1.0f, 1.0f, low)
                  : juce::jlimit(analyserMinDecibels, analyserMaxDecibels - 6.0f, low),
        phaseMode ? juce::jlimit(-1.0f, 1.0f, high)
                  : juce::jlimit(analyserMinDecibels + 6.0f, analyserMaxDecibels, high),
        phaseMode
            ? juce::jlimit(-1.0f, 1.0f, displayedPhaseThreshold)
            : juce::jlimit(analyserMinDecibels,
                           analyserMaxDecibels,
                           (dualMonoLeftAdaptiveParam != nullptr && dualMonoLeftAdaptiveParam->load(std::memory_order_relaxed) > 0.0f)
                               ? dynamicProcessor.getPublishedThreshold(0)
                               : (dualMonoLeftThresholdParam != nullptr ? dualMonoLeftThresholdParam->load(std::memory_order_relaxed) : 0.0f)),
        phaseMode
            ? juce::jlimit(-1.0f, 1.0f, displayedPhaseThreshold)
            : juce::jlimit(analyserMinDecibels,
                           analyserMaxDecibels,
                           (dualMonoRightAdaptiveParam != nullptr && dualMonoRightAdaptiveParam->load(std::memory_order_relaxed) > 0.0f)
                               ? dynamicProcessor.getPublishedThreshold(1)
                               : (dualMonoRightThresholdParam != nullptr ? dualMonoRightThresholdParam->load(std::memory_order_relaxed) : 0.0f)),
        juce::jlimit(0.0f, 6.0f, analyserSlopeValue.load(std::memory_order_relaxed)),
        phaseMode
    };
}

FftModuleProcessor::AnalysisSettings FftModuleProcessor::getAnalysisSettings() const noexcept
{
    return { getSelectedAnalyserFftSize(), getSelectedOverlapFactor(), getSelectedAveragingTimeMs() };
}

FftModuleProcessor::CompressorSettings FftModuleProcessor::getCompressorSettings() const noexcept
{
    CompressorSettings settings;
    settings.fftSize = getSelectedDspFftSize();
    settings.overlapFactor = getSelectedDspHopDivisor();
    settings.phaseMode = dynamicModeParam != nullptr
        && dynamicModeParam->load(std::memory_order_relaxed) >= 0.5f;
    const auto floorValue = juce::jlimit(-100.0f,
                                         0.0f,
                                         floorParam != nullptr ? floorParam->load(std::memory_order_relaxed) : -60.0f);
    settings.floorDb = floorValue <= -100.0f
        ? -std::numeric_limits<float>::infinity()
        : floorValue;
    settings.leftThresholdDb = juce::jlimit(-99.0f, 0.0f, dualMonoLeftThresholdParam != nullptr ? dualMonoLeftThresholdParam->load(std::memory_order_relaxed) : 0.0f);
    settings.rightThresholdDb = juce::jlimit(-99.0f, 0.0f, dualMonoRightThresholdParam != nullptr ? dualMonoRightThresholdParam->load(std::memory_order_relaxed) : 0.0f);
    const auto phaseThreshold = juce::jlimit(0.0f,
                                             100.0f,
                                             phaseThresholdParam != nullptr
                                                 ? phaseThresholdParam->load(std::memory_order_relaxed)
                                                 : 0.0f);
    const auto phaseAdaptive = juce::jlimit(0.0f,
                                            100.0f,
                                            phaseAdaptiveParam != nullptr
                                                ? phaseAdaptiveParam->load(std::memory_order_relaxed)
                                                : 0.0f);
    const auto phaseSlopePerOctave = (juce::jlimit(-9.0f,
                                                    9.0f,
                                                    phaseSlopeParam != nullptr
                                                        ? phaseSlopeParam->load(std::memory_order_relaxed)
                                                        : 0.0f)
                                      / 9.0f)
        / std::log2(analyserMaxFrequency / analyserMinFrequency);
    settings.phaseThreshold = phaseThreshold;
    settings.phaseAdaptiveAmount = phaseAdaptive;
    settings.phaseSlopePerOctave = phaseSlopePerOctave;
    settings.phaseImpact = juce::jlimit(-100.0f,
                                        100.0f,
                                        phaseImpactParam != nullptr
                                            ? phaseImpactParam->load(std::memory_order_relaxed)
                                            : 0.0f);
    settings.leftAdaptiveAmount = juce::jlimit(0.0f, 100.0f, dualMonoLeftAdaptiveParam != nullptr ? dualMonoLeftAdaptiveParam->load(std::memory_order_relaxed) : 0.0f);
    settings.rightAdaptiveAmount = juce::jlimit(0.0f, 100.0f, dualMonoRightAdaptiveParam != nullptr ? dualMonoRightAdaptiveParam->load(std::memory_order_relaxed) : 0.0f);
    const auto* adaptiveOffsetParam = settings.phaseMode ? phaseAdaptiveOffsetParam : spectralAdaptiveOffsetParam;
    const auto* adaptiveAttackParam = settings.phaseMode ? phaseAdaptiveAttackParam : spectralAdaptiveAttackParam;
    const auto* adaptiveHoldParam = settings.phaseMode ? phaseAdaptiveHoldParam : spectralAdaptiveHoldParam;
    const auto* adaptiveReleaseParam = settings.phaseMode ? phaseAdaptiveReleaseParam : spectralAdaptiveReleaseParam;
    settings.adaptiveOffset = juce::jlimit(settings.phaseMode ? -1.0f : 0.0f,
                                           settings.phaseMode ? 1.0f : 48.0f,
                                           adaptiveOffsetParam != nullptr
                                               ? adaptiveOffsetParam->load(std::memory_order_relaxed)
                                               : 0.0f);
    settings.adaptiveAttackMs = juce::jlimit(0.0f,
                                             200.0f,
                                             adaptiveAttackParam != nullptr
                                                 ? adaptiveAttackParam->load(std::memory_order_relaxed)
                                                 : 30.0f);
    settings.adaptiveHoldMs = juce::jlimit(0.0f,
                                           2000.0f,
                                           adaptiveHoldParam != nullptr
                                               ? adaptiveHoldParam->load(std::memory_order_relaxed)
                                               : 0.0f);
    settings.adaptiveReleaseMs = juce::jlimit(0.0f,
                                              2000.0f,
                                              adaptiveReleaseParam != nullptr
                                                  ? adaptiveReleaseParam->load(std::memory_order_relaxed)
                                                  : 300.0f);
    settings.slopeDbPerOct = juce::jlimit(-9.0f, 9.0f, dspSlopeParam != nullptr ? dspSlopeParam->load(std::memory_order_relaxed) : 4.5f);
    settings.attackMs = juce::jlimit(0.0f, 200.0f, attackParam != nullptr ? attackParam->load(std::memory_order_relaxed) : 0.0f);
    settings.releaseMs = juce::jlimit(0.0f, 2000.0f, releaseParam != nullptr ? releaseParam->load(std::memory_order_relaxed) : 0.0f);
    settings.kneeDb = juce::jlimit(0.0f, 24.0f, kneeParam != nullptr ? kneeParam->load(std::memory_order_relaxed) : 0.0f);
    settings.ratio = juce::jlimit(1.0f, 100.0f, ratioParam != nullptr ? ratioParam->load(std::memory_order_relaxed) : 100.0f);
    settings.dynamicBypassed = dynamicBypassParam != nullptr
        && dynamicBypassParam->load(std::memory_order_relaxed) >= 0.5f;
    return settings;
}

bool FftModuleProcessor::isDeltaEnabled() const noexcept
{
    return deltaParam != nullptr
        && juce::roundToInt(deltaParam->load(std::memory_order_relaxed)) != 0;
}

juce::AudioProcessorValueTreeState::ParameterLayout FftModuleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    const auto makeFftName = [] (const juce::String& blockName, const juce::String& parameterName)
    {
        return "FFT / " + blockName + " / " + parameterName;
    };

    for (const auto& entry : fftDeltaOrder)
    {
        const auto key = juce::String(entry.key);
        if (key == "delta")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramDeltaId, 1 },
                makeFftName("DYNAMIC PROCESSOR", entry.label),
                false,
                juce::AudioParameterBoolAttributes()));
        }
    }

    for (const auto& entry : fftMainOrder)
    {
        const auto key = juce::String(entry.key);
        const auto isFftParameter = key == "window_size" || key == "hop_divisor" || key == "slope";
        const auto name = makeFftName(isFftParameter ? "GENERAL PROCESSOR" : "DYNAMIC PROCESSOR", entry.label);

        if (key == "dynamic_mode")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramDynamicModeId, 1 },
                name,
                false,
                juce::AudioParameterBoolAttributes()));
            continue;
        }

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

        if (key == "floor")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramFloorId, 1 },
                name,
                juce::NormalisableRange<float> { -100.0f, 0.0f, 0.1f },
                -60.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatFloorValue(value);
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
                juce::StringArray { "2", "4", "8", "16", "32" },
                4,
                juce::AudioParameterChoiceAttributes()));
            continue;
        }

        if (key == "slope")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDspSlopeId, 1 },
                name,
                juce::NormalisableRange<float> { -9.0f, 9.0f, 0.01f },
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
                juce::NormalisableRange<float> { -99.0f, 0.0f, 0.1f },
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

        if (key == "r_threshold")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramDualMonoRightThresholdId, 1 },
                name,
                juce::NormalisableRange<float> { -99.0f, 0.0f, 0.1f },
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

        if (key == "link_lr")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramDualMonoLinkId, 1 },
                name,
                true,
                juce::AudioParameterBoolAttributes()));
            continue;
        }

        if (key == "dynamic_bypass")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramDynamicBypassId, 1 },
                name,
                false,
                juce::AudioParameterBoolAttributes()));
            continue;
        }

    }

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramSpectralAdaptiveOffsetId, 1 },
        makeFftName("ADAP SETTINGS", "OFFSET"),
        juce::NormalisableRange<float> { 0.0f, 48.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    const auto addAdaptiveTimeParameter = [&parameterLayout, &makeFftName] (const char* parameterId,
                                                                            const char* parameterName,
                                                                            const float maximum,
                                                                            const float defaultValue)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { parameterId, 1 },
            makeFftName("ADAP SETTINGS", parameterName),
            juce::NormalisableRange<float> { 0.0f, maximum, 1.0f },
            defaultValue,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatTimeValue(value);
                })));
    };

    addAdaptiveTimeParameter(paramSpectralAdaptiveAttackId, "ATTACK", 200.0f, 30.0f);
    addAdaptiveTimeParameter(paramSpectralAdaptiveHoldId, "HOLD", 2000.0f, 0.0f);
    addAdaptiveTimeParameter(paramSpectralAdaptiveReleaseId, "RELEASE", 2000.0f, 300.0f);

    const auto addPhaseParameter = [&parameterLayout, &makeFftName] (const char* parameterId,
                                                                     const char* parameterName,
                                                                     const float minimum,
                                                                     const float maximum,
                                                                     const float defaultValue,
                                                                     const bool reversed = false,
                                                                     const float interval = 0.01f,
                                                                     const char* blockName = "DYNAMIC PROCESSOR")
    {
        auto range = juce::NormalisableRange<float> { minimum, maximum, interval };

        if (reversed)
        {
            range = juce::NormalisableRange<float> {
                minimum,
                maximum,
                [] (const float start, const float end, const float normalised)
                {
                    return end - (normalised * (end - start));
                },
                [] (const float start, const float end, const float value)
                {
                    return (end - juce::jlimit(start, end, value)) / (end - start);
                },
                [] (const float start, const float end, const float value)
                {
                    const auto clamped = juce::jlimit(start, end, value);
                    return std::round(clamped * 100.0f) * 0.01f;
                }
            };
            range.interval = 0.01f;
        }

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { parameterId, 1 },
            makeFftName(blockName, parameterName),
            range,
            defaultValue,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatCorrelationValue(value);
                })));
    };

    addPhaseParameter(paramPhaseThresholdId, "THRESHOLD", 0.0f, 100.0f, 0.0f, true);
    addPhaseParameter(paramPhaseAdaptiveId, "ADAPTIVE", 0.0f, 100.0f, 0.0f, false, 1.0f);
    addPhaseParameter(paramPhaseAdaptiveOffsetId, "OFFSET", -1.0f, 1.0f, 0.0f, false, 0.01f, "ADAP SETTINGS");
    addAdaptiveTimeParameter(paramPhaseAdaptiveAttackId, "ATTACK", 200.0f, 30.0f);
    addAdaptiveTimeParameter(paramPhaseAdaptiveHoldId, "HOLD", 2000.0f, 0.0f);
    addAdaptiveTimeParameter(paramPhaseAdaptiveReleaseId, "RELEASE", 2000.0f, 300.0f);
    addPhaseParameter(paramPhaseSlopeId, "SLOPE", -9.0f, 9.0f, 0.0f);
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramPhaseImpactId, 1 },
        makeFftName("DYNAMIC PROCESSOR", "IMPACT"),
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([] (float value, int)
            {
                return formatImpactValue(value);
            })
            .withValueFromStringFunction([] (const juce::String& text)
            {
                const auto valueText = text.trim();

                if (valueText.equalsIgnoreCase("LEFT"))
                    return -100.0f;
                if (valueText.equalsIgnoreCase("BOTH"))
                    return 0.0f;
                if (valueText.equalsIgnoreCase("RIGHT"))
                    return 100.0f;

                return valueText.getFloatValue();
            })));

    return { parameterLayout.begin(), parameterLayout.end() };
}

int FftModuleProcessor::getSelectedAnalyserFftSize() const noexcept
{
    static constexpr std::array<int, 5> fftSizes { 1024, 2048, 4096, 8192, 16384 };
    const auto choiceIndex = juce::jlimit(0,
                                          static_cast<int>(fftSizes.size()) - 1,
                                          juce::roundToInt(analyserFftSizeValue.load(std::memory_order_relaxed)));
    return fftSizes[static_cast<size_t>(choiceIndex)];
}

int FftModuleProcessor::getSelectedDspFftSize() const noexcept
{
    static constexpr std::array<int, 5> fftSizes { 1024, 2048, 4096, 8192, 16384 };
    const auto choiceIndex = dspFftSizeParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(fftSizes.size()) - 1,
                                          juce::roundToInt(dspFftSizeParam->load(std::memory_order_relaxed)))
                           : 3;
    return fftSizes[static_cast<size_t>(choiceIndex)];
}

int FftModuleProcessor::getSelectedDspHopDivisor() const noexcept
{
    static constexpr std::array<int, 5> hopDivisors { 2, 4, 8, 16, 32 };
    const auto choiceIndex = dspHopDivisorParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(hopDivisors.size()) - 1,
                                          juce::roundToInt(dspHopDivisorParam->load(std::memory_order_relaxed)))
                           : 4;
    return hopDivisors[static_cast<size_t>(choiceIndex)];
}

int FftModuleProcessor::getSelectedOverlapFactor() const noexcept
{
    static constexpr std::array<int, 5> overlapFactors { 2, 4, 8, 16, 32 };
    const auto choiceIndex = juce::jlimit(0,
                                          static_cast<int>(overlapFactors.size()) - 1,
                                          juce::roundToInt(analyserOverlapValue.load(std::memory_order_relaxed)));
    return overlapFactors[static_cast<size_t>(choiceIndex)];
}

float FftModuleProcessor::getSelectedAveragingTimeMs() const noexcept
{
    return juce::jlimit(0.0f, 1000.0f, analyserTimeValue.load(std::memory_order_relaxed));
}
