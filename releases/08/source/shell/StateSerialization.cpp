#include "Processor.h"
#include "../modules/tls/Processor.h"
#include "../modules/dyn/Processor.h"
#include "../modules/fft/Processor.h"
#include "../modules/trs/Processor.h"

#include <optional>

namespace
{
bool hasRestoredModuleProcessor(AvaAudioProcessor& processor, const AvaAudioProcessor::ActiveModule module) noexcept
{
    switch (module)
    {
        case AvaAudioProcessor::ActiveModule::eql: return processor.getEqlModuleProcessor() != nullptr;
        case AvaAudioProcessor::ActiveModule::fft: return processor.getFftModuleProcessor() != nullptr;
        case AvaAudioProcessor::ActiveModule::tls: return processor.getTlsModuleProcessor() != nullptr;
        case AvaAudioProcessor::ActiveModule::dyn: return processor.getDynModuleProcessor() != nullptr;
        case AvaAudioProcessor::ActiveModule::trs: return processor.getTrsModuleProcessor() != nullptr;
        case AvaAudioProcessor::ActiveModule::none: break;
    }

    return false;
}

void removeUnknownParameterElements(juce::ValueTree& state, juce::AudioProcessorValueTreeState& parameters)
{
    for (int childIndex = state.getNumChildren(); --childIndex >= 0;)
    {
        auto child = state.getChild(childIndex);

        if (child.hasType("PARAM"))
        {
            const auto parameterId = child.getProperty("id").toString().trim();

            if (parameterId.isEmpty() || parameters.getParameter(parameterId) == nullptr)
                state.removeChild(childIndex, nullptr);
        }
    }
}

void copyStateProperties(juce::ValueTree& target, const juce::ValueTree& source)
{
    for (int propertyIndex = target.getNumProperties(); --propertyIndex >= 0;)
        target.removeProperty(target.getPropertyName(propertyIndex), nullptr);

    for (int propertyIndex = 0; propertyIndex < source.getNumProperties(); ++propertyIndex)
    {
        const auto propertyName = source.getPropertyName(propertyIndex);
        target.setProperty(propertyName, source.getProperty(propertyName), nullptr);
    }
}

struct RestoredModuleStateProperties
{
    explicit RestoredModuleStateProperties(const juce::ValueTree& state)
        : eqlBase64(state.getProperty(AvaAudioProcessor::eqlModuleStateKey).toString()),
          fftXml(state.getProperty(AvaAudioProcessor::fftModuleStateKey).toString()),
          tlsBase64(state.getProperty(AvaAudioProcessor::tlsModuleStateKey).toString()),
          dynBase64(state.getProperty(AvaAudioProcessor::dynModuleStateKey).toString()),
          trsXml(state.getProperty(AvaAudioProcessor::trsModuleStateKey).toString())
    {
    }

    juce::String eqlBase64;
    juce::String fftXml;
    juce::String tlsBase64;
    juce::String dynBase64;
    juce::String trsXml;
};

struct RestoredABCompareState
{
    explicit RestoredABCompareState(const juce::ValueTree& state)
        : shouldRestore(state.hasProperty(AvaAudioProcessor::abCompareSnapshotAStateKey)
                        || state.hasProperty(AvaAudioProcessor::abCompareSnapshotBStateKey)
                        || state.hasProperty(AvaAudioProcessor::abCompareActiveSlotStateKey)),
          snapshotA(state.getProperty(AvaAudioProcessor::abCompareSnapshotAStateKey).toString()),
          snapshotB(state.getProperty(AvaAudioProcessor::abCompareSnapshotBStateKey).toString()),
          activeSlot(juce::jlimit(0,
                                  1,
                                  static_cast<int>(state.getProperty(AvaAudioProcessor::abCompareActiveSlotStateKey,
                                                                     0))))
    {
    }

