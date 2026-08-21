#include "shell.EditorParameterControls.h"
#include "shell.Editor.h"

#include <utility>

namespace
{
const juce::Colour parameterTitleFocusColour { 0xFF99CC99 };
juce::Component::SafePointer<AvaAudioProcessorEditor> focusedParameterOwner;
BoxTextButton* focusedParameterTitleButton = nullptr;
juce::Slider* focusedParameterValueSlider = nullptr;

AvaAudioProcessorEditor* findFocusOwner(juce::Component* component) noexcept
{
    if (component == nullptr)
        return nullptr;

    if (auto* editor = dynamic_cast<AvaAudioProcessorEditor*>(component))
        return editor;

    return component->findParentComponentOfClass<AvaAudioProcessorEditor>();
}

bool focusBelongsTo(juce::Component& owner) noexcept
{
    return focusedParameterOwner.getComponent() != nullptr
        && focusedParameterOwner.getComponent() == findFocusOwner(&owner);
}

void focusParameterTitleButton(BoxTextButton* button, juce::Slider* valueSlider)
{
    auto* owner = findFocusOwner(button);

    if (button == nullptr || valueSlider == nullptr)
    {
        shell_parameter_focus::clearFocus();
        return;
    }

    if (owner == nullptr)
    {
        shell_parameter_focus::clearFocus();
        return;
    }

    if (focusedParameterTitleButton != nullptr && focusedParameterTitleButton != button)
        focusedParameterTitleButton->setAlwaysAccentOutline(false);

    focusedParameterOwner = owner;
    focusedParameterTitleButton = button;
    focusedParameterValueSlider = valueSlider;
    focusedParameterTitleButton->setAlwaysAccentOutline(true);
}

bool isFocusedParameterTitleButton(const BoxTextButton* button, const juce::Slider* valueSlider) noexcept
{
    return button != nullptr
        && valueSlider != nullptr
        && focusedParameterTitleButton == button
        && focusedParameterValueSlider == valueSlider;
}

void clearFocusedParameterTitleButton(BoxTextButton* button, juce::Slider* valueSlider)
{
    if (valueSlider != nullptr && focusedParameterValueSlider == valueSlider)
        focusedParameterValueSlider = nullptr;

    if (button == nullptr || focusedParameterTitleButton != button)
        return;

    focusedParameterTitleButton->setAlwaysAccentOutline(false);
    focusedParameterTitleButton = nullptr;
    focusedParameterOwner = nullptr;
    focusedParameterValueSlider = nullptr;
}

bool canUseFocusedPotentiometer(juce::RangedAudioParameter* parameter, const std::function<void()>& valueClickAction) noexcept
{
    return parameter != nullptr
        && valueClickAction == nullptr
        && dynamic_cast<juce::AudioParameterChoice*>(parameter) == nullptr;
}

bool assignParameterTitleToHostSlot(juce::Component& source,
                                    BoxTextButton* titleButton,
                                    const juce::String& parameterId,
                                    juce::RangedAudioParameter* parameter,
                                    const juce::ModifierKeys& modifiers)
{
    if (! modifiers.isCtrlDown() || parameter == nullptr)
        return false;

    if (auto* owner = source.findParentComponentOfClass<AvaAudioProcessorEditor>())
    {
        return owner->handleHostSlotAssignRequest(parameterId,
                                                  titleButton != nullptr ? titleButton->getButtonText() : parameterId,
                                                  parameter->getValue());
    }

    return false;
}
}

juce::Slider* shell_parameter_focus::getFocusedValueSlider(juce::Component& owner) noexcept
{
    return focusBelongsTo(owner) ? focusedParameterValueSlider : nullptr;
}

void shell_parameter_focus::clearFocus() noexcept
{
    if (focusedParameterTitleButton != nullptr)
        focusedParameterTitleButton->setAlwaysAccentOutline(false);

    focusedParameterTitleButton = nullptr;
    focusedParameterOwner = nullptr;
    focusedParameterValueSlider = nullptr;
}

void shell_parameter_focus::clearFocus(juce::Component& owner) noexcept
{
    if (focusBelongsTo(owner))
        clearFocus();
}

void shell_parameter_focus::clearFocusIfNotShowing(juce::Component& owner) noexcept
{
    if (! focusBelongsTo(owner))
        return;

    if (focusedParameterTitleButton == nullptr && focusedParameterValueSlider == nullptr)
        return;

    // Numeric controls use an internal non-visible slider; visibility is driven by the title button.
    if (focusedParameterTitleButton != nullptr)
    {
        if (focusedParameterTitleButton->isShowing())
            return;

        shell_parameter_focus::clearFocus();
        return;
    }

    if (focusedParameterValueSlider != nullptr && focusedParameterValueSlider->isShowing())
        return;

    shell_parameter_focus::clearFocus();
}

