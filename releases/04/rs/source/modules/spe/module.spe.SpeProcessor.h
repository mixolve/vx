#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

class SpeModuleProcessor final : private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr std::size_t analyserScopeSize = 512;

    struct DisplaySettings
    {
        float leftFrequencyHz = 21.0f;
        float rightFrequencyHz = 20000.0f;
        float rangeLowDb = -60.0f;
        float rangeHighDb = 10.0f;
        float leftThresholdDb = 0.0f;
        float rightThresholdDb = 0.0f;
        float slopeDbPerOct = 4.5f;
    };

    struct AnalysisSettings
    {
        int fftSize = 4096;
        int overlapFactor = 32;
        float averagingTimeMs = 50.0f;
    };

    struct CompressorSettings
    {
        struct SpectralFilter
        {
            bool bypassed = false;
            int type = 1;
            int place = 0;
            int slope = 1;
            float frequency = 632.0f;
            float bandwidth = 1.0f;
            float impactPercent = 0.0f;
        };

        int fftSize = 4096;
        int overlapFactor = 32;
        float leftThresholdDb = 0.0f;
        float rightThresholdDb = 0.0f;
        float leftAdaptiveAmount = 0.0f;
        float rightAdaptiveAmount = 0.0f;
        float leftAdaptiveOffsetDb = 0.0f;
        float rightAdaptiveOffsetDb = 0.0f;
        float slopeDbPerOct = 4.5f;
        float attackMs = 0.0f;
        float releaseMs = 0.0f;
        float kneeDb = 0.0f;
        float ratio = 100.0f;
        float makeupDb = 0.0f;
        int phaseFilterCount = 0;
        std::array<SpectralFilter, 16> phaseFilters {};
        int amplitudeFilterCount = 0;
        std::array<SpectralFilter, 16> amplitudeFilters {};
    };

    inline static constexpr auto paramFftSizeId = "spe_fft_size";
    inline static constexpr auto paramOverlapId = "spe_overlap";
    inline static constexpr auto paramLeftId = "spe_left";
    inline static constexpr auto paramRightId = "spe_right";
    inline static constexpr auto paramRangeLowId = "spe_range_low";
    inline static constexpr auto paramRangeHighId = "spe_range_high";
    inline static constexpr auto paramSlopeId = "spe_slope";
    inline static constexpr auto paramTimeId = "spe_time";
    inline static constexpr auto paramDualMonoLeftThresholdId = "spe_dual_mono_left_threshold";
    inline static constexpr auto paramDualMonoRightThresholdId = "spe_dual_mono_right_threshold";
    inline static constexpr auto paramDualMonoLeftAdaptiveId = "spe_dual_mono_left_adaptive";
    inline static constexpr auto paramDualMonoRightAdaptiveId = "spe_dual_mono_right_adaptive";
    inline static constexpr auto paramDualMonoLeftAdaptiveOffsetId = "spe_dual_mono_left_adaptive_offset";
    inline static constexpr auto paramDualMonoRightAdaptiveOffsetId = "spe_dual_mono_right_adaptive_offset";
    inline static constexpr auto paramDualMonoLinkId = "spe_dual_mono_link_lr";
    inline static constexpr auto paramAttackId = "spe_attack";
    inline static constexpr auto paramReleaseId = "spe_release";
    inline static constexpr auto paramKneeId = "spe_knee";
    inline static constexpr auto paramRatioId = "spe_ratio";
    inline static constexpr auto paramDeltaId = "spe_delta";
    inline static constexpr auto paramDspFftSizeId = "spe_dsp_fft_size";
    inline static constexpr auto paramDspHopDivisorId = "spe_dsp_hop_divisor";
    inline static constexpr auto paramDspSlopeId = "spe_dsp_slope";
    inline static constexpr auto paramPhaseFilterCountId = "spe_phase_filter_count";
    inline static constexpr auto paramAmplitudeFilterCountId = "spe_amplitude_filter_count";
    static constexpr int maxSpeFilterCount = 16;
    static juce::String getPhaseFilterTypeParamId(int filterIndex);
    static juce::String getPhaseFilterPlaceParamId(int filterIndex);
    static juce::String getPhaseFilterSlopeParamId(int filterIndex);
    static juce::String getPhaseFilterFrequencyParamId(int filterIndex);
    static juce::String getPhaseFilterBandwidthParamId(int filterIndex);
    static juce::String getPhaseFilterImpactParamId(int filterIndex);
    static juce::String getPhaseFilterBypassParamId(int filterIndex);
    static juce::String getAmplitudeFilterTypeParamId(int filterIndex);
    static juce::String getAmplitudeFilterPlaceParamId(int filterIndex);
    static juce::String getAmplitudeFilterSlopeParamId(int filterIndex);
    static juce::String getAmplitudeFilterFrequencyParamId(int filterIndex);
    static juce::String getAmplitudeFilterBandwidthParamId(int filterIndex);
    static juce::String getAmplitudeFilterImpactParamId(int filterIndex);
    static juce::String getAmplitudeFilterBypassParamId(int filterIndex);

    explicit SpeModuleProcessor(juce::AudioProcessor& ownerProcessor);
    ~SpeModuleProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>&);

    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    juce::String getStateXmlString() const;
    void setStateFromXmlString(const juce::String& stateXmlString);

    void copyAnalyserData(std::array<float, analyserScopeSize>& destination, double& currentSampleRate) const;
    void copyGainReductionData(std::array<float, analyserScopeSize>& leftDestination,
                               std::array<float, analyserScopeSize>& rightDestination) const;
    DisplaySettings getDisplaySettings() const noexcept;
    AnalysisSettings getAnalysisSettings() const noexcept;
    int getLatencySamples() const noexcept;
    bool refreshLatencyState() noexcept;
    juce::ValueTree getAnalyserState() const;
    float getAnalyserParameterValue(const juce::String& parameterId) const noexcept;
    void setAnalyserParameterValue(const juce::String& parameterId, float value);

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;
    int getActivePhaseFilterCount() const noexcept;
    bool addPhaseFilter() noexcept;
    bool removePhaseFilter(int filterIndex) noexcept;
    int getActiveAmplitudeFilterCount() const noexcept;
    bool addAmplitudeFilter() noexcept;
    bool removeAmplitudeFilter(int filterIndex) noexcept;

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
        const juce::String getName() const override { return "spe_parameter_host"; }
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
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    CompressorSettings getCompressorSettings() const noexcept;
    bool isDeltaEnabled() const noexcept;
    void resetDeltaDelay() noexcept;
    void ensureDeltaDryBufferSize(int channels, int samples);
    void populateAlignedDryBuffer(const juce::AudioBuffer<float>& inputBuffer,
                                  juce::AudioBuffer<float>& delayedDryBuffer,
                                  int channelsToUse,
                                  int latencySamples) noexcept;
    int getSelectedAnalyserFftSize() const noexcept;
    int getSelectedDspFftSize() const noexcept;
    int getSelectedDspHopDivisor() const noexcept;
    int getSelectedOverlapFactor() const noexcept;
    float getSelectedAveragingTimeMs() const noexcept;
    juce::ValueTree createAnalyserStateSnapshot() const;

    void resetAnalyserState();
    void applyAnalyserState(juce::ValueTree state);

    juce::AudioProcessor& ownerProcessor;
    InternalParameterHost internalParameterHost;

    class PostAnalyser
    {
    public:
        static constexpr auto maxFftOrder = 14;
        static constexpr auto maxFftSize = 1 << maxFftOrder;

        PostAnalyser();

        void prepare(double newSampleRate);
        void pushBuffer(const juce::AudioBuffer<float>& buffer,
                        int numInputChannels,
                        int fftSize,
                        int overlapFactor,
                        float averagingTimeMs);
        void copyScope(std::array<float, analyserScopeSize>& destination, double& currentSampleRate) const;

    private:
        void pushSample(float sample, int fftSize, int overlapFactor, float averagingTimeMs) noexcept;
        void generateSpectrum(int fftSize, int overlapFactor, float averagingTimeMs) noexcept;
        int getFftIndexForSize(int fftSize) const noexcept;

        std::array<std::unique_ptr<juce::dsp::FFT>, 5> ffts;
        std::array<std::unique_ptr<juce::dsp::WindowingFunction<float>>, 5> windows;
        std::array<float, maxFftSize> sampleHistory {};
        std::array<float, maxFftSize * 2> fftData {};
        std::array<std::array<float, analyserScopeSize>, 2> scopeBuffers {};
        std::array<float, analyserScopeSize> smoothedMagnitudes {};
        int historyWriteIndex = 0;
        int availableSamples = 0;
        int samplesSinceLastTransform = 0;
        std::atomic<int> activeScopeBuffer { 0 };
        std::atomic<double> sampleRate { 44100.0 };
    };

    class SpectralCompressor
    {
    public:
        static constexpr auto maxFftOrder = 14;
        static constexpr auto maxFftSize = 1 << maxFftOrder;
        static constexpr auto maxChannels = 2;
        static constexpr auto maxQueueSize = maxFftSize * 2;

        SpectralCompressor();

        void prepare(double newSampleRate, int numChannels);
        void processBuffer(juce::AudioBuffer<float>& buffer, int numInputChannels, const CompressorSettings& settings);
        void copyReductionScope(std::array<float, analyserScopeSize>& leftDestination,
                    std::array<float, analyserScopeSize>& rightDestination) const;
        float getPublishedDualMonoThresholdDb(int channel) const noexcept;

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
        static float calculateReductionDb(float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept;
        static float calculateTimeCoefficient(float timeMs, float frameDurationSeconds) noexcept;

        std::array<std::unique_ptr<juce::dsp::FFT>, 5> ffts;
        std::array<std::array<float, maxFftSize>, 5> windowTables {};
        std::array<ChannelState, maxChannels> channelStates {};
        std::array<std::array<float, maxFftSize>, maxChannels> hopBuffers {};
        std::array<std::array<float, (maxFftSize / 2) + 1>, maxChannels> dualMonoSmoothedReductionDb {};
        std::array<std::array<std::array<float, analyserScopeSize>, maxChannels>, 2> reductionScopeBuffers {};
        std::atomic<int> activeReductionScopeBuffer { 0 };
        std::array<std::atomic<float>, maxChannels> publishedDualMonoThresholdDb { 0.0f, 0.0f };
        double sampleRate = 44100.0;
        std::array<float, maxChannels> dualMonoAdaptiveReferenceDb {};
        int configuredChannels = 0;
        int currentFftSize = 0;
        int currentHopSize = 0;
        int hopFill = 0;
    };

    juce::AudioProcessorValueTreeState parameters;
    juce::ValueTree analyserState { "spe_analyser_state" };
    std::atomic<float> analyserFftSizeValue { 2.0f };
    std::atomic<float> analyserOverlapValue { 4.0f };
    std::atomic<float> analyserLeftValue { 21.0f };
    std::atomic<float> analyserRightValue { 20000.0f };
    std::atomic<float> analyserRangeLowValue { -60.0f };
    std::atomic<float> analyserRangeHighValue { 10.0f };
    std::atomic<float> analyserSlopeValue { 4.5f };
    std::atomic<float> analyserTimeValue { 50.0f };
    std::atomic<float>* dualMonoLeftThresholdParam = nullptr;
    std::atomic<float>* dualMonoRightThresholdParam = nullptr;
    std::atomic<float>* dualMonoLeftAdaptiveParam = nullptr;
    std::atomic<float>* dualMonoRightAdaptiveParam = nullptr;
    std::atomic<float>* dualMonoLeftAdaptiveOffsetParam = nullptr;
    std::atomic<float>* dualMonoRightAdaptiveOffsetParam = nullptr;
    std::atomic<float>* dualMonoLinkParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* kneeParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* deltaParam = nullptr;
    std::atomic<float>* dspFftSizeParam = nullptr;
    std::atomic<float>* dspHopDivisorParam = nullptr;
    std::atomic<float>* dspSlopeParam = nullptr;
    std::atomic<float>* phaseFilterCountParam = nullptr;
    std::atomic<float>* amplitudeFilterCountParam = nullptr;
    std::array<std::atomic<float>*, maxSpeFilterCount> phaseTypeParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> phasePlaceParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> phaseSlopeParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> phaseFrequencyParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> phaseBandwidthParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> phaseImpactParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> phaseBypassParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> amplitudeTypeParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> amplitudePlaceParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> amplitudeSlopeParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> amplitudeFrequencyParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> amplitudeBandwidthParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> amplitudeImpactParams {};
    std::array<std::atomic<float>*, maxSpeFilterCount> amplitudeBypassParams {};
    std::atomic<bool> linkedDualMonoPropagationInProgress { false };
    SpectralCompressor spectralCompressor;
    static constexpr int deltaDelayBufferSize = SpectralCompressor::maxFftSize + 1;
    std::array<std::array<float, deltaDelayBufferSize>, SpectralCompressor::maxChannels> deltaDelayBuffers {};
    juce::AudioBuffer<float> deltaDryBuffer;
    int preparedBlockSize = 0;
    int deltaDelayWriteIndex = 0;
    int activeLatencySamples = 0;
    PostAnalyser outputAnalyser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpeModuleProcessor)
};
