#include "module.mxe.EditorControls.h"

#include "module.mxe.ModuleComponent.h"
#include "module.mxe.EditorTheme.h"
#include "module.mxe.HostParameterEditing.h"

#include <cmath>

namespace mxe::editor
{
namespace
{
class WheelForwardingTextEditor final : public juce::TextEditor
{
public:
    explicit WheelForwardingTextEditor(juce::Slider& sliderToControl)
        : juce::TextEditor(sliderToControl.getName())
    {
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        juce::TextEditor::mouseWheelMove(event, wheel);

        if (auto* owner = findParentComponentOfClass<MxeModuleComponent>())
            owner->scrollPageViewport(event, wheel);
    }

private:
};
} // namespace

class ValueBoxComponent final : public juce::Component
{
public:
    ValueBoxComponent(juce::Slider& sliderToControl,
                      juce::RangedAudioParameter* parameterToControl,
                      juce::UndoManager* undoManagerToUse,
                      ValueConstraint valueConstraintIn,
                      const bool allowGestureEditingIn,
                      const float dragNormalisedPerPixelIn,
                      const float wheelStepMultiplierIn)
        : slider(sliderToControl),
          parameter(parameterToControl),
          undoManager(undoManagerToUse),
          valueConstraint(std::move(valueConstraintIn))
    {
        juce::ignoreUnused(allowGestureEditingIn, dragNormalisedPerPixelIn, wheelStepMultiplierIn);
        setMouseCursor(juce::MouseCursor::IBeamCursor);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    ~ValueBoxComponent() override
    {
        stopGlobalEditTracking();
    }

    void setOutlineColour(const juce::Colour colour)
    {
        if (outlineColour == colour)
            return;

        outlineColour = colour;

        if (editor != nullptr)
            editor->setColour(juce::TextEditor::outlineColourId, outlineColour);

        repaint();
    }

    void setHighlightColour(const juce::Colour colour)
    {
        if (highlightColour == colour)
            return;

        highlightColour = colour;

        if (editor != nullptr)
            editor->setColour(juce::TextEditor::highlightColourId, highlightColour);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.setColour(uiGrey800);
        graphics.fillRect(getLocalBounds());

        graphics.setColour(outlineColour);
        graphics.drawRect(getLocalBounds(), frameLineThickness);

        graphics.setColour(uiWhite);
        graphics.setFont(makeUiFont());
        graphics.drawFittedText(formatValueBoxText(slider.getValue()),
                                getLocalBounds().reduced(4, 0),
                                juce::Justification::centred,
                                1,
                                1.0f);
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds(getLocalBounds());
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (auto* owner = findParentComponentOfClass<MxeModuleComponent>())
            owner->scrollPageViewport(event, wheel);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        auto* clickedComponent = event.originalComponent;
        const auto clickIsInsideThisValueBox = clickedComponent != nullptr
            && (clickedComponent == this || isParentOf(clickedComponent));

        if (editor != nullptr && ! clickIsInsideThisValueBox)
        {
            hideEditor(false);
            return;
        }

        if (! clickIsInsideThisValueBox)
            return;

        if (! event.mods.isLeftButtonDown() || event.mods.isPopupMenu())
            return;

        pointerDown = true;
        dragDetected = false;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (! pointerDown)
            return;

        if (! dragDetected && event.mouseWasDraggedSinceMouseDown())
            dragDetected = true;
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (! pointerDown)
            return;

        const auto shouldOpenEditor = contains(event.getPosition())
            && ! dragDetected
            && ! event.mods.isPopupMenu();

        pointerDown = false;
        dragDetected = false;

        if (shouldOpenEditor)
            showEditor();
    }

private:
    void showEditor()
    {
        if (editor != nullptr)
            return;

        if (auto* owner = findParentComponentOfClass<MxeModuleComponent>())
        {
            const auto editorText = formatValueBoxText(slider.getValue());
            auto safeThis = juce::Component::SafePointer<ValueBoxComponent>(this);

            owner->showTextPrompt(editorText,
                                  [safeThis] (const juce::String& enteredText)
                                  {
                                      if (safeThis == nullptr)
                                          return false;

                                      safeThis->applyEnteredText(enteredText);
                                      return true;
                                  });
            return;
        }

        auto textEditor = std::make_unique<WheelForwardingTextEditor>(slider);
        textEditor->setFont(makeUiFont());
        textEditor->setPopupMenuEnabled(true);
        textEditor->setJustification(juce::Justification::centred);
        textEditor->setColour(juce::TextEditor::textColourId, uiWhite);
        textEditor->setColour(juce::TextEditor::backgroundColourId, uiGrey800);
        textEditor->setColour(juce::TextEditor::outlineColourId, outlineColour);
        textEditor->setColour(juce::TextEditor::focusedOutlineColourId, outlineColour);
        textEditor->setColour(juce::TextEditor::highlightColourId, highlightColour);
        textEditor->setColour(juce::TextEditor::highlightedTextColourId, uiWhite);
        textEditor->setText(formatValueBoxText(slider.getValue()), false);
        textEditor->onReturnKey = [this] { hideEditor(false); };
        textEditor->onEscapeKey = [this] { hideEditor(true); };
        textEditor->onFocusLost = [this] { hideEditor(false); };

        addAndMakeVisible(*textEditor);
        editor = std::move(textEditor);
        startGlobalEditTracking();
        resized();
        editor->grabKeyboardFocus();
        editor->selectAll();
    }

    void applyEnteredText(const juce::String& enteredText)
    {
        if (parameter == nullptr)
            return;

        const auto enteredValue = slider.getValueFromText(enteredText.trim());
        const auto clampedValue = juce::jlimit(static_cast<double>(slider.getMinimum()),
                                               static_cast<double>(slider.getMaximum()),
                                               enteredValue);
        const auto legalValue = parameter->getNormalisableRange().snapToLegalValue(constrainValue(static_cast<float>(clampedValue)));

        const auto normalisedValue = parameter->convertTo0to1(legalValue);

        if (! setNormalisedParameterValueForHost(*parameter, normalisedValue, undoManager))
            return;

        slider.setValue(legalValue, juce::dontSendNotification);

        if (auto* parent = getParentComponent())
            parent->repaint();

        repaint();
    }

    void hideEditor(const bool discard)
    {
        if (editor == nullptr)
            return;

        if (! discard && parameter != nullptr)
            applyEnteredText(editor->getText());

        removeChildComponent(editor.get());
        editor.reset();
        stopGlobalEditTracking();
        repaint();
    }

    void startGlobalEditTracking()
    {
        if (isTrackingGlobalClicks)
            return;

        juce::Desktop::getInstance().addGlobalMouseListener(this);
        isTrackingGlobalClicks = true;
    }

    void stopGlobalEditTracking()
    {
        if (! isTrackingGlobalClicks)
            return;

        juce::Desktop::getInstance().removeGlobalMouseListener(this);
        isTrackingGlobalClicks = false;
    }

    float constrainValue(const float value) const
    {
        if (valueConstraint)
            return valueConstraint(value);

        return value;
    }

    juce::Slider& slider;
    juce::RangedAudioParameter* parameter = nullptr;
    juce::UndoManager* undoManager = nullptr;
    ValueConstraint valueConstraint;
    juce::Colour outlineColour = uiGrey500;
    juce::Colour highlightColour = uiAccent;
    bool isTrackingGlobalClicks = false;
    bool pointerDown = false;
    bool dragDetected = false;
    std::unique_ptr<WheelForwardingTextEditor> editor;
};

BoxTextButton::BoxTextButton(const juce::Colour accent)
    : accentColour(accent)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

void BoxTextButton::setTextJustification(const juce::Justification justification)
{
    textJustification = justification;
    repaint();
}

void BoxTextButton::setLongPressAction(std::function<void()> action, const int delayMs, juce::String confirmationText)
{
    longPressAction = std::move(action);
    longPressDelayMs = juce::jmax(1, delayMs);
    longPressConfirmationText = confirmationText.isNotEmpty() ? std::move(confirmationText)
                                                              : juce::String { "RESET?" };
}

void BoxTextButton::paintButton(juce::Graphics& graphics, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto fill = pressHighlight ? uiGrey700 : uiGrey800;
    const auto outline = getToggleState() ? accentColour : uiGrey500;

    graphics.setColour(fill);
    graphics.fillRect(getLocalBounds());

    graphics.setColour(outline);
    graphics.drawRect(getLocalBounds(), frameLineThickness);

    graphics.setColour(uiWhite);
    graphics.setFont(makeUiFont());
    graphics.drawFittedText(getButtonText(), getLocalBounds().reduced(6, 0), textJustification, 1, 1.0f);
}

void BoxTextButton::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() || ! event.mods.isLeftButtonDown())
        return;