ParameterControl::ParameterControl(juce::AudioProcessorValueTreeState& state,
                                   const juce::String& parameterIdIn,
                                   const juce::String& titleText,
                                   const int editorDecimalsIn)
    : parameterId(parameterIdIn),
      parameter(state.getParameter(parameterIdIn)),
      editorDecimals(editorDecimalsIn)
{
    jassert(parameter != nullptr);

    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    titleButton = std::make_unique<BoxTextButton>(parameterTitleFocusColour);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centredLeft);
    titleButton->setClearsParameterFocusOnMouseDown(false);
    titleButton->onClickWithModifiers = [this] (const juce::ModifierKeys& modifiers)
    {
        return assignParameterTitleToHostSlot(*this, titleButton.get(), parameterId, parameter, modifiers);
    };
    titleButton->onClick = [this]
    {
        if (isFocusedParameterTitleButton(titleButton.get(), &slider))
            return;

        if (valueClickAction != nullptr)
        {
            shell_parameter_focus::clearFocus(*this);
            clearKeyboardFocus(*this);
            return;
        }

        if (interactionEnabled && canUseFocusedPotentiometer(parameter, valueClickAction))
            focusParameterTitleButton(titleButton.get(), &slider);
        else
            shell_parameter_focus::clearFocus(*this);

        clearKeyboardFocus(*this);
    };
    titleButton->setLongPressAction([this]
    {
        if (! interactionEnabled)
        {
            clearFocusedParameterTitleButton(titleButton.get(), &slider);
            clearKeyboardFocus(*this);
            return;
        }

        if (onTitleClick)
        {
            onTitleClick();
        }
        else if (parameter != nullptr)
        {
            slider.setValue(parameter->convertFrom0to1(parameter->getDefaultValue()),
                            juce::sendNotificationSync);
        }

        clearKeyboardFocus(*this);
    });

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setInterceptsMouseClicks(false, false);
    slider.setScrollWheelEnabled(false);
    slider.setAlpha(0.0f);
    slider.setWantsKeyboardFocus(false);
    slider.setMouseClickGrabsKeyboardFocus(false);
    slider.textFromValueFunction = [this] (const double value)
    {
        return formatDisplayValue(value);
    };
    slider.valueFromTextFunction = [this] (const juce::String& text)
    {
        if (text.trim().equalsIgnoreCase("MUTED"))
            return slider.getMinimum();

        if (customTextParser != nullptr)
            return customTextParser(text);

        if (auto* choiceParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter))
            return findNearestChoiceIndex(parseNumericInput(text), choiceParameter->choices, text);

        return supportsNoteFrequencyInput(parameterId) ? parseFrequencyInput(text)
                                                       : parseNumericInput(text);
    };
    slider.onValueChange = [this]
    {
        if (valueBox != nullptr)
            valueBox->repaint();

        repaint();

        if (auto* parent = getParentComponent())
            parent->repaint();

        if (onValueChanged)
            onValueChanged();
    };

    if (parameter != nullptr)
        slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));

    attachment = std::make_unique<Attachment>(state, parameterIdIn, slider);
    valueBox = std::make_unique<ValueBoxComponent>(slider);
    valueBox->displayTextProvider = [this]
    {
        return formatDisplayValue(slider.getValue());
    };
    valueBox->editorTextProvider = [this]
    {
        return formatEditorValue();
    };
    valueBox->onBeforeShowEditor = [this]
    {
        if (interactionEnabled && canUseFocusedPotentiometer(parameter, valueClickAction))
            focusParameterTitleButton(titleButton.get(), &slider);
        else
            clearFocusedParameterTitleButton(titleButton.get(), &slider);
    };
    valueBox->textToValueParser = [this] (const juce::String& text)
    {
        if (text.trim().equalsIgnoreCase("MUTED"))
            return slider.getMinimum();

        if (customTextParser != nullptr)
            return customTextParser(text);

        if (auto* choiceParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter))
            return findNearestChoiceIndex(parseNumericInput(text), choiceParameter->choices, text);

        return supportsNoteFrequencyInput(parameterId) ? parseFrequencyInput(text)
                                                       : parseNumericInput(text);
    };
    valueBox->setOutlineColour(uiGrey500);
    valueBox->setHighlightColour(uiAccent);

    addAndMakeVisible(*titleButton);
    addChildComponent(slider);
    addAndMakeVisible(*valueBox);
}

ParameterControl::~ParameterControl()
{
    clearFocusedParameterTitleButton(titleButton.get(), &slider);
}

int ParameterControl::getPreferredHeight() const noexcept
{
    return rowHeight;
}

double ParameterControl::getValue() const noexcept
{
    return slider.getValue();
}

void ParameterControl::detach() noexcept
{
    attachment.reset();
    parameter = nullptr;
}

void ParameterControl::rebind(juce::AudioProcessorValueTreeState& state)
{
    rebind(state, parameterId);
}

