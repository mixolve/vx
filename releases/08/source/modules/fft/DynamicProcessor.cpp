#include "Processor.h"
#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <limits>

FftModuleProcessor::DynamicProcessor::DynamicProcessor()
{
    const std::array<int, 5> fftOrders { 10, 11, 12, 13, 14 };

    for (auto i = 0; i < static_cast<int>(fftOrders.size()); ++i)
    {
        const auto fftSize = 1 << fftOrders[static_cast<size_t>(i)];
        ffts[static_cast<size_t>(i)] = std::make_unique<juce::dsp::FFT>(fftOrders[static_cast<size_t>(i)]);

        for (auto sampleIndex = 0; sampleIndex < fftSize; ++sampleIndex)
        {
            windowTables[static_cast<size_t>(i)][static_cast<size_t>(sampleIndex)]
                = 0.5f * (1.0f - std::cos((2.0f * juce::MathConstants<float>::pi * static_cast<float>(sampleIndex))
                                          / static_cast<float>(fftSize - 1)));
        }
    }
}

void FftModuleProcessor::DynamicProcessor::prepare(double newSampleRate, int numChannels)
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    configuredChannels = juce::jlimit(0, maxChannels, numChannels);
    reconfigure(configuredChannels, 0, 0);
}

void FftModuleProcessor::DynamicProcessor::reset() noexcept
{
    reconfigure(configuredChannels, currentFftSize, currentHopSize);
}

void FftModuleProcessor::DynamicProcessor::copyReductionScope(std::array<float, analyserScopeSize>& leftDestination,
                                                                std::array<float, analyserScopeSize>& rightDestination) const
{
    const auto activeIndex = activeReductionScopeBuffer.load(std::memory_order_acquire);
    leftDestination = reductionScopeBuffers[static_cast<size_t>(activeIndex)][0];
    rightDestination = reductionScopeBuffers[static_cast<size_t>(activeIndex)][1];
}

void FftModuleProcessor::DynamicProcessor::processBuffer(juce::AudioBuffer<float>& buffer,
                                                           int numInputChannels,
                                                           const CompressorSettings& settings)
{
    const auto channelsToUse = juce::jlimit(0, maxChannels, juce::jmin(numInputChannels, buffer.getNumChannels()));

    if (channelsToUse <= 0)
        return;

    const auto fftSize = juce::jlimit(1024, maxFftSize, settings.fftSize);
    const auto overlapFactor = juce::jmax(1, settings.overlapFactor);
    const auto hopSize = juce::jmax(1, fftSize / overlapFactor);

    if (channelsToUse != configuredChannels || fftSize != currentFftSize || hopSize != currentHopSize)
        reconfigure(channelsToUse, fftSize, hopSize);

    const auto fftIndex = getFftIndexForSize(fftSize);

    for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        for (auto channel = 0; channel < channelsToUse; ++channel)
            hopBuffers[static_cast<size_t>(channel)][static_cast<size_t>(hopFill)] = buffer.getSample(channel, sampleIndex);

        ++hopFill;

        if (hopFill >= hopSize)
        {
            for (auto channel = 0; channel < channelsToUse; ++channel)
            {
                auto& state = channelStates[static_cast<size_t>(channel)];
                std::move(state.analysisFifo.begin() + hopSize,
                          state.analysisFifo.begin() + fftSize,
                          state.analysisFifo.begin());
                std::copy_n(hopBuffers[static_cast<size_t>(channel)].begin(),
                            hopSize,
                            state.analysisFifo.begin() + (fftSize - hopSize));
                state.analysisFilled = juce::jmin(fftSize, state.analysisFilled + hopSize);
            }

            if (channelStates[0].analysisFilled >= fftSize)
                processFrame(channelsToUse, settings, fftIndex, fftSize, hopSize);

            for (auto channel = 0; channel < channelsToUse; ++channel)
                pushOutputChunk(channelStates[static_cast<size_t>(channel)], fftSize, hopSize);

            hopFill = 0;
        }

        for (auto channel = 0; channel < channelsToUse; ++channel)
            buffer.setSample(channel, sampleIndex, dequeueOutputSample(channelStates[static_cast<size_t>(channel)]));
    }
}

void FftModuleProcessor::DynamicProcessor::enqueueOutputSample(ChannelState& state, float sample) noexcept
{
    if (state.readyOutputCount >= maxQueueSize)
        return;

    state.readyOutput[static_cast<size_t>(state.readyOutputWrite)] = sample;
    state.readyOutputWrite = (state.readyOutputWrite + 1) % maxQueueSize;
    ++state.readyOutputCount;
}

