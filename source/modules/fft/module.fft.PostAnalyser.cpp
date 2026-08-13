#include "module.fft.FftProcessor.h"
#include "module.fft.ProcessorConstants.h"

#include <algorithm>
#include <cmath>

FftModuleProcessor::PostAnalyser::PostAnalyser()
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

void FftModuleProcessor::PostAnalyser::prepare(double newSampleRate)
{
    sampleRate.store(newSampleRate, std::memory_order_relaxed);
    activeMode = -1;
    resetHistoryAndSmoothing();
}

void FftModuleProcessor::PostAnalyser::resetHistoryAndSmoothing() noexcept
{
    historyWriteIndex = 0;
    availableSamples = 0;
    samplesSinceLastTransform = 0;
    activeScopeBuffer.store(0, std::memory_order_relaxed);
    phaseAnalysisReady = false;

    for (auto& channelHistory : sampleHistory)
        std::fill(channelHistory.begin(), channelHistory.end(), 0.0f);

    for (auto& channelData : fftData)
        std::fill(channelData.begin(), channelData.end(), juce::dsp::Complex<float> {});

    std::fill(smoothedMagnitudes.begin(), smoothedMagnitudes.end(), 0.0f);
    std::fill(smoothedPhaseCorrelations.begin(), smoothedPhaseCorrelations.end(), 0.0f);

    for (auto& channelReduction : latestPhaseReductions)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);

    for (auto& channelReduction : smoothedPhaseReductions)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);

    for (auto& scopeBuffer : scopeBuffers)
        std::fill(scopeBuffer.begin(), scopeBuffer.end(), analyserScopeStorageMinDecibels);

    for (auto& detectorScope : phaseDetectorScopeBuffers)
        std::fill(detectorScope.begin(), detectorScope.end(), 0.0f);

    for (auto& reductionScope : phaseReductionScopeBuffers)
        for (auto& channelReduction : reductionScope)
            std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);

}

void FftModuleProcessor::PostAnalyser::selectMode(const bool phaseMode) noexcept
{
    const auto requestedMode = phaseMode ? 1 : 0;

    if (activeMode == requestedMode)
        return;

    activeMode = requestedMode;
    historyWriteIndex = 0;
    availableSamples = 0;
    samplesSinceLastTransform = 0;
    phaseAnalysisReady = false;
    std::fill(smoothedMagnitudes.begin(), smoothedMagnitudes.end(), 0.0f);
    std::fill(smoothedPhaseCorrelations.begin(), smoothedPhaseCorrelations.end(), 0.0f);

    for (auto& channelReduction : latestPhaseReductions)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);

    for (auto& channelReduction : smoothedPhaseReductions)
        std::fill(channelReduction.begin(), channelReduction.end(), 0.0f);

}

void FftModuleProcessor::PostAnalyser::pushBuffer(const juce::AudioBuffer<float>& buffer,
                                                    int numInputChannels,
                                                    int fftSize,
                                                    int overlapFactor,
                                                    float averagingTimeMs)
{
    selectMode(false);
    const auto channelsToUse = juce::jlimit(0,
                                            maxChannels,
                                            juce::jmin(numInputChannels, buffer.getNumChannels()));

    if (channelsToUse <= 0)
        return;

    for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        const auto leftSample = buffer.getSample(0, sampleIndex);
        sampleHistory[0][static_cast<size_t>(historyWriteIndex)] = leftSample;
        sampleHistory[1][static_cast<size_t>(historyWriteIndex)] = channelsToUse > 1
            ? buffer.getSample(1, sampleIndex)
            : leftSample;

        historyWriteIndex = (historyWriteIndex + 1) % maxFftSize;
        availableSamples = juce::jmin(availableSamples + 1, maxFftSize);
        ++samplesSinceLastTransform;

        const auto hopSize = juce::jmax(1, fftSize / juce::jmax(1, overlapFactor));

        if (availableSamples >= fftSize && samplesSinceLastTransform >= hopSize)
        {
            generateAnalysis(fftSize, overlapFactor, averagingTimeMs, channelsToUse);
            samplesSinceLastTransform = 0;
        }
    }
}

