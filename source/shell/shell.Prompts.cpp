#include "shell.EditorControls.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
constexpr int promptEditorHeight = 30;
constexpr int promptPanelPadding = uiGap;
constexpr int promptItemHeight = 30;
constexpr int promptItemGap = uiGap;

struct MarkdownLink
{
    juce::String label;
    juce::String url;
};

juce::String getDisplayNameFromUrl(const juce::String& urlText)
{
    auto displayName = urlText.trim();

    if (displayName.startsWithIgnoreCase("https://"))
        displayName = displayName.substring(8);
    else if (displayName.startsWithIgnoreCase("http://"))
        displayName = displayName.substring(7);

    return displayName.upToFirstOccurrenceOf("/", false, false).trim();
}

MarkdownLink parseMarkdownLinkLine(const juce::String& line)
{
    MarkdownLink link;
    const auto trimmed = line.trim();

    if (trimmed.startsWithChar('['))
    {
        const auto labelEnd = trimmed.indexOfChar(']');
        const auto openParen = trimmed.indexOfChar('(');
        const auto closeParen = trimmed.lastIndexOfChar(')');

        if (labelEnd > 1 && openParen > labelEnd && closeParen > openParen)
        {
            link.label = trimmed.substring(1, labelEnd).trim();
            link.url = trimmed.substring(openParen + 1, closeParen).trim();
            return link;
        }
    }

    if (trimmed.startsWithIgnoreCase("https://") || trimmed.startsWithIgnoreCase("http://"))
    {
        link.label = getDisplayNameFromUrl(trimmed);
        link.url = trimmed;
    }

    return link;
}

struct MarkdownBlock
{
    enum class Kind
    {
        text,
        link,
        spacer
    };

    Kind kind = Kind::text;
    juce::String text;
    juce::String url;
    int headingLevel = 0;
};

MarkdownBlock parseMarkdownBlock(const juce::String& line)
{
    MarkdownBlock block;
    const auto trimmed = line.trim();

    if (trimmed.isEmpty())
    {
        block.kind = MarkdownBlock::Kind::spacer;
        return block;
    }

    int headingLevel = 0;

    while (headingLevel < trimmed.length() && trimmed[headingLevel] == '#')
        ++headingLevel;

    if (headingLevel > 0
        && headingLevel <= 6
        && (headingLevel == trimmed.length()
            || trimmed[headingLevel] == ' '
            || trimmed[headingLevel] == '\t'))
    {
        block.kind = MarkdownBlock::Kind::text;
        block.headingLevel = headingLevel;
        block.text = trimmed.substring(headingLevel).trim();
        return block;
    }

    const auto link = parseMarkdownLinkLine(trimmed);

    if (link.url.isNotEmpty())
    {
        block.kind = MarkdownBlock::Kind::link;
        block.text = link.label;
        block.url = link.url;
        return block;
    }

    block.kind = MarkdownBlock::Kind::text;
    block.text = trimmed;
    return block;
}

std::vector<MarkdownBlock> parseMarkdownBlocks(const juce::String& markdownText)
{
    std::vector<MarkdownBlock> blocks;
    juce::String paragraphText;
    bool lastWasSpacer = false;

    const auto flushParagraph = [&]
    {
        if (paragraphText.isEmpty())
            return;

        MarkdownBlock block;
        block.kind = MarkdownBlock::Kind::text;
        block.text = paragraphText.trimEnd();
        blocks.push_back(std::move(block));
        paragraphText.clear();
        lastWasSpacer = false;
    };

    const auto lines = juce::StringArray::fromLines(markdownText);

    for (const auto& rawLine : lines)
    {
        const auto parsed = parseMarkdownBlock(rawLine);

        if (parsed.kind == MarkdownBlock::Kind::spacer)
        {
            flushParagraph();

            if (! blocks.empty() && ! lastWasSpacer)
            {
                MarkdownBlock spacer;
                spacer.kind = MarkdownBlock::Kind::spacer;
                blocks.push_back(std::move(spacer));
                lastWasSpacer = true;
            }

            continue;
        }

        if (parsed.kind == MarkdownBlock::Kind::text && parsed.headingLevel == 0)
        {
            if (paragraphText.isNotEmpty())
                paragraphText << '\n';

            paragraphText << parsed.text;
            lastWasSpacer = false;
            continue;
        }

        flushParagraph();
        blocks.push_back(parsed);
        lastWasSpacer = false;
    }

    flushParagraph();

    while (! blocks.empty() && blocks.front().kind == MarkdownBlock::Kind::spacer)
        blocks.erase(blocks.begin());

    while (! blocks.empty() && blocks.back().kind == MarkdownBlock::Kind::spacer)
        blocks.pop_back();

    if (blocks.empty())
    {
        MarkdownBlock fallback;
        fallback.kind = MarkdownBlock::Kind::link;
        fallback.text = "mixolve.cc";
        fallback.url = "https://mixolve.cc/";
        blocks.push_back(std::move(fallback));
    }

    return blocks;
}