void ParameterControl::rebind(juce::AudioProcessorValueTreeState& state,
                              const juce::String& parameterIdIn)
{
    attachment.reset();
    parameterId = parameterIdIn;
    parameter = state.getParameter(parameterId);
    jassert(parameter != nullptr);

    if (parameter != nullptr)
    {
        const auto& range = parameter->getNormalisableRange();
        slider.setNormalisableRange({ static_cast<double>(range.start),
                                      static_cast<double>(range.end),
                                      static_cast<double>(range.interval),
                                      static_cast<double>(range.skew),
                                      range.symmetricSkew });
        slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    }

    if (! canUseFocusedPotentiometer(parameter, valueClickAction))
        clearFocusedParameterTitleButton(titleButton.get(), &slider);

    if (titleButton != nullptr)
        titleButton->setPressFillEnabled(interactionEnabled && canUseFocusedPotentiometer(parameter, valueClickAction));

    attachment = std::make_unique<Attachment>(state, parameterId, slider);
    repaint();
}

bool ParameterControl::isBoundTo(const juce::String& parameterIdIn) const noexcept
{
    return parameterId == parameterIdIn;
}

void ParameterControl::setValue(const double value, const bool sendNotification)
{
    slider.setValue(value, sendNotification ? juce::sendNotificationSync
                                            : juce::dontSendNotification);

    if (valueBox != nullptr)
        valueBox->repaint();

    repaint();

    if (auto* parent = getParentComponent())
        parent->repaint();
}

void ParameterControl::setOverrideText(const juce::String& text)
{
    if (overrideText == text)
        return;

    overrideText = text;

    if (valueBox != nullptr)
        valueBox->repaint();
}

void ParameterControl::clearOverrideText()
{
    if (overrideText.isEmpty())
        return;

    overrideText.clear();

    if (valueBox != nullptr)
        valueBox->repaint();
}

void ParameterControl::setInteractionEnabled(const bool shouldEnable)
{
    interactionEnabled = shouldEnable;

    if (! interactionEnabled || ! canUseFocusedPotentiometer(parameter, valueClickAction))
        clearFocusedParameterTitleButton(titleButton.get(), &slider);

    if (titleButton != nullptr)
    {
        titleButton->setEnabled(shouldEnable);
        titleButton->setPressFillEnabled(shouldEnable && canUseFocusedPotentiometer(parameter, valueClickAction));
    }

    if (valueBox != nullptr)
        valueBox->setInteractionEnabled(shouldEnable);
}

void ParameterControl::setValueClickAction(std::function<void()> action)
{
    valueClickAction = std::move(action);

    if (valueBox != nullptr)
        valueBox->setCustomPromptAction(valueClickAction);

    if (! canUseFocusedPotentiometer(parameter, valueClickAction))
        clearFocusedParameterTitleButton(titleButton.get(), &slider);

    if (titleButton != nullptr)
        titleButton->setPressFillEnabled(interactionEnabled && canUseFocusedPotentiometer(parameter, valueClickAction));
}

void ParameterControl::setValueRange(const double minimum, const double maximum, const double interval)
{
    if (std::abs(slider.getMinimum() - minimum) <= 1.0e-9
        && std::abs(slider.getMaximum() - maximum) <= 1.0e-9
        && std::abs(slider.getInterval() - interval) <= 1.0e-9)
    {
        return;
    }

    slider.setRange(minimum, maximum, interval);
}

void ParameterControl::setTitleWidthOverride(const int width) noexcept
{
    titleWidthOverride = width;
    resized();
}

void ParameterControl::setValueLeadingInset(const int width) noexcept
{
    valueLeadingInset = juce::jmax(0, width);
    resized();
}

void ParameterControl::setTitleText(const juce::String& text)
{
    if (titleButton == nullptr || titleButton->getButtonText() == text)
        return;

    titleButton->setButtonText(text);
    titleButton->repaint();
}

void ParameterControl::setValueTextTransform(std::function<juce::String(double)> displayFormatter,
                                             std::function<juce::String(double)> editorFormatter,
                                             std::function<double(const juce::String&)> textParser)
{
    customDisplayFormatter = std::move(displayFormatter);
    customEditorFormatter = std::move(editorFormatter);
    customTextParser = std::move(textParser);

    if (valueBox != nullptr)
        valueBox->repaint();

    repaint();
}

void ParameterControl::clearValueTextTransform()
{
    if (customDisplayFormatter == nullptr && customEditorFormatter == nullptr && customTextParser == nullptr)
        return;

    customDisplayFormatter = nullptr;
    customEditorFormatter = nullptr;
    customTextParser = nullptr;

    if (valueBox != nullptr)
        valueBox->repaint();

    repaint();
}

juce::Rectangle<int> ParameterControl::getValueBounds() const noexcept
{
    return valueBox != nullptr ? valueBox->getBounds() : juce::Rectangle<int>();
}

void ParameterControl::setTitleLongPressAction(std::function<void()> action, const int delayMs)
{
    if (titleButton != nullptr)
        titleButton->setLongPressAction(std::move(action), delayMs);
}

