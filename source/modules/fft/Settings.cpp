#include "Processor.h"
#include "Constants.h"

#include <array>
#include <cmath>
#include <limits>

float FftModuleProcessor::phaseThresholdToCorrelation(const float threshold) noexcept
{
    return juce::jmap(juce::jlimit(0.0f, 100.0f, threshold),
                      0.0f,
                      100.0f,
                      -1.0f,
                      1.0f);
}

FftModuleProcessor::CompressorSettings FftModuleProcessor::getCompressorSettings() const noexcept
{
    CompressorSettings settings;
    settings.fftSize = getSelectedDspFftSize();
    settings.overlapFactor = getSelectedDspOverlapFactor();
    settings.reductionDisplayTimeMs = getSelectedAveragingTimeMs();
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

int FftModuleProcessor::getSelectedDspFftSize() const noexcept
{
    static constexpr std::array<int, 5> fftSizes { 1024, 2048, 4096, 8192, 16384 };
    const auto choiceIndex = dspFftSizeParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(fftSizes.size()) - 1,
                                          juce::roundToInt(dspFftSizeParam->load(std::memory_order_relaxed)))
                           : 3;
    return fftSizes[static_cast<size_t>(choiceIndex)];
}

int FftModuleProcessor::getSelectedDspOverlapFactor() const noexcept
{
    static constexpr std::array<int, 5> overlapFactors { 2, 4, 8, 16, 32 };
    const auto choiceIndex = dspOverlapParam != nullptr
                           ? juce::jlimit(0, static_cast<int>(overlapFactors.size()) - 1,
                                          juce::roundToInt(dspOverlapParam->load(std::memory_order_relaxed)))
                           : 4;
    return overlapFactors[static_cast<size_t>(choiceIndex)];
}

float FftModuleProcessor::getSelectedAveragingTimeMs() const noexcept
{
    return juce::jlimit(0.0f, 1000.0f, analyserTimeValue.load(std::memory_order_relaxed));
}
