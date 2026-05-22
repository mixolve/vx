#include "shell.Processor.h"
#include "../modules/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/tse/module.tse.TseProcessor.h"
#include "shell.ShellState.h"

#include <iterator>
#include <utility>
#include <vector>

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

juce::String moduleIdForSlot(const VxAudioProcessor::ModuleSlot& slot)
{
    const auto suffix = juce::String::charToString(static_cast<juce_wchar>('a' + juce::jlimit(0, VxAudioProcessor::maxModuleInstanceCount - 1, slot.instanceIndex)));

    switch (slot.module)
    {
        case VxAudioProcessor::ActiveModule::eqe:
            return juce::String(VxAudioProcessor::eqeModuleId) + "-" + suffix;

        case VxAudioProcessor::ActiveModule::spe:
            return juce::String(VxAudioProcessor::speModuleId) + "-" + suffix;

        case VxAudioProcessor::ActiveModule::mxe:
            return juce::String(VxAudioProcessor::mxeModuleId) + "-" + suffix;

        case VxAudioProcessor::ActiveModule::tse:
            return juce::String(VxAudioProcessor::tseModuleId) + "-" + suffix;

        case VxAudioProcessor::ActiveModule::none:
            break;
    }

    return {};
}

int moduleInstanceIndexFromId(const juce::String& moduleId, const juce::String& prefix) noexcept
{
    const auto trimmed = moduleId.trim();

    if (trimmed.startsWithIgnoreCase(prefix + "-") || trimmed.startsWithIgnoreCase(prefix + "_"))
    {
        const auto suffix = trimmed.substring(prefix.length() + 1, prefix.length() + 2).toLowerCase();
        const auto instanceIndex = suffix.isNotEmpty() ? static_cast<int>(suffix[0] - static_cast<juce_wchar>('a')) : 0;

        if (juce::isPositiveAndBelow(instanceIndex, VxAudioProcessor::maxModuleInstanceCount))
            return instanceIndex;
    }

    return 0;
}

VxAudioProcessor::ModuleSlot moduleSlotFromId(const juce::String& moduleId)
{
    const auto trimmed = moduleId.trim();

    if (trimmed.startsWithIgnoreCase("eqe-") || trimmed.startsWithIgnoreCase("eqe_"))
    {
        const auto instanceIndex = moduleInstanceIndexFromId(trimmed, VxAudioProcessor::eqeModuleId);

        if (juce::isPositiveAndBelow(instanceIndex, VxAudioProcessor::maxModuleInstanceCount))
            return { VxAudioProcessor::ActiveModule::eqe, instanceIndex };
    }

    if (trimmed.startsWithIgnoreCase("spe-") || trimmed.startsWithIgnoreCase("spe_"))
        return { VxAudioProcessor::ActiveModule::spe, moduleInstanceIndexFromId(trimmed, VxAudioProcessor::speModuleId) };

    if (trimmed.startsWithIgnoreCase("mxe-") || trimmed.startsWithIgnoreCase("mxe_"))
        return { VxAudioProcessor::ActiveModule::mxe, moduleInstanceIndexFromId(trimmed, VxAudioProcessor::mxeModuleId) };

    if (trimmed.startsWithIgnoreCase("tse-") || trimmed.startsWithIgnoreCase("tse_"))
        return { VxAudioProcessor::ActiveModule::tse, moduleInstanceIndexFromId(trimmed, VxAudioProcessor::tseModuleId) };

    if (trimmed.equalsIgnoreCase(VxAudioProcessor::eqeModuleId))
        return { VxAudioProcessor::ActiveModule::eqe, 0 };

    if (trimmed.equalsIgnoreCase(VxAudioProcessor::speModuleId))
        return { VxAudioProcessor::ActiveModule::spe, 0 };

    if (trimmed.equalsIgnoreCase(VxAudioProcessor::mxeModuleId))
        return { VxAudioProcessor::ActiveModule::mxe, 0 };

    if (trimmed.equalsIgnoreCase(VxAudioProcessor::tseModuleId))
        return { VxAudioProcessor::ActiveModule::tse, 0 };

    return {};
}

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
    return activeModule.load(std::memory_order_relaxed);
}

void VxAudioProcessor::setActiveModule(const ActiveModule module)
{
    setActiveModule(module, module == ActiveModule::none ? -1 : 0);
}

