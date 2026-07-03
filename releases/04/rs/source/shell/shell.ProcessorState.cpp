#include "shell.Processor.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"

namespace
{
bool hasRestoredModuleProcessor(VxAudioProcessor& processor, const VxAudioProcessor::ActiveModule module) noexcept
{
    switch (module)
    {
        case VxAudioProcessor::ActiveModule::eqe: return processor.getEqeModuleProcessor() != nullptr;
        case VxAudioProcessor::ActiveModule::spe: return processor.getSpeModuleProcessor() != nullptr;
        case VxAudioProcessor::ActiveModule::mie: return processor.getMieModuleProcessor() != nullptr;
        case VxAudioProcessor::ActiveModule::mxe: return processor.getMxeModuleProcessor() != nullptr;
        case VxAudioProcessor::ActiveModule::tse: return processor.getTseModuleProcessor() != nullptr;
        case VxAudioProcessor::ActiveModule::none: break;
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

void removeUnknownParametersFromState(juce::ValueTree& state, juce::AudioProcessorValueTreeState& parameters)
{
    removeUnknownParameterElements(state, parameters);
}

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

void VxAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock(processingLock);

    const auto editorWidth = lastEditorWidth.load(std::memory_order_relaxed);
    const auto editorHeight = lastEditorHeight.load(std::memory_order_relaxed);

    if (editorWidth > 0 && editorHeight > 0)
    {
        parameters.state.setProperty(editorWidthStateKey, editorWidth, nullptr);
        parameters.state.setProperty(editorHeightStateKey, editorHeight, nullptr);
    }

    auto state = parameters.copyState();
    removeUnknownParametersFromState(state, parameters);

    const auto active = getActiveModule();

    if (active == ActiveModule::none)
    {
        state.removeProperty(activeModuleStateKey, nullptr);
        state.removeProperty(eqeModuleStateKey, nullptr);
        state.removeProperty(speModuleStateKey, nullptr);
        state.removeProperty(mieModuleStateKey, nullptr);
        state.removeProperty(mxeModuleStateKey, nullptr);
        state.removeProperty(tseModuleStateKey, nullptr);
    }
    else
    {
        state.setProperty(activeModuleStateKey, stateIdForModule(active), nullptr);

        if (auto* eqeProcessor = getEqeModuleProcessor())
        {
            juce::MemoryBlock eqeStateData;
            eqeProcessor->getStateInformation(eqeStateData);

            if (eqeStateData.getSize() > 0)
                state.setProperty(eqeModuleStateKey, eqeStateData.toBase64Encoding(), nullptr);
            else
                state.removeProperty(eqeModuleStateKey, nullptr);
        }
        else
        {
            state.removeProperty(eqeModuleStateKey, nullptr);
        }

        if (auto* processor = getSpeModuleProcessor())
        {
            const auto stateXml = processor->getStateXmlString();

            if (stateXml.isNotEmpty())
                state.setProperty(speModuleStateKey, stateXml, nullptr);
            else
                state.removeProperty(speModuleStateKey, nullptr);
        }
        else
        {
            state.removeProperty(speModuleStateKey, nullptr);
        }

        if (auto* processor = getMieModuleProcessor())
        {
            juce::MemoryBlock stateData;
            processor->getStateInformation(stateData);

            if (stateData.getSize() > 0)
                state.setProperty(mieModuleStateKey, stateData.toBase64Encoding(), nullptr);
            else
                state.removeProperty(mieModuleStateKey, nullptr);
        }
        else
        {
            state.removeProperty(mieModuleStateKey, nullptr);
        }

        if (auto* processor = getMxeModuleProcessor())
        {
            juce::MemoryBlock stateData;
            processor->getStateInformation(stateData);

            if (stateData.getSize() > 0)
                state.setProperty(mxeModuleStateKey, stateData.toBase64Encoding(), nullptr);
            else
                state.removeProperty(mxeModuleStateKey, nullptr);
        }
        else
        {
            state.removeProperty(mxeModuleStateKey, nullptr);
        }

        if (auto* processor = getTseModuleProcessor())
        {
            const auto stateXml = processor->getStateXmlString();

            if (stateXml.isNotEmpty())
                state.setProperty(tseModuleStateKey, stateXml, nullptr);
            else
                state.removeProperty(tseModuleStateKey, nullptr);
        }
        else
        {
            state.removeProperty(tseModuleStateKey, nullptr);
        }
    }

    if (auto stateXml = state.createXml())
        copyXmlToBinary(*stateXml, destData);
}

void VxAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
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
            const auto restoredEqeStateBase64 = restoredState.getProperty(eqeModuleStateKey).toString();
            const auto restoredSpeStateXml = restoredState.getProperty(speModuleStateKey).toString();
            const auto restoredMieStateBase64 = restoredState.getProperty(mieModuleStateKey).toString();
            const auto restoredMxeStateBase64 = restoredState.getProperty(mxeModuleStateKey).toString();
            const auto restoredTseStateXml = restoredState.getProperty(tseModuleStateKey).toString();

            if (restoredActiveModule == ActiveModule::none)
                restoredState.removeProperty(activeModuleStateKey, nullptr);
            else
                restoredState.setProperty(activeModuleStateKey, stateIdForModule(restoredActiveModule), nullptr);

            removeUnknownParametersFromState(restoredState, parameters);

            const auto restoredModuleId = restoredState.getProperty(activeModuleStateKey).toString();
            parameters.replaceState(restoredState);
            setActiveModule(ActiveModule::none);
            restoreLoadedModuleFromStateText(restoredModuleId, false);

            if (auto* eqeProcessor = getEqeModuleProcessor())
            {
                juce::MemoryBlock eqeStateData;

                if (eqeStateData.fromBase64Encoding(restoredEqeStateBase64))
                    eqeProcessor->setStateInformation(eqeStateData.getData(), static_cast<int>(eqeStateData.getSize()));
            }

            if (auto* processor = getSpeModuleProcessor())
            {
                if (restoredSpeStateXml.isNotEmpty())
                    processor->setStateFromXmlString(restoredSpeStateXml);
            }

            if (auto* processor = getMieModuleProcessor())
            {
                juce::MemoryBlock stateData;

                if (stateData.fromBase64Encoding(restoredMieStateBase64))
                    processor->setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
            }

            if (auto* processor = getMxeModuleProcessor())
            {
                juce::MemoryBlock stateData;

                if (stateData.fromBase64Encoding(restoredMxeStateBase64))
                    processor->setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
            }

            if (auto* processor = getTseModuleProcessor())
            {
                if (restoredTseStateXml.isNotEmpty())
                    processor->setStateFromXmlString(restoredTseStateXml);
            }

            setActiveModule(hasRestoredModuleProcessor(*this, restoredActiveModule) ? restoredActiveModule : ActiveModule::none);
            registerActiveModuleStateListeners();

            updateShellLatency();
            setLastEditorSize(static_cast<int>(parameters.state.getProperty(editorWidthStateKey, 0)),
                              static_cast<int>(parameters.state.getProperty(editorHeightStateKey, 0)));

            if (wasProcessingPrepared && currentSampleRate > 0.0)
                processingPrepared.store(true, std::memory_order_release);

            suppressHostStateNotifications.store(previousNotificationSuppression, std::memory_order_release);
        }
    }
}

juce::Point<int> VxAudioProcessor::getLastEditorSize() const noexcept
{
    return { lastEditorWidth.load(std::memory_order_relaxed),
             lastEditorHeight.load(std::memory_order_relaxed) };
}

void VxAudioProcessor::setLastEditorSize(const int width, const int height) noexcept
{
    lastEditorWidth.store(juce::jmax(0, width), std::memory_order_relaxed);
    lastEditorHeight.store(juce::jmax(0, height), std::memory_order_relaxed);
}

void VxAudioProcessor::notifyHostOfStateChange()
{
    if (suppressHostStateNotifications.load(std::memory_order_relaxed))
        return;

    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails()
                          .withNonParameterStateChanged(true)
                          .withProgramChanged(true));
}
