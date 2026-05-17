#pragma once

#include "EditorStyle.h"

#include <functional>
#include <memory>

class NoTickComboBox final : public juce::ComboBox
{
public:
    using juce::ComboBox::ComboBox;

    NoTickComboBox();

    void showPopup() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent&) override;

    void setPopupMenuTextJustification(juce::Justification justification) noexcept;
    juce::Justification getPopupMenuTextJustification() const noexcept;
    void setPromptStylePopupEnabled(bool shouldEnable) noexcept;
    bool isPromptStylePopupEnabled() const noexcept;
    void setChoiceEnabled(int choiceIndex, bool shouldEnable);
    bool isPressedHighlightEnabled() const noexcept;

    std::function<void()> onReselectedCurrentItem;

private:
    juce::Justification popupMenuTextJustification = juce::Justification::centred;
    bool pointerDown = false;
    bool dragDetected = false;
    bool pressHighlight = false;
    bool promptStylePopupEnabled = false;
};

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
    ValueBoxComponent(juce::Slider& sliderToControl, juce::RangedAudioParameter* parameterToControl);
    ~ValueBoxComponent() override;

    void setInteractionEnabled(bool shouldEnable);
    void setOutlineColour(juce::Colour colour);
    void setHighlightColour(juce::Colour colour);
    void setPromptActive(bool shouldBeActive);

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
    void showEditor();
    void applyEnteredText(const juce::String& enteredText);
    void hideEditor(bool discard);
    void startGlobalEditTracking();
    void stopGlobalEditTracking();

    juce::Slider& slider;
    juce::RangedAudioParameter* parameter = nullptr;
    bool isTrackingGlobalClicks = false;
    bool interactionEnabled = true;
    bool pointerDown = false;
    bool dragDetected = false;
    bool pressHighlight = false;
    bool promptActive = false;
    juce::Colour outlineColour = uiGrey500;
    juce::Colour highlightColour = uiAccent;
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

class EqeAudioProcessorEditor::EqeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EqeLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
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
    void drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) override;
    int getCallOutBoxBorderSize(const juce::CallOutBox&) override;
    float getCallOutBoxCornerSize(const juce::CallOutBox&) override;
    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
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
