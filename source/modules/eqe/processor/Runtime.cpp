#include "Editor.h"
#include "ProcessorSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

void EqeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lastProcessedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedNumChannels = juce::jlimit(1, static_cast<int>(maxSupportedChannels), getTotalNumOutputChannels());
    bellProcessBufferA.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));
    bellProcessBufferB.setSize(preparedNumChannels, juce::jmax(1, samplesPerBlock));

    resetBellFilters();
    updateBellFilters();
}

void EqeAudioProcessor::releaseResources()
{
    resetBellFilters();
}

bool EqeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    return mainInput == juce::AudioChannelSet::mono()
        || mainInput == juce::AudioChannelSet::stereo();
}

void EqeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    lastProcessedBlockSize = juce::jmax(1, buffer.getNumSamples());

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (! isEqeModuleLoaded())
    {
        globalClipIndicator.store(0.0f, std::memory_order_relaxed);
        return;
    }

    if (globalBypassParam != nullptr
        && globalBypassParam->load(std::memory_order_relaxed) >= 0.5f)
    {
        globalClipIndicator.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const auto bypassExceptOutGain = globalBypassOutGainOnlyParam != nullptr
        && globalBypassOutGainOnlyParam->load(std::memory_order_relaxed) >= 0.5f;

    if (! bypassExceptOutGain)
        updateBellFilters();

    const auto processChannels = juce::jmin(buffer.getNumChannels(), preparedNumChannels);
    const auto bellCount = getActiveBellCount();
    const auto wideAmount = globalWideParam != nullptr
        ? juce::jlimit(0.0f, 4.0f, globalWideParam->load(std::memory_order_relaxed) / 100.0f)
        : 1.0f;

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

        juce::AudioBuffer<float> workBuffer;
        juce::AudioBuffer<float> auxBuffer;
        workBuffer.setSize(1, targetBuffer.getNumSamples());
        auxBuffer.setSize(1, targetBuffer.getNumSamples());

        auto copyChannel = [&] (const int sourceChannel, juce::AudioBuffer<float>& destination)
        {
            destination.setSize(1, targetBuffer.getNumSamples(), false, false, true);
            destination.copyFrom(0, 0, targetBuffer, sourceChannel, 0, targetBuffer.getNumSamples());
        };

        switch (mode)
        {
            case 1:
                copyChannel(0, workBuffer);
                processor(workBuffer, 1);
                targetBuffer.copyFrom(0, 0, workBuffer, 0, 0, targetBuffer.getNumSamples());
                break;

            case 2:
                copyChannel(1, workBuffer);
                processor(workBuffer, 1);
                targetBuffer.copyFrom(1, 0, workBuffer, 0, 0, targetBuffer.getNumSamples());
                break;

            case 3:
                auxBuffer.makeCopyOf(targetBuffer, true);
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
                auxBuffer.makeCopyOf(targetBuffer, true);
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

    auto processBand = [this, &buffer, &processLrmsWrapped] (const int bellIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(bellIndex);
        const auto filterType = getFilterTypeForBand(bandArrayIndex);
        const auto bandBypassed = bellBypassParams[bandArrayIndex] != nullptr
            && bellBypassParams[bandArrayIndex]->load(std::memory_order_relaxed) >= 0.5f;

        if (bandBypassed)
            return;

        const auto slopeDbPerOct = bellSlopeChoiceParams[bandArrayIndex] != nullptr
            ? static_cast<double>(EqeAudioProcessor::getBellSlopeValueForChoiceIndex(bellSlopeChoiceParams[bandArrayIndex]->getIndex()))
            : static_cast<double>(EqeAudioProcessor::fixedSlopeDbPerOct);

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

            processLrmsWrapped(buffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBuffer, int targetChannels)
            {
                if (lowerOrder == upperOrder || blend < 1.0e-6)
                {
                    if (lowerOrder > 0)
                        bandFilters[static_cast<size_t>(lowerOrder - 1)].process(targetBuffer, targetChannels);
                    return;
                }

                if (lowerOrder > 0)
                {
                    bellProcessBufferA.makeCopyOf(targetBuffer, true);
                    bandFilters[static_cast<size_t>(lowerOrder - 1)].process(bellProcessBufferA, targetChannels);
                }
                else
                {
                    bellProcessBufferA.makeCopyOf(targetBuffer, true);
                }

                bellProcessBufferB.makeCopyOf(targetBuffer, true);
                if (upperOrder > 0)
                    bandFilters[static_cast<size_t>(upperOrder - 1)].process(bellProcessBufferB, targetChannels);

                for (int channel = 0; channel < targetChannels; ++channel)
                {
                    auto* output = targetBuffer.getWritePointer(channel);
                    const auto* lower = bellProcessBufferA.getReadPointer(channel);
                    const auto* upper = bellProcessBufferB.getReadPointer(channel);

                    for (int sampleIndex = 0; sampleIndex < targetBuffer.getNumSamples(); ++sampleIndex)
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

            processLrmsWrapped(buffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBuffer, int targetChannels)
            {
                tiltFilters[bandArrayIndex].process(targetBuffer, targetChannels);
            });
            return;
        }

        if (isShelfFilterType(filterType))
        {
            if (shelfOrderFilters[bandArrayIndex].front().stageCount <= 0)
                return;

            processLrmsWrapped(buffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBuffer, int targetChannels)
            {
                auto& bandFilters = shelfOrderFilters[bandArrayIndex];
                const auto slopeBlend = mapShelfSlopeToBlend(slopeDbPerOct);
                const auto lowerOrder = slopeBlend.lowerOrder;
                const auto upperOrder = slopeBlend.upperOrder;
                const auto blend = slopeBlend.blend;

                if (lowerOrder == upperOrder || blend < 1.0e-6)
                {
                    bandFilters[static_cast<size_t>(lowerOrder - 1)].process(targetBuffer, targetChannels);
                    return;
                }

                bellProcessBufferA.makeCopyOf(targetBuffer, true);
                bellProcessBufferB.makeCopyOf(targetBuffer, true);
                bandFilters[static_cast<size_t>(lowerOrder - 1)].process(bellProcessBufferA, targetChannels);
                bandFilters[static_cast<size_t>(upperOrder - 1)].process(bellProcessBufferB, targetChannels);

                for (int channel = 0; channel < targetChannels; ++channel)
                {
                    auto* output = targetBuffer.getWritePointer(channel);
                    const auto* lower = bellProcessBufferA.getReadPointer(channel);
                    const auto* upper = bellProcessBufferB.getReadPointer(channel);

                    for (int sampleIndex = 0; sampleIndex < targetBuffer.getNumSamples(); ++sampleIndex)
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

            processLrmsWrapped(buffer, static_cast<int>(bandArrayIndex), [&] (juce::AudioBuffer<float>& targetBuffer, int targetChannels)
            {
                cutBlendFilters[bandArrayIndex].process(targetBuffer, targetChannels);
            });
            return;
        }
    };

    if (! bypassExceptOutGain)
    {
        for (int bellIndex = 0; bellIndex < bellCount; ++bellIndex)
            processBand(bellIndex);

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

        const auto gainLDb = globalGainLParam != nullptr ? globalGainLParam->load(std::memory_order_relaxed) : 0.0f;
        const auto gainRDb = globalGainRParam != nullptr ? globalGainRParam->load(std::memory_order_relaxed) : 0.0f;

        if (processChannels > 0 && std::abs(gainLDb) > 1.0e-6f)
            buffer.applyGain(0, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(gainLDb));

        if (processChannels > 1 && std::abs(gainRDb) > 1.0e-6f)
            buffer.applyGain(1, 0, buffer.getNumSamples(), juce::Decibels::decibelsToGain(gainRDb));
    }

    const auto outGainDb = outGainParam != nullptr ? outGainParam->load(std::memory_order_relaxed) : 0.0f;

    if (std::abs(outGainDb) > 1.0e-6f)
        buffer.applyGain(juce::Decibels::decibelsToGain(outGainDb));

    auto clipped = false;

    for (int channel = 0; channel < processChannels && ! clipped; ++channel)
        clipped = buffer.getMagnitude(channel, 0, buffer.getNumSamples()) >= 1.0f;

    if (clipped)
        globalClipIndicator.store(1.0f, std::memory_order_relaxed);
}

#if ! JUCE_IOS
void EqeAudioProcessor::copyVisualiserResponse(std::array<float, visualizerScopeSize>& stereoDb,
                                               std::array<float, visualizerScopeSize>& leftDb,
                                               std::array<float, visualizerScopeSize>& rightDb,
                                               std::array<float, visualizerScopeSize>& midDb,
                                               std::array<float, visualizerScopeSize>& sideDb,
                                               double& sampleRateOut) noexcept
{
    sampleRateOut = currentSampleRate;
    stereoDb.fill(0.0f);
    leftDb.fill(0.0f);
    rightDb.fill(0.0f);
    midDb.fill(0.0f);
    sideDb.fill(0.0f);

    if (currentSampleRate <= 0.0)
        return;

    const auto nyquist = (currentSampleRate * 0.5) * nyquistSafetyFactor;
    const auto activeCount = getActiveBellCount();
    const auto bypassAll = globalBypassParam != nullptr
        && globalBypassParam->load(std::memory_order_relaxed) >= 0.5f;
    const auto bypassExceptOutGain = globalBypassOutGainOnlyParam != nullptr
        && globalBypassOutGainOnlyParam->load(std::memory_order_relaxed) >= 0.5f;
    const auto globalGainL = globalGainLParam != nullptr
        ? juce::Decibels::decibelsToGain(static_cast<double>(globalGainLParam->load(std::memory_order_relaxed)))
        : 1.0;
    const auto globalGainR = globalGainRParam != nullptr
        ? juce::Decibels::decibelsToGain(static_cast<double>(globalGainRParam->load(std::memory_order_relaxed)))
        : 1.0;
    const auto outGain = outGainParam != nullptr
        ? juce::Decibels::decibelsToGain(static_cast<double>(outGainParam->load(std::memory_order_relaxed)))
        : 1.0;
    const auto wideAmount = globalWideParam != nullptr
        ? juce::jlimit(0.0f, 4.0f, globalWideParam->load(std::memory_order_relaxed) / 100.0f)
        : 1.0f;

    struct DisplayBandState
    {
        bool active = false;
        int lrmsMode = 0;
        FilterType type = FilterType::bell;
        BellOrderFilter bellLower;
        BellOrderFilter bellUpper;
        double bellBlend = 0.0;
        BiquadCascade cascade;
    };

    std::array<DisplayBandState, maxBellFilterCount> bandStates {};

    if (! bypassAll && ! bypassExceptOutGain)
    {
        for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
        {
            const auto bandArrayIndex = static_cast<size_t>(bellIndex);
            const auto bandBypassed = bellBypassParams[bandArrayIndex] != nullptr
                && bellBypassParams[bandArrayIndex]->load(std::memory_order_relaxed) >= 0.5f;

            if (bandBypassed)
                continue;

            auto& bandState = bandStates[bandArrayIndex];
            bandState.lrmsMode = filterLrmsParams[bandArrayIndex] != nullptr
                ? juce::jlimit(0, 4, static_cast<int>(std::lround(filterLrmsParams[bandArrayIndex]->load(std::memory_order_relaxed))))
                : 0;
            bandState.type = getFilterTypeForBand(bandArrayIndex);

            const auto frequency = bellFrequencyParams[bandArrayIndex] != nullptr
                ? juce::jlimit(minimumVisibleFilterFrequency,
                               maximumVisibleFilterFrequency,
                               bellFrequencyParams[bandArrayIndex]->load(std::memory_order_relaxed))
                : defaultTiltFrequency;
            const auto designFrequency = computeDesignFrequency(static_cast<double>(frequency), currentSampleRate);
            const auto slope = bellSlopeChoiceParams[bandArrayIndex] != nullptr
                ? static_cast<double>(EqeAudioProcessor::getBellSlopeValueForChoiceIndex(bellSlopeChoiceParams[bandArrayIndex]->getIndex()))
                : static_cast<double>(fixedSlopeDbPerOct);
            const auto bandwidth = bellBandwidthParams[bandArrayIndex] != nullptr
                ? static_cast<double>(juce::jlimit(minimumBellBandwidth,
                                                   maximumBellBandwidth,
                                                   bellBandwidthParams[bandArrayIndex]->load(std::memory_order_relaxed)))
                : 1.0;
            const auto gainDb = bellGainParams[bandArrayIndex] != nullptr
                ? static_cast<double>(juce::jlimit(-48.0f,
                                                   48.0f,
                                                   bellGainParams[bandArrayIndex]->load(std::memory_order_relaxed)))
                : 0.0;

            if (bandState.type == FilterType::bell)
            {
                if (bellSlopeChoiceParams[bandArrayIndex] != nullptr
                    && bellSlopeChoiceParams[bandArrayIndex]->getIndex() == 0)
                    continue;

                if (std::abs(gainDb) < 1.0e-6)
                    continue;

                const auto bellSlopeBlend = mapBellSlopeToBlend(slope);
                bandState.bellBlend = bellSlopeBlend.blend;

                if (bellSlopeBlend.lowerOrder > 0)
                    updateBellOrderFilter(bandState.bellLower,
                                          bellSlopeBlend.lowerOrder,
                                          designFrequency,
                                          bandwidth,
                                          juce::Decibels::decibelsToGain(gainDb));

                if (bellSlopeBlend.upperOrder > 0)
                    updateBellOrderFilter(bandState.bellUpper,
                                          bellSlopeBlend.upperOrder,
                                          designFrequency,
                                          bandwidth,
                                          juce::Decibels::decibelsToGain(gainDb));

                bandState.active = true;
                continue;
            }

            if (std::abs(gainDb) < 1.0e-6 && ! isCutFilterType(bandState.type))
                continue;

            if (isTiltFilterType(bandState.type))
            {
                updateTiltFilter(bandState.cascade, designFrequency, gainDb);
                bandState.active = bandState.cascade.stageCount > 0;
                continue;
            }

            if (isShelfFilterType(bandState.type))
            {
                const auto shelfSlopeBlend = mapShelfSlopeToBlend(slope);
                BiquadCascade lowerFilter;
                BiquadCascade upperFilter;

                updateShelfOrderFilterRaw(lowerFilter,
                                          bandState.type,
                                          shelfSlopeBlend.lowerOrder,
                                          designFrequency,
                                          bandwidth,
                                          juce::Decibels::decibelsToGain(gainDb));
                updateShelfOrderFilterRaw(upperFilter,
                                          bandState.type,
                                          shelfSlopeBlend.upperOrder,
                                          designFrequency,
                                          bandwidth,
                                          juce::Decibels::decibelsToGain(gainDb));
                updateInterpolatedCascadeFilter(bandState.cascade,
                                                lowerFilter,
                                                upperFilter,
                                                shelfSlopeBlend.blend);
                bandState.active = bandState.cascade.stageCount > 0;
                continue;
            }

            if (isCutFilterType(bandState.type))
            {
                buildCutBlendFilter(bandState.cascade,
                                    bandState.type,
                                    designFrequency,
                                    bandwidth,
                                    slope);
                bandState.active = bandState.cascade.stageCount > 0;
            }
        }
    }

    for (size_t pointIndex = 0; pointIndex < stereoDb.size(); ++pointIndex)
    {
        const auto denominator = stereoDb.size() > 1 ? static_cast<double>(stereoDb.size() - 1) : 1.0;
        const auto proportion = static_cast<double>(pointIndex) / denominator;
        const auto frequency = juce::mapToLog10(static_cast<float>(proportion),
                                                minimumVisibleFilterFrequency,
                                                static_cast<float>(nyquist));

        std::complex<double> matrixLL(1.0, 0.0);
        std::complex<double> matrixLR(0.0, 0.0);
        std::complex<double> matrixRL(0.0, 0.0);
        std::complex<double> matrixRR(1.0, 0.0);

        for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
        {
            const auto& bandState = bandStates[static_cast<size_t>(bellIndex)];

            if (! bandState.active)
                continue;

            std::complex<double> response(1.0, 0.0);

            if (bandState.type == FilterType::bell)
            {
                const auto lowerResponse = evaluateBellResponseAt(bandState.bellLower, frequency);
                const auto upperResponse = evaluateBellResponseAt(bandState.bellUpper, frequency);
                response = lowerResponse + ((upperResponse - lowerResponse) * bandState.bellBlend);
            }
            else
            {
                response = evaluateCascadeResponseAt(bandState.cascade, frequency);
            }

            switch (bandState.lrmsMode)
            {
                case 1:
                    matrixLL *= response;
                    matrixLR *= response;
                    break;

                case 2:
                    matrixRL *= response;
                    matrixRR *= response;
                    break;

                case 3:
                {
                    const auto halfSum = 0.5 * (response + 1.0);
                    const auto halfDiff = 0.5 * (response - 1.0);
                    const auto nextLL = (halfSum * matrixLL) + (halfDiff * matrixRL);
                    const auto nextLR = (halfSum * matrixLR) + (halfDiff * matrixRR);
                    const auto nextRL = (halfDiff * matrixLL) + (halfSum * matrixRL);
                    const auto nextRR = (halfDiff * matrixLR) + (halfSum * matrixRR);
                    matrixLL = nextLL;
                    matrixLR = nextLR;
                    matrixRL = nextRL;
                    matrixRR = nextRR;
                    break;
                }

                case 4:
                {
                    const auto halfSum = 0.5 * (response + 1.0);
                    const auto halfDiff = 0.5 * (1.0 - response);
                    const auto nextLL = (halfSum * matrixLL) + (halfDiff * matrixRL);
                    const auto nextLR = (halfSum * matrixLR) + (halfDiff * matrixRR);
                    const auto nextRL = (halfDiff * matrixLL) + (halfSum * matrixRL);
                    const auto nextRR = (halfDiff * matrixLR) + (halfSum * matrixRR);
                    matrixLL = nextLL;
                    matrixLR = nextLR;
                    matrixRL = nextRL;
                    matrixRR = nextRR;
                    break;
                }

                default:
                    matrixLL *= response;
                    matrixLR *= response;
                    matrixRL *= response;
                    matrixRR *= response;
                    break;
            }
        }

        if (! bypassAll && ! bypassExceptOutGain)
        {
            if (std::abs(wideAmount - 1.0f) > 1.0e-6f)
            {
                const auto halfSum = 0.5 * (1.0 + static_cast<double>(wideAmount));
                const auto halfDiff = 0.5 * (1.0 - static_cast<double>(wideAmount));
                const auto nextLL = (halfSum * matrixLL) + (halfDiff * matrixRL);
                const auto nextLR = (halfSum * matrixLR) + (halfDiff * matrixRR);
                const auto nextRL = (halfDiff * matrixLL) + (halfSum * matrixRL);
                const auto nextRR = (halfDiff * matrixLR) + (halfSum * matrixRR);
                matrixLL = nextLL;
                matrixLR = nextLR;
                matrixRL = nextRL;
                matrixRR = nextRR;
            }

            matrixLL *= (globalGainL * outGain);
            matrixLR *= (globalGainL * outGain);
            matrixRL *= (globalGainR * outGain);
            matrixRR *= (globalGainR * outGain);
        }
        else if (! bypassAll)
        {
            matrixLL *= outGain;
            matrixLR *= outGain;
            matrixRL *= outGain;
            matrixRR *= outGain;
        }

        const auto matrixMid = 0.5 * (matrixLL + matrixLR + matrixRL + matrixRR);
        const auto matrixSide = 0.5 * (matrixLL - matrixLR - matrixRL + matrixRR);
        const auto leftMagnitude = static_cast<float>(std::sqrt(std::norm(matrixLL)
                                                                + std::norm(matrixLR)));
        const auto rightMagnitude = static_cast<float>(std::sqrt(std::norm(matrixRL)
                                                                 + std::norm(matrixRR)));
        const auto stereoMagnitude = static_cast<float>(std::sqrt((std::norm(matrixLL)
                                                                   + std::norm(matrixLR)
                                                                   + std::norm(matrixRL)
                                                                   + std::norm(matrixRR)) * 0.5));

        stereoDb[pointIndex] = juce::Decibels::gainToDecibels(stereoMagnitude, -120.0f);
        leftDb[pointIndex] = juce::Decibels::gainToDecibels(leftMagnitude, -120.0f);
        rightDb[pointIndex] = juce::Decibels::gainToDecibels(rightMagnitude, -120.0f);
        midDb[pointIndex] = juce::Decibels::gainToDecibels(static_cast<float>(std::abs(matrixMid)), -120.0f);
        sideDb[pointIndex] = juce::Decibels::gainToDecibels(static_cast<float>(std::abs(matrixSide)), -120.0f);
    }
}
#endif

bool EqeAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EqeAudioProcessor::createEditor()
{
    return new EqeAudioProcessorEditor(*this);
}

bool EqeAudioProcessor::addBellFilter() noexcept
{
    const auto currentCount = getActiveBellCount();

    if (currentCount >= maxBellFilterCount)
        return false;

    auto setParameterValue = [this] (const juce::String& parameterId, const float value)
    {
        auto* parameter = parameters.getParameter(parameterId);

        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    };

    const auto newIndex = currentCount;
    constexpr auto defaultType = FilterType::bell;
    setParameterValue(getFilterTypeParamId(newIndex), static_cast<float>(choiceIndexFromFilterType(defaultType)));
    setParameterValue(getFilterLrmsParamId(newIndex), 0.0f);
    setParameterValue(getBellFrequencyParamId(newIndex), defaultFilterFrequencyForType(defaultType));
    setParameterValue(getBellBandwidthParamId(newIndex), defaultFilterBandwidthForType(defaultType));
    setParameterValue(getBellSlopeParamId(newIndex), static_cast<float>(EqeAudioProcessor::getBellSlopeChoiceIndexForValue(defaultFilterSlopeForType(defaultType))));
    setParameterValue(getBellGainParamId(newIndex), 0.0f);
    setParameterValue(getBellBypassParamId(newIndex), 0.0f);
    setActiveBellCount(currentCount + 1);
    return true;
}

bool EqeAudioProcessor::removeBellFilter(const int bellIndex) noexcept
{
    const auto currentCount = getActiveBellCount();

    if (currentCount <= 0 || bellIndex < 0 || bellIndex >= currentCount)
        return false;

    auto copyParameterValue = [this] (const juce::String& destinationParameterId,
                                      const juce::String& sourceParameterId)
    {
        auto* destinationParameter = parameters.getParameter(destinationParameterId);
        auto* sourceParameter = parameters.getParameter(sourceParameterId);

        if (destinationParameter == nullptr || sourceParameter == nullptr)
            return;

        const auto sourceValue = sourceParameter->convertFrom0to1(sourceParameter->getValue());
        destinationParameter->beginChangeGesture();
        destinationParameter->setValueNotifyingHost(destinationParameter->convertTo0to1(sourceValue));
        destinationParameter->endChangeGesture();
    };

    auto resetParameterToDefault = [this] (const juce::String& parameterId)
    {
        auto* parameter = parameters.getParameter(parameterId);

        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
        parameter->endChangeGesture();
    };

    for (int sourceIndex = bellIndex + 1; sourceIndex < currentCount; ++sourceIndex)
    {
        const auto destinationIndex = sourceIndex - 1;
        copyParameterValue(getFilterTypeParamId(destinationIndex), getFilterTypeParamId(sourceIndex));
        copyParameterValue(getFilterLrmsParamId(destinationIndex), getFilterLrmsParamId(sourceIndex));
        copyParameterValue(getBellFrequencyParamId(destinationIndex), getBellFrequencyParamId(sourceIndex));
        copyParameterValue(getBellBandwidthParamId(destinationIndex), getBellBandwidthParamId(sourceIndex));
        copyParameterValue(getBellSlopeParamId(destinationIndex), getBellSlopeParamId(sourceIndex));
        copyParameterValue(getBellGainParamId(destinationIndex), getBellGainParamId(sourceIndex));
        copyParameterValue(getBellBypassParamId(destinationIndex), getBellBypassParamId(sourceIndex));
    }

    const auto lastIndex = currentCount - 1;
    resetParameterToDefault(getFilterTypeParamId(lastIndex));
    resetParameterToDefault(getFilterLrmsParamId(lastIndex));
    resetParameterToDefault(getBellFrequencyParamId(lastIndex));
    resetParameterToDefault(getBellBandwidthParamId(lastIndex));
    resetParameterToDefault(getBellSlopeParamId(lastIndex));
    resetParameterToDefault(getBellGainParamId(lastIndex));
    resetParameterToDefault(getBellBypassParamId(lastIndex));

    setActiveBellCount(currentCount - 1);
    resetBellFilters();
    updateBellFilters();
    return true;
}

bool EqeAudioProcessor::clearBellFilters() noexcept
{
    const auto currentCount = getActiveBellCount();

    if (currentCount <= 0)
        return false;

    auto resetParameterToDefault = [this] (const juce::String& parameterId)
    {
        auto* parameter = parameters.getParameter(parameterId);

        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
        parameter->endChangeGesture();
    };

    for (int bellIndex = 0; bellIndex < currentCount; ++bellIndex)
    {
        resetParameterToDefault(getFilterTypeParamId(bellIndex));
        resetParameterToDefault(getFilterLrmsParamId(bellIndex));
        resetParameterToDefault(getBellFrequencyParamId(bellIndex));
        resetParameterToDefault(getBellBandwidthParamId(bellIndex));
        resetParameterToDefault(getBellSlopeParamId(bellIndex));
        resetParameterToDefault(getBellGainParamId(bellIndex));
        resetParameterToDefault(getBellBypassParamId(bellIndex));
    }

    setActiveBellCount(0);
    resetBellFilters();
    updateBellFilters();
    return true;
}

bool EqeAudioProcessor::moveBellFilter(const int sourceIndex, const int destinationIndex) noexcept
{
    const auto currentCount = getActiveBellCount();

    if (currentCount <= 1
        || sourceIndex < 0 || sourceIndex >= currentCount
        || destinationIndex < 0 || destinationIndex >= currentCount
        || sourceIndex == destinationIndex)
        return false;

    struct BandSnapshot
    {
        float type = 0.0f;
        float lrms = 0.0f;
        float frequency = 0.0f;
        float bandwidth = 0.0f;
        float slope = 0.0f;
        float gain = 0.0f;
        float bypass = 0.0f;
    };

    auto readParameterValue = [this] (const juce::String& parameterId) -> float
    {
        if (auto* parameter = parameters.getParameter(parameterId))
            return parameter->convertFrom0to1(parameter->getValue());

        return 0.0f;
    };

    auto writeParameterValue = [this] (const juce::String& parameterId, const float value)
    {
        if (auto* parameter = parameters.getParameter(parameterId))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };

    std::vector<BandSnapshot> snapshots(static_cast<size_t>(currentCount));

    for (int bandIndex = 0; bandIndex < currentCount; ++bandIndex)
    {
        auto& snapshot = snapshots[static_cast<size_t>(bandIndex)];
        snapshot.type = readParameterValue(getFilterTypeParamId(bandIndex));
        snapshot.lrms = readParameterValue(getFilterLrmsParamId(bandIndex));
        snapshot.frequency = readParameterValue(getBellFrequencyParamId(bandIndex));
        snapshot.bandwidth = readParameterValue(getBellBandwidthParamId(bandIndex));
        snapshot.slope = readParameterValue(getBellSlopeParamId(bandIndex));
        snapshot.gain = readParameterValue(getBellGainParamId(bandIndex));
        snapshot.bypass = readParameterValue(getBellBypassParamId(bandIndex));
    }

    auto movedSnapshot = snapshots[static_cast<size_t>(sourceIndex)];
    snapshots.erase(snapshots.begin() + sourceIndex);
    snapshots.insert(snapshots.begin() + destinationIndex, movedSnapshot);

    for (int bandIndex = 0; bandIndex < currentCount; ++bandIndex)
    {
        const auto& snapshot = snapshots[static_cast<size_t>(bandIndex)];
        writeParameterValue(getFilterTypeParamId(bandIndex), snapshot.type);
        writeParameterValue(getFilterLrmsParamId(bandIndex), snapshot.lrms);
        writeParameterValue(getBellFrequencyParamId(bandIndex), snapshot.frequency);
        writeParameterValue(getBellBandwidthParamId(bandIndex), snapshot.bandwidth);
        writeParameterValue(getBellSlopeParamId(bandIndex), snapshot.slope);
        writeParameterValue(getBellGainParamId(bandIndex), snapshot.gain);
        writeParameterValue(getBellBypassParamId(bandIndex), snapshot.bypass);
    }

    resetBellFilters();
    updateBellFilters();
    return true;
}

void EqeAudioProcessor::setActiveBellCount(const int newCount) noexcept
{
    activeBellCount.store(clampActiveBellCount(newCount), std::memory_order_relaxed);
}

EqeAudioProcessor::FilterType EqeAudioProcessor::getFilterTypeForBand(const size_t bellIndex) const noexcept
{
    if (bellIndex >= filterTypeParams.size() || filterTypeParams[bellIndex] == nullptr)
        return FilterType::bell;

    return filterTypeFromChoiceIndex(static_cast<int>(std::lround(filterTypeParams[bellIndex]->load(std::memory_order_relaxed))));
}

bool EqeAudioProcessor::filterDesignMatches(const size_t bellIndex,
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

void EqeAudioProcessor::storeFilterDesignState(const size_t bellIndex,
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
