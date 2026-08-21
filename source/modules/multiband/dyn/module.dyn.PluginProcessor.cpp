#include "module.dyn.PluginProcessor.h"

#include "module.dyn.ParameterIds.h"

#include <array>
#include <cmath>
#include <optional>

DynAudioProcessor::DynAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
    registerParameterListeners();
}

DynAudioProcessor::~DynAudioProcessor()
{
    unregisterParameterListeners();
}

void DynAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    multibandProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    syncParameters(true);
    multibandProcessor.reset();
}

void DynAudioProcessor::reset()
{
    multibandProcessor.reset();
}

bool DynAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return ava::multiband::detail::supportsMatchingMonoOrStereoLayout(layouts);
}

void DynAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    ava::multiband::detail::clearOutputOnlyChannels(*this, buffer);

    syncParameters();

    multibandProcessor.process(buffer);
}

juce::AudioProcessorEditor* DynAudioProcessor::createEditor()
{
    return nullptr;
}

bool DynAudioProcessor::hasEditor() const
{
    return false;
}

const juce::String DynAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DynAudioProcessor::acceptsMidi() const
{
    return false;
}

bool DynAudioProcessor::producesMidi() const
{
    return false;
}

bool DynAudioProcessor::isMidiEffect() const
{
    return false;
}

double DynAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DynAudioProcessor::getNumPrograms()
{
    return 1;
}

int DynAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DynAudioProcessor::setCurrentProgram(const int)
{
}

const juce::String DynAudioProcessor::getProgramName(const int)
{
    return {};
}

void DynAudioProcessor::changeProgramName(const int, const juce::String&)
{
}

void DynAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = valueTreeState.copyState().createXml())
        copyXmlToBinary(*stateXml, destData);
}

void DynAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(valueTreeState.state.getType()))
        {
            valueTreeState.replaceState(juce::ValueTree::fromXml(*xmlState));

            using dyn::parameters::makeBandParameterId;
            using dyn::parameters::parameterSpecs;
            using dyn::parameters::ParameterSlot;
            using dyn::parameters::toIndex;

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

                syncGlobalFromBand0(ParameterSlot::morph);
                syncGlobalFromBand0(ParameterSlot::peakHoldMs);
                syncGlobalFromBand0(ParameterSlot::lookahead);
                syncGlobalFromBand0(ParameterSlot::tensionFloor);
                syncGlobalFromBand0(ParameterSlot::tensionHysteresis);
                syncGlobalFromBand0(ParameterSlot::releaseForm);

                if (readBandSlotValue(0, ParameterSlot::releaseForm) < 0.5f)
                {
                    for (size_t targetBand = 0; targetBand < numBands; ++targetBand)
                        setBandSlotValue(targetBand, ParameterSlot::releaseCurve, 0.0f);
                }
                else
                {
                    syncGlobalFromBand0(ParameterSlot::releaseCurve);
                }
            }

            markParametersDirty();
            syncParameters(true);
        }
    }
}

juce::AudioProcessorValueTreeState& DynAudioProcessor::getValueTreeState() noexcept
{
    return valueTreeState;
}

const juce::AudioProcessorValueTreeState& DynAudioProcessor::getValueTreeState() const noexcept
{
    return valueTreeState;
}

juce::UndoManager& DynAudioProcessor::getUndoManager() noexcept
{
    return undoManager;
}

const juce::UndoManager& DynAudioProcessor::getUndoManager() const noexcept
{
    return undoManager;
}

int DynAudioProcessor::getModuleLatencySamples() const noexcept
{
    return moduleLatencySamples;
}

void DynAudioProcessor::markParametersDirty() noexcept
{
    parametersDirty.store(true, std::memory_order_relaxed);
}

void DynAudioProcessor::registerParameterListeners()
{
    using dyn::parameters::crossoverSpecs;
    using dyn::parameters::makeActiveSplitCountParameterId;
    using dyn::parameters::makeBandParameterId;
    using dyn::parameters::makeFullbandParameterId;
    using dyn::parameters::makeSoloParameterId;
    using dyn::parameters::parameterSpecs;

    const auto addListenerIfPresent = [this] (const juce::String& parameterId)
    {
        if (valueTreeState.getParameter(parameterId) != nullptr)
            valueTreeState.addParameterListener(parameterId, this);
    };

    addListenerIfPresent(makeActiveSplitCountParameterId());

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        valueTreeState.addParameterListener(makeSoloParameterId(bandIndex), this);

        for (const auto& spec : parameterSpecs)
            valueTreeState.addParameterListener(makeBandParameterId(bandIndex, spec.suffix), this);
    }

    for (const auto& spec : crossoverSpecs)
        addListenerIfPresent(makeFullbandParameterId(spec.suffix));
}

void DynAudioProcessor::unregisterParameterListeners()
{
    using dyn::parameters::crossoverSpecs;
    using dyn::parameters::makeActiveSplitCountParameterId;
    using dyn::parameters::makeBandParameterId;
    using dyn::parameters::makeFullbandParameterId;
    using dyn::parameters::makeSoloParameterId;
    using dyn::parameters::parameterSpecs;

    const auto removeListenerIfPresent = [this] (const juce::String& parameterId)
    {
        if (valueTreeState.getParameter(parameterId) != nullptr)
            valueTreeState.removeParameterListener(parameterId, this);
    };

    removeListenerIfPresent(makeActiveSplitCountParameterId());

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        valueTreeState.removeParameterListener(makeSoloParameterId(bandIndex), this);

        for (const auto& spec : parameterSpecs)
            valueTreeState.removeParameterListener(makeBandParameterId(bandIndex, spec.suffix), this);
    }

    for (const auto& spec : crossoverSpecs)
        removeListenerIfPresent(makeFullbandParameterId(spec.suffix));
}

void DynAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    using dyn::parameters::makeBandParameterId;
    using dyn::parameters::parameterSpecs;
    using dyn::parameters::ParameterSlot;
    using dyn::parameters::toIndex;

    if (linkedParameterPropagationInProgress.exchange(true, std::memory_order_acq_rel))
    {
        markParametersDirty();
        return;
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
        if (slot == ParameterSlot::thLU || slot == ParameterSlot::tensLU || slot == ParameterSlot::relLU || slot == ParameterSlot::outLU)
            return 0;

        if (slot == ParameterSlot::thLD || slot == ParameterSlot::tensLD || slot == ParameterSlot::relLD || slot == ParameterSlot::outLD)
            return 1;

        if (slot == ParameterSlot::thRU || slot == ParameterSlot::tensRU || slot == ParameterSlot::relRU || slot == ParameterSlot::outRU)
            return 2;

        if (slot == ParameterSlot::thRD || slot == ParameterSlot::tensRD || slot == ParameterSlot::relRD || slot == ParameterSlot::outRD)
            return 3;

        return -1;
    };

    const auto fieldSlotsFor = [] (const ParameterSlot slot) -> std::array<ParameterSlot, 4>
    {
        if (slot == ParameterSlot::thLU || slot == ParameterSlot::thLD || slot == ParameterSlot::thRU || slot == ParameterSlot::thRD)
            return { ParameterSlot::thLU, ParameterSlot::thLD, ParameterSlot::thRU, ParameterSlot::thRD };

        if (slot == ParameterSlot::tensLU || slot == ParameterSlot::tensLD || slot == ParameterSlot::tensRU || slot == ParameterSlot::tensRD)
            return { ParameterSlot::tensLU, ParameterSlot::tensLD, ParameterSlot::tensRU, ParameterSlot::tensRD };

        if (slot == ParameterSlot::relLU || slot == ParameterSlot::relLD || slot == ParameterSlot::relRU || slot == ParameterSlot::relRD)
            return { ParameterSlot::relLU, ParameterSlot::relLD, ParameterSlot::relRU, ParameterSlot::relRD };

        if (slot == ParameterSlot::outLU || slot == ParameterSlot::outLD || slot == ParameterSlot::outRU || slot == ParameterSlot::outRD)
            return { ParameterSlot::outLU, ParameterSlot::outLD, ParameterSlot::outRU, ParameterSlot::outRD };

        return { slot, slot, slot, slot };
    };

    const auto outSlotForThresholdSlot = [] (const ParameterSlot slot) -> std::optional<ParameterSlot>
    {
        if (slot == ParameterSlot::thLU)
            return ParameterSlot::outLU;

        if (slot == ParameterSlot::thLD)
            return ParameterSlot::outLD;

        if (slot == ParameterSlot::thRU)
            return ParameterSlot::outRU;

        if (slot == ParameterSlot::thRD)
            return ParameterSlot::outRD;

        return std::nullopt;
    };

    const auto isThresholdSlot = [] (const ParameterSlot slot)
    {
        return slot == ParameterSlot::thLU
            || slot == ParameterSlot::thLD
            || slot == ParameterSlot::thRU
            || slot == ParameterSlot::thRD;
    };

    const auto thresholdScopeFor = [&branchIndexFromSlot] (const ParameterSlot sourceSlot,
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
            return slotToCheck == ParameterSlot::morph
                || slotToCheck == ParameterSlot::peakHoldMs
                || slotToCheck == ParameterSlot::lookahead
                || slotToCheck == ParameterSlot::tensionFloor
                || slotToCheck == ParameterSlot::tensionHysteresis
                || slotToCheck == ParameterSlot::releaseForm
                || slotToCheck == ParameterSlot::releaseCurve;
        };

        if (isGlobalMainSlot(sourceSlot))
        {
            const auto* source = rawBandParameters[bandIndex][toIndex(sourceSlot)];

            if (source != nullptr)
            {
                const auto sourceValue = source->load(std::memory_order_relaxed);

                for (size_t targetBand = 0; targetBand < numBands; ++targetBand)
                    setBandSlotValue(targetBand, sourceSlot, sourceValue);

                if (sourceSlot == ParameterSlot::releaseForm && sourceValue < 0.5f)
                {
                    for (size_t targetBand = 0; targetBand < numBands; ++targetBand)
                        setBandSlotValue(targetBand, ParameterSlot::releaseCurve, 0.0f);
                }
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
            const auto [thresholdSlots, thresholdSlotCount] = thresholdScopeFor(sourceSlot, linkUpDnActive, linkLrActive);

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

juce::AudioProcessorValueTreeState::ParameterLayout DynAudioProcessor::createParameterLayout()
{
    return dyn::parameters::createParameterLayout();
}
