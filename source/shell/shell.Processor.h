#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

#include "../modules/eqe/module.eqe.Processor.h"

class SpeModuleProcessor;
class MxeAudioProcessor;
class TseModuleProcessor;

class VxAudioProcessor final : public juce::AudioProcessor
{
public:
    using FilterType = EqeModuleProcessor::FilterType;

    inline static constexpr auto paramGlobalGainLId = "global_gain_l";
    inline static constexpr auto paramGlobalGainRId = "global_gain_r";
    inline static constexpr auto paramGlobalWideId = "global_wide";
    inline static constexpr auto paramOutGainId = "out_gain";
    inline static constexpr auto paramGlobalBypassId = "global_bypass";
    inline static constexpr auto paramGlobalBypassOutGainOnlyId = "global_bypass_out_gain_only";
    inline static constexpr auto activeBellCountStateKey = EqeModuleProcessor::activeBellCountStateKey;
    inline static constexpr float fixedSlopeDbPerOct = EqeModuleProcessor::fixedSlopeDbPerOct;
    inline static constexpr auto filterPresetLastSelectedStateKey = "filter_preset_last_selected";
    inline static constexpr auto filterPresetDefaultSelectedStateKey = "filter_preset_default_selected";
    inline static constexpr auto activeModuleStateKey = "vx.active_module";
    inline static constexpr auto moduleChainStateKey = "vx.module_chain";
    inline static constexpr auto speModuleStateKey = "vx.spe_state";
    inline static constexpr auto mxeModuleStateKey = "vx.mxe_state";
    inline static constexpr auto tseModuleStateKey = "vx.tse_state";
    inline static constexpr auto eqeModuleId = "eqe";
    inline static constexpr auto speModuleId = "spe";
    inline static constexpr auto mxeModuleId = "mxe";
    inline static constexpr auto tseModuleId = "tse";
    inline static constexpr auto editorWidthStateKey = "mxe.editor.width";
    inline static constexpr auto editorHeightStateKey = "mxe.editor.height";
    static constexpr int maxBellFilterCount = EqeModuleProcessor::maxBellFilterCount;
    static constexpr int visualizerScopeSize = EqeModuleProcessor::visualizerScopeSize;
    static constexpr int maxModuleInstanceCount = 8;
    static constexpr int maxEqeModuleInstanceCount = maxModuleInstanceCount;
    static constexpr int maxModuleSlotCount = 16;

    enum class ActiveModule
    {
        none,
        eqe,
        spe,
        mxe,
        tse,
    };
    inline static constexpr std::array<FilterType, 6> filterTypePresetOrder
    {
        FilterType::lowCut,
        FilterType::lowShelf,
        FilterType::bell,
        FilterType::tilt,
        FilterType::highShelf,
        FilterType::highCut
    };

    explicit VxAudioProcessor();
    ~VxAudioProcessor() override;

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
    ActiveModule getActiveModule() const noexcept;
    void setActiveModule(ActiveModule module);
    void setActiveModule(ActiveModule module, int instanceIndex);
    bool setActiveModuleIfPresent(ActiveModule module, int instanceIndex);
    bool addModuleToChain(ActiveModule module);
    bool removeModuleFromChain(ActiveModule module);
    bool removeModuleFromChain(ActiveModule module, int instanceIndex);
    bool moveModuleInChainAtPosition(int position, int direction);
    int getLoadedModuleCount() const noexcept;
    struct ModuleSlot
    {
        ActiveModule module = ActiveModule::none;
        int instanceIndex = -1;
    };
    ModuleSlot getLoadedModuleSlotAtPosition(int position) const noexcept;
    juce::String getLoadedModuleLabelAtPosition(int position) const;
    bool hasTtssSourceForActiveEqeModule() const noexcept;
    int getActiveModuleInstanceIndex() const noexcept;
    bool isEqeModuleLoaded() const noexcept;
    EqeModuleProcessor* getEqeModuleProcessor(int instanceIndex) noexcept;
    const EqeModuleProcessor* getEqeModuleProcessor(int instanceIndex) const noexcept;
    EqeModuleProcessor* getActiveEqeModuleProcessor() noexcept;
    const EqeModuleProcessor* getActiveEqeModuleProcessor() const noexcept;
    bool isSpeModuleLoaded() const noexcept;
    SpeModuleProcessor* getSpeModuleProcessor(int instanceIndex) noexcept;
    const SpeModuleProcessor* getSpeModuleProcessor(int instanceIndex) const noexcept;
    SpeModuleProcessor* getSpeModuleProcessor() noexcept;
    const SpeModuleProcessor* getSpeModuleProcessor() const noexcept;
    bool isMxeModuleLoaded() const noexcept;
    MxeAudioProcessor* getMxeModuleProcessor(int instanceIndex) noexcept;
    const MxeAudioProcessor* getMxeModuleProcessor(int instanceIndex) const noexcept;
    MxeAudioProcessor* getMxeModuleProcessor() noexcept;
    const MxeAudioProcessor* getMxeModuleProcessor() const noexcept;
    bool isTseModuleLoaded() const noexcept;
    TseModuleProcessor* getTseModuleProcessor(int instanceIndex) noexcept;
    const TseModuleProcessor* getTseModuleProcessor(int instanceIndex) const noexcept;
    TseModuleProcessor* getTseModuleProcessor() noexcept;
    const TseModuleProcessor* getTseModuleProcessor() const noexcept;
    juce::Point<int> getLastEditorSize() const noexcept;
    void setLastEditorSize(int width, int height) noexcept;

    int getActiveBellCount() const noexcept;
    void copyVisualiserResponse(std::array<float, visualizerScopeSize>& stereoDb,
                                std::array<float, visualizerScopeSize>& leftDb,
                                std::array<float, visualizerScopeSize>& rightDb,
                                std::array<float, visualizerScopeSize>& midDb,
                                std::array<float, visualizerScopeSize>& sideDb,
                                double& sampleRateOut) noexcept;
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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setActiveBellCount(int newCount) noexcept;
    int createEqeModuleInstance();
    int createSpeModuleInstance();
    int createMxeModuleInstance();
    int createTseModuleInstance();
    int getModuleChainLatencySamples() const noexcept;
    void updateShellLatency() noexcept;
    juce::String encodeModuleChainStateText() const;
    void restoreModuleChainFromStateText(const juce::String& text);
    void storeModuleChainStateProperty();

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* globalGainLParam = nullptr;
    std::atomic<float>* globalGainRParam = nullptr;
    std::atomic<float>* globalWideParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;
    std::atomic<float>* globalBypassParam = nullptr;
    std::atomic<float>* globalBypassOutGainOnlyParam = nullptr;
    std::atomic<float> globalClipIndicator { 0.0f };
    std::vector<std::unique_ptr<EqeModuleProcessor>> eqeModuleProcessors;
    std::vector<std::unique_ptr<SpeModuleProcessor>> speModuleProcessors;
    std::vector<std::unique_ptr<MxeAudioProcessor>> mxeModuleProcessors;
    std::vector<std::unique_ptr<TseModuleProcessor>> tseModuleProcessors;
    std::vector<ModuleSlot> moduleChain;
    std::atomic<ActiveModule> activeModule { ActiveModule::none };
    std::atomic<int> activeModuleInstanceIndex { -1 };
    std::atomic<int> lastEditorWidth { 0 };
    std::atomic<int> lastEditorHeight { 0 };
    int preparedNumChannels = 2;
    int lastProcessedBlockSize = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VxAudioProcessor)
};
