#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <complex>

class EqeAudioProcessor final : public juce::AudioProcessor
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

    inline static constexpr auto paramGlobalGainLId = "global_gain_l";
    inline static constexpr auto paramGlobalGainRId = "global_gain_r";
    inline static constexpr auto paramGlobalWideId = "global_wide";
    inline static constexpr auto paramOutGainId = "out_gain";
    inline static constexpr auto paramGlobalBypassId = "global_bypass";
    inline static constexpr auto paramGlobalBypassOutGainOnlyId = "global_bypass_out_gain_only";
    inline static constexpr auto activeBellCountStateKey = "active_bell_count";
    inline static constexpr float fixedSlopeDbPerOct = 12.0f;
    inline static constexpr auto filterPresetLastSelectedStateKey = "filter_preset_last_selected";
    inline static constexpr auto filterPresetDefaultSelectedStateKey = "filter_preset_default_selected";
    inline static constexpr auto activeModuleStateKey = "vx.active_module";
    inline static constexpr auto eqeModuleId = "eqe";
    inline static constexpr auto editorWidthStateKey = "mxe.editor.width";
    inline static constexpr auto editorHeightStateKey = "mxe.editor.height";
    static constexpr int maxBellFilterCount = 64;
#if ! JUCE_IOS
    static constexpr int visualizerScopeSize = 1024;
#endif

    static juce::String getFilterTypeParamId(int filterIndex);
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
    inline static constexpr std::array<FilterType, 6> filterTypePresetOrder
    {
        FilterType::lowCut,
        FilterType::lowShelf,
        FilterType::bell,
        FilterType::tilt,
        FilterType::highShelf,
        FilterType::highCut
    };

    EqeAudioProcessor();
    ~EqeAudioProcessor() override;

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
    bool isEqeModuleLoaded() const noexcept;
    void setEqeModuleLoaded(bool shouldBeLoaded);
    juce::Point<int> getLastEditorSize() const noexcept;
    void setLastEditorSize(int width, int height) noexcept;

    int getActiveBellCount() const noexcept;
    bool addBellFilter() noexcept;
    bool removeBellFilter(int bellIndex) noexcept;
    bool clearBellFilters() noexcept;
    bool moveBellFilter(int sourceIndex, int destinationIndex) noexcept;
    juce::String getDefaultFilterPresetName() const;
    juce::String getLastFilterPresetName() const;
    juce::StringArray getFilterPresetNames() const;
#if ! JUCE_IOS
    void copyVisualiserResponse(std::array<float, visualizerScopeSize>& stereoDb,
                                std::array<float, visualizerScopeSize>& leftDb,
                                std::array<float, visualizerScopeSize>& rightDb,
                                std::array<float, visualizerScopeSize>& midDb,
                                std::array<float, visualizerScopeSize>& sideDb,
                                double& sampleRateOut) noexcept;
#endif
    bool saveFilterPreset(const juce::String& presetName);
    bool renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName);
    bool setDefaultFilterPreset(const juce::String& presetName);
    bool loadFilterPreset(const juce::String& presetName) noexcept;
    bool deleteFilterPreset(const juce::String& presetName);
    float getGlobalClipIndicator() const noexcept
    {
        return globalClipIndicator.load(std::memory_order_relaxed);
    }
    void resetGlobalClipIndicator() noexcept
    {
        globalClipIndicator.store(0.0f, std::memory_order_relaxed);
    }
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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setActiveBellCount(int newCount) noexcept;
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
#if ! JUCE_IOS
    std::complex<double> evaluateBellResponseAt(const BellOrderFilter& filter, double frequency) const noexcept;
#endif
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

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* globalGainLParam = nullptr;
    std::atomic<float>* globalGainRParam = nullptr;
    std::atomic<float>* globalWideParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;
    std::atomic<float>* globalBypassParam = nullptr;
    std::atomic<float>* globalBypassOutGainOnlyParam = nullptr;
    std::array<std::atomic<float>*, maxBellFilterCount> filterTypeParams {};
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
    std::atomic<float> globalClipIndicator { 0.0f };
    std::atomic<bool> eqeModuleLoaded { false };
    std::atomic<int> lastEditorWidth { 0 };
    std::atomic<int> lastEditorHeight { 0 };
    juce::AudioBuffer<float> bellProcessBufferA;
    juce::AudioBuffer<float> bellProcessBufferB;
    int preparedNumChannels = 2;
    int lastProcessedBlockSize = 0;
    double currentSampleRate = 44100.0;
    std::atomic<int> activeBellCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqeAudioProcessor)
};
