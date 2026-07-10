#include "shell.EditorFilterSection.h"
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

bool applyWheelToNormalisedSliderValue(juce::Slider& slider, const juce::MouseWheelDetails& wheel)
{
    const auto dominantDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY
                                                                                : -wheel.deltaX;

    if (std::abs(dominantDelta) < 1.0e-6f)
        return false;

    const auto directionalDelta = wheel.isReversed ? -dominantDelta : dominantDelta;
    const auto direction = directionalDelta < 0.0f ? -1.0 : 1.0;
    const auto minimumSmoothStep = 0.005;
    const auto normalisedStep = wheel.isSmooth
        ? direction * juce::jmax(std::abs(static_cast<double>(directionalDelta) * 0.02), minimumSmoothStep)
        : direction * 0.025;
    const auto nextValue = juce::jlimit(0.0,
                                        1.0,
                                        static_cast<double>(slider.getValue()) + normalisedStep);

    if (std::abs(nextValue - static_cast<double>(slider.getValue())) <= 1.0e-9)
        return false;

    slider.setValue(nextValue, juce::sendNotificationSync);
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
        const auto delta = (horizontalDelta + verticalDelta) / static_cast<double>(width);
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

#if ! JUCE_IOS
class EdgeResizeHandle final : public juce::Component
{
public:
    enum class Axis
    {
        horizontal,
        vertical
    };

    EdgeResizeHandle(VxAudioProcessorEditor& ownerIn, const Axis axisIn)
        : owner(ownerIn),
          axis(axisIn)
    {
        setMouseCursor(axis == Axis::horizontal ? juce::MouseCursor::LeftRightResizeCursor
                                                : juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void paint(juce::Graphics& graphics) override
    {
        auto bounds = getLocalBounds();
        graphics.setColour(uiGrey500);

        if (axis == Axis::horizontal)
            graphics.fillRect(bounds.removeFromRight(1));
        else
            graphics.fillRect(bounds.removeFromBottom(1));
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        dragStartSize = { owner.getWidth(), owner.getHeight() };
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        auto width = dragStartSize.x;
        auto height = dragStartSize.y;

        if (axis == Axis::horizontal)
            width = juce::jlimit(minimumEditorWidth,
                                 maximumEditorWidth,
                                 dragStartSize.x + event.getDistanceFromDragStartX());
        else
            height = juce::jlimit(minimumEditorHeight,
                                  maximumEditorHeight,
                                  dragStartSize.y + event.getDistanceFromDragStartY());

        owner.setSize(width, height);
    }

private:
    VxAudioProcessorEditor& owner;
    Axis axis;
    juce::Point<int> dragStartSize;
};
#endif
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
#if JUCE_IOS
    setResizable(false, false);
#else
    setResizable(true, false);
#endif
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(false);
    tooltipWindow = std::make_unique<DelayedTooltipWindow>(this, 1500);
    hostParametersViewport.setViewedComponent(&hostParametersContent, false);
    hostParametersViewport.setScrollBarsShown(false, true);
    hostParametersViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    hostParametersViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(hostParametersViewport);
    speAnalyserViewport.setViewedComponent(&speAnalyserContent, false);
    speAnalyserViewport.setScrollBarsShown(false, false);
    speAnalyserViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    speAnalyserViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(speAnalyserViewport);
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

        return focusedParameterControl != nullptr
            && applyWheelToNormalisedSliderValue(*focusedParameterControl, wheel);
    };
    focusedParameterControl = std::move(focusedSlider);
    focusedParameterControl->setSliderStyle(juce::Slider::LinearHorizontal);
    focusedParameterControl->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    focusedParameterControl->setSliderSnapsToMousePosition(false);
    focusedParameterControl->setVelocityBasedMode(false);
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

        const juce::ScopedValueSetter<bool> scopedIgnore(suppressFocusedParameterControlChangeHandlers, true);
        focusedParameterControl->setValue(getFocusedParameterControlValueForTarget(),
                                          juce::dontSendNotification);
    };
    addAndMakeVisible(*focusedParameterControl);

#if ! JUCE_IOS
    horizontalResizeHandle = std::make_unique<EdgeResizeHandle>(*this, EdgeResizeHandle::Axis::horizontal);
    verticalResizeHandle = std::make_unique<EdgeResizeHandle>(*this, EdgeResizeHandle::Axis::vertical);
    addAndMakeVisible(*horizontalResizeHandle);
    addAndMakeVisible(*verticalResizeHandle);
#endif

    clipButton = std::make_unique<BoxTextButton>(uiClip);
    clipButton->setButtonText("C");
    clipButton->setTooltip("CLIP INDICATOR");
    clipButton->setTextJustification(juce::Justification::centred);
    clipButton->setClickingTogglesState(false);
    clipButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*clipButton);

    hostButton = std::make_unique<BoxTextButton>(uiAccent);
    hostButton->setButtonText("H");
    hostButton->setTooltip("CLICK: HOST PARAMETERS -- LONG PRESS: TURN ON/OFF HINTS");
    hostButton->setTextJustification(juce::Justification::centred);
    hostButton->setClickingTogglesState(true);
    hostButton->onClick = [this]
    {
        toggleHostParametersSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*hostButton);

    moduleAddButton = std::make_unique<BoxTextButton>(uiClip);
    moduleAddButton->setButtonText("ADD MODULE");
    moduleAddButton->setTextJustification(juce::Justification::centred);
    moduleAddButton->setPressFillEnabled(false);
    moduleAddButton->onClick = [this]
    {
        showModulePicker();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*moduleAddButton);

    eqeModuleLoaded = audioProcessor.isEqeModuleLoaded();
    speModuleLoaded = audioProcessor.isSpeModuleLoaded();
    mieModuleLoaded = audioProcessor.isMieModuleLoaded();
    mxeModuleLoaded = audioProcessor.isMxeModuleLoaded();
    tseModuleLoaded = audioProcessor.isTseModuleLoaded();

    if (auto* speProcessor = audioProcessor.getSpeModuleProcessor())
    {
        auto& speState = speProcessor->getValueTreeState();

        setupSpeControls(speState, *speProcessor);
        speAnalyserComponent = shell_setup_support::createSpeAnalyserComponent(*speProcessor);
        speAnalyserContent.addAndMakeVisible(*speAnalyserComponent);
    }

    setupShellControls();

    setupPresetControls();

    filterDisplayOrder.reserve(VxAudioProcessor::maxEqeFilterCount);
    for (int filterIndex = 0; filterIndex < VxAudioProcessor::maxEqeFilterCount; ++filterIndex)
        filterDisplayOrder.push_back(filterIndex);

    if (auto* initialEqeProcessor = audioProcessor.getEqeModuleProcessor())
        setupEqeControls(initialEqeProcessor->getValueTreeState());

    restoreEditorStateFromValueTree();

    footerTab = std::make_unique<BoxTextButton>(uiGrey500);
    footerTab->setButtonText("VX by MIXOLVE");
    footerTab->setTooltip("ABOUT");
    footerTab->onClick = [this]
    {
        showInfoPrompt(shell_setup_support::getMixolveInfoMarkdown());
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*footerTab);

    startTimerHz(60);
    registerParameterListeners();
    rebindActiveModuleEditors();

    refreshModuleTabButton();
    updateSectionStates();
    updateTooltipTogglePrompt();
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);

    const auto restoredEditorSize = getRestoredEditorSize();
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
    setSize(restoredEditorSize.x, restoredEditorSize.y);
    audioProcessor.setLastEditorSize(restoredEditorSize.x, restoredEditorSize.y);

    suppressEditorSizeStateSave = false;
    syncFocusedParameterControl();
    refreshSpeAnalyserResponse();

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
}
