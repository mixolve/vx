#pragma once

#include "shell.UiParameterControls.h"
#include "shell.EditorControls.h"

#include <functional>
#include <memory>
#include <vector>

namespace shell_parameter_focus
{
juce::Slider* getFocusedValueSlider(juce::Component& owner) noexcept;
void clearFocus() noexcept;
void clearFocus(juce::Component& owner) noexcept;
void clearFocusIfNotShowing(juce::Component& owner) noexcept;
}

class ChoiceControl final : public juce::Component
{
public:
    using Attachment = juce::ParameterAttachment;

    ChoiceControl(juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterIdIn,
                  const juce::String& titleText,
                  std::vector<int> displayOrderIn = {});
    ~ChoiceControl() override;

    int getPreferredHeight() const noexcept;
    void detach() noexcept;
    void rebind(juce::AudioProcessorValueTreeState& state);
    int getSelectedChoiceIndex() const noexcept;
    void setSelectedChoiceIndex(int choiceIndex, bool sendNotification);
    void setChoices(const juce::StringArray& choicesIn);
    void setChoiceEnabled(int choiceIndex, bool shouldEnable);
    void setTitleMouseEnabled(bool shouldEnable);
    void setTitleLongPressAction(std::function<void()> action, int delayMs = 500);
    void setInteractionEnabled(bool shouldEnable);
    void setOverrideText(const juce::String&);
    void clearOverrideText();
    void resized() override;

    std::function<void()> onValueChanged;
    std::function<void()> onTitleClick;

private:
    juce::String parameterId;
    juce::AudioParameterChoice* parameter = nullptr;
    std::unique_ptr<BoxTextButton> titleButton;
    NoTickComboBox comboBox;
    std::unique_ptr<Attachment> attachment;
    juce::StringArray choices;
    std::vector<int> displayOrder;
    std::vector<int> choiceToDisplayIndex;
    bool ignoreCallbacks = false;
    juce::String overrideText;
    bool interactionEnabled = true;
};

class LocalChoiceControl final : public juce::Component
{
public:
    LocalChoiceControl(const juce::String& titleText,
                       const juce::StringArray& choices,
                       int defaultChoiceIndexIn);

    int getPreferredHeight() const noexcept;
    int getSelectedChoiceIndex() const noexcept;
    void setSelectedChoiceIndex(int choiceIndex, bool sendNotification);
    void resized() override;

    std::function<void()> onValueChanged;

private:
    const int defaultChoiceIndex = 0;
    std::unique_ptr<BoxTextButton> titleButton;
    NoTickComboBox comboBox;
    bool ignoreCallbacks = false;
};

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
