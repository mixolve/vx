#include "module.mxe.PluginProcessor.h"

#include "module.mxe.ParameterIds.h"

#include <array>
#include <cmath>
#include <optional>

MxeAudioProcessor::MxeAudioProcessor(juce::AudioProcessor& ownerProcessor)
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    juce::ignoreUnused(ownerProcessor);
    cacheParameterPointers();
    registerParameterListeners();
}

MxeAudioProcessor::~MxeAudioProcessor()
{
    unregisterParameterListeners();
}

void MxeAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    multibandProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    syncParameters(true);
    multibandProcessor.reset();
}

void MxeAudioProcessor::releaseResources()
{
}

bool MxeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    return mainOutput == juce::AudioChannelSet::mono()
        || mainOutput == juce::AudioChannelSet::stereo();
}

void MxeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    syncParameters();

    if (isModuleBypassEnabled())
        return;

    if (isModuleBypassWithGainEnabled())
    {
        applyFullbandOutGain(buffer);
        return;
    }

    multibandProcessor.process(buffer);
}

juce::AudioProcessorEditor* MxeAudioProcessor::createEditor()
{
    return nullptr;
}

bool MxeAudioProcessor::hasEditor() const
{
    return false;
}

const juce::String MxeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MxeAudioProcessor::acceptsMidi() const
{
    return false;
}

bool MxeAudioProcessor::producesMidi() const
{
    return false;
}

bool MxeAudioProcessor::isMidiEffect() const
{
    return false;
}

double MxeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MxeAudioProcessor::getNumPrograms()
{
    return 1;
}

int MxeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MxeAudioProcessor::setCurrentProgram(const int index)
{
    juce::ignoreUnused(index);
}

const juce::String MxeAudioProcessor::getProgramName(const int index)
{
    juce::ignoreUnused(index);
    return {};
}

void MxeAudioProcessor::changeProgramName(const int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void MxeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = valueTreeState.copyState().createXml())
        copyXmlToBinary(*stateXml, destData);
}

void MxeAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(valueTreeState.state.getType()))
        {
            valueTreeState.replaceState(juce::ValueTree::fromXml(*xmlState));

            using mxe::parameters::makeBandParameterId;
            using mxe::parameters::parameterSpecs;
            using mxe::parameters::ParameterSlot;
            using mxe::parameters::toIndex;

            const auto setBandSlotValue = [this] (const size_t bandIndex,
                                                  const ParameterSlot targetSlot,
                                                  const float targetValue)
            {
                const auto targetIndex = toIndex(targetSlot);
                auto* target = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(
                    makeBandParameterId(bandIndex, parameterSpecs[targetIndex].suffix)));

                if (target == nullptr)
                    return;

                const auto normalizedValue = target->convertTo0to1(targetValue);

                if (std::abs(target->getValue() - normalizedValue) <= 1.0e-6f)
                    return;

                target->setValueNotifyingHost(normalizedValue);
            };

            const auto readBandSlotValue = [this] (const size_t bandIndex, const ParameterSlot slot) -> float
            {
                const auto* value = rawBandParameters[bandIndex][toIndex(slot)];
                return value != nullptr ? value->load(std::memory_order_relaxed) : 0.0f;
            };

            for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
            {
                auto* linkLr = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(
                    makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::linkLr)].suffix)));
                auto* linkUpDn = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(
                    makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::linkUpDn)].suffix)));

                if (linkLr == nullptr || linkUpDn == nullptr)
                    continue;

                const auto linkLrOn = linkLr->convertFrom0to1(linkLr->getValue()) >= 0.5f;
                const auto linkUpDnOn = linkUpDn->convertFrom0to1(linkUpDn->getValue()) >= 0.5f;

                if (linkLrOn && linkUpDnOn)
                    linkUpDn->setValueNotifyingHost(linkUpDn->convertTo0to1(0.0f));

                const auto syncAllFieldSlots = [&] (const ParameterSlot lu,
                                                    const ParameterSlot ld,
                                                    const ParameterSlot ru,
                                                    const ParameterSlot rd)
                {
                    const auto masterValue = readBandSlotValue(bandIndex, lu);
                    setBandSlotValue(bandIndex, lu, masterValue);
                    setBandSlotValue(bandIndex, ld, masterValue);
                    setBandSlotValue(bandIndex, ru, masterValue);
                    setBandSlotValue(bandIndex, rd, masterValue);
                };

                const auto syncUpDnPairs = [&] (const ParameterSlot lu,
                                                const ParameterSlot ld,
                                                const ParameterSlot ru,
                                                const ParameterSlot rd)
                {
                    const auto leftValue = readBandSlotValue(bandIndex, lu);
                    const auto rightValue = readBandSlotValue(bandIndex, ru);
                    setBandSlotValue(bandIndex, lu, leftValue);
                    setBandSlotValue(bandIndex, ld, leftValue);
                    setBandSlotValue(bandIndex, ru, rightValue);
                    setBandSlotValue(bandIndex, rd, rightValue);
                };

                const auto effectiveLinkLrOn = linkLr->convertFrom0to1(linkLr->getValue()) >= 0.5f;
                const auto effectiveLinkUpDnOn = linkUpDn->convertFrom0to1(linkUpDn->getValue()) >= 0.5f;

                if (effectiveLinkLrOn)
                {
                    syncAllFieldSlots(ParameterSlot::thLU, ParameterSlot::thLD, ParameterSlot::thRU, ParameterSlot::thRD);
                    syncAllFieldSlots(ParameterSlot::tensLU, ParameterSlot::tensLD, ParameterSlot::tensRU, ParameterSlot::tensRD);
                    syncAllFieldSlots(ParameterSlot::relLU, ParameterSlot::relLD, ParameterSlot::relRU, ParameterSlot::relRD);
                    syncAllFieldSlots(ParameterSlot::outLU, ParameterSlot::outLD, ParameterSlot::outRU, ParameterSlot::outRD);
                }
                else if (effectiveLinkUpDnOn)
                {
                    syncUpDnPairs(ParameterSlot::thLU, ParameterSlot::thLD, ParameterSlot::thRU, ParameterSlot::thRD);
                    syncUpDnPairs(ParameterSlot::tensLU, ParameterSlot::tensLD, ParameterSlot::tensRU, ParameterSlot::tensRD);
                    syncUpDnPairs(ParameterSlot::relLU, ParameterSlot::relLD, ParameterSlot::relRU, ParameterSlot::relRD);
                    syncUpDnPairs(ParameterSlot::outLU, ParameterSlot::outLD, ParameterSlot::outRU, ParameterSlot::outRD);
                }
            }

            if (numBands > 0)
            {
                const auto syncGlobalFromBand0 = [&] (const ParameterSlot slot)
                {
                    const auto* source = rawBandParameters[0][toIndex(slot)];

                    if (source == nullptr)
                        return;

                    const auto sourceValue = source->load(std::memory_order_relaxed);

                    for (size_t targetBand = 0; targetBand < numBands; ++targetBand)
                        setBandSlotValue(targetBand, slot, sourceValue);
                };

                syncGlobalFromBand0(ParameterSlot::moRph);
                syncGlobalFromBand0(ParameterSlot::peakHoldHz);
                syncGlobalFromBand0(ParameterSlot::TensionFlooR);
                syncGlobalFromBand0(ParameterSlot::TensionHysT);
            }

            syncExternalParameterValueTreeState();
            markParametersDirty();
            syncParameters(true);
        }
    }
}

juce::AudioProcessorValueTreeState& MxeAudioProcessor::getValueTreeState() noexcept
{
    return valueTreeState;
}

const juce::AudioProcessorValueTreeState& MxeAudioProcessor::getValueTreeState() const noexcept
{
    return valueTreeState;
}

juce::UndoManager& MxeAudioProcessor::getUndoManager() noexcept
{
    return undoManager;
}

const juce::UndoManager& MxeAudioProcessor::getUndoManager() const noexcept
{
    return undoManager;
}

int MxeAudioProcessor::getModuleLatencySamples() const noexcept
{
    return moduleLatencySamples;
}