    pointerDown = true;
    dragActive = false;
    pressHighlight = true;
    longPressEligible = longPressAction != nullptr;
    longPressArmed = false;
    longPressOriginalText = getButtonText();

    if (longPressEligible)
        startTimer(longPressDelayMs);
    else
        stopTimer();

    repaint();
}

void BoxTextButton::mouseDrag(const juce::MouseEvent& event)
{
    if (! pointerDown)
        return;

    if (! dragActive && event.getDistanceFromDragStart() >= 4)
    {
        dragActive = true;
        longPressEligible = false;
        stopTimer();
    }

    const auto shouldHighlight = contains(event.getPosition());

    if (pressHighlight != shouldHighlight)
    {
        pressHighlight = shouldHighlight;
        repaint();
    }

    if (! shouldHighlight && ! longPressArmed)
    {
        longPressEligible = false;
        stopTimer();
    }
}

void BoxTextButton::mouseUp(const juce::MouseEvent& event)
{
    if (! pointerDown)
        return;

    stopTimer();

    const auto wasLongPressArmed = longPressArmed;
    pointerDown = false;
    dragActive = false;
    pressHighlight = false;
    longPressEligible = false;
    longPressArmed = false;
    repaint();

    if (wasLongPressArmed)
    {
        if (getButtonText() != longPressOriginalText)
            setButtonText(longPressOriginalText);

        repaint();

        if (contains(event.getPosition()) && longPressAction != nullptr)
            longPressAction();

        return;
    }

    if (contains(event.getPosition()))
        triggerClick();
}

