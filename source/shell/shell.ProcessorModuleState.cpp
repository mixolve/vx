#include "shell.Processor.h"
#include "../modules/multiband/tls/module.tls.PluginProcessor.h"
#include "../modules/multiband/tls/module.tls.ParameterIds.h"
#include "../modules/multiband/dyn/module.dyn.PluginProcessor.h"
#include "../modules/fft/module.fft.FftProcessor.h"
#include "../modules/multiband/trs/module.trs.TrsProcessor.h"
#include "shell.ShellState.h"

#include <optional>

const char* AvaAudioProcessor::stateIdForModule(const ActiveModule module) noexcept
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

AvaAudioProcessor::ActiveModule AvaAudioProcessor::moduleFromStateId(const juce::String& moduleId)
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

juce::AudioProcessorValueTreeState& AvaAudioProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& AvaAudioProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

AvaAudioProcessor::ActiveModule AvaAudioProcessor::getActiveModule() const noexcept
{
    return activeModule.load(std::memory_order_acquire);
}

void AvaAudioProcessor::setActiveModule(const ActiveModule module)
{
    activeModule.store(module, std::memory_order_release);

    const auto activeId = juce::String(stateIdForModule(module));

    if (activeId.isNotEmpty())
        parameters.state.setProperty(activeModuleStateKey, activeId, nullptr);
    else
        parameters.state.removeProperty(activeModuleStateKey, nullptr);
}

bool AvaAudioProcessor::decodeTlsDirectHostParameterId(const juce::String& parameterId,
                                                       size_t& bandIndex,
                                                       juce::String& suffix) noexcept
{
    for (size_t candidateBand = 0; candidateBand < tlsDirectHostBandCount; ++candidateBand)
    {
        if (parameterId == getTlsDirectHostParameterId(candidateBand, "solo"))
        {
            bandIndex = candidateBand;
            suffix = "solo";
            return true;
        }
    }

    for (const auto* candidateSuffix : tlsWidebandListenParameterSuffixes)
    {
        if (parameterId == getTlsDirectHostParameterId(tlsWidebandListenHostIndex, candidateSuffix))
        {
            bandIndex = tlsWidebandListenHostIndex;
            suffix = candidateSuffix;
            return true;
        }
    }

    return false;
}

juce::String AvaAudioProcessor::getTlsModuleParameterId(const size_t bandIndex, const juce::String& suffix)
{
    if (bandIndex == tlsWidebandListenHostIndex)
        return tls::parameters::makeFullbandParameterId(suffix.toRawUTF8());

    if (suffix == "solo")
        return tls::parameters::makeSoloParameterId(bandIndex);

    return tls::parameters::makeBandParameterId(bandIndex, suffix.toRawUTF8());
}

void AvaAudioProcessor::registerTlsDirectHostParameterListeners()
{
    for (size_t bandIndex = 0; bandIndex < tlsDirectHostBandCount; ++bandIndex)
        parameters.addParameterListener(getTlsDirectHostParameterId(bandIndex, "solo"), this);

    for (const auto* suffix : tlsWidebandListenParameterSuffixes)
        parameters.addParameterListener(getTlsDirectHostParameterId(tlsWidebandListenHostIndex, suffix), this);
}

void AvaAudioProcessor::unregisterTlsDirectHostParameterListeners()
{
    for (size_t bandIndex = 0; bandIndex < tlsDirectHostBandCount; ++bandIndex)
        parameters.removeParameterListener(getTlsDirectHostParameterId(bandIndex, "solo"), this);

    for (const auto* suffix : tlsWidebandListenParameterSuffixes)
        parameters.removeParameterListener(getTlsDirectHostParameterId(tlsWidebandListenHostIndex, suffix), this);
}