class MarkdownRowComponent : public juce::Component
{
public:
    ~MarkdownRowComponent() override = default;

    virtual int getPreferredHeight(int width) const = 0;
};

class MarkdownTextRowBase : public MarkdownRowComponent
{
public:
    MarkdownTextRowBase(juce::String textIn,
                        juce::Font fontIn,
                        juce::Colour colourIn)
        : text(std::move(textIn)),
          font(std::move(fontIn)),
          colour(colourIn)
    {
        setOpaque(false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(false, false);
    }

    int getPreferredHeight(int width) const override
    {
        updateLayout(width);
        return juce::jmax(1, juce::roundToInt(layout.getHeight()));
    }

protected:
    void paint(juce::Graphics& g) override
    {
        updateLayout(getWidth());
        g.setColour(colour);
        layout.draw(g, getLocalBounds().toFloat());
    }

    void updateLayout(int width) const
    {
        const auto contentWidth = juce::jmax(1, width);

        if (cachedWidth == contentWidth)
            return;

        juce::AttributedString attributed(text);
        attributed.setFont(font);
        attributed.setColour(colour);
        attributed.setJustification(juce::Justification::centred);
        attributed.setWordWrap(juce::AttributedString::byWord);

        layout.createLayout(attributed, static_cast<float>(contentWidth));
        cachedWidth = contentWidth;
    }

    juce::String text;
    juce::Font font;
    juce::Colour colour;
    mutable juce::TextLayout layout;
    mutable int cachedWidth = -1;
};

class MarkdownTextRow final : public MarkdownTextRowBase
{
public:
    MarkdownTextRow(juce::String rowText,
                    juce::Font rowFont,
                    juce::Colour rowColour)
        : MarkdownTextRowBase(std::move(rowText), std::move(rowFont), rowColour)
    {
        setInterceptsMouseClicks(false, false);
    }
};

class MarkdownLinkRow final : public MarkdownRowComponent
{
public:
    MarkdownLinkRow(juce::String text, juce::String urlText)
        : linkButton(std::move(text), juce::URL(std::move(urlText)))
    {
        setOpaque(false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(false, true);

        const auto linkFont = makeUiFont(juce::Font::underlined, 22.0f);
        const auto linkHeight = juce::jmax(1, juce::roundToInt(linkFont.getHeight()));

        linkButton.setFont(linkFont, false);
        linkButton.setJustificationType(juce::Justification::centred);
        linkButton.setColour(juce::HyperlinkButton::textColourId, uiAccent);
        linkButton.setMouseClickGrabsKeyboardFocus(false);
        linkButton.setWantsKeyboardFocus(false);
        linkButton.setSize(1, linkHeight);
        linkButton.changeWidthToFitText();
        addAndMakeVisible(linkButton);
    }

    int getPreferredHeight(int) const override
    {
        return juce::jmax(1, juce::roundToInt(linkButton.getHeight()));
    }

    void resized() override
    {
        const auto buttonBounds = linkButton.getBounds().withCentre(getLocalBounds().getCentre());
        linkButton.setBounds(buttonBounds);
    }

private:
    juce::HyperlinkButton linkButton;
};

class MarkdownSpacerRow final : public MarkdownRowComponent
{
public:
    explicit MarkdownSpacerRow(const int spacerHeightIn)
        : spacerHeight(spacerHeightIn)
    {
        setOpaque(false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(false, false);
    }

    int getPreferredHeight(int) const override
    {
        return juce::jmax(0, spacerHeight);
    }

private:
    int spacerHeight = 0;
};

class MarkdownContentView final : public juce::Component
{
public:
    explicit MarkdownContentView(juce::String markdownText)
    {
        setOpaque(false);
        setInterceptsMouseClicks(false, true);
        setMarkdownText(std::move(markdownText));
    }