void ParameterControl::resized()
{
    auto row = getLocalBounds();
    const auto defaultTitleWidth = getScaledParameterNameWidth(row.getWidth());
    const auto titleWidth = juce::jlimit(0, row.getWidth(), titleWidthOverride >= 0 ? titleWidthOverride : defaultTitleWidth);
    titleButton->setBounds(row.removeFromLeft(titleWidth));
    if (titleWidth > 0)
        row.removeFromLeft(juce::jmin(parameterGap, row.getWidth()));
    row.removeFromLeft(juce::jmin(valueLeadingInset, row.getWidth()));
    slider.setBounds(row);

    if (valueBox != nullptr)
        valueBox->setBounds(row);
}

juce::String ParameterControl::formatDisplayValue(const double value) const
{
    if (overrideText.isNotEmpty())
        return overrideText;

    if (customDisplayFormatter != nullptr)
        return customDisplayFormatter(value);

    if (parameter == nullptr)
        return formatFixedDecimalValue(value, editorDecimals);

    if (dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr)
        return parameter->getText(parameter->convertTo0to1(static_cast<float>(value)), 64).trim();

    const auto parameterText = parameter->getText(parameter->convertTo0to1(static_cast<float>(value)), 64).trim();
    const auto numericText = parameterText.retainCharacters("0123456789+-.");

    if (numericText.isNotEmpty())
    {
        if (parameterText.containsChar('%'))
            return formatFixedDecimalValue(parseNumericInput(parameterText), editorDecimals) + "%";

        return formatFixedDecimalValue(parseNumericInput(parameterText), editorDecimals);
    }

    if (parameterText.isNotEmpty())
        return parameterText;

    return formatFixedDecimalValue(value, editorDecimals);
}

juce::String ParameterControl::formatEditorValue() const
{
    if (overrideText.isNotEmpty())
        return overrideText;

    if (customEditorFormatter != nullptr)
        return customEditorFormatter(slider.getValue());

    if (parameter != nullptr && dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr)
        return parameter->getText(parameter->convertTo0to1(static_cast<float>(slider.getValue())), 64).trim();

    if (parameter != nullptr)
    {
        const auto parameterText = parameter->getText(parameter->convertTo0to1(static_cast<float>(slider.getValue())), 64).trim();
        const auto numericText = parameterText.retainCharacters("0123456789+-.");

        if (numericText.isEmpty() && parameterText.isNotEmpty())
            return parameterText;
    }

    return formatFixedDecimalValue(slider.getValue(), editorDecimals);
}

ChoiceControl::ChoiceControl(juce::AudioProcessorValueTreeState& state,
                             const juce::String& parameterIdIn,
                             const juce::String& titleText,
                             std::vector<int> displayOrderIn)
    : parameterId(parameterIdIn),
      parameter(dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterIdIn)))
{
    jassert(parameter != nullptr);

    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    titleButton = std::make_unique<BoxTextButton>(parameterTitleFocusColour);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centredLeft);
    titleButton->onClickWithModifiers = [this] (const juce::ModifierKeys& modifiers)
    {
        return assignParameterTitleToHostSlot(*this, titleButton.get(), parameterId, parameter, modifiers);
    };
    titleButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    titleButton->setLongPressAction([this]
    {
        if (onTitleClick)
        {
            onTitleClick();
        }
        else if (parameter != nullptr)
        {
            auto& rangedParameter = static_cast<juce::RangedAudioParameter&>(*parameter);
            setSelectedChoiceIndex(juce::roundToInt(rangedParameter.convertFrom0to1(rangedParameter.getDefaultValue())),
                                   true);
        }

        clearKeyboardFocus(*this);
    });

    comboBox.setEditableText(false);
    comboBox.setJustificationType(juce::Justification::centred);
    comboBox.setColour(juce::ComboBox::backgroundColourId, uiGrey800);
    comboBox.setColour(juce::ComboBox::outlineColourId, uiGrey500);
    comboBox.setColour(juce::ComboBox::textColourId, uiWhite);
    comboBox.setColour(juce::ComboBox::arrowColourId, uiWhite);
    comboBox.setColour(juce::ComboBox::buttonColourId, uiGrey800);
    comboBox.setWantsKeyboardFocus(false);
    comboBox.setMouseClickGrabsKeyboardFocus(false);
    comboBox.setName(titleText);
    comboBox.setPromptStylePopupEnabled(true);

    if (parameter != nullptr)
    {
        choices = parameter->choices;

        if (displayOrderIn.empty())
        {
            displayOrderIn.reserve(static_cast<size_t>(choices.size()));

            for (int choiceIndex = 0; choiceIndex < choices.size(); ++choiceIndex)
                displayOrderIn.push_back(choiceIndex);
        }

        displayOrder = std::move(displayOrderIn);
        choiceToDisplayIndex.resize(static_cast<size_t>(choices.size()), 0);

        for (int displayIndex = 0; displayIndex < static_cast<int>(displayOrder.size()); ++displayIndex)
        {
            const auto choiceIndex = displayOrder[static_cast<size_t>(displayIndex)];

            if (! juce::isPositiveAndBelow(choiceIndex, choices.size()))
                continue;

            comboBox.addItem(choices[choiceIndex], displayIndex + 1);
            choiceToDisplayIndex[static_cast<size_t>(choiceIndex)] = displayIndex;
        }
    }

    comboBox.onChange = [this]
    {
        if (ignoreCallbacks || parameter == nullptr)
            return;

        const auto displayIndex = comboBox.getSelectedItemIndex();

        if (! juce::isPositiveAndBelow(displayIndex, static_cast<int>(displayOrder.size())))
            return;

        attachment->setValueAsCompleteGesture(static_cast<float>(displayOrder[static_cast<size_t>(displayIndex)]));

        if (onValueChanged)
            onValueChanged();
    };

    if (parameter != nullptr)
    {
        attachment = std::make_unique<Attachment>(*parameter,
                                                  [this] (float newValue)
                                                  {
                                                      const auto choiceIndex = juce::roundToInt(newValue);

                                                      if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
                                                          return;

                                                      const auto displayIndex = choiceToDisplayIndex[static_cast<size_t>(choiceIndex)];
                                                      const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
                                                      comboBox.setSelectedItemIndex(displayIndex, juce::dontSendNotification);
                                                  },
                                                  state.undoManager);
        attachment->sendInitialUpdate();
    }

    addAndMakeVisible(*titleButton);
    addAndMakeVisible(comboBox);
}

