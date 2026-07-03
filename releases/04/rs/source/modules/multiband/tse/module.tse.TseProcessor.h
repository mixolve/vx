#pragma once

#include <JuceHeader.h>

#include "../module.multiband.Processor.h"
#include "module.tse.DspCore.h"

#include <array>
#include <atomic>

class TseModuleProcessor final
{
public:
    inline static constexpr auto paramTransOnId = "trans_on";
    inline static constexpr auto paramTransGainId = "trans_gain";
    inline static constexpr auto paramSusOnId = "sus_on";
    inline static constexpr auto paramSusGainId = "sus_gain";
    inline static constexpr auto paramTimeHoldId = "hold";
    inline static constexpr auto paramTimeHoldModeId = "hold_mode";
    inline static constexpr auto paramTimeHoldSyncId = "hold_sync";
    inline static constexpr auto paramTimeReleaseId = "release";
    inline static constexpr auto paramTimeReleaseCurveId = "rel_curve";
    inline static constexpr auto paramTimeReleaseModeId = "release_mode";
    inline static constexpr auto paramTimeReleaseSyncId = "release_sync";
    inline static constexpr auto paramSensLevelId = "sens_lvl";
    inline static constexpr auto paramSensKneeId = "sens_knee";
    inline static constexpr auto paramSensRetriggerId = "sens_retr";
    inline static constexpr auto paramLookaheadId = "lookahead";

    static constexpr size_t numBands = vx::multiband::Crossover::numBands;
    static constexpr size_t numCrossoverSlots = vx::multiband::Crossover::numSplits;

    explicit TseModuleProcessor(juce::AudioProcessor& ownerProcessor);
    ~TseModuleProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>& buffer);

    juce::String getStateXmlString() const;
    void setStateFromXmlString(const juce::String& stateXmlString);
    int getLatencySamples() const noexcept;
    bool refreshLatencyState() noexcept;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;
    static juce::StringArray getHostSyncChoices();
    static int getDefaultHostSyncChoiceIndex() noexcept;
    static juce::String makeBandParameterId(size_t bandIndex, const char* suffix);
    static juce::String makeFullbandParameterId(const char* suffix);
    static juce::String makeSoloParameterId(size_t bandIndex);
    static juce::String makeActiveSplitCountParameterId();

private:
    using MultibandProcessor = vx::multiband::Processor<tse::dsp::DspCore>;

    class InternalParameterHost final : public juce::AudioProcessor
    {
    public:
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        const juce::String getName() const override { return "tse_parameter_host"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };

    struct RawBandParameters
    {
        std::atomic<float>* transOn = nullptr;
        std::atomic<float>* transGain = nullptr;
        std::atomic<float>* sustainOn = nullptr;
        std::atomic<float>* sustainGain = nullptr;
        std::atomic<float>* hold = nullptr;
        std::atomic<float>* holdMode = nullptr;
        std::atomic<float>* holdSync = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* releaseCurve = nullptr;
        std::atomic<float>* releaseMode = nullptr;
        std::atomic<float>* releaseSync = nullptr;
        std::atomic<float>* threshold = nullptr;
        std::atomic<float>* knee = nullptr;
        std::atomic<float>* retrigger = nullptr;
        std::atomic<float>* lookahead = nullptr;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();
    tse::dsp::DspCore::Parameters readBandParameters(size_t bandIndex) const noexcept;
    MultibandProcessor::CrossoverFrequencies readCrossoverFrequencies() const noexcept;
    size_t readActiveSplitCount() const noexcept;
    MultibandProcessor::SoloMask readSoloMask() const noexcept;
    void syncParameters();
    double getHostBpm() const noexcept;

    juce::AudioProcessor& ownerProcessor;
    InternalParameterHost internalParameterHost;
    juce::AudioProcessorValueTreeState parameters;
    MultibandProcessor multibandProcessor;
    std::atomic<float>* rawActiveSplitCountParameter = nullptr;
    std::array<std::atomic<float>*, numBands> rawSoloParameters {};
    std::array<std::atomic<float>*, numCrossoverSlots> rawCrossoverParameters {};
    std::array<RawBandParameters, numBands> rawBandParameters {};
    MultibandProcessor::BandParameters currentBandParameters {};
    MultibandProcessor::SoloMask currentSoloMask {};
    MultibandProcessor::CrossoverFrequencies currentCrossoverFrequencies { 134.0, 523.0, 2093.0, 5000.0, 10000.0 };
    size_t currentActiveSplitCount = numCrossoverSlots;
    double currentSampleRate = 44100.0;
    int moduleLatencySamples = 0;
    int preparedBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TseModuleProcessor)
};
