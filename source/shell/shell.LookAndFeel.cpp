#include "shell.EditorControls.h"

namespace
{
constexpr auto tooltipFontSize = uiFontSize - 4.0f;
constexpr auto tooltipHorizontalPadding = 12;
constexpr auto tooltipVerticalPadding = 6;
constexpr auto tooltipLineGap = 2;
constexpr auto maximumTooltipWidth = 240;

juce::Font makeTooltipFont()
{
    return makeUiFont(juce::Font::plain, tooltipFontSize);
}

juce::StringArray wrapTooltipText(const juce::String& text,
                                  const juce::Font& font,
                                  const int maximumTextWidth)
{
    juce::StringArray lines;
    const auto words = juce::StringArray::fromTokens(text, " ", "");
    juce::String currentLine;
    const auto textWidth = juce::jmax(1, maximumTextWidth);

    auto addWrappedWord = [&lines, &font, textWidth] (const juce::String& word)
    {
        juce::String currentPart;

        for (auto index = 0; index < word.length(); ++index)
        {
            const auto nextPart = currentPart + word.substring(index, index + 1);

            if (currentPart.isNotEmpty() && getTextPixelWidth(font, nextPart) > textWidth)
            {
                lines.add(currentPart);
                currentPart = word.substring(index, index + 1);
                continue;
            }

            currentPart = nextPart;
        }

        if (currentPart.isNotEmpty())
            lines.add(currentPart);
    };

    for (int wordIndex = 0; wordIndex < words.size(); ++wordIndex)
    {
        const auto& word = words[wordIndex];
        const auto candidate = currentLine.isEmpty() ? word
                                                     : currentLine + " " + word;

        if (getTextPixelWidth(font, candidate) <= textWidth)
        {
            currentLine = candidate;
            continue;
        }

        if (currentLine.isNotEmpty())
            lines.add(currentLine);

        if (getTextPixelWidth(font, word) > textWidth)
        {
            addWrappedWord(word);
            currentLine.clear();
            continue;
        }

        currentLine = word;
    }

    if (currentLine.isNotEmpty())
        lines.add(currentLine);

    if (lines.isEmpty())
        lines.add(text);

    return lines;
}
}

DelayedTooltipWindow::DelayedTooltipWindow(juce::Component* parentComponent, const int delayMs)
    : juce::TooltipWindow(parentComponent, 0),
      hoverDelayMs(delayMs)
{
}

void DelayedTooltipWindow::setHintsEnabled(const bool shouldEnable)
{
    hintsEnabled = shouldEnable;
    resetHoverState();

    if (! hintsEnabled)
        hideTip();
}

void DelayedTooltipWindow::setHoverDelayMs(const int delayMs) noexcept
{
    hoverDelayMs = juce::jmax(0, delayMs);
}

void DelayedTooltipWindow::resetHoverState() noexcept
{
    hoveredComponent = nullptr;
    hoveredTip.clear();
    hoverStartTimeMs = 0;
}

juce::String DelayedTooltipWindow::getTipFor(juce::Component& component)
{
    if (! hintsEnabled || juce::ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown())
    {
        resetHoverState();
        return {};
    }

    auto* tooltipClient = dynamic_cast<juce::TooltipClient*>(&component);

    if (tooltipClient == nullptr || component.isCurrentlyBlockedByAnotherModalComponent())
    {
        resetHoverState();
        return {};
    }

    const auto tip = tooltipClient->getTooltip();

    if (tip.isEmpty())
    {
        resetHoverState();
        return {};
    }

    const auto now = juce::Time::getApproximateMillisecondCounter();

    if (hoveredComponent.getComponent() != &component || hoveredTip != tip)
    {
        hoveredComponent = &component;
        hoveredTip = tip;
        hoverStartTimeMs = now;
        return {};
    }

    return now - hoverStartTimeMs >= static_cast<uint32_t>(hoverDelayMs) ? tip
                                                                          : juce::String {};
}

VxAudioProcessorEditor::VxLookAndFeel::VxLookAndFeel()
{
    if (auto typeface = getUiRegularTypeface())
        setDefaultSansSerifTypeface(typeface);
}

void VxAudioProcessorEditor::VxLookAndFeel::setTooltipBoundsConstraint(juce::Rectangle<int> bounds) noexcept
{
    tooltipBoundsConstraint = bounds;
}