    bool shouldRestore = false;
    juce::String snapshotA;
    juce::String snapshotB;
    int activeSlot = 0;
};

void removeABCompareStateProperties(juce::ValueTree& state)
{
    state.removeProperty(AvaAudioProcessor::abCompareSnapshotAStateKey, nullptr);
    state.removeProperty(AvaAudioProcessor::abCompareSnapshotBStateKey, nullptr);
    state.removeProperty(AvaAudioProcessor::abCompareActiveSlotStateKey, nullptr);
}

void restoreABCompareState(AvaAudioProcessor& processor, const RestoredABCompareState& restoredState)
{
    if (! restoredState.shouldRestore)
        return;

    const auto restoreSnapshot = [&processor] (const int slot, const juce::String& encodedState)
    {
        juce::MemoryBlock snapshot;

        if (! snapshot.fromBase64Encoding(encodedState))
            snapshot.reset();

        processor.setABCompareSnapshot(slot, snapshot);
    };

    restoreSnapshot(0, restoredState.snapshotA);
    restoreSnapshot(1, restoredState.snapshotB);
    processor.setABCompareActiveSlot(restoredState.activeSlot);
}

void storeABCompareState(juce::ValueTree& state, const AvaAudioProcessor& processor)
{
    const auto storeSnapshot = [&state, &processor] (const int slot, const juce::Identifier& property)
    {
        const auto snapshot = processor.getABCompareSnapshot(slot);

        if (snapshot.isEmpty())
            state.removeProperty(property, nullptr);
        else
            state.setProperty(property, snapshot.toBase64Encoding(), nullptr);
    };

    storeSnapshot(0, AvaAudioProcessor::abCompareSnapshotAStateKey);
    storeSnapshot(1, AvaAudioProcessor::abCompareSnapshotBStateKey);
    state.setProperty(AvaAudioProcessor::abCompareActiveSlotStateKey,
                      processor.getABCompareActiveSlot(),
                      nullptr);
}

template <typename Processor>
bool restoreBinaryModuleState(Processor* processor, const juce::String& stateBase64)
{
    if (processor == nullptr)
        return false;

    juce::MemoryBlock stateData;

    if (! stateData.fromBase64Encoding(stateBase64))
        return false;

    processor->setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    return true;
}

template <typename Processor>
bool restoreXmlModuleState(Processor* processor, const juce::String& stateXml)
{
    if (processor == nullptr || stateXml.isEmpty())
        return false;

    processor->setStateFromXmlString(stateXml);
    return true;
}

bool restoreEqlModuleState(AvaAudioProcessor& processor,
                           const juce::String& stateBase64,
                           const bool useABCompareRestore)
{
    auto* eqlProcessor = processor.getEqlModuleProcessor();

    if (eqlProcessor == nullptr)
        return false;

    juce::MemoryBlock stateData;

    if (! stateData.fromBase64Encoding(stateBase64))
        return false;

    if (useABCompareRestore)
        return eqlProcessor->applyStateInformationForABCompare(stateData.getData(), static_cast<int>(stateData.getSize()));

    eqlProcessor->setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    return true;
}

bool restoreActiveModuleState(AvaAudioProcessor& processor,
                              const AvaAudioProcessor::ActiveModule module,
                              const RestoredModuleStateProperties& moduleStates,
                              const bool useABCompareRestore)
{
    switch (module)
    {
        case AvaAudioProcessor::ActiveModule::eql:
            return restoreEqlModuleState(processor, moduleStates.eqlBase64, useABCompareRestore);

        case AvaAudioProcessor::ActiveModule::fft:
            return restoreXmlModuleState(processor.getFftModuleProcessor(), moduleStates.fftXml);

        case AvaAudioProcessor::ActiveModule::tls:
            return restoreBinaryModuleState(processor.getTlsModuleProcessor(), moduleStates.tlsBase64);

        case AvaAudioProcessor::ActiveModule::dyn:
            return restoreBinaryModuleState(processor.getDynModuleProcessor(), moduleStates.dynBase64);

        case AvaAudioProcessor::ActiveModule::trs:
            return restoreXmlModuleState(processor.getTrsModuleProcessor(), moduleStates.trsXml);

        case AvaAudioProcessor::ActiveModule::none:
            return true;
    }

    return false;
}

template <typename Processor>
void storeBinaryModuleState(juce::ValueTree& state,
                            const juce::Identifier& property,
                            Processor* processor)
{
    if (processor == nullptr)
    {
        state.removeProperty(property, nullptr);
        return;
    }

    juce::MemoryBlock stateData;
    processor->getStateInformation(stateData);

    if (stateData.getSize() > 0)
        state.setProperty(property, stateData.toBase64Encoding(), nullptr);
    else
        state.removeProperty(property, nullptr);
}

template <typename Processor>
void storeXmlModuleState(juce::ValueTree& state,
                         const juce::Identifier& property,
                         Processor* processor)
{
    if (processor == nullptr)
    {
        state.removeProperty(property, nullptr);
        return;
    }

    const auto stateXml = processor->getStateXmlString();

    if (stateXml.isNotEmpty())
        state.setProperty(property, stateXml, nullptr);
    else
        state.removeProperty(property, nullptr);
}

}

