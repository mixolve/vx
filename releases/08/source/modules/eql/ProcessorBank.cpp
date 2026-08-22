#include "ProcessorBank.h"

#include <algorithm>

EqlProcessorBank::EqlProcessorBank(juce::AudioProcessor&)
    : ranges([] { return std::make_unique<EqlModuleProcessor>(); })
{}

void EqlProcessorBank::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    ranges.prepareToPlay(sampleRate, samplesPerBlock);
}

void EqlProcessorBank::releaseResources()
{
    ranges.releaseResources();
}

void EqlProcessorBank::resetProcessingState() noexcept
{
    ranges.resetProcessingState();
}

void EqlProcessorBank::processRange(const size_t rangeIndex, juce::AudioBuffer<float>& buffer)
{
    ranges.processRange(rangeIndex, buffer);
}

int EqlProcessorBank::getLatencySamples() const noexcept
{
    const auto latencies = ranges.getRangeLatencies();
    return *std::max_element(latencies.begin(), latencies.end());
}

ava::modules::ProcessorRangeBank<EqlModuleProcessor>::RangeLatencies EqlProcessorBank::getRangeLatencies() const noexcept
{
    return ranges.getRangeLatencies();
}

void EqlProcessorBank::loadInitialFilterPreset()
{
    ranges.forEachProcessor([] (EqlModuleProcessor& processor)
    {
        processor.loadInitialFilterPreset();
    });

    initialFilterPresetLoaded = true;
}

size_t EqlProcessorBank::ensureRangeCount(const size_t rangeCount)
{
    const auto previousRangeCount = ranges.getCreatedRangeCount();
    const auto createdRangeCount = ranges.ensureRangeCount(rangeCount);

    if (initialFilterPresetLoaded)
    {
        for (auto rangeIndex = previousRangeCount; rangeIndex < createdRangeCount; ++rangeIndex)
            if (auto* processor = ranges.getProcessor(rangeIndex))
                processor->loadInitialFilterPreset();
    }

    return createdRangeCount;
}

size_t EqlProcessorBank::getCreatedRangeCount() const noexcept
{
    return ranges.getCreatedRangeCount();
}

void EqlProcessorBank::setSelectedRange(const size_t rangeIndex) noexcept
{
    ranges.setSelectedRange(rangeIndex);
}

EqlModuleProcessor* EqlProcessorBank::getSelectedProcessor() noexcept
{
    return ranges.getSelectedProcessor();
}

const EqlModuleProcessor* EqlProcessorBank::getSelectedProcessor() const noexcept
{
    return ranges.getSelectedProcessor();
}
