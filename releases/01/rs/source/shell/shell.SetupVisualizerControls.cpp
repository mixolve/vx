#include "shell.EditorParameterControls.h"

void VxAudioProcessorEditor::setupVisualizerControls()
{
    visualizerRangeLowControl = std::make_unique<LocalParameterControl>("LOW",
                                                                        1,
                                                                        -120.0,
                                                                        60.0,
                                                                        0.1,
                                                                        -24.0);
    visualizerRangeLowControl->setValue(visualizerRangeLowDb, false);
    visualizerRangeLowControl->onValueChanged = [this]
    {
        if (suppressVisualizerControlChangeHandlers)
            return;

        visualizerRangeLowDb = static_cast<float>(visualizerRangeLowControl->getValue());

        if (visualizerRangeHighDb < visualizerRangeLowDb + 6.0f)
        {
            const juce::ScopedValueSetter<bool> scopedIgnore(suppressVisualizerControlChangeHandlers, true);
            visualizerRangeHighDb = visualizerRangeLowDb + 6.0f;
            visualizerRangeHighControl->setValue(visualizerRangeHighDb, false);
        }

        refreshVisualizerResponse();
        storeEditorStateToValueTree();
    };
    addAndMakeVisible(*visualizerRangeLowControl);

    visualizerRangeHighControl = std::make_unique<LocalParameterControl>("HIGH",
                                                                         1,
                                                                         -120.0,
                                                                         60.0,
                                                                         0.1,
                                                                         24.0);
    visualizerRangeHighControl->setValue(visualizerRangeHighDb, false);
    visualizerRangeHighControl->onValueChanged = [this]
    {
        if (suppressVisualizerControlChangeHandlers)
            return;

        visualizerRangeHighDb = static_cast<float>(visualizerRangeHighControl->getValue());

        if (visualizerRangeHighDb < visualizerRangeLowDb + 6.0f)
        {
            const juce::ScopedValueSetter<bool> scopedIgnore(suppressVisualizerControlChangeHandlers, true);
            visualizerRangeLowDb = visualizerRangeHighDb - 6.0f;
            visualizerRangeLowControl->setValue(visualizerRangeLowDb, false);
        }

        refreshVisualizerResponse();
        storeEditorStateToValueTree();
    };
    addAndMakeVisible(*visualizerRangeHighControl);

    visualizerCursorButton = std::make_unique<BoxTextButton>(uiGrey500);
    visualizerCursorButton->setButtonText(visualizerCursorEnabled ? "CURSOR-OFF" : "CURSOR-ON");
    visualizerCursorButton->setTextJustification(juce::Justification::centred);
    visualizerCursorButton->setClickingTogglesState(true);
    visualizerCursorButton->setToggleState(visualizerCursorEnabled, juce::dontSendNotification);
    visualizerCursorButton->onClick = [this]
    {
        visualizerCursorEnabled = visualizerCursorButton->getToggleState();
        visualizerCursorButton->setButtonText(visualizerCursorEnabled ? "CURSOR-OFF" : "CURSOR-ON");
        refreshVisualizerResponse();
        storeEditorStateToValueTree();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*visualizerCursorButton);

    auto makeVisualizerLegendButton = [this] (const juce::String& text, const juce::Colour colour, bool& stateValue)
    {
        auto button = std::make_unique<BoxTextButton>(colour);
        button->setButtonText(text);
        button->setTextJustification(juce::Justification::centred);
        button->setClickingTogglesState(true);
        button->setToggleState(stateValue, juce::dontSendNotification);
        button->onClick = [this, &stateValue, buttonPtr = button.get()]
        {
            stateValue = buttonPtr->getToggleState();
            refreshVisualizerResponse();
            storeEditorStateToValueTree();
            clearKeyboardFocus(*this);
        };
        return button;
    };

    visualizerShowStereoButton = makeVisualizerLegendButton("LR", visualizerStereoColour, visualizerShowStereo);
    addAndMakeVisible(*visualizerShowStereoButton);

    visualizerShowLeftButton = makeVisualizerLegendButton("L", visualizerLeftColour, visualizerShowLeft);
    addAndMakeVisible(*visualizerShowLeftButton);

    visualizerShowRightButton = makeVisualizerLegendButton("R", visualizerRightColour, visualizerShowRight);
    addAndMakeVisible(*visualizerShowRightButton);

    visualizerShowMidButton = makeVisualizerLegendButton("M", visualizerMidColour, visualizerShowMid);
    addAndMakeVisible(*visualizerShowMidButton);

    visualizerShowSideButton = makeVisualizerLegendButton("S", visualizerSideColour, visualizerShowSide);
    addAndMakeVisible(*visualizerShowSideButton);

    visualizerVisibilityButton = std::make_unique<BoxTextButton>(uiGrey500);
    visualizerVisibilityButton->setButtonText("SHOW");
    visualizerVisibilityButton->setClickingTogglesState(true);
    visualizerVisibilityButton->onClick = [this]
    {
        visualizerVisible = visualizerVisibilityButton->getToggleState();
        updateEditorWidthForVisualizerVisibility();
        updateSectionStates();
        resized();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*visualizerVisibilityButton);
}
