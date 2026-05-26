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

    void openGlobalSection();
    void openShellGlobalSection();
    void openShellGlobalMiscSection();
    void openFiltersSection();
    void openSpeMainSection();
    void openPresetsSection();
    void showModulePicker();
    void loadEqeModule();
    void loadSpeModule();
    void loadMxeModule();
    void loadTseModule();
    void openModuleSlot(int slotIndex);
    void moveModuleSlot(int slotIndex, int direction);
    void toggleEqeModule();
    void openVisualizerSection();
    void openBellSection(int bellIndex);
    void selectBellSection(int bellIndex);
    void refreshFilterPresetList(const juce::String& preferredSelection = {});
    void reloadFilterPresetFromProcessor();
    void addFilterPreset();
    void saveFilterPreset();
    bool renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName);
    void setDefaultFilterPreset();
    void deleteSelectedFilterPreset();
    void closeActiveModule();

public:
    void requestCloseActiveModule() { closeActiveModule(); }
    void showTextPrompt(const juce::String& currentText,
                        std::function<bool(const juce::String&)> onCommit,
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
    juce::Rectangle<int> getGlobalHeaderBounds() const noexcept;
    juce::Rectangle<int> getMainPanelBounds() const noexcept;

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
    void selectAdjacentBellSection(int direction);
    juce::Rectangle<int> getVisibleBellSectionBounds(int bellIndex) const;
    void resetBellSectionStoredValues(int bellIndex);
    void removeBellSectionStoredValues(int removedIndex, int previousCount);
    void updateSectionStates();
    void updateEditorWidthForVisualizerVisibility();
    void refreshVisualizerResponse();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                  const juce::Identifier& property) override;
    void registerParameterListeners();
    void unregisterParameterListeners();
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
    void setupEqeControls(juce::AudioProcessorValueTreeState& initialEqeState);
    EqeModuleProcessor* getActiveEqeProcessor() noexcept;
    const EqeModuleProcessor* getActiveEqeProcessor() const noexcept;
    void scheduleHistorySnapshot();
    void commitPendingHistorySnapshot(bool force = false);
    void applyHistorySnapshot(const juce::MemoryBlock& snapshot);
    void updateUndoRedoButtons();
    int getShellGlobalContentHeight() const;
    int getGlobalContentHeight() const;
    int getActiveGlobalContentHeight() const;
    int getActiveFilterContentHeight() const;
    int getFilterContentHeight() const;
    int getSpeMiscContentHeight() const;
    int getSpeMainContentHeight() const;
    int getSpeAnalyserContentHeight() const;
    int getSpeSectionContentHeight() const;
    int getActiveBellCount() const noexcept;
    bool shouldShowDetachedVisualizerPanel() const noexcept;
    int getCurrentMainPanelWidth() const noexcept;
    void updateVisualizerPanelBounds(juce::Rectangle<int>& bounds);
    void layoutShellGlobalSection(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutFooter(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutModuleTabRows(juce::Rectangle<int>& bounds, int editorInsetX);
    void finalizeLayout() noexcept;
    void layoutNoModuleState(juce::Rectangle<int>& bounds);
    void layoutCollapsedModuleState();
    void layoutModuleEditorContent(juce::Rectangle<int>& bounds);
    void layoutSpeModuleSections(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutEqeModuleSections(juce::Rectangle<int>& bounds, int editorInsetX);
    void rebuildModuleTabRows();
    void includeBounds(juce::Rectangle<int>& sectionBounds, const juce::Rectangle<int>& boundsToInclude) const;
    void includeComponentBounds(juce::Rectangle<int>& sectionBounds, const juce::Component* component) const;
    void placeSectionFrame(juce::Component* frame,
                           bool shouldShow,
                           juce::Rectangle<int> sectionBounds) const;
    void placeModuleFrame(juce::Component* frame,
                          bool shouldShow,
                          juce::Rectangle<int> sectionBounds) const;
    juce::Rectangle<int> buildShellGlobalFrameBounds() const;
    void includeModuleTabRowBounds(juce::Rectangle<int>& sectionBounds) const;

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
    std::unique_ptr<BoxTextButton> shellGlobalMiscHeader;
    std::unique_ptr<BoxTextButton> moduleAddButton;
    struct ModuleTabRow
    {
        std::unique_ptr<BoxTextButton> moveUpButton;
        std::unique_ptr<BoxTextButton> tabButton;
        std::unique_ptr<BoxTextButton> moveDownButton;
        int slotIndex = -1;
    };
    std::vector<std::unique_ptr<ModuleTabRow>> moduleTabRows;
    std::unique_ptr<BoxTextButton> globalHeader;
    std::unique_ptr<BoxTextButton> speMainHeader;
    std::unique_ptr<BoxTextButton> filtersHeader;
    std::unique_ptr<BoxTextButton> visualizerHeader;
    std::unique_ptr<BoxTextButton> addFilterButton;
    std::unique_ptr<PresetsSection> presetsSection;
    std::unique_ptr<ParameterControl> gainLControl;
    std::unique_ptr<ParameterControl> gainRControl;
    std::unique_ptr<ParameterControl> globalInGainLrControl;
    std::unique_ptr<ParameterControl> wideControl;
    std::unique_ptr<ParameterControl> outGainControl;
    std::unique_ptr<LocalParameterControl> clipControl;
    std::unique_ptr<ParameterControl> speInputGainControl;
    std::unique_ptr<ParameterControl> speInputGainLControl;
    std::unique_ptr<ParameterControl> speInputGainRControl;
    std::unique_ptr<ParameterControl> speWideControl;
    std::unique_ptr<ParameterControl> speAttackControl;
    std::unique_ptr<ParameterControl> speReleaseControl;
    std::unique_ptr<ParameterControl> speKneeControl;
    std::unique_ptr<ParameterControl> speRatioControl;
    std::unique_ptr<ParameterControl> speDspFftSizeControl;
    std::unique_ptr<ParameterControl> speDspSlopeControl;
    std::unique_ptr<ParameterControl> speMakeupControl;
    std::unique_ptr<BoxTextButton> speDeltaButton;
    std::unique_ptr<ButtonAttachment> speDeltaAttachment;
    std::unique_ptr<ParameterControl> speThresholdControl;
    std::unique_ptr<ParameterControl> speStereoAdaptiveControl;
    std::unique_ptr<ParameterControl> speStereoAdaptiveOffsetControl;
    std::unique_ptr<BoxTextButton> speStereoBypassButton;
    std::unique_ptr<ButtonAttachment> speStereoBypassAttachment;
    std::unique_ptr<BoxTextButton> speBypassButton;
    std::unique_ptr<ButtonAttachment> speBypassAttachment;
    std::unique_ptr<BoxTextButton> speBypassWithGainButton;
    std::unique_ptr<ButtonAttachment> speBypassWithGainAttachment;
    std::unique_ptr<ParameterControl> speDualMonoLeftThresholdControl;
    std::unique_ptr<ParameterControl> speDualMonoLeftAdaptiveControl;
    std::unique_ptr<ParameterControl> speDualMonoLeftAdaptiveOffsetControl;
    std::unique_ptr<ParameterControl> speDualMonoRightThresholdControl;
    std::unique_ptr<ParameterControl> speDualMonoRightAdaptiveControl;
    std::unique_ptr<ParameterControl> speDualMonoRightAdaptiveOffsetControl;
    std::unique_ptr<BoxTextButton> speDualMonoBypassButton;
    std::unique_ptr<ButtonAttachment> speDualMonoBypassAttachment;
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
    std::unique_ptr<BoxTextButton> speAnalyserVisibilityButton;
    std::unique_ptr<BoxTextButton> globalBypassButton;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment;
    std::unique_ptr<BoxTextButton> globalBypassOutGainOnlyButton;
    std::unique_ptr<ButtonAttachment> globalBypassOutGainOnlyAttachment;
    std::unique_ptr<BoxTextButton> eqeBypassButton;
    std::unique_ptr<ButtonAttachment> eqeBypassAttachment;
    std::unique_ptr<BoxTextButton> eqeBypassWithGainButton;
    std::unique_ptr<ButtonAttachment> eqeBypassWithGainAttachment;
    std::unique_ptr<ParameterControl> eqeInGainLrControl;
    std::unique_ptr<ParameterControl> eqeInGainLControl;
    std::unique_ptr<ParameterControl> eqeInGainRControl;
    std::unique_ptr<ParameterControl> eqeWideControl;
    std::unique_ptr<ParameterControl> eqeOutGainControl;
    std::unique_ptr<BoxTextButton> moduleCloseButton;
    std::unique_ptr<BoxTextButton> speModuleCloseButton;
    std::unique_ptr<BoxTextButton> clearFiltersButton;
    std::unique_ptr<BoxTextButton> undoButton;
    std::unique_ptr<BoxTextButton> redoButton;
    std::unique_ptr<BoxTextButton> sortPlaceButton;
    std::unique_ptr<BoxTextButton> sortFreqButton;
    std::unique_ptr<BoxTextButton> sortDuoButton;
    std::unique_ptr<BoxTextButton> visualizerVisibilityButton;
    std::array<std::unique_ptr<BellSection>, VxAudioProcessor::maxBellFilterCount> bellSections;
    juce::Viewport globalViewport;
    juce::Component globalContent;
    juce::Viewport filterViewport;
    juce::Component filterContent;
    std::unique_ptr<BoxTextButton> footerTab;
    std::unique_ptr<juce::Component> eqeModuleFrame;
    std::unique_ptr<juce::Component> shellGlobalSectionFrame;
    std::unique_ptr<juce::Component> globalSectionFrame;
    std::unique_ptr<juce::Component> speMainSectionFrame;
    std::unique_ptr<juce::Component> filtersSectionFrame;
    std::unique_ptr<juce::Component> presetsSectionFrame;
    std::unique_ptr<juce::Component> visualizerSectionFrame;
    std::unique_ptr<juce::Component> visualizerComponent;
    std::unique_ptr<juce::Component> speAnalyserComponent;
    std::unique_ptr<juce::Component> textPromptOverlay;
    std::unique_ptr<juce::Component> mxeModuleEditor;
    std::unique_ptr<juce::Component> tseModuleEditor;
    bool eqeModuleLoaded = false;
    bool speModuleLoaded = false;
    bool mxeModuleLoaded = false;
    bool tseModuleLoaded = false;
    bool eqeModuleExpanded = true;
    bool shellGlobalExpanded = false;
    bool shellGlobalMiscExpanded = true;
    bool globalExpanded = false;
    bool speMainExpanded = false;
    bool filtersExpanded = false;
    bool presetsExpanded = false;
    bool visualizerExpanded = false;
    bool visualizerVisible = false;
    bool visualizerCursorEnabled = true;
    bool visualizerShowStereo = true;
    bool visualizerShowLeft = true;
    bool visualizerShowRight = true;
    bool visualizerShowMid = true;
    bool visualizerShowSide = true;
    int expandedBellIndex = -1;
    int lastCollapsedEditorWidth = 0;
    int lastVisualizerWidth = 0;
    float visualizerRangeLowDb = -24.0f;
    float visualizerRangeHighDb = 24.0f;
    std::vector<int> bellDisplayOrder;
    bool suppressBellSectionValueChangeHandlers = false;
    bool suppressVisualizerControlChangeHandlers = false;
    bool suppressEditorSizeStateSave = true;
    bool suppressHistorySnapshots = false;
    juce::MemoryBlock committedHistorySnapshot;
    std::vector<juce::MemoryBlock> undoHistory;
    std::vector<juce::MemoryBlock> redoHistory;
    std::atomic<bool> pendingHistorySnapshot { false };
    std::atomic<uint32_t> lastHistoryChangeTimeMs { 0 };

    int getBellIndexForOrderPosition(int orderIndex) const noexcept;
    int getBellOrderPositionForIndex(int bellIndex) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VxAudioProcessorEditor)
};