void AvaAudioProcessor::removeModuleStateProperties(juce::ValueTree& state)
{
    state.removeProperty(activeModuleStateKey, nullptr);
    state.removeProperty(eqlModuleStateKey, nullptr);
    state.removeProperty(fftModuleStateKey, nullptr);
    state.removeProperty(tlsModuleStateKey, nullptr);
    state.removeProperty(dynModuleStateKey, nullptr);
    state.removeProperty(trsModuleStateKey, nullptr);
}

void AvaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    writeStateInformation(destData, true);
}

void AvaAudioProcessor::getStateInformationForABCompareSnapshot(juce::MemoryBlock& destData)
{
    writeStateInformation(destData, false);
}

void AvaAudioProcessor::writeStateInformation(juce::MemoryBlock& destData, const bool includeABCompareState)
{
    const auto editorWidth = lastEditorWidth.load(std::memory_order_relaxed);
    const auto editorHeight = lastEditorHeight.load(std::memory_order_relaxed);
    auto state = parameters.copyState();

    if (editorWidth > 0 && editorHeight > 0)
    {
        state.setProperty(editorWidthStateKey, editorWidth, nullptr);
        state.setProperty(editorHeightStateKey, editorHeight, nullptr);
    }

    const auto active = getActiveModule();

    if (active == ActiveModule::none)
    {
        removeModuleStateProperties(state);
    }
    else
    {
        state.setProperty(activeModuleStateKey, stateIdForModule(active), nullptr);
        storeBinaryModuleState(state, eqlModuleStateKey, getEqlModuleProcessor());
        storeXmlModuleState(state, fftModuleStateKey, getFftModuleProcessor());
        storeBinaryModuleState(state, tlsModuleStateKey, getTlsModuleProcessor());
        storeBinaryModuleState(state, dynModuleStateKey, getDynModuleProcessor());
        storeXmlModuleState(state, trsModuleStateKey, getTrsModuleProcessor());
    }

    if (includeABCompareState)
        storeABCompareState(state, *this);
    else
        removeABCompareStateProperties(state);

    if (auto stateXml = state.createXml())
        copyXmlToBinary(*stateXml, destData);
}

void AvaAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (setStateInformationPreservingLoadedModule(data, sizeInBytes))
        return;

    if (auto stateXml = getXmlFromBinary(data, sizeInBytes))
    {
        if (stateXml->hasTagName(parameters.state.getType()))
        {
            const auto previousNotificationSuppression = suppressHostStateNotifications.exchange(true, std::memory_order_acq_rel);
            const ScopedProcessingSuspend suspendGuard(*this);
            const auto wasProcessingPrepared = processingPrepared.exchange(false, std::memory_order_acq_rel);
            const juce::ScopedLock lock(processingLock);
            auto restoredState = juce::ValueTree::fromXml(*stateXml);
            const auto restoredActiveModule = moduleFromStateId(restoredState.getProperty(activeModuleStateKey).toString());
            const auto restoredModuleStates = RestoredModuleStateProperties(restoredState);
            const auto restoredABCompareState = RestoredABCompareState(restoredState);
            removeABCompareStateProperties(restoredState);

            if (restoredActiveModule == ActiveModule::none)
                removeModuleStateProperties(restoredState);
            else
                restoredState.setProperty(activeModuleStateKey, stateIdForModule(restoredActiveModule), nullptr);

            removeUnknownParameterElements(restoredState, parameters);

            const auto restoredModuleId = restoredState.getProperty(activeModuleStateKey).toString();
            parameters.replaceState(restoredState);
            setActiveModule(ActiveModule::none);
            restoreLoadedModuleFromStateText(restoredModuleId, false);
            restoreActiveModuleState(*this, restoredActiveModule, restoredModuleStates, false);

            setActiveModule(hasRestoredModuleProcessor(*this, restoredActiveModule) ? restoredActiveModule : ActiveModule::none);
            registerActiveModuleStateListeners();
            restoreABCompareState(*this, restoredABCompareState);

            updateShellLatency();
            setLastEditorSize(static_cast<int>(parameters.state.getProperty(editorWidthStateKey, 0)),
                              static_cast<int>(parameters.state.getProperty(editorHeightStateKey, 0)));

            if (wasProcessingPrepared && currentSampleRate > 0.0)
                processingPrepared.store(true, std::memory_order_release);

            suppressHostStateNotifications.store(previousNotificationSuppression, std::memory_order_release);
        }
    }
}

