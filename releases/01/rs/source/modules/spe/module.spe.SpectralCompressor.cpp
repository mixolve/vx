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

void SpeModuleProcessor::SpectralCompressor::copyReductionScope(std::array<float, analyserScopeSize>& destination) const
{
    const auto activeIndex = activeReductionScopeBuffer.load(std::memory_order_acquire);
    destination = reductionScopeBuffers[static_cast<size_t>(activeIndex)];
}

float SpeModuleProcessor::SpectralCompressor::getPublishedStereoThresholdDb() const noexcept
{
    return publishedStereoThresholdDb.load(std::memory_order_acquire);
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
    const auto adaptiveAttackCoefficient = calculateTimeCoefficient(stereoAdaptiveAttackMs,
                                                                    static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto adaptiveReleaseCoefficient = calculateTimeCoefficient(stereoAdaptiveReleaseMs,
                                                                     static_cast<float>(hopSize) / static_cast<float>(sampleRate));
    const auto adaptiveAmount = juce::jlimit(0.0f, 1.0f, settings.stereoAdaptiveAmount * 0.01f);
    const auto adaptiveThresholdDb = stereoAdaptiveReferenceDb + settings.stereoAdaptiveOffsetDb;
    const auto effectiveBaseStereoThresholdDb = juce::jmap(adaptiveAmount,
                                                           settings.thresholdDb,
                                                           adaptiveThresholdDb);
    const auto makeupGain = juce::Decibels::decibelsToGain(settings.makeupDb);
    const auto& window = windowTables[static_cast<size_t>(fftIndex)];
    auto& fft = *ffts[static_cast<size_t>(fftIndex)];
    auto accumulatedStereoDetectorPower = 0.0;
    std::array<double, maxChannels> accumulatedDualMonoDetectorPower {};

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
        const auto stereoThresholdDb = effectiveBaseStereoThresholdDb
                                     - (settings.slopeDbPerOct * juce::jmax(0.0f, octavesAboveMin));

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            auto& smoothedDualMonoReduction = dualMonoSmoothedReductionDb[static_cast<size_t>(channel)][static_cast<size_t>(bin)];

            if (settings.dualMonoBypass)
            {
                smoothedDualMonoReduction = 0.0f;
                dualMonoReductionDb[static_cast<size_t>(channel)] = 0.0f;
                dualMonoGain[static_cast<size_t>(channel)] = 1.0f;
                continue;
            }

            const auto channelThresholdDb = channel == 0 ? settings.leftThresholdDb : settings.rightThresholdDb;
            const auto channelAdaptiveAmount = channel == 0 ? settings.leftAdaptiveAmount : settings.rightAdaptiveAmount;
            const auto channelAdaptiveOffsetDb = channel == 0 ? settings.leftAdaptiveOffsetDb : settings.rightAdaptiveOffsetDb;
            const auto dualMonoAdaptiveAmount = juce::jlimit(0.0f, 1.0f, channelAdaptiveAmount * 0.01f);
            const auto adaptiveChannelThresholdDb = dualMonoAdaptiveReferenceDb[static_cast<size_t>(channel)]
                                                  + channelAdaptiveOffsetDb;
            const auto effectiveChannelThresholdDb = juce::jmap(dualMonoAdaptiveAmount,
                                                                channelThresholdDb,
                                                                adaptiveChannelThresholdDb);
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

        auto stereoDetectorMagnitude = 0.0f;

        for (auto channel = 0; channel < channelsToUse; ++channel)
            stereoDetectorMagnitude = juce::jmax(stereoDetectorMagnitude,
                                                 channelMagnitudes[static_cast<size_t>(channel)] * dualMonoGain[static_cast<size_t>(channel)]);

        accumulatedStereoDetectorPower += static_cast<double>(stereoDetectorMagnitude) * static_cast<double>(stereoDetectorMagnitude);

        auto& smoothedStereoReduction = stereoSmoothedReductionDb[static_cast<size_t>(bin)];

        if (settings.stereoBypass)
        {
            smoothedStereoReduction = 0.0f;
        }
        else
        {
            const auto stereoLevelDb = juce::Decibels::gainToDecibels(stereoDetectorMagnitude, -120.0f);
            const auto desiredStereoReductionDb = calculateReductionDb(stereoLevelDb,
                                                                       stereoThresholdDb,
                                                                       settings.ratio,
                                                                       settings.kneeDb);
            const auto stereoCoefficient = desiredStereoReductionDb > smoothedStereoReduction ? attackCoefficient : releaseCoefficient;
            smoothedStereoReduction = (stereoCoefficient * smoothedStereoReduction)
                                    + ((1.0f - stereoCoefficient) * desiredStereoReductionDb);
        }

        for (auto channel = 0; channel < channelsToUse; ++channel)
        {
            const auto totalReductionDb = dualMonoReductionDb[static_cast<size_t>(channel)] + smoothedStereoReduction;
            combinedReductionDb[static_cast<size_t>(bin)] = channel == 0
                ? totalReductionDb
                : juce::jmax(combinedReductionDb[static_cast<size_t>(bin)], totalReductionDb);
            const auto gain = makeupGain
                            * dualMonoGain[static_cast<size_t>(channel)]
                            * juce::Decibels::decibelsToGain(-smoothedStereoReduction);
            auto& frequencyData = channelStates[static_cast<size_t>(channel)].frequencyData;
            frequencyData[static_cast<size_t>(bin)] *= gain;

            if (bin > 0 && bin < (fftSize / 2))
                frequencyData[static_cast<size_t>(fftSize - bin)] *= gain;
        }
    }

    const auto processedBinCount = juce::jmax(1, (fftSize / 2) + 1);
    const auto stereoDetectorRms = std::sqrt(accumulatedStereoDetectorPower / static_cast<double>(processedBinCount));
    const auto desiredAdaptiveReferenceDb = juce::Decibels::gainToDecibels(static_cast<float>(stereoDetectorRms), analyserMinDecibels);
    const auto adaptiveCoefficient = desiredAdaptiveReferenceDb > stereoAdaptiveReferenceDb
        ? adaptiveAttackCoefficient
        : adaptiveReleaseCoefficient;
    stereoAdaptiveReferenceDb = (adaptiveCoefficient * stereoAdaptiveReferenceDb)
                              + ((1.0f - adaptiveCoefficient) * desiredAdaptiveReferenceDb);

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
    }

    publishedStereoThresholdDb.store(effectiveBaseStereoThresholdDb, std::memory_order_release);

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
        reductionScope[i] = juce::jmap(interpolation,
                                       combinedReductionDb[static_cast<size_t>(lowerBin)],
                                       combinedReductionDb[static_cast<size_t>(upperBin)]);
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
    stereoAdaptiveReferenceDb = 0.0f;
    dualMonoAdaptiveReferenceDb.fill(0.0f);
    std::fill(stereoSmoothedReductionDb.begin(), stereoSmoothedReductionDb.end(), 0.0f);
    std::fill(combinedReductionDb.begin(), combinedReductionDb.end(), 0.0f);
    for (auto& channelReduction : dualMonoSmoothedReductionDb)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);
    activeReductionScopeBuffer.store(0, std::memory_order_relaxed);
    publishedStereoThresholdDb.store(12.0f, std::memory_order_relaxed);

    for (auto& reductionScope : reductionScopeBuffers)
        std::fill(reductionScope.begin(), reductionScope.end(), 0.0f);

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