void VxAudioProcessor::setActiveModule(const ActiveModule module, const int instanceIndex)
{
    activeModule.store(module, std::memory_order_relaxed);
    activeModuleInstanceIndex.store(module == ActiveModule::none ? -1 : juce::jmax(0, instanceIndex),
                                    std::memory_order_relaxed);

    const ModuleSlot slot { module, activeModuleInstanceIndex.load(std::memory_order_relaxed) };
    const auto activeId = moduleIdForSlot(slot);

    if (activeId.isNotEmpty())
        parameters.state.setProperty(activeModuleStateKey, activeId, nullptr);
    else
        parameters.state.removeProperty(activeModuleStateKey, nullptr);

}

bool VxAudioProcessor::setActiveModuleIfPresent(const ActiveModule module, const int instanceIndex)
{
    if (module == ActiveModule::none)
    {
        setActiveModule(ActiveModule::none, -1);
        return true;
    }

    for (const auto& slot : moduleChain)
    {
        if (slot.module == module && slot.instanceIndex == instanceIndex)
        {
            setActiveModule(module, instanceIndex);
            return true;
        }
    }

    return false;
}

bool VxAudioProcessor::addModuleToChain(const ActiveModule module)
{
    if (module == ActiveModule::none || static_cast<int>(moduleChain.size()) >= maxModuleSlotCount)
        return false;

    const ScopedProcessingSuspend suspendGuard(*this);

    auto instanceIndex = -1;

    if (module == ActiveModule::eqe)
    {
        instanceIndex = createEqeModuleInstance();
    }
    else if (module == ActiveModule::spe)
    {
        instanceIndex = createSpeModuleInstance();
    }
    else if (module == ActiveModule::mxe)
    {
        instanceIndex = createMxeModuleInstance();
    }
    else if (module == ActiveModule::tse)
    {
        instanceIndex = createTseModuleInstance();
    }

    if (instanceIndex < 0)
        return false;

    moduleChain.push_back({ module, instanceIndex });
    storeModuleChainStateProperty();
    setActiveModule(module, instanceIndex);
    updateShellLatency();
    return true;
}

bool VxAudioProcessor::removeModuleFromChain(const ActiveModule module)
{
    return removeModuleFromChain(module, getActiveModule() == module ? getActiveModuleInstanceIndex() : -1);
}

bool VxAudioProcessor::removeModuleFromChain(const ActiveModule module, const int instanceIndex)
{
    if (module == ActiveModule::none)
        return false;

    const ScopedProcessingSuspend suspendGuard(*this);

    for (auto iterator = moduleChain.begin(); iterator != moduleChain.end(); ++iterator)
    {
        const auto matchesModule = iterator->module == module;
        const auto matchesInstance = instanceIndex < 0 || iterator->instanceIndex == instanceIndex;

        if (! matchesModule || ! matchesInstance)
            continue;

        const auto removedPosition = static_cast<int>(std::distance(moduleChain.begin(), iterator));
        moduleChain.erase(iterator);
        storeModuleChainStateProperty();

        if (moduleChain.empty())
        {
            setActiveModule(ActiveModule::none, -1);
            updateShellLatency();
            return true;
        }

        const auto nextPosition = juce::jlimit(0, static_cast<int>(moduleChain.size()) - 1, removedPosition);
        const auto nextSlot = moduleChain[static_cast<size_t>(nextPosition)];
        setActiveModule(nextSlot.module, nextSlot.instanceIndex);
        updateShellLatency();
        return true;
    }

    return false;
}

bool VxAudioProcessor::moveModuleInChainAtPosition(const int position, const int direction)
{
    const auto destinationPosition = position + direction;

    if (direction == 0
        || ! juce::isPositiveAndBelow(position, static_cast<int>(moduleChain.size()))
        || ! juce::isPositiveAndBelow(destinationPosition, static_cast<int>(moduleChain.size())))
        return false;

    const ScopedProcessingSuspend suspendGuard(*this);
    std::swap(moduleChain[static_cast<size_t>(position)],
              moduleChain[static_cast<size_t>(destinationPosition)]);
    storeModuleChainStateProperty();
    updateShellLatency();
    return true;
}

int VxAudioProcessor::getLoadedModuleCount() const noexcept
{
    return static_cast<int>(moduleChain.size());
}

