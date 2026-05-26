#include "module.eqe.ProcessorSupport.h"

#include <array>
#include <cmath>
#include <functional>
#include <limits>

void EqeModuleProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lastProcessedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedNumChannels = static_cast<int>(maxSupportedChannels);
    bellProcessBufferA.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    bellProcessBufferB.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    lrmsWorkBuffer.setSize(1, juce::jmax(1, samplesPerBlock));
    lrmsAuxBuffer.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    ttssTransientBuffer.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    ttssSustainBuffer.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));

    resetBellFilters();
    resetTtssSplitState();
    markEqeFiltersDirty();
    updateBellFilters();
}

void EqeModuleProcessor::releaseResources()
{
    resetBellFilters();
    resetTtssSplitState();
}

void EqeModuleProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    const auto bypass = bypassParam != nullptr
        && bypassParam->load(std::memory_order_relaxed) >= 0.5f;
    const auto bypassWithGain = bypassWithGainParam != nullptr
        && bypassWithGainParam->load(std::memory_order_relaxed) >= 0.5f;

    if (bypass)
    {
        resetTtssSplitState();
        return;
    }

    const auto processChannels = juce::jmin(buffer.getNumChannels(), preparedNumChannels);

    if (processChannels > 0)
    {
        const auto wideAmount = wideParam != nullptr
            ? juce::jlimit(0.0f, 4.0f, wideParam->load(std::memory_order_relaxed) / 100.0f)
            : 1.0f;

        if (processChannels > 1 && std::abs(wideAmount - 1.0f) > 1.0e-6f)
        {
            const auto numSamples = buffer.getNumSamples();
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);

            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                const auto mid = 0.5f * (left[sampleIndex] + right[sampleIndex]);
                const auto side = 0.5f * (left[sampleIndex] - right[sampleIndex]) * wideAmount;
                left[sampleIndex] = mid + side;
                right[sampleIndex] = mid - side;
            }
        }

        const auto gainLDb = inGainLParam != nullptr ? inGainLParam->load(std::memory_order_relaxed) : 0.0f;
        const auto gainRDb = inGainRParam != nullptr ? inGainRParam->load(std::memory_order_relaxed) : 0.0f;
        const auto gainLrDb = inGainLrParam != nullptr ? inGainLrParam->load(std::memory_order_relaxed) : 0.0f;

        const auto effectiveGainLDb = gainLDb + gainLrDb;
        const auto effectiveGainRDb = gainRDb + gainLrDb;

        if (std::abs(effectiveGainLDb) > 1.0e-6f)
            buffer.applyGain(0, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveGainLDb));

        if (processChannels > 1 && std::abs(effectiveGainRDb) > 1.0e-6f)
            buffer.applyGain(1, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(effectiveGainRDb));
    }

    if (bypassWithGain)
    {
        const auto outGainDb = outGainParam != nullptr ? outGainParam->load(std::memory_order_relaxed) : 0.0f;

        if (std::abs(outGainDb) > 1.0e-6f)
            buffer.applyGain(juce::Decibels::decibelsToGain(outGainDb));

        resetTtssSplitState();
        return;
    }

    if (eqeFiltersDirty.exchange(false, std::memory_order_acq_rel))
        updateBellFilters();

    const auto bellCount = getActiveBellCount();
    auto* splitSource = transientSplitProvider;
    const auto filterTargetsTtssSplit = [this] (const int bellIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(bellIndex);

        if (bellBypassParams[bandArrayIndex] != nullptr
            && bellBypassParams[bandArrayIndex]->load(std::memory_order_relaxed) >= 0.5f)
            return false;

        const auto ttssMode = filterTtssParams[bandArrayIndex] != nullptr
            ? ttssModeFromChoiceIndex(static_cast<int>(std::lround(filterTtssParams[bandArrayIndex]->load(std::memory_order_relaxed))))
            : TtssMode::ts;

        return ttssMode == TtssMode::tt || ttssMode == TtssMode::ss;
    };

    auto anyFilterTargetsTtssSplit = false;

    for (int bellIndex = 0; bellIndex < bellCount; ++bellIndex)
    {
        if (filterTargetsTtssSplit(bellIndex))
        {
            anyFilterTargetsTtssSplit = true;
            break;
        }
    }

    const auto ttssSourceAvailable = anyFilterTargetsTtssSplit
        && splitSource != nullptr
        && splitSource->canProvideSplit();

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

    auto processBand = [this, &buffer, &processBandBuffer, ttssSourceAvailable] (const int bellIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(bellIndex);
        const auto ttssMode = filterTtssParams[bandArrayIndex] != nullptr
            ? ttssModeFromChoiceIndex(static_cast<int>(std::lround(filterTtssParams[bandArrayIndex]->load(std::memory_order_relaxed))))
            : TtssMode::ts;

        if (! ttssSourceAvailable)
        {
            processBandBuffer(buffer, bellIndex);
            return;
        }

        if (ttssMode == TtssMode::tt)
            processBandBuffer(ttssTransientBuffer, bellIndex);
        else if (ttssMode == TtssMode::ss)
            processBandBuffer(ttssSustainBuffer, bellIndex);
        else
        {
            processBandBuffer(ttssTransientBuffer, bellIndex);
            processBandBuffer(ttssSustainBuffer, bellIndex);
        }
    };

    auto ensureTtssSplitBuffersReady = [this, processChannels, numSamples = buffer.getNumSamples()]() -> bool
    {
        if (processChannels <= 0 || numSamples <= 0)
            return false;

        if (ttssTransientBuffer.getNumChannels() < processChannels || ttssTransientBuffer.getNumSamples() < numSamples)
            ttssTransientBuffer.setSize(processChannels, numSamples, false, false, true);

        if (ttssSustainBuffer.getNumChannels() < processChannels || ttssSustainBuffer.getNumSamples() < numSamples)
            ttssSustainBuffer.setSize(processChannels, numSamples, false, false, true);

        return ttssTransientBuffer.getNumChannels() >= processChannels
            && ttssTransientBuffer.getNumSamples() >= numSamples
            && ttssSustainBuffer.getNumChannels() >= processChannels
            && ttssSustainBuffer.getNumSamples() >= numSamples;
    };

    auto buildDetectorSplitBuffers = [this, &buffer, processChannels] (const TransientSplitSettings& settings)
    {
        ttssTransientBuffer.clear();
        ttssSustainBuffer.clear();

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            auto detectorLevel = 0.0f;

            for (int channel = 0; channel < processChannels; ++channel)
                detectorLevel = juce::jmax(detectorLevel, std::abs(buffer.getSample(channel, sampleIndex)));

            const auto transientAmount = processTtssDetectorSample(detectorLevel, settings);
            const auto sustainAmount = 1.0f - transientAmount;

            for (int channel = 0; channel < processChannels; ++channel)
            {
                const auto sample = buffer.getSample(channel, sampleIndex);
                ttssTransientBuffer.setSample(channel, sampleIndex, sample * transientAmount);
                ttssSustainBuffer.setSample(channel, sampleIndex, sample * sustainAmount);
            }
        }
    };

    auto canUseTtssSplit = false;

    if (ttssSourceAvailable && ensureTtssSplitBuffersReady())
    {
        buildDetectorSplitBuffers(splitSource->getSplitSettings());
        canUseTtssSplit = true;
    }
    else
    {
        resetTtssSplitState();
    }

    for (int bellIndex = 0; bellIndex < bellCount; ++bellIndex)
    {
        if (! canUseTtssSplit)
        {
            processBandBuffer(buffer, bellIndex);
            continue;
        }

        processBand(bellIndex);
    }

    if (canUseTtssSplit)
    {
        for (int channel = 0; channel < processChannels; ++channel)
        {
            auto* output = buffer.getWritePointer(channel);
            const auto* transientRead = ttssTransientBuffer.getReadPointer(channel);
            const auto* sustainRead = ttssSustainBuffer.getReadPointer(channel);

            for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
                output[sampleIndex] = transientRead[sampleIndex] + sustainRead[sampleIndex];
        }
    }

    const auto outGainDb = outGainParam != nullptr ? outGainParam->load(std::memory_order_relaxed) : 0.0f;

    if (std::abs(outGainDb) > 1.0e-6f)
        buffer.applyGain(juce::Decibels::decibelsToGain(outGainDb));
}