void BoxTextButton::mouseExit(const juce::MouseEvent&)
{
    if (! pointerDown || ! pressHighlight)
        return;

    pressHighlight = false;

    if (! longPressArmed)
    {
        longPressEligible = false;
        stopTimer();
    }

    repaint();
}

void BoxTextButton::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (auto* owner = findParentComponentOfClass<MxeModuleComponent>())
        owner->scrollPageViewport(event, wheel);
}

void BoxTextButton::timerCallback()
{
    stopTimer();

    if (! pointerDown || ! pressHighlight || ! longPressEligible || dragActive)
        return;

    longPressEligible = false;
    longPressArmed = true;
    longPressOriginalText = getButtonText();
    setButtonText(longPressConfirmationText);
    repaint();
}

ParameterControl::ParameterControl(juce::AudioProcessorValueTreeState& state,
                                   const juce::String& parameterId,
                                   const ControlSpec& spec,
                                   const juce::Colour accent,
                                   ValueConstraint valueConstraintIn,
                                   std::function<void()> onSoloClickIn,
                                   std::function<bool()> isSoloActiveIn,
                                   std::function<bool()> isSoloEnabledIn)
    : isToggle(spec.isToggle),
      hasSoloButton(spec.isToggle && juce::String(spec.suffix) == "delTa" && onSoloClickIn != nullptr),
      accentColour(accent),
      parameter(dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(parameterId))),
      undoManager(state.undoManager),
      valueConstraint(std::move(valueConstraintIn)),
      onSoloClick(std::move(onSoloClickIn)),
      isSoloActive(std::move(isSoloActiveIn)),
      isSoloEnabled(std::move(isSoloEnabledIn)),
      toggle(accent),
      soloButton(accent),
      title(accent)
{
    jassert(parameter != nullptr);

    if (isToggle)
    {
        toggle.setButtonText(spec.label);
        toggle.setClickingTogglesState(true);
        buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            state,
            parameterId,
            toggle);

        addAndMakeVisible(toggle);

        if (hasSoloButton)
        {
            soloButton.setButtonText("SOLO");
            soloButton.setClickingTogglesState(false);
            soloButton.onClick = [this]
            {
                if (onSoloClick)
                    onSoloClick();

                refreshExternalState();
            };
            addAndMakeVisible(soloButton);
            refreshExternalState();
        }
    }
    else
    {
        title.setButtonText(spec.label);
        title.setTextJustification(juce::Justification::centred);
        title.onClick = [] {};
        title.setLongPressAction([this]
        {
            resetParameterToDefault();
        });

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setInterceptsMouseClicks(false, false);
        slider.setAlpha(0.0f);
        slider.onValueChange = [this]
        {
            if (valueBox != nullptr)
                valueBox->repaint();
        };
        slider.textFromValueFunction = [] (const double value)
        {
            return formatValueBoxText(value);
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.trim().getDoubleValue();
        };
        slider.setColour(juce::Slider::thumbColourId, uiBlack);
        slider.setColour(juce::Slider::trackColourId, uiBlack);
        slider.setColour(juce::Slider::backgroundColourId, uiBlack);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, uiGrey800);
        slider.setColour(juce::Slider::textBoxTextColourId, uiWhite);
        slider.setColour(juce::Slider::textBoxOutlineColourId, uiGrey500);
        slider.setColour(juce::Slider::textBoxHighlightColourId, accentColour);

        if (parameter != nullptr)
            slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));

        sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            state,
            parameterId,
            slider);

        valueBox = std::make_unique<ValueBoxComponent>(slider,
                                                       parameter,
                                                       undoManager,
                                                       valueConstraint,
                                                       ! spec.valueInputOnly,
                                                       spec.dragNormalisedPerPixel,
                                                       spec.wheelMultiplier);
        applyCurrentAppearance();

        addAndMakeVisible(title);
        addAndMakeVisible(slider);
        addAndMakeVisible(*valueBox);
    }

    applyCurrentAppearance();
}

