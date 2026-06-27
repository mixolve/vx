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

    for (auto& mieModuleProcessor : mieModuleProcessors)
    {
        if (mieModuleProcessor != nullptr)
            mieModuleProcessor->prepareToPlay(sampleRate, samplesPerBlock);
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
    processingPrepared.store(true, std::memory_order_release);
}

void VxAudioProcessor::releaseResources()
{
    processingPrepared.store(false, std::memory_order_release);
    const juce::ScopedLock lock(processingLock);

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

    for (auto& mieModuleProcessor : mieModuleProcessors)
    {
        if (mieModuleProcessor != nullptr)
            mieModuleProcessor->releaseResources();
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
    currentSampleRate = 0.0;
}

int VxAudioProcessor::getLoadedModulesLatencySamples() const noexcept
{
    const auto module = activeModule.load(std::memory_order_acquire);

    switch (module)
    {
        case ActiveModule::spe:
            if (const auto* processor = getSpeModuleProcessor(0))
                return processor->getLatencySamples();
            break;

        case ActiveModule::mie:
            if (const auto* processor = getMieModuleProcessor(0))
                return processor->getModuleLatencySamples();
            break;

        case ActiveModule::mxe:
            if (const auto* processor = getMxeModuleProcessor(0))
                return processor->getModuleLatencySamples();
            break;

        case ActiveModule::tse:
            if (const auto* processor = getTseModuleProcessor(0))
                return processor->getLatencySamples();
            break;

        case ActiveModule::eqe:
        case ActiveModule::none:
            break;
    }

    return 0;
}

void VxAudioProcessor::updateShellLatency() noexcept
{
    const auto totalLatencySamples = getLoadedModulesLatencySamples();

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
    const juce::ScopedLock lock(processingLock);

    lastProcessedBlockSize = juce::jmax(1, buffer.getNumSamples());

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (! processingPrepared.load(std::memory_order_acquire))
    {
        globalClipIndicator.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const auto active = activeModule.load(std::memory_order_acquire);

    auto applyShellGlobalOutputStage = [this, &buffer]
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
        applyShellGlobalOutputStage();
        return;
    }

    switch (active)
    {
        case ActiveModule::spe:
            if (auto* processor = getSpeModuleProcessor(0))
                processor->refreshLatencyState();
            break;

        case ActiveModule::mie:
            if (auto* processor = getMieModuleProcessor(0))
                processor->syncParameters();
            break;

        case ActiveModule::mxe:
            if (auto* processor = getMxeModuleProcessor(0))
                processor->syncParameters();
            break;

        case ActiveModule::tse:
            if (auto* processor = getTseModuleProcessor(0))
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
            if (auto* eqeModuleProcessor = getEqeModuleProcessor(0))
                eqeModuleProcessor->processBlock(buffer);
            break;

        case ActiveModule::spe:
            if (auto* processor = getSpeModuleProcessor(0))
                processor->processBlock(buffer);
            break;

        case ActiveModule::mie:
            if (auto* processor = getMieModuleProcessor(0))
            {
                juce::MidiBuffer mieMidiBuffer;
                processor->processBlock(buffer, mieMidiBuffer);
            }
            break;

        case ActiveModule::mxe:
            if (auto* processor = getMxeModuleProcessor(0))
            {
                juce::MidiBuffer mxeMidiBuffer;
                processor->processBlock(buffer, mxeMidiBuffer);
            }
            break;

        case ActiveModule::tse:
            if (auto* processor = getTseModuleProcessor(0))
                processor->processBlock(buffer);
            break;

        case ActiveModule::none:
            break;
    }

    applyShellGlobalOutputStage();
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