bool AvaAudioProcessor::setStateInformationPreservingLoadedModule(const void* data,
                                                                 const int sizeInBytes,
                                                                 const bool suspendProcessingForRestore)
{
    auto stateXml = getXmlFromBinary(data, sizeInBytes);

    if (stateXml == nullptr || ! stateXml->hasTagName(parameters.state.getType()))
        return false;

    auto restoredState = juce::ValueTree::fromXml(*stateXml);
    const auto restoredActiveModule = moduleFromStateId(restoredState.getProperty(activeModuleStateKey).toString());

    if (restoredActiveModule != getActiveModule())
        return false;

    const auto restoredModuleStates = RestoredModuleStateProperties(restoredState);
    const auto restoredABCompareState = RestoredABCompareState(restoredState);
    removeABCompareStateProperties(restoredState);
    const auto previousNotificationSuppression = suppressHostStateNotifications.exchange(true, std::memory_order_acq_rel);
    std::optional<ScopedProcessingSuspend> suspendGuard;

    if (suspendProcessingForRestore)
        suspendGuard.emplace(*this);

    const auto wasProcessingPrepared = suspendProcessingForRestore
        ? processingPrepared.exchange(false, std::memory_order_acq_rel)
        : processingPrepared.load(std::memory_order_acquire);
    const juce::ScopedLock lock(processingLock);

    if (restoredActiveModule == ActiveModule::none)
        removeModuleStateProperties(restoredState);
    else
        restoredState.setProperty(activeModuleStateKey, stateIdForModule(restoredActiveModule), nullptr);

    removeUnknownParameterElements(restoredState, parameters);
    parameters.replaceState(restoredState);

    clearActiveModuleStateListeners();
    restoreActiveModuleState(*this, restoredActiveModule, restoredModuleStates, false);

    if (restoredActiveModule != ActiveModule::none)
        registerActiveModuleStateListeners();

    restoreABCompareState(*this, restoredABCompareState);

    updateShellLatency();
    setLastEditorSize(static_cast<int>(parameters.state.getProperty(editorWidthStateKey, 0)),
                      static_cast<int>(parameters.state.getProperty(editorHeightStateKey, 0)));

    if (suspendProcessingForRestore && wasProcessingPrepared && currentSampleRate > 0.0)
        processingPrepared.store(true, std::memory_order_release);

    suppressHostStateNotifications.store(previousNotificationSuppression, std::memory_order_release);
    return true;
}