void FftModuleProcessor::PostAnalyser::pushPhaseBuffer(const juce::AudioBuffer<float>& buffer,
                                                        int numInputChannels,
                                                        int fftSize,
                                                        int overlapFactor,
                                                        float averagingTimeMs,
                                                        const std::array<float, analyserScopeSize>& leftReduction,
                                                        const std::array<float, analyserScopeSize>& rightReduction)
{
    selectMode(true);
    latestPhaseReductions[0] = leftReduction;
    latestPhaseReductions[1] = rightReduction;
    const auto channelsToUse = juce::jlimit(0,
                                            maxChannels,
                                            juce::jmin(numInputChannels, buffer.getNumChannels()));

    if (channelsToUse < 2)
        return;

    for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        sampleHistory[0][static_cast<size_t>(historyWriteIndex)] = buffer.getSample(0, sampleIndex);
        sampleHistory[1][static_cast<size_t>(historyWriteIndex)] = buffer.getSample(1, sampleIndex);
        historyWriteIndex = (historyWriteIndex + 1) % maxFftSize;
        availableSamples = juce::jmin(availableSamples + 1, maxFftSize);
        ++samplesSinceLastTransform;

        const auto hopSize = juce::jmax(1, fftSize / juce::jmax(1, overlapFactor));

        if (availableSamples >= fftSize && samplesSinceLastTransform >= hopSize)
        {
            generatePhaseAnalysis(fftSize, overlapFactor, averagingTimeMs);
            samplesSinceLastTransform = 0;
        }
    }
}

void FftModuleProcessor::PostAnalyser::copyScope(std::array<float, analyserScopeSize>& destination,
                                                  double& currentSampleRate) const
{
    const auto activeIndex = activeScopeBuffer.load(std::memory_order_acquire);
    destination = scopeBuffers[static_cast<size_t>(activeIndex)];
    currentSampleRate = sampleRate.load(std::memory_order_relaxed);
}

void FftModuleProcessor::PostAnalyser::copyPhaseAnalysisScopes(
    std::array<float, analyserScopeSize>& detectorDestination,
    std::array<float, analyserScopeSize>& leftReductionDestination,
    std::array<float, analyserScopeSize>& rightReductionDestination) const
{
    const auto activeIndex = activeScopeBuffer.load(std::memory_order_acquire);
    const auto bufferIndex = static_cast<size_t>(activeIndex);
    detectorDestination = phaseDetectorScopeBuffers[bufferIndex];
    leftReductionDestination = phaseReductionScopeBuffers[bufferIndex][0];
    rightReductionDestination = phaseReductionScopeBuffers[bufferIndex][1];
}

