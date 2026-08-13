#include "shell.Processor.h"
#include "../modules/multiband/tls/module.tls.PluginProcessor.h"
#include "../modules/multiband/dyn/module.dyn.PluginProcessor.h"
#include "../modules/fft/module.fft.FftProcessor.h"
#include "../modules/multiband/trs/module.trs.TrsProcessor.h"
#include "shell.ShellState.h"

#include <optional>

const char* VxAudioProcessor::stateIdForModule(const ActiveModule module) noexcept
{
    switch (module)
    {
        case ActiveModule::tls: return tlsModuleId;
        case ActiveModule::eql: return eqlModuleId;
        case ActiveModule::fft: return fftModuleId;
        case ActiveModule::dyn: return dynModuleId;
        case ActiveModule::trs: return trsModuleId;
        case ActiveModule::none: break;
    }

    return "";
}

VxAudioProcessor::ActiveModule VxAudioProcessor::moduleFromStateId(const juce::String& moduleId)
{
    const auto trimmed = moduleId.trim();

    if (trimmed.equalsIgnoreCase(eqlModuleId))
        return ActiveModule::eql;

    if (trimmed.equalsIgnoreCase(fftModuleId))
        return ActiveModule::fft;

    if (trimmed.equalsIgnoreCase(tlsModuleId))
        return ActiveModule::tls;

    if (trimmed.equalsIgnoreCase(dynModuleId))
        return ActiveModule::dyn;

    if (trimmed.equalsIgnoreCase(trsModuleId))
        return ActiveModule::trs;

    return ActiveModule::none;
}

juce::AudioProcessorValueTreeState& VxAudioProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& VxAudioProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

VxAudioProcessor::ActiveModule VxAudioProcessor::getActiveModule() const noexcept
{
    return activeModule.load(std::memory_order_acquire);
}

void VxAudioProcessor::setActiveModule(const ActiveModule module)
{
    activeModule.store(module, std::memory_order_release);

    const auto activeId = juce::String(stateIdForModule(module));

    if (activeId.isNotEmpty())
        parameters.state.setProperty(activeModuleStateKey, activeId, nullptr);
    else
        parameters.state.removeProperty(activeModuleStateKey, nullptr);
}

bool VxAudioProcessor::loadModule(const ActiveModule module)
{
    const juce::ScopedLock lock(processingLock);

    if (module == ActiveModule::none || getActiveModule() != ActiveModule::none)
        return false;

    const ScopedProcessingSuspend suspendGuard(*this);
    clearActiveModuleStateListeners();
    const auto wasProcessingPrepared = processingPrepared.exchange(false, std::memory_order_acq_rel);
    const auto restoreProcessingPrepared = [this, wasProcessingPrepared]
    {
        if (wasProcessingPrepared && currentSampleRate > 0.0)
            processingPrepared.store(true, std::memory_order_release);
    };

    if (! createModuleInstance(module))
    {
        restoreProcessingPrepared();
        return false;
    }

    if (module == ActiveModule::eql)
        if (auto* eql = getEqlModuleProcessor())
            eql->loadInitialFilterPreset();

    setActiveModule(module);
    registerActiveModuleStateListeners();
    updateShellLatency();
    notifyHostOfStateChange();
    restoreProcessingPrepared();
    return true;
}

bool VxAudioProcessor::clearLoadedModule()
{
    const juce::ScopedLock lock(processingLock);

    if (getActiveModule() == ActiveModule::none)
        return false;

    const ScopedProcessingSuspend suspendGuard(*this);
    clearActiveModuleStateListeners();
    resetModuleProcessors();
    setActiveModule(ActiveModule::none);
    updateShellLatency();
    notifyHostOfStateChange();
    return true;
}

void VxAudioProcessor::registerActiveModuleStateListeners()
{
    clearActiveModuleStateListeners();

    auto observeModule = [this] (auto* processor)
    {
        if (processor == nullptr)
            return;

        observedModuleValueTreeState = &processor->getValueTreeState();
    };

    switch (getActiveModule())
    {
        case ActiveModule::eql: observeModule(getEqlModuleProcessor()); break;
        case ActiveModule::fft: observeModule(getFftModuleProcessor()); break;
        case ActiveModule::tls: observeModule(getTlsModuleProcessor()); break;
        case ActiveModule::dyn: observeModule(getDynModuleProcessor()); break;
        case ActiveModule::trs: observeModule(getTrsModuleProcessor()); break;
        case ActiveModule::none: break;
    }

    if (observedModuleValueTreeState == nullptr)
        return;

    observedModuleParameterIds.reserve(static_cast<size_t>(observedModuleValueTreeState->state.getNumChildren()));

    for (const auto parameterState : observedModuleValueTreeState->state)
    {
        const auto parameterId = parameterState.getProperty("id").toString();

        if (parameterId.isEmpty())
            continue;

        observedModuleValueTreeState->addParameterListener(parameterId, this);
        observedModuleParameterIds.push_back(parameterId);
    }

    observedModuleState = observedModuleValueTreeState->state;

    if (observedModuleState.isValid())
        observedModuleState.addListener(this);
}

