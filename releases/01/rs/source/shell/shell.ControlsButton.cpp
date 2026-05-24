#include "shell.EditorControls.h"

#include <cmath>

void BoxTextButton::setCancelClickOnLeave(const bool shouldEnable) noexcept
{
    cancelClickOnLeave = shouldEnable;
}

BoxTextButton::BoxTextButton(const juce::Colour accent)
    : accentColour(accent)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

BoxTextButton::~BoxTextButton()
{
    stopTimer();
}

void BoxTextButton::setManualInteractionEnabled(const bool shouldEnable) noexcept
{
    manualInteractionEnabled = shouldEnable;
}

void BoxTextButton::setAlwaysAccentOutline(const bool shouldAlwaysAccent)
{
    if (alwaysAccentOutline == shouldAlwaysAccent)
        return;

    alwaysAccentOutline = shouldAlwaysAccent;
    repaint();
}

void BoxTextButton::setTextJustification(const juce::Justification justification) noexcept
{
    textJustification = justification;
    repaint();
}

void BoxTextButton::setArrowDirection(const ArrowDirection direction) noexcept
{
    arrowDirection = direction;
    repaint();
}

void BoxTextButton::setBorderVisible(const bool shouldShow) noexcept
{
    if (borderVisible == shouldShow)
        return;

    borderVisible = shouldShow;
    repaint();
}

void BoxTextButton::setLeadingDot(const juce::Colour colour, const float level)
{
    leadingDotVisible = true;
    leadingDotColour = colour;
    setLeadingDotLevel(level);
}

void BoxTextButton::setLeadingDotLevel(const float level)
{
    const auto clampedLevel = juce::jlimit(0.0f, 1.0f, level);

    if (std::abs(leadingDotLevel - clampedLevel) < 1.0e-6f)
        return;

    leadingDotLevel = clampedLevel;
    repaint();
}

void BoxTextButton::setLongPressAction(std::function<void()> action, const int delayMs, juce::String promptText)
{
    longPressAction = std::move(action);
    longPressDelayMs = juce::jmax(1, delayMs);
    longPressPromptText = std::move(promptText);
}