VxAudioProcessor::ModuleSlot VxAudioProcessor::getLoadedModuleSlotAtPosition(const int position) const noexcept
{
    if (! juce::isPositiveAndBelow(position, static_cast<int>(moduleChain.size())))
        return {};

    return moduleChain[static_cast<size_t>(position)];
}

juce::String VxAudioProcessor::getLoadedModuleLabelAtPosition(const int position) const
{
    const auto slot = getLoadedModuleSlotAtPosition(position);

    switch (slot.module)
    {
        case ActiveModule::eqe:
        {
            auto moduleInstanceCount = 0;

            for (const auto& moduleSlot : moduleChain)
                if (moduleSlot.module == slot.module)
                    ++moduleInstanceCount;

            if (moduleInstanceCount < 2)
                return "EQE";

            return "EQE-" + juce::String::charToString(static_cast<juce_wchar>('A' + juce::jlimit(0, maxModuleInstanceCount - 1, slot.instanceIndex)));
        }

        case ActiveModule::spe:
        case ActiveModule::mxe:
        case ActiveModule::tse:
        {
            auto moduleInstanceCount = 0;

            for (const auto& moduleSlot : moduleChain)
                if (moduleSlot.module == slot.module)
                    ++moduleInstanceCount;

            const auto prefix = slot.module == ActiveModule::spe ? juce::String("SPE")
                : slot.module == ActiveModule::mxe ? juce::String("MXE")
                                                   : juce::String("TSE");

            if (moduleInstanceCount < 2)
                return prefix;

            return prefix + "-" + juce::String::charToString(static_cast<juce_wchar>('A' + juce::jlimit(0, maxModuleInstanceCount - 1, slot.instanceIndex)));
        }

        case ActiveModule::none: break;
    }

    return {};
}

bool VxAudioProcessor::hasTtssSourceForActiveEqeModule() const noexcept
{
    if (getActiveModule() != ActiveModule::eqe)
        return false;

    for (const auto& slot : moduleChain)
        if (slot.module == ActiveModule::tse && slot.instanceIndex == 0)
            if (const auto* tseProcessor = getTseModuleProcessor(0))
                return tseProcessor->canProvideSplit();

    return false;
}

int VxAudioProcessor::getActiveModuleInstanceIndex() const noexcept
{
    return activeModuleInstanceIndex.load(std::memory_order_relaxed);
}

bool VxAudioProcessor::isEqeModuleLoaded() const noexcept
{
    for (const auto& slot : moduleChain)
        if (slot.module == ActiveModule::eqe)
            return true;

    return false;
}

bool VxAudioProcessor::isSpeModuleLoaded() const noexcept
{
    for (const auto& slot : moduleChain)
        if (slot.module == ActiveModule::spe)
            return true;

    return false;
}

SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor() noexcept
{
    return getSpeModuleProcessor(getActiveModule() == ActiveModule::spe ? getActiveModuleInstanceIndex() : 0);
}

const SpeModuleProcessor* VxAudioProcessor::getSpeModuleProcessor() const noexcept
{
    return getSpeModuleProcessor(getActiveModule() == ActiveModule::spe ? getActiveModuleInstanceIndex() : 0);
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
    for (const auto& slot : moduleChain)
        if (slot.module == ActiveModule::mxe)
            return true;

    return false;
}

MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor() noexcept
{
    return getMxeModuleProcessor(getActiveModule() == ActiveModule::mxe ? getActiveModuleInstanceIndex() : 0);
}

const MxeAudioProcessor* VxAudioProcessor::getMxeModuleProcessor() const noexcept
{
    return getMxeModuleProcessor(getActiveModule() == ActiveModule::mxe ? getActiveModuleInstanceIndex() : 0);
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
    for (const auto& slot : moduleChain)
        if (slot.module == ActiveModule::tse)
            return true;

    return false;
}

TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor() noexcept
{
    return getTseModuleProcessor(getActiveModule() == ActiveModule::tse ? getActiveModuleInstanceIndex() : 0);
}

