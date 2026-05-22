#include "shell.Processor.h"
#include "../modules/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/tse/module.tse.TseProcessor.h"

#include <cmath>

void VxAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lastProcessedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedNumChannels = juce::jlimit(1, static_cast<int>(maxSupportedChannels), getTotalNumOutputChannels());

    for (auto& eqeModuleProcessor : eqeModuleProcessors)
    {
        if (eqeModuleProcessor != nullptr)
            eqeModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    }

    for (auto& speModuleProcessor : speModuleProcessors)
    {
        if (speModuleProcessor != nullptr)
            speModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    }

    for (auto& mxeModuleProcessor : mxeModuleProcessors)
    {
        if (mxeModuleProcessor != nullptr)
            mxeModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    }

    for (auto& tseModuleProcessor : tseModuleProcessors)
    {
        if (tseModuleProcessor != nullptr)
            tseModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    }

    updateShellLatency();
}

void VxAudioProcessor::releaseResources()
{
    for (auto& eqeModuleProcessor : eqeModuleProcessors)
    {
        if (eqeModuleProcessor != nullptr)
            eqeModuleProcessor->releaseResources();
    }

    for (auto& speModuleProcessor : speModuleProcessors)
    {
        if (speModuleProcessor != nullptr)
            speModuleProcessor->releaseResources();
    }

    for (auto& mxeModuleProcessor : mxeModuleProcessors)
    {
        if (mxeModuleProcessor != nullptr)
            mxeModuleProcessor->releaseResources();
    }

    for (auto& tseModuleProcessor : tseModuleProcessors)
    {
        if (tseModuleProcessor != nullptr)
            tseModuleProcessor->releaseResources();
    }

    setLatencySamples(0);
}

int VxAudioProcessor::getModuleChainLatencySamples() const noexcept
{
    auto totalLatencySamples = 0;

    for (const auto& slot : moduleChain)
    {
        if (slot.module == ActiveModule::spe)
        {
            if (const auto* processor = getSpeModuleProcessor(slot.instanceIndex))
                totalLatencySamples += processor->getLatencySamples();
        }
        else if (slot.module == ActiveModule::mxe)
        {
            if (const auto* processor = getMxeModuleProcessor(slot.instanceIndex))
                totalLatencySamples += processor->getModuleLatencySamples();
        }
        else if (slot.module == ActiveModule::tse)
        {
            if (const auto* processor = getTseModuleProcessor(slot.instanceIndex))
                totalLatencySamples += processor->getLatencySamples();
        }
    }

    return totalLatencySamples;
}

void VxAudioProcessor::updateShellLatency() noexcept
{
    const auto totalLatencySamples = getModuleChainLatencySamples();

    if (getLatencySamples() != totalLatencySamples)
        setLatencySamples(totalLatencySamples);
}

bool VxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    return mainInput == juce::AudioChannelSet::mono()
        || mainInput == juce::AudioChannelSet::stereo();
}

void VxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiBuffer);
    lastProcessedBlockSize = juce::jmax(1, buffer.getNumSamples());

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const auto eqeLoaded = isEqeModuleLoaded();
    const auto speLoaded = isSpeModuleLoaded();
    const auto mxeLoaded = isMxeModuleLoaded();
    const auto tseLoaded = isTseModuleLoaded();

    auto applyShellGlobalBlock = [this, &buffer]
    {
        const auto processChannels = juce::jmin(buffer.getNumChannels(), preparedNumChannels);

        if (processChannels <= 0)
        {
            globalClipIndicator.store(0.0f, std::memory_order_relaxed);
            return;
        }

        const auto bypassAll = globalBypassParam != nullptr
            && globalBypassParam->load(std::memory_order_relaxed) >= 0.5f;

        if (bypassAll)
        {
            globalClipIndicator.store(0.0f, std::memory_order_relaxed);
            return;
        }

        const auto bypassExceptOutGain = globalBypassOutGainOnlyParam != nullptr
            && globalBypassOutGainOnlyParam->load(std::memory_order_relaxed) >= 0.5f;

        if (! bypassExceptOutGain)
        {
            const auto wideAmount = globalWideParam != nullptr
                ? juce::jlimit(0.0f, 4.0f, globalWideParam->load(std::memory_order_relaxed) / 100.0f)
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

        globalClipIndicator.store(clipped ? 1.0f : 0.0f, std::memory_order_relaxed);
    };

    if (! eqeLoaded && ! speLoaded && ! mxeLoaded && ! tseLoaded)
    {
        applyShellGlobalBlock();
        return;
    }

    auto getTseASplitSource = [this]() -> TseModuleProcessor*
    {
        auto tseAIsLoaded = false;
        for (const auto& slot : moduleChain)
        {
            if (slot.module == ActiveModule::tse && slot.instanceIndex == 0)
            {
                tseAIsLoaded = true;
                break;
            }
        }

        if (! tseAIsLoaded)
            return nullptr;

        auto* processor = getTseModuleProcessor(0);
        return processor != nullptr && processor->canProvideSplit() ? processor : nullptr;
    };

    for (int moduleIndex = 0; moduleIndex < getLoadedModuleCount(); ++moduleIndex)
    {
        const auto moduleSlot = getLoadedModuleSlotAtPosition(moduleIndex);

        switch (moduleSlot.module)
        {
            case ActiveModule::spe:
                if (auto* processor = getSpeModuleProcessor(moduleSlot.instanceIndex))
                    processor->refreshLatencyState();
                break;

            case ActiveModule::mxe:
                if (auto* processor = getMxeModuleProcessor(moduleSlot.instanceIndex))
                    processor->syncParameters();
                break;

            case ActiveModule::tse:
                if (auto* processor = getTseModuleProcessor(moduleSlot.instanceIndex))
                    processor->refreshLatencyState();
                break;

            case ActiveModule::eqe:
            case ActiveModule::none:
                break;
        }
    }

    updateShellLatency();

    for (int moduleIndex = 0; moduleIndex < getLoadedModuleCount(); ++moduleIndex)
    {
        const auto moduleSlot = getLoadedModuleSlotAtPosition(moduleIndex);

        switch (moduleSlot.module)
        {
            case ActiveModule::eqe:
                if (auto* eqeModuleProcessor = getEqeModuleProcessor(moduleSlot.instanceIndex))
                {
                    eqeModuleProcessor->setTransientSplitProvider(getTseASplitSource());
                    eqeModuleProcessor->processBlock(buffer);
                }
                break;

            case ActiveModule::spe:
                if (auto* processor = getSpeModuleProcessor(moduleSlot.instanceIndex))
                    processor->processBlock(buffer);
                break;

            case ActiveModule::mxe:
                if (auto* processor = getMxeModuleProcessor(moduleSlot.instanceIndex))
                {
                    juce::MidiBuffer mxeMidiBuffer;
                    processor->processBlock(buffer, mxeMidiBuffer);
                }
                break;

            case ActiveModule::tse:
                if (auto* processor = getTseModuleProcessor(moduleSlot.instanceIndex))
                    processor->processBlock(buffer);
                break;

            case ActiveModule::none:
                break;
        }
    }

    applyShellGlobalBlock();
    updateShellLatency();
}

void VxAudioProcessor::copyVisualiserResponse(std::array<float, visualizerScopeSize>& stereoDb,
                                               std::array<float, visualizerScopeSize>& leftDb,
                                               std::array<float, visualizerScopeSize>& rightDb,
                                               std::array<float, visualizerScopeSize>& midDb,
                                               std::array<float, visualizerScopeSize>& sideDb,
                                               double& sampleRateOut) noexcept
{
    if (auto* eqeModuleProcessor = getActiveEqeModuleProcessor())
    {
        eqeModuleProcessor->copyVisualiserResponse(stereoDb, leftDb, rightDb, midDb, sideDb, sampleRateOut);
        return;
    }

    sampleRateOut = currentSampleRate;
    stereoDb.fill(0.0f);
    leftDb.fill(0.0f);
    rightDb.fill(0.0f);
    midDb.fill(0.0f);
    sideDb.fill(0.0f);
}

void VxAudioProcessor::setActiveBellCount(const int newCount) noexcept
{
    if (auto* eqeModuleProcessor = getActiveEqeModuleProcessor())
        eqeModuleProcessor->setActiveBellCount(newCount);
}