void BoxTextButton::paintButton(juce::Graphics& graphics, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto buttonDown = manualInteractionEnabled ? manualPointerDown
                                                     : pressHighlight;
    const auto accentActive = alwaysAccentOutline || getToggleState();
    const auto fill = buttonDown ? uiGrey700 : uiGrey800;
    const auto outline = accentActive ? accentColour : uiGrey500;

    graphics.setColour(fill);
    graphics.fillRect(getLocalBounds());

    if (borderVisible)
    {
        graphics.setColour(outline);
        graphics.drawRect(getLocalBounds(), 1);
    }

    graphics.setColour(uiWhite);
    if (arrowDirection == ArrowDirection::none)
    {
        const auto font = makeUiFont();
        graphics.setFont(font);

        if (! leadingDotVisible || getButtonText().isEmpty())
        {
            graphics.drawFittedText(getButtonText(), getLocalBounds().reduced(6, 0), textJustification, 1, 1.0f);
            return;
        }

        const auto textBounds = getLocalBounds().toFloat().reduced(6.0f, 0.0f);
        const auto dotDiameter = 6.0f;
        const auto dotGap = uiGapFloat;
        const auto textWidth = static_cast<float>(getTextPixelWidth(font, getButtonText()));
        const auto groupWidth = dotDiameter + dotGap + textWidth;
        const auto horizontalFlags = textJustification.getOnlyHorizontalFlags();
        auto groupLeft = textBounds.getX();

        if ((horizontalFlags & juce::Justification::horizontallyCentred) != 0)
            groupLeft = textBounds.getCentreX() - (groupWidth * 0.5f);
        else if ((horizontalFlags & juce::Justification::right) != 0)
            groupLeft = textBounds.getRight() - groupWidth;

        const auto dotBounds = juce::Rectangle<float>(groupLeft,
                                                      textBounds.getCentreY() - dotDiameter * 0.5f,
                                                      dotDiameter,
                                                      dotDiameter);

        graphics.setColour(leadingDotColour.withAlpha(leadingDotLevel > 0.5f ? 1.0f : 0.25f));
        graphics.fillEllipse(dotBounds);

        graphics.setColour(uiWhite);
        graphics.drawFittedText(getButtonText(),
                                juce::Rectangle<int>(juce::roundToInt(groupLeft + dotDiameter + dotGap),
                                                     getLocalBounds().getY(),
                                                     juce::roundToInt(textWidth) + 2,
                                                     getLocalBounds().getHeight()),
                                juce::Justification::centredLeft,
                                1,
                                1.0f);
        return;
    }

    auto arrowBounds = getLocalBounds().toFloat().reduced(10.0f, 8.0f);

    if (arrowBounds.getWidth() <= 0.0f || arrowBounds.getHeight() <= 0.0f)
        return;

    const auto centreX = arrowBounds.getCentreX();
    const auto top = arrowBounds.getY();
    const auto bottom = arrowBounds.getBottom();
    const auto headWidth = juce::jmax(3.0f, arrowBounds.getWidth() * 0.35f);
    const auto headHeight = juce::jmax(3.0f, arrowBounds.getHeight() * 0.35f);

    juce::Path arrowPath;

    if (arrowDirection == ArrowDirection::up)
    {
        arrowPath.startNewSubPath(centreX, bottom);
        arrowPath.lineTo(centreX, top + headHeight);
        arrowPath.startNewSubPath(centreX - headWidth, top + headHeight);
        arrowPath.lineTo(centreX, top);
        arrowPath.lineTo(centreX + headWidth, top + headHeight);
    }
    else
    {
        arrowPath.startNewSubPath(centreX, top);
        arrowPath.lineTo(centreX, bottom - headHeight);
        arrowPath.startNewSubPath(centreX - headWidth, bottom - headHeight);
        arrowPath.lineTo(centreX, bottom);
        arrowPath.lineTo(centreX + headWidth, bottom - headHeight);
    }

    graphics.strokePath(arrowPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void BoxTextButton::mouseDown(const juce::MouseEvent& event)
{
    if (! manualInteractionEnabled)
    {
        if (event.mods.isPopupMenu() || ! event.mods.isLeftButtonDown())
            return;

        pointerDown = true;
        dragActive = false;
        pressCanceled = false;
        pressHighlight = true;
        longPressEligible = longPressAction != nullptr;
        longPressArmed = false;
        longPressOriginalText = getButtonText();
        if (longPressEligible)
            startTimer(longPressDelayMs);
        else
            stopTimer();
        repaint();
        return;
    }

    manualPointerDown = true;
    manualDragActive = false;
    pressHighlight = true;
    repaint();
}

void BoxTextButton::mouseDrag(const juce::MouseEvent& event)
{
    if (! manualInteractionEnabled)
    {
        if (! pointerDown)
            return;

        if (cancelClickOnLeave && ! pressCanceled && ! contains(event.getPosition()))
        {
            pressCanceled = true;
            pressHighlight = false;
            longPressEligible = false;
            stopTimer();
            repaint();
        }

        if (! dragActive && event.getDistanceFromDragStart() >= 4)
        {
            dragActive = true;
            longPressEligible = false;
            stopTimer();
        }

        const auto shouldHighlight = contains(event.getPosition());

        if (pressHighlight != shouldHighlight)
        {
            pressHighlight = shouldHighlight;
            repaint();
        }

        if (! shouldHighlight)
        {
            longPressEligible = false;
            stopTimer();
        }

        return;
    }

    if (! manualDragActive && event.getDistanceFromDragStart() >= 4)
    {
        manualDragActive = true;

        if (onDragBegin)
            onDragBegin(event);
    }

    if (manualDragActive && onDragMove)
        onDragMove(event);
}

void BoxTextButton::mouseUp(const juce::MouseEvent& event)
{
    if (! manualInteractionEnabled)
    {
        stopTimer();

        const auto wasLongPressArmed = longPressArmed;
        const auto wasPressCanceled = pressCanceled;
        pointerDown = false;
        dragActive = false;
        pressHighlight = false;
        longPressEligible = false;
        longPressArmed = false;
        pressCanceled = false;
        repaint();

        if (wasLongPressArmed)
        {
            if (getButtonText() != longPressOriginalText)
                setButtonText(longPressOriginalText);

            repaint();

            if (! wasPressCanceled && contains(event.getPosition()) && longPressAction != nullptr)
                longPressAction();

            return;
        }

        if (wasPressCanceled)
            return;

        if (contains(event.getPosition()))
            triggerClick();

        return;
    }

    const auto wasManualDrag = manualDragActive;
    manualPointerDown = false;
    manualDragActive = false;
    pressHighlight = false;
    repaint();

    if (wasManualDrag)
    {
        if (onDragFinish)
            onDragFinish(event);
    }
    else if (contains(event.getPosition()) && onPressed)
    {
        onPressed();
    }
}

void BoxTextButton::mouseExit(const juce::MouseEvent&)
{
    if (manualInteractionEnabled)
    {
        if (! manualPointerDown)
            return;

        manualPointerDown = false;
        repaint();
        return;
    }

    if (! pointerDown || ! pressHighlight)
        return;

    if (cancelClickOnLeave)
    {
        pressCanceled = true;
        longPressEligible = false;
        stopTimer();
    }

    pressHighlight = false;
    if (! longPressArmed)
    {
        longPressEligible = false;
        stopTimer();
    }
    repaint();
}

void BoxTextButton::timerCallback()
{
    stopTimer();

    if (! pointerDown || ! pressHighlight || ! longPressEligible || dragActive || manualInteractionEnabled)
        return;

    longPressEligible = false;
    longPressArmed = true;
    longPressOriginalText = getButtonText();
    setButtonText(longPressPromptText);
    repaint();
}