void AvaAudioProcessor::syncTlsDirectHostParametersToModule()
{
    if (getActiveModule() != ActiveModule::tls || synchronisingTlsDirectHostParameters)
        return;

    const juce::ScopedValueSetter<bool> syncGuard(synchronisingTlsDirectHostParameters, true);

    const auto syncParameter = [this] (const size_t bandIndex, const char* suffix)
    {
        auto* hostParameter = parameters.getParameter(getTlsDirectHostParameterId(bandIndex, suffix));
        auto* tlsParameter = getTlsModuleProcessor() != nullptr
            ? getTlsModuleProcessor()->getValueTreeState().getParameter(getTlsModuleParameterId(bandIndex, suffix))
            : nullptr;

        if (hostParameter == nullptr || tlsParameter == nullptr)
            return;

        const auto hostValue = hostParameter->getValue() >= 0.5f;
        const auto tlsValue = tlsParameter->getValue() >= 0.5f;

        if (hostValue != tlsValue)
            tlsParameter->setValueNotifyingHost(hostValue ? 1.0f : 0.0f);
    };

    for (size_t bandIndex = 0; bandIndex < tlsDirectHostBandCount; ++bandIndex)
        syncParameter(bandIndex, "solo");

    for (const auto* suffix : tlsWidebandListenParameterSuffixes)
        syncParameter(tlsWidebandListenHostIndex, suffix);
}

void AvaAudioProcessor::syncTlsModuleParametersToHost()
{
    if (getActiveModule() != ActiveModule::tls || synchronisingTlsDirectHostParameters)
        return;

    auto* tlsProcessor = getTlsModuleProcessor();

    if (tlsProcessor == nullptr)
        return;

    const juce::ScopedValueSetter<bool> syncGuard(synchronisingTlsDirectHostParameters, true);

    const auto syncParameter = [this, tlsProcessor] (const size_t bandIndex, const char* suffix)
    {
        auto* hostParameter = parameters.getParameter(getTlsDirectHostParameterId(bandIndex, suffix));
        auto* tlsParameter = tlsProcessor->getValueTreeState().getParameter(getTlsModuleParameterId(bandIndex, suffix));

        if (hostParameter == nullptr || tlsParameter == nullptr)
            return;

        const auto hostValue = hostParameter->getValue() >= 0.5f;
        const auto tlsValue = tlsParameter->getValue() >= 0.5f;

        if (hostValue != tlsValue)
            hostParameter->setValueNotifyingHost(tlsValue ? 1.0f : 0.0f);
    };

    for (size_t bandIndex = 0; bandIndex < tlsDirectHostBandCount; ++bandIndex)
        syncParameter(bandIndex, "solo");

    for (const auto* suffix : tlsWidebandListenParameterSuffixes)
        syncParameter(tlsWidebandListenHostIndex, suffix);
}

bool AvaAudioProcessor::syncTlsDirectHostParameterToModule(const juce::String& parameterId, const float value)
{
    if (synchronisingTlsDirectHostParameters || getActiveModule() != ActiveModule::tls)
        return false;

    size_t bandIndex = 0;
    juce::String suffix;

    if (! decodeTlsDirectHostParameterId(parameterId, bandIndex, suffix))
        return false;

    auto* tlsProcessor = getTlsModuleProcessor();
    auto* tlsParameter = tlsProcessor != nullptr
        ? tlsProcessor->getValueTreeState().getParameter(getTlsModuleParameterId(bandIndex, suffix))
        : nullptr;

    if (tlsParameter == nullptr)
        return true;

    const juce::ScopedValueSetter<bool> syncGuard(synchronisingTlsDirectHostParameters, true);
    const auto isEnabled = value >= 0.5f;

    if ((tlsParameter->getValue() >= 0.5f) != isEnabled)
        tlsParameter->setValueNotifyingHost(isEnabled ? 1.0f : 0.0f);

    return true;
}