void VxAudioProcessorEditor::updateTooltipBoundsConstraint() noexcept
{
    if (lookAndFeel == nullptr
        || footerTab == nullptr
        || shellGlobalHeader == nullptr
        || footerTab->getBounds().isEmpty()
        || shellGlobalHeader->getBounds().isEmpty())
    {
        return;
    }

    auto tooltipBounds = getLocalBounds();
    tooltipBounds.setLeft(footerTab->getX());
    tooltipBounds.setRight(footerTab->getRight());
    tooltipBounds.setTop(shellGlobalHeader->getY());
    tooltipBounds.setBottom(footerTab->getBottom());

    lookAndFeel->setTooltipBoundsConstraint(tooltipBounds);
}

juce::Typeface::Ptr VxAudioProcessorEditor::VxLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
#if JUCE_TARGET_HAS_BINARY_DATA
    const auto useBold = font.isBold();
    if (auto typeface = useBold ? getUiBoldTypeface()
                                : getUiRegularTypeface())
        return typeface;
#else
    juce::ignoreUnused(font);
#endif
    return LookAndFeel_V4::getTypefaceForFont(font);
}

juce::Font VxAudioProcessorEditor::VxLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return makeUiFont();
}

juce::Font VxAudioProcessorEditor::VxLookAndFeel::getPopupMenuFont()
{
    return makeUiFont();
}

void VxAudioProcessorEditor::VxLookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                                                                 int width,
                                                                                 int height,
                                                                                 const juce::PopupMenu::Options&)
{
    g.setColour(uiGrey800);
    g.fillRect(0, 0, width, height);

    g.setColour(uiAccent);
    g.drawRect(0, 0, width, height, 1);
}

int VxAudioProcessorEditor::VxLookAndFeel::getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&)
{
    return 1;
}

void VxAudioProcessorEditor::VxLookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& text,
                                                                                   bool isSeparator,
                                                                                   int,
                                                                                   int& idealWidth,
                                                                                   int& idealHeight,
                                                                                   const juce::PopupMenu::Options&)
{
    if (isSeparator)
    {
        idealWidth = 0;
        idealHeight = 2;
        return;
    }

    idealWidth = juce::jmax(80, getTextPixelWidth(makeUiFont(), text) + 16);
    idealHeight = 30;
}

void VxAudioProcessorEditor::VxLookAndFeel::drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour(uiGrey700);
    g.fillPath(path);

    g.setColour(uiGrey500);
    g.strokePath(path, juce::PathStrokeType(1.0f));
}

int VxAudioProcessorEditor::VxLookAndFeel::getCallOutBoxBorderSize(const juce::CallOutBox&)
{
    return 1;
}

float VxAudioProcessorEditor::VxLookAndFeel::getCallOutBoxCornerSize(const juce::CallOutBox&)
{
    return 0.0f;
}

juce::Rectangle<int> VxAudioProcessorEditor::VxLookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                                             juce::Point<int> screenPos,
                                                                             juce::Rectangle<int> parentArea)
{
    auto allowedArea = tooltipBoundsConstraint.isEmpty() ? parentArea
                                                         : parentArea.getIntersection(tooltipBoundsConstraint);

    if (allowedArea.isEmpty())
        allowedArea = parentArea;

    const auto tooltipFont = makeTooltipFont();
    const auto maximumWidth = juce::jlimit(80, maximumTooltipWidth, juce::jmax(1, allowedArea.getWidth()));
    const auto lines = wrapTooltipText(tipText,
                                       tooltipFont,
                                       maximumWidth - tooltipHorizontalPadding);
    auto textWidth = 0;

    for (const auto& line : lines)
        textWidth = juce::jmax(textWidth, getTextPixelWidth(tooltipFont, line));

    const auto lineHeight = juce::roundToInt(tooltipFont.getHeight()) + tooltipLineGap;
    const auto width = juce::jlimit(12,
                                    maximumWidth,
                                    textWidth + tooltipHorizontalPadding);
    const auto height = juce::jmax(12,
                                   (lineHeight * lines.size()) + tooltipVerticalPadding);
    auto bounds = juce::Rectangle<int>(width, height);
    bounds.setCentre(screenPos.translated(0, -(height + verticalGap)));

    if (bounds.getRight() > allowedArea.getRight())
        bounds.setX(allowedArea.getRight() - bounds.getWidth());

    if (bounds.getX() < allowedArea.getX())
        bounds.setX(allowedArea.getX());

    if (bounds.getY() < allowedArea.getY())
        bounds.setY(screenPos.y + verticalGap);

    if (bounds.getBottom() > allowedArea.getBottom())
        bounds.setBottom(allowedArea.getBottom());

    if (bounds.getY() < allowedArea.getY())
        bounds.setY(allowedArea.getY());

    return bounds;
}