const TseModuleProcessor* VxAudioProcessor::getTseModuleProcessor() const noexcept
{
    return getTseModuleProcessor(getActiveModule() == ActiveModule::tse ? getActiveModuleInstanceIndex() : 0);
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
    auto instanceIndex = -1;

    for (int candidate = 0; candidate < maxModuleInstanceCount; ++candidate)
    {
        auto used = false;

        for (const auto& slot : moduleChain)
            used = used || (slot.module == ActiveModule::eqe && slot.instanceIndex == candidate);

        if (! used)
        {
            instanceIndex = candidate;
            break;
        }
    }

    if (instanceIndex < 0)
        return -1;

    std::unique_ptr<EqeModuleProcessor> moduleProcessor;

    if (juce::isPositiveAndBelow(instanceIndex, static_cast<int>(eqeModuleProcessors.size())))
        moduleProcessor = std::move(eqeModuleProcessors[static_cast<size_t>(instanceIndex)]);

    if (moduleProcessor == nullptr)
        moduleProcessor = std::make_unique<EqeModuleProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    if (static_cast<int>(eqeModuleProcessors.size()) <= instanceIndex)
        eqeModuleProcessors.resize(static_cast<size_t>(instanceIndex + 1));

    eqeModuleProcessors[static_cast<size_t>(instanceIndex)] = std::move(moduleProcessor);
    return instanceIndex;
}

int VxAudioProcessor::createSpeModuleInstance()
{
    auto instanceIndex = -1;

    for (int candidate = 0; candidate < maxModuleInstanceCount; ++candidate)
    {
        auto used = false;

        for (const auto& slot : moduleChain)
            used = used || (slot.module == ActiveModule::spe && slot.instanceIndex == candidate);

        if (! used)
        {
            instanceIndex = candidate;
            break;
        }
    }

    if (instanceIndex < 0)
        return -1;

    auto moduleProcessor = std::make_unique<SpeModuleProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    if (static_cast<int>(speModuleProcessors.size()) <= instanceIndex)
        speModuleProcessors.resize(static_cast<size_t>(instanceIndex + 1));

    speModuleProcessors[static_cast<size_t>(instanceIndex)] = std::move(moduleProcessor);
    return instanceIndex;
}

int VxAudioProcessor::createMxeModuleInstance()
{
    auto instanceIndex = -1;

    for (int candidate = 0; candidate < maxModuleInstanceCount; ++candidate)
    {
        auto used = false;

        for (const auto& slot : moduleChain)
            used = used || (slot.module == ActiveModule::mxe && slot.instanceIndex == candidate);

        if (! used)
        {
            instanceIndex = candidate;
            break;
        }
    }

    if (instanceIndex < 0)
        return -1;

    auto moduleProcessor = std::make_unique<MxeAudioProcessor>();

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    if (static_cast<int>(mxeModuleProcessors.size()) <= instanceIndex)
        mxeModuleProcessors.resize(static_cast<size_t>(instanceIndex + 1));

    mxeModuleProcessors[static_cast<size_t>(instanceIndex)] = std::move(moduleProcessor);
    return instanceIndex;
}

int VxAudioProcessor::createTseModuleInstance()
{
    auto instanceIndex = -1;

    for (int candidate = 0; candidate < maxModuleInstanceCount; ++candidate)
    {
        auto used = false;

        for (const auto& slot : moduleChain)
            used = used || (slot.module == ActiveModule::tse && slot.instanceIndex == candidate);

        if (! used)
        {
            instanceIndex = candidate;
            break;
        }
    }

    if (instanceIndex < 0)
        return -1;

    auto moduleProcessor = std::make_unique<TseModuleProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
        moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

    if (static_cast<int>(tseModuleProcessors.size()) <= instanceIndex)
        tseModuleProcessors.resize(static_cast<size_t>(instanceIndex + 1));

    tseModuleProcessors[static_cast<size_t>(instanceIndex)] = std::move(moduleProcessor);
    return instanceIndex;
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

    return getEqeModuleProcessor(getActiveModuleInstanceIndex());
}

const EqeModuleProcessor* VxAudioProcessor::getActiveEqeModuleProcessor() const noexcept
{
    if (getActiveModule() != ActiveModule::eqe)
        return nullptr;

    return getEqeModuleProcessor(getActiveModuleInstanceIndex());
}

juce::String VxAudioProcessor::encodeModuleChainStateText() const
{
    juce::StringArray modules;

    for (const auto& slot : moduleChain)
    {
        const auto id = moduleIdForSlot(slot);

        if (id.isNotEmpty())
            modules.add(id);
    }

    return modules.joinIntoString(",");
}

void VxAudioProcessor::storeModuleChainStateProperty()
{
    const auto encoded = encodeModuleChainStateText();

    if (encoded.isEmpty())
        parameters.state.removeProperty(moduleChainStateKey, nullptr);
    else
        parameters.state.setProperty(moduleChainStateKey, encoded, nullptr);
}

