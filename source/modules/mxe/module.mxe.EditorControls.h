#pragma once

#include "module.mxe.EditorControlSpecs.h"

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace mxe::editor
{
class ValueBoxComponent;

class BoxTextButton final : public juce::TextButton, private juce::Timer
{
public:
    explicit BoxTextButton(juce::Colour accent);

    void setTextJustification(juce::Justification justification);
    void setLongPressAction(std::function<void()> action, int delayMs = 500, juce::String confirmationText = "RESET?");
    void paintButton(juce::Graphics& graphics, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    juce::Colour accentColour;
    juce::Justification textJustification = juce::Justification::centred;
    bool pointerDown = false;
    bool dragActive = false;
    bool pressHighlight = false;
    std::function<void()> longPressAction;
    int longPressDelayMs = 500;
    bool longPressEligible = false;
    bool longPressArmed = false;
    juce::String longPressOriginalText;
    juce::String longPressConfirmationText { "RESET?" };

    void timerCallback() override;
};

class ParameterControl final : public juce::Component
{
public:
    ParameterControl(juce::AudioProcessorValueTreeState& state,
                     const juce::String& parameterId,
                     const ControlSpec& spec,
                     juce::Colour accent,
                     ValueConstraint valueConstraintIn = {},
                     std::function<void()> onSoloClickIn = {},
                     std::function<bool()> isSoloActiveIn = {},
                     std::function<bool()> isSoloEnabledIn = {});
    ~ParameterControl() override;

    int getPreferredHeight() const noexcept;
    void setControlEnabled(bool shouldBeEnabled);
    void refreshExternalState();
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void resized() override;

private:
    void applyCurrentAppearance();
    void resetParameterToDefault();

    const bool isToggle = false;
    const bool hasSoloButton = false;
    const juce::Colour accentColour;
    juce::RangedAudioParameter* parameter = nullptr;
    juce::UndoManager* undoManager = nullptr;
    ValueConstraint valueConstraint;
    std::function<void()> onSoloClick;
    std::function<bool()> isSoloActive;
    std::function<bool()> isSoloEnabled;
    BoxTextButton toggle;
    BoxTextButton soloButton;
    BoxTextButton title;
    juce::Slider slider;
    std::unique_ptr<ValueBoxComponent> valueBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
};
} // namespace mxe::editor