int ChoiceControl::getPreferredHeight() const noexcept
{
    return rowHeight;
}

ChoiceControl::~ChoiceControl()
{
    clearFocusedParameterTitleButton(titleButton.get(), nullptr);
}

void ChoiceControl::detach() noexcept
{
    attachment.reset();
    parameter = nullptr;
}

void ChoiceControl::rebind(juce::AudioProcessorValueTreeState& state)
{
    attachment.reset();
    parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId));
    jassert(parameter != nullptr);

    if (parameter == nullptr)
        return;

    if (displayOrder.empty())
    {
        displayOrder.reserve(static_cast<size_t>(parameter->choices.size()));

        for (int choiceIndex = 0; choiceIndex < parameter->choices.size(); ++choiceIndex)
            displayOrder.push_back(choiceIndex);
    }

    if (choices.size() != parameter->choices.size())
        choices = parameter->choices;

    choiceToDisplayIndex.assign(static_cast<size_t>(parameter->choices.size()), 0);

    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.clear(juce::dontSendNotification);

    for (int displayIndex = 0; displayIndex < static_cast<int>(displayOrder.size()); ++displayIndex)
    {
        const auto choiceIndex = displayOrder[static_cast<size_t>(displayIndex)];

        if (! juce::isPositiveAndBelow(choiceIndex, parameter->choices.size()))
            continue;

        comboBox.addItem(choices[choiceIndex], displayIndex + 1);
        choiceToDisplayIndex[static_cast<size_t>(choiceIndex)] = displayIndex;
    }

    attachment = std::make_unique<Attachment>(*parameter,
                                              [this] (float newValue)
                                              {
                                                  const auto choiceIndex = juce::roundToInt(newValue);

                                                  if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
                                                      return;

                                                  const auto displayIndex = choiceToDisplayIndex[static_cast<size_t>(choiceIndex)];
                                                  const juce::ScopedValueSetter<bool> scopedAttachmentIgnore(ignoreCallbacks, true);
                                                  comboBox.setSelectedItemIndex(displayIndex, juce::dontSendNotification);
                                              },
                                              state.undoManager);
    attachment->sendInitialUpdate();
    repaint();
}

int ChoiceControl::getSelectedChoiceIndex() const noexcept
{
    const auto displayIndex = comboBox.getSelectedItemIndex();

    if (! juce::isPositiveAndBelow(displayIndex, static_cast<int>(displayOrder.size())))
        return 0;

    return displayOrder[static_cast<size_t>(displayIndex)];
}

void ChoiceControl::setSelectedChoiceIndex(const int choiceIndex, const bool sendNotification)
{
    if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
        return;

    const auto displayIndex = choiceToDisplayIndex[static_cast<size_t>(choiceIndex)];
    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.setSelectedItemIndex(displayIndex,
                                  sendNotification ? juce::sendNotificationSync
                                                   : juce::dontSendNotification);

    if (sendNotification && attachment != nullptr)
        attachment->setValueAsCompleteGesture(static_cast<float>(choiceIndex));

    if (sendNotification && onValueChanged)
        onValueChanged();
}

