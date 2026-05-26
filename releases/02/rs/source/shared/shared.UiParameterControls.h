#pragma once

#include "shared.UiControls.h"

#include <functional>
#include <memory>

class ParameterControl final : public juce::Component
{
public:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    ParameterControl(juce::AudioProcessorValueTreeState& state,
                     const juce::String& parameterIdIn,
                     const juce::String& titleText,
                     int editorDecimalsIn);
    ~ParameterControl() override;

    int getPreferredHeight() const noexcept;
    void detach() noexcept;
    void rebind(juce::AudioProcessorValueTreeState& state);
    void setValue(double value, bool sendNotification);
    void setOverrideText(const juce::String& text);
    void clearOverrideText();
    void setInteractionEnabled(bool shouldEnable);
    void setValueClickAction(std::function<void()> action);
    void setTitleWidthOverride(int width) noexcept;
    juce::Rectangle<int> getValueBounds() const noexcept;
    void setTitleLongPressAction(std::function<void()> action, int delayMs = 500);
    void resized() override;

    std::function<void()> onValueChanged;
    std::function<void()> onTitleClick;

private:
    juce::String formatDisplayValue(double value) const;
    juce::String formatEditorValue() const;

    juce::String parameterId;
    juce::RangedAudioParameter* parameter = nullptr;
    const int editorDecimals = 2;
    std::unique_ptr<BoxTextButton> titleButton;
    juce::Slider slider;
    std::unique_ptr<ValueBoxComponent> valueBox;
    std::unique_ptr<Attachment> attachment;
    juce::String overrideText;
    int titleWidthOverride = -1;
};