    void setMarkdownText(juce::String markdownText)
    {
        rows.clear();

        const auto blocks = parseMarkdownBlocks(markdownText);
        rows.reserve(blocks.size());

        for (const auto& block : blocks)
        {
            if (block.kind == MarkdownBlock::Kind::spacer)
            {
                rows.push_back(std::make_unique<MarkdownSpacerRow>(promptItemGap));
            }
            else if (block.kind == MarkdownBlock::Kind::link)
            {
                rows.push_back(std::make_unique<MarkdownLinkRow>(block.text, block.url));
            }
            else
            {
                const auto headingLevel = juce::jlimit(1, 6, block.headingLevel);
                const auto headingHeight = juce::jmax(18.0f, 25.0f - static_cast<float>(headingLevel - 1) * 2.0f);
                const auto font = block.headingLevel > 0 ? makeUiFont(juce::Font::bold, headingHeight)
                                                         : makeUiFont();

                rows.push_back(std::make_unique<MarkdownTextRow>(block.text, font, uiWhite));
            }

            addAndMakeVisible(*rows.back());
        }

        resized();
        repaint();
    }

    int getContentHeight(int width) const
    {
        const auto contentWidth = juce::jmax(1, width - (contentPadding * 2));

        if (rows.empty())
            return contentPadding * 2;

        return getRowsHeight(contentWidth) + (contentPadding * 2);
    }

    void resized() override
    {
        const auto bounds = getLocalBounds().reduced(contentPadding);
        const auto contentWidth = bounds.getWidth();
        auto y = bounds.getY();

        for (size_t index = 0; index < rows.size(); ++index)
        {
            const auto rowHeight = rows[index]->getPreferredHeight(contentWidth);
            rows[index]->setBounds(bounds.getX(), y, contentWidth, rowHeight);
            y += rowHeight;

            if (index + 1 < rows.size())
                y += promptItemGap;
        }
    }

private:
    int getRowsHeight(int width) const
    {
        const auto contentWidth = juce::jmax(1, width);

        auto totalHeight = 0;

        for (size_t index = 0; index < rows.size(); ++index)
        {
            totalHeight += rows[index]->getPreferredHeight(contentWidth);

            if (index + 1 < rows.size())
                totalHeight += promptItemGap;
        }

        return totalHeight;
    }

private:
    std::vector<std::unique_ptr<MarkdownRowComponent>> rows;
    int contentPadding = promptPanelPadding;
};

class FloatingTextPrompt final : public juce::Component
{
public:
    using CommitCallback = std::function<bool(const juce::String&)>;
    using DismissCallback = std::function<void()>;
    using CloseCallback = std::function<void()>;

