#pragma once

#include "UiControls.h"

#include <functional>
#include <memory>

class LocalParameterControl final : public juce::Component
{
public:
    LocalParameterControl(const juce::String& titleText,
                          int editorDecimalsIn,
                          double minimum,
                          double maximum,
                          double interval,
                          double defaultValueIn,
                          double skewCentre = 0.0,
                          bool supportsBrickwText = false,
                          bool supportsNoteTextIn = false);
    ~LocalParameterControl() override;

    int getPreferredHeight() const noexcept;
    double getValue() const noexcept;
    void setValue(double value, bool sendNotification);
    void setValueRange(double minimum, double maximum, double interval, bool reversed = false);
    void setDefaultValue(double value);
    void setTitleText(const juce::String& text);
    void setOverrideText(const juce::String& text);
    void clearOverrideText();
    void setInteractionEnabled(bool shouldEnable);
    void setValueClickAction(std::function<void()> action);
    juce::Rectangle<int> getValueBounds() const noexcept;
    void setTitleMouseEnabled(bool shouldEnable);
    void setTitleLongPressAction(std::function<void()> action, int delayMs = 500);
    void resized() override;

    std::function<void()> onValueChanged;

private:
    juce::String formatDisplayValue(double value) const;
    juce::String formatEditorValue() const;

    double defaultValue = 0.0;
    const int editorDecimals = 2;
    const bool brickwSupported = false;
    const bool supportsNoteText = false;
    std::unique_ptr<BoxTextButton> titleButton;
    juce::Slider slider;
    std::unique_ptr<ValueBoxComponent> valueBox;
    juce::String overrideText;
    std::function<void()> valueClickAction;
    bool interactionEnabled = true;
    bool valueRangeReversed = false;
};
