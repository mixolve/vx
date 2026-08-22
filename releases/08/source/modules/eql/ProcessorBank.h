#pragma once

#include "Processor.h"
#include "../ProcessorRangeBank.h"

#include <cstddef>

class EqlProcessorBank final
{
public:
    static constexpr size_t numRanges = ava::modules::ProcessorRangeBank<EqlModuleProcessor>::numRanges;

    explicit EqlProcessorBank(juce::AudioProcessor& owner);

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void resetProcessingState() noexcept;
    void processRange(size_t rangeIndex, juce::AudioBuffer<float>& buffer);
    int getLatencySamples() const noexcept;
    ava::modules::ProcessorRangeBank<EqlModuleProcessor>::RangeLatencies getRangeLatencies() const noexcept;
    void loadInitialFilterPreset();
    size_t ensureRangeCount(size_t rangeCount);
    size_t getCreatedRangeCount() const noexcept;

    void setSelectedRange(size_t rangeIndex) noexcept;
    EqlModuleProcessor* getSelectedProcessor() noexcept;
    const EqlModuleProcessor* getSelectedProcessor() const noexcept;

private:
    ava::modules::ProcessorRangeBank<EqlModuleProcessor> ranges;
    bool initialFilterPresetLoaded = false;
};
