#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>
#include <utility>

#include "module.spe.SpeShared.h"

class SpeModuleProcessor final
{
public:
    static constexpr auto analyserScopeSize = spe::analyserScopeSize;
    using DisplaySettings = spe::DisplaySettings;
    using AnalysisSettings = spe::AnalysisSettings;

    struct CompressorSettings
    {
        int fftSize = 8192;
        int overlapFactor = 32;
        float thresholdDb = 12.0f;
        float stereoAdaptiveAmount = 0.0f;
        float stereoAdaptiveOffsetDb = 0.0f;
        float leftThresholdDb = 12.0f;
        float rightThresholdDb = 12.0f;
        float leftAdaptiveAmount = 0.0f;
        float rightAdaptiveAmount = 0.0f;
        float leftAdaptiveOffsetDb = 0.0f;
        float rightAdaptiveOffsetDb = 0.0f;
        bool stereoBypass = false;
        bool dualMonoBypass = false;
        float slopeDbPerOct = 4.0f;
        float attackMs = 0.0f;
        float releaseMs = 0.0f;
        float kneeDb = 0.0f;
        float ratio = 100.0f;
        float makeupDb = 0.0f;
    };

    inline static constexpr auto paramFftSizeId = "spe_fft_size";
    inline static constexpr auto paramOverlapId = "spe_overlap";
    inline static constexpr auto paramLeftId = "spe_left";
    inline static constexpr auto paramRightId = "spe_right";
    inline static constexpr auto paramRangeLowId = "spe_range_low";
    inline static constexpr auto paramRangeHighId = "spe_range_high";
    inline static constexpr auto paramSlopeId = "spe_slope";
    inline static constexpr auto paramTimeId = "spe_time";
    inline static constexpr auto paramThresholdId = "spe_threshold";
    inline static constexpr auto paramStereoAdaptiveId = "spe_stereo_adaptive";
    inline static constexpr auto paramStereoAdaptiveOffsetId = "spe_stereo_adaptive_offset";
    inline static constexpr auto paramDualMonoLeftThresholdId = "spe_dual_mono_left_threshold";
    inline static constexpr auto paramDualMonoRightThresholdId = "spe_dual_mono_right_threshold";
    inline static constexpr auto paramDualMonoLeftAdaptiveId = "spe_dual_mono_left_adaptive";
    inline static constexpr auto paramDualMonoRightAdaptiveId = "spe_dual_mono_right_adaptive";
    inline static constexpr auto paramDualMonoLeftAdaptiveOffsetId = "spe_dual_mono_left_adaptive_offset";
    inline static constexpr auto paramDualMonoRightAdaptiveOffsetId = "spe_dual_mono_right_adaptive_offset";
    inline static constexpr auto paramInputGainId = "spe_input_gain";
    inline static constexpr auto paramAttackId = "spe_attack";
    inline static constexpr auto paramReleaseId = "spe_release";
    inline static constexpr auto paramKneeId = "spe_knee";
    inline static constexpr auto paramRatioId = "spe_ratio";
    inline static constexpr auto paramMakeupId = "spe_makeup";
    inline static constexpr auto paramBypassId = "spe_bypass";
    inline static constexpr auto paramMiscBypassId = "spe_misc_bypass";
    inline static constexpr auto paramMiscBypassWithGainId = "spe_misc_bypass_wt_gain";
    inline static constexpr auto paramDualMonoBypassId = "spe_dual_mono_bypass";
    inline static constexpr auto paramDeltaId = "spe_delta";
    inline static constexpr auto paramDspFftSizeId = "spe_dsp_fft_size";
    inline static constexpr auto paramDspSlopeId = "spe_dsp_slope";