    FloatingTextPrompt(juce::String currentText,
                       CommitCallback commitCallback,
                     juce::Rectangle<int> anchorBoundsIn,
                       DismissCallback dismissCallback,
                       CloseCallback closeCallback)
        : onCommit(std::move(commitCallback)),
            anchorBounds(anchorBoundsIn),
          onDismiss(std::move(dismissCallback)),
          onClose(std::move(closeCallback))
    {
        setOpaque(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(true, true);

        textEditor.setFont(makeUiFont());
        textEditor.setWantsKeyboardFocus(true);
        textEditor.setMouseClickGrabsKeyboardFocus(true);
        textEditor.setPopupMenuEnabled(true);
        textEditor.setJustification(juce::Justification::centred);
        textEditor.setColour(juce::TextEditor::textColourId, uiWhite);
        textEditor.setColour(juce::TextEditor::backgroundColourId, uiGrey800);
        textEditor.setColour(juce::TextEditor::outlineColourId, uiGrey500);
        textEditor.setColour(juce::TextEditor::focusedOutlineColourId, uiAccent);
        textEditor.setColour(juce::TextEditor::highlightColourId, uiAccent);
        textEditor.setColour(juce::TextEditor::highlightedTextColourId, uiWhite);
        textEditor.setText(std::move(currentText), false);
        textEditor.setReturnKeyStartsNewLine(false);
        textEditor.onReturnKey = [this] { commit(); };
        textEditor.onEscapeKey = [this] { cancel(); };
        addAndMakeVisible(textEditor);
    }

    void grabEditorFocus()
    {
        textEditor.grabKeyboardFocus();
        textEditor.selectAll();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillAll();
    }

    void resized() override
    {
        auto promptBounds = anchorBounds;

        if (! promptBounds.isEmpty())
        {
            promptBounds = promptBounds.getIntersection(getLocalBounds());

            if (! promptBounds.isEmpty())
            {
                const auto minimumHeight = juce::jmin(promptEditorHeight,
                                                      juce::jmax(rowHeight, promptBounds.getHeight()));
                textEditor.setBounds(promptBounds.withHeight(minimumHeight));
                return;
            }
        }

        const auto visibleBounds = getVisibleBounds();
        const auto editorInsetX = getEditorInsetX(getWidth());
        const auto editorInsetTop = juce::roundToInt(static_cast<float>(juce::jmax(0, visibleBounds.getHeight())) * editorInsetTopRatio);
        const auto editorInsetBottom = juce::roundToInt(static_cast<float>(juce::jmax(0, visibleBounds.getHeight())) * editorInsetBottomRatio);
        const auto promptWidth = juce::jmax(0, getWidth() - editorInsetX * 2);
        const auto promptHeight = juce::jmin(promptEditorHeight, juce::jmax(0, visibleBounds.getHeight() - editorInsetTop - editorInsetBottom));

        textEditor.setBounds(editorInsetX,
                             visibleBounds.getY() + editorInsetTop,
                             promptWidth,
                             promptHeight);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        cancel();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        textEditor.grabKeyboardFocus();
        return textEditor.keyPressed(key);
    }

private:
    juce::Rectangle<int> getVisibleBounds() const
    {
        auto bounds = getLocalBounds();


        return bounds;
    }

    void requestClose()
    {
        if (closePending)
            return;

        closePending = true;

        if (onDismiss != nullptr)
            onDismiss();

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<FloatingTextPrompt>(this)]
                                        {
                                            if (safeThis == nullptr || safeThis->onClose == nullptr)
                                                return;

                                            safeThis->onClose();
                                        });
    }

    void commit()
    {
        if (closePending)
            return;

        const auto enteredText = textEditor.getText().trim();

        if (enteredText.isEmpty())
        {
            textEditor.selectAll();
            textEditor.grabKeyboardFocus();
            return;
        }

        if (onCommit != nullptr && ! onCommit(enteredText))
        {
            textEditor.selectAll();
            textEditor.grabKeyboardFocus();
            return;
        }

        requestClose();
    }

    void cancel()
    {
        requestClose();
    }

    CopyPasteTextEditor textEditor;
    CommitCallback onCommit;
    juce::Rectangle<int> anchorBounds;
    DismissCallback onDismiss;
    CloseCallback onClose;
    bool closePending = false;
};

class FloatingChoicePrompt final : public juce::Component
{
public:
    using SelectCallback = std::function<void(int)>;
    using DismissCallback = std::function<void()>;
    using CloseCallback = std::function<void()>;

