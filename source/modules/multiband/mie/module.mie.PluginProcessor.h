#pragma once

#include <JuceHeader.h>

#include "module.mie.MultibandProcessor.h"
#include "module.mie.PluginParameters.h"

#include <array>
#include <atomic>
#include <functional>
#include <vector>

class MieAudioProcessor final : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit MieAudioProcessor(juce::AudioProcessor& ownerProcessor);
    ~MieAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
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
    static constexpr size_t numBands = mie::dsp::MultibandProcessor::numBands;
    static constexpr size_t numParameterSlots = mie::parameters::numParameterSlots;
    static constexpr size_t numCrossoverSlots = mie::parameters::numCrossoverSlots;

    void cacheParameterPointers();
    void registerParameterListeners();
    void unregisterParameterListeners();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    mie::dsp::DspCore::Parameters readBandParameters(size_t bandIndex) const;
    mie::dsp::MultibandProcessor::CrossoverFrequencies readCrossoverFrequencies() const;
    size_t readActiveSplitCount() const;
    mie::dsp::MultibandProcessor::SoloMask readSoloMask() const;
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState valueTreeState;
    mie::dsp::MultibandProcessor multibandProcessor;
    std::atomic<float>* rawActiveSplitCountParameter = nullptr;
    std::array<std::atomic<float>*, numBands> rawSoloParameters {};
    std::array<std::atomic<float>*, numCrossoverSlots> rawCrossoverParameters {};
    std::array<std::array<std::atomic<float>*, numParameterSlots>, numBands> rawBandParameters {};
    mie::dsp::MultibandProcessor::CrossoverFrequencies currentCrossoverFrequencies {};
    size_t currentActiveSplitCount = mie::dsp::MultibandProcessor::numSplits;
    std::array<mie::dsp::DspCore::Parameters, numBands> currentBandParameters {};
    mie::dsp::MultibandProcessor::SoloMask currentSoloMask {};
    int moduleLatencySamples = 0;
    std::atomic<bool> parametersDirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MieAudioProcessor)
};
