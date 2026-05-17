#pragma once

#include "Processor.h"

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

class BoxTextButton;
class ChoiceControl;
class LocalChoiceControl;
class LocalParameterControl;
class ParameterControl;

class EqeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer,
                                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit EqeAudioProcessorEditor(EqeAudioProcessor&);
    ~EqeAudioProcessorEditor() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class EqeLookAndFeel;
    struct PresetsSection;
    struct BellSection;

    void openGlobalSection();
    void openFiltersSection();
    void openPresetsSection();
    void showModulePicker();
    void loadEqeModule();
#if ! JUCE_IOS
    void openVisualizerSection();
#endif
    void openBellSection(int bellIndex);
    void selectBellSection(int bellIndex);
    void refreshFilterPresetList(const juce::String& preferredSelection = {});
    void reloadFilterPresetFromProcessor();
    void addFilterPreset();
    void saveFilterPreset();
    bool renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName);
    void setDefaultFilterPreset();
    void deleteSelectedFilterPreset();

public:
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
    void moveBellSectionStoredValues(int sourceIndex, int destinationIndex);
    void removeBellSectionStoredValues(int removedIndex, int previousCount);
    void updateSectionStates();
#if ! JUCE_IOS
    void updateEditorWidthForVisualizerVisibility();
    void refreshVisualizerResponse();
#endif
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void registerParameterListeners();
    void unregisterParameterListeners();
    void scheduleHistorySnapshot();
    void commitPendingHistorySnapshot(bool force = false);
    void applyHistorySnapshot(const juce::MemoryBlock& snapshot);
    void updateUndoRedoButtons();
    int getGlobalContentHeight() const;
    int getFilterContentHeight() const;
    int getActiveBellCount() const noexcept;
#if ! JUCE_IOS
    int getCurrentMainPanelWidth() const noexcept;
#endif

    EqeAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;
    std::unique_ptr<EqeLookAndFeel> lookAndFeel;
    std::unique_ptr<BoxTextButton> moduleAddButton;
    std::unique_ptr<BoxTextButton> globalHeader;
    std::unique_ptr<BoxTextButton> filtersHeader;
#if ! JUCE_IOS
    std::unique_ptr<BoxTextButton> visualizerHeader;
#endif
    std::unique_ptr<BoxTextButton> addFilterButton;
    std::unique_ptr<PresetsSection> presetsSection;
    std::unique_ptr<ParameterControl> gainLControl;
    std::unique_ptr<ParameterControl> gainRControl;
    std::unique_ptr<ParameterControl> wideControl;
    std::unique_ptr<ParameterControl> outGainControl;
    std::unique_ptr<LocalParameterControl> clipControl;
#if ! JUCE_IOS
    std::unique_ptr<LocalParameterControl> visualizerRangeLowControl;
    std::unique_ptr<LocalParameterControl> visualizerRangeHighControl;
    std::unique_ptr<BoxTextButton> visualizerCursorButton;
    std::unique_ptr<BoxTextButton> visualizerShowStereoButton;
    std::unique_ptr<BoxTextButton> visualizerShowLeftButton;
    std::unique_ptr<BoxTextButton> visualizerShowRightButton;
    std::unique_ptr<BoxTextButton> visualizerShowMidButton;
    std::unique_ptr<BoxTextButton> visualizerShowSideButton;
#endif
    std::unique_ptr<BoxTextButton> globalBypassButton;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment;
    std::unique_ptr<BoxTextButton> globalBypassOutGainOnlyButton;
    std::unique_ptr<ButtonAttachment> globalBypassOutGainOnlyAttachment;
    std::unique_ptr<BoxTextButton> clearFiltersButton;
    std::unique_ptr<BoxTextButton> undoButton;
    std::unique_ptr<BoxTextButton> redoButton;
    std::unique_ptr<BoxTextButton> sortPlaceButton;
    std::unique_ptr<BoxTextButton> sortFreqButton;
    std::unique_ptr<BoxTextButton> sortDuoButton;
#if ! JUCE_IOS
    std::unique_ptr<BoxTextButton> visualizerVisibilityButton;
#endif
    std::array<std::unique_ptr<BellSection>, EqeAudioProcessor::maxBellFilterCount> bellSections;
    juce::Viewport globalViewport;
    juce::Component globalContent;
    juce::Viewport filterViewport;
    juce::Component filterContent;
    std::unique_ptr<BoxTextButton> footerTab;
    std::unique_ptr<juce::Component> globalSectionFrame;
    std::unique_ptr<juce::Component> filtersSectionFrame;
    std::unique_ptr<juce::Component> presetsSectionFrame;
#if ! JUCE_IOS
    std::unique_ptr<juce::Component> visualizerSectionFrame;
    std::unique_ptr<juce::Component> visualizerComponent;
#endif
    std::unique_ptr<juce::Component> textPromptOverlay;
    bool eqeModuleLoaded = false;
    bool globalExpanded = false;
    bool filtersExpanded = true;
    bool presetsExpanded = false;
#if ! JUCE_IOS
    bool visualizerExpanded = false;
    bool visualizerVisible = false;
    bool visualizerCursorEnabled = true;
    bool visualizerShowStereo = true;
    bool visualizerShowLeft = true;
    bool visualizerShowRight = true;
    bool visualizerShowMid = true;
    bool visualizerShowSide = true;
#endif
    int expandedBellIndex = -1;
#if ! JUCE_IOS
    int lastCollapsedEditorWidth = 0;
    int lastVisualizerWidth = 0;
    float visualizerRangeLowDb = -24.0f;
    float visualizerRangeHighDb = 24.0f;
#endif
    std::vector<int> bellDisplayOrder;
    bool suppressBellSectionValueChangeHandlers = false;
#if ! JUCE_IOS
    bool suppressVisualizerControlChangeHandlers = false;
#endif
    bool suppressEditorSizeStateSave = true;
    bool suppressHistorySnapshots = false;
    juce::MemoryBlock committedHistorySnapshot;
    std::vector<juce::MemoryBlock> undoHistory;
    std::vector<juce::MemoryBlock> redoHistory;
    std::atomic<bool> pendingHistorySnapshot { false };
    std::atomic<uint32_t> lastHistoryChangeTimeMs { 0 };

    int getBellIndexForOrderPosition(int orderIndex) const noexcept;
    int getBellOrderPositionForIndex(int bellIndex) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqeAudioProcessorEditor)
};
