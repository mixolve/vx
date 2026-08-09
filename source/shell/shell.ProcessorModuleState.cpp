#include "shell.Processor.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"
#include "shell.ShellState.h"

#include <optional>

const char* VxAudioProcessor::stateIdForModule(const ActiveModule module) noexcept
{
    switch (module)
    {
        case ActiveModule::mie: return mieModuleId;
        case ActiveModule::eqe: return eqeModuleId;
        case ActiveModule::spe: return speModuleId;
        case ActiveModule::mxe: return mxeModuleId;
        case ActiveModule::tse: return tseModuleId;
        case ActiveModule::none: break;
    }

    return "";
}

VxAudioProcessor::ActiveModule VxAudioProcessor::moduleFromStateId(const juce::String& moduleId)
{
    const auto trimmed = moduleId.trim();

    if (trimmed.equalsIgnoreCase(eqeModuleId))
        return ActiveModule::eqe;

    if (trimmed.equalsIgnoreCase(speModuleId))
        return ActiveModule::spe;

    if (trimmed.equalsIgnoreCase(mieModuleId))
        return ActiveModule::mie;

    if (trimmed.equalsIgnoreCase(mxeModuleId))
        return ActiveModule::mxe;

    if (trimmed.equalsIgnoreCase(tseModuleId))
        return ActiveModule::tse;

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

    if (module == ActiveModule::eqe)
        if (auto* eqe = getEqeModuleProcessor())
            eqe->loadInitialFilterPreset();

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
        case ActiveModule::eqe: observeModule(getEqeModuleProcessor()); break;
        case ActiveModule::spe: observeModule(getSpeModuleProcessor()); break;
        case ActiveModule::mie: observeModule(getMieModuleProcessor()); break;
        case ActiveModule::mxe: observeModule(getMxeModuleProcessor()); break;
        case ActiveModule::tse: observeModule(getTseModuleProcessor()); break;
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

MieAudioProcessor* VxAudioProcessor::getMieModuleProcessor() noexcept
{
    return mieModuleProcessor.get();
}

const MieAudioProcessor* VxAudioProcessor::getMieModuleProcessor() const noexcept
{
    return mieModuleProcessor.get();
}

SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor() noexcept
{
    return speModuleProcessor.get();
}

const SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor() const noexcept
{
    return speModuleProcessor.get();
}

MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor() noexcept
{
    return mxeModuleProcessor.get();
}

const MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor() const noexcept
{
    return mxeModuleProcessor.get();
}

TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor() noexcept
{
    return tseModuleProcessor.get();
}

const TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor() const noexcept
{
    return tseModuleProcessor.get();
}

void VxAudioProcessor::resetModuleProcessors() noexcept
{
    eqeModuleProcessor.reset();
    speModuleProcessor.reset();
    mieModuleProcessor.reset();
    mxeModuleProcessor.reset();
    tseModuleProcessor.reset();
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
        case ActiveModule::eqe:
            eqeModuleProcessor = std::make_unique<EqeModuleProcessor>();
            return prepareModule(eqeModuleProcessor);

        case ActiveModule::spe:
            speModuleProcessor = std::make_unique<SpeModuleProcessor>(*this);
            return prepareModule(speModuleProcessor);

        case ActiveModule::mie:
            mieModuleProcessor = std::make_unique<MieAudioProcessor>();
            return prepareModule(mieModuleProcessor);

        case ActiveModule::mxe:
            mxeModuleProcessor = std::make_unique<MxeAudioProcessor>();
            return prepareModule(mxeModuleProcessor);

        case ActiveModule::tse:
            tseModuleProcessor = std::make_unique<TseModuleProcessor>(*this);
            return prepareModule(tseModuleProcessor);

        case ActiveModule::none:
            break;
    }

    return false;
}

EqeModuleProcessor* VxAudioProcessor::getEqeModuleProcessor() noexcept
{
    return eqeModuleProcessor.get();
}

const EqeModuleProcessor* VxAudioProcessor::getEqeModuleProcessor() const noexcept
{
    return eqeModuleProcessor.get();
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
