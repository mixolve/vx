#pragma once

#include <JuceHeader.h>
#include "../../shared/shared.TransientSplit.h"

#include <atomic>
#include <vector>

class TseModuleProcessor final : public TransientSplitProvider
{
public:
    inline static constexpr auto paramTransOnId = "tse_trans_on";
    inline static constexpr auto paramTransGainId = "tse_trans_gain";
    inline static constexpr auto paramSusOnId = "tse_sus_on";
    inline static constexpr auto paramSusGainId = "tse_sus_gain";
    inline static constexpr auto paramBypassId = "tse_bypass";
    inline static constexpr auto paramBypassWithGainId = "tse_bypass_wt_gain";
    inline static constexpr auto paramTimeHoldId = "tse_time_hold";
    inline static constexpr auto paramTimeHoldModeId = "tse_time_hold_mode";
    inline static constexpr auto paramTimeHoldSyncId = "tse_time_hold_sync";
    inline static constexpr auto paramTimeReleaseId = "tse_time_release";
    inline static constexpr auto paramTimeReleaseCurveId = "tse_time_release_curve";
    inline static constexpr auto paramTimeReleaseModeId = "tse_time_release_mode";
    inline static constexpr auto paramTimeReleaseSyncId = "tse_time_release_sync";
    inline static constexpr auto paramSensLevelId = "tse_sens_level";
    inline static constexpr auto paramSensKneeId = "tse_sens_knee";
    inline static constexpr auto paramSensRetriggerId = "tse_sens_retrigger";
    inline static constexpr auto paramMiscInGainLrId = "tse_misc_in_gain_lr";
    inline static constexpr auto paramMiscInGainLId = "tse_misc_in_gain_l";
    inline static constexpr auto paramMiscInGainRId = "tse_misc_in_gain_r";
    inline static constexpr auto paramMiscWideId = "tse_misc_wide";
    inline static constexpr auto paramMiscLookaheadId = "tse_misc_lookahead";
    inline static constexpr auto paramMiscOutGainId = "tse_misc_out_gain";

    explicit TseModuleProcessor(juce::AudioProcessor& ownerProcessor);
    ~TseModuleProcessor() override;

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
    bool canProvideSplit() const noexcept override;
    TransientSplitSettings getSplitSettings() const noexcept override;

private:
    struct Settings
    {
        bool bypassed = false;
        bool bypassedWithGain = false;
        bool transEnabled = true;
        float transGainDb = 0.0f;
        bool sustainEnabled = true;
        float sustainGainDb = 0.0f;
        float holdMs = 0.0f;
        float releaseMs = 10.0f;
        float releaseCurve = 0.0f;
        float thresholdDb = -42.0f;
        float kneeDb = 0.0f;
        float retriggerMs = 100.0f;
        float lookaheadMs = 1.0f;
        float outGainDb = 0.0f;
        float fastReleaseCoefficient = 0.0f;
        float bodyAttackCoefficient = 0.0f;
        float bodyReleaseCoefficient = 0.0f;
        float normalizedReleaseCurve = 0.0f;
        int holdSamples = 0;
        int retriggerSamples = 0;
        int releaseSamples = 1;
        float transientGain = 1.0f;
        float sustainGain = 1.0f;
    };

    struct DetectorState
    {
        float fastEnvelope = 0.0f;
        float bodyEnvelope = 0.0f;
        float transientEnvelope = 0.0f;
        float heldTransientAmount = 0.0f;
        float releaseStartAmount = 0.0f;
        int holdSamplesRemaining = 0;
        int releaseSamplesRemaining = 0;
        int releaseSamplesTotal = 0;
        int samplesSinceTrigger = 0;
        bool wasAboveThreshold = false;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    Settings getSettings(double hostBpm) const noexcept;
    double getHostBpm() const noexcept;
    void updateDelayConfiguration(int channelsToUse, int lookaheadSamples);
    void resetDelayBuffers() noexcept;
    void resetDetector() noexcept;
    float processDetectorSample(float level, const Settings& settings) noexcept;
    static float makeReleaseCoefficient(float timeMs, double sampleRate) noexcept;

    juce::AudioProcessor& ownerProcessor;
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* transOnParam = nullptr;
    std::atomic<float>* transGainParam = nullptr;
    std::atomic<float>* sustainOnParam = nullptr;
    std::atomic<float>* sustainGainParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* bypassWithGainParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* holdModeParam = nullptr;
    std::atomic<float>* holdSyncParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* releaseCurveParam = nullptr;
    std::atomic<float>* releaseModeParam = nullptr;
    std::atomic<float>* releaseSyncParam = nullptr;
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* kneeParam = nullptr;
    std::atomic<float>* retriggerParam = nullptr;
    std::atomic<float>* inGainLrParam = nullptr;
    std::atomic<float>* inGainLParam = nullptr;
    std::atomic<float>* inGainRParam = nullptr;
    std::atomic<float>* wideParam = nullptr;
    std::atomic<float>* lookaheadParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;

    std::vector<std::vector<float>> delayBuffers;
    DetectorState detector;
    double currentSampleRate = 44100.0;
    int delayBufferLength = 1;
    int delayWriteIndex = 0;
    int preparedChannels = 0;
    int activeLatencySamples = 0;
    int maximumLookaheadSamples = 0;
    int preparedBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TseModuleProcessor)
};
