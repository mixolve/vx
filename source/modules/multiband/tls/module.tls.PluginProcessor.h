#pragma once

#include <JuceHeader.h>

#include "module.tls.DspCore.h"
#include "module.tls.PluginParameters.h"

#include <array>
#include <atomic>

class TlsAudioProcessor final : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener
{
public:
    TlsAudioProcessor();
    ~TlsAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
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
    static constexpr size_t numBands = tls::dsp::MultibandProcessor::numBands;
    static constexpr size_t numParameterSlots = tls::parameters::numParameterSlots;
    static constexpr size_t numCrossoverSlots = tls::parameters::numCrossoverSlots;
    static constexpr size_t numWidebandListenSlots = tls::parameters::widebandListenSpecs.size();

    enum class WidebandListenMode
    {
        neutral,
        leftCenter,
        rightCenter,
        midCenter,
        sideCenter,
        leftLeft,
        rightRight,
        sideStereo
    };

    void cacheParameterPointers();
    void registerParameterListeners();
    void unregisterParameterListeners();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    tls::dsp::DspCore::Parameters readBandParameters(size_t bandIndex) const;
    tls::dsp::MultibandProcessor::CrossoverFrequencies readCrossoverFrequencies() const;
    size_t readActiveSplitCount() const;
    tls::dsp::MultibandProcessor::SoloMask readSoloMask() const;
    WidebandListenMode readWidebandListenMode() const noexcept;
    static void applyWidebandListen(juce::AudioBuffer<float>& buffer, WidebandListenMode mode) noexcept;
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState valueTreeState;
    tls::dsp::MultibandProcessor multibandProcessor;
    std::atomic<float>* rawActiveSplitCountParameter = nullptr;
    std::array<std::atomic<float>*, numBands> rawSoloParameters {};
    std::array<std::atomic<float>*, numCrossoverSlots> rawCrossoverParameters {};
    std::array<std::atomic<float>*, numWidebandListenSlots> rawWidebandListenParameters {};
    std::array<std::array<std::atomic<float>*, numParameterSlots>, numBands> rawBandParameters {};
    tls::dsp::MultibandProcessor::CrossoverFrequencies currentCrossoverFrequencies {};
    size_t currentActiveSplitCount = tls::dsp::MultibandProcessor::numSplits;
    std::array<tls::dsp::DspCore::Parameters, numBands> currentBandParameters {};
    tls::dsp::MultibandProcessor::SoloMask currentSoloMask {};
    WidebandListenMode currentWidebandListenMode = WidebandListenMode::neutral;
    std::atomic<int> moduleLatencySamples { 0 };
    std::atomic<bool> parametersDirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TlsAudioProcessor)
};
