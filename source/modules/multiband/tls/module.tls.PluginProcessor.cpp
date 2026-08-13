#include "module.tls.PluginProcessor.h"

#include "module.tls.ParameterIds.h"

TlsAudioProcessor::TlsAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
    registerParameterListeners();
}

TlsAudioProcessor::~TlsAudioProcessor()
{
    unregisterParameterListeners();
}

void TlsAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    multibandProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    syncParameters(true);
    multibandProcessor.reset();
}

void TlsAudioProcessor::reset()
{
    multibandProcessor.reset();
}

bool TlsAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return vx::multiband::detail::supportsMatchingMonoOrStereoLayout(layouts);
}

void TlsAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    vx::multiband::detail::clearOutputOnlyChannels(*this, buffer);

    syncParameters();

    multibandProcessor.process(buffer);
}

juce::AudioProcessorEditor* TlsAudioProcessor::createEditor()
{
    return nullptr;
}

bool TlsAudioProcessor::hasEditor() const
{
    return false;
}

const juce::String TlsAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TlsAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TlsAudioProcessor::producesMidi() const
{
    return false;
}

bool TlsAudioProcessor::isMidiEffect() const
{
    return false;
}

double TlsAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TlsAudioProcessor::getNumPrograms()
{
    return 1;
}

int TlsAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TlsAudioProcessor::setCurrentProgram(const int)
{
}

const juce::String TlsAudioProcessor::getProgramName(const int)
{
    return {};
}

void TlsAudioProcessor::changeProgramName(const int, const juce::String&)
{
}

void TlsAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = valueTreeState.copyState().createXml())
        copyXmlToBinary(*stateXml, destData);
}

void TlsAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
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

juce::AudioProcessorValueTreeState& TlsAudioProcessor::getValueTreeState() noexcept
{
    return valueTreeState;
}

const juce::AudioProcessorValueTreeState& TlsAudioProcessor::getValueTreeState() const noexcept
{
    return valueTreeState;
}

juce::UndoManager& TlsAudioProcessor::getUndoManager() noexcept
{
    return undoManager;
}

const juce::UndoManager& TlsAudioProcessor::getUndoManager() const noexcept
{
    return undoManager;
}

int TlsAudioProcessor::getModuleLatencySamples() const noexcept
{
    return moduleLatencySamples.load(std::memory_order_acquire);
}

void TlsAudioProcessor::markParametersDirty() noexcept
{
    parametersDirty.store(true, std::memory_order_relaxed);
}

void TlsAudioProcessor::registerParameterListeners()
{
    using tls::parameters::crossoverSpecs;
    using tls::parameters::makeActiveSplitCountParameterId;
    using tls::parameters::makeBandParameterId;
    using tls::parameters::makeFullbandParameterId;
    using tls::parameters::makeSoloParameterId;
    using tls::parameters::parameterSpecs;

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

void TlsAudioProcessor::unregisterParameterListeners()
{
    using tls::parameters::crossoverSpecs;
    using tls::parameters::makeActiveSplitCountParameterId;
    using tls::parameters::makeBandParameterId;
    using tls::parameters::makeFullbandParameterId;
    using tls::parameters::makeSoloParameterId;
    using tls::parameters::parameterSpecs;

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

void TlsAudioProcessor::parameterChanged(const juce::String&, float)
{
    markParametersDirty();
}

juce::AudioProcessorValueTreeState::ParameterLayout TlsAudioProcessor::createParameterLayout()
{
    return tls::parameters::createParameterLayout();
}