void VxAudioProcessor::clearActiveModuleStateListeners()
{
    if (observedModuleValueTreeState != nullptr)
    {
        for (const auto& parameterId : observedModuleParameterIds)
            observedModuleValueTreeState->removeParameterListener(parameterId, this);
    }

    observedModuleValueTreeState = nullptr;
    observedModuleParameterIds.clear();

    if (observedModuleState.isValid())
        observedModuleState.removeListener(this);

    observedModuleState = {};
}

void VxAudioProcessor::parameterChanged(const juce::String&, float)
{
    notifyHostOfStateChange();
}

void VxAudioProcessor::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&)
{
    notifyHostOfStateChange();
}

TlsAudioProcessor* VxAudioProcessor::getTlsModuleProcessor() noexcept
{
    return tlsModuleProcessor.get();
}

const TlsAudioProcessor* VxAudioProcessor::getTlsModuleProcessor() const noexcept
{
    return tlsModuleProcessor.get();
}

FftModuleProcessor* VxAudioProcessor::getFftModuleProcessor() noexcept
{
    return fftModuleProcessor.get();
}

const FftModuleProcessor* VxAudioProcessor::getFftModuleProcessor() const noexcept
{
    return fftModuleProcessor.get();
}

DynAudioProcessor* VxAudioProcessor::getDynModuleProcessor() noexcept
{
    return dynModuleProcessor.get();
}

const DynAudioProcessor* VxAudioProcessor::getDynModuleProcessor() const noexcept
{
    return dynModuleProcessor.get();
}

TrsModuleProcessor* VxAudioProcessor::getTrsModuleProcessor() noexcept
{
    return trsModuleProcessor.get();
}

const TrsModuleProcessor* VxAudioProcessor::getTrsModuleProcessor() const noexcept
{
    return trsModuleProcessor.get();
}

void VxAudioProcessor::resetModuleProcessors() noexcept
{
    eqlModuleProcessor.reset();
    fftModuleProcessor.reset();
    tlsModuleProcessor.reset();
    dynModuleProcessor.reset();
    trsModuleProcessor.reset();
}

bool VxAudioProcessor::createModuleInstance(const ActiveModule module)
{
    auto prepareModule = [this] (auto& processor)
    {
        if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
            processor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

        return true;
    };

    switch (module)
    {
        case ActiveModule::eql:
            eqlModuleProcessor = std::make_unique<EqlModuleProcessor>();
            return prepareModule(eqlModuleProcessor);

        case ActiveModule::fft:
            fftModuleProcessor = std::make_unique<FftModuleProcessor>(*this);
            return prepareModule(fftModuleProcessor);

        case ActiveModule::tls:
            tlsModuleProcessor = std::make_unique<TlsAudioProcessor>();
            return prepareModule(tlsModuleProcessor);

        case ActiveModule::dyn:
            dynModuleProcessor = std::make_unique<DynAudioProcessor>();
            return prepareModule(dynModuleProcessor);

        case ActiveModule::trs:
            trsModuleProcessor = std::make_unique<TrsModuleProcessor>(*this);
            return prepareModule(trsModuleProcessor);

        case ActiveModule::none:
            break;
    }

    return false;
}

EqlModuleProcessor* VxAudioProcessor::getEqlModuleProcessor() noexcept
{
    return eqlModuleProcessor.get();
}

const EqlModuleProcessor* VxAudioProcessor::getEqlModuleProcessor() const noexcept
{
    return eqlModuleProcessor.get();
}

void VxAudioProcessor::restoreLoadedModuleFromStateText(const juce::String& text, const bool publishActiveModule)
{
    const juce::ScopedLock lock(processingLock);

    std::optional<ScopedProcessingSuspend> suspendGuard;

    if (publishActiveModule)
        suspendGuard.emplace(*this);

    clearActiveModuleStateListeners();
    resetModuleProcessors();

    const auto module = moduleFromStateId(text);

    if (module == ActiveModule::none)
    {
        if (publishActiveModule)
            setActiveModule(ActiveModule::none);

        updateShellLatency();
        return;
    }

    if (createModuleInstance(module))
    {
        if (publishActiveModule)
        {
            setActiveModule(module);
            registerActiveModuleStateListeners();
        }
    }
    else
    {
        if (publishActiveModule)
            setActiveModule(ActiveModule::none);
    }

    updateShellLatency();
}
