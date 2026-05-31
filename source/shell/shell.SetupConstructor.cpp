#include "shell.EditorBellSection.h"
#include "shell.EditorPresetSections.h"
#include "shell.SetupSupport.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/mxe/module.mxe.EditorControls.h"

namespace
{
class FocusedParameterSlider final : public juce::Slider
{
public:
    FocusedParameterSlider()
        : juce::Slider(juce::Slider::LinearBarVertical, juce::Slider::NoTextBox)
    {
    }

#if JUCE_IOS
    void mouseDown(const juce::MouseEvent&) override
    {
        touchDragStartValue = getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        const auto height = juce::jmax(1, getHeight());
        const auto delta = -static_cast<double>(event.getDistanceFromDragStartY()) / static_cast<double>(height);
        setValue(juce::jlimit(0.0, 1.0, touchDragStartValue + delta), juce::sendNotificationSync);
    }
#else
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        auto scaledWheel = wheel;
        const auto scale = focusedParameterScrollSensitivity / 220.0f;
        scaledWheel.deltaX *= scale;
        scaledWheel.deltaY *= scale;
        juce::Slider::mouseWheelMove(event, scaledWheel);
    }
#endif

private:
#if JUCE_IOS
    double touchDragStartValue = 0.0;
#endif
};
}

VxAudioProcessorEditor::VxAudioProcessorEditor(VxAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      audioProcessor(processorToEdit),
      valueTreeState(processorToEdit.getValueTreeState()),
      lookAndFeel(std::make_unique<VxLookAndFeel>())
{
    shell_parameter_focus::clearFocus();
    mxe::editor::parameter_focus::clearFocus();

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
    shellGlobalHostViewport.setViewedComponent(&shellGlobalHostContent, false);
    shellGlobalHostViewport.setScrollBarsShown(false, true);
    shellGlobalHostViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    shellGlobalHostViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(shellGlobalHostViewport);
    filterViewport.setViewedComponent(&filterContent, false);
    filterViewport.setScrollBarsShown(false, false);
    filterViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    filterViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(filterViewport);

    focusedParameterControl = std::make_unique<FocusedParameterSlider>();
    focusedParameterControl->setSliderStyle(juce::Slider::LinearBarVertical);
    focusedParameterControl->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    focusedParameterControl->setSliderSnapsToMousePosition(false);
#if JUCE_IOS
    focusedParameterControl->setVelocityBasedMode(false);
#else
    focusedParameterControl->setVelocityBasedMode(true);
    focusedParameterControl->setMouseDragSensitivity(focusedParameterDragSensitivity);
    focusedParameterControl->setVelocityModeParameters(static_cast<double>(juce::jmax(1, focusedParameterDragSensitivity)),
                                                       1,
                                                       0.0,
                                                       false);
#endif
    focusedParameterControl->setScrollWheelEnabled(true);
    focusedParameterControl->setColour(juce::Slider::backgroundColourId, uiGrey800);
    focusedParameterControl->setColour(juce::Slider::trackColourId, uiGrey500);
    focusedParameterControl->setColour(juce::Slider::thumbColourId, uiGrey500);
    focusedParameterControl->setColour(juce::Slider::rotarySliderFillColourId, uiGrey500);
    focusedParameterControl->setColour(juce::Slider::rotarySliderOutlineColourId, uiGrey500);
    focusedParameterControl->setRange(0.0, 1.0, 0.0);
    focusedParameterControl->setValue(0.5, juce::dontSendNotification);
    focusedParameterControl->setEnabled(false);
    focusedParameterControl->setAlpha(1.0f);
    focusedParameterControlPinnedWidth = rowHeight;
    focusedParameterControl->onValueChange = [this]
    {
        if (suppressFocusedParameterControlChangeHandlers || focusedParameterControl == nullptr || focusedParameterTargetSlider == nullptr)
            return;

        const auto& targetRange = focusedParameterTargetSlider->getNormalisableRange();
        const auto normalizedValue = static_cast<float>(focusedParameterControl->getValue());
        focusedParameterTargetSlider->setValue(targetRange.convertFrom0to1(normalizedValue),
                                               juce::sendNotificationSync);
    };
    addAndMakeVisible(*focusedParameterControl);

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

    shellGlobalMiscHeader = std::make_unique<BoxTextButton>(uiAccent);
    shellGlobalMiscHeader->setButtonText("MISC");
    shellGlobalMiscHeader->setTextJustification(juce::Justification::centred);
    shellGlobalMiscHeader->setClickingTogglesState(true);
    shellGlobalMiscHeader->onClick = [this]
    {
        openShellGlobalMiscSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*shellGlobalMiscHeader);

    shellGlobalHostHeader = std::make_unique<BoxTextButton>(uiAccent);
    shellGlobalHostHeader->setButtonText("HOST");
    shellGlobalHostHeader->setTextJustification(juce::Justification::centred);
    shellGlobalHostHeader->setClickingTogglesState(true);
    shellGlobalHostHeader->onClick = [this]
    {
        openShellGlobalHostSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*shellGlobalHostHeader);

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

        setupSpeControls(speState, *speProcessor);
        speAnalyserComponent = shell_setup_support::createSpeAnalyserComponent(*speProcessor);
        addAndMakeVisible(*speAnalyserComponent);
    }

    setupShellControls();
    setupVisualizerControls();

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
    shellGlobalSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    globalSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    speMainSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    filtersSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    presetsSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    visualizerSectionFrame = shell_setup_support::createSectionFrameComponent(uiAccent);
    addAndMakeVisible(*eqeModuleFrame);
    addAndMakeVisible(*shellGlobalSectionFrame);
    addAndMakeVisible(*globalSectionFrame);
    addAndMakeVisible(*speMainSectionFrame);
    addAndMakeVisible(*filtersSectionFrame);
    addAndMakeVisible(*presetsSectionFrame);
    addAndMakeVisible(*visualizerSectionFrame);
    eqeModuleFrame->setVisible(false);
    shellGlobalSectionFrame->setVisible(false);

    startTimerHz(60);
    registerParameterListeners();
    rebindActiveModuleEditors();

    rebuildModuleTabRows();
    updateSectionStates();
    setResizeLimits(minimumEditorWidthWithFocusedControl, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);

    const auto restoredEditorSize = getRestoredEditorSize();
    lastCollapsedEditorWidth = juce::jlimit(minimumEditorWidth,
                                            juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                                            lastCollapsedEditorWidth);
    setResizeLimits(minimumEditorWidthWithFocusedControl, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
    setSize(restoredEditorSize.x, restoredEditorSize.y);
    audioProcessor.setLastEditorSize(restoredEditorSize.x, restoredEditorSize.y);

    suppressEditorSizeStateSave = false;
    syncFocusedParameterControl();
    refreshVisualizerResponse();

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
}