bool EqeModuleProcessor::isModuleBypassed() const noexcept
{
    const auto bypass = bypassParam != nullptr
        && bypassParam->load(std::memory_order_relaxed) >= 0.5f;
    return bypass;
}

EqeModuleProcessor::FilterType EqeModuleProcessor::getFilterTypeForBand(const size_t bellIndex) const noexcept
{
    if (bellIndex >= filterTypeParams.size() || filterTypeParams[bellIndex] == nullptr)
        return FilterType::bell;

    return filterTypeFromChoiceIndex(static_cast<int>(std::lround(filterTypeParams[bellIndex]->load(std::memory_order_relaxed))));
}

EqeModuleProcessor::TtssMode EqeModuleProcessor::ttssModeFromChoiceIndex(const int choiceIndex) noexcept
{
    switch (juce::jlimit(0, 2, choiceIndex))
    {
        case 1: return TtssMode::tt;
        case 2: return TtssMode::ss;
        case 0:
        default: return TtssMode::ts;
    }
}

void EqeModuleProcessor::resetTtssSplitState() noexcept
{
    ttssSplitState = {};
    ttssSplitState.samplesSinceTrigger = std::numeric_limits<int>::max() / 2;
}

float EqeModuleProcessor::makeTtssReleaseProgress(const float progress, const float curveAmount) noexcept
{
    const auto normalizedCurve = juce::jlimit(-1.0f, 1.0f, curveAmount * 0.01f);
    return normalizedCurve >= 0.0f
        ? std::pow(progress, 1.0f + (normalizedCurve * 3.0f))
        : 1.0f - std::pow(1.0f - progress, 1.0f + (-normalizedCurve * 3.0f));
}