bool AvaAudioProcessor::syncTlsModuleParameterToHost(const juce::String& parameterId, const float value)
{
    if (synchronisingTlsDirectHostParameters || getActiveModule() != ActiveModule::tls)
        return false;

    const auto syncParameter = [this, &parameterId, value] (const size_t bandIndex, const char* suffix)
    {
        if (parameterId != getTlsModuleParameterId(bandIndex, suffix))
            return false;

        if (auto* hostParameter = parameters.getParameter(getTlsDirectHostParameterId(bandIndex, suffix));
            hostParameter != nullptr)
        {
            const juce::ScopedValueSetter<bool> syncGuard(synchronisingTlsDirectHostParameters, true);
            const auto isEnabled = value >= 0.5f;

            if ((hostParameter->getValue() >= 0.5f) != isEnabled)
                hostParameter->setValueNotifyingHost(isEnabled ? 1.0f : 0.0f);
        }

        return true;
    };

    for (size_t bandIndex = 0; bandIndex < tlsDirectHostBandCount; ++bandIndex)
        if (syncParameter(bandIndex, "solo"))
            return true;

    for (const auto* suffix : tlsWidebandListenParameterSuffixes)
        if (syncParameter(tlsWidebandListenHostIndex, suffix))
            return true;

    return false;
}

bool AvaAudioProcessor::loadModule(const ActiveModule module)
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
    syncTlsDirectHostParametersToModule();
    registerActiveModuleStateListeners();
    updateShellLatency();
    notifyHostOfStateChange();
    restoreProcessingPrepared();
    return true;
}

bool AvaAudioProcessor::clearLoadedModule()
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

void AvaAudioProcessor::registerActiveModuleStateListeners()
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

void AvaAudioProcessor::clearActiveModuleStateListeners()
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

void AvaAudioProcessor::parameterChanged(const juce::String& parameterID, const float newValue)
{
    if (syncTlsDirectHostParameterToModule(parameterID, newValue)
        || syncTlsModuleParameterToHost(parameterID, newValue))
    {
        notifyHostOfStateChange();
        return;
    }

    notifyHostOfStateChange();
}

void AvaAudioProcessor::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&)
{
    notifyHostOfStateChange();
}

TlsAudioProcessor* AvaAudioProcessor::getTlsModuleProcessor() noexcept
{
    return tlsModuleProcessor.get();
}

const TlsAudioProcessor* AvaAudioProcessor::getTlsModuleProcessor() const noexcept
{
    return tlsModuleProcessor.get();
}

FftModuleProcessor* AvaAudioProcessor::getFftModuleProcessor() noexcept
{
    return fftModuleProcessor.get();
}

const FftModuleProcessor* AvaAudioProcessor::getFftModuleProcessor() const noexcept
{
    return fftModuleProcessor.get();
}

DynAudioProcessor* AvaAudioProcessor::getDynModuleProcessor() noexcept
{
    return dynModuleProcessor.get();
}

const DynAudioProcessor* AvaAudioProcessor::getDynModuleProcessor() const noexcept
{
    return dynModuleProcessor.get();
}

TrsModuleProcessor* AvaAudioProcessor::getTrsModuleProcessor() noexcept
{
    return trsModuleProcessor.get();
}

const TrsModuleProcessor* AvaAudioProcessor::getTrsModuleProcessor() const noexcept
{
    return trsModuleProcessor.get();
}

void AvaAudioProcessor::resetModuleProcessors() noexcept
{
    eqlModuleProcessor.reset();
    fftModuleProcessor.reset();
    tlsModuleProcessor.reset();
    dynModuleProcessor.reset();
    trsModuleProcessor.reset();
}

bool AvaAudioProcessor::createModuleInstance(const ActiveModule module)
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

EqlModuleProcessor* AvaAudioProcessor::getEqlModuleProcessor() noexcept
{
    return eqlModuleProcessor.get();
}

const EqlModuleProcessor* AvaAudioProcessor::getEqlModuleProcessor() const noexcept
{
    return eqlModuleProcessor.get();
}

void AvaAudioProcessor::restoreLoadedModuleFromStateText(const juce::String& text, const bool publishActiveModule)
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
