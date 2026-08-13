#include "shell.Processor.h"
#include "../modules/multiband/tls/module.tls.PluginProcessor.h"
#include "../modules/multiband/dyn/module.dyn.ParameterIds.h"
#include "../modules/multiband/dyn/module.dyn.PluginProcessor.h"
#include "../modules/fft/module.fft.FftProcessor.h"
#include "../modules/multiband/trs/module.trs.TrsProcessor.h"

#include <cmath>

void VxAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    processingPrepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(processingLock);

    currentSampleRate = sampleRate;
    lastProcessedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedNumChannels = juce::jlimit(1, static_cast<int>(maxSupportedChannels), getTotalNumOutputChannels());

    if (eqlModuleProcessor != nullptr)
        eqlModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (fftModuleProcessor != nullptr)
        fftModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (tlsModuleProcessor != nullptr)
        tlsModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (dynModuleProcessor != nullptr)
        dynModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
    if (trsModuleProcessor != nullptr)
        trsModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);

    updateShellLatency();
    processingPrepared.store(true, std::memory_order_release);
}

void VxAudioProcessor::releaseResources()
{
    processingPrepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(processingLock);

    if (eqlModuleProcessor != nullptr)
        eqlModuleProcessor->releaseResources();
    if (fftModuleProcessor != nullptr)
        fftModuleProcessor->releaseResources();
    if (tlsModuleProcessor != nullptr)
        tlsModuleProcessor->releaseResources();
    if (dynModuleProcessor != nullptr)
        dynModuleProcessor->releaseResources();
    if (trsModuleProcessor != nullptr)
        trsModuleProcessor->releaseResources();

    setLatencySamples(0);
    currentSampleRate = 0.0;
}

void VxAudioProcessor::reset()
{
    const juce::ScopedLock lock(processingLock);

    if (eqlModuleProcessor != nullptr)
        eqlModuleProcessor->resetProcessingState();
    if (fftModuleProcessor != nullptr)
        fftModuleProcessor->releaseResources();
    if (tlsModuleProcessor != nullptr)
        tlsModuleProcessor->reset();
    if (dynModuleProcessor != nullptr)
        dynModuleProcessor->reset();
    if (trsModuleProcessor != nullptr)
        trsModuleProcessor->releaseResources();

    globalClipIndicator.store(0.0f, std::memory_order_relaxed);
}

int VxAudioProcessor::getActiveModuleLatencySamples() const noexcept
{
    const auto module = activeModule.load(std::memory_order_acquire);

    switch (module)
    {
        case ActiveModule::fft:
            if (const auto* processor = getFftModuleProcessor())
                return processor->getLatencySamples();
            break;

        case ActiveModule::tls:
            if (const auto* processor = getTlsModuleProcessor())
                return processor->getModuleLatencySamples();
            break;

        case ActiveModule::dyn:
            if (const auto* processor = getDynModuleProcessor())
                return processor->getModuleLatencySamples();
            break;

        case ActiveModule::trs:
            if (const auto* processor = getTrsModuleProcessor())
                return processor->getLatencySamples();
            break;

        case ActiveModule::eql:
            if (const auto* processor = getEqlModuleProcessor())
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

void VxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

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
        case ActiveModule::fft:
            if (auto* processor = getFftModuleProcessor())
                processor->refreshLatencyState();
            break;

        case ActiveModule::tls:
            if (auto* processor = getTlsModuleProcessor())
                processor->syncParameters();
            break;

        case ActiveModule::dyn:
            if (auto* processor = getDynModuleProcessor())
                processor->syncParameters();
            break;

        case ActiveModule::trs:
            if (auto* processor = getTrsModuleProcessor())
                processor->refreshLatencyState();
            break;

        case ActiveModule::eql:
        case ActiveModule::none:
            break;
    }

    updateShellLatency();

    switch (active)
    {
        case ActiveModule::eql:
            if (auto* eqlProcessor = getEqlModuleProcessor())
                eqlProcessor->processBlock(buffer);
            break;

        case ActiveModule::fft:
            if (auto* processor = getFftModuleProcessor())
                processor->processBlock(buffer);
            break;

        case ActiveModule::tls:
            if (auto* processor = getTlsModuleProcessor())
            {
                juce::MidiBuffer tlsMidiBuffer;
                processor->processBlock(buffer, tlsMidiBuffer);
            }
            break;

        case ActiveModule::dyn:
            if (auto* processor = getDynModuleProcessor())
            {
                juce::MidiBuffer dynMidiBuffer;
                processor->processBlock(buffer, dynMidiBuffer);
            }
            break;

        case ActiveModule::trs:
            if (auto* processor = getTrsModuleProcessor())
                processor->processBlock(buffer);
            break;

        case ActiveModule::none:
            break;
    }

    applyGlobalOutputStage();
    updateShellLatency();
}
