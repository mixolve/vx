#include "shell.EditorBellSection.h"
#include "shell.EditorPresetSections.h"
#include "shell.SetupSupport.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

VxAudioProcessorEditor::VxAudioProcessorEditor(VxAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      audioProcessor(processorToEdit),
      valueTreeState(processorToEdit.getValueTreeState()),
      lookAndFeel(std::make_unique<VxLookAndFeel>())
{
    setLookAndFeel(lookAndFeel.get());
    setOpaque(true);
    setResizable(true, true);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(false);
    globalViewport.setViewedComponent(&globalContent, false);
    globalViewport.setScrollBarsShown(false, false);
    globalViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    globalViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(globalViewport);
    filterViewport.setViewedComponent(&filterContent, false);
    filterViewport.setScrollBarsShown(false, false);
    filterViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    filterViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(filterViewport);

    shellGlobalHeader = std::make_unique<BoxTextButton>(uiClip);
    shellGlobalHeader->setButtonText("GLOBAL");
    shellGlobalHeader->setTextJustification(juce::Justification::centred);
    shellGlobalHeader->setLeadingDot(uiClip, audioProcessor.getGlobalClipIndicator());
    shellGlobalHeader->setClickingTogglesState(true);
    shellGlobalHeader->onClick = [this]
    {
        openShellGlobalSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*shellGlobalHeader);

    moduleAddButton = std::make_unique<BoxTextButton>(uiClip);
    moduleAddButton->setButtonText("ADD-MODULE");
    moduleAddButton->setTextJustification(juce::Justification::centred);
    moduleAddButton->onClick = [this]
    {
        showModulePicker();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*moduleAddButton);

    globalHeader = std::make_unique<BoxTextButton>(uiAccent);
    globalHeader->setButtonText("MISC");
    globalHeader->setClickingTogglesState(true);
    globalHeader->onClick = [this]
    {
        openGlobalSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*globalHeader);

    speMainHeader = std::make_unique<BoxTextButton>(uiAccent);
    speMainHeader->setButtonText("MAIN");
    speMainHeader->setClickingTogglesState(true);
    speMainHeader->onClick = [this]
    {
        openSpeMainSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*speMainHeader);

    if (auto* speProcessor = audioProcessor.getSpeModuleProcessor())
    {
        auto& speState = speProcessor->getValueTreeState();

        setupSpeControls(speState);
        speAnalyserComponent = shell_setup_support::createSpeAnalyserComponent(*speProcessor);
        addAndMakeVisible(*speAnalyserComponent);
    }

    setupShellControls();
    setupVisualizerControls();

    visualizerComponent = shell_setup_support::createEqResponseVisualizerComponent(audioProcessor,
                                                                                   [this] (const int bellIndex)
                                                                                   {
                                                                                       selectBellSection(bellIndex);
                                                                                       clearKeyboardFocus(*this);
                                                                                   });
    addAndMakeVisible(*visualizerComponent);

    setupPresetControls();

    bellDisplayOrder.reserve(VxAudioProcessor::maxBellFilterCount);
    for (int bellIndex = 0; bellIndex < VxAudioProcessor::maxBellFilterCount; ++bellIndex)
        bellDisplayOrder.push_back(bellIndex);

    restoreEditorStateFromValueTree();

    auto* initialEqeProcessor = audioProcessor.getEqeModuleProcessor(0);
    auto& initialEqeState = initialEqeProcessor != nullptr ? initialEqeProcessor->getValueTreeState()
                                                           : valueTreeState;

    setupEqeControls(initialEqeState);

    footerTab = std::make_unique<BoxTextButton>(uiGrey500);
    footerTab->setButtonText("VX by MIXOLVE");
    footerTab->onClick = [this]
    {
        showInfoPrompt(shell_setup_support::getMixolveInfoMarkdown());
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*footerTab);

    eqeModuleFrame = shell_setup_support::createSectionFrameComponent(uiClip);
    globalSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    speMainSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    filtersSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    presetsSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    visualizerSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    addAndMakeVisible(*eqeModuleFrame);
    addAndMakeVisible(*globalSectionFrame);
    addAndMakeVisible(*speMainSectionFrame);
    addAndMakeVisible(*filtersSectionFrame);
    addAndMakeVisible(*presetsSectionFrame);
    addAndMakeVisible(*visualizerSectionFrame);
    eqeModuleFrame->setVisible(false);

    startTimerHz(30);
    registerParameterListeners();
    rebindActiveModuleEditors();

    rebuildModuleTabRows();
    updateSectionStates();
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);

    const auto restoredEditorSize = getRestoredEditorSize();
    lastCollapsedEditorWidth = juce::jlimit(minimumEditorWidth,
                                            juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                                            lastCollapsedEditorWidth);
    lastVisualizerWidth = juce::jmax(minimumVisualizerWidth, lastVisualizerWidth);

    if (visualizerVisible)
    {
        const auto minimumVisibleWidth = lastCollapsedEditorWidth + visualizerPanelGap + minimumVisualizerWidth;
        const auto preferredVisibleWidth = lastCollapsedEditorWidth + visualizerPanelGap + lastVisualizerWidth;
        const auto width = juce::jmax(minimumVisibleWidth,
                                      juce::jmax(restoredEditorSize.x, preferredVisibleWidth));
        setResizeLimits(minimumVisibleWidth,
                        minimumEditorHeight,
                        maximumEditorWidth,
                        maximumEditorHeight);
        setSize(width, restoredEditorSize.y);
        audioProcessor.setLastEditorSize(width, restoredEditorSize.y);
    }
    else
    {
        setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
        setSize(restoredEditorSize.x, restoredEditorSize.y);
        audioProcessor.setLastEditorSize(restoredEditorSize.x, restoredEditorSize.y);
    }

    suppressEditorSizeStateSave = false;
    refreshVisualizerResponse();

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
}
