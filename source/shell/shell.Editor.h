#pragma once

#include "shell.Processor.h"

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

class BoxTextButton;
class ChoiceControl;
class LocalParameterControl;
class ParameterControl;

class VxAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer,
                                      private juce::AudioProcessorValueTreeState::Listener,
                                      private juce::ValueTree::Listener
{
public:
    explicit VxAudioProcessorEditor(VxAudioProcessor&);
    ~VxAudioProcessorEditor() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class VxLookAndFeel;
    struct PresetsSection;
    struct BellSection;

    void openShellGlobalHostSection();
    void showModulePicker();
    void loadEqeModule();
    void loadSpeModule();
    void loadMieModule();
    void loadMxeModule();
    void loadTseModule();
    void openVisualizerSection();
    void selectBellSection(int bellIndex);
    void refreshFilterPresetList(const juce::String& preferredSelection = {});
    void reloadFilterPresetFromProcessor();
    void addFilterPreset();
    void saveFilterPreset();
    bool renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName);
    void setDefaultFilterPreset();
    void deleteSelectedFilterPreset();

public:
    bool handleHostSlotAssignRequest(const juce::String& parameterId,
                                     const juce::String& parameterName,
                                     float normalizedValue);
    void showTextPrompt(const juce::String& currentText,
                        std::function<bool(const juce::String&)> onCommit,
                        juce::Rectangle<int> anchorBounds = {},
                        std::function<void()> onClose = {},
                        std::function<void()> onDismiss = {});
    void showInfoPrompt(const juce::String& markdownText);
    void showChoicePrompt(const juce::Rectangle<int>& anchorBounds,
                          const juce::StringArray& choices,
                          int selectedIndex,
                          std::vector<bool> itemEnabledStates,
                          juce::Justification itemJustification,
                          std::function<void(int)> onSelect,
                          std::function<void()> onClose = {},
                          std::function<void()> onDismiss = {});
    juce::Rectangle<int> getInfoPromptAnchorBounds() const noexcept;
    juce::Rectangle<int> getInfoPromptVisibleBounds() const noexcept;
