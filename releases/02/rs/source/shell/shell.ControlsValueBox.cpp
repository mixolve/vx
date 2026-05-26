#include "shell.EditorControls.h"

ValueBoxComponent::ValueBoxComponent(juce::Slider& sliderToControl)
    : slider(sliderToControl)
{
    setMouseCursor(juce::MouseCursor::IBeamCursor);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

ValueBoxComponent::~ValueBoxComponent()
{
    stopGlobalEditTracking();
}

void ValueBoxComponent::setInteractionEnabled(const bool shouldEnable)
{
    if (interactionEnabled == shouldEnable)
        return;

    interactionEnabled = shouldEnable;
    updateMouseCursor();

    if (! interactionEnabled && editor != nullptr)
        hideEditor(true);
}

void ValueBoxComponent::setOutlineColour(const juce::Colour colour)
{
    if (outlineColour == colour)
        return;

    outlineColour = colour;

    if (editor != nullptr)
        editor->setColour(juce::TextEditor::outlineColourId, outlineColour);

    repaint();
}

void ValueBoxComponent::setHighlightColour(const juce::Colour colour)
{
    if (highlightColour == colour)
        return;

    highlightColour = colour;

    if (editor != nullptr)
        editor->setColour(juce::TextEditor::highlightColourId, highlightColour);
}

void ValueBoxComponent::setPromptActive(const bool shouldBeActive)
{
    if (promptActive == shouldBeActive)
        return;

    promptActive = shouldBeActive;

    if (! shouldBeActive)
    {
        pressHighlight = false;
        pointerDown = false;
        dragDetected = false;
    }

    repaint();
}

void ValueBoxComponent::setCustomPromptAction(std::function<void()> action)
{
    customPromptAction = std::move(action);
    updateMouseCursor();
}

void ValueBoxComponent::updateMouseCursor()
{
    if (interactionEnabled)
    {
        setMouseCursor(juce::MouseCursor::IBeamCursor);
        return;
    }

    setMouseCursor(customPromptAction != nullptr ? juce::MouseCursor::PointingHandCursor
                                                 : juce::MouseCursor::NormalCursor);
}

void ValueBoxComponent::paint(juce::Graphics& g)
{
    const auto displayText = displayTextProvider != nullptr ? displayTextProvider()
                                                            : slider.getTextFromValue(slider.getValue());
    auto backgroundColour = uiGrey800;
    auto borderColour = outlineColour;

    if (promptActive)
    {
        backgroundColour = uiGrey700;
        borderColour = uiAccent;
    }
    else if (pressHighlight)
    {
        backgroundColour = uiGrey700;
    }

    g.setColour(backgroundColour);
    g.fillRect(getLocalBounds());

    g.setColour(borderColour);
    g.drawRect(getLocalBounds(), 1);

    g.setColour(getDisplayTextColour(displayText));
    g.setFont(makeUiFont());
    g.drawFittedText(displayText,
                     getLocalBounds().reduced(4, 0),
                     juce::Justification::centred,
                     1,
                     1.0f);
}

void ValueBoxComponent::resized()
{
    if (editor != nullptr)
        editor->setBounds(getLocalBounds());
}

void ValueBoxComponent::mouseDown(const juce::MouseEvent& event)
{
    auto* clickedComponent = event.originalComponent;
    const auto clickIsInsideThisValueBox = clickedComponent != nullptr
        && (clickedComponent == this || isParentOf(clickedComponent));

    if (editor != nullptr && ! clickIsInsideThisValueBox)
    {
        hideEditor(false);
        return;
    }

    if (! clickIsInsideThisValueBox)
        return;

    if ((! interactionEnabled && customPromptAction == nullptr) || ! event.mods.isLeftButtonDown())
        return;

    pointerDown = true;
    dragDetected = false;
    pressHighlight = true;
    repaint();
}

void ValueBoxComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (! pointerDown)
        return;

    if (! dragDetected && event.mouseWasDraggedSinceMouseDown())
        dragDetected = true;

    const auto shouldHighlight = contains(event.getPosition());

    if (pressHighlight != shouldHighlight)
    {
        pressHighlight = shouldHighlight;
        repaint();
    }
}

void ValueBoxComponent::mouseUp(const juce::MouseEvent& event)
{
    if (! pointerDown)
        return;

    const auto shouldOpenPrompt = contains(event.getPosition())
        && ! event.mods.isPopupMenu();

    pointerDown = false;
    dragDetected = false;

    if (shouldOpenPrompt)
    {
        if (customPromptAction != nullptr)
            customPromptAction();
        else
            showEditor();
    }

    pressHighlight = false;
    repaint();
}