    FloatingChoicePrompt(juce::Rectangle<int> anchorBoundsIn,
                         juce::StringArray choicesIn,
                         int selectedIndexIn,
                         std::vector<bool> itemEnabledStatesIn,
                         juce::Justification itemJustificationIn,
                         SelectCallback selectCallback,
                         DismissCallback dismissCallback,
                         CloseCallback closeCallback,
                         juce::StringArray itemTooltipsIn)
        : anchorBounds(std::move(anchorBoundsIn)),
          choices(std::move(choicesIn)),
          selectedIndex(selectedIndexIn),
          itemEnabledStates(std::move(itemEnabledStatesIn)),
          itemJustification(itemJustificationIn),
          itemTooltips(std::move(itemTooltipsIn)),
          onSelect(std::move(selectCallback)),
          onDismiss(std::move(dismissCallback)),
          onClose(std::move(closeCallback))
    {
        setOpaque(false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(true, true);
        choiceViewport.setViewedComponent(&choiceContent, false);
        choiceViewport.setScrollBarsShown(false, false);
        choiceViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
        choiceViewport.setWantsKeyboardFocus(false);
        choiceViewport.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(choiceViewport);

        if (itemEnabledStates.size() != static_cast<size_t>(choices.size()))
            itemEnabledStates.assign(static_cast<size_t>(choices.size()), true);

        itemButtons.reserve(static_cast<size_t>(choices.size()));

        for (int index = 0; index < choices.size(); ++index)
        {
            auto button = std::make_unique<BoxTextButton>(uiAccent);
            button->setButtonText(choices[index]);
            if (juce::isPositiveAndBelow(index, itemTooltips.size()))
                button->setTooltip(itemTooltips[index]);
            button->setTextJustification(itemJustification);
            button->setAlwaysAccentOutline(index == selectedIndex);
            const auto isEnabled = itemEnabled(index);
            button->setEnabled(isEnabled);
            button->setAlpha(isEnabled ? 1.0f : 0.45f);
            button->onClick = [safeThis = juce::Component::SafePointer<FloatingChoicePrompt>(this), index]
            {
                if (safeThis != nullptr)
                    safeThis->choose(index);
            };
            choiceContent.addAndMakeVisible(*button);
            itemButtons.push_back(std::move(button));
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillAll();

        g.setColour(uiGrey700);
        g.fillRect(panelBounds);

        g.setColour(uiGrey500);
        g.drawRect(panelBounds, 1);
    }

    void resized() override
    {
        const auto visibleBounds = getVisibleBounds();
        const auto itemCount = static_cast<int>(choices.size());
        const auto itemBlockHeight = (itemCount * promptItemHeight)
            + (juce::jmax(0, itemCount - 1) * promptItemGap);
        const auto availableWidth = juce::jmax(0, visibleBounds.getWidth() - (promptPanelPadding * 2));
        const auto promptWidth = juce::jmax(1, juce::jmin(availableWidth, anchorBounds.getWidth()));
        const auto promptHeight = juce::jmin(juce::jmax(anchorBounds.getHeight(), itemBlockHeight + (promptPanelPadding * 2)),
                                             juce::jmax(0, visibleBounds.getHeight() - (promptPanelPadding * 2)));

        panelBounds = juce::Rectangle<int>(promptWidth, promptHeight);
        panelBounds.setX(anchorBounds.getX());
        panelBounds.setY(anchorBounds.getY());
        panelBounds = panelBounds.constrainedWithin(visibleBounds.reduced(promptPanelPadding));

        const auto viewportBounds = panelBounds.reduced(promptPanelPadding);
        choiceViewport.setBounds(viewportBounds);
        choiceContent.setSize(viewportBounds.getWidth(), juce::jmax(viewportBounds.getHeight(), itemBlockHeight));

        auto contentBounds = choiceContent.getLocalBounds();
        for (size_t index = 0; index < itemButtons.size(); ++index)
        {
            auto itemBounds = contentBounds.removeFromTop(promptItemHeight);
            itemButtons[index]->setBounds(itemBounds);

            if (index + 1 < itemButtons.size())
                contentBounds.removeFromTop(promptItemGap);
        }

        if (! initialScrollApplied)
        {
            initialScrollApplied = true;

            if (juce::isPositiveAndBelow(selectedIndex, static_cast<int>(itemButtons.size())))
            {
                const auto itemY = selectedIndex * (promptItemHeight + promptItemGap);
                const auto maxOffset = juce::jmax(0, choiceContent.getHeight() - choiceViewport.getHeight());
                const auto targetY = itemY - juce::jmax(0, choiceViewport.getHeight() - promptItemHeight) / 2;
                choiceViewport.setViewPosition(0, juce::jlimit(0, maxOffset, targetY));
            }
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (! panelBounds.contains(event.getPosition()))
            cancel();
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! panelBounds.contains(event.getPosition()))
            return;

        const auto directionalDelta = wheel.isReversed ? wheel.deltaY : -wheel.deltaY;
        const auto scrollAmount = wheel.isSmooth
            ? static_cast<int>(std::round(directionalDelta * 220.0f))
            : static_cast<int>(std::round((directionalDelta < 0.0f ? -1.0f : 1.0f) * 48.0f));

        if (scrollAmount == 0)
            return;

        const auto maxOffset = juce::jmax(0, choiceContent.getHeight() - choiceViewport.getHeight());

        if (maxOffset <= 0)
            return;

        choiceViewport.setViewPosition(0, juce::jlimit(0, maxOffset, choiceViewport.getViewPositionY() + scrollAmount));
    }

private:
    juce::Rectangle<int> getVisibleBounds() const
    {
        auto bounds = getLocalBounds();


        return bounds;
    }

    void choose(const int index)
    {
        if (closePending)
            return;

        if (! itemEnabled(index))
            return;

        auto deferredSelectCallback = std::move(onSelect);
        const auto choiceIndex = index;
        requestClose();

        juce::MessageManager::callAsync([choiceIndex,
                                         callback = std::move(deferredSelectCallback)]() mutable
                                        {
                                            if (callback != nullptr)
                                                callback(choiceIndex);
                                        });
    }

    void requestClose()
    {
        if (closePending)
            return;

        closePending = true;

        if (onDismiss != nullptr)
            onDismiss();

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<FloatingChoicePrompt>(this)]
                                        {
                                            if (safeThis == nullptr || safeThis->onClose == nullptr)
                                                return;

                                            safeThis->onClose();
                                        });
    }

