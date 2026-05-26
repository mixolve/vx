#include "module.mxe.MultibandProcessor.h"

#include <algorithm>
#include <cmath>

namespace mxe::dsp
{
void MultibandProcessor::prepare(const double sampleRate, const int maxBlockSize, const int numChannels)
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
    setFullbandParameters(fullbandParameters);
    reset();
}

void MultibandProcessor::reset()
{
    crossover.reset();

    for (auto& bandProcessor : bandProcessors)
        bandProcessor.reset();

    snapFullbandParameters();
    clearAlignmentBuffers();
}

void MultibandProcessor::setBandParameters(const BandParameters& newParameters)
{
    parameters = newParameters;

    for (size_t bandIndex = 0; bandIndex < bandProcessors.size(); ++bandIndex)
    {
        bandProcessors[bandIndex].setParameters(parameters[bandIndex]);
        bandLatencies[bandIndex] = bandProcessors[bandIndex].getLatencySamples();
    }

    updateLatencyCompensation();
}

void MultibandProcessor::setActiveSplitCount(const size_t newActiveSplitCount)
{
    const auto constrainedSplitCount = std::min(newActiveSplitCount, numSplits);

    if (activeSplitCount == constrainedSplitCount)
        return;

    activeSplitCount = constrainedSplitCount;
    crossover.setActiveSplitCount(activeSplitCount);
    updateLatencyCompensation();
    clearAlignmentBuffers();
}

void MultibandProcessor::setCrossoverFrequencies(const CrossoverFrequencies& newFrequencies)
{
    if (crossoverFrequencies == newFrequencies)
        return;

    crossoverFrequencies = newFrequencies;
    crossover.setSplitFrequencies(crossoverFrequencies);
    clearAlignmentBuffers();
}

void MultibandProcessor::setFullbandParameters(const FullbandParameters& newParameters)
{
    fullbandParameters = newParameters;
    fullbandInGain = dbToAmp(roundToJsfxStep(fullbandParameters.inGn + fullbandParameters.autoInGn));
    fullbandOutGain = dbToAmp(roundToJsfxStep(fullbandParameters.outGn));
    fullbandRightGain = dbToAmp(roundToJsfxStep(fullbandParameters.autoInRight));
    fullbandLeftGain = dbToAmp(roundToJsfxStep(fullbandParameters.autoInLeft));
}

void MultibandProcessor::setSoloMask(const SoloMask& newSoloMask)
{
    soloMask = newSoloMask;
    anySoloActive = std::any_of(soloMask.begin(), soloMask.end(), [] (const bool isSoloed) { return isSoloed; });
}

void MultibandProcessor::process(juce::AudioBuffer<float>& buffer)
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
        const auto fullbandGain = fullbandInGain;
        const auto fullbandOut = fullbandOutGain;
        const auto fullbandRight = fullbandRightGain;
        const auto fullbandLeft = fullbandLeftGain;
        const auto widthGain = roundToJsfxStep(fullbandParameters.wide) / 100.0;
        const auto gainedLeft = leftInput * fullbandGain * fullbandLeft;
        const auto gainedRight = rightInput * fullbandGain * fullbandRight;
        const auto mid = 0.5 * (gainedLeft + gainedRight);
        const auto side = 0.5 * (gainedLeft - gainedRight) * widthGain;
        const auto preLeft = mid + side;
        const auto preRight = mid - side;
        const auto bands = crossover.processSample(preLeft, preRight);

        auto sumLeft = 0.0;
        auto sumRight = 0.0;

        const auto activeBandCount = activeSplitCount + 1;

        for (size_t bandIndex = 0; bandIndex < activeBandCount; ++bandIndex)
        {
            const auto bandOutput = bandProcessors[bandIndex].processSample(bands[bandIndex].left, bands[bandIndex].right);
            alignmentLeft[bandIndex][static_cast<size_t>(alignmentWritePosition)] = bandOutput.left;
            alignmentRight[bandIndex][static_cast<size_t>(alignmentWritePosition)] = bandOutput.right;

            const auto compensationSamples = targetLatencySamples - bandLatencies[bandIndex];
            const auto readPosition = wrapIndex(alignmentWritePosition - compensationSamples, alignmentBufferSize);
            const auto includeBand = ! anySoloActive || soloMask[bandIndex];

            if (includeBand)
            {
                sumLeft += alignmentLeft[bandIndex][static_cast<size_t>(readPosition)];
                sumRight += alignmentRight[bandIndex][static_cast<size_t>(readPosition)];
            }
        }

        leftChannel[sampleIndex] = static_cast<float>(sumLeft * fullbandOut);

        if (rightChannel != nullptr)
            rightChannel[sampleIndex] = static_cast<float>(sumRight * fullbandOut);

        alignmentWritePosition = wrapIndex(alignmentWritePosition + 1, alignmentBufferSize);
    }
}

int MultibandProcessor::getLatencySamples() const noexcept
{
    return targetLatencySamples;
}

void MultibandProcessor::clearAlignmentBuffers()
{
    for (auto& buffer : alignmentLeft)
        std::fill(buffer.begin(), buffer.end(), 0.0);

    for (auto& buffer : alignmentRight)
        std::fill(buffer.begin(), buffer.end(), 0.0);

    alignmentWritePosition = 0;
}

void MultibandProcessor::snapFullbandParameters() noexcept
{
    fullbandInGain = dbToAmp(roundToJsfxStep(fullbandParameters.inGn + fullbandParameters.autoInGn));
    fullbandOutGain = dbToAmp(roundToJsfxStep(fullbandParameters.outGn));
    fullbandRightGain = dbToAmp(roundToJsfxStep(fullbandParameters.autoInRight));
    fullbandLeftGain = dbToAmp(roundToJsfxStep(fullbandParameters.autoInLeft));
}

void MultibandProcessor::updateLatencyCompensation()
{
    const auto activeBandCount = activeSplitCount + 1;
    targetLatencySamples = *std::max_element(bandLatencies.begin(), bandLatencies.begin() + static_cast<std::ptrdiff_t>(activeBandCount));
    jassert(targetLatencySamples < alignmentBufferSize);
}
} // namespace mxe::dsp
