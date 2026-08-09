#include "module.eqe.ProcessorSupport.h"

#include <cmath>
#include <functional>

void EqeModuleProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    prepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(filterProcessLock);

    currentSampleRate = sampleRate;
    lastProcessedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedNumChannels = static_cast<int>(maxSupportedChannels);
    filterProcessBufferA.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    filterProcessBufferB.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    placeWorkBuffer.setSize(1, juce::jmax(1, samplesPerBlock));
    placeAuxBuffer.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));

    resetFilters();
    markEqeFiltersDirty();
    updateFilters();
    eqeFiltersDirty.store(false, std::memory_order_release);
    prepared.store(true, std::memory_order_release);
}

void EqeModuleProcessor::releaseResources()
{
    prepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(filterProcessLock);

    resetFilters();
    currentSampleRate = 0.0;
}

void EqeModuleProcessor::resetProcessingState() noexcept
{
    const juce::ScopedLock lock(filterProcessLock);

    for (auto& bandFilters : bellOrderFilters)
        for (auto& filter : bandFilters)
            filter.reset();

    for (auto& bandFilters : shelfOrderFilters)
        for (auto& filter : bandFilters)
            filter.reset();

    for (auto& filter : tiltFilters)
        filter.reset();

    for (auto& filter : cutBlendFilters)
        filter.reset();

    for (auto& filter : phaseFirFilters)
        filter.reset();

    filterProcessBufferA.clear();
    filterProcessBufferB.clear();
    placeWorkBuffer.clear();
    placeAuxBuffer.clear();
}

void EqeModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! prepared.load(std::memory_order_acquire))
        return;

    const juce::ScopedTryLock lock(filterProcessLock);

    if (! lock.isLocked())
        return;

    if (! prepared.load(std::memory_order_acquire))
        return;

    const auto processChannels = juce::jmin(buffer.getNumChannels(), preparedNumChannels);
    const auto processSamples = buffer.getNumSamples();

    if (processChannels <= 0 || processSamples <= 0)
        return;

    filterProcessBufferA.setSize(processChannels, processSamples, false, false, true);
    filterProcessBufferB.setSize(processChannels, processSamples, false, false, true);
    placeWorkBuffer.setSize(1, processSamples, false, false, true);
    placeAuxBuffer.setSize(processChannels, processSamples, false, false, true);

    if (eqeFiltersDirty.exchange(false, std::memory_order_acq_rel))
        updateFilters();

    const auto filterCount = getActiveFilterCount();

    auto processPlacedSignal = [this, processChannels] (juce::AudioBuffer<float>& targetBuffer,
                                                       const int bandIndex,
                                                       const std::function<void(juce::AudioBuffer<float>&, int)>& processor)
    {
        if (processChannels < 2)
        {
            processor(targetBuffer, processChannels);
            return;
        }

        const auto mode = filterPlaceParams[static_cast<size_t>(bandIndex)] != nullptr
            ? juce::jlimit(0, 7, static_cast<int>(std::lround(filterPlaceParams[static_cast<size_t>(bandIndex)]->load(std::memory_order_relaxed))))
            : 0;

        if (mode == 0 || mode == 5)
        {
            processor(targetBuffer, processChannels);
            return;
        }

        auto& workBuffer = placeWorkBuffer;
        auto& auxBuffer = placeAuxBuffer;

        switch (mode)
        {
            case 1:
                workBuffer.copyFrom(0, 0, targetBuffer, 0, 0, targetBuffer.getNumSamples());
                processor(workBuffer, 1);
                targetBuffer.copyFrom(0, 0, workBuffer, 0, 0, targetBuffer.getNumSamples());
                break;

            case 2:
                workBuffer.copyFrom(0, 0, targetBuffer, 1, 0, targetBuffer.getNumSamples());
                processor(workBuffer, 1);
                targetBuffer.copyFrom(1, 0, workBuffer, 0, 0, targetBuffer.getNumSamples());
                break;

            case 3:
                auxBuffer.copyFrom(0, 0, targetBuffer, 0, 0, targetBuffer.getNumSamples());
                auxBuffer.copyFrom(1, 0, targetBuffer, 1, 0, targetBuffer.getNumSamples());
                for (int sampleIndex = 0; sampleIndex < targetBuffer.getNumSamples(); ++sampleIndex)
                {
                    const auto left = auxBuffer.getReadPointer(0)[sampleIndex];
                    const auto right = auxBuffer.getReadPointer(1)[sampleIndex];
                    workBuffer.setSample(0, sampleIndex, 0.5f * (left + right));
                }
                processor(workBuffer, 1);
                for (int sampleIndex = 0; sampleIndex < targetBuffer.getNumSamples(); ++sampleIndex)
                {
                    const auto processedMid = workBuffer.getReadPointer(0)[sampleIndex];
                    const auto side = 0.5f * (auxBuffer.getReadPointer(0)[sampleIndex] - auxBuffer.getReadPointer(1)[sampleIndex]);
                    targetBuffer.setSample(0, sampleIndex, processedMid + side);
                    targetBuffer.setSample(1, sampleIndex, processedMid - side);
                }
                break;

            case 4:
                auxBuffer.copyFrom(0, 0, targetBuffer, 0, 0, targetBuffer.getNumSamples());
                auxBuffer.copyFrom(1, 0, targetBuffer, 1, 0, targetBuffer.getNumSamples());
                for (int sampleIndex = 0; sampleIndex < targetBuffer.getNumSamples(); ++sampleIndex)
                {
                    const auto left = auxBuffer.getReadPointer(0)[sampleIndex];
                    const auto right = auxBuffer.getReadPointer(1)[sampleIndex];
                    workBuffer.setSample(0, sampleIndex, 0.5f * (left - right));
                }
                processor(workBuffer, 1);
                for (int sampleIndex = 0; sampleIndex < targetBuffer.getNumSamples(); ++sampleIndex)
                {
                    const auto side = workBuffer.getReadPointer(0)[sampleIndex];
                    const auto mid = 0.5f * (auxBuffer.getReadPointer(0)[sampleIndex] + auxBuffer.getReadPointer(1)[sampleIndex]);
                    targetBuffer.setSample(0, sampleIndex, mid + side);
                    targetBuffer.setSample(1, sampleIndex, mid - side);
                }
                break;

            case 6:
                workBuffer.copyFrom(0, 0, targetBuffer, 0, 0, targetBuffer.getNumSamples());
                processor(workBuffer, 1);
                targetBuffer.copyFrom(0, 0, workBuffer, 0, 0, targetBuffer.getNumSamples());
                break;

            case 7:
                workBuffer.copyFrom(0, 0, targetBuffer, 1, 0, targetBuffer.getNumSamples());
                processor(workBuffer, 1);
                targetBuffer.copyFrom(1, 0, workBuffer, 0, 0, targetBuffer.getNumSamples());
                break;

            default:
                processor(targetBuffer, processChannels);
                break;
        }
    };

    auto processBandBuffer = [this, &processPlacedSignal] (juce::AudioBuffer<float>& targetBuffer,
                                                          const int filterIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(filterIndex);
        const auto filterType = getFilterTypeForBand(bandArrayIndex);
        const auto bandBypassed = filterBypassParams[bandArrayIndex] != nullptr
            && filterBypassParams[bandArrayIndex]->load(std::memory_order_relaxed) >= 0.5f;

        if (bandBypassed)
            return;

        auto placeChoice = filterPlaceParams[bandArrayIndex] != nullptr
            ? juce::jlimit(0, 7, static_cast<int>(std::lround(filterPlaceParams[bandArrayIndex]->load(std::memory_order_relaxed))))
            : 0;

        if (isVolumeFilterType(filterType) && isPhasePlaceChoice(placeChoice))
            placeChoice = 0;

        if (isPhasePlaceChoice(placeChoice) && ! isCutFilterType(filterType))
        {
            if (filterType == FilterType::bell)
            {
                if (filterSlopeChoiceParams[bandArrayIndex] != nullptr
                    && filterSlopeChoiceParams[bandArrayIndex]->getIndex() == 0)
                    return;

                if (! phaseFirFilters[bandArrayIndex].active)
                    return;

                phaseFirFilters[bandArrayIndex].processWithChannelMask(targetBuffer,
                                                                       juce::jmin(targetBuffer.getNumChannels(), preparedNumChannels),
                                                                       placeChoice != 7,
                                                                       placeChoice != 6);
                return;
            }

            if (! phaseFirFilters[bandArrayIndex].active)
                return;

            phaseFirFilters[bandArrayIndex].processWithChannelMask(targetBuffer,
                                                                   juce::jmin(targetBuffer.getNumChannels(), preparedNumChannels),
                                                                   placeChoice != 7,
                                                                   placeChoice != 6);

            return;
        }

        if (isVolumeFilterType(filterType))
        {
            const auto gainDb = filterGainParams[bandArrayIndex] != nullptr
                ? juce::jlimit(-48.0f,
                               48.0f,
                               filterGainParams[bandArrayIndex]->load(std::memory_order_relaxed))
                : 0.0f;

            if (std::abs(gainDb) < 1.0e-6f)
                return;

            const auto gain = juce::Decibels::decibelsToGain(gainDb);
            processPlacedSignal(targetBuffer, static_cast<int>(bandArrayIndex), [gain] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                for (int channel = 0; channel < targetChannels; ++channel)
                    targetBufferToProcess.applyGain(channel, 0, targetBufferToProcess.getNumSamples(), gain);
            });
            return;
        }

        const auto slopeDbPerOct = filterSlopeChoiceParams[bandArrayIndex] != nullptr
            ? static_cast<double>(EqeModuleProcessor::getBellSlopeValueForChoiceIndex(filterSlopeChoiceParams[bandArrayIndex]->getIndex()))
            : static_cast<double>(EqeModuleProcessor::fixedSlopeDbPerOct);

        if (filterType == FilterType::bell)
        {
            if (filterSlopeChoiceParams[bandArrayIndex] != nullptr
                && filterSlopeChoiceParams[bandArrayIndex]->getIndex() == 0)
                return;

            if (bellOrderFilters[bandArrayIndex].front().sectionCount <= 0)
                return;

            const auto bellSlopeBlend = mapBellSlopeToBlend(slopeDbPerOct);
            const auto lowerOrder = bellSlopeBlend.lowerOrder;
            const auto upperOrder = bellSlopeBlend.upperOrder;
            const auto blend = bellSlopeBlend.blend;
            auto& bandFilters = bellOrderFilters[bandArrayIndex];

            processPlacedSignal(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                if (lowerOrder == upperOrder || blend < 1.0e-6)
                {
                    if (lowerOrder > 0)
                        bandFilters[static_cast<size_t>(lowerOrder - 1)].process(targetBufferToProcess, targetChannels);
                    return;
                }

                if (lowerOrder > 0)
                {
                    filterProcessBufferA.makeCopyOf(targetBufferToProcess, true);
                    bandFilters[static_cast<size_t>(lowerOrder - 1)].process(filterProcessBufferA, targetChannels);
                }
                else
                {
                    filterProcessBufferA.makeCopyOf(targetBufferToProcess, true);
                }

                filterProcessBufferB.makeCopyOf(targetBufferToProcess, true);

                if (upperOrder > 0)
                    bandFilters[static_cast<size_t>(upperOrder - 1)].process(filterProcessBufferB, targetChannels);

                for (int channel = 0; channel < targetChannels; ++channel)
                {
                    auto* output = targetBufferToProcess.getWritePointer(channel);
                    const auto* lower = filterProcessBufferA.getReadPointer(channel);
                    const auto* upper = filterProcessBufferB.getReadPointer(channel);

                    for (int sampleIndex = 0; sampleIndex < targetBufferToProcess.getNumSamples(); ++sampleIndex)
                        output[sampleIndex] = static_cast<float>(lower[sampleIndex]
                                                                 + ((upper[sampleIndex] - lower[sampleIndex]) * blend));
                }
            });
            return;
        }

        if (isTiltFilterType(filterType))
        {
            if (tiltFilters[bandArrayIndex].stageCount <= 0)
                return;

            processPlacedSignal(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                tiltFilters[bandArrayIndex].process(targetBufferToProcess, targetChannels);
            });
            return;
        }

        if (isShelfFilterType(filterType))
        {
            if (shelfOrderFilters[bandArrayIndex].front().stageCount <= 0)
                return;

            processPlacedSignal(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                auto& bandFilters = shelfOrderFilters[bandArrayIndex];
                const auto slopeBlend = mapShelfSlopeToBlend(slopeDbPerOct);
                const auto lowerOrder = slopeBlend.lowerOrder;
                const auto upperOrder = slopeBlend.upperOrder;
                const auto blend = slopeBlend.blend;

                if (lowerOrder == upperOrder || blend < 1.0e-6)
                {
                    bandFilters[static_cast<size_t>(lowerOrder - 1)].process(targetBufferToProcess, targetChannels);
                    return;
                }

                filterProcessBufferA.makeCopyOf(targetBufferToProcess, true);
                filterProcessBufferB.makeCopyOf(targetBufferToProcess, true);
                bandFilters[static_cast<size_t>(lowerOrder - 1)].process(filterProcessBufferA, targetChannels);
                bandFilters[static_cast<size_t>(upperOrder - 1)].process(filterProcessBufferB, targetChannels);

                for (int channel = 0; channel < targetChannels; ++channel)
                {
                    auto* output = targetBufferToProcess.getWritePointer(channel);
                    const auto* lower = filterProcessBufferA.getReadPointer(channel);
                    const auto* upper = filterProcessBufferB.getReadPointer(channel);

                    for (int sampleIndex = 0; sampleIndex < targetBufferToProcess.getNumSamples(); ++sampleIndex)
                        output[sampleIndex] = static_cast<float>(lower[sampleIndex]
                                                                 + ((upper[sampleIndex] - lower[sampleIndex]) * blend));
                }
            });
            return;
        }

        if (isCutFilterType(filterType))
        {
            if (cutBlendFilters[bandArrayIndex].stageCount <= 0)
                return;

            processPlacedSignal(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                cutBlendFilters[bandArrayIndex].process(targetBufferToProcess, targetChannels);
            });
        }
    };

    for (int filterIndex = 0; filterIndex < filterCount; ++filterIndex)
        processBandBuffer(buffer, filterIndex);
}