void FftModuleProcessor::PostAnalyser::generateAnalysis(int fftSize,
                                                         int overlapFactor,
                                                         float averagingTimeMs,
                                                         int channelsToUse) noexcept
{
    const auto fftIndex = getFftIndexForSize(fftSize);
    const auto& window = windowTables[static_cast<size_t>(fftIndex)];
    auto& fft = *ffts[static_cast<size_t>(fftIndex)];
    const auto channelNormalisation = 1.0f / static_cast<float>(juce::jmax(1, channelsToUse));

    for (auto i = 0; i < fftSize; ++i)
    {
        const auto historyIndex = (historyWriteIndex - fftSize + i + maxFftSize) % maxFftSize;
        const auto sourceSample = (sampleHistory[0][static_cast<size_t>(historyIndex)]
                                + (channelsToUse > 1 ? sampleHistory[1][static_cast<size_t>(historyIndex)] : 0.0f))
                                * channelNormalisation;
        fftData[0][static_cast<size_t>(i)] = { sourceSample * window[static_cast<size_t>(i)], 0.0f };
    }

    fft.perform(fftData[0].data(), fftData[0].data(), false);

    const auto currentSampleRate = juce::jmax(1.0, sampleRate.load(std::memory_order_relaxed));
    const auto nyquist = static_cast<float>(currentSampleRate * 0.5);
    const auto maxFrequency = juce::jlimit(analyserMinFrequency + 1.0f, analyserMaxFrequency, nyquist);
    const auto hopSize = juce::jmax(1, fftSize / juce::jmax(1, overlapFactor));
    const auto frameIntervalSeconds = static_cast<float>(hopSize) / static_cast<float>(currentSampleRate);
    const auto smoothingTimeSeconds = averagingTimeMs * 0.001f;
    const auto smoothingCoefficient = smoothingTimeSeconds > 0.0f
        ? std::exp(-frameIntervalSeconds / smoothingTimeSeconds)
        : 0.0f;
    const auto publishedIndex = activeScopeBuffer.load(std::memory_order_relaxed);
    const auto writeIndex = 1 - publishedIndex;
    auto& scopeBuffer = scopeBuffers[static_cast<size_t>(writeIndex)];

    for (std::size_t i = 0; i < analyserScopeSize; ++i)
    {
        const auto proportion = static_cast<float>(i) / static_cast<float>(analyserScopeSize - 1);
        const auto frequency = juce::mapToLog10(proportion, analyserMinFrequency, maxFrequency);
        const auto fractionalBin = juce::jlimit(0.0f,
                                                static_cast<float>(fftSize / 2),
                                                frequency * static_cast<float>(fftSize)
                                                    / static_cast<float>(currentSampleRate));
        const auto lowerBin = juce::jlimit(0, fftSize / 2, static_cast<int>(std::floor(fractionalBin)));
        const auto upperBin = juce::jlimit(0, fftSize / 2, lowerBin + 1);
        const auto interpolation = fractionalBin - static_cast<float>(lowerBin);
        const auto interpolatedBin = fftData[0][static_cast<size_t>(lowerBin)]
                                   + ((fftData[0][static_cast<size_t>(upperBin)]
                                       - fftData[0][static_cast<size_t>(lowerBin)]) * interpolation);
        const auto rawMagnitude = std::abs(interpolatedBin) / static_cast<float>(fftSize);
        auto& smoothedMagnitude = smoothedMagnitudes[i];
        smoothedMagnitude = smoothingCoefficient > 0.0f
            ? (smoothingCoefficient * smoothedMagnitude)
                + ((1.0f - smoothingCoefficient) * rawMagnitude)
            : rawMagnitude;
        scopeBuffer[i] = juce::Decibels::gainToDecibels(juce::jmax(smoothedMagnitude, 1.0e-10f),
                                                        analyserScopeStorageMinDecibels);
    }

    activeScopeBuffer.store(writeIndex, std::memory_order_release);
}