    explicit SpeModuleProcessor(juce::AudioProcessor& ownerProcessor);
    ~SpeModuleProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>&);

    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    juce::String getStateXmlString() const;
    void setStateFromXmlString(const juce::String& stateXmlString);

    void copyAnalyserData(std::array<float, analyserScopeSize>& destination, double& currentSampleRate) const;
    void copyGainReductionData(std::array<float, analyserScopeSize>& destination) const;
    DisplaySettings getDisplaySettings() const noexcept;
    AnalysisSettings getAnalysisSettings() const noexcept;
    int getLatencySamples() const noexcept;
    bool refreshLatencyState() noexcept;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    CompressorSettings getCompressorSettings() const noexcept;
    bool isStereoBypassEnabled() const noexcept;
    bool isModuleBypassEnabled() const noexcept;
    bool isModuleBypassWithGainEnabled() const noexcept;
    bool isDeltaEnabled() const noexcept;
    void applyMakeupGain(juce::AudioBuffer<float>& buffer, int channelsToUse) const noexcept;
    void resetDeltaDelay() noexcept;
    void ensureDeltaDryBufferSize(int channels, int samples);
    void populateAlignedDryBuffer(const juce::AudioBuffer<float>& inputBuffer,
                                  juce::AudioBuffer<float>& delayedDryBuffer,
                                  int channelsToUse,
                                  int latencySamples) noexcept;
    int getSelectedAnalyserFftSize() const noexcept;
    int getSelectedDspFftSize() const noexcept;
    int getSelectedOverlapFactor() const noexcept;
    float getSelectedAveragingTimeMs() const noexcept;

    juce::AudioProcessor& ownerProcessor;

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
        void copyReductionScope(std::array<float, analyserScopeSize>& destination) const;
        float getPublishedStereoThresholdDb() const noexcept;

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
        std::array<float, (maxFftSize / 2) + 1> stereoSmoothedReductionDb {};
        std::array<std::array<float, (maxFftSize / 2) + 1>, maxChannels> dualMonoSmoothedReductionDb {};
        std::array<float, (maxFftSize / 2) + 1> combinedReductionDb {};
        std::array<std::array<float, analyserScopeSize>, 2> reductionScopeBuffers {};
        std::atomic<int> activeReductionScopeBuffer { 0 };
        std::atomic<float> publishedStereoThresholdDb { 12.0f };
        double sampleRate = 44100.0;
        float stereoAdaptiveReferenceDb = 0.0f;
        std::array<float, maxChannels> dualMonoAdaptiveReferenceDb {};
        int configuredChannels = 0;
        int currentFftSize = 0;
        int currentHopSize = 0;
        int hopFill = 0;
    };

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* fftSizeParam = nullptr;
    std::atomic<float>* overlapParam = nullptr;
    std::atomic<float>* leftParam = nullptr;
    std::atomic<float>* rightParam = nullptr;
    std::atomic<float>* rangeLowParam = nullptr;
    std::atomic<float>* rangeHighParam = nullptr;
    std::atomic<float>* slopeParam = nullptr;
    std::atomic<float>* timeParam = nullptr;
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* stereoAdaptiveParam = nullptr;
    std::atomic<float>* stereoAdaptiveOffsetParam = nullptr;
    std::atomic<float>* dualMonoLeftThresholdParam = nullptr;
    std::atomic<float>* dualMonoRightThresholdParam = nullptr;
    std::atomic<float>* dualMonoLeftAdaptiveParam = nullptr;
    std::atomic<float>* dualMonoRightAdaptiveParam = nullptr;
    std::atomic<float>* dualMonoLeftAdaptiveOffsetParam = nullptr;
    std::atomic<float>* dualMonoRightAdaptiveOffsetParam = nullptr;
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* kneeParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* makeupParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* miscBypassParam = nullptr;
    std::atomic<float>* miscBypassWithGainParam = nullptr;
    std::atomic<float>* dualMonoBypassParam = nullptr;
    std::atomic<float>* deltaParam = nullptr;
    std::atomic<float>* dspFftSizeParam = nullptr;
    std::atomic<float>* dspSlopeParam = nullptr;
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