float FftModuleProcessor::DynamicProcessor::dequeueOutputSample(ChannelState& state) noexcept
{
    if (state.readyOutputCount <= 0)
        return 0.0f;

    const auto sample = state.readyOutput[static_cast<size_t>(state.readyOutputRead)];
    state.readyOutputRead = (state.readyOutputRead + 1) % maxQueueSize;
    --state.readyOutputCount;
    return sample;
}

void FftModuleProcessor::DynamicProcessor::processFrame(int channelsToUse,
                                                          const CompressorSettings& settings,
                                                          int fftIndex,
                                                          int fftSize,
                                                          int hopSize) noexcept
{
    const auto attackCoefficient = calculateTimeCoefficient(settings.attackMs,
                                                            static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto releaseCoefficient = calculateTimeCoefficient(settings.releaseMs,
                                                             static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto frameDurationMs = 1000.0f * static_cast<float>(hopSize) / static_cast<float>(sampleRate);
    const auto adaptiveAttackCoefficient = calculateTimeCoefficient(settings.adaptiveAttackMs,
                                                                    static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto adaptiveReleaseCoefficient = calculateTimeCoefficient(settings.adaptiveReleaseMs,
                                                                     static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto makeupGain = juce::Decibels::decibelsToGain(settings.makeupDb);
    const auto& window = windowTables[static_cast<size_t>(fftIndex)];
    auto& fft = *ffts[static_cast<size_t>(fftIndex)];
    std::array<double, maxChannels> accumulatedDualMonoDetectorPower {};
    auto accumulatedPhaseCorrelation = 0.0;
    auto accumulatedPhaseWeight = 0.0;
    std::array<float, maxChannels> publishedThreshold {
        settings.phaseMode ? phaseThresholdToCorrelation(settings.phaseThreshold) : settings.leftThresholdDb,
        settings.phaseMode ? phaseThresholdToCorrelation(settings.phaseThreshold) : settings.rightThresholdDb
    };

    for (auto channel = 0; channel < channelsToUse; ++channel)
    {
        const auto channelAdaptiveAmount = settings.phaseMode
            ? settings.phaseAdaptiveAmount
            : (channel == 0 ? settings.leftAdaptiveAmount : settings.rightAdaptiveAmount);
        const auto adaptiveAmount = juce::jlimit(0.0f, 1.0f, channelAdaptiveAmount * 0.01f);

        if (settings.phaseMode)
        {
            const auto manualThreshold = phaseThresholdToCorrelation(settings.phaseThreshold);
            const auto adaptiveThreshold = juce::jlimit(-1.0f,
                                                        1.0f,
                                                        phaseAdaptiveReference[0]
                                                            + settings.adaptiveOffset);
            publishedThreshold[static_cast<size_t>(channel)] = juce::jmap(adaptiveAmount,
                                                                          manualThreshold,
                                                                          adaptiveThreshold);
        }
        else
        {
            const auto manualThresholdDb = channel == 0 ? settings.leftThresholdDb : settings.rightThresholdDb;
            const auto adaptiveThresholdDb = dualMonoAdaptiveReferenceDb[static_cast<size_t>(channel)]
                                           + settings.adaptiveOffset;
            publishedThreshold[static_cast<size_t>(channel)] = juce::jmap(adaptiveAmount,
                                                                          manualThresholdDb,
                                                                          adaptiveThresholdDb);
        }
    }

    for (auto channel = 0; channel < channelsToUse; ++channel)
    {
        auto& state = channelStates[static_cast<size_t>(channel)];

        for (auto sampleIndex = 0; sampleIndex < fftSize; ++sampleIndex)
        {
            const auto windowedSample = state.analysisFifo[static_cast<size_t>(sampleIndex)]
                                      * window[static_cast<size_t>(sampleIndex)];
            state.frequencyData[static_cast<size_t>(sampleIndex)] = { windowedSample, 0.0f };
        }

        fft.perform(state.frequencyData.data(), state.frequencyData.data(), false);
    }

    for (auto bin = 0; bin <= fftSize / 2; ++bin)
    {
        std::array<float, maxChannels> channelMagnitudes {};

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            channelMagnitudes[static_cast<size_t>(channel)]
                = std::abs(channelStates[static_cast<size_t>(channel)].frequencyData[static_cast<size_t>(bin)])
                / static_cast<float>(fftSize);
            accumulatedDualMonoDetectorPower[static_cast<size_t>(channel)]
                += static_cast<double>(channelMagnitudes[static_cast<size_t>(channel)])
                 * static_cast<double>(channelMagnitudes[static_cast<size_t>(channel)]);
        }

        const auto binFrequency = juce::jmax(analyserMinFrequency,
                                             (static_cast<float>(bin) * static_cast<float>(sampleRate))
                                                 / static_cast<float>(fftSize));
        const auto octavesAboveMin = std::log2(binFrequency / analyserMinFrequency);
        std::array<float, maxChannels> channelLevelsDb {};
        std::array<float, maxChannels> smoothedDetectorReductionDb {};

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            const auto levelDb = juce::Decibels::gainToDecibels(channelMagnitudes[static_cast<size_t>(channel)], -120.0f);
            channelLevelsDb[static_cast<size_t>(channel)] = levelDb;

            if (! settings.phaseMode)
            {
                auto& smoothedReduction = dualMonoSmoothedReductionDb[static_cast<size_t>(channel)][static_cast<size_t>(bin)];
                const auto thresholdSlopeDb = settings.slopeDbPerOct * juce::jmax(0.0f, octavesAboveMin);
                const auto effectiveThresholdDb = publishedThreshold[static_cast<size_t>(channel)]
                                                - thresholdSlopeDb;
                const auto desiredReductionDb = calculateReduction(levelDb,
                                                                   effectiveThresholdDb,
                                                                   settings.ratio,
                                                                   settings.kneeDb);
                const auto coefficient = desiredReductionDb > smoothedReduction ? attackCoefficient : releaseCoefficient;
                smoothedReduction = (coefficient * smoothedReduction)
                                  + ((1.0f - coefficient) * desiredReductionDb);
                smoothedDetectorReductionDb[static_cast<size_t>(channel)] = smoothedReduction;
            }
        }

        if (! settings.phaseMode)
        {
            for (auto channel = 0; channel < channelsToUse; ++channel)
            {
                const auto gain = settings.dynamicBypassed
                    ? 1.0f
                    : makeupGain * juce::Decibels::decibelsToGain(-smoothedDetectorReductionDb[static_cast<size_t>(channel)]);
                auto& frequencyData = channelStates[static_cast<size_t>(channel)].frequencyData;
                frequencyData[static_cast<size_t>(bin)] *= gain;

                if (bin > 0 && bin < (fftSize / 2))
                    frequencyData[static_cast<size_t>(fftSize - bin)] *= gain;
            }

            continue;
        }

        if (channelsToUse < 2)
            continue;

        auto& leftFrequencyData = channelStates[0].frequencyData;
        auto& rightFrequencyData = channelStates[1].frequencyData;
        const auto leftBin = leftFrequencyData[static_cast<size_t>(bin)];
        const auto rightBin = rightFrequencyData[static_cast<size_t>(bin)];
        const auto leftAmplitude = std::abs(leftBin);
        const auto rightAmplitude = std::abs(rightBin);
        const auto minimumLevelDb = juce::jmin(channelLevelsDb[0], channelLevelsDb[1]);
        const auto wrapPhase = [] (float phase) noexcept
        {
            return std::remainder(phase, juce::MathConstants<float>::twoPi);
        };
        const auto leftPhase = std::atan2(leftBin.imag(), leftBin.real());
        const auto phaseDifference = wrapPhase(std::atan2(rightBin.imag(), rightBin.real()) - leftPhase);
        const auto absoluteDifference = std::abs(phaseDifference);
        const auto phaseCorrelation = std::cos(absoluteDifference);
        const auto phaseWeight = static_cast<double>(juce::jmin(leftAmplitude, rightAmplitude));
        accumulatedPhaseCorrelation += static_cast<double>(phaseCorrelation) * phaseWeight;
        accumulatedPhaseWeight += phaseWeight;

        for (auto channel = 0; channel < maxChannels; ++channel)
            phaseCorrelationReductions[static_cast<size_t>(channel)][static_cast<size_t>(bin)] = 0.0f;

        if (bin == 0 || bin == fftSize / 2)
            continue;

        const auto correlationKnee = juce::jmap(settings.kneeDb, 0.0f, 24.0f, 0.0f, 2.0f);
        const auto phaseRatio = settings.ratio >= 100.0f
            ? std::numeric_limits<float>::infinity()
            : settings.ratio;
        const auto threshold = juce::jlimit(-1.0f,
                                            1.0f,
                                            publishedThreshold[0]
                                                + (settings.phaseSlopePerOctave
                                                   * juce::jmax(0.0f, octavesAboveMin)));
        const auto desiredCorrelationIncrease = minimumLevelDb >= settings.floorDb
                                                 && threshold > -1.0f
            ? calculateReduction(threshold - phaseCorrelation,
                                 0.0f,
                                 phaseRatio,
                                 correlationKnee)
            : 0.0f;
        const auto targetCorrelation = juce::jlimit(-1.0f,
                                                    1.0f,
                                                    phaseCorrelation + desiredCorrelationIncrease);
        const auto desiredPhaseReduction = juce::jmax(0.0f,
                                                      absoluteDifference - std::acos(targetCorrelation));
        auto& smoothedReduction = phaseSmoothedReductionRadians[0][static_cast<size_t>(bin)];
        const auto coefficient = desiredPhaseReduction > smoothedReduction ? attackCoefficient : releaseCoefficient;
        smoothedReduction = (coefficient * smoothedReduction)
                          + ((1.0f - coefficient) * desiredPhaseReduction);
        phaseSmoothedReductionRadians[1][static_cast<size_t>(bin)] = smoothedReduction;
        const auto appliedPhaseReduction = settings.dynamicBypassed ? 0.0f : smoothedReduction;
        const auto correctedCorrelation = std::cos(juce::jmax(0.0f,
                                                               absoluteDifference
                                                                   - appliedPhaseReduction));
        const auto correlationReduction = juce::jlimit(0.0f,
                                                       2.0f,
                                                       correctedCorrelation - phaseCorrelation);
        phaseCorrelationReductions[0][static_cast<size_t>(bin)] = correlationReduction;
        phaseCorrelationReductions[1][static_cast<size_t>(bin)] = correlationReduction;

        const auto direction = phaseDifference >= 0.0f ? 1.0f : -1.0f;
        const auto rightShare = juce::jmap(settings.phaseImpact, -100.0f, 100.0f, 0.0f, 1.0f);
        const auto leftShare = 1.0f - rightShare;
        const auto processedLeftPhase = leftPhase + (direction * appliedPhaseReduction * leftShare);
        const auto processedRightPhase = std::atan2(rightBin.imag(), rightBin.real())
                                       - (direction * appliedPhaseReduction * rightShare);
        const juce::dsp::Complex<float> processedLeft {
            leftAmplitude * std::cos(processedLeftPhase),
            leftAmplitude * std::sin(processedLeftPhase)
        };
        const juce::dsp::Complex<float> processedRight {
            rightAmplitude * std::cos(processedRightPhase),
            rightAmplitude * std::sin(processedRightPhase)
        };

        leftFrequencyData[static_cast<size_t>(bin)] = processedLeft;
        rightFrequencyData[static_cast<size_t>(bin)] = processedRight;
        leftFrequencyData[static_cast<size_t>(fftSize - bin)] = std::conj(processedLeft);
        rightFrequencyData[static_cast<size_t>(fftSize - bin)] = std::conj(processedRight);
    }

    const auto processedBinCount = juce::jmax(1, (fftSize / 2) + 1);
    const auto updateAdaptiveReference = [adaptiveAttackCoefficient,
                                          adaptiveReleaseCoefficient,
                                          frameDurationMs,
                                          holdMs = settings.adaptiveHoldMs] (float& current,
                                                                            const float desired,
                                                                            float& holdRemainingMs)
    {
        if (desired >= current)
        {
            current = (adaptiveAttackCoefficient * current)
                    + ((1.0f - adaptiveAttackCoefficient) * desired);
            holdRemainingMs = holdMs;
            return;
        }

        if (holdRemainingMs > 0.0f)
        {
            holdRemainingMs = juce::jmax(0.0f, holdRemainingMs - frameDurationMs);
            return;
        }

        current = (adaptiveReleaseCoefficient * current)
                + ((1.0f - adaptiveReleaseCoefficient) * desired);
    };

    for (auto channel = 0; channel < channelsToUse; ++channel)
    {
        const auto dualMonoDetectorRms = std::sqrt(accumulatedDualMonoDetectorPower[static_cast<size_t>(channel)]
                                                   / static_cast<double>(processedBinCount));
        const auto desiredDualMonoAdaptiveReferenceDb = juce::Decibels::gainToDecibels(static_cast<float>(dualMonoDetectorRms),
                                                                                        analyserMinDecibels);
        updateAdaptiveReference(dualMonoAdaptiveReferenceDb[static_cast<size_t>(channel)],
                                desiredDualMonoAdaptiveReferenceDb,
                                dualMonoAdaptiveHoldRemainingMs[static_cast<size_t>(channel)]);
        if (settings.phaseMode && channel == 0 && accumulatedPhaseWeight > 1.0e-12)
        {
            const auto desiredPhaseReference = juce::jlimit(-1.0f,
                                                            1.0f,
                                                            static_cast<float>(accumulatedPhaseCorrelation
                                                                               / accumulatedPhaseWeight));
            updateAdaptiveReference(phaseAdaptiveReference[0],
                                    desiredPhaseReference,
                                    phaseAdaptiveHoldRemainingMs[0]);
            phaseAdaptiveReference[1] = phaseAdaptiveReference[0];
            phaseAdaptiveHoldRemainingMs[1] = phaseAdaptiveHoldRemainingMs[0];
        }

    }

    for (auto channel = 0; channel < channelsToUse; ++channel)
    {
        auto& state = channelStates[static_cast<size_t>(channel)];
        fft.perform(state.frequencyData.data(), state.frequencyData.data(), true);

        for (auto sampleIndex = 0; sampleIndex < fftSize; ++sampleIndex)
        {
            const auto synthesisWeight = window[static_cast<size_t>(sampleIndex)];
            const auto weightedSample = state.frequencyData[static_cast<size_t>(sampleIndex)].real() * synthesisWeight;
            state.outputAccum[static_cast<size_t>(sampleIndex)] += weightedSample;
            state.normalizationAccum[static_cast<size_t>(sampleIndex)] += synthesisWeight * synthesisWeight;
        }
    }

    const auto currentSampleRate = juce::jmax(1.0, sampleRate);
    const auto displayCoefficient = calculateTimeCoefficient(
        settings.reductionDisplayTimeMs,
        static_cast<float>(hopSize) / static_cast<float>(currentSampleRate));
    const auto sourceMaximumHz = juce::jlimit(analyserMinFrequency + 1.0f,
                                              analyserMaxFrequency,
                                              static_cast<float>(currentSampleRate * 0.5));
    const auto publishedIndex = activeReductionScopeBuffer.load(std::memory_order_relaxed);
    const auto writeIndex = 1 - publishedIndex;
    auto& reductionScope = reductionScopeBuffers[static_cast<size_t>(writeIndex)];

    for (std::size_t i = 0; i < analyserScopeSize; ++i)
    {
        const auto proportion = static_cast<float>(i) / static_cast<float>(analyserScopeSize - 1);
        const auto frequency = juce::mapToLog10(proportion, analyserMinFrequency, sourceMaximumHz);
        const auto fractionalBin = juce::jlimit(0.0f,
                                                static_cast<float>(fftSize / 2),
                                                frequency * static_cast<float>(fftSize)
                                                    / static_cast<float>(currentSampleRate));
        const auto lowerBin = juce::jlimit(0, fftSize / 2, static_cast<int>(std::floor(fractionalBin)));
        const auto upperBin = juce::jlimit(0, fftSize / 2, lowerBin + 1);
        const auto interpolation = fractionalBin - static_cast<float>(lowerBin);

        for (auto channel = 0; channel < maxChannels; ++channel)
        {
            const auto& source = settings.phaseMode
                ? phaseCorrelationReductions[static_cast<size_t>(channel)]
                : dualMonoSmoothedReductionDb[static_cast<size_t>(channel)];
            const auto rawReduction = settings.dynamicBypassed
                ? 0.0f
                : juce::jmap(interpolation,
                             source[static_cast<size_t>(lowerBin)],
                             source[static_cast<size_t>(upperBin)]);
            auto& smoothedReduction = smoothedReductionScopes[static_cast<size_t>(channel)][i];
            smoothedReduction = (displayCoefficient * smoothedReduction)
                              + ((1.0f - displayCoefficient) * rawReduction);
            reductionScope[static_cast<size_t>(channel)][i] = smoothedReduction;
        }
    }

    activeReductionScopeBuffer.store(writeIndex, std::memory_order_release);

}

void FftModuleProcessor::DynamicProcessor::pushOutputChunk(ChannelState& state, int fftSize, int hopSize) noexcept
{
    for (auto sampleIndex = 0; sampleIndex < hopSize; ++sampleIndex)
    {
        const auto normalization = state.normalizationAccum[static_cast<size_t>(sampleIndex)];
        const auto outputSample = normalization > 1.0e-6f
                                ? state.outputAccum[static_cast<size_t>(sampleIndex)] / normalization
                                : 0.0f;
        enqueueOutputSample(state, outputSample);
    }

    std::move(state.outputAccum.begin() + hopSize,
              state.outputAccum.begin() + fftSize,
              state.outputAccum.begin());
    std::fill(state.outputAccum.begin() + (fftSize - hopSize),
              state.outputAccum.begin() + fftSize,
              0.0f);

    std::move(state.normalizationAccum.begin() + hopSize,
              state.normalizationAccum.begin() + fftSize,
              state.normalizationAccum.begin());
    std::fill(state.normalizationAccum.begin() + (fftSize - hopSize),
              state.normalizationAccum.begin() + fftSize,
              0.0f);
}

void FftModuleProcessor::DynamicProcessor::reconfigure(int channelsToUse, int fftSize, int hopSize) noexcept
{
    configuredChannels = juce::jlimit(0, maxChannels, channelsToUse);
    currentFftSize = fftSize;
    currentHopSize = hopSize;
    hopFill = 0;
    dualMonoAdaptiveReferenceDb.fill(0.0f);
    phaseAdaptiveReference.fill(-1.0f);
    dualMonoAdaptiveHoldRemainingMs.fill(0.0f);
    phaseAdaptiveHoldRemainingMs.fill(0.0f);
    for (auto& channelReduction : dualMonoSmoothedReductionDb)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);
    for (auto& channelReduction : phaseSmoothedReductionRadians)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);
    for (auto& channelReduction : phaseCorrelationReductions)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);
    activeReductionScopeBuffer.store(0, std::memory_order_relaxed);
    for (auto& reductionScope : reductionScopeBuffers)
        for (auto& channelReductionScope : reductionScope)
            std::fill(channelReductionScope.begin(), channelReductionScope.end(), 0.0f);
    for (auto& channelReductionScope : smoothedReductionScopes)
        std::fill(channelReductionScope.begin(), channelReductionScope.end(), 0.0f);

    for (auto channel = 0; channel < maxChannels; ++channel)
    {
        auto& state = channelStates[static_cast<size_t>(channel)];
        std::fill(state.analysisFifo.begin(), state.analysisFifo.end(), 0.0f);
        std::fill(state.outputAccum.begin(), state.outputAccum.end(), 0.0f);
        std::fill(state.normalizationAccum.begin(), state.normalizationAccum.end(), 0.0f);
        std::fill(state.readyOutput.begin(), state.readyOutput.end(), 0.0f);
        std::fill(hopBuffers[static_cast<size_t>(channel)].begin(),
                  hopBuffers[static_cast<size_t>(channel)].end(),
                  0.0f);

        for (auto& value : state.frequencyData)
            value = {};

        state.readyOutputRead = 0;
        state.readyOutputWrite = 0;
        state.readyOutputCount = 0;
        state.analysisFilled = 0;
    }
}

