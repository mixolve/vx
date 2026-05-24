#pragma once

#include <JuceHeader.h>
#include "../../shared/shared.TransientSplit.h"

#include <array>
#include <atomic>
#include <complex>
#include <memory>
#include <vector>

class EqeModuleProcessor final : private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class FilterType
    {
        lowCut,
        lowShelf,
        bell,
        tilt,
        highShelf,
        highCut,
    };

    enum class TtssMode
    {
        ts = 0,
        tt,
        ss,
    };

    inline static constexpr auto activeBellCountStateKey = "active_bell_count";
    inline static constexpr auto paramBypassId = "eqe_bypass";
    inline static constexpr auto paramBypassWithGainId = "eqe_bypass_wt_gn";
    inline static constexpr float fixedSlopeDbPerOct = 12.0f;
    inline static constexpr auto filterPresetLastSelectedStateKey = "filter_preset_last_selected";
    inline static constexpr auto filterPresetDefaultSelectedStateKey = "filter_preset_default_selected";
    static constexpr int maxBellFilterCount = 64;
    static constexpr int visualizerScopeSize = 1024;

    static juce::String getFilterTypeParamId(int filterIndex);
    static juce::String getFilterTtssParamId(int filterIndex);
    static juce::String getFilterLrmsParamId(int filterIndex);
    static juce::String getBellFrequencyParamId(int bellIndex);
    static juce::String getBellBandwidthParamId(int bellIndex);
    static juce::String getBellSlopeParamId(int bellIndex);
    static juce::String getBellGainParamId(int bellIndex);
    static juce::String getBellBypassParamId(int bellIndex);
    static juce::StringArray getBellSlopeChoices() noexcept;
    static float getBellSlopeValueForChoiceIndex(int choiceIndex) noexcept;
    static int getBellSlopeChoiceIndexForValue(float slope) noexcept;
    static juce::String getFilterHeaderText(FilterType type, int filterIndex);
    juce::String getBellHeaderText(int bellIndex, int displayIndex) const noexcept;
    static FilterType filterTypeFromChoiceIndex(int choiceIndex) noexcept;
    static int choiceIndexFromFilterType(FilterType type) noexcept;
    static void appendEqeParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& parameterLayout);

    explicit EqeModuleProcessor(juce::AudioProcessor& ownerProcessor);
    ~EqeModuleProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>& buffer);
    bool isModuleBypassed() const noexcept;

    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;

    int getActiveBellCount() const noexcept;
    bool addBellFilter() noexcept;
    bool removeBellFilter(int bellIndex) noexcept;
    bool clearBellFilters() noexcept;
    bool moveBellFilter(int sourceIndex, int destinationIndex) noexcept;
    juce::String getDefaultFilterPresetName() const;
    juce::String getLastFilterPresetName() const;
    juce::StringArray getFilterPresetNames() const;
    bool saveFilterPreset(const juce::String& presetName);
    bool renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName);
    bool setDefaultFilterPreset(const juce::String& presetName);
    bool loadFilterPreset(const juce::String& presetName) noexcept;
    bool deleteFilterPreset(const juce::String& presetName);
    void copyVisualiserResponse(std::array<float, visualizerScopeSize>& stereoDb,
                                std::array<float, visualizerScopeSize>& leftDb,
                                std::array<float, visualizerScopeSize>& rightDb,
                                std::array<float, visualizerScopeSize>& midDb,
                                std::array<float, visualizerScopeSize>& sideDb,
                                double& sampleRateOut) noexcept;
    void markEqeFiltersDirty() noexcept;
    void setActiveBellCount(int newCount) noexcept;
    void setTransientSplitProvider(TransientSplitProvider* provider) noexcept;

