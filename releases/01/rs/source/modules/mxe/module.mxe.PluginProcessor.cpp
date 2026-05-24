#include "module.mxe.PluginProcessor.h"

#include "module.mxe.ParameterIds.h"

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

void MxeAudioProcessor::parameterChanged(const juce::String&, float)
{
    markParametersDirty();
}

juce::AudioProcessorValueTreeState::ParameterLayout MxeAudioProcessor::createParameterLayout()
{
    return mxe::parameters::createParameterLayout();
}
