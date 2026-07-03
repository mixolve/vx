#pragma once

#include "module.multiband.Crossover.h"

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace vx::multiband
{
namespace detail
{
inline int wrapIndex(int index, const int size)
{
    jassert(size > 0);

    index %= size;

    if (index < 0)
        index += size;

    return index;
}
} // namespace detail

template <typename DspCore>
class Processor
{
public:
    static constexpr size_t numBands = Crossover::numBands;
    static constexpr size_t numSplits = Crossover::numSplits;

    using Parameters = typename DspCore::Parameters;
    using BandParameters = std::array<Parameters, numBands>;
    using CrossoverFrequencies = Crossover::SplitFrequencies;
    using SoloMask = std::array<bool, numBands>;

    void prepare(double sampleRate, int maxBlockSize, int numChannels)
    {
        crossover.prepare(sampleRate);

        alignmentBufferSize = std::max(1, DspCore::getMaximumLatencySamples(sampleRate) + 1);

        for (auto& buffer : alignmentLeft)
            buffer.assign(static_cast<size_t>(alignmentBufferSize), 0.0);

        for (auto& buffer : alignmentRight)
            buffer.assign(static_cast<size_t>(alignmentBufferSize), 0.0);

        for (auto& bandProcessor : bandProcessors)
            bandProcessor.prepare(sampleRate, maxBlockSize, numChannels);

        crossover.setActiveSplitCount(activeSplitCount);
        crossover.setSplitFrequencies(crossoverFrequencies);
        setBandParameters(parameters);
        reset();
    }

    void reset()
    {
        crossover.reset();

        for (auto& bandProcessor : bandProcessors)
            bandProcessor.reset();

        clearAlignmentBuffers();
    }

    void setBandParameters(const BandParameters& newParameters)
    {
        parameters = newParameters;

        for (size_t bandIndex = 0; bandIndex < bandProcessors.size(); ++bandIndex)
        {
            bandProcessors[bandIndex].setParameters(parameters[bandIndex]);
            bandLatencies[bandIndex] = bandProcessors[bandIndex].getLatencySamples();
        }

        updateLatencyCompensation();
    }

    void setActiveSplitCount(const size_t newActiveSplitCount)
    {
        const auto constrainedSplitCount = std::min(newActiveSplitCount, numSplits);

        if (activeSplitCount == constrainedSplitCount)
            return;

        activeSplitCount = constrainedSplitCount;
        crossover.setActiveSplitCount(activeSplitCount);
        updateLatencyCompensation();
        clearAlignmentBuffers();
    }

    void setCrossoverFrequencies(const CrossoverFrequencies& newFrequencies)
    {
        if (crossoverFrequencies == newFrequencies)
            return;

        crossoverFrequencies = newFrequencies;
        crossover.setSplitFrequencies(crossoverFrequencies);
        clearAlignmentBuffers();
    }

    void setSoloMask(const SoloMask& newSoloMask)
    {
        soloMask = newSoloMask;
        anySoloActive = std::any_of(soloMask.begin(), soloMask.end(), [] (const bool isSoloed) { return isSoloed; });
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
            return;

        auto* leftChannel = buffer.getWritePointer(0);
        auto* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (auto& bandProcessor : bandProcessors)
            bandProcessor.beginBlock(buffer.getNumSamples());

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            const auto leftInput = static_cast<double>(leftChannel[sampleIndex]);
            const auto rightInput = rightChannel != nullptr ? static_cast<double>(rightChannel[sampleIndex]) : leftInput;
            const auto bands = crossover.processSample(leftInput, rightInput);

            auto sumLeft = 0.0;
            auto sumRight = 0.0;
            const auto activeBandCount = activeSplitCount + 1;

            for (size_t bandIndex = 0; bandIndex < activeBandCount; ++bandIndex)
            {
                const auto bandOutput = bandProcessors[bandIndex].processSample(bands[bandIndex].left, bands[bandIndex].right);
                alignmentLeft[bandIndex][static_cast<size_t>(alignmentWritePosition)] = bandOutput.left;
                alignmentRight[bandIndex][static_cast<size_t>(alignmentWritePosition)] = bandOutput.right;

                const auto compensationSamples = targetLatencySamples - bandLatencies[bandIndex];
                const auto readPosition = detail::wrapIndex(alignmentWritePosition - compensationSamples, alignmentBufferSize);
                const auto includeBand = ! anySoloActive || soloMask[bandIndex];

                if (includeBand)
                {
                    sumLeft += alignmentLeft[bandIndex][static_cast<size_t>(readPosition)];
                    sumRight += alignmentRight[bandIndex][static_cast<size_t>(readPosition)];
                }
            }

            leftChannel[sampleIndex] = static_cast<float>(sumLeft);

            if (rightChannel != nullptr)
                rightChannel[sampleIndex] = static_cast<float>(sumRight);

            alignmentWritePosition = detail::wrapIndex(alignmentWritePosition + 1, alignmentBufferSize);
        }
    }

    int getLatencySamples() const noexcept
    {
        return targetLatencySamples;
    }

private:
    void clearAlignmentBuffers()
    {
        for (auto& buffer : alignmentLeft)
            std::fill(buffer.begin(), buffer.end(), 0.0);

        for (auto& buffer : alignmentRight)
            std::fill(buffer.begin(), buffer.end(), 0.0);

        alignmentWritePosition = 0;
    }

    void updateLatencyCompensation()
    {
        const auto activeBandCount = activeSplitCount + 1;
        targetLatencySamples = *std::max_element(bandLatencies.begin(), bandLatencies.begin() + static_cast<std::ptrdiff_t>(activeBandCount));
        jassert(targetLatencySamples < alignmentBufferSize);
    }

    BandParameters parameters {};
    CrossoverFrequencies crossoverFrequencies { 134.0, 523.0, 2093.0, 5000.0, 10000.0 };
    size_t activeSplitCount = numSplits;
    Crossover crossover;
    std::array<DspCore, numBands> bandProcessors;
    std::array<int, numBands> bandLatencies {};
    SoloMask soloMask {};
    bool anySoloActive = false;
    std::array<std::vector<double>, numBands> alignmentLeft;
    std::array<std::vector<double>, numBands> alignmentRight;
    int alignmentBufferSize = 1;
    int alignmentWritePosition = 0;
    int targetLatencySamples = 0;
};
} // namespace vx::multiband
