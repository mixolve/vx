#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

class FftModuleProcessor final : private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr std::size_t analyserScopeSize = 512;

    struct CompressorSettings
    {
        int fftSize = 4096;
        int overlapFactor = 32;
        bool phaseMode = false;
        float floorDb = -60.0f;
        float leftThresholdDb = 0.0f;
        float rightThresholdDb = 0.0f;
        float phaseThreshold = 0.0f;
        float phaseAdaptiveAmount = 0.0f;
        float phaseSlopePerOctave = 0.0f;
        float phaseImpact = 0.0f;
        float leftAdaptiveAmount = 0.0f;
        float rightAdaptiveAmount = 0.0f;
        float adaptiveOffset = 0.0f;
        float adaptiveAttackMs = 30.0f;
        float adaptiveHoldMs = 0.0f;
        float adaptiveReleaseMs = 300.0f;
        float slopeDbPerOct = 4.5f;
        float attackMs = 0.0f;
        float releaseMs = 0.0f;
        float kneeDb = 0.0f;
        float ratio = 100.0f;
        float reductionDisplayTimeMs = 50.0f;
        float makeupDb = 0.0f;
        bool dynamicBypassed = false;
    };

    inline static constexpr auto paramTimeId = "fft_time";
    inline static constexpr auto paramSpectralReductionRangeId = "fft_spectral_reduction_range";
    inline static constexpr auto paramPhaseReductionRangeId = "fft_phase_reduction_range";
    inline static constexpr auto paramDualMonoLeftThresholdId = "fft_dual_mono_left_threshold";
    inline static constexpr auto paramDualMonoRightThresholdId = "fft_dual_mono_right_threshold";
    inline static constexpr auto paramPhaseThresholdId = "fft_phase_threshold";
    inline static constexpr auto paramPhaseAdaptiveId = "fft_phase_adaptive";
    inline static constexpr auto paramPhaseSlopeId = "fft_phase_slope";
    inline static constexpr auto paramPhaseImpactId = "fft_phase_impact";
    inline static constexpr auto paramDualMonoLeftAdaptiveId = "fft_dual_mono_left_adaptive";
    inline static constexpr auto paramDualMonoRightAdaptiveId = "fft_dual_mono_right_adaptive";
    inline static constexpr auto paramSpectralAdaptiveOffsetId = "fft_spectral_adaptive_offset";
    inline static constexpr auto paramSpectralAdaptiveAttackId = "fft_spectral_adaptive_attack";
    inline static constexpr auto paramSpectralAdaptiveHoldId = "fft_spectral_adaptive_hold";
    inline static constexpr auto paramSpectralAdaptiveReleaseId = "fft_spectral_adaptive_release";
    inline static constexpr auto paramPhaseAdaptiveOffsetId = "fft_phase_adaptive_offset";
    inline static constexpr auto paramPhaseAdaptiveAttackId = "fft_phase_adaptive_attack";
    inline static constexpr auto paramPhaseAdaptiveHoldId = "fft_phase_adaptive_hold";
    inline static constexpr auto paramPhaseAdaptiveReleaseId = "fft_phase_adaptive_release";
    inline static constexpr auto paramDualMonoLinkId = "fft_dual_mono_link_lr";
    inline static constexpr auto paramDynamicBypassId = "fft_dynamic_bypass";
    inline static constexpr auto paramDynamicModeId = "fft_dynamic_mode";
    inline static constexpr auto paramFloorId = "fft_floor";
    inline static constexpr auto paramAttackId = "fft_attack";
    inline static constexpr auto paramReleaseId = "fft_release";
    inline static constexpr auto paramKneeId = "fft_knee";
    inline static constexpr auto paramRatioId = "fft_ratio";
    inline static constexpr auto paramDeltaId = "fft_delta";
    inline static constexpr auto paramDspFftSizeId = "fft_dsp_fft_size";
    inline static constexpr auto paramDspOverlapId = "fft_dsp_overlap";
    inline static constexpr auto paramDspSlopeId = "fft_dsp_slope";

    explicit FftModuleProcessor(juce::AudioProcessor& ownerProcessor);
    ~FftModuleProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>&);

    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    juce::String getStateXmlString() const;
    void setStateFromXmlString(const juce::String& stateXmlString);

    void copyGainReductionData(std::array<float, analyserScopeSize>& leftDestination,
                               std::array<float, analyserScopeSize>& rightDestination) const;
    bool isPhaseCorrMode() const noexcept;
    float getReductionDisplayFloor() const noexcept;
    int getLatencySamples() const noexcept;
    bool refreshLatencyState() noexcept;
    float getAnalyserParameterValue(const juce::String& parameterId) const noexcept;
    void setAnalyserParameterValue(const juce::String& parameterId, float value);

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;

