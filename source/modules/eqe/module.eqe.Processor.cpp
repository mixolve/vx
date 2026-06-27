#include "module.eqe.ProcessorSupport.h"

#include <array>
#include <cmath>
#include <functional>

void EqeModuleProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    prepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(filterProcessLock);

    currentSampleRate = sampleRate;
    lastProcessedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedNumChannels = static_cast<int>(maxSupportedChannels);
    bellProcessBufferA.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    bellProcessBufferB.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    lrmsWorkBuffer.setSize(1, juce::jmax(1, samplesPerBlock));
    lrmsAuxBuffer.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));

    resetBellFilters();
    markEqeFiltersDirty();
    updateBellFilters();
    eqeFiltersDirty.store(false, std::memory_order_release);
    prepared.store(true, std::memory_order_release);
}

void EqeModuleProcessor::releaseResources()
{
    prepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(filterProcessLock);

    resetBellFilters();
    currentSampleRate = 0.0;
}

void EqeModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! prepared.load(std::memory_order_acquire))
        return;

    const juce::ScopedLock lock(filterProcessLock);

    if (! prepared.load(std::memory_order_acquire))
        return;

    const auto processChannels = juce::jmin(buffer.getNumChannels(), preparedNumChannels);

    if (eqeFiltersDirty.exchange(false, std::memory_order_acq_rel))
        updateBellFilters();

    const auto bellCount = getActiveBellCount();

    auto processLrmsWrapped = [this, processChannels] (juce::AudioBuffer<float>& targetBuffer,
                                                       const int bandIndex,
                                                       const std::function<void(juce::AudioBuffer<float>&, int)>& processor)
    {
        if (processChannels < 2)
        {
            processor(targetBuffer, processChannels);
            return;
        }

        const auto mode = filterLrmsParams[static_cast<size_t>(bandIndex)] != nullptr
            ? juce::jlimit(0, 4, static_cast<int>(std::lround(filterLrmsParams[static_cast<size_t>(bandIndex)]->load(std::memory_order_relaxed))))
            : 0;

        if (mode == 0)
        {
            processor(targetBuffer, processChannels);
            return;
        }

        auto& workBuffer = lrmsWorkBuffer;
        auto& auxBuffer = lrmsAuxBuffer;

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

            default:
                processor(targetBuffer, processChannels);
                break;
        }
    };

    auto processBandBuffer = [this, &processLrmsWrapped] (juce::AudioBuffer<float>& targetBuffer,
                                                          const int bellIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(bellIndex);
        const auto filterType = getFilterTypeForBand(bandArrayIndex);
        const auto bandBypassed = bellBypassParams[bandArrayIndex] != nullptr
            && bellBypassParams[bandArrayIndex]->load(std::memory_order_relaxed) >= 0.5f;

        if (bandBypassed)
            return;

        const auto slopeDbPerOct = bellSlopeChoiceParams[bandArrayIndex] != nullptr
            ? static_cast<double>(EqeModuleProcessor::getBellSlopeValueForChoiceIndex(bellSlopeChoiceParams[bandArrayIndex]->getIndex()))
            : static_cast<double>(EqeModuleProcessor::fixedSlopeDbPerOct);

        if (filterType == FilterType::bell)
        {
            if (bellSlopeChoiceParams[bandArrayIndex] != nullptr
                && bellSlopeChoiceParams[bandArrayIndex]->getIndex() == 0)
                return;

            if (bellOrderFilters[bandArrayIndex].front().sectionCount <= 0)
                return;

            const auto bellSlopeBlend = mapBellSlopeToBlend(slopeDbPerOct);
            const auto lowerOrder = bellSlopeBlend.lowerOrder;
            const auto upperOrder = bellSlopeBlend.upperOrder;
            const auto blend = bellSlopeBlend.blend;
            auto& bandFilters = bellOrderFilters[bandArrayIndex];

            processLrmsWrapped(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                if (lowerOrder == upperOrder || blend < 1.0e-6)
                {
                    if (lowerOrder > 0)
                        bandFilters[static_cast<size_t>(lowerOrder - 1)].process(targetBufferToProcess, targetChannels);
                    return;
                }

                if (lowerOrder > 0)
                {
                    bellProcessBufferA.makeCopyOf(targetBufferToProcess, true);
                    bandFilters[static_cast<size_t>(lowerOrder - 1)].process(bellProcessBufferA, targetChannels);
                }
                else
                {
                    bellProcessBufferA.makeCopyOf(targetBufferToProcess, true);
                }

                bellProcessBufferB.makeCopyOf(targetBufferToProcess, true);

                if (upperOrder > 0)
                    bandFilters[static_cast<size_t>(upperOrder - 1)].process(bellProcessBufferB, targetChannels);

                for (int channel = 0; channel < targetChannels; ++channel)
                {
                    auto* output = targetBufferToProcess.getWritePointer(channel);
                    const auto* lower = bellProcessBufferA.getReadPointer(channel);
                    const auto* upper = bellProcessBufferB.getReadPointer(channel);

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

            processLrmsWrapped(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                tiltFilters[bandArrayIndex].process(targetBufferToProcess, targetChannels);
            });
            return;
        }

        if (isShelfFilterType(filterType))
        {
            if (shelfOrderFilters[bandArrayIndex].front().stageCount <= 0)
                return;

            processLrmsWrapped(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
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

                bellProcessBufferA.makeCopyOf(targetBufferToProcess, true);
                bellProcessBufferB.makeCopyOf(targetBufferToProcess, true);
                bandFilters[static_cast<size_t>(lowerOrder - 1)].process(bellProcessBufferA, targetChannels);
                bandFilters[static_cast<size_t>(upperOrder - 1)].process(bellProcessBufferB, targetChannels);

                for (int channel = 0; channel < targetChannels; ++channel)
                {
                    auto* output = targetBufferToProcess.getWritePointer(channel);
                    const auto* lower = bellProcessBufferA.getReadPointer(channel);
                    const auto* upper = bellProcessBufferB.getReadPointer(channel);

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

            processLrmsWrapped(targetBuffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBufferToProcess, int targetChannels)
            {
                cutBlendFilters[bandArrayIndex].process(targetBufferToProcess, targetChannels);
            });
        }
    };

    for (int bellIndex = 0; bellIndex < bellCount; ++bellIndex)
        processBandBuffer(buffer, bellIndex);
}

EqeModuleProcessor::FilterType EqeModuleProcessor::getFilterTypeForBand(const size_t bellIndex) const noexcept
{
    if (bellIndex >= filterTypeParams.size() || filterTypeParams[bellIndex] == nullptr)
        return FilterType::bell;

    return filterTypeFromChoiceIndex(static_cast<int>(std::lround(filterTypeParams[bellIndex]->load(std::memory_order_relaxed))));
}

bool EqeModuleProcessor::filterDesignMatches(const size_t bellIndex,
                                             const bool active,
                                             const FilterType type,
                                             const float frequency,
                                             const float bandwidth,
                                             const float slope,
                                             const float gainDb) const noexcept
{
    const auto& cachedState = cachedFilterStates[bellIndex];

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

void EqeModuleProcessor::storeFilterDesignState(const size_t bellIndex,
                                                const bool active,
                                                const FilterType type,
                                                const float frequency,
                                                const float bandwidth,
                                                const float slope,
                                                const float gainDb) noexcept
{
    auto& cachedState = cachedFilterStates[bellIndex];
    cachedState.valid = true;
    cachedState.active = active;
    cachedState.type = type;
    cachedState.frequency = frequency;
    cachedState.bandwidth = bandwidth;
    cachedState.slope = slope;
    cachedState.gainDb = gainDb;
    cachedState.sampleRate = currentSampleRate;
}
