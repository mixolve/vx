#include "module.mxe.PluginProcessor.h"

#include "module.mxe.ParameterIds.h"

#include <cmath>

MxeAudioProcessor::MxeAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
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

    valueTreeState.addParameterListener(makeActiveSplitCountParameterId(), this);
    valueTreeState.addParameterListener(makeModuleBypassParameterId(), this);
    valueTreeState.addParameterListener(makeModuleBypassWithGainParameterId(), this);

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        valueTreeState.addParameterListener(makeSoloParameterId(bandIndex), this);

        for (const auto& spec : parameterSpecs)
            valueTreeState.addParameterListener(makeBandParameterId(bandIndex, spec.suffix), this);
    }

    for (const auto& spec : fullbandAutomationSpecs)
        valueTreeState.addParameterListener(makeFullbandParameterId(spec.suffix), this);

    for (const auto& spec : fullbandVisibleSpecs)
        valueTreeState.addParameterListener(makeFullbandParameterId(spec.suffix), this);

    for (const auto& spec : crossoverSpecs)
        valueTreeState.addParameterListener(makeFullbandParameterId(spec.suffix), this);
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

    valueTreeState.removeParameterListener(makeActiveSplitCountParameterId(), this);
    valueTreeState.removeParameterListener(makeModuleBypassParameterId(), this);
    valueTreeState.removeParameterListener(makeModuleBypassWithGainParameterId(), this);

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        valueTreeState.removeParameterListener(makeSoloParameterId(bandIndex), this);

        for (const auto& spec : parameterSpecs)
            valueTreeState.removeParameterListener(makeBandParameterId(bandIndex, spec.suffix), this);
    }

    for (const auto& spec : fullbandAutomationSpecs)
        valueTreeState.removeParameterListener(makeFullbandParameterId(spec.suffix), this);

    for (const auto& spec : fullbandVisibleSpecs)
        valueTreeState.removeParameterListener(makeFullbandParameterId(spec.suffix), this);

    for (const auto& spec : crossoverSpecs)
        valueTreeState.removeParameterListener(makeFullbandParameterId(spec.suffix), this);
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

    const auto mirrorBandParameter = [this] (const size_t bandIndex,
                                             const ParameterSlot sourceSlot,
                                             const ParameterSlot targetSlot)
    {
        const auto sourceIndex = toIndex(sourceSlot);
        const auto targetIndex = toIndex(targetSlot);
        const auto* source = rawBandParameters[bandIndex][sourceIndex];

        if (source == nullptr)
            return;

        auto* target = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(
            makeBandParameterId(bandIndex, parameterSpecs[targetIndex].suffix)));

        if (target == nullptr)
            return;

        const auto targetValue = source->load(std::memory_order_relaxed);
        const auto normalizedValue = target->convertTo0to1(targetValue);

        if (std::abs(target->getValue() - normalizedValue) <= 1.0e-6f)
            return;

        target->setValueNotifyingHost(normalizedValue);
    };

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        const auto linkLrActive = rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)] != nullptr
            && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkLr)]->load(std::memory_order_relaxed) >= 0.5f;
        const auto linkUpDnActive = rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)] != nullptr
            && rawBandParameters[bandIndex][toIndex(ParameterSlot::linkUpDn)]->load(std::memory_order_relaxed) >= 0.5f;

        if (linkLrActive)
        {
            if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::LLThResh)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::LLThResh, ParameterSlot::RRThResh);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::LLTension)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::LLTension, ParameterSlot::RRTension);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::LLRelease)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::LLRelease, ParameterSlot::RRRelease);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::LLmk)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::LLmk, ParameterSlot::RRmk);
        }

        if (linkUpDnActive)
        {
            if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::thLU)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::thLU, ParameterSlot::thRU);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::thLD)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::thLD, ParameterSlot::thRD);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::mkLU)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::mkLU, ParameterSlot::mkRU);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::mkLD)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::mkLD, ParameterSlot::mkRD);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::thRU)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::thRU, ParameterSlot::thLU);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::thRD)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::thRD, ParameterSlot::thLD);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::mkRU)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::mkRU, ParameterSlot::mkLU);
            else if (parameterID == makeBandParameterId(bandIndex, parameterSpecs[toIndex(ParameterSlot::mkRD)].suffix))
                mirrorBandParameter(bandIndex, ParameterSlot::mkRD, ParameterSlot::mkLD);
        }
    }

    linkedParameterPropagationInProgress.store(false, std::memory_order_release);
    markParametersDirty();
}

juce::AudioProcessorValueTreeState::ParameterLayout MxeAudioProcessor::createParameterLayout()
{
    return mxe::parameters::createParameterLayout();
}
