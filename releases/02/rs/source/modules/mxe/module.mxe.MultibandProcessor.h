#pragma once

#include "module.mxe.DspCore.h"
#include "module.mxe.MultibandCrossover.h"

#include <array>
#include <vector>

namespace mxe::dsp
{
class MultibandProcessor
{
public:
    static constexpr size_t numBands = MultibandCrossover::numBands;
    static constexpr size_t numSplits = MultibandCrossover::numSplits;
    using Parameters = DspCore::Parameters;
    using BandParameters = std::array<Parameters, numBands>;
    using CrossoverFrequencies = MultibandCrossover::SplitFrequencies;
    using SoloMask = std::array<bool, numBands>;
    struct FullbandParameters
    {
        float inGn = 0.0f;
        float outGn = 0.0f;
        float autoInGn = 0.0f;
        float autoInRight = 0.0f;
        float autoInLeft = 0.0f;
        float wide = 100.0f;
    };

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setBandParameters(const BandParameters& newParameters);
    void setActiveSplitCount(size_t newActiveSplitCount);
    void setCrossoverFrequencies(const CrossoverFrequencies& newFrequencies);
    void setFullbandParameters(const FullbandParameters& newParameters);
    void setSoloMask(const SoloMask& newSoloMask);
    void process(juce::AudioBuffer<float>& buffer);
    int getLatencySamples() const noexcept;

private:
    void snapFullbandParameters() noexcept;
    void clearAlignmentBuffers();
    void updateLatencyCompensation();

    BandParameters parameters {};
    CrossoverFrequencies crossoverFrequencies { 134.0, 523.0, 2093.0, 5000.0, 10000.0 };
    size_t activeSplitCount = numSplits;
    FullbandParameters fullbandParameters {};
    MultibandCrossover crossover;
    std::array<DspCore, numBands> bandProcessors;
    std::array<int, numBands> bandLatencies {};
    SoloMask soloMask {};
    bool anySoloActive = false;
    double fullbandInGain = 1.0;
    double fullbandOutGain = 1.0;
    double fullbandRightGain = 1.0;
    double fullbandLeftGain = 1.0;
    std::array<std::vector<double>, numBands> alignmentLeft;
    std::array<std::vector<double>, numBands> alignmentRight;
    int alignmentBufferSize = 1;
    int alignmentWritePosition = 0;
    int targetLatencySamples = 0;
};
} // namespace mxe::dsp