private:
    class InternalParameterHost final : public juce::AudioProcessor
    {
    public:
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        const juce::String getName() const override { return "fft_parameter_host"; }
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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float phaseThresholdToCorrelation(float threshold) noexcept;
    static float calculateReduction(float detectorValue, float threshold, float ratio, float knee) noexcept;
    static float calculateTimeCoefficient(float timeMs, float frameDurationSeconds) noexcept;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    CompressorSettings getCompressorSettings() const noexcept;
    bool isDeltaEnabled() const noexcept;
    void resetDeltaDelay() noexcept;
    void ensureDeltaDryBufferSize(int channels, int samples);
    void populateAlignedDryBuffer(const juce::AudioBuffer<float>& inputBuffer,
                                  juce::AudioBuffer<float>& delayedDryBuffer,
                                  int channelsToUse,
                                  int latencySamples) noexcept;
    int getSelectedDspFftSize() const noexcept;
    int getSelectedDspOverlapFactor() const noexcept;
    float getSelectedAveragingTimeMs() const noexcept;
    juce::ValueTree createAnalyserStateSnapshot() const;

    void resetAnalyserState();
    void applyAnalyserState(juce::ValueTree state);

    juce::AudioProcessor& ownerProcessor;
    InternalParameterHost internalParameterHost;

    class DynamicProcessor
    {
    public:
        static constexpr auto maxFftOrder = 14;
        static constexpr auto maxFftSize = 1 << maxFftOrder;
        static constexpr auto maxChannels = 2;
        static constexpr auto maxQueueSize = maxFftSize * 2;

        DynamicProcessor();

        void prepare(double newSampleRate, int numChannels);
        void processBuffer(juce::AudioBuffer<float>& buffer, int numInputChannels, const CompressorSettings& settings);
        void copyReductionScope(std::array<float, analyserScopeSize>& leftDestination,
                                std::array<float, analyserScopeSize>& rightDestination) const;

    private:
        struct ChannelState
        {
            std::array<float, maxFftSize> analysisFifo {};
            std::array<float, maxFftSize> outputAccum {};
            std::array<float, maxFftSize> normalizationAccum {};
            std::array<float, maxQueueSize> readyOutput {};
            std::array<juce::dsp::Complex<float>, maxFftSize> frequencyData {};
            int readyOutputRead = 0;
            int readyOutputWrite = 0;
            int readyOutputCount = 0;
            int analysisFilled = 0;
        };

        void enqueueOutputSample(ChannelState& state, float sample) noexcept;
        float dequeueOutputSample(ChannelState& state) noexcept;
        void processFrame(int channelsToUse,
                          const CompressorSettings& settings,
                          int fftIndex,
                          int fftSize,
                          int hopSize) noexcept;
        void pushOutputChunk(ChannelState& state, int fftSize, int hopSize) noexcept;
        void reconfigure(int channelsToUse, int fftSize, int hopSize) noexcept;
        int getFftIndexForSize(int fftSize) const noexcept;
        std::array<std::unique_ptr<juce::dsp::FFT>, 5> ffts;
        std::array<std::array<float, maxFftSize>, 5> windowTables {};
        std::array<ChannelState, maxChannels> channelStates {};
        std::array<std::array<float, maxFftSize>, maxChannels> hopBuffers {};
        std::array<std::array<float, (maxFftSize / 2) + 1>, maxChannels> dualMonoSmoothedReductionDb {};
        std::array<std::array<float, (maxFftSize / 2) + 1>, maxChannels> phaseSmoothedReductionRadians {};
        std::array<std::array<float, (maxFftSize / 2) + 1>, maxChannels> phaseCorrelationReductions {};
        std::array<std::array<std::array<float, analyserScopeSize>, maxChannels>, 2> reductionScopeBuffers {};
        std::array<std::array<float, analyserScopeSize>, maxChannels> smoothedReductionScopes {};
        std::atomic<int> activeReductionScopeBuffer { 0 };
        double sampleRate = 44100.0;
        std::array<float, maxChannels> dualMonoAdaptiveReferenceDb {};
        std::array<float, maxChannels> phaseAdaptiveReference { -1.0f, -1.0f };
        std::array<float, maxChannels> dualMonoAdaptiveHoldRemainingMs {};
        std::array<float, maxChannels> phaseAdaptiveHoldRemainingMs {};
        int configuredChannels = 0;
        int currentFftSize = 0;
        int currentHopSize = 0;
        int hopFill = 0;
    };

    juce::AudioProcessorValueTreeState parameters;
    juce::ValueTree analyserState { "fft_analyser_state" };
    std::atomic<float> analyserTimeValue { 50.0f };
    std::atomic<float> spectralReductionRangeValue { -36.0f };
    std::atomic<float> phaseReductionRangeValue { 50.0f };
    std::atomic<float>* dualMonoLeftThresholdParam = nullptr;
    std::atomic<float>* dualMonoRightThresholdParam = nullptr;
    std::atomic<float>* phaseThresholdParam = nullptr;
    std::atomic<float>* phaseAdaptiveParam = nullptr;
    std::atomic<float>* phaseSlopeParam = nullptr;
    std::atomic<float>* phaseImpactParam = nullptr;
    std::atomic<float>* dualMonoLeftAdaptiveParam = nullptr;
    std::atomic<float>* dualMonoRightAdaptiveParam = nullptr;
    std::atomic<float>* spectralAdaptiveOffsetParam = nullptr;
    std::atomic<float>* spectralAdaptiveAttackParam = nullptr;
    std::atomic<float>* spectralAdaptiveHoldParam = nullptr;
    std::atomic<float>* spectralAdaptiveReleaseParam = nullptr;
    std::atomic<float>* phaseAdaptiveOffsetParam = nullptr;
    std::atomic<float>* phaseAdaptiveAttackParam = nullptr;
    std::atomic<float>* phaseAdaptiveHoldParam = nullptr;
    std::atomic<float>* phaseAdaptiveReleaseParam = nullptr;
    std::atomic<float>* dualMonoLinkParam = nullptr;
    std::atomic<float>* dynamicBypassParam = nullptr;
    std::atomic<float>* dynamicModeParam = nullptr;
    std::atomic<float>* floorParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* kneeParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* deltaParam = nullptr;
    std::atomic<float>* dspFftSizeParam = nullptr;
    std::atomic<float>* dspOverlapParam = nullptr;
    std::atomic<float>* dspSlopeParam = nullptr;
    std::atomic<bool> linkedDualMonoPropagationInProgress { false };
    DynamicProcessor dynamicProcessor;
    static constexpr int deltaDelayBufferSize = DynamicProcessor::maxFftSize + 1;
    std::array<std::array<float, deltaDelayBufferSize>, DynamicProcessor::maxChannels> deltaDelayBuffers {};
    juce::AudioBuffer<float> deltaDryBuffer;
    int preparedBlockSize = 0;
    int deltaDelayWriteIndex = 0;
    int activeLatencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FftModuleProcessor)
};
