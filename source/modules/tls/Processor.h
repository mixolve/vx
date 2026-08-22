#pragma once

#include <JuceHeader.h>

#include "DspCore.h"
#include "Parameters.h"

#include <array>
#include <atomic>

class TlsAudioProcessor final : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener
{
public:
    TlsAudioProcessor();
    ~TlsAudioProcessor() override;

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
    tls::dsp::ProcessorBank::RangeLatencies getRangeLatencies() const noexcept;
    size_t ensureRangeCount(size_t rangeCount);
    size_t getCreatedRangeCount() const noexcept;
    void processRange(size_t rangeIndex, juce::AudioBuffer<float>& buffer);
    bool syncParameters(bool force = false);
    void markParametersDirty() noexcept;

private:
    static constexpr size_t numRanges = tls::dsp::ProcessorBank::numRanges;
    static constexpr size_t numParameterSlots = tls::parameters::numParameterSlots;
    void cacheParameterPointers();
    void setParameterListenersEnabled(bool enabled);
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    tls::dsp::DspCore::Parameters readCrossoverRangeParameters(size_t rangeIndex) const;
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState valueTreeState;
    tls::dsp::ProcessorBank processorBank;
    std::array<std::array<std::atomic<float>*, numParameterSlots>, numRanges> rawRangeParameters {};
    std::array<tls::dsp::DspCore::Parameters, numRanges> currentRangeParameters {};
    std::atomic<int> moduleLatencySamples { 0 };
    std::atomic<bool> parametersDirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TlsAudioProcessor)
};
