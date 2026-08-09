#include "shell.Processor.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.ParameterIds.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"

#include <cmath>

void VxAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    processingPrepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(processingLock);

    currentSampleRate = sampleRate;
    lastProcessedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedNumChannels = juce::jlimit(1, static_cast<int>(maxSupportedChannels), getTotalNumOutputChannels());

    if (eqeModuleProcessor != nullptr)
        eqeModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (speModuleProcessor != nullptr)
        speModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (mieModuleProcessor != nullptr)
        mieModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (mxeModuleProcessor != nullptr)
        mxeModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (tseModuleProcessor != nullptr)
        tseModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);

    updateShellLatency();
    processingPrepared.store(true, std::memory_order_release);
}

void VxAudioProcessor::releaseResources()
{
    processingPrepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(processingLock);

    if (eqeModuleProcessor != nullptr)
        eqeModuleProcessor->releaseResources();
    if (speModuleProcessor != nullptr)
        speModuleProcessor->releaseResources();
    if (mieModuleProcessor != nullptr)
        mieModuleProcessor->releaseResources();
    if (mxeModuleProcessor != nullptr)
        mxeModuleProcessor->releaseResources();
    if (tseModuleProcessor != nullptr)
        tseModuleProcessor->releaseResources();

    setLatencySamples(0);
    currentSampleRate = 0.0;
}

void VxAudioProcessor::reset()
{
    const juce::ScopedLock lock(processingLock);

    if (eqeModuleProcessor != nullptr)
        eqeModuleProcessor->resetProcessingState();
    if (speModuleProcessor != nullptr)
        speModuleProcessor->releaseResources();
    if (mieModuleProcessor != nullptr)
        mieModuleProcessor->reset();
    if (mxeModuleProcessor != nullptr)
        mxeModuleProcessor->reset();
    if (tseModuleProcessor != nullptr)
        tseModuleProcessor->releaseResources();

    globalClipIndicator.store(0.0f, std::memory_order_relaxed);
}

int VxAudioProcessor::getActiveModuleLatencySamples() const noexcept
{
    const auto module = activeModule.load(std::memory_order_acquire);

    switch (module)
    {
        case ActiveModule::spe:
            if (const auto* processor = getSpeModuleProcessor())
                return processor->getLatencySamples();
            break;

        case ActiveModule::mie:
            if (const auto* processor = getMieModuleProcessor())
                return processor->getModuleLatencySamples();
            break;

        case ActiveModule::mxe:
            if (const auto* processor = getMxeModuleProcessor())
                return processor->getModuleLatencySamples();
            break;

        case ActiveModule::tse:
            if (const auto* processor = getTseModuleProcessor())
                return processor->getLatencySamples();
            break;

        case ActiveModule::eqe:
            if (const auto* processor = getEqeModuleProcessor())
                return processor->getLatencySamples();
            break;

        case ActiveModule::none:
            break;
    }

    return 0;
}

void VxAudioProcessor::updateShellLatency() noexcept
{
    auto totalLatencySamples = getActiveModuleLatencySamples();

    if (abCompareLatencyLocked.load(std::memory_order_acquire))
    {
        totalLatencySamples = juce::jmax(totalLatencySamples,
                                         abCompareLatencyFloorSamples.load(std::memory_order_acquire));
    }

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

    const juce::ScopedTryLock lock(processingLock);

    if (! lock.isLocked())
    {
        globalClipIndicator.store(0.0f, std::memory_order_relaxed);
        return;
    }

    if (! processingPrepared.load(std::memory_order_acquire))
    {
        globalClipIndicator.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const auto active = activeModule.load(std::memory_order_acquire);

    auto applyGlobalOutputStage = [this, &buffer]
    {
        const auto outputProcessChannels = juce::jmin(buffer.getNumChannels(), preparedNumChannels);

        if (outputProcessChannels <= 0)
        {
            globalClipIndicator.store(0.0f, std::memory_order_relaxed);
            return;
        }

        auto clipped = false;

        for (int channel = 0; channel < outputProcessChannels && ! clipped; ++channel)
            clipped = buffer.getMagnitude(channel, 0, buffer.getNumSamples()) >= 1.0f;

        globalClipIndicator.store(clipped ? 1.0f : 0.0f, std::memory_order_relaxed);
    };

    const auto globalBypassActive = globalBypassParam != nullptr
        && globalBypassParam->load(std::memory_order_relaxed) >= 0.5f;

    if (globalBypassActive)
    {
        if (getLatencySamples() != 0)
            setLatencySamples(0);

        globalClipIndicator.store(0.0f, std::memory_order_relaxed);
        return;
    }

    if (active == ActiveModule::none)
    {
        applyGlobalOutputStage();
        return;
    }

    switch (active)
    {
        case ActiveModule::spe:
            if (auto* processor = getSpeModuleProcessor())
                processor->refreshLatencyState();
            break;

        case ActiveModule::mie:
            if (auto* processor = getMieModuleProcessor())
                processor->syncParameters();
            break;

        case ActiveModule::mxe:
            if (auto* processor = getMxeModuleProcessor())
                processor->syncParameters();
            break;

        case ActiveModule::tse:
            if (auto* processor = getTseModuleProcessor())
                processor->refreshLatencyState();
            break;

        case ActiveModule::eqe:
        case ActiveModule::none:
            break;
    }

    updateShellLatency();

    switch (active)
    {
        case ActiveModule::eqe:
            if (auto* eqeProcessor = getEqeModuleProcessor())
                eqeProcessor->processBlock(buffer);
            break;

        case ActiveModule::spe:
            if (auto* processor = getSpeModuleProcessor())
                processor->processBlock(buffer);
            break;

        case ActiveModule::mie:
            if (auto* processor = getMieModuleProcessor())
            {
                juce::MidiBuffer mieMidiBuffer;
                processor->processBlock(buffer, mieMidiBuffer);
            }
            break;

        case ActiveModule::mxe:
            if (auto* processor = getMxeModuleProcessor())
            {
                juce::MidiBuffer mxeMidiBuffer;
                processor->processBlock(buffer, mxeMidiBuffer);
            }
            break;

        case ActiveModule::tse:
            if (auto* processor = getTseModuleProcessor())
                processor->processBlock(buffer);
            break;

        case ActiveModule::none:
            break;
    }

    applyGlobalOutputStage();
    updateShellLatency();
}
