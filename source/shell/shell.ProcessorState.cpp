#include "shell.Processor.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"

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
    state.setProperty(activeBellCountStateKey, getActiveBellCount(), nullptr);

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

        if (auto* eqeModuleProcessor = getEqeModuleProcessor(0))
        {
            juce::MemoryBlock eqeStateData;
            eqeModuleProcessor->getStateInformation(eqeStateData);

            if (eqeStateData.getSize() > 0)
                state.setProperty(eqeModuleStateKey, eqeStateData.toBase64Encoding(), nullptr);
            else
                state.removeProperty(eqeModuleStateKey, nullptr);
        }
        else
        {
            state.removeProperty(eqeModuleStateKey, nullptr);
        }

        if (auto* processor = getSpeModuleProcessor(0))
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

        if (auto* processor = getMieModuleProcessor(0))
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

        if (auto* processor = getMxeModuleProcessor(0))
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

        if (auto* processor = getTseModuleProcessor(0))
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
            const ScopedProcessingSuspend suspendGuard(*this);
            const auto wasProcessingPrepared = processingPrepared.exchange(false, std::memory_order_acq_rel);
            const juce::ScopedLock lock(processingLock);
            auto restoredState = juce::ValueTree::fromXml(*stateXml);
            const auto restoredActiveModule = moduleFromStateId(restoredState.getProperty(activeModuleStateKey).toString());

            if (restoredActiveModule == ActiveModule::none)
                restoredState.removeProperty(activeModuleStateKey, nullptr);
            else
                restoredState.setProperty(activeModuleStateKey, stateIdForModule(restoredActiveModule), nullptr);

            const auto restoredModuleId = restoredState.getProperty(activeModuleStateKey).toString();
            parameters.replaceState(restoredState);
            setActiveModule(ActiveModule::none, -1);
            restoreLoadedModuleFromStateText(restoredModuleId, false);

            if (auto* eqeModuleProcessor = getEqeModuleProcessor(0))
            {
                const auto eqeStateBase64 = restoredState.getProperty(eqeModuleStateKey).toString();
                juce::MemoryBlock eqeStateData;

                if (eqeStateData.fromBase64Encoding(eqeStateBase64))
                    eqeModuleProcessor->setStateInformation(eqeStateData.getData(), static_cast<int>(eqeStateData.getSize()));
            }

            if (auto* processor = getSpeModuleProcessor(0))
            {
                const auto speStateXml = restoredState.getProperty(speModuleStateKey).toString();

                if (speStateXml.isNotEmpty())
                    processor->setStateFromXmlString(speStateXml);
            }

            if (auto* processor = getMieModuleProcessor(0))
            {
                const auto stateBase64 = restoredState.getProperty(mieModuleStateKey).toString();
                juce::MemoryBlock stateData;

                if (stateData.fromBase64Encoding(stateBase64))
                    processor->setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
            }

            if (auto* processor = getMxeModuleProcessor(0))
            {
                const auto stateBase64 = restoredState.getProperty(mxeModuleStateKey).toString();
                juce::MemoryBlock stateData;

                if (stateData.fromBase64Encoding(stateBase64))
                    processor->setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
            }

            if (auto* processor = getTseModuleProcessor(0))
            {
                const auto tseStateXml = restoredState.getProperty(tseModuleStateKey).toString();

                if (tseStateXml.isNotEmpty())
                    processor->setStateFromXmlString(tseStateXml);
            }

            if (restoredActiveModule != ActiveModule::none && ! loadedModules.empty())
                setActiveModule(restoredActiveModule, 0);
            else
                setActiveModule(ActiveModule::none, -1);

            updateShellLatency();
            setLastEditorSize(static_cast<int>(parameters.state.getProperty(editorWidthStateKey, 0)),
                              static_cast<int>(parameters.state.getProperty(editorHeightStateKey, 0)));

            if (wasProcessingPrepared && currentSampleRate > 0.0)
                processingPrepared.store(true, std::memory_order_release);
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