void VxAudioProcessor::restoreModuleChainFromStateText(const juce::String& text)
{
    const ScopedProcessingSuspend suspendGuard(*this);
    moduleChain.clear();
    eqeModuleProcessors.clear();
    speModuleProcessors.clear();
    mxeModuleProcessors.clear();
    tseModuleProcessors.clear();

    eqeModuleProcessors.resize(1);
    eqeModuleProcessors[0] = std::make_unique<EqeModuleProcessor>(*this);
    speModuleProcessors.resize(1);
    speModuleProcessors[0] = std::make_unique<SpeModuleProcessor>(*this);
    mxeModuleProcessors.resize(1);
    mxeModuleProcessors[0] = std::make_unique<MxeAudioProcessor>();
    tseModuleProcessors.resize(1);
    tseModuleProcessors[0] = std::make_unique<TseModuleProcessor>(*this);

    if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
    {
        eqeModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);
        speModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);
        mxeModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);
        tseModuleProcessors[0]->prepareToPlay(currentSampleRate, lastProcessedBlockSize);
    }

    const auto tokens = juce::StringArray::fromTokens(text, ",", "");

    for (const auto& token : tokens)
    {
        if (static_cast<int>(moduleChain.size()) >= maxModuleSlotCount)
            break;

        auto slot = moduleSlotFromId(token);

        if (slot.module == ActiveModule::none)
            continue;

        if (slot.module == ActiveModule::eqe)
        {
            while (static_cast<int>(eqeModuleProcessors.size()) <= slot.instanceIndex
                   && static_cast<int>(eqeModuleProcessors.size()) < maxModuleInstanceCount)
            {
                auto moduleProcessor = std::make_unique<EqeModuleProcessor>(*this);

                if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
                    moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

                eqeModuleProcessors.push_back(std::move(moduleProcessor));
            }

            if (! juce::isPositiveAndBelow(slot.instanceIndex, static_cast<int>(eqeModuleProcessors.size())))
                continue;
        }
        else if (slot.module == ActiveModule::spe)
        {
            while (static_cast<int>(speModuleProcessors.size()) <= slot.instanceIndex
                   && static_cast<int>(speModuleProcessors.size()) < maxModuleInstanceCount)
            {
                auto moduleProcessor = std::make_unique<SpeModuleProcessor>(*this);

                if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
                    moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

                speModuleProcessors.push_back(std::move(moduleProcessor));
            }

            if (! juce::isPositiveAndBelow(slot.instanceIndex, static_cast<int>(speModuleProcessors.size())))
                continue;
        }
        else if (slot.module == ActiveModule::mxe)
        {
            while (static_cast<int>(mxeModuleProcessors.size()) <= slot.instanceIndex
                   && static_cast<int>(mxeModuleProcessors.size()) < maxModuleInstanceCount)
            {
                auto moduleProcessor = std::make_unique<MxeAudioProcessor>();

                if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
                    moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

                mxeModuleProcessors.push_back(std::move(moduleProcessor));
            }

            if (! juce::isPositiveAndBelow(slot.instanceIndex, static_cast<int>(mxeModuleProcessors.size())))
                continue;
        }
        else if (slot.module == ActiveModule::tse)
        {
            while (static_cast<int>(tseModuleProcessors.size()) <= slot.instanceIndex
                   && static_cast<int>(tseModuleProcessors.size()) < maxModuleInstanceCount)
            {
                auto moduleProcessor = std::make_unique<TseModuleProcessor>(*this);

                if (currentSampleRate > 0.0 && lastProcessedBlockSize > 0)
                    moduleProcessor->prepareToPlay(currentSampleRate, lastProcessedBlockSize);

                tseModuleProcessors.push_back(std::move(moduleProcessor));
            }

            if (! juce::isPositiveAndBelow(slot.instanceIndex, static_cast<int>(tseModuleProcessors.size())))
                continue;
        }

        moduleChain.push_back(slot);
    }

    storeModuleChainStateProperty();
}

int VxAudioProcessor::getActiveBellCount() const noexcept
{
    if (const auto* activeEqeModule = getActiveEqeModuleProcessor())
        return activeEqeModule->getActiveBellCount();

    return 0;
}