ParameterControl::~ParameterControl() = default;

int ParameterControl::getPreferredHeight() const noexcept
{
    if (hasSoloButton)
        return (buttonHeight * 2) + sectionRowGap;

    return buttonHeight;
}

void ParameterControl::setControlEnabled(const bool shouldBeEnabled)
{
    setEnabled(shouldBeEnabled);
    setAlpha(shouldBeEnabled ? 1.0f : 0.45f);
}

void ParameterControl::refreshExternalState()
{
    if (! hasSoloButton)
        return;

    const auto enabled = isSoloEnabled == nullptr || isSoloEnabled();
    soloButton.setEnabled(enabled);
    soloButton.setAlpha(enabled ? 1.0f : 0.45f);
    soloButton.setToggleState(enabled && isSoloActive != nullptr && isSoloActive(), juce::dontSendNotification);
}

void ParameterControl::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (auto* owner = findParentComponentOfClass<MxeModuleComponent>())
        owner->scrollPageViewport(event, wheel);
}

void ParameterControl::resized()
{
    auto bounds = getLocalBounds();
    const auto titleWidth = getScaledParameterNameWidth(bounds.getWidth());

    if (isToggle)
    {
        if (hasSoloButton)
        {
            soloButton.setBounds(bounds.removeFromTop(buttonHeight));
            bounds.removeFromTop(sectionRowGap);
            toggle.setBounds(bounds.removeFromTop(buttonHeight));
            refreshExternalState();
            return;
        }

        toggle.setBounds(bounds);
        return;
    }

    auto row = bounds;
    auto titleBounds = row.removeFromLeft(titleWidth);
    row.removeFromLeft(parameterGap);
    auto valueBounds = row;

    title.setBounds(titleBounds);
    slider.setBounds(valueBounds);

    if (valueBox != nullptr)
        valueBox->setBounds(valueBounds);
}

void ParameterControl::applyCurrentAppearance()
{
    if (isToggle)
        return;

    const auto outline = uiGrey500;
    const auto highlight = accentColour;
    juce::ignoreUnused(outline);

    if (valueBox != nullptr)
    {
        valueBox->setOutlineColour(uiGrey500);
        valueBox->setHighlightColour(highlight);
    }
}

void ParameterControl::resetParameterToDefault()
{
    if (parameter == nullptr)
        return;

    auto defaultValue = parameter->convertFrom0to1(parameter->getDefaultValue());

    if (valueConstraint)
        defaultValue = valueConstraint(defaultValue);

    const auto normalisedValue = parameter->convertTo0to1(defaultValue);

    if (! setNormalisedParameterValueForHost(*parameter, normalisedValue, undoManager))
        return;

    slider.setValue(defaultValue, juce::dontSendNotification);

    if (valueBox != nullptr)
        valueBox->repaint();
}
} // namespace mxe::editor