void FftModuleProcessor::PostAnalyser::generatePhaseAnalysis(int fftSize,
                                                              int overlapFactor,
                                                              float averagingTimeMs) noexcept
{
    const auto fftIndex = getFftIndexForSize(fftSize);
    const auto& window = windowTables[static_cast<size_t>(fftIndex)];
    auto& fft = *ffts[static_cast<size_t>(fftIndex)];

    for (auto channel = 0; channel < maxChannels; ++channel)
    {
        for (auto i = 0; i < fftSize; ++i)
        {
            const auto historyIndex = (historyWriteIndex - fftSize + i + maxFftSize) % maxFftSize;
            const auto sample = sampleHistory[static_cast<size_t>(channel)][static_cast<size_t>(historyIndex)];
            fftData[static_cast<size_t>(channel)][static_cast<size_t>(i)] = {
                sample * window[static_cast<size_t>(i)], 0.0f
            };
        }

        fft.perform(fftData[static_cast<size_t>(channel)].data(),
                    fftData[static_cast<size_t>(channel)].data(),
                    false);
    }

    const auto currentSampleRate = juce::jmax(1.0, sampleRate.load(std::memory_order_relaxed));
    const auto nyquist = static_cast<float>(currentSampleRate * 0.5);
    const auto maxFrequency = juce::jlimit(analyserMinFrequency + 1.0f, analyserMaxFrequency, nyquist);
    const auto hopSize = juce::jmax(1, fftSize / juce::jmax(1, overlapFactor));
    const auto frameIntervalSeconds = static_cast<float>(hopSize) / static_cast<float>(currentSampleRate);
    const auto displaySmoothingCoefficient = calculateTimeCoefficient(averagingTimeMs, frameIntervalSeconds);
    const auto publishedIndex = activeScopeBuffer.load(std::memory_order_relaxed);
    const auto writeIndex = 1 - publishedIndex;
    auto& detectorScope = phaseDetectorScopeBuffers[static_cast<size_t>(writeIndex)];
    auto& reductionScope = phaseReductionScopeBuffers[static_cast<size_t>(writeIndex)];

    const auto correlationAtBin = [this] (const int bin)
    {
        const auto& left = fftData[0][static_cast<size_t>(bin)];
        const auto& right = fftData[1][static_cast<size_t>(bin)];
        const auto denominator = std::abs(left) * std::abs(right);
        return denominator > 1.0e-12f
            ? juce::jlimit(-1.0f,
                           1.0f,
                           std::real(left * std::conj(right)) / denominator)
            : 0.0f;
    };

    for (std::size_t i = 0; i < analyserScopeSize; ++i)
    {
        const auto proportion = static_cast<float>(i) / static_cast<float>(analyserScopeSize - 1);
        const auto frequency = juce::mapToLog10(proportion, analyserMinFrequency, maxFrequency);
        const auto fractionalBin = juce::jlimit(0.0f,
                                                static_cast<float>(fftSize / 2),
                                                frequency * static_cast<float>(fftSize)
                                                    / static_cast<float>(currentSampleRate));
        const auto lowerBin = juce::jlimit(0, fftSize / 2, static_cast<int>(std::floor(fractionalBin)));
        const auto upperBin = juce::jlimit(0, fftSize / 2, lowerBin + 1);
        const auto interpolation = fractionalBin - static_cast<float>(lowerBin);
        const auto rawCorrelation = juce::jmap(interpolation,
                                               correlationAtBin(lowerBin),
                                               correlationAtBin(upperBin));
        auto& smoothedCorrelation = smoothedPhaseCorrelations[i];
        smoothedCorrelation = phaseAnalysisReady
            ? (displaySmoothingCoefficient * smoothedCorrelation)
                + ((1.0f - displaySmoothingCoefficient) * rawCorrelation)
            : rawCorrelation;
        detectorScope[i] = juce::jlimit(-1.0f, 1.0f, smoothedCorrelation);

        for (auto channel = 0; channel < maxChannels; ++channel)
        {
            const auto rawReduction = latestPhaseReductions[static_cast<size_t>(channel)][i];
            auto& smoothedReduction = smoothedPhaseReductions[static_cast<size_t>(channel)][i];
            smoothedReduction = phaseAnalysisReady
                ? (displaySmoothingCoefficient * smoothedReduction)
                    + ((1.0f - displaySmoothingCoefficient) * rawReduction)
                : rawReduction;
            reductionScope[static_cast<size_t>(channel)][i] = smoothedReduction;
        }
    }

    phaseAnalysisReady = true;
    activeScopeBuffer.store(writeIndex, std::memory_order_release);
}

int FftModuleProcessor::PostAnalyser::getFftIndexForSize(int fftSize) const noexcept
{
    switch (fftSize)
    {
        case 1024: return 0;
        case 2048: return 1;
        case 4096: return 2;
        case 8192: return 3;
        case 16384: return 4;
        default: return 1;
    }
}