void ValueBoxComponent::mouseExit(const juce::MouseEvent&)
{
    if (! pointerDown)
        return;

    if (pressHighlight)
    {
        pressHighlight = false;
        repaint();
    }
}

void ValueBoxComponent::showEditor()
{
    if (editor != nullptr)
        return;

    if (auto* owner = findParentComponentOfClass<VxAudioProcessorEditor>())
    {
        const auto editorText = editorTextProvider != nullptr ? editorTextProvider()
                                                              : slider.getTextFromValue(slider.getValue());
        auto safeThis = juce::Component::SafePointer<ValueBoxComponent>(this);

        setPromptActive(true);
        pressHighlight = false;
        repaint();

        owner->showTextPrompt(editorText,
                              [safeThis] (const juce::String& enteredText)
                              {
                                  if (safeThis == nullptr)
                                      return false;

                                  safeThis->applyEnteredText(enteredText);
                                  return true;
                              },
                              [safeThis]
                              {
                                  if (safeThis != nullptr)
                                      safeThis->setPromptActive(false);
                              },
                              [safeThis]
                              {
                                  if (safeThis != nullptr)
                                      safeThis->setPromptActive(false);
                              });
        return;
    }

    const auto editorText = editorTextProvider != nullptr ? editorTextProvider()
                                                          : slider.getTextFromValue(slider.getValue());
    auto textEditor = std::make_unique<CopyPasteTextEditor>();
    textEditor->setName(slider.getName());
    textEditor->setFont(makeUiFont());
    textEditor->setPopupMenuEnabled(true);
    textEditor->setJustification(juce::Justification::centred);
    textEditor->setColour(juce::TextEditor::textColourId, uiWhite);
    textEditor->setColour(juce::TextEditor::backgroundColourId, uiGrey800);
    textEditor->setColour(juce::TextEditor::outlineColourId, outlineColour);
    textEditor->setColour(juce::TextEditor::focusedOutlineColourId, outlineColour);
    textEditor->setColour(juce::TextEditor::highlightColourId, highlightColour);
    textEditor->setText(editorText, false);
    textEditor->onReturnKey = [this] { hideEditor(false); };
    textEditor->onEscapeKey = [this] { hideEditor(true); };
    textEditor->onFocusLost = [this] { hideEditor(false); };

    addAndMakeVisible(*textEditor);
    editor = std::move(textEditor);
    startGlobalEditTracking();
    resized();
    editor->grabKeyboardFocus();
    editor->selectAll();
}

void ValueBoxComponent::applyEnteredText(const juce::String& enteredText)
{
    const auto enteredValue = textToValueParser != nullptr
        ? textToValueParser(enteredText)
        : slider.getValueFromText(enteredText);
    const auto clampedValue = juce::jlimit(static_cast<double>(slider.getMinimum()),
                                           static_cast<double>(slider.getMaximum()),
                                           enteredValue);

    applyValue(clampedValue);

    if (auto* parent = getParentComponent())
        parent->repaint();

    repaint();
}

void ValueBoxComponent::applyValue(const double value)
{
    slider.setValue(value, juce::sendNotificationSync);
}

void ValueBoxComponent::hideEditor(const bool discard)
{
    if (editor == nullptr)
        return;

    if (! discard)
    {
        const auto enteredText = editor->getText().trim();
        const auto enteredValue = textToValueParser != nullptr
            ? textToValueParser(enteredText)
            : slider.getValueFromText(enteredText);
        const auto clampedValue = juce::jlimit(static_cast<double>(slider.getMinimum()),
                                               static_cast<double>(slider.getMaximum()),
                                               enteredValue);

        applyValue(clampedValue);
    }

    removeChildComponent(editor.get());
    editor.reset();
    stopGlobalEditTracking();
    clearKeyboardFocus(*this);
    repaint();
}

void ValueBoxComponent::startGlobalEditTracking()
{
    if (isTrackingGlobalClicks)
        return;

    juce::Desktop::getInstance().addGlobalMouseListener(this);
    isTrackingGlobalClicks = true;
}

void ValueBoxComponent::stopGlobalEditTracking()
{
    if (! isTrackingGlobalClicks)
        return;

    juce::Desktop::getInstance().removeGlobalMouseListener(this);
    isTrackingGlobalClicks = false;
}