void ChoiceControl::setChoices(const juce::StringArray& choicesIn)
{
    if (parameter == nullptr || choicesIn.size() != choices.size())
        return;

    const auto selectedChoiceIndex = getSelectedChoiceIndex();
    choices = choicesIn;

    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.clear(juce::dontSendNotification);

    for (int displayIndex = 0; displayIndex < static_cast<int>(displayOrder.size()); ++displayIndex)
    {
        const auto choiceIndex = displayOrder[static_cast<size_t>(displayIndex)];

        if (! juce::isPositiveAndBelow(choiceIndex, choices.size()))
            continue;

        comboBox.addItem(choices[choiceIndex], displayIndex + 1);
    }

    if (juce::isPositiveAndBelow(selectedChoiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
        comboBox.setSelectedItemIndex(choiceToDisplayIndex[static_cast<size_t>(selectedChoiceIndex)], juce::dontSendNotification);
}

void ChoiceControl::setChoiceEnabled(const int choiceIndex, const bool shouldEnable)
{
    if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
        return;

    const auto displayIndex = choiceToDisplayIndex[static_cast<size_t>(choiceIndex)];

    if (! juce::isPositiveAndBelow(displayIndex, static_cast<int>(displayOrder.size())))
        return;

    comboBox.setItemEnabled(displayIndex + 1, shouldEnable);
}

void ChoiceControl::setTitleMouseEnabled(const bool shouldEnable)
{
    if (titleButton != nullptr)
    {
        titleButton->setInterceptsMouseClicks(shouldEnable, shouldEnable);
        titleButton->setPressFillEnabled(shouldEnable);
    }
}

void ChoiceControl::setTitleLongPressAction(std::function<void()> action, const int delayMs)
{
    if (titleButton != nullptr)
        titleButton->setLongPressAction(std::move(action), delayMs);
}

void ChoiceControl::setInteractionEnabled(const bool shouldEnable)
{
    interactionEnabled = shouldEnable;

    if (titleButton != nullptr)
    {
        titleButton->setEnabled(shouldEnable);
        titleButton->setPressFillEnabled(shouldEnable);
    }

    comboBox.setEnabled(true);
    comboBox.setAlpha(1.0f);
    comboBox.setInterceptsMouseClicks(shouldEnable, shouldEnable);
    const auto displayText = overrideText.isNotEmpty() ? overrideText : comboBox.getText();
    comboBox.setColour(juce::ComboBox::textColourId,
                       shouldEnable ? getDisplayTextColour(displayText) : uiGrey500);
}

void ChoiceControl::setOverrideText(const juce::String& text)
{
    overrideText = text.trim().isNotEmpty() ? text.trim() : juce::String("OFF");
    comboBox.setColour(juce::ComboBox::textColourId, getDisplayTextColour(overrideText));

    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.setText(overrideText, juce::dontSendNotification);
}

void ChoiceControl::clearOverrideText()
{
    if (overrideText.isEmpty())
        return;

    overrideText.clear();
    comboBox.setColour(juce::ComboBox::textColourId, uiWhite);

    if (parameter == nullptr)
        return;

    const auto choiceIndex = parameter->getIndex();

    if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
        return;

    const auto displayIndex = choiceToDisplayIndex[static_cast<size_t>(choiceIndex)];
    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.setSelectedItemIndex(displayIndex, juce::dontSendNotification);
}

void ChoiceControl::resized()
{
    auto row = getLocalBounds();
    const auto titleWidth = getScaledParameterNameWidth(row.getWidth());
    titleButton->setBounds(row.removeFromLeft(titleWidth));
    row.removeFromLeft(parameterGap);
    comboBox.setBounds(row);
}

LocalChoiceControl::LocalChoiceControl(const juce::String& titleText,
                                       const juce::StringArray& choices,
                                       const int defaultChoiceIndexIn)
    : defaultChoiceIndex(defaultChoiceIndexIn)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    titleButton = std::make_unique<BoxTextButton>(parameterTitleFocusColour);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centredLeft);
    titleButton->setClearsParameterFocusOnMouseDown(false);
    titleButton->onClick = [] {};
    titleButton->setLongPressAction([this]
    {
        setSelectedChoiceIndex(defaultChoiceIndex, true);
    });

    comboBox.setEditableText(false);
    comboBox.setJustificationType(juce::Justification::centred);
    comboBox.setColour(juce::ComboBox::backgroundColourId, uiGrey800);
    comboBox.setColour(juce::ComboBox::outlineColourId, uiGrey500);
    comboBox.setColour(juce::ComboBox::textColourId, uiWhite);
    comboBox.setColour(juce::ComboBox::arrowColourId, uiWhite);
    comboBox.setColour(juce::ComboBox::buttonColourId, uiGrey800);
    comboBox.setWantsKeyboardFocus(false);
    comboBox.setMouseClickGrabsKeyboardFocus(false);
    comboBox.setName(titleText);
    comboBox.setPromptStylePopupEnabled(true);

    for (auto choiceIndex = 0; choiceIndex < choices.size(); ++choiceIndex)
        comboBox.addItem(choices[choiceIndex], choiceIndex + 1);

    comboBox.setSelectedItemIndex(juce::jlimit(0, juce::jmax(0, choices.size() - 1), defaultChoiceIndex),
                                  juce::dontSendNotification);
    comboBox.onChange = [this]
    {
        if (! ignoreCallbacks && onValueChanged)
            onValueChanged();
    };

    addAndMakeVisible(*titleButton);
    addAndMakeVisible(comboBox);
}