void MxeAudioProcessor::markParametersDirty() noexcept
{
    parametersDirty.store(true, std::memory_order_relaxed);
}

void MxeAudioProcessor::setExternalParameterConnection(ExternalParameterConnection connection)
{
    externalParameterConnection = std::move(connection);
    syncExternalParameterValueTreeState();
    markParametersDirty();
}

void MxeAudioProcessor::clearExternalParameterConnection() noexcept
{
    externalParameterConnection = {};
    markParametersDirty();
}

bool MxeAudioProcessor::syncExternalParameterValueTreeState()
{
    if (! hasExternalParameterConnection())
        return false;

    auto changed = false;
    const juce::ScopedValueSetter<bool> syncGuard(mirroredExternalStateSyncInProgress, true);

    const auto syncParameter = [this, &changed] (const juce::String& parameterId, const float value)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(parameterId));

        if (parameter == nullptr)
            return;

        const auto normalisedValue = parameter->convertTo0to1(value);

        if (std::abs(parameter->getValue() - normalisedValue) <= 1.0e-6f)
            return;

        parameter->setValueNotifyingHost(normalisedValue);
        changed = true;
    };

    syncParameter(mxe::parameters::makeModuleBypassParameterId(), loadExternalBool(externalParameterConnection.moduleBypass) ? 1.0f : 0.0f);
    syncParameter(mxe::parameters::makeModuleBypassWithGainParameterId(), loadExternalBool(externalParameterConnection.moduleBypassWithGain) ? 1.0f : 0.0f);
    syncParameter(mxe::parameters::makeFullbandParameterId("inGnVisible"), loadExternalFloat(externalParameterConnection.inGainLr, 0.0f));
    syncParameter(mxe::parameters::makeFullbandParameterId("autoInLeft"), loadExternalFloat(externalParameterConnection.inGainL, 0.0f));
    syncParameter(mxe::parameters::makeFullbandParameterId("autoInRight"), loadExternalFloat(externalParameterConnection.inGainR, 0.0f));
    syncParameter(mxe::parameters::makeFullbandParameterId("wideVisible"), loadExternalFloat(externalParameterConnection.wide, 100.0f));
    syncParameter(mxe::parameters::makeFullbandParameterId("outGnVisible"), loadExternalFloat(externalParameterConnection.outGain, 0.0f));

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        const auto& band = externalParameterConnection.bandParameters[bandIndex];
        syncParameter(mxe::parameters::makeBandParameterId(bandIndex, "inGn"), loadExternalFloat(band.inGainLr, 0.0f));
        syncParameter(mxe::parameters::makeBandParameterId(bandIndex, "inLeft"), loadExternalFloat(band.inGainL, 0.0f));
        syncParameter(mxe::parameters::makeBandParameterId(bandIndex, "inRight"), loadExternalFloat(band.inGainR, 0.0f));
        syncParameter(mxe::parameters::makeBandParameterId(bandIndex, "wide"), loadExternalFloat(band.wide, 100.0f));
        syncParameter(mxe::parameters::makeBandParameterId(bandIndex, "outGn"), loadExternalFloat(band.outGain, 0.0f));
    }

    return changed;
}

