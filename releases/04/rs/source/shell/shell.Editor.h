#pragma once

#include "shell.Processor.h"

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

class BoxTextButton;
class ChoiceControl;
class DelayedTooltipWindow;
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
    static constexpr int speFilterControlCount = 16;

    class VxLookAndFeel;
    struct PresetsSection;
    struct FilterSection;

    void toggleHostParametersSection();
    void updateTooltipTogglePrompt();
    void showModulePicker();
    void closeActiveModule();
    void loadEqeModule();
    void loadSpeModule();
    void loadMieModule();
    void loadMxeModule();
    void loadTseModule();
    void selectFilterSection(int filterIndex);
    void refreshFilterPresetList(const juce::String& preferredSelection = {});
    void reloadFilterPresetFromProcessor();
    void addFilterPreset();
    void saveFilterPreset();
    bool renameFilterPreset(const juce::String& sourcePresetName, const juce::String& newPresetName);
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
                          std::function<void()> onDismiss = {},
                          juce::StringArray itemTooltips = {});
    juce::Rectangle<int> getInfoPromptAnchorBounds() const noexcept;
    juce::Rectangle<int> getInfoPromptVisibleBounds() const noexcept;
private:
    void dismissTextPrompt();
    void timerCallback() override;
    void normalizeSlopeForType(int filterIndex);
    void sortFilterSectionsByPlace();
    void sortFilterSectionsByFrequency();
    void sortFilterSectionsByDuo();
    void clearAllFilters();
    void performUndo();
    void performRedo();
    void applyFilterSortOrder(const std::vector<int>& orderedIndices);
    void moveFilterSection(int sourceIndex, int destinationIndex);
    void enforceSingleExpandedFilterSection(int preferredFilterIndex = -1);
    void restoreEditorStateFromValueTree();
    void storeEditorStateToValueTree() noexcept;
    juce::Point<int> getRestoredEditorSize() const noexcept;
    juce::Rectangle<int> getFilterSectionBounds(int filterIndex) const;
    void resetFilterSectionStoredValues(int filterIndex);
    void removeFilterSectionStoredValues(int removedIndex, int previousCount);
    void updateSectionStates();
    void syncEditorWidthToBounds();
    void refreshSpeAnalyserResponse();
    void syncFocusedParameterControl();
    double getFocusedParameterControlValueForTarget() const noexcept;
    double getFocusedParameterTargetValueForControl() const noexcept;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                  const juce::Identifier& property) override;
    void valueTreeRedirected(juce::ValueTree& treeWhichHasBeenChanged) override;
    void resyncEditorFromProcessorState();
    void registerParameterListeners();
    void unregisterParameterListeners();
    juce::RangedAudioParameter* findHostAssignableParameter(const juce::String& parameterId) const noexcept;
    void syncHostSlotAssignmentValue(int slotIndex, float normalizedValue);
    void registerObservedModuleParameterListeners(juce::AudioProcessorValueTreeState& moduleValueTreeState);
    void refreshModuleStateListeners();
    void clearModuleStateListeners();
    int getActiveSpePhaseFilterCount() const noexcept;
    bool shouldEnableSpePhaseOrder(int filterIndex) const noexcept;
    bool shouldEnableSpePhaseFrequency(int filterIndex) const noexcept;
    bool shouldEnableSpePhaseBandwidth(int filterIndex) const noexcept;
    bool shouldShowSpePhaseImpact(int filterIndex) const noexcept;
    void enforceSingleExpandedSpePhaseFilter(int preferredFilterIndex = -1);
    juce::String getSpePhaseFilterHeaderText(int filterIndex) const;
    int getActiveSpeAmplitudeFilterCount() const noexcept;
    bool shouldEnableSpeAmplitudeOrder(int filterIndex) const noexcept;
    bool shouldEnableSpeAmplitudeFrequency(int filterIndex) const noexcept;
    bool shouldEnableSpeAmplitudeBandwidth(int filterIndex) const noexcept;
    bool shouldShowSpeAmplitudeImpact(int filterIndex) const noexcept;
    void enforceSingleExpandedSpeAmplitudeFilter(int preferredFilterIndex = -1);
    juce::String getSpeAmplitudeFilterHeaderText(int filterIndex) const;
    void detachModuleEditorBindings();
    void rebindActiveModuleEditors();
    void setupShellControls();
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
    int getActiveFilterCount() const noexcept;
    void resetAnalyserPanelBounds();
    void layoutGlobalControlsSection(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutFooter(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutModuleTabButton(juce::Rectangle<int>& bounds, int editorInsetX);
    void finalizeLayout() noexcept;
    void layoutNoModuleState(juce::Rectangle<int>& bounds);
    void layoutModuleEditorContent(juce::Rectangle<int>& bounds);
    void layoutSpeModuleSections(juce::Rectangle<int>& bounds, int editorInsetX);
    void layoutEqeModuleSections(juce::Rectangle<int>& bounds, int editorInsetX);
    void refreshModuleTabButton();
    void updateTooltipBoundsConstraint() noexcept;
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
    std::unique_ptr<DelayedTooltipWindow> tooltipWindow;
    std::unique_ptr<BoxTextButton> clipButton;
    std::unique_ptr<BoxTextButton> hostButton;
    std::unique_ptr<BoxTextButton> moduleAddButton;
    std::unique_ptr<BoxTextButton> moduleTabButton;
    std::unique_ptr<BoxTextButton> addFilterButton;
    std::unique_ptr<PresetsSection> presetsSection;
    std::unique_ptr<BoxTextButton> speFftProcessorHeader;
    std::unique_ptr<BoxTextButton> speDynamicProcessorHeader;
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
    std::unique_ptr<BoxTextButton> spePhaseProcessorHeader;
    std::unique_ptr<BoxTextButton> speAmplitudeProcessorHeader;
    std::unique_ptr<ParameterControl> speDspHopDivisorControl;
    std::unique_ptr<BoxTextButton> spePhaseAddButton;
    std::array<std::unique_ptr<BoxTextButton>, speFilterControlCount> spePhaseBypassButtons;
    std::array<std::unique_ptr<ButtonAttachment>, speFilterControlCount> spePhaseBypassAttachments;
    std::array<std::unique_ptr<BoxTextButton>, speFilterControlCount> spePhaseHeaderButtons;
    std::array<std::unique_ptr<ChoiceControl>, speFilterControlCount> spePhaseTypeControls;
    std::array<std::unique_ptr<ChoiceControl>, speFilterControlCount> spePhasePlaceControls;
    std::array<std::unique_ptr<ChoiceControl>, speFilterControlCount> spePhaseSlopeControls;
    std::array<std::unique_ptr<ParameterControl>, speFilterControlCount> spePhaseFrequencyControls;
    std::array<std::unique_ptr<ParameterControl>, speFilterControlCount> spePhaseBandwidthControls;
    std::array<std::unique_ptr<ParameterControl>, speFilterControlCount> spePhaseImpactControls;
    std::array<bool, speFilterControlCount> spePhaseExpanded {};
    std::unique_ptr<BoxTextButton> speAmplitudeAddButton;
    std::array<std::unique_ptr<BoxTextButton>, speFilterControlCount> speAmplitudeBypassButtons;
    std::array<std::unique_ptr<ButtonAttachment>, speFilterControlCount> speAmplitudeBypassAttachments;
    std::array<std::unique_ptr<BoxTextButton>, speFilterControlCount> speAmplitudeHeaderButtons;
    std::array<std::unique_ptr<ChoiceControl>, speFilterControlCount> speAmplitudeTypeControls;
    std::array<std::unique_ptr<ChoiceControl>, speFilterControlCount> speAmplitudePlaceControls;
    std::array<std::unique_ptr<ChoiceControl>, speFilterControlCount> speAmplitudeSlopeControls;
    std::array<std::unique_ptr<ParameterControl>, speFilterControlCount> speAmplitudeFrequencyControls;
    std::array<std::unique_ptr<ParameterControl>, speFilterControlCount> speAmplitudeBandwidthControls;
    std::array<std::unique_ptr<ParameterControl>, speFilterControlCount> speAmplitudeImpactControls;
    std::array<bool, speFilterControlCount> speAmplitudeExpanded {};
    std::unique_ptr<BoxTextButton> speAnalyserSettingsHeader;
    std::unique_ptr<LocalParameterControl> speAnalyserFftSizeControl;
    std::unique_ptr<LocalParameterControl> speAnalyserOverlapControl;
    std::unique_ptr<LocalParameterControl> speAnalyserLeftControl;
    std::unique_ptr<LocalParameterControl> speAnalyserRightControl;
    std::unique_ptr<LocalParameterControl> speAnalyserRangeLowControl;
    std::unique_ptr<LocalParameterControl> speAnalyserRangeHighControl;
    std::unique_ptr<LocalParameterControl> speAnalyserSlopeControl;
    std::unique_ptr<LocalParameterControl> speAnalyserTimeControl;
    std::unique_ptr<BoxTextButton> globalBypassButton;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment;
    std::unique_ptr<BoxTextButton> clearFiltersButton;
    std::unique_ptr<BoxTextButton> undoButton;
    std::unique_ptr<BoxTextButton> redoButton;
    std::unique_ptr<BoxTextButton> sortPlaceButton;
    std::unique_ptr<BoxTextButton> sortFreqButton;
    std::unique_ptr<BoxTextButton> sortDuoButton;
    std::array<std::unique_ptr<BoxTextButton>, VxAudioProcessor::hostAutomationSlotCount> hostSlotButtons;
    std::array<std::unique_ptr<FilterSection>, VxAudioProcessor::maxEqeFilterCount> filterSections;
    juce::Viewport hostParametersViewport;
    juce::Component hostParametersContent;
    juce::Viewport speAnalyserViewport;
    juce::Component speAnalyserContent;
    juce::Viewport filterViewport;
    juce::Component filterContent;
    std::unique_ptr<juce::Slider> focusedParameterControl;
    std::unique_ptr<BoxTextButton> footerTab;
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
    bool hostParametersExpanded = false;
    bool tooltipsEnabled = true;
    std::vector<int> filterDisplayOrder;
    bool suppressFilterSectionValueChangeHandlers = false;
    bool suppressSpeAnalyserControlChangeHandlers = false;
    bool suppressFocusedParameterControlChangeHandlers = false;
    bool suppressHostSlotAutomationSync = false;
    bool suppressEditorSizeStateSave = true;
    bool suppressHistorySnapshots = false;
    bool shellStateListenerRegistered = false;
    juce::MemoryBlock committedHistorySnapshot;
    std::vector<juce::MemoryBlock> undoHistory;
    std::vector<juce::MemoryBlock> redoHistory;
    std::atomic<bool> pendingHistorySnapshot { false };
    std::atomic<uint32_t> lastHistoryChangeTimeMs { 0 };
    juce::Slider* focusedParameterTargetSlider = nullptr;
    uint32_t lastClipIndicatorTimeMs = 0;

    struct HostSlotAssignment
    {
        juce::String parameterId;
        juce::String parameterName;
    };
    std::array<HostSlotAssignment, VxAudioProcessor::hostAutomationSlotCount> hostSlotAssignments;

    int getFilterIndexForOrderPosition(int orderIndex) const noexcept;
    int getFilterOrderPositionForIndex(int filterIndex) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VxAudioProcessorEditor)
};
