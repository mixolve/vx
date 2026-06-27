#include "shell.Processor.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"
#include "shell.ShellState.h"

namespace
{
struct ScopedProcessingSuspend
{
    explicit ScopedProcessingSuspend(VxAudioProcessor& processorIn) noexcept
        : processor(processorIn)
    {
        processor.suspendProcessing(true);
    }

    ~ScopedProcessingSuspend()
    {
        processor.suspendProcessing(false);
    }

    VxAudioProcessor& processor;
};
}

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
    setActiveModule(module, module == ActiveModule::none ? -1 : 0);
}

void VxAudioProcessor::setActiveModule(const ActiveModule module, const int)
{
    activeModule.store(module, std::memory_order_release);

    const auto activeId = juce::String(stateIdForModule(module));

    if (activeId.isNotEmpty())
        parameters.state.setProperty(activeModuleStateKey, activeId, nullptr);
    else
        parameters.state.removeProperty(activeModuleStateKey, nullptr);
}

bool VxAudioProcessor::setActiveModuleIfPresent(const ActiveModule module, const int)
{
    if (module == ActiveModule::none)
    {
        setActiveModule(ActiveModule::none, -1);
        return true;
    }

    if (! loadedModules.empty() && loadedModules.front().module == module)
    {
        setActiveModule(module, 0);
        return true;
    }

    return false;
}

bool VxAudioProcessor::loadModule(const ActiveModule module)
{
    const juce::ScopedLock lock(processingLock);

    if (module == ActiveModule::none || ! loadedModules.empty())
        return false;

    const ScopedProcessingSuspend suspendGuard(*this);
    const auto wasProcessingPrepared = processingPrepared.exchange(false, std::memory_order_acq_rel);
    const auto restoreProcessingPrepared = [this, wasProcessingPrepared]
    {
        if (wasProcessingPrepared && currentSampleRate > 0.0)
            processingPrepared.store(true, std::memory_order_release);
    };

    auto instanceIndex = -1;

    switch (module)
    {
        case ActiveModule::mie: instanceIndex = createMieModuleInstance(); break;
        case ActiveModule::eqe: instanceIndex = createEqeModuleInstance(); break;
        case ActiveModule::spe: instanceIndex = createSpeModuleInstance(); break;
        case ActiveModule::mxe: instanceIndex = createMxeModuleInstance(); break;
        case ActiveModule::tse: instanceIndex = createTseModuleInstance(); break;
        case ActiveModule::none: break;
    }

    if (instanceIndex < 0)
    {
        restoreProcessingPrepared();
        return false;
    }

    if (module == ActiveModule::eqe)
        if (auto* eqeModuleProcessor = getEqeModuleProcessor(instanceIndex))
            eqeModuleProcessor->loadInitialFilterPreset();

    loadedModules.push_back({ module, 0 });
    setActiveModule(module, 0);
    updateShellLatency();
    restoreProcessingPrepared();
    return true;
}

int VxAudioProcessor::getLoadedModuleCount() const noexcept
{
    return static_cast<int>(loadedModules.size());
}

VxAudioProcessor::ModuleSlot VxAudioProcessor::getLoadedModuleSlotAtPosition(const int position) const noexcept
{
    if (position != 0 || loadedModules.empty())
        return {};

    return loadedModules.front();
}

juce::String VxAudioProcessor::getLoadedModuleLabelAtPosition(const int position) const
{
    const auto slot = getLoadedModuleSlotAtPosition(position);

    switch (slot.module)
    {
        case ActiveModule::mie: return "MIE";
        case ActiveModule::eqe: return "EQE";
        case ActiveModule::spe: return "SPE";
        case ActiveModule::mxe: return "MXE";
        case ActiveModule::tse: return "TSE";
        case ActiveModule::none: break;
    }

    return {};
}

int VxAudioProcessor::getActiveModuleInstanceIndex() const noexcept
{
    return getActiveModule() == ActiveModule::none ? -1 : 0;
}

bool VxAudioProcessor::isEqeModuleLoaded() const noexcept
{
    return ! loadedModules.empty() && loadedModules.front().module == ActiveModule::eqe;
}

bool VxAudioProcessor::isSpeModuleLoaded() const noexcept
{
    return ! loadedModules.empty() && loadedModules.front().module == ActiveModule::spe;
}

bool VxAudioProcessor::isMieModuleLoaded() const noexcept
{
    return ! loadedModules.empty() && loadedModules.front().module == ActiveModule::mie;
}

MieAudioProcessor* VxAudioProcessor::getMieModuleProcessor() noexcept
{
    return getMieModuleProcessor(0);
}

const MieAudioProcessor* VxAudioProcessor::getMieModuleProcessor() const noexcept
{
    return getMieModuleProcessor(0);
}

MieAudioProcessor* VxAudioProcessor::getMieModuleProcessor(const int instanceIndex) noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(mieModuleProcessors.size())))
        return nullptr;

    return mieModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

const MieAudioProcessor* VxAudioProcessor::getMieModuleProcessor(const int instanceIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(mieModuleProcessors.size())))
        return nullptr;

    return mieModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor() noexcept
{
    return getSpeModuleProcessor(0);
}

const SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor() const noexcept
{
    return getSpeModuleProcessor(0);
}

SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor(const int instanceIndex) noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(speModuleProcessors.size())))
        return nullptr;

    return speModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

const SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor(const int instanceIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(speModuleProcessors.size())))
        return nullptr;

    return speModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

bool VxAudioProcessor::isMxeModuleLoaded() const noexcept
{
    return ! loadedModules.empty() && loadedModules.front().module == ActiveModule::mxe;
}

MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor() noexcept
{
    return getMxeModuleProcessor(0);
}

const MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor() const noexcept
{
    return getMxeModuleProcessor(0);
}

MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor(const int instanceIndex) noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(mxeModuleProcessors.size())))
        return nullptr;

    return mxeModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

const MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor(const int instanceIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(mxeModuleProcessors.size())))
        return nullptr;

    return mxeModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

bool VxAudioProcessor::isTseModuleLoaded() const noexcept
{
    return ! loadedModules.empty() && loadedModules.front().module == ActiveModule::tse;
}

TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor() noexcept
{
    return getTseModuleProcessor(0);
}

const TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor() const noexcept
{
    return getTseModuleProcessor(0);
}

TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor(const int instanceIndex) noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(tseModuleProcessors.size())))
        return nullptr;

    return tseModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

const TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor(const int instanceIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(tseModuleProcessors.size())))
        return nullptr;

    return tseModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

int VxAudioProcessor::createEqeModuleInstance()
{
    if (eqeModuleProcessors.empty())
        eqeModuleProcessors.resize(1);

    if (eqeModuleProcessors[0] == nullptr)
        eqeModuleProcessors[0] = std::make_unique<EqeModuleProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        eqeModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    return 0;
}

int VxAudioProcessor::createSpeModuleInstance()
{
    if (speModuleProcessors.empty())
        speModuleProcessors.resize(1);

    speModuleProcessors[0] = std::make_unique<SpeModuleProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        speModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    return 0;
}

int VxAudioProcessor::createMieModuleInstance()
{
    if (mieModuleProcessors.empty())
        mieModuleProcessors.resize(1);

    mieModuleProcessors[0] = std::make_unique<MieAudioProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        mieModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    return 0;
}

int VxAudioProcessor::createMxeModuleInstance()
{
    if (mxeModuleProcessors.empty())
        mxeModuleProcessors.resize(1);

    mxeModuleProcessors[0] = std::make_unique<MxeAudioProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        mxeModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    return 0;
}

int VxAudioProcessor::createTseModuleInstance()
{
    if (tseModuleProcessors.empty())
        tseModuleProcessors.resize(1);

    tseModuleProcessors[0] = std::make_unique<TseModuleProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        tseModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    return 0;
}

EqeModuleProcessor* VxAudioProcessor::getEqeModuleProcessor(const int instanceIndex) noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(eqeModuleProcessors.size())))
        return nullptr;

    return eqeModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

const EqeModuleProcessor* VxAudioProcessor::getEqeModuleProcessor(const int instanceIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(instanceIndex, static_cast<int>(eqeModuleProcessors.size())))
        return nullptr;

    return eqeModuleProcessors[static_cast<size_t>(instanceIndex)].get();
}

EqeModuleProcessor* VxAudioProcessor::getActiveEqeModuleProcessor() noexcept
{
    if (getActiveModule() != ActiveModule::eqe)
        return nullptr;

    return getEqeModuleProcessor(0);
}

const EqeModuleProcessor* VxAudioProcessor::getActiveEqeModuleProcessor() const noexcept
{
    if (getActiveModule() != ActiveModule::eqe)
        return nullptr;

    return getEqeModuleProcessor(0);
}

void VxAudioProcessor::restoreLoadedModuleFromStateText(const juce::String& text, const bool publishActiveModule)
{
    const juce::ScopedLock lock(processingLock);

    std::unique_ptr<ScopedProcessingSuspend> suspendGuard;

    if (publishActiveModule)
        suspendGuard = std::make_unique<ScopedProcessingSuspend>(*this);

    loadedModules.clear();
    eqeModuleProcessors.clear();
    speModuleProcessors.clear();
    mieModuleProcessors.clear();
    mxeModuleProcessors.clear();
    tseModuleProcessors.clear();

    const auto module = moduleFromStateId(text);

    if (module == ActiveModule::none)
    {
        if (publishActiveModule)
            setActiveModule(ActiveModule::none, -1);

        updateShellLatency();
        return;
    }

    auto instanceIndex = -1;

    switch (module)
    {
        case ActiveModule::mie: instanceIndex = createMieModuleInstance(); break;
        case ActiveModule::eqe: instanceIndex = createEqeModuleInstance(); break;
        case ActiveModule::spe: instanceIndex = createSpeModuleInstance(); break;
        case ActiveModule::mxe: instanceIndex = createMxeModuleInstance(); break;
        case ActiveModule::tse: instanceIndex = createTseModuleInstance(); break;
        case ActiveModule::none: break;
    }

    if (instanceIndex >= 0)
    {
        loadedModules.push_back({ module, 0 });

        if (publishActiveModule)
            setActiveModule(module, 0);
    }
    else
    {
        if (publishActiveModule)
            setActiveModule(ActiveModule::none, -1);
    }

    updateShellLatency();
}

int VxAudioProcessor::getActiveBellCount() const noexcept
{
    if (const auto* activeEqeModule = getActiveEqeModuleProcessor())
        return activeEqeModule->getActiveBellCount();

    return 0;
}
