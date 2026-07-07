#include "shell.EditorControls.h"
#include "shell.EditorParameterControls.h"

#include <cmath>

namespace
{
const juce::Colour leftTokenColour { 0xFF99CC99 };
const juce::Colour rightTokenColour { 0xFFFF9999 };

bool drawChannelTokenHighlight(juce::Graphics& graphics,
                               const juce::String& text,
                               const juce::Rectangle<int>& bounds,
                               const juce::Font& font,
                               const juce::Justification justification)
{
    if (! text.containsChar('.'))
        return false;

    juce::AttributedString attributed;
    attributed.setJustification(justification);

    auto foundToken = false;
    auto startIndex = 0;

    while (startIndex < text.length())
    {
        const auto dotIndex = text.indexOfChar(startIndex, '.');
        const auto tokenEnd = dotIndex >= 0 ? dotIndex : text.length();
        const auto token = text.substring(startIndex, tokenEnd);
        auto colour = uiWhite;

        if (token == "L")
        {
            colour = leftTokenColour;
            foundToken = true;
        }
        else if (token == "R")
        {
            colour = rightTokenColour;
            foundToken = true;
        }

        attributed.append(token, font, colour);

        if (dotIndex >= 0)
            attributed.append(".", font, uiWhite);

        startIndex = tokenEnd + 1;
    }

    if (! foundToken)
        return false;

    juce::TextLayout layout;
    layout.createLayout(attributed, static_cast<float>(bounds.getWidth()));
    layout.draw(graphics, bounds.toFloat());
    return true;
}
}

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

void BoxTextButton::setPressFillEnabled(const bool shouldEnable) noexcept
{
    pressFillEnabled = shouldEnable;
}

void BoxTextButton::setClearsParameterFocusOnMouseDown(const bool shouldClear) noexcept
{
    clearsParameterFocusOnMouseDown = shouldClear;
}

void BoxTextButton::setFillVisible(const bool shouldShow) noexcept
{
    if (fillVisible == shouldShow)
        return;

    fillVisible = shouldShow;
    repaint();
}

void BoxTextButton::setDividerLineVisible(const bool shouldShow) noexcept
{
    if (dividerLineVisible == shouldShow)
        return;

    dividerLineVisible = shouldShow;
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

void BoxTextButton::setTextColourOverride(const juce::Colour colour)
{
    hasTextColourOverride = true;
    textColourOverride = colour;
    repaint();
}

void BoxTextButton::clearTextColourOverride()
{
    if (! hasTextColourOverride)
        return;

    hasTextColourOverride = false;
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

void BoxTextButton::flashHostAssignmentOutline()
{
    hostAssignmentFlashActive = true;
    repaint();
    startTimer(500);
}

void BoxTextButton::paintButton(juce::Graphics& graphics, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto buttonDown = pressFillEnabled
        && (manualInteractionEnabled ? manualPointerDown : pressHighlight);
    const auto accentActive = alwaysAccentOutline || getToggleState();
    const auto fill = buttonDown ? uiGrey700 : uiGrey800;
    const auto outline = hostAssignmentFlashActive ? juce::Colour { 0xFF99CCCC }
                                                   : (accentActive ? accentColour : uiGrey500);

    if (fillVisible)
    {
        graphics.setColour(fill);
        graphics.fillRect(getLocalBounds());
    }

    if (borderVisible)
    {
        graphics.setColour(outline);
        graphics.drawRect(getLocalBounds(), 1);
    }

    const auto textColour = isEnabled()
        ? (hasTextColourOverride ? textColourOverride : uiWhite)
        : uiGrey500;
    const auto drawBottomDivider = [&graphics, this]
    {
        if (! dividerLineVisible)
            return;

        const auto bounds = getLocalBounds().toFloat();
        constexpr auto dividerThickness = 2.0f;
        graphics.setColour(uiGrey500);
        graphics.fillRect(bounds.withY(bounds.getBottom() - dividerThickness).withHeight(dividerThickness));
    };

    graphics.setColour(textColour);
    if (arrowDirection == ArrowDirection::none)
    {
        const auto font = makeUiFont();
        graphics.setFont(font);

        if (! leadingDotVisible || getButtonText().isEmpty())
        {
            const auto textBounds = getLocalBounds().reduced(6, 0);

            if (drawChannelTokenHighlight(graphics, getButtonText(), textBounds, font, textJustification))
            {
                drawBottomDivider();
                return;
            }

            graphics.drawFittedText(getButtonText(), textBounds, textJustification, 1, 1.0f);
            drawBottomDivider();
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

        graphics.setColour(textColour);
        graphics.drawFittedText(getButtonText(),
                                juce::Rectangle<int>(juce::roundToInt(groupLeft + dotDiameter + dotGap),
                                                     getLocalBounds().getY(),
                                                     juce::roundToInt(textWidth) + 2,
                                                     getLocalBounds().getHeight()),
                                juce::Justification::centredLeft,
                                1,
                                1.0f);
        drawBottomDivider();
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
    drawBottomDivider();
}

void BoxTextButton::mouseDown(const juce::MouseEvent& event)
{
    if (clearsParameterFocusOnMouseDown)
        shell_parameter_focus::clearFocus();

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
        setViewportIgnoreDragFlag(false);
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

        if (longPressArmed)
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
        setViewportIgnoreDragFlag(false);
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
        {
            if (onClickWithModifiers && event.mods.isCtrlDown())
            {
                if (onClickWithModifiers(event.mods))
                    flashHostAssignmentOutline();

                return;
            }

            triggerClick();
        }

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
        if (! longPressArmed)
            setViewportIgnoreDragFlag(false);
        stopTimer();
    }

    pressHighlight = false;
    if (! longPressArmed)
    {
        longPressEligible = false;
        setViewportIgnoreDragFlag(false);
        stopTimer();
    }
    repaint();
}

void BoxTextButton::timerCallback()
{
    stopTimer();

    if (hostAssignmentFlashActive)
    {
        hostAssignmentFlashActive = false;
        repaint();
        return;
    }

    if (! pointerDown || ! pressHighlight || ! longPressEligible || dragActive || manualInteractionEnabled)
        return;

    longPressEligible = false;
    longPressArmed = true;
    setViewportIgnoreDragFlag(true);
    longPressOriginalText = getButtonText();
    setButtonText(longPressPromptText);
    repaint();
}
