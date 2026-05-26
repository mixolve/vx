#pragma once

#include "shared.UiStyle.h"

#include <functional>
#include <memory>

class CopyPasteTextEditor final : public juce::TextEditor
{
public:
    CopyPasteTextEditor();
    ~CopyPasteTextEditor() override;

    void addPopupMenuItems(juce::PopupMenu& menuToAddTo, const juce::MouseEvent* mouseClickEvent) override;
    void performPopupMenuAction(int menuItemID) override;

private:
    class PopupLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getPopupMenuFont() override;
        void drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                                int width,
                                                int height,
                                                const juce::PopupMenu::Options&) override;
        int getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&) override;
        void getIdealPopupMenuItemSizeWithOptions(const juce::String& text,
                                                  bool isSeparator,
                                                  int standardMenuItemHeight,
                                                  int& idealWidth,
                                                  int& idealHeight,
                                                  const juce::PopupMenu::Options&) override;
        void drawPopupMenuItem(juce::Graphics& g,
                               const juce::Rectangle<int>& area,
                               bool isSeparator,
                               bool isActive,
                               bool isHighlighted,
                               bool isTicked,
                               bool hasSubMenu,
                               const juce::String& text,
                               const juce::String& shortcutKeyText,
                               const juce::Drawable* icon,
                               const juce::Colour* textColour) override;
        void drawPopupMenuItemWithOptions(juce::Graphics& g,
                                          const juce::Rectangle<int>& area,
                                          bool isHighlighted,
                                          const juce::PopupMenu::Item& item,
                                          const juce::PopupMenu::Options& options) override;
    };

    PopupLookAndFeel popupLookAndFeel;
};

class ValueBoxComponent final : public juce::Component
{
public:
    explicit ValueBoxComponent(juce::Slider& sliderToControl);
    ~ValueBoxComponent() override;

    void setInteractionEnabled(bool shouldEnable);
    void setOutlineColour(juce::Colour colour);
    void setHighlightColour(juce::Colour colour);
    void setPromptActive(bool shouldBeActive);
    void setCustomPromptAction(std::function<void()> action);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent&) override;

    std::function<juce::String()> displayTextProvider;
    std::function<juce::String()> editorTextProvider;
    std::function<double(const juce::String&)> textToValueParser;

private:
    void updateMouseCursor();
    void showEditor();
    void applyValue(double value);
    void applyEnteredText(const juce::String& enteredText);
    void hideEditor(bool discard);
    void startGlobalEditTracking();
    void stopGlobalEditTracking();

    juce::Slider& slider;
    bool isTrackingGlobalClicks = false;
    bool interactionEnabled = true;
    bool pointerDown = false;
    bool dragDetected = false;
    bool pressHighlight = false;
    bool promptActive = false;
    juce::Colour outlineColour = uiGrey500;
    juce::Colour highlightColour = uiAccent;
    std::function<void()> customPromptAction;
    std::unique_ptr<juce::TextEditor> editor;
};

class BoxTextButton final : public juce::TextButton, private juce::Timer
{
public:
    enum class ArrowDirection
    {
        none,
        up,
        down
    };

    explicit BoxTextButton(juce::Colour accent);
    ~BoxTextButton() override;

    void setManualInteractionEnabled(bool shouldEnable) noexcept;
    void setAlwaysAccentOutline(bool shouldAlwaysAccent);
    void setTextJustification(juce::Justification justification) noexcept;
    void setArrowDirection(ArrowDirection direction) noexcept;
    void setBorderVisible(bool shouldShow) noexcept;
    void setCancelClickOnLeave(bool shouldEnable) noexcept;
    void setLeadingDot(juce::Colour colour, float level);
    void setLeadingDotLevel(float level);
    void setLongPressAction(std::function<void()> action, int delayMs = 500, juce::String promptText = "RESET?");

    void paintButton(juce::Graphics& graphics, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent&) override;

    std::function<void()> onPressed;
    std::function<void(const juce::MouseEvent&)> onDragBegin;
    std::function<void(const juce::MouseEvent&)> onDragMove;
    std::function<void(const juce::MouseEvent&)> onDragFinish;

private:
    juce::Colour accentColour;
    bool alwaysAccentOutline = false;
    bool manualInteractionEnabled = false;
    bool manualPointerDown = false;
    bool manualDragActive = false;
    bool pointerDown = false;
    bool dragActive = false;
    bool pressHighlight = false;
    bool cancelClickOnLeave = false;
    bool pressCanceled = false;
    juce::Justification textJustification = juce::Justification::centred;
    ArrowDirection arrowDirection = ArrowDirection::none;
    bool borderVisible = true;
    bool leadingDotVisible = false;
    juce::Colour leadingDotColour;
    float leadingDotLevel = 0.0f;
    std::function<void()> longPressAction;
    int longPressDelayMs = 500;
    bool longPressEligible = false;
    bool longPressArmed = false;
    juce::String longPressOriginalText;
    juce::String longPressPromptText = "RESET?";

    void timerCallback() override;
};