float EqeModuleProcessor::processTtssDetectorSample(const float level,
                                                    const TransientSplitSettings& settings) noexcept
{
    const auto calculateThresholdAmount = [] (const float levelDb, const float thresholdDb, const float kneeDb) noexcept
    {
        if (kneeDb <= 0.0f)
            return levelDb >= thresholdDb ? 1.0f : 0.0f;

        const auto kneeStartDb = thresholdDb - kneeDb;

        if (levelDb <= kneeStartDb)
            return 0.0f;

        if (levelDb >= thresholdDb)
            return 1.0f;

        const auto normalized = juce::jlimit(0.0f, 1.0f, (levelDb - kneeStartDb) / kneeDb);
        return normalized * normalized * (3.0f - (2.0f * normalized));
    };

    const auto makeReleaseCoefficient = [this] (const float timeMs) noexcept
    {
        const auto timeSeconds = juce::jmax(0.001f, timeMs * 0.001f);
        const auto samples = juce::jmax(1.0, static_cast<double>(timeSeconds) * currentSampleRate);
        return static_cast<float>(std::exp(-1.0 / samples));
    };

    const auto fastReleaseCoefficient = makeReleaseCoefficient(5.0f);
    const auto bodyAttackCoefficient = makeReleaseCoefficient(25.0f);
    const auto bodyReleaseCoefficient = makeReleaseCoefficient(juce::jmax(50.0f, settings.holdMs + settings.releaseMs));

    ttssSplitState.fastEnvelope = level >= ttssSplitState.fastEnvelope
        ? level
        : level + ((ttssSplitState.fastEnvelope - level) * fastReleaseCoefficient);

    const auto bodyCoefficient = level >= ttssSplitState.bodyEnvelope ? bodyAttackCoefficient
                                                                       : bodyReleaseCoefficient;
    ttssSplitState.bodyEnvelope = level + ((ttssSplitState.bodyEnvelope - level) * bodyCoefficient);

    const auto levelDb = juce::Decibels::gainToDecibels(ttssSplitState.fastEnvelope, -120.0f);
    const auto bodyDb = juce::Decibels::gainToDecibels(ttssSplitState.bodyEnvelope, -120.0f);
    const auto onsetDb = levelDb - bodyDb;
    const auto thresholdAmount = calculateThresholdAmount(levelDb, settings.thresholdDb, settings.kneeDb);
    const auto aboveThreshold = thresholdAmount > 1.0e-4f;
    const auto thresholdRisingEdge = aboveThreshold && ! ttssSplitState.wasAboveThreshold;
    const auto holdSamples = juce::jmax(0, static_cast<int>(std::round(settings.holdMs * 0.001 * currentSampleRate)));
    const auto retriggerSamples = juce::jmax(0, static_cast<int>(std::round(settings.retriggerMs * 0.001 * currentSampleRate)));
    const auto retriggerElapsed = ttssSplitState.samplesSinceTrigger >= holdSamples + retriggerSamples;
    const auto shouldTrigger = aboveThreshold
        && retriggerElapsed
        && (thresholdRisingEdge || onsetDb >= 6.0f || retriggerSamples > 0);

    if (shouldTrigger)
    {
        ttssSplitState.heldTransientAmount = thresholdAmount;
        ttssSplitState.transientEnvelope = juce::jmax(ttssSplitState.transientEnvelope, ttssSplitState.heldTransientAmount);
        ttssSplitState.releaseStartAmount = ttssSplitState.transientEnvelope;
        ttssSplitState.holdSamplesRemaining = holdSamples;
        ttssSplitState.releaseSamplesRemaining = 0;
        ttssSplitState.releaseSamplesTotal = 0;
        ttssSplitState.samplesSinceTrigger = 0;
    }
    else
    {
        ttssSplitState.samplesSinceTrigger = ttssSplitState.samplesSinceTrigger < (std::numeric_limits<int>::max() / 4)
            ? ttssSplitState.samplesSinceTrigger + 1
            : ttssSplitState.samplesSinceTrigger;

        if (ttssSplitState.holdSamplesRemaining > 0)
        {
            --ttssSplitState.holdSamplesRemaining;
            ttssSplitState.heldTransientAmount = juce::jmax(ttssSplitState.heldTransientAmount, thresholdAmount);
            ttssSplitState.transientEnvelope = ttssSplitState.heldTransientAmount;
            ttssSplitState.releaseStartAmount = ttssSplitState.transientEnvelope;
            ttssSplitState.releaseSamplesRemaining = 0;
            ttssSplitState.releaseSamplesTotal = 0;
        }
        else
        {
            if (ttssSplitState.releaseSamplesTotal <= 0)
            {
                ttssSplitState.releaseStartAmount = ttssSplitState.transientEnvelope;
                ttssSplitState.releaseSamplesTotal = juce::jmax(1,
                                                                static_cast<int>(std::round(juce::jmax(1.0f, settings.releaseMs)
                                                                                            * 0.001
                                                                                            * currentSampleRate)));
                ttssSplitState.releaseSamplesRemaining = ttssSplitState.releaseSamplesTotal;
            }

            if (ttssSplitState.releaseSamplesRemaining > 0)
            {
                const auto completedSamples = ttssSplitState.releaseSamplesTotal - ttssSplitState.releaseSamplesRemaining + 1;
                const auto progress = juce::jlimit(0.0f,
                                                   1.0f,
                                                   static_cast<float>(completedSamples) / static_cast<float>(ttssSplitState.releaseSamplesTotal));
                const auto shapedProgress = makeTtssReleaseProgress(progress, settings.releaseCurve);
                ttssSplitState.transientEnvelope = ttssSplitState.releaseStartAmount * (1.0f - shapedProgress);
                --ttssSplitState.releaseSamplesRemaining;
            }
            else
            {
                ttssSplitState.transientEnvelope = 0.0f;
            }
        }
    }

    if (ttssSplitState.transientEnvelope < 1.0e-4f)
    {
        ttssSplitState.transientEnvelope = 0.0f;
        ttssSplitState.heldTransientAmount = 0.0f;
    }

    ttssSplitState.wasAboveThreshold = aboveThreshold;
    return juce::jlimit(0.0f, 1.0f, ttssSplitState.transientEnvelope);
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

void EqeModuleProcessor::setTransientSplitProvider(TransientSplitProvider* provider) noexcept
{
    transientSplitProvider = provider;
}
