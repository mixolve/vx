#include "module.spe.SpeProcessor.h"
#include "module.spe.ProcessorConstants.h"

#include <algorithm>
#include <cmath>

SpeModuleProcessor::SpectralCompressor::SpectralCompressor()
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

void SpeModuleProcessor::SpectralCompressor::prepare(double newSampleRate, int numChannels)
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    configuredChannels = juce::jlimit(0, maxChannels, numChannels);
    reconfigure(configuredChannels, 0, 0);
}

void SpeModuleProcessor::SpectralCompressor::copyReductionScope(std::array<float, analyserScopeSize>& leftDestination,
                                                                std::array<float, analyserScopeSize>& rightDestination) const
{
    const auto activeIndex = activeReductionScopeBuffer.load(std::memory_order_acquire);
    leftDestination = reductionScopeBuffers[static_cast<size_t>(activeIndex)][0];
    rightDestination = reductionScopeBuffers[static_cast<size_t>(activeIndex)][1];
}

float SpeModuleProcessor::SpectralCompressor::getPublishedDualMonoThresholdDb(const int channel) const noexcept
{
    const auto channelIndex = juce::jlimit(0, maxChannels - 1, channel);
    return publishedDualMonoThresholdDb[static_cast<size_t>(channelIndex)].load(std::memory_order_acquire);
}

