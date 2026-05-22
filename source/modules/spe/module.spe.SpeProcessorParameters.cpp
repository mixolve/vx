#include "module.spe.SpeProcessor.h"
#include "module.spe.ProcessorConstants.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::String formatDecibelValue(const float value)
{
    return juce::String::formatted("%.0f dB", static_cast<double>(value));
}

juce::String formatFrequencyValue(const float value)
{
    return juce::String::formatted("%.0f Hz", static_cast<double>(value));
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
}

SpeModuleProcessor::DisplaySettings SpeModuleProcessor::getDisplaySettings() const noexcept
{
    auto left = leftParam != nullptr ? leftParam->load(std::memory_order_relaxed) : 21.0f;
    auto right = rightParam != nullptr ? rightParam->load(std::memory_order_relaxed) : 20000.0f;
    auto low = rangeLowParam != nullptr ? rangeLowParam->load(std::memory_order_relaxed) : -60.0f;
    auto high = rangeHighParam != nullptr ? rangeHighParam->load(std::memory_order_relaxed) : 10.0f;

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
                     (stereoAdaptiveParam != nullptr && stereoAdaptiveParam->load(std::memory_order_relaxed) > 0.0f)
                         ? spectralCompressor.getPublishedStereoThresholdDb()
                         : (thresholdParam != nullptr ? thresholdParam->load(std::memory_order_relaxed) : 12.0f)),
        juce::jlimit(0.0f, 6.0f, slopeParam != nullptr ? slopeParam->load(std::memory_order_relaxed) : 4.0f)
    };
}

SpeModuleProcessor::AnalysisSettings SpeModuleProcessor::getAnalysisSettings() const noexcept
{
    return { getSelectedAnalyserFftSize(), getSelectedOverlapFactor(), getSelectedAveragingTimeMs() };
}

