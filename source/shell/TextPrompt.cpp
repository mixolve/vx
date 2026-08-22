#include "PromptComponents.h"

#include <utility>

namespace
{
constexpr int promptEditorHeight = 30;

class FloatingTextPrompt final : public PromptComponent
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

    void focusEditor() override
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
} // namespace

std::unique_ptr<PromptComponent> makeTextPrompt(juce::String currentText,
                                                std::function<bool(const juce::String&)> onCommit,
                                                const juce::Rectangle<int> anchorBounds,
                                                std::function<void()> onDismiss,
                                                std::function<void()> onClose)
{
    return std::make_unique<FloatingTextPrompt>(std::move(currentText),
                                                std::move(onCommit),
                                                anchorBounds,
                                                std::move(onDismiss),
                                                std::move(onClose));
}
