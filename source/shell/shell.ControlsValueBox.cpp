#include "shell.EditorControls.h"

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
}

bool scrollViewportWithWheel(juce::Viewport& viewport, const int contentHeight, const juce::MouseWheelDetails& wheel)
{
    const auto maxScrollY = juce::jmax(0, contentHeight - viewport.getHeight());

    if (maxScrollY <= 0)
        return false;

    const auto dominantDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY
                                                                                : -wheel.deltaX;

    if (std::abs(dominantDelta) < 1.0e-6f)
        return false;

    auto pixelDelta = juce::roundToInt(-dominantDelta
                                       * (wheel.isSmooth
                                              ? focusedParameterScrollSensitivity
                                              : (focusedParameterScrollSensitivity / 2.0f)));

    if (pixelDelta == 0)
        pixelDelta = dominantDelta < 0.0f ? 24 : -24;

    viewport.setViewPosition(0, juce::jlimit(0, maxScrollY, viewport.getViewPositionY() + pixelDelta));
    return true;
}

ValueBoxComponent::ValueBoxComponent(juce::Slider& sliderToControl)
    : slider(sliderToControl)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    updateMouseCursor();
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

void ValueBoxComponent::setScrollGesturesPassThrough(const bool shouldPassThrough)
{
    scrollGesturesPassThrough = shouldPassThrough;
}

void ValueBoxComponent::updateMouseCursor()
{
    if (customPromptAction != nullptr)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }

    if (interactionEnabled)
    {
        setMouseCursor(juce::MouseCursor::IBeamCursor);
        return;
    }

    setMouseCursor(juce::MouseCursor::NormalCursor);
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
    dragStartViewportY = 0;

    if (scrollGesturesPassThrough)
    {
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
            dragStartViewportY = viewport->getViewPositionY();
    }

    pressHighlight = true;
    repaint();
}

void ValueBoxComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (! pointerDown)
        return;

    if (! dragDetected && event.mouseWasDraggedSinceMouseDown())
        dragDetected = true;

    if (scrollGesturesPassThrough)
    {
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        {
            const auto viewedHeight = viewport->getViewedComponent() != nullptr
                ? viewport->getViewedComponent()->getHeight()
                : 0;
            const auto maxScrollY = juce::jmax(0, viewedHeight - viewport->getHeight());
            viewport->setViewPosition(0,
                                      juce::jlimit(0,
                                                   maxScrollY,
                                                   dragStartViewportY - event.getDistanceFromDragStartY()));
        }
    }

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
        && ! dragDetected
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

void ValueBoxComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);

    if (! interactionEnabled || editor != nullptr || customPromptAction != nullptr)
        return;

    if (scrollGesturesPassThrough)
    {
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        {
            const auto viewedHeight = viewport->getViewedComponent() != nullptr
                ? viewport->getViewedComponent()->getHeight()
                : 0;
            scrollViewportWithWheel(*viewport, viewedHeight, wheel);
        }

        return;
    }

    if (applyWheelToSliderValue(slider, wheel))
    {
        if (auto* parent = getParentComponent())
            parent->repaint();

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
        const auto anchorBounds = owner->getLocalArea(this, getLocalBounds());

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
                              anchorBounds,
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
