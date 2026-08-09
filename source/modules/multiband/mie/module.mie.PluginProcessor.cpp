#include "module.mie.PluginProcessor.h"

#include "module.mie.ParameterIds.h"

MieAudioProcessor::MieAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
    registerParameterListeners();
}

MieAudioProcessor::~MieAudioProcessor()
{
    unregisterParameterListeners();
}

void MieAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    multibandProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    syncParameters(true);
    multibandProcessor.reset();
}

void MieAudioProcessor::reset()
{
    multibandProcessor.reset();
}

bool MieAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return vx::multiband::detail::supportsMatchingMonoOrStereoLayout(layouts);
}

void MieAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    vx::multiband::detail::clearOutputOnlyChannels(*this, buffer);

    syncParameters();

    multibandProcessor.process(buffer);
}

juce::AudioProcessorEditor* MieAudioProcessor::createEditor()
{
    return nullptr;
}

bool MieAudioProcessor::hasEditor() const
{
    return false;
}

const juce::String MieAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MieAudioProcessor::acceptsMidi() const
{
    return false;
}

bool MieAudioProcessor::producesMidi() const
{
    return false;
}

bool MieAudioProcessor::isMidiEffect() const
{
    return false;
}

double MieAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MieAudioProcessor::getNumPrograms()
{
    return 1;
}

int MieAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MieAudioProcessor::setCurrentProgram(const int)
{
}

const juce::String MieAudioProcessor::getProgramName(const int)
{
    return {};
}

void MieAudioProcessor::changeProgramName(const int, const juce::String&)
{
}

void MieAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = valueTreeState.copyState().createXml())
        copyXmlToBinary(*stateXml, destData);
}

void MieAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
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

juce::AudioProcessorValueTreeState& MieAudioProcessor::getValueTreeState() noexcept
{
    return valueTreeState;
}

const juce::AudioProcessorValueTreeState& MieAudioProcessor::getValueTreeState() const noexcept
{
    return valueTreeState;
}

juce::UndoManager& MieAudioProcessor::getUndoManager() noexcept
{
    return undoManager;
}

const juce::UndoManager& MieAudioProcessor::getUndoManager() const noexcept
{
    return undoManager;
}

int MieAudioProcessor::getModuleLatencySamples() const noexcept
{
    return moduleLatencySamples;
}

void MieAudioProcessor::markParametersDirty() noexcept
{
    parametersDirty.store(true, std::memory_order_relaxed);
}

void MieAudioProcessor::registerParameterListeners()
{
    using mie::parameters::crossoverSpecs;
    using mie::parameters::makeActiveSplitCountParameterId;
    using mie::parameters::makeBandParameterId;
    using mie::parameters::makeFullbandParameterId;
    using mie::parameters::makeSoloParameterId;
    using mie::parameters::parameterSpecs;

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

void MieAudioProcessor::unregisterParameterListeners()
{
    using mie::parameters::crossoverSpecs;
    using mie::parameters::makeActiveSplitCountParameterId;
    using mie::parameters::makeBandParameterId;
    using mie::parameters::makeFullbandParameterId;
    using mie::parameters::makeSoloParameterId;
    using mie::parameters::parameterSpecs;

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

void MieAudioProcessor::parameterChanged(const juce::String&, float)
{
    markParametersDirty();
}

juce::AudioProcessorValueTreeState::ParameterLayout MieAudioProcessor::createParameterLayout()
{
    return mie::parameters::createParameterLayout();
}