private:
    static constexpr size_t maxSupportedChannels = 2;
    static constexpr size_t maxBellOrder = 128;
    static constexpr size_t maxShelfOrder = 128;
    static constexpr size_t maxBellFourthOrderSections = maxBellOrder;

    struct SecondOrderSection
    {
        void reset() noexcept;
        void setIdentity() noexcept;
        void process(juce::AudioBuffer<float>& buffer, int numChannels) noexcept;

        std::array<double, 3> b { 1.0, 0.0, 0.0 };
        std::array<double, 2> a { 0.0, 0.0 };
        std::array<std::array<double, 2>, maxSupportedChannels> state {};
    };

    struct FourthOrderSection
    {
        void reset() noexcept;
        void setIdentity() noexcept;
        void process(juce::AudioBuffer<float>& buffer, int numChannels) noexcept;

        std::array<double, 5> b { 1.0, 0.0, 0.0, 0.0, 0.0 };
        std::array<double, 4> a { 0.0, 0.0, 0.0, 0.0 };
        std::array<std::array<double, 4>, maxSupportedChannels> state {};
    };

    struct BellOrderFilter
    {
        void reset() noexcept;
        void setIdentity() noexcept;
        void process(juce::AudioBuffer<float>& buffer, int numChannels) noexcept;

        int sectionCount = 0;
        std::array<FourthOrderSection, maxBellFourthOrderSections> sections;
    };

    struct BiquadCascade
    {
        void reset() noexcept;
        void setIdentity() noexcept;
        void process(juce::AudioBuffer<float>& buffer, int numChannels) noexcept;

        int stageCount = 0;
        std::array<SecondOrderSection, maxShelfOrder> sections;
    };

    struct FilterDesignState
    {
        bool valid = false;
        bool active = false;
        FilterType type = FilterType::bell;
        float frequency = 0.0f;
        float bandwidth = 0.0f;
        float slope = 0.0f;
        float gainDb = 0.0f;
        double sampleRate = 0.0;
    };

    struct TtssSplitState
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
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void registerParameterListeners();
    void unregisterParameterListeners();
    FilterType getFilterTypeForBand(size_t bellIndex) const noexcept;
    bool filterDesignMatches(size_t bellIndex,
                             bool active,
                             FilterType type,
                             float frequency,
                             float bandwidth,
                             float slope,
                             float gainDb) const noexcept;
    void storeFilterDesignState(size_t bellIndex,
                                bool active,
                                FilterType type,
                                float frequency,
                                float bandwidth,
                                float slope,
                                float gainDb) noexcept;
    void resetBellFilters() noexcept;
    void setBellIdentityResponse(size_t bellIndex) noexcept;
    void setShelfIdentityResponse(size_t bellIndex) noexcept;
    void setCutIdentityResponse(size_t bellIndex) noexcept;
    void setTiltIdentityResponse(size_t bellIndex) noexcept;
    void updateBellOrderFilter(BellOrderFilter& filter, int order, double frequency, double octaveBandwidth, double gain) noexcept;
    void updateShelfOrderFilterRaw(BiquadCascade& filter, FilterType filterType, int order, double frequency, double octaveBandwidth, double gain) noexcept;
    void updateCutOrderFilterRaw(BiquadCascade& filter, FilterType filterType, int order, double frequency, double octaveBandwidth) noexcept;
    void updateTiltFilter(BiquadCascade& filter, double frequency, double gainDb) noexcept;
    std::complex<double> evaluateBellResponseAt(const BellOrderFilter& filter, double frequency) const noexcept;
    std::complex<double> evaluateCascadeResponseAt(const BiquadCascade& filter, double frequency) const noexcept;
    double evaluateCascadeMagnitudeAt(const BiquadCascade& filter, double frequency) const noexcept;
    void updateInterpolatedCascadeFilter(BiquadCascade& target,
                                         const BiquadCascade& lower,
                                         const BiquadCascade& upper,
                                         double blend) noexcept;
    void buildCutBlendFilter(BiquadCascade& target,
                             FilterType filterType,
                             double frequency,
                             double octaveBandwidth,
                             double slope) noexcept;
    void rebuildCutBlendFilter(size_t bellIndex,
                               FilterType filterType,
                               double frequency,
                               double octaveBandwidth,
                               double slope) noexcept;
    void updateBellFilters();
    static TtssMode ttssModeFromChoiceIndex(int choiceIndex) noexcept;
    float processTtssDetectorSample(float level,
                                    const TransientSplitSettings& settings) noexcept;
    void resetTtssSplitState() noexcept;
    static float makeTtssReleaseProgress(float progress, float curveAmount) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* bypassWithGainParam = nullptr;
    std::array<std::atomic<float>*, maxBellFilterCount> filterTypeParams {};
    std::array<std::atomic<float>*, maxBellFilterCount> filterTtssParams {};
    std::array<std::atomic<float>*, maxBellFilterCount> filterLrmsParams {};
    std::array<std::atomic<float>*, maxBellFilterCount> bellFrequencyParams {};
    std::array<std::atomic<float>*, maxBellFilterCount> bellBandwidthParams {};
    std::array<juce::AudioParameterChoice*, maxBellFilterCount> bellSlopeChoiceParams {};
    std::array<std::atomic<float>*, maxBellFilterCount> bellGainParams {};
    std::array<std::atomic<float>*, maxBellFilterCount> bellBypassParams {};
    std::array<std::array<BellOrderFilter, maxBellOrder>, maxBellFilterCount> bellOrderFilters;
    std::array<std::array<BiquadCascade, maxShelfOrder>, maxBellFilterCount> shelfOrderFilters;
    std::array<BiquadCascade, maxBellFilterCount> tiltFilters;
    std::array<BiquadCascade, maxBellFilterCount> cutBlendFilters;
    std::array<FilterDesignState, maxBellFilterCount> cachedFilterStates {};
    juce::AudioBuffer<float> bellProcessBufferA;
    juce::AudioBuffer<float> bellProcessBufferB;
    juce::AudioBuffer<float> lrmsWorkBuffer;
    juce::AudioBuffer<float> lrmsAuxBuffer;
    juce::AudioBuffer<float> ttssTransientBuffer;
    juce::AudioBuffer<float> ttssSustainBuffer;
    int preparedNumChannels = 2;
    int lastProcessedBlockSize = 0;
    double currentSampleRate = 44100.0;
    std::atomic<int> activeBellCount { 0 };
    std::atomic<bool> eqeFiltersDirty { true };
    TtssSplitState ttssSplitState;
    TransientSplitProvider* transientSplitProvider = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqeModuleProcessor)
};