    void cancel()
    {
        requestClose();
    }

    bool itemEnabled(const int index) const
    {
        if (! juce::isPositiveAndBelow(index, static_cast<int>(itemEnabledStates.size())))
            return true;

        return itemEnabledStates[static_cast<size_t>(index)];
    }

    juce::Rectangle<int> anchorBounds;
    juce::StringArray choices;
    int selectedIndex = -1;
    std::vector<bool> itemEnabledStates;
    juce::Justification itemJustification = juce::Justification::centred;
    juce::Viewport choiceViewport;
    juce::Component choiceContent;
    std::vector<std::unique_ptr<BoxTextButton>> itemButtons;
    juce::StringArray itemTooltips;
    SelectCallback onSelect;
    DismissCallback onDismiss;
    CloseCallback onClose;
    juce::Rectangle<int> panelBounds;
    bool initialScrollApplied = false;
    bool closePending = false;
};

class FloatingInfoPrompt final : public juce::Component
{
public:
    using CloseCallback = std::function<void()>;

    FloatingInfoPrompt(juce::String markdownText,
                       CloseCallback closeCallback)
        : markdownContent(std::move(markdownText)),
          okButton(uiAccent),
          onClose(std::move(closeCallback))
    {
        setOpaque(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(true, true);

        markdownViewport.setViewedComponent(&markdownContent, false);
        markdownViewport.setScrollBarsShown(false, false);
        markdownViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
        markdownViewport.setWantsKeyboardFocus(false);
        markdownViewport.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(markdownViewport);

        okButton.setButtonText("OK");
        okButton.setTextJustification(juce::Justification::centred);
        okButton.onClick = [safeThis = juce::Component::SafePointer<FloatingInfoPrompt>(this)]
        {
            if (safeThis != nullptr)
                safeThis->requestClose();
        };
        addAndMakeVisible(okButton);
    }

    ~FloatingInfoPrompt() override
    {
        markdownViewport.setViewedComponent(nullptr, false);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillAll();

        g.setColour(uiGrey700);
        g.fillRect(panelBounds);

        g.setColour(uiGrey500);
        g.drawRect(panelBounds, 1);
    }

    void resized() override
    {
        const auto visibleBounds = getVisibleBounds();
        auto anchorBounds = juce::Rectangle<int>();
        auto panelVisibleBounds = visibleBounds;

        if (auto* owner = findParentComponentOfClass<VxAudioProcessorEditor>())
        {
            const auto ownerVisibleBounds = owner->getInfoPromptVisibleBounds().getIntersection(visibleBounds);
            const auto ownerAnchorBounds = owner->getInfoPromptAnchorBounds().getIntersection(visibleBounds);

            if (! ownerVisibleBounds.isEmpty())
                panelVisibleBounds = ownerVisibleBounds;

            if (! ownerAnchorBounds.isEmpty())
                anchorBounds = ownerAnchorBounds;
        }

        if (panelVisibleBounds.isEmpty())
            panelVisibleBounds = visibleBounds.reduced(promptPanelPadding);

        if (anchorBounds.isEmpty())
            anchorBounds = panelVisibleBounds;

        const auto desiredWidth = juce::jmax(1, panelVisibleBounds.getWidth());
        const auto desiredHeight = juce::jmax(1, panelVisibleBounds.getHeight());

        panelBounds = juce::Rectangle<int>(desiredWidth, desiredHeight);
        panelBounds.setX(panelVisibleBounds.getX());
        panelBounds.setY(panelVisibleBounds.getY());
        panelBounds = panelBounds.constrainedWithin(panelVisibleBounds);

        auto contentBounds = panelBounds.reduced(promptPanelPadding);
        auto okBounds = contentBounds.removeFromBottom(rowHeight);

        if (! contentBounds.isEmpty())
            contentBounds.removeFromBottom(promptPanelPadding);

        markdownViewport.setBounds(contentBounds);
        markdownContent.setSize(contentBounds.getWidth(),
                                juce::jmax(contentBounds.getHeight(),
                                           markdownContent.getContentHeight(contentBounds.getWidth())));
        okButton.setBounds(okBounds);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (! panelBounds.contains(event.getPosition()))
            cancel();
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! markdownViewport.getBounds().contains(event.getPosition()))
            return;

        const auto directionalDelta = wheel.isReversed ? wheel.deltaY : -wheel.deltaY;
        const auto scrollAmount = wheel.isSmooth
            ? static_cast<int>(std::round(directionalDelta * 220.0f))
            : static_cast<int>(std::round((directionalDelta < 0.0f ? -1.0f : 1.0f) * 48.0f));

        if (scrollAmount == 0)
            return;

        const auto maxOffset = juce::jmax(0, markdownContent.getHeight() - markdownViewport.getHeight());

        if (maxOffset <= 0)
            return;

        markdownViewport.setViewPosition(0, juce::jlimit(0, maxOffset, markdownViewport.getViewPositionY() + scrollAmount));
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            cancel();
            return true;
        }