void MxeAudioProcessor::registerParameterListeners()
{
    using mxe::parameters::crossoverSpecs;
    using mxe::parameters::fullbandAutomationSpecs;
    using mxe::parameters::fullbandVisibleSpecs;
    using mxe::parameters::makeActiveSplitCountParameterId;
    using mxe::parameters::makeBandParameterId;
    using mxe::parameters::makeFullbandParameterId;
    using mxe::parameters::makeModuleBypassParameterId;
    using mxe::parameters::makeModuleBypassWithGainParameterId;
    using mxe::parameters::makeSoloParameterId;
    using mxe::parameters::parameterSpecs;

    const auto addListenerIfPresent = [this] (const juce::String& parameterId)
    {
        if (valueTreeState.getParameter(parameterId) != nullptr)
            valueTreeState.addParameterListener(parameterId, this);
    };

    addListenerIfPresent(makeActiveSplitCountParameterId());
    addListenerIfPresent(makeModuleBypassParameterId());
    addListenerIfPresent(makeModuleBypassWithGainParameterId());

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        valueTreeState.addParameterListener(makeSoloParameterId(bandIndex), this);

        for (const auto& spec : parameterSpecs)
            valueTreeState.addParameterListener(makeBandParameterId(bandIndex, spec.suffix), this);
    }

    for (const auto& spec : fullbandAutomationSpecs)
        addListenerIfPresent(makeFullbandParameterId(spec.suffix));

    for (const auto& spec : fullbandVisibleSpecs)
        addListenerIfPresent(makeFullbandParameterId(spec.suffix));

    for (const auto& spec : crossoverSpecs)
        addListenerIfPresent(makeFullbandParameterId(spec.suffix));
}

void MxeAudioProcessor::unregisterParameterListeners()
{
    using mxe::parameters::crossoverSpecs;
    using mxe::parameters::fullbandAutomationSpecs;
    using mxe::parameters::fullbandVisibleSpecs;
    using mxe::parameters::makeActiveSplitCountParameterId;
    using mxe::parameters::makeBandParameterId;
    using mxe::parameters::makeFullbandParameterId;
    using mxe::parameters::makeModuleBypassParameterId;
    using mxe::parameters::makeModuleBypassWithGainParameterId;
    using mxe::parameters::makeSoloParameterId;
    using mxe::parameters::parameterSpecs;

    const auto removeListenerIfPresent = [this] (const juce::String& parameterId)
    {
        if (valueTreeState.getParameter(parameterId) != nullptr)
            valueTreeState.removeParameterListener(parameterId, this);
    };

    removeListenerIfPresent(makeActiveSplitCountParameterId());
    removeListenerIfPresent(makeModuleBypassParameterId());
    removeListenerIfPresent(makeModuleBypassWithGainParameterId());

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        valueTreeState.removeParameterListener(makeSoloParameterId(bandIndex), this);

        for (const auto& spec : parameterSpecs)
            valueTreeState.removeParameterListener(makeBandParameterId(bandIndex, spec.suffix), this);
    }

    for (const auto& spec : fullbandAutomationSpecs)
        removeListenerIfPresent(makeFullbandParameterId(spec.suffix));

    for (const auto& spec : fullbandVisibleSpecs)
        removeListenerIfPresent(makeFullbandParameterId(spec.suffix));

    for (const auto& spec : crossoverSpecs)
        removeListenerIfPresent(makeFullbandParameterId(spec.suffix));
}

void MxeAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    using mxe::parameters::makeBandParameterId;
    using mxe::parameters::parameterSpecs;
    using mxe::parameters::ParameterSlot;
    using mxe::parameters::toIndex;

    if (mirroredExternalStateSyncInProgress)
        return;

    if (linkedParameterPropagationInProgress.exchange(true, std::memory_order_acq_rel))
    {
        markParametersDirty();
        return;
    }

    if (externalParameterConnection.pushMirroredParameterValue != nullptr)
    {
        const auto pushExternalParameter = [this] (const ExternalParameterTarget target,
                                                   const size_t bandIndex,
                                                   const float value)
        {
            externalParameterConnection.pushMirroredParameterValue({ target, bandIndex, value });
        };

        if (parameterID == mxe::parameters::makeModuleBypassParameterId())
            pushExternalParameter(ExternalParameterTarget::moduleBypass,
                                  0,
                                  loadExternalFloat(rawModuleBypassParameter, 0.0f));
        else if (parameterID == mxe::parameters::makeModuleBypassWithGainParameterId())
            pushExternalParameter(ExternalParameterTarget::moduleBypassWithGain,
                                  0,
                                  loadExternalFloat(rawModuleBypassWithGainParameter, 0.0f));
        else if (parameterID == mxe::parameters::makeFullbandParameterId("inGnVisible"))
            pushExternalParameter(ExternalParameterTarget::fullbandInGainLr,
                                  0,
                                  loadExternalFloat(rawFullbandVisibleParameters[static_cast<size_t>(mxe::parameters::FullbandVisibleSlot::inGn)], 0.0f));
        else if (parameterID == mxe::parameters::makeFullbandParameterId("autoInLeft"))
            pushExternalParameter(ExternalParameterTarget::fullbandInGainL,
                                  0,
                                  loadExternalFloat(rawFullbandParameters[static_cast<size_t>(mxe::parameters::FullbandAutomationSlot::inLeft)], 0.0f));
        else if (parameterID == mxe::parameters::makeFullbandParameterId("autoInRight"))
            pushExternalParameter(ExternalParameterTarget::fullbandInGainR,
                                  0,
                                  loadExternalFloat(rawFullbandParameters[static_cast<size_t>(mxe::parameters::FullbandAutomationSlot::inRight)], 0.0f));
        else if (parameterID == mxe::parameters::makeFullbandParameterId("wideVisible"))
            pushExternalParameter(ExternalParameterTarget::fullbandWide,
                                  0,
                                  loadExternalFloat(rawFullbandVisibleParameters[static_cast<size_t>(mxe::parameters::FullbandVisibleSlot::wide)], 100.0f));
        else if (parameterID == mxe::parameters::makeFullbandParameterId("outGnVisible"))
            pushExternalParameter(ExternalParameterTarget::fullbandOutGain,
                                  0,
                                  loadExternalFloat(rawFullbandVisibleParameters[static_cast<size_t>(mxe::parameters::FullbandVisibleSlot::outGn)], 0.0f));
        else
        {
            for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
            {
                if (parameterID == makeBandParameterId(bandIndex, "inGn"))
                    pushExternalParameter(ExternalParameterTarget::bandInGainLr,
                                          bandIndex,
                                          loadExternalFloat(rawBandParameters[bandIndex][toIndex(ParameterSlot::inGn)], 0.0f));
                else if (parameterID == makeBandParameterId(bandIndex, "inLeft"))
                    pushExternalParameter(ExternalParameterTarget::bandInGainL,
                                          bandIndex,
                                          loadExternalFloat(rawBandParameters[bandIndex][toIndex(ParameterSlot::inLeft)], 0.0f));
                else if (parameterID == makeBandParameterId(bandIndex, "inRight"))
                    pushExternalParameter(ExternalParameterTarget::bandInGainR,
                                          bandIndex,
                                          loadExternalFloat(rawBandParameters[bandIndex][toIndex(ParameterSlot::inRight)], 0.0f));
                else if (parameterID == makeBandParameterId(bandIndex, "wide"))
                    pushExternalParameter(ExternalParameterTarget::bandWide,
                                          bandIndex,
                                          loadExternalFloat(rawBandParameters[bandIndex][toIndex(ParameterSlot::wide)], 100.0f));
                else if (parameterID == makeBandParameterId(bandIndex, "outGn"))
                    pushExternalParameter(ExternalParameterTarget::bandOutGain,
                                          bandIndex,
                                          loadExternalFloat(rawBandParameters[bandIndex][toIndex(ParameterSlot::outGn)], 0.0f));
            }
        }
    }

    const auto setBandSlotValue = [this] (const size_t bandIndex,
                                                                 const ParameterSlot targetSlot,
                                                                 const float targetValue)
    {
        const auto targetIndex = toIndex(targetSlot);
        auto* target = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(
            makeBandParameterId(bandIndex, parameterSpecs[targetIndex].suffix)));

        if (target == nullptr)
            return;

        const auto normalizedValue = target->convertTo0to1(targetValue);

        if (std::abs(target->getValue() - normalizedValue) <= 1.0e-6f)
            return;

        target->setValueNotifyingHost(normalizedValue);
    };

    const auto parseSlotForBand = [&] (const size_t bandIndex, const juce::String& id) -> std::optional<ParameterSlot>
    {
        for (size_t slotIndex = 0; slotIndex < numParameterSlots; ++slotIndex)
        {
            const auto slot = static_cast<ParameterSlot>(slotIndex);

            if (id == makeBandParameterId(bandIndex, parameterSpecs[slotIndex].suffix))
                return slot;
        }

        return std::nullopt;
    };

    const auto branchIndexFromSlot = [] (const ParameterSlot slot) -> int
    {
        switch (slot)
        {
            case ParameterSlot::thLU:
            case ParameterSlot::tensLU:
            case ParameterSlot::relLU:
            case ParameterSlot::outLU:
                return 0;
            case ParameterSlot::thLD:
            case ParameterSlot::tensLD:
            case ParameterSlot::relLD:
            case ParameterSlot::outLD:
                return 1;
            case ParameterSlot::thRU:
            case ParameterSlot::tensRU:
            case ParameterSlot::relRU:
            case ParameterSlot::outRU:
                return 2;
            case ParameterSlot::thRD:
            case ParameterSlot::tensRD:
            case ParameterSlot::relRD:
            case ParameterSlot::outRD:
                return 3;
            default:
                return -1;
        }
    };

    const auto fieldSlotsFor = [] (const ParameterSlot slot) -> std::array<ParameterSlot, 4>
    {
        switch (slot)
        {
            case ParameterSlot::thLU:
            case ParameterSlot::thLD:
            case ParameterSlot::thRU:
            case ParameterSlot::thRD:
                return { ParameterSlot::thLU, ParameterSlot::thLD, ParameterSlot::thRU, ParameterSlot::thRD };
            case ParameterSlot::tensLU:
            case ParameterSlot::tensLD:
            case ParameterSlot::tensRU:
            case ParameterSlot::tensRD:
                return { ParameterSlot::tensLU, ParameterSlot::tensLD, ParameterSlot::tensRU, ParameterSlot::tensRD };
            case ParameterSlot::relLU:
            case ParameterSlot::relLD:
            case ParameterSlot::relRU:
            case ParameterSlot::relRD:
                return { ParameterSlot::relLU, ParameterSlot::relLD, ParameterSlot::relRU, ParameterSlot::relRD };
            case ParameterSlot::outLU:
            case ParameterSlot::outLD:
            case ParameterSlot::outRU:
            case ParameterSlot::outRD:
                return { ParameterSlot::outLU, ParameterSlot::outLD, ParameterSlot::outRU, ParameterSlot::outRD };
            default:
                return { slot, slot, slot, slot };
        }
    };

    const auto outSlotForThresholdSlot = [] (const ParameterSlot slot) -> std::optional<ParameterSlot>
    {
        switch (slot)
        {
            case ParameterSlot::thLU: return ParameterSlot::outLU;
            case ParameterSlot::thLD: return ParameterSlot::outLD;
            case ParameterSlot::thRU: return ParameterSlot::outRU;
            case ParameterSlot::thRD: return ParameterSlot::outRD;
            default: return std::nullopt;
        }
    };

    const auto isThresholdSlot = [] (const ParameterSlot slot)
    {
        return slot == ParameterSlot::thLU
            || slot == ParameterSlot::thLD
            || slot == ParameterSlot::thRU
            || slot == ParameterSlot::thRD;
    };

    const auto thresholdScopeFor = [&branchIndexFromSlot] (const size_t bandIndex,
                                                                const ParameterSlot sourceSlot,
                                                                const bool linkUpDnActive,
                                                                const bool linkLrActive)
    {
        std::array<ParameterSlot, 4> scope { sourceSlot, sourceSlot, sourceSlot, sourceSlot };
        size_t scopeSize = 1;

        if (linkLrActive)
        {
            scope = { ParameterSlot::thLU, ParameterSlot::thLD, ParameterSlot::thRU, ParameterSlot::thRD };
            scopeSize = 4;
            return std::pair { scope, scopeSize };
        }

        if (linkUpDnActive)
        {
            const auto sourceBranch = branchIndexFromSlot(sourceSlot);

            if (sourceBranch < 0)
                return std::pair { scope, scopeSize };

            if (sourceBranch < 2)
                scope = { ParameterSlot::thLU, ParameterSlot::thLD, sourceSlot, sourceSlot };
            else
                scope = { ParameterSlot::thRU, ParameterSlot::thRD, sourceSlot, sourceSlot };

            scopeSize = 2;
            return std::pair { scope, scopeSize };
        }

        juce::ignoreUnused(bandIndex);
        return std::pair { scope, scopeSize };
    };

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto linkLrActive = rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)] != nullptr
            && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)]->load(std::memory_order_relaxed) >= 0.5f;
        auto linkUpDnActive = rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)] != nullptr
            && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)]->load(std::memory_order_relaxed) >= 0.5f;
        const auto linkOppActive = rawBandParameters[bandIndex][toIndex(ParameterSlot::linkOpp)] != nullptr
            && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkOpp)]->load(std::memory_order_relaxed) >= 0.5f;

        const auto slot = parseSlotForBand(bandIndex, parameterID);

        if (! slot.has_value())
            continue;

        const auto sourceSlot = *slot;

        const auto isGlobalMainSlot = [] (const ParameterSlot slotToCheck)
        {
            return slotToCheck == ParameterSlot::moRph
                || slotToCheck == ParameterSlot::peakHoldHz
                || slotToCheck == ParameterSlot::TensionFlooR
                || slotToCheck == ParameterSlot::TensionHysT;
        };

        if (isGlobalMainSlot(sourceSlot))
        {
            const auto* source = rawBandParameters[bandIndex][toIndex(sourceSlot)];

            if (source != nullptr)
            {
                const auto sourceValue = source->load(std::memory_order_relaxed);

                for (size_t targetBand = 0; targetBand < numBands; ++targetBand)
                    setBandSlotValue(targetBand, sourceSlot, sourceValue);
            }

            continue;
        }

        if (sourceSlot == ParameterSlot::linkUpDn && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)] != nullptr)
        {
            if (rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)]->load(std::memory_order_relaxed) >= 0.5f
                && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)] != nullptr)
                setBandSlotValue(bandIndex, ParameterSlot::linkLr, 0.0f);
        }
        else if (sourceSlot == ParameterSlot::linkLr && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)] != nullptr)
        {
            if (rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)]->load(std::memory_order_relaxed) >= 0.5f
                && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)] != nullptr)
                setBandSlotValue(bandIndex, ParameterSlot::linkUpDn, 0.0f);
        }

        linkLrActive = rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)] != nullptr
            && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)]->load(std::memory_order_relaxed) >= 0.5f;
        linkUpDnActive = rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)] != nullptr
            && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)]->load(std::memory_order_relaxed) >= 0.5f;

        const auto syncAllFieldSlots = [&] (const ParameterSlot lu,
                                            const ParameterSlot ld,
                                            const ParameterSlot ru,
                                            const ParameterSlot rd)
        {
            const auto* source = rawBandParameters[bandIndex][toIndex(lu)];

            if (source == nullptr)
                return;

            const auto value = source->load(std::memory_order_relaxed);
            setBandSlotValue(bandIndex, lu, value);
            setBandSlotValue(bandIndex, ld, value);
            setBandSlotValue(bandIndex, ru, value);
            setBandSlotValue(bandIndex, rd, value);
        };

        const auto syncUpDnPairs = [&] (const ParameterSlot lu,
                                        const ParameterSlot ld,
                                        const ParameterSlot ru,
                                        const ParameterSlot rd)
        {
            const auto* leftSource = rawBandParameters[bandIndex][toIndex(lu)];
            const auto* rightSource = rawBandParameters[bandIndex][toIndex(ru)];

            if (leftSource != nullptr)
            {
                const auto leftValue = leftSource->load(std::memory_order_relaxed);
                setBandSlotValue(bandIndex, lu, leftValue);
                setBandSlotValue(bandIndex, ld, leftValue);
            }

            if (rightSource != nullptr)
            {
                const auto rightValue = rightSource->load(std::memory_order_relaxed);
                setBandSlotValue(bandIndex, ru, rightValue);
                setBandSlotValue(bandIndex, rd, rightValue);
            }
        };

        if (sourceSlot == ParameterSlot::linkLr && linkLrActive)
        {
            syncAllFieldSlots(ParameterSlot::thLU, ParameterSlot::thLD, ParameterSlot::thRU, ParameterSlot::thRD);
            syncAllFieldSlots(ParameterSlot::tensLU, ParameterSlot::tensLD, ParameterSlot::tensRU, ParameterSlot::tensRD);
            syncAllFieldSlots(ParameterSlot::relLU, ParameterSlot::relLD, ParameterSlot::relRU, ParameterSlot::relRD);
            syncAllFieldSlots(ParameterSlot::outLU, ParameterSlot::outLD, ParameterSlot::outRU, ParameterSlot::outRD);
            continue;
        }

        if (sourceSlot == ParameterSlot::linkUpDn && linkUpDnActive)
        {
            syncUpDnPairs(ParameterSlot::thLU, ParameterSlot::thLD, ParameterSlot::thRU, ParameterSlot::thRD);
            syncUpDnPairs(ParameterSlot::tensLU, ParameterSlot::tensLD, ParameterSlot::tensRU, ParameterSlot::tensRD);
            syncUpDnPairs(ParameterSlot::relLU, ParameterSlot::relLD, ParameterSlot::relRU, ParameterSlot::relRD);
            syncUpDnPairs(ParameterSlot::outLU, ParameterSlot::outLD, ParameterSlot::outRU, ParameterSlot::outRD);
            continue;
        }

        if (isThresholdSlot(sourceSlot))
        {
            const auto* source = rawBandParameters[bandIndex][toIndex(sourceSlot)];

            if (source == nullptr)
                continue;

            const auto sourceValue = source->load(std::memory_order_relaxed);
            const auto [thresholdSlots, thresholdSlotCount] = thresholdScopeFor(bandIndex, sourceSlot, linkUpDnActive, linkLrActive);

            for (size_t thresholdIndex = 0; thresholdIndex < thresholdSlotCount; ++thresholdIndex)
            {
                const auto thresholdSlot = thresholdSlots[thresholdIndex];
                setBandSlotValue(bandIndex, thresholdSlot, sourceValue);
            }

            if (linkOppActive)
            {
                for (size_t thresholdIndex = 0; thresholdIndex < thresholdSlotCount; ++thresholdIndex)
                {
                    const auto thresholdSlot = thresholdSlots[thresholdIndex];
                    const auto* thresholdParam = rawBandParameters[bandIndex][toIndex(thresholdSlot)];

                    if (thresholdParam == nullptr)
                        continue;

                    const auto compensatedOut = juce::jlimit(-48.0f, 48.0f, -thresholdParam->load(std::memory_order_relaxed));
                    const auto outSlot = outSlotForThresholdSlot(thresholdSlot);

                    if (outSlot.has_value())
                        setBandSlotValue(bandIndex, *outSlot, compensatedOut);
                }
            }

            continue;
        }

        const auto sourceBranch = branchIndexFromSlot(sourceSlot);

        if (sourceBranch < 0)
            continue;

        const auto fieldSlots = fieldSlotsFor(sourceSlot);
        const auto* source = rawBandParameters[bandIndex][toIndex(sourceSlot)];

        if (source == nullptr)
            continue;

        const auto sourceValue = source->load(std::memory_order_relaxed);

        for (const auto targetSlot : fieldSlots)
        {
            const auto targetBranch = branchIndexFromSlot(targetSlot);

            if (targetBranch < 0)
                continue;

            const auto sameSide = (sourceBranch < 2) == (targetBranch < 2);
            const auto connected = sourceBranch == targetBranch
                || linkLrActive
                || (linkUpDnActive && sameSide)
                || (linkUpDnActive && linkLrActive);

            if (connected)
                setBandSlotValue(bandIndex, targetSlot, sourceValue);
        }
    }

    linkedParameterPropagationInProgress.store(false, std::memory_order_release);
    markParametersDirty();
}

juce::AudioProcessorValueTreeState::ParameterLayout MxeAudioProcessor::createParameterLayout()
{
    return mxe::parameters::createParameterLayout();
}