private:
    void dismissTextPrompt();
    void timerCallback() override;
    void normalizeSlopeForType(int bellIndex);
    void sortBellSectionsByPlace();
    void sortBellSectionsByFrequency();
    void sortBellSectionsByDuo();
    void clearAllFilters();
    void performUndo();
    void performRedo();
    void applyBellSortOrder(const std::vector<int>& orderedIndices);
    void moveBellSection(int sourceIndex, int destinationIndex);
    void restoreEditorStateFromValueTree();
    void storeEditorStateToValueTree() noexcept;
    juce::Point<int> getRestoredEditorSize() const noexcept;
    juce::Rectangle<int> getBellSectionBounds(int bellIndex) const;
    void resetBellSectionStoredValues(int bellIndex);
    void removeBellSectionStoredValues(int removedIndex, int previousCount);
    void updateSectionStates();
    void updateEditorWidthForVisualizerVisibility();
    void refreshVisualizerResponse();
    void syncFocusedParameterControl();
    double getFocusedParameterControlValueForTarget() const noexcept;
    double getFocusedParameterTargetValueForControl() const noexcept;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                  const juce::Identifier& property) override;
    void registerParameterListeners();
    void unregisterParameterListeners();
    void syncHostSlotAssignmentValue(int slotIndex, float normalizedValue);
    void registerObservedModuleParameterListeners(juce::AudioProcessorValueTreeState& moduleValueTreeState);
    void refreshModuleStateListeners();
    void clearModuleStateListeners();
    void detachModuleEditorBindings();
    void rebindActiveModuleEditors();
    void setupShellControls();
    void setupVisualizerControls();
    void setupPresetControls();
    void setupSpeControls(juce::AudioProcessorValueTreeState& speState,
                          SpeModuleProcessor& speProcessor);
    void refreshSpeAnalyserControls(SpeModuleProcessor& speProcessor);
    void setupEqeControls(juce::AudioProcessorValueTreeState& initialEqeState);
    EqeModuleProcessor* getActiveEqeProcessor() noexcept;
    const EqeModuleProcessor* getActiveEqeProcessor() const noexcept;
    void scheduleHistorySnapshot();
    void commitPendingHistorySnapshot(bool force = false);
    void applyHistorySnapshot(const juce::MemoryBlock& snapshot);
    void updateUndoRedoButtons();
    int getActiveFilterContentHeight() const;
    int getFilterContentHeight() const;
    int getSpeMainContentHeight() const;
    int getSpeAnalyserContentHeight() const;
    int getSpeSectionContentHeight() const;
    int getActiveBellCount() const noexcept;
    void updateVisualizerPanelBounds();
    void layoutShellGlobalSection(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutFooter(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutModuleTabRows(juce::Rectangle<int>& bounds, int editorInsetX);
    void finalizeLayout() noexcept;
    void layoutNoModuleState(juce::Rectangle<int>& bounds);
    void layoutModuleEditorContent(juce::Rectangle<int>& bounds);
    void layoutSpeModuleSections(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutEqeModuleSections(juce::Rectangle<int>& bounds, int editorInsetX);
    void rebuildModuleTabRows();
    void clearHostSlot(int slotIndex);
    void refreshHostSlotButtons();

    struct ObservedModuleParameterListeners
    {
        juce::AudioProcessorValueTreeState* valueTreeState = nullptr;
        std::vector<juce::String> parameterIds;
    };

    VxAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;
    std::vector<juce::ValueTree> observedModuleStates;
    std::vector<ObservedModuleParameterListeners> observedModuleParameterListeners;
    std::unique_ptr<VxLookAndFeel> lookAndFeel;
    std::unique_ptr<BoxTextButton> shellGlobalHeader;
    std::unique_ptr<BoxTextButton> shellGlobalHostHeader;
    std::unique_ptr<BoxTextButton> moduleAddButton;
    struct ModuleTabRow
    {
        std::unique_ptr<BoxTextButton> tabButton;
        int slotIndex = -1;
    };
    std::vector<std::unique_ptr<ModuleTabRow>> moduleTabRows;
    std::unique_ptr<BoxTextButton> visualizerHeader;
    std::unique_ptr<BoxTextButton> addFilterButton;
    std::unique_ptr<PresetsSection> presetsSection;
    std::unique_ptr<ParameterControl> speAttackControl;
    std::unique_ptr<ParameterControl> speReleaseControl;
    std::unique_ptr<ParameterControl> speKneeControl;
    std::unique_ptr<ParameterControl> speRatioControl;
    std::unique_ptr<ParameterControl> speDspFftSizeControl;
    std::unique_ptr<ParameterControl> speDspSlopeControl;
    std::unique_ptr<BoxTextButton> speDeltaButton;
    std::unique_ptr<ButtonAttachment> speDeltaAttachment;
    std::unique_ptr<ParameterControl> speDualMonoLeftThresholdControl;
    std::unique_ptr<ParameterControl> speDualMonoLeftAdaptiveControl;
    std::unique_ptr<ParameterControl> speDualMonoLeftAdaptiveOffsetControl;
    std::unique_ptr<ParameterControl> speDualMonoRightThresholdControl;
    std::unique_ptr<ParameterControl> speDualMonoRightAdaptiveControl;
    std::unique_ptr<ParameterControl> speDualMonoRightAdaptiveOffsetControl;
    std::unique_ptr<BoxTextButton> speDualMonoLinkButton;
    std::unique_ptr<ButtonAttachment> speDualMonoLinkAttachment;
    std::unique_ptr<LocalParameterControl> speAnalyserFftSizeControl;
    std::unique_ptr<LocalParameterControl> speAnalyserOverlapControl;
    std::unique_ptr<LocalParameterControl> speAnalyserLeftControl;
    std::unique_ptr<LocalParameterControl> speAnalyserRightControl;
    std::unique_ptr<LocalParameterControl> speAnalyserRangeLowControl;
    std::unique_ptr<LocalParameterControl> speAnalyserRangeHighControl;
    std::unique_ptr<LocalParameterControl> speAnalyserSlopeControl;
    std::unique_ptr<LocalParameterControl> speAnalyserTimeControl;
    std::unique_ptr<LocalParameterControl> visualizerRangeLowControl;
    std::unique_ptr<LocalParameterControl> visualizerRangeHighControl;
    std::unique_ptr<BoxTextButton> visualizerCursorButton;
    std::unique_ptr<BoxTextButton> visualizerShowStereoButton;
    std::unique_ptr<BoxTextButton> visualizerShowLeftButton;
    std::unique_ptr<BoxTextButton> visualizerShowRightButton;
    std::unique_ptr<BoxTextButton> visualizerShowMidButton;
    std::unique_ptr<BoxTextButton> visualizerShowSideButton;
    std::unique_ptr<BoxTextButton> globalBypassButton;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment;
    std::unique_ptr<BoxTextButton> clearFiltersButton;
    std::unique_ptr<BoxTextButton> undoButton;
    std::unique_ptr<BoxTextButton> redoButton;
    std::unique_ptr<BoxTextButton> sortPlaceButton;
    std::unique_ptr<BoxTextButton> sortFreqButton;
    std::unique_ptr<BoxTextButton> sortDuoButton;
    std::array<std::unique_ptr<BoxTextButton>, VxAudioProcessor::hostAutomationSlotCount> hostSlotButtons;
    std::array<std::unique_ptr<BellSection>, VxAudioProcessor::maxBellFilterCount> bellSections;
    juce::Viewport shellGlobalHostViewport;
    juce::Component shellGlobalHostContent;
    juce::Viewport filterViewport;
    juce::Component filterContent;
    std::unique_ptr<juce::Slider> focusedParameterControl;
    std::unique_ptr<BoxTextButton> footerTab;
    std::unique_ptr<juce::Component> visualizerComponent;
    std::unique_ptr<juce::Component> speAnalyserComponent;
    std::unique_ptr<juce::Component> textPromptOverlay;
    std::unique_ptr<juce::Component> mieModuleEditor;
    std::unique_ptr<juce::Component> mxeModuleEditor;
    std::unique_ptr<juce::Component> tseModuleEditor;
    bool eqeModuleLoaded = false;
    bool speModuleLoaded = false;
    bool mieModuleLoaded = false;
    bool mxeModuleLoaded = false;
    bool tseModuleLoaded = false;
    bool shellGlobalHostExpanded = false;
    bool visualizerExpanded = false;
    bool visualizerCursorEnabled = true;
    bool visualizerShowStereo = true;
    bool visualizerShowLeft = true;
    bool visualizerShowRight = true;
    bool visualizerShowMid = true;
    bool visualizerShowSide = true;
    int lastCollapsedEditorWidth = 0;
    float visualizerRangeLowDb = -24.0f;
    float visualizerRangeHighDb = 24.0f;
    std::vector<int> bellDisplayOrder;
    bool suppressBellSectionValueChangeHandlers = false;
    bool suppressVisualizerControlChangeHandlers = false;
    bool suppressSpeAnalyserControlChangeHandlers = false;
    bool suppressFocusedParameterControlChangeHandlers = false;
    bool suppressHostSlotAutomationSync = false;
    bool suppressEditorSizeStateSave = true;
    bool suppressHistorySnapshots = false;
    juce::MemoryBlock committedHistorySnapshot;
    std::vector<juce::MemoryBlock> undoHistory;
    std::vector<juce::MemoryBlock> redoHistory;
    std::atomic<bool> pendingHistorySnapshot { false };
    std::atomic<uint32_t> lastHistoryChangeTimeMs { 0 };
    juce::Slider* focusedParameterTargetSlider = nullptr;

    struct HostSlotAssignment
    {
        juce::String parameterId;
        juce::String parameterName;
    };
    std::array<HostSlotAssignment, VxAudioProcessor::hostAutomationSlotCount> hostSlotAssignments;

    int getBellIndexForOrderPosition(int orderIndex) const noexcept;
    int getBellOrderPositionForIndex(int bellIndex) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VxAudioProcessorEditor)
};