        return false;
    }

private:
    juce::Rectangle<int> getVisibleBounds() const
    {
        auto bounds = getLocalBounds();


        return bounds;
    }

    void requestClose()
    {
        if (closePending)
            return;

        closePending = true;

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<FloatingInfoPrompt>(this)]
                                        {
                                            if (safeThis == nullptr || safeThis->onClose == nullptr)
                                                return;

                                            safeThis->onClose();
                                        });
    }

    void cancel()
    {
        requestClose();
    }

    juce::Rectangle<int> panelBounds;
    MarkdownContentView markdownContent;
    juce::Viewport markdownViewport;
    BoxTextButton okButton;
    CloseCallback onClose;
    bool closePending = false;
};
}

void VxAudioProcessorEditor::showTextPrompt(const juce::String& currentText,
                                             std::function<bool(const juce::String&)> onCommit,
                                             juce::Rectangle<int> anchorBounds,
                                             std::function<void()> onClose,
                                             std::function<void()> onDismiss)
{
    dismissTextPrompt();

    const auto preservedFilterScrollY = filterViewport.getViewPositionY();
    const auto preservedSpeAnalyserScrollY = speAnalyserViewport.getViewPositionY();

    auto* prompt = new FloatingTextPrompt(currentText,
                                          std::move(onCommit),
                                          anchorBounds,
                                          std::move(onDismiss),
                                          [this, closeCallback = std::move(onClose)]
                                          {
                                              const auto closePreservedFilterScrollY = filterViewport.getViewPositionY();
                                              const auto closePreservedSpeAnalyserScrollY = speAnalyserViewport.getViewPositionY();
                                              dismissTextPrompt();

                                              if (filterViewport.isVisible())
                                              {
                                                  const auto maxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
                                                  filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, closePreservedFilterScrollY));
                                              }

                                              if (speAnalyserViewport.isVisible())
                                              {
                                                  const auto maxOffset = juce::jmax(0, speAnalyserContent.getHeight() - speAnalyserViewport.getHeight());
                                                  speAnalyserViewport.setViewPosition(0, juce::jlimit(0, maxOffset, closePreservedSpeAnalyserScrollY));
                                              }

                                              if (closeCallback)
                                                  closeCallback();
                                          });

    textPromptOverlay.reset(prompt);
    addAndMakeVisible(*textPromptOverlay);
    resized();

    if (filterViewport.isVisible())
    {
        const auto maxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
        filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, preservedFilterScrollY));
    }

    if (speAnalyserViewport.isVisible())
    {
        const auto maxOffset = juce::jmax(0, speAnalyserContent.getHeight() - speAnalyserViewport.getHeight());
        speAnalyserViewport.setViewPosition(0, juce::jlimit(0, maxOffset, preservedSpeAnalyserScrollY));
    }

    juce::MessageManager::callAsync([safePrompt = juce::Component::SafePointer<FloatingTextPrompt>(prompt)]
                                    {
                                        if (safePrompt != nullptr)
                                        {
                                            safePrompt->toFront(false);
                                            safePrompt->grabEditorFocus();
                                        }
                                    });

    juce::Timer::callAfterDelay(25, [safePrompt = juce::Component::SafePointer<FloatingTextPrompt>(prompt)]
                                    {
                                        if (safePrompt != nullptr)
                                            safePrompt->grabEditorFocus();
                                    });
}