bool AvaAudioProcessor::applyStateInformationForABCompare(const void* data, const int sizeInBytes)
{
    auto stateXml = getXmlFromBinary(data, sizeInBytes);

    if (stateXml == nullptr || ! stateXml->hasTagName(parameters.state.getType()))
        return false;

    auto restoredState = juce::ValueTree::fromXml(*stateXml);
    const auto restoredActiveModule = moduleFromStateId(restoredState.getProperty(activeModuleStateKey).toString());
    removeABCompareStateProperties(restoredState);

    const auto restoredModuleStates = RestoredModuleStateProperties(restoredState);
    const auto previousNotificationSuppression = suppressHostStateNotifications.exchange(true, std::memory_order_acq_rel);
    const auto reportedLatencySamples = getLatencySamples();
    const auto previousLatencyFloor = abCompareLatencyFloorSamples.load(std::memory_order_acquire);
    abCompareLatencyFloorSamples.store(juce::jmax(previousLatencyFloor, reportedLatencySamples),
                                       std::memory_order_release);
    abCompareLatencyLocked.store(true, std::memory_order_release);
    const juce::ScopedLock lock(processingLock);

    if (restoredActiveModule == ActiveModule::none)
        removeModuleStateProperties(restoredState);
    else
        restoredState.setProperty(activeModuleStateKey, stateIdForModule(restoredActiveModule), nullptr);

    copyStateProperties(parameters.state, restoredState);
    clearActiveModuleStateListeners();
    resetModuleProcessors();
    setActiveModule(ActiveModule::none);

    auto restoredModuleState = restoredActiveModule == ActiveModule::none;

    if (restoredActiveModule != ActiveModule::none && createModuleInstance(restoredActiveModule))
    {
        restoredModuleState = restoreActiveModuleState(*this, restoredActiveModule, restoredModuleStates, true);

        if (restoredModuleState)
        {
            setActiveModule(restoredActiveModule);
            registerActiveModuleStateListeners();
        }
    }

    updateShellLatency();
    setLastEditorSize(static_cast<int>(parameters.state.getProperty(editorWidthStateKey, 0)),
                      static_cast<int>(parameters.state.getProperty(editorHeightStateKey, 0)));

    suppressHostStateNotifications.store(previousNotificationSuppression, std::memory_order_release);
    return restoredModuleState;
}

int AvaAudioProcessor::getABCompareActiveSlot() const noexcept
{
    return juce::jlimit(0, 1, abCompareActiveSlot.load(std::memory_order_acquire));
}

void AvaAudioProcessor::setABCompareActiveSlot(const int slot) noexcept
{
    abCompareActiveSlot.store(juce::jlimit(0, 1, slot), std::memory_order_release);
}

bool AvaAudioProcessor::isABCompareSnapshotValid(const int slot) const noexcept
{
    if (! juce::isPositiveAndBelow(slot, static_cast<int>(abCompareSnapshotValid.size())))
        return false;

    const juce::ScopedLock lock(abCompareLock);
    return abCompareSnapshotValid[static_cast<size_t>(slot)]
        && ! abCompareSnapshots[static_cast<size_t>(slot)].isEmpty();
}

juce::MemoryBlock AvaAudioProcessor::getABCompareSnapshot(const int slot) const
{
    if (! juce::isPositiveAndBelow(slot, static_cast<int>(abCompareSnapshots.size())))
        return {};

    const juce::ScopedLock lock(abCompareLock);
    return abCompareSnapshots[static_cast<size_t>(slot)];
}

void AvaAudioProcessor::setABCompareSnapshot(const int slot, const juce::MemoryBlock& snapshot)
{
    if (! juce::isPositiveAndBelow(slot, static_cast<int>(abCompareSnapshots.size())))
        return;

    const juce::ScopedLock lock(abCompareLock);
    abCompareSnapshots[static_cast<size_t>(slot)] = snapshot;
    abCompareSnapshotValid[static_cast<size_t>(slot)] = ! snapshot.isEmpty();
}

juce::Point<int> AvaAudioProcessor::getLastEditorSize() const noexcept
{
    return { lastEditorWidth.load(std::memory_order_relaxed),
             lastEditorHeight.load(std::memory_order_relaxed) };
}

void AvaAudioProcessor::setLastEditorSize(const int width, const int height) noexcept
{
    lastEditorWidth.store(juce::jmax(0, width), std::memory_order_relaxed);
    lastEditorHeight.store(juce::jmax(0, height), std::memory_order_relaxed);
}

void AvaAudioProcessor::notifyHostOfStateChange()
{
    if (suppressHostStateNotifications.load(std::memory_order_relaxed))
        return;

    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails()
                          .withNonParameterStateChanged(true));
}