int EqeModuleProcessor::getLatencySamples() const noexcept
{
    const auto filterCount = getActiveFilterCount();

    for (int filterIndex = 0; filterIndex < filterCount; ++filterIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(filterIndex);
        const auto placeChoice = filterPlaceParams[bandArrayIndex] != nullptr
            ? juce::jlimit(0, 7, static_cast<int>(std::lround(filterPlaceParams[bandArrayIndex]->load(std::memory_order_relaxed))))
            : 0;

        if (isPhasePlaceChoice(placeChoice) && phaseFirFilters[bandArrayIndex].active)
            return phaseFirLatencySamples;
    }

    return 0;
}

EqeModuleProcessor::FilterType EqeModuleProcessor::getFilterTypeForBand(const size_t filterIndex) const noexcept
{
    if (filterIndex >= filterTypeParams.size() || filterTypeParams[filterIndex] == nullptr)
        return FilterType::bell;

    return filterTypeFromChoiceIndex(static_cast<int>(std::lround(filterTypeParams[filterIndex]->load(std::memory_order_relaxed))));
}

bool EqeModuleProcessor::filterDesignMatches(const size_t filterIndex,
                                             const bool active,
                                             const FilterType type,
                                             const float frequency,
                                             const float bandwidth,
                                             const float slope,
                                             const float gainDb) const noexcept
{
    const auto& cachedState = cachedFilterStates[filterIndex];

    if (! cachedState.valid)
        return false;

    if (cachedState.active != active
        || cachedState.type != type
        || std::abs(cachedState.sampleRate - currentSampleRate) > 1.0e-9)
        return false;

    if (! active)
        return true;

    return std::abs(cachedState.frequency - frequency) < 1.0e-4f
        && std::abs(cachedState.bandwidth - bandwidth) < 1.0e-4f
        && std::abs(cachedState.slope - slope) < 1.0e-4f
        && std::abs(cachedState.gainDb - gainDb) < 1.0e-4f;
}

void EqeModuleProcessor::storeFilterDesignState(const size_t filterIndex,
                                                const bool active,
                                                const FilterType type,
                                                const float frequency,
                                                const float bandwidth,
                                                const float slope,
                                                const float gainDb) noexcept
{
    auto& cachedState = cachedFilterStates[filterIndex];
    cachedState.valid = true;
    cachedState.active = active;
    cachedState.type = type;
    cachedState.frequency = frequency;
    cachedState.bandwidth = bandwidth;
    cachedState.slope = slope;
    cachedState.gainDb = gainDb;
    cachedState.sampleRate = currentSampleRate;
}