int LocalChoiceControl::getPreferredHeight() const noexcept
{
    return rowHeight;
}

int LocalChoiceControl::getSelectedChoiceIndex() const noexcept
{
    return juce::jmax(0, comboBox.getSelectedItemIndex());
}

void LocalChoiceControl::setSelectedChoiceIndex(const int choiceIndex, const bool sendNotification)
{
    const auto clampedChoiceIndex = juce::jlimit(0, juce::jmax(0, comboBox.getNumItems() - 1), choiceIndex);
    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.setSelectedItemIndex(clampedChoiceIndex, juce::dontSendNotification);

    if (sendNotification && onValueChanged)
        onValueChanged();
}

void LocalChoiceControl::resized()
{
    auto row = getLocalBounds();
    const auto titleWidth = getScaledParameterNameWidth(row.getWidth());
    titleButton->setBounds(row.removeFromLeft(titleWidth));
    row.removeFromLeft(parameterGap);
    comboBox.setBounds(row);
}

LocalParameterControl::LocalParameterControl(const juce::String& titleText,
                                             const int editorDecimalsIn,
                                             const double minimum,
                                             const double maximum,
                                             const double interval,
                                             const double defaultValueIn,
                                             const double skewCentre,
                                             const bool supportsBrickwText,
                                             const bool supportsNoteTextIn)
    : defaultValue(defaultValueIn),
      editorDecimals(editorDecimalsIn),
      brickwSupported(supportsBrickwText),
      supportsNoteText(supportsNoteTextIn)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    titleButton = std::make_unique<BoxTextButton>(parameterTitleFocusColour);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centredLeft);
    titleButton->setClearsParameterFocusOnMouseDown(false);
    titleButton->onClick = [this]
    {
        if (isFocusedParameterTitleButton(titleButton.get(), &slider))
            return;

        if (valueClickAction != nullptr)
        {
            shell_parameter_focus::clearFocus(*this);
            valueClickAction();
            clearKeyboardFocus(*this);
            return;
        }

        if (interactionEnabled && valueClickAction == nullptr)
            focusParameterTitleButton(titleButton.get(), &slider);
        else
            shell_parameter_focus::clearFocus(*this);

        clearKeyboardFocus(*this);
    };
    titleButton->setLongPressAction([this]
    {
        if (! interactionEnabled)
        {
            clearFocusedParameterTitleButton(titleButton.get(), &slider);
            clearKeyboardFocus(*this);
            return;
        }

        setValue(defaultValue, true);
        clearKeyboardFocus(*this);
    });

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setInterceptsMouseClicks(false, false);
    slider.setScrollWheelEnabled(false);
    slider.setAlpha(0.0f);
    slider.setWantsKeyboardFocus(false);
    slider.setMouseClickGrabsKeyboardFocus(false);

    juce::NormalisableRange<double> range { minimum, maximum, interval };
    if (skewCentre > 0.0)
        range.setSkewForCentre(skewCentre);
    slider.setNormalisableRange(range);
    slider.setDoubleClickReturnValue(true, defaultValue);
    slider.setValue(defaultValue, juce::dontSendNotification);
    slider.textFromValueFunction = [this] (const double value)
    {
        return formatDisplayValue(value);
    };
    slider.valueFromTextFunction = [this] (const juce::String& text)
    {
        if (brickwSupported && text.trim().containsIgnoreCase("brick"))
            return 96.1;

        return supportsNoteText ? parseFrequencyInput(text)
                                : parseNumericInput(text);
    };
    slider.onValueChange = [this]
    {
        if (valueBox != nullptr)
            valueBox->repaint();

        repaint();

        if (auto* parent = getParentComponent())
            parent->repaint();

        if (onValueChanged)
            onValueChanged();
    };

    valueBox = std::make_unique<ValueBoxComponent>(slider);
    valueBox->displayTextProvider = [this]
    {
        return formatDisplayValue(slider.getValue());
    };
    valueBox->editorTextProvider = [this]
    {
        return formatEditorValue();
    };
    valueBox->onBeforeShowEditor = [this]
    {
        if (interactionEnabled && valueClickAction == nullptr)
            focusParameterTitleButton(titleButton.get(), &slider);
        else
            clearFocusedParameterTitleButton(titleButton.get(), &slider);
    };
    valueBox->textToValueParser = [this] (const juce::String& text)
    {
        if (brickwSupported && text.trim().containsIgnoreCase("brick"))
            return 96.1;

        return supportsNoteText ? parseFrequencyInput(text)
                                : parseNumericInput(text);
    };
    valueBox->setOutlineColour(uiGrey500);
    valueBox->setHighlightColour(uiAccent);

    addAndMakeVisible(*titleButton);
    addChildComponent(slider);
    addAndMakeVisible(*valueBox);
}

