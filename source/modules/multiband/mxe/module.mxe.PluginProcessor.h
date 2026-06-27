#pragma once

#include <JuceHeader.h>

#include "module.mxe.MultibandProcessor.h"
#include "module.mxe.PluginParameters.h"

#include <array>
#include <atomic>

class MxeAudioProcessor final : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit MxeAudioProcessor(juce::AudioProcessor& ownerProcessor);
    ~MxeAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;
    juce::UndoManager& getUndoManager() noexcept;
    const juce::UndoManager& getUndoManager() const noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    int getModuleLatencySamples() const noexcept;
    bool syncParameters(bool force = false);
    void markParametersDirty() noexcept;

private:
    static constexpr size_t numBands = mxe::dsp::MultibandProcessor::numBands;
    static constexpr size_t numParameterSlots = mxe::parameters::numParameterSlots;
    static constexpr size_t numCrossoverSlots = mxe::parameters::numCrossoverSlots;

    void cacheParameterPointers();
    void registerParameterListeners();
    void unregisterParameterListeners();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    mxe::dsp::DspCore::Parameters readBandParameters(size_t bandIndex) const;
    mxe::dsp::MultibandProcessor::CrossoverFrequencies readCrossoverFrequencies() const;
    size_t readActiveSplitCount() const;
    mxe::dsp::MultibandProcessor::SoloMask readSoloMask() const;
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState valueTreeState;
    mxe::dsp::MultibandProcessor multibandProcessor;
    std::atomic<float>* rawActiveSplitCountParameter = nullptr;
    std::array<std::atomic<float>*, numBands> rawSoloParameters {};
    std::array<std::atomic<float>*, numCrossoverSlots> rawCrossoverParameters {};
    std::array<std::array<std::atomic<float>*, numParameterSlots>, numBands> rawBandParameters {};
    mxe::dsp::MultibandProcessor::CrossoverFrequencies currentCrossoverFrequencies {};
    size_t currentActiveSplitCount = mxe::dsp::MultibandProcessor::numSplits;
    std::array<mxe::dsp::DspCore::Parameters, numBands> currentBandParameters {};
    mxe::dsp::MultibandProcessor::SoloMask currentSoloMask {};
    int moduleLatencySamples = 0;
    std::atomic<bool> parametersDirty { true };
    std::atomic<bool> linkedParameterPropagationInProgress { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MxeAudioProcessor)
};
