#include "shell.Processor.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"
#include "../modules/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/tse/module.tse.TseProcessor.h"

namespace
{
VxAudioProcessor::ActiveModule activeModuleFromId(const juce::String& moduleId)
{
    if (moduleId.equalsIgnoreCase(VxAudioProcessor::eqeModuleId)
        || moduleId.startsWithIgnoreCase("eqe-")
        || moduleId.startsWithIgnoreCase("eqe_"))
        return VxAudioProcessor::ActiveModule::eqe;

    if (moduleId.equalsIgnoreCase(VxAudioProcessor::speModuleId)
        || moduleId.startsWithIgnoreCase("spe-")
        || moduleId.startsWithIgnoreCase("spe_"))
        return VxAudioProcessor::ActiveModule::spe;

    if (moduleId.equalsIgnoreCase(VxAudioProcessor::mxeModuleId)
        || moduleId.startsWithIgnoreCase("mxe-")
        || moduleId.startsWithIgnoreCase("mxe_"))
        return VxAudioProcessor::ActiveModule::mxe;

    if (moduleId.equalsIgnoreCase(VxAudioProcessor::tseModuleId)
        || moduleId.startsWithIgnoreCase("tse-")
        || moduleId.startsWithIgnoreCase("tse_"))
        return VxAudioProcessor::ActiveModule::tse;

    return VxAudioProcessor::ActiveModule::none;
}

int moduleInstanceIndexFromId(const juce::String& moduleId, const juce::String& prefix) noexcept
{
    const auto trimmed = moduleId.trim();

    if (trimmed.startsWithIgnoreCase(prefix + "-") || trimmed.startsWithIgnoreCase(prefix + "_"))
    {
        const auto suffix = trimmed.substring(prefix.length() + 1, prefix.length() + 2).toLowerCase();
        const auto instanceIndex = suffix.isNotEmpty() ? static_cast<int>(suffix[0] - static_cast<juce_wchar>('a')) : 0;
        return juce::jlimit(0, VxAudioProcessor::maxModuleInstanceCount - 1, instanceIndex);
    }

    return 0;
}

int moduleInstanceIndexFromId(const juce::String& moduleId) noexcept
{
    const auto module = activeModuleFromId(moduleId);

    if (module == VxAudioProcessor::ActiveModule::eqe)
        return moduleInstanceIndexFromId(moduleId, VxAudioProcessor::eqeModuleId);

    if (module == VxAudioProcessor::ActiveModule::spe)
        return moduleInstanceIndexFromId(moduleId, VxAudioProcessor::speModuleId);

    if (module == VxAudioProcessor::ActiveModule::mxe)
        return moduleInstanceIndexFromId(moduleId, VxAudioProcessor::mxeModuleId);

    if (module == VxAudioProcessor::ActiveModule::tse)
        return moduleInstanceIndexFromId(moduleId, VxAudioProcessor::tseModuleId);

    return -1;
}

juce::String moduleStateKeyForInstance(const char* moduleId, const int instanceIndex)
{
    return "vx." + juce::String(moduleId) + "_" + juce::String::charToString(static_cast<juce_wchar>('a' + juce::jmax(0, instanceIndex))) + "_state";
}

juce::String moduleIdWithInstance(const char* moduleId, const int instanceIndex)
{
    return juce::String(moduleId) + "-" + juce::String::charToString(static_cast<juce_wchar>('a' + juce::jlimit(0, VxAudioProcessor::maxModuleInstanceCount - 1, instanceIndex)));
}

const char* moduleIdForModule(const VxAudioProcessor::ActiveModule module) noexcept
{
    switch (module)
    {
        case VxAudioProcessor::ActiveModule::eqe: return VxAudioProcessor::eqeModuleId;
        case VxAudioProcessor::ActiveModule::spe: return VxAudioProcessor::speModuleId;
        case VxAudioProcessor::ActiveModule::mxe: return VxAudioProcessor::mxeModuleId;
        case VxAudioProcessor::ActiveModule::tse: return VxAudioProcessor::tseModuleId;
        case VxAudioProcessor::ActiveModule::none: break;
    }

    return "";
}

juce::String normalizeModuleChainState(const juce::String& text)
{
    juce::StringArray modules;
    const auto tokens = juce::StringArray::fromTokens(text, ",", "");

    for (const auto& token : tokens)
    {
        const auto trimmed = token.trim();

        const auto module = activeModuleFromId(trimmed);

        if (module == VxAudioProcessor::ActiveModule::none)
            continue;

        modules.addIfNotAlreadyThere(moduleIdWithInstance(moduleIdForModule(module), moduleInstanceIndexFromId(trimmed)));
    }

    return modules.joinIntoString(",");
}

}

void VxAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto editorWidth = lastEditorWidth.load(std::memory_order_relaxed);
    const auto editorHeight = lastEditorHeight.load(std::memory_order_relaxed);

    if (editorWidth > 0 && editorHeight > 0)
    {
        parameters.state.setProperty(editorWidthStateKey, editorWidth, nullptr);
        parameters.state.setProperty(editorHeightStateKey, editorHeight, nullptr);
    }

    auto state = parameters.copyState();
    state.setProperty(activeBellCountStateKey, getActiveBellCount(), nullptr);

    auto moduleInstanceInChain = [this] (const ActiveModule module, const int instanceIndex)
    {
        for (int slotIndex = 0; slotIndex < getLoadedModuleCount(); ++slotIndex)
        {
            const auto slot = getLoadedModuleSlotAtPosition(slotIndex);

            if (slot.module == module && slot.instanceIndex == instanceIndex)
                return true;
        }

        return false;
    };

    switch (getActiveModule())
    {
        case ActiveModule::eqe:
        case ActiveModule::spe:
        case ActiveModule::mxe:
        case ActiveModule::tse:
            state.setProperty(activeModuleStateKey,
                              moduleIdWithInstance(moduleIdForModule(getActiveModule()), getActiveModuleInstanceIndex()),
                              nullptr);
            break;

        case ActiveModule::none:
            state.removeProperty(activeModuleStateKey, nullptr);
            break;
    }

    const auto normalizedModuleChain = normalizeModuleChainState(encodeModuleChainStateText());

    if (normalizedModuleChain.isNotEmpty())
        state.setProperty(moduleChainStateKey, normalizedModuleChain, nullptr);
    else
        state.removeProperty(moduleChainStateKey, nullptr);

        for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
        {
            auto* eqeModuleProcessor = getEqeModuleProcessor(instanceIndex);
            const auto stateKey = moduleStateKeyForInstance(eqeModuleId, instanceIndex);

            if (eqeModuleProcessor == nullptr || ! moduleInstanceInChain(ActiveModule::eqe, instanceIndex))
            {
                state.removeProperty(stateKey, nullptr);
                continue;
            }

            juce::MemoryBlock eqeStateData;
            eqeModuleProcessor->getStateInformation(eqeStateData);

            if (eqeStateData.getSize() > 0)
                state.setProperty(stateKey, eqeStateData.toBase64Encoding(), nullptr);
            else
                state.removeProperty(stateKey, nullptr);
        }

        for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
        {
            auto* processor = getSpeModuleProcessor(instanceIndex);
            const auto stateKey = moduleStateKeyForInstance(speModuleId, instanceIndex);

            if (processor == nullptr || ! moduleInstanceInChain(ActiveModule::spe, instanceIndex))
            {
                state.removeProperty(stateKey, nullptr);
                continue;
            }

            const auto stateXml = processor->getStateXmlString();

            if (stateXml.isNotEmpty())
                state.setProperty(stateKey, stateXml, nullptr);
            else
                state.removeProperty(stateKey, nullptr);
        }

        for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
        {
            auto* processor = getMxeModuleProcessor(instanceIndex);
            const auto stateKey = moduleStateKeyForInstance(mxeModuleId, instanceIndex);

            if (processor == nullptr || ! moduleInstanceInChain(ActiveModule::mxe, instanceIndex))
            {
                state.removeProperty(stateKey, nullptr);
                continue;
            }

            juce::MemoryBlock stateData;
            processor->getStateInformation(stateData);

            if (stateData.getSize() > 0)
                state.setProperty(stateKey, stateData.toBase64Encoding(), nullptr);
            else
                state.removeProperty(stateKey, nullptr);
        }

        for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
        {
            auto* processor = getTseModuleProcessor(instanceIndex);
            const auto stateKey = moduleStateKeyForInstance(tseModuleId, instanceIndex);

            if (processor == nullptr || ! moduleInstanceInChain(ActiveModule::tse, instanceIndex))
            {
                state.removeProperty(stateKey, nullptr);
                continue;
            }

            const auto stateXml = processor->getStateXmlString();

            if (stateXml.isNotEmpty())
                state.setProperty(stateKey, stateXml, nullptr);
            else
                state.removeProperty(stateKey, nullptr);
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
            auto restoredState = juce::ValueTree::fromXml(*stateXml);
            const auto restoredActiveModuleText = restoredState.getProperty(activeModuleStateKey).toString();
            const auto restoredActiveModule = activeModuleFromId(restoredActiveModuleText);
            const auto restoredActiveModuleInstanceIndex = moduleInstanceIndexFromId(restoredActiveModuleText);
            const auto restoredModuleChain = normalizeModuleChainState(restoredState.getProperty(moduleChainStateKey).toString());
            const auto speStateXml = restoredState.getProperty(speModuleStateKey).toString();
            const auto mxeStateBase64 = restoredState.getProperty(mxeModuleStateKey).toString();
            const auto tseStateXml = restoredState.getProperty(tseModuleStateKey).toString();

            if (restoredModuleChain.isNotEmpty())
                restoredState.setProperty(moduleChainStateKey, restoredModuleChain, nullptr);
            else if (restoredActiveModule != ActiveModule::none)
                restoredState.setProperty(moduleChainStateKey,
                                          moduleIdWithInstance(moduleIdForModule(restoredActiveModule),
                                                               restoredActiveModuleInstanceIndex),
                                          nullptr);

            parameters.replaceState(restoredState);
            const auto moduleChainText = parameters.state.getProperty(moduleChainStateKey).toString();

            restoreModuleChainFromStateText(moduleChainText);

            auto resolvedActiveModule = restoredActiveModule;
            auto resolvedActiveModuleInstanceIndex = restoredActiveModuleInstanceIndex;

            if (resolvedActiveModule == ActiveModule::none)
            {
                const auto chainText = parameters.state.getProperty(moduleChainStateKey).toString();

                const auto tokens = juce::StringArray::fromTokens(chainText, ",", "");

                if (! tokens.isEmpty())
                {
                    resolvedActiveModule = activeModuleFromId(tokens[0]);
                    resolvedActiveModuleInstanceIndex = moduleInstanceIndexFromId(tokens[0]);
                }
                else if (chainText.containsIgnoreCase(eqeModuleId))
                    resolvedActiveModule = ActiveModule::eqe;
                else if (chainText.containsIgnoreCase(speModuleId))
                    resolvedActiveModule = ActiveModule::spe;
                else if (chainText.containsIgnoreCase(mxeModuleId))
                    resolvedActiveModule = ActiveModule::mxe;
                else if (chainText.containsIgnoreCase(tseModuleId))
                    resolvedActiveModule = ActiveModule::tse;
            }

            for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
            {
                auto* eqeModuleProcessor = getEqeModuleProcessor(instanceIndex);
                const auto eqeStateBase64 = restoredState.getProperty(moduleStateKeyForInstance(eqeModuleId, instanceIndex)).toString();

                if (eqeModuleProcessor == nullptr || eqeStateBase64.isEmpty())
                    continue;

                juce::MemoryBlock eqeStateData;

                if (eqeStateData.fromBase64Encoding(eqeStateBase64))
                    eqeModuleProcessor->setStateInformation(eqeStateData.getData(), static_cast<int>(eqeStateData.getSize()));
            }

            for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
            {
                auto* processor = getSpeModuleProcessor(instanceIndex);
                auto speStateXmlForInstance = restoredState.getProperty(moduleStateKeyForInstance(speModuleId, instanceIndex)).toString();

                if (instanceIndex == 0 && speStateXmlForInstance.isEmpty())
                    speStateXmlForInstance = speStateXml;

                if (processor != nullptr && speStateXmlForInstance.isNotEmpty())
                    processor->setStateFromXmlString(speStateXmlForInstance);
            }

            for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
            {
                auto* processor = getMxeModuleProcessor(instanceIndex);
                auto stateBase64 = restoredState.getProperty(moduleStateKeyForInstance(mxeModuleId, instanceIndex)).toString();

                if (instanceIndex == 0 && stateBase64.isEmpty())
                    stateBase64 = mxeStateBase64;

                if (processor == nullptr || stateBase64.isEmpty())
                    continue;

                juce::MemoryBlock stateData;

                if (stateData.fromBase64Encoding(stateBase64))
                    processor->setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
            }

            for (int instanceIndex = 0; instanceIndex < maxModuleInstanceCount; ++instanceIndex)
            {
                auto* processor = getTseModuleProcessor(instanceIndex);
                auto tseStateXmlForInstance = restoredState.getProperty(moduleStateKeyForInstance(tseModuleId, instanceIndex)).toString();

                if (instanceIndex == 0 && tseStateXmlForInstance.isEmpty())
                    tseStateXmlForInstance = tseStateXml;

                if (processor != nullptr && tseStateXmlForInstance.isNotEmpty())
                    processor->setStateFromXmlString(tseStateXmlForInstance);
            }

            if (! setActiveModuleIfPresent(resolvedActiveModule, resolvedActiveModuleInstanceIndex))
            {
                const auto firstSlot = getLoadedModuleCount() > 0 ? getLoadedModuleSlotAtPosition(0)
                                                                  : ModuleSlot {};
                setActiveModule(firstSlot.module, firstSlot.instanceIndex);
            }

            updateShellLatency();
            setLastEditorSize(static_cast<int>(parameters.state.getProperty(editorWidthStateKey, 0)),
                              static_cast<int>(parameters.state.getProperty(editorHeightStateKey, 0)));

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
