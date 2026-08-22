#include "ProcessorBank.h"

#include <algorithm>

FftProcessorBank::FftProcessorBank(juce::AudioProcessor& owner)
    : ranges([&owner] { return std::make_unique<FftModuleProcessor>(owner); })
{}

void FftProcessorBank::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    ranges.prepareToPlay(sampleRate, samplesPerBlock);
}

void FftProcessorBank::releaseResources()
{
    ranges.releaseResources();
}

void FftProcessorBank::resetProcessingState() noexcept
{
    ranges.resetProcessingState();
}

void FftProcessorBank::processRange(const size_t rangeIndex, juce::AudioBuffer<float>& buffer)
{
    ranges.processRange(rangeIndex, buffer);
}

int FftProcessorBank::getLatencySamples() const noexcept
{
    const auto latencies = ranges.getRangeLatencies();
    return *std::max_element(latencies.begin(), latencies.end());
}

ava::modules::ProcessorRangeBank<FftModuleProcessor>::RangeLatencies FftProcessorBank::getRangeLatencies() const noexcept
{
    return ranges.getRangeLatencies();
}

bool FftProcessorBank::refreshLatencyState() noexcept
{
    auto changed = false;
    ranges.forEachProcessor([&changed] (FftModuleProcessor& processor)
    {
        changed = processor.refreshLatencyState() || changed;
    });

    return changed;
}

size_t FftProcessorBank::ensureRangeCount(const size_t rangeCount)
{
    return ranges.ensureRangeCount(rangeCount);
}

size_t FftProcessorBank::getCreatedRangeCount() const noexcept
{
    return ranges.getCreatedRangeCount();
}

void FftProcessorBank::setSelectedRange(const size_t rangeIndex) noexcept
{
    ranges.setSelectedRange(rangeIndex);
}

FftModuleProcessor* FftProcessorBank::getSelectedProcessor() noexcept
{
    return ranges.getSelectedProcessor();
}

const FftModuleProcessor* FftProcessorBank::getSelectedProcessor() const noexcept
{
    return ranges.getSelectedProcessor();
}
