#include "shell.EditorBellSection.h"
#include "shell.EditorPresetSections.h"
#include "shell.SetupSupport.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

namespace
{
bool applyWheelToSliderValue(juce::Slider& slider, const juce::MouseWheelDetails& wheel)
{
    const auto dominantDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY
                                                                                : -wheel.deltaX;

    if (std::abs(dominantDelta) < 1.0e-6f)
        return false;

    const auto directionalDelta = wheel.isReversed ? -dominantDelta : dominantDelta;
    const auto direction = directionalDelta < 0.0f ? -1.0 : 1.0;
    const auto minimum = static_cast<double>(slider.getMinimum());
    const auto maximum = static_cast<double>(slider.getMaximum());
    const auto currentValue = static_cast<double>(slider.getValue());

    if (minimum > 0.0 && maximum / minimum >= 100.0)
    {
        const auto smoothOctaves = static_cast<double>(directionalDelta) * 0.75;
        const auto minimumSmoothOctaves = 1.0 / 96.0;
        const auto octaveDelta = wheel.isSmooth
            ? direction * juce::jmax(std::abs(smoothOctaves), minimumSmoothOctaves)
            : direction / 12.0;
        const auto clampedOctaveDelta = juce::jlimit(-0.25, 0.25, octaveDelta);
        const auto nextValue = currentValue * std::pow(2.0, clampedOctaveDelta);

        slider.setValue(juce::jlimit(minimum, maximum, nextValue), juce::sendNotificationSync);
        return true;
    }

    const auto& range = slider.getNormalisableRange();
    const auto currentNormalised = static_cast<double>(range.convertTo0to1(currentValue));
    const auto smoothStep = static_cast<double>(directionalDelta) * 0.025;
    const auto minimumSmoothStep = 0.0025;
    const auto normalisedStep = wheel.isSmooth
        ? direction * juce::jmax(std::abs(smoothStep), minimumSmoothStep)
        : direction * 0.025;
    const auto nextNormalised = juce::jlimit(0.0, 1.0, currentNormalised + normalisedStep);

    slider.setValue(range.convertFrom0to1(nextNormalised), juce::sendNotificationSync);
    return true;
}

class FocusedParameterSlider final : public juce::Slider
{
public:
    FocusedParameterSlider()
        : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
    {
    }

    void paint(juce::Graphics& graphics) override
    {
        const auto bounds = getLocalBounds();
        const auto normalisedValue = static_cast<float>(juce::jlimit(0.0, 1.0, getValue()));
        const auto fillWidth = juce::roundToInt(static_cast<float>(bounds.getWidth()) * normalisedValue);

        graphics.setColour(findColour(juce::Slider::backgroundColourId));
        graphics.fillRect(bounds);

        if (isEnabled() && fillWidth > 0)
        {
            graphics.setColour(findColour(juce::Slider::trackColourId));
            graphics.fillRect(bounds.withWidth(fillWidth));
        }

        graphics.setColour(uiGrey500);
        graphics.drawRect(bounds, 1);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        dragStartValue = getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        const auto width = juce::jmax(1, getWidth());
        const auto horizontalDelta = static_cast<double>(event.getDistanceFromDragStartX());
        const auto verticalDelta = -static_cast<double>(event.getDistanceFromDragStartY());
        const auto dominantDelta = std::abs(horizontalDelta) >= std::abs(verticalDelta) ? horizontalDelta
                                                                                        : verticalDelta;
        const auto delta = dominantDelta / static_cast<double>(width);
        setValue(juce::jlimit(0.0, 1.0, dragStartValue + delta), juce::sendNotificationSync);
    }

#if ! JUCE_IOS
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        juce::ignoreUnused(event);

        if (onWheel != nullptr && onWheel(wheel))
            return;

        applyWheelToSliderValue(*this, wheel);
    }
#endif

    std::function<bool(const juce::MouseWheelDetails&)> onWheel;

private:
    double dragStartValue = 0.0;
};
}

VxAudioProcessorEditor::VxAudioProcessorEditor(VxAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      audioProcessor(processorToEdit),
      valueTreeState(processorToEdit.getValueTreeState()),
      lookAndFeel(std::make_unique<VxLookAndFeel>())
{
    shell_parameter_focus::clearFocus();

    setLookAndFeel(lookAndFeel.get());
    setOpaque(true);
    setResizable(true, true);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(false);
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

    auto focusedSlider = std::make_unique<FocusedParameterSlider>();
    focusedSlider->onWheel = [this] (const juce::MouseWheelDetails& wheel)
    {
        if (focusedParameterTargetSlider == nullptr)
            return false;

        if (! applyWheelToSliderValue(*focusedParameterTargetSlider, wheel))
            return false;

        const juce::ScopedValueSetter<bool> scopedIgnore(suppressFocusedParameterControlChangeHandlers, true);
        focusedParameterControl->setValue(getFocusedParameterControlValueForTarget(), juce::dontSendNotification);
        return true;
    };
    focusedParameterControl = std::move(focusedSlider);
    focusedParameterControl->setSliderStyle(juce::Slider::LinearHorizontal);
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
    focusedParameterControl->onValueChange = [this]
    {
        if (suppressFocusedParameterControlChangeHandlers || focusedParameterControl == nullptr || focusedParameterTargetSlider == nullptr)
            return;

        focusedParameterTargetSlider->setValue(getFocusedParameterTargetValueForControl(),
                                               juce::sendNotificationSync);
    };
    addAndMakeVisible(*focusedParameterControl);

    shellGlobalHeader = std::make_unique<BoxTextButton>(uiClip);
    shellGlobalHeader->setButtonText("CLIP");
    shellGlobalHeader->setTextJustification(juce::Justification::centred);
    shellGlobalHeader->setClickingTogglesState(false);
    shellGlobalHeader->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*shellGlobalHeader);

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

    if (auto* initialEqeProcessor = audioProcessor.getEqeModuleProcessor(0))
        setupEqeControls(initialEqeProcessor->getValueTreeState());

    footerTab = std::make_unique<BoxTextButton>(uiGrey500);
    footerTab->setButtonText("VX by MIXOLVE");
    footerTab->onClick = [this]
    {
        showInfoPrompt(shell_setup_support::getMixolveInfoMarkdown());
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*footerTab);

    startTimerHz(60);
    registerParameterListeners();
    rebindActiveModuleEditors();

    rebuildModuleTabRows();
    updateSectionStates();
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);

    const auto restoredEditorSize = getRestoredEditorSize();
    lastCollapsedEditorWidth = juce::jlimit(minimumEditorWidth,
                                            juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                                            lastCollapsedEditorWidth);
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
    setSize(restoredEditorSize.x, restoredEditorSize.y);
    audioProcessor.setLastEditorSize(restoredEditorSize.x, restoredEditorSize.y);

    suppressEditorSizeStateSave = false;
    syncFocusedParameterControl();
    refreshVisualizerResponse();

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
}
