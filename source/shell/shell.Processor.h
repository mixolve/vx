#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

#include "../modules/eql/module.eql.Processor.h"

class FftModuleProcessor;
class TlsAudioProcessor;
class DynAudioProcessor;
class TrsModuleProcessor;

class VxAudioProcessor final : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::ValueTree::Listener
{
public:
    using FilterType = EqlModuleProcessor::FilterType;

    inline static constexpr auto paramGlobalBypassId = "global_bypass";
    inline static constexpr auto paramHostSlotPrefix = "host_slot_";
    inline static constexpr auto activeModuleStateKey = "vx.active_module";
    inline static constexpr auto eqlModuleStateKey = "vx.eql_state";
    inline static constexpr auto fftModuleStateKey = "vx.fft_state";
    inline static constexpr auto tlsModuleStateKey = "vx.tls_state";
    inline static constexpr auto dynModuleStateKey = "vx.dyn_state";
    inline static constexpr auto trsModuleStateKey = "vx.trs_state";
    inline static constexpr auto abCompareSnapshotAStateKey = "vx.ab_compare.a";
    inline static constexpr auto abCompareSnapshotBStateKey = "vx.ab_compare.b";
    inline static constexpr auto abCompareActiveSlotStateKey = "vx.ab_compare.active";
    inline static constexpr auto eqlModuleId = "eql";
    inline static constexpr auto fftModuleId = "fft";
    inline static constexpr auto tlsModuleId = "tls";
    inline static constexpr auto dynModuleId = "dyn";
    inline static constexpr auto trsModuleId = "trs";
    inline static constexpr auto editorWidthStateKey = "vx.editor.width";
    inline static constexpr auto editorHeightStateKey = "vx.editor.height";
    static constexpr int maxEqlFilterCount = EqlModuleProcessor::maxFilterCount;
    static constexpr int hostAutomationSlotCount = 64;

    enum class ActiveModule
    {
        none,
        tls,
        eql,
        fft,
        dyn,
        trs,
    };
    inline static constexpr std::array<FilterType, 7> filterTypePresetOrder
    {
        FilterType::lowCut,
        FilterType::lowShelf,
        FilterType::bell,
        FilterType::tilt,
        FilterType::highShelf,
        FilterType::highCut,
        FilterType::volume
    };

    explicit VxAudioProcessor();
    ~VxAudioProcessor() override;

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
    void getStateInformationForABCompareSnapshot(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes) override;
    bool setStateInformationPreservingLoadedModule(const void* data,
                                                   int sizeInBytes,
                                                   bool suspendProcessingForRestore = true);
    bool applyStateInformationForABCompare(const void* data, int sizeInBytes);
    static void removeModuleStateProperties(juce::ValueTree& state);
    int getABCompareActiveSlot() const noexcept;
    void setABCompareActiveSlot(int slot) noexcept;
    bool isABCompareSnapshotValid(int slot) const noexcept;
    juce::MemoryBlock getABCompareSnapshot(int slot) const;
    void setABCompareSnapshot(int slot, const juce::MemoryBlock& snapshot);

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;
    static juce::String getHostSlotParameterId(int slotIndex);
    static juce::String getHostSlotLetterLabel(int slotIndex);
    static const char* stateIdForModule(ActiveModule module) noexcept;
    ActiveModule getActiveModule() const noexcept;
    void setActiveModule(ActiveModule module);
    bool loadModule(ActiveModule module);
    bool clearLoadedModule();
    EqlModuleProcessor* getEqlModuleProcessor() noexcept;
    const EqlModuleProcessor* getEqlModuleProcessor() const noexcept;
    FftModuleProcessor* getFftModuleProcessor() noexcept;
    const FftModuleProcessor* getFftModuleProcessor() const noexcept;
    TlsAudioProcessor* getTlsModuleProcessor() noexcept;
    const TlsAudioProcessor* getTlsModuleProcessor() const noexcept;
    DynAudioProcessor* getDynModuleProcessor() noexcept;
    const DynAudioProcessor* getDynModuleProcessor() const noexcept;
    TrsModuleProcessor* getTrsModuleProcessor() noexcept;
    const TrsModuleProcessor* getTrsModuleProcessor() const noexcept;
    juce::Point<int> getLastEditorSize() const noexcept;
    void setLastEditorSize(int width, int height) noexcept;
    void notifyHostOfStateChange();

    float getGlobalClipIndicator() const noexcept
    {
        return globalClipIndicator.load(std::memory_order_relaxed);
    }
private:
    class ScopedProcessingSuspend
    {
    public:
        explicit ScopedProcessingSuspend(VxAudioProcessor& processorIn) noexcept
            : processor(processorIn)
        {
            processor.suspendProcessing(true);
        }

        ~ScopedProcessingSuspend()
        {
            processor.suspendProcessing(false);
        }

    private:
        VxAudioProcessor& processor;
    };

    static constexpr size_t maxSupportedChannels = 2;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    bool createModuleInstance(ActiveModule module);
    void resetModuleProcessors() noexcept;
    static ActiveModule moduleFromStateId(const juce::String& moduleId);
    int getActiveModuleLatencySamples() const noexcept;
    void updateShellLatency() noexcept;
    void restoreLoadedModuleFromStateText(const juce::String& text, bool publishActiveModule = true);
    void registerActiveModuleStateListeners();
    void clearActiveModuleStateListeners();
    void writeStateInformation(juce::MemoryBlock& destData, bool includeABCompareState);
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                  const juce::Identifier& property) override;

    juce::AudioProcessorValueTreeState parameters;
    mutable juce::CriticalSection processingLock;
    juce::AudioProcessorValueTreeState* observedModuleValueTreeState = nullptr;
    std::vector<juce::String> observedModuleParameterIds;
    juce::ValueTree observedModuleState;
    std::atomic<float>* globalBypassParam = nullptr;
    std::atomic<float> globalClipIndicator { 0.0f };
    std::unique_ptr<EqlModuleProcessor> eqlModuleProcessor;
    std::unique_ptr<FftModuleProcessor> fftModuleProcessor;
    std::unique_ptr<TlsAudioProcessor> tlsModuleProcessor;
    std::unique_ptr<DynAudioProcessor> dynModuleProcessor;
    std::unique_ptr<TrsModuleProcessor> trsModuleProcessor;
    std::atomic<ActiveModule> activeModule { ActiveModule::none };
    std::atomic<bool> processingPrepared { false };
    std::atomic<int> lastEditorWidth { 0 };
    std::atomic<int> lastEditorHeight { 0 };
    std::atomic<bool> suppressHostStateNotifications { false };
    mutable juce::CriticalSection abCompareLock;
    std::array<juce::MemoryBlock, 2> abCompareSnapshots;
    std::array<bool, 2> abCompareSnapshotValid {};
    std::atomic<int> abCompareActiveSlot { 0 };
    std::atomic<bool> abCompareLatencyLocked { false };
    std::atomic<int> abCompareLatencyFloorSamples { 0 };
    int preparedNumChannels = 2;
    int lastProcessedBlockSize = 0;
    double currentSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VxAudioProcessor)
};