int LocalParameterControl::getPreferredHeight() const noexcept
{
    return rowHeight;
}

LocalParameterControl::~LocalParameterControl()
{
    clearFocusedParameterTitleButton(titleButton.get(), &slider);
}

double LocalParameterControl::getValue() const noexcept
{
    return slider.getValue();
}

void LocalParameterControl::setValue(const double value, const bool sendNotification)
{
    slider.setValue(value, sendNotification ? juce::sendNotificationSync
                                            : juce::dontSendNotification);

    if (valueBox != nullptr)
        valueBox->repaint();

    repaint();

    if (auto* parent = getParentComponent())
        parent->repaint();
}

void LocalParameterControl::setValueRange(const double minimum,
                                           const double maximum,
                                           const double interval,
                                           const bool reversed)
{
    if (std::abs(slider.getMinimum() - minimum) <= 1.0e-9
        && std::abs(slider.getMaximum() - maximum) <= 1.0e-9
        && std::abs(slider.getInterval() - interval) <= 1.0e-9
        && valueRangeReversed == reversed)
    {
        return;
    }

    valueRangeReversed = reversed;

    if (! reversed)
    {
        slider.setRange(minimum, maximum, interval);
        return;
    }

    juce::NormalisableRange<double> range {
        minimum,
        maximum,
        [] (const double start, const double end, const double normalised)
        {
            return end - (normalised * (end - start));
        },
        [] (const double start, const double end, const double value)
        {
            return (end - juce::jlimit(start, end, value)) / (end - start);
        },
        [interval] (const double start, const double end, const double value)
        {
            const auto snapped = start + (std::round((value - start) / interval) * interval);
            return juce::jlimit(start, end, snapped);
        }
    };
    range.interval = interval;
    slider.setNormalisableRange(range);
}

void LocalParameterControl::setDefaultValue(const double value)
{
    defaultValue = slider.getNormalisableRange().snapToLegalValue(value);
    slider.setDoubleClickReturnValue(true, defaultValue);
}

void LocalParameterControl::setTitleText(const juce::String& text)
{
    if (titleButton == nullptr || titleButton->getButtonText() == text)
        return;

    titleButton->setButtonText(text);
    titleButton->repaint();
}

void LocalParameterControl::setOverrideText(const juce::String& text)
{
    if (overrideText == text)
        return;

    overrideText = text;

    if (valueBox != nullptr)
        valueBox->repaint();
}

void LocalParameterControl::clearOverrideText()
{
    if (overrideText.isEmpty())
        return;

    overrideText.clear();

    if (valueBox != nullptr)
        valueBox->repaint();
}

void LocalParameterControl::setInteractionEnabled(const bool shouldEnable)
{
    interactionEnabled = shouldEnable;

    if (! interactionEnabled || valueClickAction != nullptr)
        clearFocusedParameterTitleButton(titleButton.get(), &slider);

    if (titleButton != nullptr)
    {
        titleButton->setEnabled(shouldEnable);
        titleButton->setPressFillEnabled(shouldEnable);
    }

    if (valueBox != nullptr)
        valueBox->setInteractionEnabled(shouldEnable);
}

void LocalParameterControl::setValueClickAction(std::function<void()> action)
{
    valueClickAction = std::move(action);

    if (valueBox != nullptr)
        valueBox->setCustomPromptAction(valueClickAction);

    if (valueClickAction != nullptr)
        clearFocusedParameterTitleButton(titleButton.get(), &slider);
}

juce::Rectangle<int> LocalParameterControl::getValueBounds() const noexcept
{
    return valueBox != nullptr ? valueBox->getBounds() : juce::Rectangle<int>();
}

void LocalParameterControl::setTitleMouseEnabled(const bool shouldEnable)
{
    if (titleButton != nullptr)
        titleButton->setInterceptsMouseClicks(shouldEnable, shouldEnable);
}

void LocalParameterControl::setTitleLongPressAction(std::function<void()> action, const int delayMs)
{
    if (titleButton != nullptr)
        titleButton->setLongPressAction(std::move(action), delayMs);
}

void LocalParameterControl::resized()
{
    auto row = getLocalBounds();
    const auto titleWidth = getScaledParameterNameWidth(row.getWidth());
    titleButton->setBounds(row.removeFromLeft(titleWidth));
    row.removeFromLeft(parameterGap);
    slider.setBounds(row);

    if (valueBox != nullptr)
        valueBox->setBounds(row);
}

juce::String LocalParameterControl::formatDisplayValue(const double value) const
{
    if (overrideText.isNotEmpty())
        return overrideText;

    if (brickwSupported && value > 96.0)
        return "BRICKW";

    return formatFixedDecimalValue(value, editorDecimals);
}

juce::String LocalParameterControl::formatEditorValue() const
{
    if (overrideText.isNotEmpty())
        return overrideText;

    if (brickwSupported && slider.getValue() > 96.0)
        return "BRICKW";

    return formatFixedDecimalValue(slider.getValue(), editorDecimals);
}