SpeModuleProcessor::CompressorSettings SpeModuleProcessor::getCompressorSettings() const noexcept
{
    return {
        getSelectedDspFftSize(),
        getSelectedOverlapFactor(),
        juce::jlimit(-120.0f, 12.0f, thresholdParam != nullptr ? thresholdParam->load(std::memory_order_relaxed) : 12.0f),
        juce::jlimit(0.0f, 100.0f, stereoAdaptiveParam != nullptr ? stereoAdaptiveParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(0.0f, 48.0f, stereoAdaptiveOffsetParam != nullptr ? stereoAdaptiveOffsetParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(-120.0f, 12.0f, dualMonoLeftThresholdParam != nullptr ? dualMonoLeftThresholdParam->load(std::memory_order_relaxed) : 12.0f),
        juce::jlimit(-120.0f, 12.0f, dualMonoRightThresholdParam != nullptr ? dualMonoRightThresholdParam->load(std::memory_order_relaxed) : 12.0f),
        juce::jlimit(0.0f, 100.0f, dualMonoLeftAdaptiveParam != nullptr ? dualMonoLeftAdaptiveParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(0.0f, 100.0f, dualMonoRightAdaptiveParam != nullptr ? dualMonoRightAdaptiveParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(0.0f, 48.0f, dualMonoLeftAdaptiveOffsetParam != nullptr ? dualMonoLeftAdaptiveOffsetParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(0.0f, 48.0f, dualMonoRightAdaptiveOffsetParam != nullptr ? dualMonoRightAdaptiveOffsetParam->load(std::memory_order_relaxed) : 0.0f),
        isStereoBypassEnabled(),
        dualMonoBypassParam != nullptr && juce::roundToInt(dualMonoBypassParam->load(std::memory_order_relaxed)) != 0,
        juce::jlimit(0.0f, 6.0f, dspSlopeParam != nullptr ? dspSlopeParam->load(std::memory_order_relaxed) : 4.0f),
        juce::jlimit(0.0f, 200.0f, attackParam != nullptr ? attackParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(0.0f, 2000.0f, releaseParam != nullptr ? releaseParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(0.0f, 24.0f, kneeParam != nullptr ? kneeParam->load(std::memory_order_relaxed) : 0.0f),
        juce::jlimit(1.0f, 100.0f, ratioParam != nullptr ? ratioParam->load(std::memory_order_relaxed) : 100.0f),
        juce::jlimit(-24.0f, 24.0f, makeupParam != nullptr ? makeupParam->load(std::memory_order_relaxed) : 0.0f)
    };
}

bool SpeModuleProcessor::isDeltaEnabled() const noexcept
{
    return deltaParam != nullptr
        && juce::roundToInt(deltaParam->load(std::memory_order_relaxed)) != 0;
}

bool SpeModuleProcessor::isStereoBypassEnabled() const noexcept
{
    return bypassParam != nullptr
        && juce::roundToInt(bypassParam->load(std::memory_order_relaxed)) != 0;
}

bool SpeModuleProcessor::isModuleBypassEnabled() const noexcept
{
    return miscBypassParam != nullptr
        && juce::roundToInt(miscBypassParam->load(std::memory_order_relaxed)) != 0;
}

bool SpeModuleProcessor::isModuleBypassWithGainEnabled() const noexcept
{
    return miscBypassWithGainParam != nullptr
        && juce::roundToInt(miscBypassWithGainParam->load(std::memory_order_relaxed)) != 0;
}

void SpeModuleProcessor::applyMakeupGain(juce::AudioBuffer<float>& buffer, const int channelsToUse) const noexcept
{
    const auto makeupDb = juce::jlimit(-24.0f, 24.0f, makeupParam != nullptr ? makeupParam->load(std::memory_order_relaxed) : 0.0f);

    if (std::abs(makeupDb) <= 1.0e-6f)
        return;

    const auto gain = juce::Decibels::decibelsToGain(makeupDb);

    for (auto channel = 0; channel < channelsToUse; ++channel)
        buffer.applyGain(channel, 0, buffer.getNumSamples(), gain);
}

juce::AudioProcessorValueTreeState::ParameterLayout SpeModuleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDualMonoLeftThresholdId, 1 },
        "DUAL-MONO - LL-THRESHOLD",
        juce::NormalisableRange<float> { -120.0f, 12.0f, 0.1f },
        12.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDualMonoLeftAdaptiveId, 1 },
        "DUAL-MONO - LL-ADAPTIVE",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f },
        0.0f));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDualMonoLeftAdaptiveOffsetId, 1 },
        "DUAL-MONO - LL-OFFSET",
        juce::NormalisableRange<float> { 0.0f, 48.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDualMonoRightThresholdId, 1 },
        "DUAL-MONO - RR-THRESHOLD",
        juce::NormalisableRange<float> { -120.0f, 12.0f, 0.1f },
        12.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDualMonoRightAdaptiveId, 1 },
        "DUAL-MONO - RR-ADAPTIVE",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f },
        0.0f));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDualMonoRightAdaptiveOffsetId, 1 },
        "DUAL-MONO - RR-OFFSET",
        juce::NormalisableRange<float> { 0.0f, 48.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramDualMonoBypassId, 1 },
        "DUAL-MONO - BYPASS",
        true));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramThresholdId, 1 },
        "STEREO - THRESHOLD",
        juce::NormalisableRange<float> { -120.0f, 12.0f, 0.1f },
        12.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramStereoAdaptiveId, 1 },
        "STEREO - ADAPTIVE",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f },
        0.0f));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramStereoAdaptiveOffsetId, 1 },
        "STEREO - OFFSET",
        juce::NormalisableRange<float> { 0.0f, 48.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramBypassId, 1 },
        "STEREO - BYPASS",
        true));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramMiscBypassId, 1 },
        "MISC - BYPASS",
        false));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramMiscBypassWithGainId, 1 },
        "MISC - BYPASS.WT-GAIN",
        false));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramInputGainId, 1 },
        "GLOBAL - IN-GAIN",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramAttackId, 1 },
        "GLOBAL - ATTACK",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 1.0f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatTimeValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramReleaseId, 1 },
        "GLOBAL - RELEASE",
        juce::NormalisableRange<float> { 0.0f, 2000.0f, 1.0f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatTimeValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramKneeId, 1 },
        "GLOBAL - KNEE",
        juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramRatioId, 1 },
        "GLOBAL - RATIO",
        juce::NormalisableRange<float> { 1.0f, 100.0f, 0.1f },
        100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatRatioValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramDspFftSizeId, 1 },
        "GLOBAL - WINDOW-SIZE",
        juce::StringArray { "1024", "2048", "4096", "8192", "16384" },
        3));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDspSlopeId, 1 },
        "GLOBAL - SLOPE",
        juce::NormalisableRange<float> { 0.0f, 6.0f, 0.01f },
        4.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatSlopeValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMakeupId, 1 },
        "GLOBAL - OUT-GAIN",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramDeltaId, 1 },
        "GLOBAL - DELTA",
        false));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramFftSizeId, 1 },
        "ANALYSER - FFT-SIZE",
        juce::StringArray { "1024", "2048", "4096", "8192", "16384" },
        3));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramOverlapId, 1 },
        "ANALYSER - OVERLAP",
        juce::StringArray { "2x", "4x", "8x", "16x", "32x" },
        4));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramLeftId, 1 },
        "ANALYSER - LEFT",
        juce::NormalisableRange<float> { 0.0f, 1000.0f, 1.0f },
        21.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatFrequencyValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramRightId, 1 },
        "ANALYSER - RIGHT",
        juce::NormalisableRange<float> { 1000.0f, analyserMaxFrequency, 1.0f },
        20000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatFrequencyValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramRangeLowId, 1 },
        "ANALYSER - LOW",
        juce::NormalisableRange<float> { -120.0f, -24.0f, 0.1f },
        -60.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramRangeHighId, 1 },
        "ANALYSER - HIGH",
        juce::NormalisableRange<float> { -48.0f, 20.0f, 0.1f },
        10.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramSlopeId, 1 },
        "ANALYSER - SLOPE",
        juce::NormalisableRange<float> { 0.0f, 6.0f, 0.01f },
        4.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatSlopeValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramTimeId, 1 },
        "ANALYSER - TIME",
        juce::NormalisableRange<float> { 0.0f, 1000.0f, 1.0f },
        50.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatTimeValue(value);
            })));

    return { parameterLayout.begin(), parameterLayout.end() };
}

int SpeModuleProcessor::getSelectedAnalyserFftSize() const noexcept
{
    static constexpr std::array<int, 5> fftSizes { 1024, 2048, 4096, 8192, 16384 };
    const auto choiceIndex = fftSizeParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(fftSizes.size()) - 1,
                                          juce::roundToInt(fftSizeParam->load(std::memory_order_relaxed)))
                           : 3;
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

int SpeModuleProcessor::getSelectedOverlapFactor() const noexcept
{
    static constexpr std::array<int, 5> overlapFactors { 2, 4, 8, 16, 32 };
    const auto choiceIndex = overlapParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(overlapFactors.size()) - 1,
                                          juce::roundToInt(overlapParam->load(std::memory_order_relaxed)))
                           : 4;
    return overlapFactors[static_cast<size_t>(choiceIndex)];
}

float SpeModuleProcessor::getSelectedAveragingTimeMs() const noexcept
{
    return juce::jlimit(0.0f,
                        1000.0f,
                        timeParam != nullptr ? timeParam->load(std::memory_order_relaxed) : 50.0f);
}