void VxAudioProcessorEditor::showChoicePrompt(const juce::Rectangle<int>& anchorBounds,
                                               const juce::StringArray& choices,
                                               int selectedIndex,
                                               std::vector<bool> itemEnabledStates,
                                               const juce::Justification itemJustification,
                                               std::function<void(int)> onSelect,
                                               std::function<void()> onClose,
                                               std::function<void()> onDismiss,
                                               juce::StringArray itemTooltips)
{
    const auto preservedFilterScrollY = filterViewport.getViewPositionY();
    const auto preservedSpeAnalyserScrollY = speAnalyserViewport.getViewPositionY();

    dismissTextPrompt();

    auto* prompt = new FloatingChoicePrompt(anchorBounds,
                                            choices,
                                            selectedIndex,
                                            std::move(itemEnabledStates),
                                            itemJustification,
                                            std::move(onSelect),
                                            std::move(onDismiss),
                                            [this, closeCallback = std::move(onClose)]
                                            {
                                                const auto closePreservedFilterScrollY = filterViewport.getViewPositionY();
                                                const auto closePreservedSpeAnalyserScrollY = speAnalyserViewport.getViewPositionY();
                                               dismissTextPrompt();

                                               if (filterViewport.isVisible())
                                               {
                                                   const auto maxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
                                                   filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, closePreservedFilterScrollY));
                                               }

                                               if (speAnalyserViewport.isVisible())
                                               {
                                                   const auto maxOffset = juce::jmax(0, speAnalyserContent.getHeight() - speAnalyserViewport.getHeight());
                                                   speAnalyserViewport.setViewPosition(0, juce::jlimit(0, maxOffset, closePreservedSpeAnalyserScrollY));
                                               }

                                               if (closeCallback)
                                                   closeCallback();
                                           },
                                           std::move(itemTooltips));

    textPromptOverlay.reset(prompt);
    addAndMakeVisible(*textPromptOverlay);
    resized();

    if (filterViewport.isVisible())
    {
        const auto maxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
        filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, preservedFilterScrollY));
    }

    if (speAnalyserViewport.isVisible())
    {
        const auto maxOffset = juce::jmax(0, speAnalyserContent.getHeight() - speAnalyserViewport.getHeight());
        speAnalyserViewport.setViewPosition(0, juce::jlimit(0, maxOffset, preservedSpeAnalyserScrollY));
    }

    prompt->toFront(true);
}

void VxAudioProcessorEditor::showInfoPrompt(const juce::String& markdownText)
{
    dismissTextPrompt();

    auto* prompt = new FloatingInfoPrompt(markdownText,
                                          [this]
                                          {
                                              dismissTextPrompt();
                                          });

    textPromptOverlay.reset(prompt);
    addAndMakeVisible(*textPromptOverlay);
    resized();
    prompt->grabKeyboardFocus();
    prompt->toFront(true);
}

void VxAudioProcessorEditor::dismissTextPrompt()
{
    if (textPromptOverlay == nullptr)
        return;

    removeChildComponent(textPromptOverlay.get());
    textPromptOverlay.reset();
    clearKeyboardFocus(*this);
    repaint();
}
