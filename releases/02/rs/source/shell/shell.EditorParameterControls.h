#pragma once

#include "../shared/shared.UiParameterControls.h"
#include "shell.EditorControls.h"

#include <functional>
#include <memory>
#include <vector>

class ChoiceControl final : public juce::Component
{
public:
    using Attachment = juce::ParameterAttachment;

    ChoiceControl(juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterIdIn,
                  const juce::String& titleText,
                  std::vector<int> displayOrderIn = {});

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

    int getPreferredHeight() const noexcept;
    double getValue() const noexcept;
    void setValue(double value, bool sendNotification);
    void setOverrideText(const juce::String& text);
    void clearOverrideText();
    void setInteractionEnabled(bool shouldEnable);
    void setTitleBorderVisible(bool shouldShow);
    void setTitleMouseEnabled(bool shouldEnable);
    void setTitleLongPressAction(std::function<void()> action, int delayMs = 500);
    void setTextToValueParser(std::function<double(const juce::String&)> parser);
    void resized() override;

    std::function<void()> onValueChanged;

private:
    juce::String formatDisplayValue(double value) const;
    juce::String formatEditorValue() const;

    const double defaultValue = 0.0;
    const int editorDecimals = 2;
    const bool brickwSupported = false;
    const bool supportsNoteText = false;
    std::unique_ptr<BoxTextButton> titleButton;
    juce::Slider slider;
    std::unique_ptr<ValueBoxComponent> valueBox;
    juce::String overrideText;
    std::function<double(const juce::String&)> textToValueParser;
};
