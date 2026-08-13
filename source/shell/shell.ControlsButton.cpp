#include "shell.EditorControls.h"
#include "shell.EditorParameterControls.h"

#include <vector>

namespace
{
const juce::Colour leftTokenColour { 0xFF99CC99 };
const juce::Colour rightTokenColour { 0xFFFF9999 };
const juce::Colour midTokenColour { 0xFF99CCCC };
const juce::Colour sideTokenColour { 0xFFFFCC99 };
const juce::Colour volumeTokenColour { 0xFF9999FF };

juce::Colour colourForEqlFilterHeaderToken(const juce::String& token)
{
    if (token == "LL")
        return leftTokenColour;
    if (token == "RR")
        return rightTokenColour;
    if (token == "MM")
        return midTokenColour;
    if (token == "SS")
        return sideTokenColour;
    if (token == "VOL")
        return volumeTokenColour;

    return uiWhite;
}

struct ColouredTextSegment
{
    juce::String text;
    juce::Colour colour;
};

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

void drawFittedSingleLineSegments(juce::Graphics& graphics,
                                  const std::vector<ColouredTextSegment>& segments,
                                  const juce::Rectangle<int>& bounds,
                                  const juce::Font& font,
                                  const juce::Justification justification)
{
    auto totalWidth = 0.0f;

    for (const auto& segment : segments)
        totalWidth += static_cast<float>(getTextPixelWidth(font, segment.text));

    if (totalWidth <= 0.0f || bounds.isEmpty())
        return;

    auto segmentsToDraw = segments;
    auto fittedWidth = totalWidth;

    if (totalWidth > static_cast<float>(bounds.getWidth()))
    {
        static const juce::String ellipsis { "..." };
        const auto ellipsisWidth = static_cast<float>(getTextPixelWidth(font, ellipsis));
        auto remainingWidth = juce::jmax(0.0f, static_cast<float>(bounds.getWidth()) - ellipsisWidth);
        std::vector<ColouredTextSegment> truncatedSegments;

        for (const auto& segment : segments)
        {
            const auto segmentWidth = static_cast<float>(getTextPixelWidth(font, segment.text));

            if (segmentWidth <= remainingWidth)
            {
                truncatedSegments.push_back(segment);
                remainingWidth -= segmentWidth;
                continue;
            }

            auto keptLength = 0;

            for (auto length = 1; length <= segment.text.length(); ++length)
            {
                const auto candidate = segment.text.substring(0, length);

                if (static_cast<float>(getTextPixelWidth(font, candidate)) > remainingWidth)
                    break;

                keptLength = length;
            }

            if (keptLength > 0)
                truncatedSegments.push_back({ segment.text.substring(0, keptLength), segment.colour });

            break;
        }

        truncatedSegments.push_back({ ellipsis, uiWhite });
        segmentsToDraw = std::move(truncatedSegments);

        fittedWidth = 0.0f;
        for (const auto& segment : segmentsToDraw)
            fittedWidth += static_cast<float>(getTextPixelWidth(font, segment.text));
    }

    const auto horizontalFlags = justification.getOnlyHorizontalFlags();
    auto x = static_cast<float>(bounds.getX());

    if ((horizontalFlags & juce::Justification::horizontallyCentred) != 0)
        x += (static_cast<float>(bounds.getWidth()) - fittedWidth) * 0.5f;
    else if ((horizontalFlags & juce::Justification::right) != 0)
        x += static_cast<float>(bounds.getWidth()) - fittedWidth;

    const auto y = static_cast<float>(bounds.getY());
    const auto height = static_cast<float>(bounds.getHeight());

    for (const auto& segment : segmentsToDraw)
    {
        const auto segmentWidth = static_cast<float>(getTextPixelWidth(font, segment.text));
        graphics.setColour(segment.colour);
        graphics.setFont(font);
        graphics.drawText(segment.text,
                          juce::Rectangle<float>(x, y, segmentWidth + 1.0f, height),
                          juce::Justification::centredLeft,
                          false);
        x += segmentWidth;
    }
}

bool drawEqlFilterHeaderHighlight(juce::Graphics& graphics,
                                  const juce::String& text,
                                  const juce::Rectangle<int>& bounds,
                                  const juce::Font& font,
                                  const juce::Justification justification)
{
    if (! text.containsChar('-'))
        return false;

    std::vector<ColouredTextSegment> segments;

    auto foundToken = false;
    auto startIndex = 0;

    while (startIndex < text.length())
    {
        const auto separatorIndex = text.indexOfChar(startIndex, '-');
        const auto tokenEnd = separatorIndex >= 0 ? separatorIndex : text.length();
        const auto token = text.substring(startIndex, tokenEnd);

        if (token == "PHL")
        {
            segments.push_back({ "PH", uiWhite });
            segments.push_back({ "L", leftTokenColour });
            foundToken = true;
        }
        else if (token == "PHR")
        {
            segments.push_back({ "PH", uiWhite });
            segments.push_back({ "R", rightTokenColour });
            foundToken = true;
        }
        else
        {
            const auto tokenColour = colourForEqlFilterHeaderToken(token);

            if (tokenColour != uiWhite)
                foundToken = true;

            segments.push_back({ token, tokenColour });
        }

        if (separatorIndex >= 0)
            segments.push_back({ "-", uiWhite });

        startIndex = tokenEnd + 1;
    }

    if (! foundToken)
        return false;

    drawFittedSingleLineSegments(graphics, segments, bounds, font, justification);
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
    const auto accentActive = isEnabled() && (alwaysAccentOutline || getToggleState());
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

    graphics.setColour(textColour);
    if (arrowDirection == ArrowDirection::none)
    {
        const auto font = makeUiFont();
        graphics.setFont(font);

        const auto textBounds = getLocalBounds().reduced(6, 0);

        if (eqlFilterHeaderColouringEnabled
            && drawEqlFilterHeaderHighlight(graphics, getButtonText(), textBounds, font, textJustification))
        {
            drawBottomDivider();
            return;
        }

        if (drawChannelTokenHighlight(graphics, getButtonText(), textBounds, font, textJustification))
        {
            drawBottomDivider();
            return;
        }

        if (abCompareHighlightIndex >= 0 && getButtonText() == "AB")
        {
            static const juce::Colour activeABColour { 0xFF9999FF };
            drawFittedSingleLineSegments(graphics,
                                         {
                                             { "A", abCompareHighlightIndex == 0 ? activeABColour : textColour },
                                             { "B", abCompareHighlightIndex == 1 ? activeABColour : textColour }
                                         },
                                         textBounds,
                                         font,
                                         textJustification);
            drawBottomDivider();
            return;
        }

        graphics.drawFittedText(getButtonText(), textBounds, textJustification, 1, 1.0f);
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