void VxAudioProcessorEditor::VxLookAndFeel::drawTooltip(juce::Graphics& g,
                                                        const juce::String& text,
                                                        const int width,
                                                        const int height)
{
    const auto bounds = juce::Rectangle<int>(width, height);
    const auto tooltipFont = makeTooltipFont();
    const auto lines = wrapTooltipText(text,
                                       tooltipFont,
                                       juce::jmax(1, width - tooltipHorizontalPadding));

    g.setColour(juce::Colour(0xff666666));
    g.fillRect(bounds);

    g.setColour(uiGrey500);
    g.drawRect(bounds, 1);

    g.setColour(uiWhite);
    g.setFont(tooltipFont);

    const auto lineHeight = juce::roundToInt(tooltipFont.getHeight()) + tooltipLineGap;
    auto y = (height - (lineHeight * lines.size())) / 2;

    for (const auto& line : lines)
    {
        g.drawFittedText(line,
                         juce::Rectangle<int>(6, y, width - 12, lineHeight),
                         juce::Justification::centred,
                         1,
                         1.0f);
        y += lineHeight;
    }
}

void VxAudioProcessorEditor::VxLookAndFeel::drawComboBox(juce::Graphics& g,
                                                           int width,
                                                           int height,
                                                           bool,
                                                           int,
                                                           int,
                                                           int,
                                                           int,
                                                           juce::ComboBox& box)
{
    if (dynamic_cast<NoTickComboBox*>(&box) == nullptr)
    {
        juce::LookAndFeel_V4::drawComboBox(g,
                                           width,
                                           height,
                                           false,
                                           0,
                                           0,
                                           0,
                                           0,
                                           box);
        return;
    }

    const auto* noTickBox = dynamic_cast<NoTickComboBox*>(&box);
    const auto backgroundColour = noTickBox != nullptr && noTickBox->isPressedHighlightEnabled()
        ? uiGrey700
        : box.findColour(juce::ComboBox::backgroundColourId);

    g.setColour(backgroundColour);
    g.fillRect(0, 0, width, height);

    g.setColour(noTickBox != nullptr && noTickBox->isPressedHighlightEnabled() ? uiGrey500
                                                                               : box.findColour(juce::ComboBox::outlineColourId));
    g.drawRect(0, 0, width, height, 1);
}

void VxAudioProcessorEditor::VxLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    if (dynamic_cast<NoTickComboBox*>(&box) == nullptr)
    {
        juce::LookAndFeel_V4::positionComboBoxText(box, label);
        label.setFont(getComboBoxFont(box));
        return;
    }

    label.setBounds(1, 1,
                    box.getWidth() - 2,
                    box.getHeight() - 2);

    label.setFont(getComboBoxFont(box));
}

void VxAudioProcessorEditor::VxLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                                                const juce::Rectangle<int>& area,
                                                                bool isSeparator,
                                                                bool isActive,
                                                                bool isHighlighted,
                                                                bool isTicked,
                                                                bool hasSubMenu,
                                                                const juce::String& text,
                                                                const juce::String& shortcutKeyText,
                                                                const juce::Drawable* icon,
                                                                const juce::Colour* textColour)
{
    juce::ignoreUnused(isTicked);
    juce::LookAndFeel_V4::drawPopupMenuItem(g,
                                            area,
                                            isSeparator,
                                            isActive,
                                            isHighlighted,
                                            false,
                                            hasSubMenu,
                                            text,
                                            shortcutKeyText,
                                            icon,
                                            textColour);
}

void VxAudioProcessorEditor::VxLookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g,
                                                                           const juce::Rectangle<int>& area,
                                                                           bool isHighlighted,
                                                                           const juce::PopupMenu::Item& item,
                                                                           const juce::PopupMenu::Options& options)
{
    if (item.isSeparator)
    {
        g.setColour(uiGrey500);
        g.fillRect(area.withHeight(1).withCentre(area.getCentre()));
        return;
    }

    g.setColour(isHighlighted ? uiGrey700 : uiGrey800);
    g.fillRect(area);

    g.setColour(uiAccent);
    g.drawRect(area, 1);

    g.setColour(item.isEnabled ? uiWhite : uiGrey500);
    g.setFont(makeUiFont());
    const auto isNoTickTarget = dynamic_cast<NoTickComboBox*>(options.getTargetComponent()) != nullptr;
    const auto justification = isNoTickTarget ? dynamic_cast<NoTickComboBox*>(options.getTargetComponent())->getPopupMenuTextJustification()
                                              : juce::Justification::centred;
    g.drawFittedText(item.text,
                     area.reduced(isNoTickTarget ? 8 : 6, 0),
                     justification,
                     1,
                     1.0f);
}