int FftModuleProcessor::DynamicProcessor::getFftIndexForSize(int fftSize) const noexcept
{
    switch (fftSize)
    {
        case 1024: return 0;
        case 2048: return 1;
        case 4096: return 2;
        case 8192: return 3;
        case 16384: return 4;
        default: return 3;
    }
}

float FftModuleProcessor::calculateReduction(float detectorValue,
                                              float threshold,
                                              float ratio,
                                              float knee) noexcept
{
    const auto safeRatio = juce::jmax(1.0f, ratio);
    const auto safeKnee = juce::jmax(0.0f, knee);
    const auto ratioFactor = 1.0f - (1.0f / safeRatio);
    const auto distanceAboveThreshold = detectorValue - threshold;

    if (safeKnee > 0.0f)
    {
        const auto halfKnee = safeKnee * 0.5f;

        if (distanceAboveThreshold <= -halfKnee)
            return 0.0f;

        if (distanceAboveThreshold >= halfKnee)
            return ratioFactor * juce::jmax(0.0f, distanceAboveThreshold);

        const auto kneePosition = distanceAboveThreshold + halfKnee;
        return ratioFactor * (kneePosition * kneePosition) / (2.0f * safeKnee);
    }

    return ratioFactor * juce::jmax(0.0f, distanceAboveThreshold);
}

float FftModuleProcessor::calculateTimeCoefficient(float timeMs,
                                                     float frameDurationSeconds) noexcept
{
    if (timeMs <= 0.0f)
        return 0.0f;

    const auto timeSeconds = timeMs * 0.001f;
    return std::exp(-frameDurationSeconds / timeSeconds);
}
