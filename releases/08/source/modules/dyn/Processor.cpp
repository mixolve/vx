#include "Processor.h"

DynAudioProcessor::DynAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
    setParameterListenersEnabled(true);
}

DynAudioProcessor::~DynAudioProcessor()
{
    setParameterListenersEnabled(false);
}

void DynAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    processorBank.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    syncParameters(true);
    processorBank.reset();
}

void DynAudioProcessor::releaseResources()
{
    processorBank.releaseResources();
}

void DynAudioProcessor::reset()
{
    processorBank.reset();
}

bool DynAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return ava::modules::dsp::supportsMatchingMonoOrStereoLayout(layouts);
}

void DynAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    ava::modules::dsp::clearOutputOnlyChannels(*this, buffer);

    syncParameters();

    processorBank.processRange(0, buffer);
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

dyn::dsp::ProcessorBank::RangeLatencies DynAudioProcessor::getRangeLatencies() const noexcept
{
    return processorBank.getRangeLatencies();
}

size_t DynAudioProcessor::ensureRangeCount(const size_t rangeCount)
{
    const auto createdRangeCount = processorBank.ensureRangeCount(rangeCount);
    processorBank.setRangeParameters(currentRangeParameters);
    return createdRangeCount;
}

size_t DynAudioProcessor::getCreatedRangeCount() const noexcept
{
    return processorBank.getCreatedRangeCount();
}

void DynAudioProcessor::processRange(const size_t rangeIndex, juce::AudioBuffer<float>& buffer)
{
    processorBank.processRange(rangeIndex, buffer);
}

void DynAudioProcessor::markParametersDirty() noexcept
{
    parametersDirty.store(true, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout DynAudioProcessor::createParameterLayout()
{
    return dyn::parameters::createParameterLayout();
}