void SpeModuleProcessor::SpectralCompressor::processBuffer(juce::AudioBuffer<float>& buffer,
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

void SpeModuleProcessor::SpectralCompressor::enqueueOutputSample(ChannelState& state, float sample) noexcept
{
    if (state.readyOutputCount >= maxQueueSize)
        return;

    state.readyOutput[static_cast<size_t>(state.readyOutputWrite)] = sample;
    state.readyOutputWrite = (state.readyOutputWrite + 1) % maxQueueSize;
    ++state.readyOutputCount;
}

float SpeModuleProcessor::SpectralCompressor::dequeueOutputSample(ChannelState& state) noexcept
{
    if (state.readyOutputCount <= 0)
        return 0.0f;

    const auto sample = state.readyOutput[static_cast<size_t>(state.readyOutputRead)];
    state.readyOutputRead = (state.readyOutputRead + 1) % maxQueueSize;
    --state.readyOutputCount;
    return sample;
}

void SpeModuleProcessor::SpectralCompressor::processFrame(int channelsToUse,
                                                          const CompressorSettings& settings,
                                                          int fftIndex,
                                                          int fftSize,
                                                          int hopSize) noexcept
{
    const auto attackCoefficient = calculateTimeCoefficient(settings.attackMs,
                                                            static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto releaseCoefficient = calculateTimeCoefficient(settings.releaseMs,
                                                             static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto adaptiveAttackCoefficient = calculateTimeCoefficient(adaptiveAttackMs,
                                                                    static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto adaptiveReleaseCoefficient = calculateTimeCoefficient(adaptiveReleaseMs,
                                                                     static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto makeupGain = juce::Decibels::decibelsToGain(settings.makeupDb);
    const auto& window = windowTables[static_cast<size_t>(fftIndex)];
    auto& fft = *ffts[static_cast<size_t>(fftIndex)];
    std::array<double, maxChannels> accumulatedDualMonoDetectorPower {};
    std::array<float, maxChannels> publishedThresholdDb { settings.leftThresholdDb, settings.rightThresholdDb };

    for (auto channel = 0; channel < channelsToUse; ++channel)
    {
        const auto channelThresholdDb = channel == 0 ? settings.leftThresholdDb : settings.rightThresholdDb;
        const auto channelAdaptiveAmount = channel == 0 ? settings.leftAdaptiveAmount : settings.rightAdaptiveAmount;
        const auto channelAdaptiveOffsetDb = channel == 0 ? settings.leftAdaptiveOffsetDb : settings.rightAdaptiveOffsetDb;
        const auto dualMonoAdaptiveAmount = juce::jlimit(0.0f, 1.0f, channelAdaptiveAmount * 0.01f);
        const auto adaptiveChannelThresholdDb = dualMonoAdaptiveReferenceDb[static_cast<size_t>(channel)]
                                              + channelAdaptiveOffsetDb;
        publishedThresholdDb[static_cast<size_t>(channel)] = juce::jmap(dualMonoAdaptiveAmount,
                                                                        channelThresholdDb,
                                                                        adaptiveChannelThresholdDb);
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
        std::array<float, maxChannels> dualMonoReductionDb {};
        std::array<float, maxChannels> dualMonoGain {};

        for (auto channel = 0; channel < channelsToUse; ++channel)
            channelMagnitudes[static_cast<size_t>(channel)]
                = std::abs(channelStates[static_cast<size_t>(channel)].frequencyData[static_cast<size_t>(bin)])
                / static_cast<float>(fftSize);

        const auto binFrequency = juce::jmax(analyserMinFrequency,
                                             (static_cast<float>(bin) * static_cast<float>(sampleRate))
                                                 / static_cast<float>(fftSize));
        const auto octavesAboveMin = std::log2(binFrequency / analyserMinFrequency);

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            auto& smoothedDualMonoReduction = dualMonoSmoothedReductionDb[static_cast<size_t>(channel)][static_cast<size_t>(bin)];

            const auto effectiveChannelThresholdDb = publishedThresholdDb[static_cast<size_t>(channel)]
                                                 - (settings.slopeDbPerOct * juce::jmax(0.0f, octavesAboveMin));
            const auto channelLevelDb = juce::Decibels::gainToDecibels(channelMagnitudes[static_cast<size_t>(channel)], -120.0f);
            const auto desiredDualMonoReductionDb = calculateReductionDb(channelLevelDb,
                                                                         effectiveChannelThresholdDb,
                                                                         settings.ratio,
                                                                         settings.kneeDb);
            const auto dualMonoCoefficient = desiredDualMonoReductionDb > smoothedDualMonoReduction ? attackCoefficient : releaseCoefficient;
            smoothedDualMonoReduction = (dualMonoCoefficient * smoothedDualMonoReduction)
                                      + ((1.0f - dualMonoCoefficient) * desiredDualMonoReductionDb);
            dualMonoReductionDb[static_cast<size_t>(channel)] = smoothedDualMonoReduction;
            dualMonoGain[static_cast<size_t>(channel)] = juce::Decibels::decibelsToGain(-smoothedDualMonoReduction);
            accumulatedDualMonoDetectorPower[static_cast<size_t>(channel)] += static_cast<double>(channelMagnitudes[static_cast<size_t>(channel)])
                                                                            * static_cast<double>(channelMagnitudes[static_cast<size_t>(channel)]);
        }

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            const auto gain = makeupGain * dualMonoGain[static_cast<size_t>(channel)];
            auto& frequencyData = channelStates[static_cast<size_t>(channel)].frequencyData;
            frequencyData[static_cast<size_t>(bin)] *= gain;

            if (bin > 0 && bin < (fftSize / 2))
                frequencyData[static_cast<size_t>(fftSize - bin)] *= gain;
        }

        if (channelsToUse >= 2)
        {
            static constexpr auto eps = 1.0e-12f;
            auto& leftFrequencyData = channelStates[0].frequencyData;
            auto& rightFrequencyData = channelStates[1].frequencyData;
            auto leftBin = leftFrequencyData[static_cast<size_t>(bin)];
            auto rightBin = rightFrequencyData[static_cast<size_t>(bin)];
            const auto wrapPhase = [] (float phase) noexcept
            {
                while (phase > juce::MathConstants<float>::pi)
                    phase -= juce::MathConstants<float>::twoPi;

                while (phase < -juce::MathConstants<float>::pi)
                    phase += juce::MathConstants<float>::twoPi;

                return phase;
            };

            const auto phaseOf = [] (const juce::dsp::Complex<float> source) noexcept
            {
                return std::atan2(source.imag(), source.real());
            };

            const auto polarBin = [] (const float amplitude,
                                      const float phase) noexcept -> juce::dsp::Complex<float>
            {
                const auto safeAmplitude = juce::jmax(0.0f, amplitude);
                return { safeAmplitude * std::cos(phase), safeAmplitude * std::sin(phase) };
            };

            const auto movePhaseValue = [wrapPhase] (const float sourcePhase,
                                                     const float targetPhase,
                                                     const float amount) noexcept
            {
                const auto delta = wrapPhase(targetPhase - sourcePhase);
                return sourcePhase + (delta * amount);
            };

            const auto moveAmplitudeValue = [] (const float sourceAmplitude,
                                                const float targetAmplitude,
                                                const float amount) noexcept
            {
                return juce::jmax(0.0f, sourceAmplitude + ((targetAmplitude - sourceAmplitude) * amount));
            };

            auto leftAmplitude = std::abs(leftBin);
            auto rightAmplitude = std::abs(rightBin);
            auto leftPhase = phaseOf(leftBin);
            auto rightPhase = phaseOf(rightBin);

            const auto slopeForChoice = [] (const int choiceIndex) noexcept
            {
                switch (juce::jlimit(0, 5, choiceIndex))
                {
                    case 0: return 6.0f;
                    case 1: return 12.0f;
                    case 2: return 24.0f;
                    case 3: return 48.0f;
                    case 4: return 96.0f;
                    case 5: return 96.1f;
                    default: return 12.0f;
                }
            };

            const auto filterShapeFor = [binFrequency, slopeForChoice] (const CompressorSettings::PhaseFilter& filter) noexcept
            {
                const auto safeFrequency = juce::jlimit(analyserMinFrequency, analyserMaxFrequency, filter.frequency);
                const auto safeBandwidth = juce::jlimit(0.05f, 5.0f, filter.bandwidth);
                const auto octaveOffset = std::log2(juce::jmax(analyserMinFrequency, binFrequency) / safeFrequency);
                const auto fixedSteepness = juce::jmax(0.5f, slopeForChoice(filter.slope) / 6.0f);

                switch (juce::jlimit(0, 4, filter.type))
                {
                    case 0: return 1.0f / (1.0f + std::exp(octaveOffset * fixedSteepness));
                    case 1:
                    {
                        const auto orderScale = juce::jlimit(0.125f, 2.0f, 12.0f / juce::jmax(6.0f, slopeForChoice(filter.slope)));
                        const auto sigma = juce::jmax(0.025f, safeBandwidth * orderScale);
                        return std::exp(-0.5f * (octaveOffset * octaveOffset) / (sigma * sigma));
                    }
                    case 2:
                    {
                        const auto lowOctaveOffset = std::log2(analyserMinFrequency / safeFrequency);
                        const auto highOctaveOffset = std::log2(analyserMaxFrequency / safeFrequency);
                        const auto octaveRange = juce::jmax(1.0e-6f, highOctaveOffset - lowOctaveOffset);
                        return juce::jlimit(0.0f, 1.0f, (octaveOffset - lowOctaveOffset) / octaveRange);
                    }
                    case 3: return 1.0f / (1.0f + std::exp(-octaveOffset * fixedSteepness));
                    case 4: return 1.0f;
                    default: return 0.0f;
                }
            };

            for (auto phaseFilterIndex = 0; phaseFilterIndex < settings.phaseFilterCount; ++phaseFilterIndex)
            {
                const auto& phaseFilter = settings.phaseFilters[static_cast<size_t>(phaseFilterIndex)];
                if (phaseFilter.bypassed)
                    continue;

                const auto phaseAmount = juce::jlimit(-1.0f, 1.0f, phaseFilter.impactPercent * 0.01f);

                if (std::abs(phaseAmount) <= eps)
                    continue;

                const auto phaseImpact = juce::jlimit(-1.0f, 1.0f, phaseAmount * filterShapeFor(phaseFilter));

                if (std::abs(phaseImpact) <= eps)
                    continue;

                const auto phasePlace = juce::jlimit(0, 2, phaseFilter.place);
                const auto absoluteImpact = std::abs(phaseImpact);

                if (phasePlace == 1)
                {
                    if (leftAmplitude > eps)
                        leftPhase = movePhaseValue(leftPhase,
                                                   rightPhase + (phaseImpact >= 0.0f ? 0.0f : juce::MathConstants<float>::pi),
                                                   absoluteImpact);
                }
                else if (phasePlace == 2)
                {
                    const auto originalLeftPhase = leftPhase;
                    const auto originalRightPhase = rightPhase;

                    if (phaseImpact >= 0.0f)
                    {
                        const auto targetPhase = originalLeftPhase + (wrapPhase(originalRightPhase - originalLeftPhase) * 0.5f);
                        if (leftAmplitude > eps)
                            leftPhase = movePhaseValue(originalLeftPhase, targetPhase, absoluteImpact);
                        if (rightAmplitude > eps)
                            rightPhase = movePhaseValue(originalRightPhase, targetPhase, absoluteImpact);
                    }
                    else
                    {
                        const auto targetLeftPhase = originalLeftPhase
                                                   + (wrapPhase((originalRightPhase - juce::MathConstants<float>::pi) - originalLeftPhase) * 0.5f);
                        if (leftAmplitude > eps)
                            leftPhase = movePhaseValue(originalLeftPhase, targetLeftPhase, absoluteImpact);
                        if (rightAmplitude > eps)
                            rightPhase = movePhaseValue(originalRightPhase,
                                                        targetLeftPhase + juce::MathConstants<float>::pi,
                                                        absoluteImpact);
                    }
                }
                else
                {
                    if (rightAmplitude > eps)
                        rightPhase = movePhaseValue(rightPhase,
                                                    leftPhase + (phaseImpact >= 0.0f ? 0.0f : juce::MathConstants<float>::pi),
                                                    absoluteImpact);
                }
            }

            for (auto amplitudeFilterIndex = 0; amplitudeFilterIndex < settings.amplitudeFilterCount; ++amplitudeFilterIndex)
            {
                const auto& amplitudeFilter = settings.amplitudeFilters[static_cast<size_t>(amplitudeFilterIndex)];
                if (amplitudeFilter.bypassed)
                    continue;

                const auto amplitudeAmount = juce::jlimit(-1.0f, 1.0f, amplitudeFilter.impactPercent * 0.01f);

                if (std::abs(amplitudeAmount) <= eps)
                    continue;

                const auto amplitudeImpact = juce::jlimit(-1.0f, 1.0f, amplitudeAmount * filterShapeFor(amplitudeFilter));

                if (std::abs(amplitudeImpact) <= eps)
                    continue;

                const auto amplitudePlace = juce::jlimit(0, 2, amplitudeFilter.place);
                const auto absoluteImpact = std::abs(amplitudeImpact);

                if (amplitudePlace == 1)
                {
                    const auto targetAmplitude = amplitudeImpact >= 0.0f
                        ? rightAmplitude
                        : leftAmplitude + (leftAmplitude - rightAmplitude);
                    leftAmplitude = moveAmplitudeValue(leftAmplitude, targetAmplitude, absoluteImpact);
                }
                else if (amplitudePlace == 2)
                {
                    const auto averageAmplitude = (leftAmplitude + rightAmplitude) * 0.5f;

                    if (amplitudeImpact >= 0.0f)
                    {
                        leftAmplitude = moveAmplitudeValue(leftAmplitude, averageAmplitude, absoluteImpact);
                        rightAmplitude = moveAmplitudeValue(rightAmplitude, averageAmplitude, absoluteImpact);
                    }
                    else
                    {
                        leftAmplitude = moveAmplitudeValue(leftAmplitude, leftAmplitude + (leftAmplitude - averageAmplitude), absoluteImpact);
                        rightAmplitude = moveAmplitudeValue(rightAmplitude, rightAmplitude + (rightAmplitude - averageAmplitude), absoluteImpact);
                    }
                }
                else
                {
                    const auto targetAmplitude = amplitudeImpact >= 0.0f
                        ? leftAmplitude
                        : rightAmplitude + (rightAmplitude - leftAmplitude);
                    rightAmplitude = moveAmplitudeValue(rightAmplitude, targetAmplitude, absoluteImpact);
                }
            }

            leftBin = polarBin(leftAmplitude, leftPhase);
            rightBin = polarBin(rightAmplitude, rightPhase);

            leftFrequencyData[static_cast<size_t>(bin)] = leftBin;
            rightFrequencyData[static_cast<size_t>(bin)] = rightBin;

            if (bin > 0 && bin < (fftSize / 2))
            {
                leftFrequencyData[static_cast<size_t>(fftSize - bin)] = std::conj(leftBin);
                rightFrequencyData[static_cast<size_t>(fftSize - bin)] = std::conj(rightBin);
            }
        }
    }

    const auto processedBinCount = juce::jmax(1, (fftSize / 2) + 1);

    for (auto channel = 0; channel < channelsToUse; ++channel)
    {
        const auto dualMonoDetectorRms = std::sqrt(accumulatedDualMonoDetectorPower[static_cast<size_t>(channel)]
                                                   / static_cast<double>(processedBinCount));
        const auto desiredDualMonoAdaptiveReferenceDb = juce::Decibels::gainToDecibels(static_cast<float>(dualMonoDetectorRms),
                                                                                        analyserMinDecibels);
        const auto dualMonoAdaptiveCoefficient = desiredDualMonoAdaptiveReferenceDb > dualMonoAdaptiveReferenceDb[static_cast<size_t>(channel)]
            ? adaptiveAttackCoefficient
            : adaptiveReleaseCoefficient;
        dualMonoAdaptiveReferenceDb[static_cast<size_t>(channel)]
            = (dualMonoAdaptiveCoefficient * dualMonoAdaptiveReferenceDb[static_cast<size_t>(channel)])
            + ((1.0f - dualMonoAdaptiveCoefficient) * desiredDualMonoAdaptiveReferenceDb);
        publishedDualMonoThresholdDb[static_cast<size_t>(channel)].store(publishedThresholdDb[static_cast<size_t>(channel)],
                                          std::memory_order_release);
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
            reductionScope[static_cast<size_t>(channel)][i] = juce::jmap(interpolation,
                                                                         dualMonoSmoothedReductionDb[static_cast<size_t>(channel)][static_cast<size_t>(lowerBin)],
                                                                         dualMonoSmoothedReductionDb[static_cast<size_t>(channel)][static_cast<size_t>(upperBin)]);
        }
    }

    activeReductionScopeBuffer.store(writeIndex, std::memory_order_release);
}

void SpeModuleProcessor::SpectralCompressor::pushOutputChunk(ChannelState& state, int fftSize, int hopSize) noexcept
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

void SpeModuleProcessor::SpectralCompressor::reconfigure(int channelsToUse, int fftSize, int hopSize) noexcept
{
    configuredChannels = juce::jlimit(0, maxChannels, channelsToUse);
    currentFftSize = fftSize;
    currentHopSize = hopSize;
    hopFill = 0;
    dualMonoAdaptiveReferenceDb.fill(0.0f);
    for (auto& channelReduction : dualMonoSmoothedReductionDb)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);
    activeReductionScopeBuffer.store(0, std::memory_order_relaxed);
    for (auto& publishedThreshold : publishedDualMonoThresholdDb)
        publishedThreshold.store(0.0f, std::memory_order_relaxed);

    for (auto& reductionScope : reductionScopeBuffers)
        for (auto& channelReductionScope : reductionScope)
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

int SpeModuleProcessor::SpectralCompressor::getFftIndexForSize(int fftSize) const noexcept
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

float SpeModuleProcessor::SpectralCompressor::calculateReductionDb(float levelDb,
                                                                   float thresholdDb,
                                                                   float ratio,
                                                                   float kneeDb) noexcept
{
    const auto safeRatio = juce::jmax(1.0f, ratio);
    const auto safeKnee = juce::jmax(0.0f, kneeDb);
    const auto ratioFactor = 1.0f - (1.0f / safeRatio);
    const auto deltaDb = levelDb - thresholdDb;

    if (safeKnee > 0.0f)
    {
        const auto halfKnee = safeKnee * 0.5f;

        if (deltaDb <= -halfKnee)
            return 0.0f;

        if (deltaDb >= halfKnee)
            return ratioFactor * juce::jmax(0.0f, deltaDb);

        const auto kneePosition = deltaDb + halfKnee;
        return ratioFactor * (kneePosition * kneePosition) / (2.0f * safeKnee);
    }

    return ratioFactor * juce::jmax(0.0f, deltaDb);
}

float SpeModuleProcessor::SpectralCompressor::calculateTimeCoefficient(float timeMs,
                                                                       float frameDurationSeconds) noexcept
{
    if (timeMs <= 0.0f)
        return 0.0f;

    const auto timeSeconds = timeMs * 0.001f;
    return std::exp(-frameDurationSeconds / timeSeconds);
}
