#include "module.spe.SpeProcessor.h"
#include "module.spe.ProcessorConstants.h"

#include <algorithm>
#include <cmath>

SpeModuleProcessor::PostAnalyser::PostAnalyser()
{
    const std::array<int, 5> fftOrders { 10, 11, 12, 13, 14 };

    for (auto i = 0; i < static_cast<int>(fftOrders.size()); ++i)
    {
        ffts[static_cast<size_t>(i)] = std::make_unique<juce::dsp::FFT>(fftOrders[static_cast<size_t>(i)]);
        windows[static_cast<size_t>(i)] = std::make_unique<juce::dsp::WindowingFunction<float>>(
            1 << fftOrders[static_cast<size_t>(i)],
            juce::dsp::WindowingFunction<float>::hann);
    }
}

void SpeModuleProcessor::PostAnalyser::prepare(double newSampleRate)
{
    sampleRate.store(newSampleRate, std::memory_order_relaxed);
    historyWriteIndex = 0;
    availableSamples = 0;
    samplesSinceLastTransform = 0;
    activeScopeBuffer.store(0, std::memory_order_relaxed);
    std::fill(sampleHistory.begin(), sampleHistory.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(smoothedMagnitudes.begin(), smoothedMagnitudes.end(), 0.0f);

    for (auto& scopeBuffer : scopeBuffers)
        std::fill(scopeBuffer.begin(), scopeBuffer.end(), analyserScopeStorageMinDecibels);
}

void SpeModuleProcessor::PostAnalyser::pushBuffer(const juce::AudioBuffer<float>& buffer,
                                                  int numInputChannels,
                                                  int fftSize,
                                                  int overlapFactor,
                                                  float averagingTimeMs)
{
    const auto channelsToUse = juce::jmin(numInputChannels, buffer.getNumChannels());

    if (channelsToUse <= 0)
        return;

    const auto normalisation = 1.0f / static_cast<float>(channelsToUse);

    for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        auto mixedSample = 0.0f;

        for (auto channel = 0; channel < channelsToUse; ++channel)
            mixedSample += buffer.getSample(channel, sampleIndex);

        pushSample(mixedSample * normalisation, fftSize, overlapFactor, averagingTimeMs);
    }
}

void SpeModuleProcessor::PostAnalyser::copyScope(std::array<float, analyserScopeSize>& destination,
                                                 double& currentSampleRate) const
{
    const auto activeIndex = activeScopeBuffer.load(std::memory_order_acquire);
    destination = scopeBuffers[static_cast<size_t>(activeIndex)];
    currentSampleRate = sampleRate.load(std::memory_order_relaxed);
}

void SpeModuleProcessor::PostAnalyser::pushSample(float sample,
                                                  int fftSize,
                                                  int overlapFactor,
                                                  float averagingTimeMs) noexcept
{
    sampleHistory[static_cast<size_t>(historyWriteIndex)] = sample;
    historyWriteIndex = (historyWriteIndex + 1) % maxFftSize;
    availableSamples = juce::jmin(availableSamples + 1, maxFftSize);
    ++samplesSinceLastTransform;

    const auto hopSize = juce::jmax(1, fftSize / juce::jmax(1, overlapFactor));

    if (availableSamples >= fftSize && samplesSinceLastTransform >= hopSize)
    {
        generateSpectrum(fftSize, overlapFactor, averagingTimeMs);
        samplesSinceLastTransform = 0;
    }
}

void SpeModuleProcessor::PostAnalyser::generateSpectrum(int fftSize,
                                                        int overlapFactor,
                                                        float averagingTimeMs) noexcept
{
    std::fill(fftData.begin(), fftData.end(), 0.0f);

    const auto fftIndex = getFftIndexForSize(fftSize);

    for (auto i = 0; i < fftSize; ++i)
    {
        const auto historyIndex = (historyWriteIndex - fftSize + i + maxFftSize) % maxFftSize;
        fftData[static_cast<size_t>(i)] = sampleHistory[static_cast<size_t>(historyIndex)];
    }

    windows[static_cast<size_t>(fftIndex)]->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
    ffts[static_cast<size_t>(fftIndex)]->performFrequencyOnlyForwardTransform(fftData.data());

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
        const auto lowerMagnitude = fftData[static_cast<size_t>(lowerBin)] / static_cast<float>(fftSize);
        const auto upperMagnitude = fftData[static_cast<size_t>(upperBin)] / static_cast<float>(fftSize);
        const auto rawMagnitude = juce::jmax(juce::jmap(interpolation, lowerMagnitude, upperMagnitude), 0.0f);
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

int SpeModuleProcessor::PostAnalyser::getFftIndexForSize(int fftSize) const noexcept
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
