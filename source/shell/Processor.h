#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

#include "../modules/eql/Processor.h"
#include "../crossover/BufferRouter.h"

class FftModuleProcessor;
class FftProcessorBank;
class EqlProcessorBank;
class TlsAudioProcessor;
class DynAudioProcessor;
class TrsModuleProcessor;

class AvaAudioProcessor final : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::ValueTree::Listener,
                               private juce::AsyncUpdater
{
public:
    using FilterType = EqlModuleProcessor::FilterType;

    inline static constexpr auto paramGlobalBypassId = "global_bypass";
    inline static constexpr auto paramCrossoverPrefix = "crossover_";
    inline static constexpr auto paramCrossoverActiveSplitCountId = "crossover_activeXovers";
    inline static constexpr auto paramHostSlotPrefix = "host_slot_";
    inline static constexpr auto activeModuleStateKey = "ava.active_module";
    inline static constexpr auto eqlModuleStateKey = "ava.eql_state";
    inline static constexpr auto fftModuleStateKey = "ava.fft_state";
    inline static constexpr auto tlsModuleStateKey = "ava.tls_state";
    inline static constexpr auto dynModuleStateKey = "ava.dyn_state";
    inline static constexpr auto trsModuleStateKey = "ava.trs_state";
    inline static constexpr auto abCompareSnapshotAStateKey = "ava.ab_compare.a";
    inline static constexpr auto abCompareSnapshotBStateKey = "ava.ab_compare.b";
    inline static constexpr auto abCompareActiveSlotStateKey = "ava.ab_compare.active";
    inline static constexpr auto eqlModuleId = "eql";
    inline static constexpr auto fftModuleId = "fft";
    inline static constexpr auto tlsModuleId = "tls";
    inline static constexpr auto dynModuleId = "dyn";
    inline static constexpr auto trsModuleId = "trs";
    inline static constexpr auto editorWidthStateKey = "ava.editor.width";
    inline static constexpr auto editorHeightStateKey = "ava.editor.height";
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

    explicit AvaAudioProcessor();
    ~AvaAudioProcessor() override;

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
    static juce::String getHostSlotTargetStateKey(int slotIndex);
    static juce::String getCrossoverParameterId(const char* suffix);
    static juce::String getCrossoverSoloParameterId(size_t rangeIndex);
    ava::crossover::Settings getCrossoverSettings() const noexcept;
    static const char* stateIdForModule(ActiveModule module) noexcept;
    ActiveModule getActiveModule() const noexcept;
    void setActiveModule(ActiveModule module);
    bool loadModule(ActiveModule module);
    bool clearLoadedModule();
    EqlModuleProcessor* getEqlModuleProcessor() noexcept;
    const EqlModuleProcessor* getEqlModuleProcessor() const noexcept;
    EqlProcessorBank* getEqlProcessorBank() noexcept;
    const EqlProcessorBank* getEqlProcessorBank() const noexcept;
    void setSelectedCrossoverRange(size_t rangeIndex);
    FftModuleProcessor* getFftModuleProcessor() noexcept;
    const FftModuleProcessor* getFftModuleProcessor() const noexcept;
    FftProcessorBank* getFftProcessorBank() noexcept;
    const FftProcessorBank* getFftProcessorBank() const noexcept;
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
        explicit ScopedProcessingSuspend(AvaAudioProcessor& processorIn) noexcept
            : processor(processorIn)
        {
            processor.suspendProcessing(true);
        }

        ~ScopedProcessingSuspend()
        {
            processor.suspendProcessing(false);
        }

    private:
        AvaAudioProcessor& processor;
    };

    static constexpr size_t maxSupportedChannels = 2;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::RangedAudioParameter* findHostSlotTarget(const juce::String& parameterId) noexcept;
    void applyHostSlotValue(int slotIndex, float normalizedValue) noexcept;
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
    void handleAsyncUpdate() override;
    void ensureActiveCrossoverRangeCount(size_t rangeCount);

    juce::AudioProcessorValueTreeState parameters;
    ava::crossover::BufferRouter crossoverRouter;
    mutable juce::CriticalSection processingLock;
    juce::AudioProcessorValueTreeState* observedModuleValueTreeState = nullptr;
    std::vector<juce::String> observedModuleParameterIds;
    juce::ValueTree observedModuleState;
    std::atomic<float>* globalBypassParam = nullptr;
    std::atomic<float> globalClipIndicator { 0.0f };
    std::unique_ptr<EqlProcessorBank> eqlProcessorBank;
    std::unique_ptr<FftProcessorBank> fftProcessorBank;
    std::unique_ptr<TlsAudioProcessor> tlsModuleProcessor;
    std::unique_ptr<DynAudioProcessor> dynModuleProcessor;
    std::unique_ptr<TrsModuleProcessor> trsModuleProcessor;
    std::atomic<ActiveModule> activeModule { ActiveModule::none };
    std::atomic<bool> processingPrepared { false };
    std::atomic<int> lastEditorWidth { 0 };
    std::atomic<int> lastEditorHeight { 0 };
    std::atomic<bool> suppressHostStateNotifications { false };
    std::atomic<size_t> requestedCrossoverRangeCount { 1 };
    mutable juce::CriticalSection abCompareLock;
    std::array<juce::MemoryBlock, 2> abCompareSnapshots;
    std::array<bool, 2> abCompareSnapshotValid {};
    std::atomic<int> abCompareActiveSlot { 0 };
    std::atomic<bool> abCompareLatencyLocked { false };
    std::atomic<int> abCompareLatencyFloorSamples { 0 };
    int preparedNumChannels = 2;
    int lastProcessedBlockSize = 0;
    double currentSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AvaAudioProcessor)
};
