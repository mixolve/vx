#include "EditorControls.h"
#include "EqlHeaderRenderer.h"

#include <utility>

using namespace eql_header_renderer;

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

void BoxTextButton::setAlwaysAccentOutline(const bool shouldAlwaysAccent)
{
    if (alwaysAccentOutline == shouldAlwaysAccent)
        return;

    alwaysAccentOutline = shouldAlwaysAccent;
    repaint();
}

void BoxTextButton::setToggleAccentVisible(const bool shouldShow) noexcept
{
    if (toggleAccentVisible == shouldShow)
        return;

    toggleAccentVisible = shouldShow;
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

void BoxTextButton::setDisclosureArrowVisible(const bool shouldShow) noexcept
{
    if (disclosureArrowVisible == shouldShow)
        return;

    disclosureArrowVisible = shouldShow;
    repaint();
}

void BoxTextButton::setBorderVisible(const bool shouldShow) noexcept
{
    if (borderVisible == shouldShow)
        return;

    borderVisible = shouldShow;
    repaint();
}

void BoxTextButton::setEqlFilterHeaderColouringEnabled(const bool shouldEnable) noexcept
{
    if (eqlFilterHeaderColouringEnabled == shouldEnable)
        return;

    eqlFilterHeaderColouringEnabled = shouldEnable;
    repaint();
}

void BoxTextButton::setABCompareHighlightIndex(const int highlightedIndex) noexcept
{
    const auto clampedIndex = (highlightedIndex == 0 || highlightedIndex == 1) ? highlightedIndex : -1;

    if (abCompareHighlightIndex == clampedIndex)
        return;

    abCompareHighlightIndex = clampedIndex;
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

void BoxTextButton::setLongPressAction(std::function<void()> action, const int delayMs, juce::String promptText)
{
    longPressAction = std::move(action);
    longPressDelayMs = juce::jmax(1, delayMs);
    longPressPromptText = std::move(promptText);
}

void BoxTextButton::flashConfirmationOutline()
{
    confirmationFlashActive = true;
    repaint();
    startTimer(500);
}

void BoxTextButton::paintButton(juce::Graphics& graphics, bool, bool)
{
    const auto buttonDown = isEnabled()
        && pressFillEnabled
        && pressHighlight;
    const auto accentActive = isEnabled() && (alwaysAccentOutline || (toggleAccentVisible && getToggleState()));
    const auto fill = buttonDown ? uiGrey700 : uiGrey800;
    const auto outline = confirmationFlashActive ? juce::Colour { 0xFF99CCCC }
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

    const auto drawDisclosureArrow = [&graphics, this, textColour]
    {
        if (! disclosureArrowVisible)
            return;

        const auto bounds = getLocalBounds().toFloat();
        const auto firstCentreX = bounds.getRight() - 21.0f;
        const auto secondCentreX = bounds.getRight() - 11.0f;
        const auto centreY = bounds.getCentreY();
        constexpr auto shaftHalfHeight = 6.0f;
        constexpr auto headWidth = 2.5f;
        constexpr auto headHeight = 3.0f;
        const auto drawArrow = [&graphics, textColour, centreY] (const float centreX, const bool pointsUp)
        {
            juce::Path arrowPath;
            const auto top = centreY - shaftHalfHeight;
            const auto bottom = centreY + shaftHalfHeight;

            if (pointsUp)
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

            graphics.setColour(textColour);
            graphics.strokePath(arrowPath, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        const auto pointsUp = getToggleState();
        drawArrow(firstCentreX, pointsUp);
        drawArrow(secondCentreX, pointsUp);
    };

    graphics.setColour(textColour);
    if (arrowDirection == ArrowDirection::none)
    {
        const auto font = makeUiFont();
        graphics.setFont(font);

        auto textBounds = getLocalBounds().reduced(6, 0);
        if (disclosureArrowVisible)
            textBounds.removeFromRight(26);

        if (eqlFilterHeaderColouringEnabled
            && drawFilterHeaderHighlight(graphics, getButtonText(), textBounds, font, textJustification))
        {
            drawDisclosureArrow();
            drawBottomDivider();
            return;
        }

        if (drawChannelTokenHighlight(graphics, getButtonText(), textBounds, font, textJustification))
        {
            drawDisclosureArrow();
            drawBottomDivider();
            return;
        }

        if (abCompareHighlightIndex >= 0 && getButtonText() == "AB")
        {
            drawABCompareHighlight(graphics,
                                   textBounds,
                                   font,
                                   textJustification,
                                   abCompareHighlightIndex,
                                   textColour);
            drawDisclosureArrow();
            drawBottomDivider();
            return;
        }

        graphics.drawFittedText(getButtonText(), textBounds, textJustification, 1, 1.0f);
        drawDisclosureArrow();
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

void BoxTextButton::enablementChanged()
{
    if (! isEnabled())
    {
        stopTimer();
        pointerDown = false;
        dragActive = false;
        pressHighlight = false;
        pressCanceled = false;
        longPressEligible = false;
        confirmationFlashActive = false;
        setViewportIgnoreDragFlag(false);

        if (longPressArmed && getButtonText() != longPressOriginalText)
            setButtonText(longPressOriginalText);

        longPressArmed = false;
    }

    repaint();
}

void BoxTextButton::mouseDown(const juce::MouseEvent& event)
{
    if (! isEnabled())
        return;

    if (clearsParameterFocusOnMouseDown)
        shell_parameter_focus::clearFocus(*this);

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
}

void BoxTextButton::mouseDrag(const juce::MouseEvent& event)
{
    if (! isEnabled())
        return;

    if (! pointerDown || longPressArmed)
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
}

void BoxTextButton::mouseUp(const juce::MouseEvent& event)
{
    if (! isEnabled())
        return;

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
        {
            flashConfirmationOutline();
            longPressAction();
        }

        return;
    }

    if (wasPressCanceled)
        return;

    if (contains(event.getPosition()))
    {
        if (onClickWithModifiers && event.mods.isCtrlDown())
        {
            if (onClickWithModifiers(event.mods))
                flashConfirmationOutline();

            return;
        }

        triggerClick();
    }
}

void BoxTextButton::mouseExit(const juce::MouseEvent&)
{
    if (! isEnabled())
        return;

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

    if (! isEnabled())
        return;

    if (confirmationFlashActive)
    {
        confirmationFlashActive = false;
        repaint();
        return;
    }

    if (! pointerDown || ! pressHighlight || ! longPressEligible || dragActive)
        return;

    longPressEligible = false;
    longPressArmed = true;
    setViewportIgnoreDragFlag(true);
    longPressOriginalText = getButtonText();
    setButtonText(longPressPromptText);
    repaint();
}
