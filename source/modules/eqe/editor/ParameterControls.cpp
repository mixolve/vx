#include "EditorParameterControls.h"

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

    titleButton = std::make_unique<BoxTextButton>(uiGrey500);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centred);
    titleButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    titleButton->setLongPressAction([this]
    {
        if (onTitleClick)
            onTitleClick();

        clearKeyboardFocus(*this);
    });

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setInterceptsMouseClicks(false, false);
    slider.setAlpha(0.0f);
    slider.setWantsKeyboardFocus(false);
    slider.setMouseClickGrabsKeyboardFocus(false);
    slider.textFromValueFunction = [this] (const double value)
    {
        return formatDisplayValue(value);
    };
    slider.valueFromTextFunction = [this] (const juce::String& text)
    {
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
    valueBox = std::make_unique<ValueBoxComponent>(slider, parameter);
    valueBox->displayTextProvider = [this]
    {
        return formatDisplayValue(slider.getValue());
    };
    valueBox->editorTextProvider = [this]
    {
        return formatEditorValue();
    };
    valueBox->textToValueParser = [this] (const juce::String& text)
    {
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

ParameterControl::~ParameterControl() = default;

int ParameterControl::getPreferredHeight() const noexcept
{
    return rowHeight;
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
    if (valueBox != nullptr)
        valueBox->setInteractionEnabled(shouldEnable);
}

void ParameterControl::setTitleLongPressAction(std::function<void()> action, const int delayMs)
{
    if (titleButton != nullptr)
        titleButton->setLongPressAction(std::move(action), delayMs);
}

void ParameterControl::resized()
{
    auto row = getLocalBounds();
    const auto titleWidth = getScaledParameterNameWidth(row.getWidth());
    titleButton->setBounds(row.removeFromLeft(titleWidth));
    row.removeFromLeft(parameterGap);
    slider.setBounds(row);

    if (valueBox != nullptr)
        valueBox->setBounds(row);
}

juce::String ParameterControl::formatDisplayValue(const double value) const
{
    if (overrideText.isNotEmpty())
        return overrideText;

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

    return formatFixedDecimalValue(value, editorDecimals);
}

juce::String ParameterControl::formatEditorValue() const
{
    if (overrideText.isNotEmpty())
        return overrideText;

    if (parameter != nullptr && dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr)
        return parameter->getText(parameter->convertTo0to1(static_cast<float>(slider.getValue())), 64).trim();

    if (parameter != nullptr)
    {
        const auto parameterText = parameter->getText(parameter->convertTo0to1(static_cast<float>(slider.getValue())), 64).trim();
    }

    return formatFixedDecimalValue(slider.getValue(), editorDecimals);
}

ChoiceControl::ChoiceControl(juce::AudioProcessorValueTreeState& state,
                             const juce::String& parameterIdIn,
                             const juce::String& titleText,
                             std::vector<int> displayOrderIn)
    : parameter(dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterIdIn)))
{
    jassert(parameter != nullptr);

    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    titleButton = std::make_unique<BoxTextButton>(uiGrey500);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centred);
    titleButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    titleButton->setLongPressAction([this]
    {
        if (onTitleClick)
            onTitleClick();

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
        titleButton->setInterceptsMouseClicks(shouldEnable, shouldEnable);
}

void ChoiceControl::setTitleLongPressAction(std::function<void()> action, const int delayMs)
{
    if (titleButton != nullptr)
        titleButton->setLongPressAction(std::move(action), delayMs);
}

void ChoiceControl::setInteractionEnabled(const bool shouldEnable)
{
    interactionEnabled = shouldEnable;
    comboBox.setEnabled(true);
    comboBox.setAlpha(1.0f);
    comboBox.setInterceptsMouseClicks(shouldEnable, shouldEnable);
}

void ChoiceControl::setOverrideText(const juce::String&)
{
    overrideText = "OFF";
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
                                       const juce::StringArray& choicesIn,
                                       std::vector<int> displayOrderIn,
                                       const int initialChoiceIndex)
    : choices(choicesIn)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    titleButton = std::make_unique<BoxTextButton>(uiGrey500);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centred);
    titleButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    titleButton->setLongPressAction([this]
    {
        if (onTitleClick)
            onTitleClick();

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

    comboBox.onChange = [this]
    {
        if (ignoreCallbacks)
            return;

        if (onValueChanged)
            onValueChanged();
    };

    addAndMakeVisible(*titleButton);
    addAndMakeVisible(comboBox);
    setSelectedChoiceIndex(initialChoiceIndex, false);
}

int LocalChoiceControl::getPreferredHeight() const noexcept
{
    return rowHeight;
}

void LocalChoiceControl::setInteractionEnabled(const bool shouldEnable)
{
    interactionEnabled = shouldEnable;
    comboBox.setEnabled(true);
    comboBox.setAlpha(1.0f);
    comboBox.setInterceptsMouseClicks(shouldEnable, shouldEnable);
}

void LocalChoiceControl::setOverrideText(const juce::String&)
{
    overrideText = "OFF";
    comboBox.setColour(juce::ComboBox::textColourId, getDisplayTextColour(overrideText));

    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.setText(overrideText, juce::dontSendNotification);
}

void LocalChoiceControl::setChoices(const juce::StringArray& choicesIn)
{
    if (choicesIn.size() != choices.size())
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

    if (juce::isPositiveAndBelow(selectedChoiceIndex, static_cast<int>(displayOrder.size())))
        comboBox.setSelectedItemIndex(choiceToDisplayIndex[static_cast<size_t>(selectedChoiceIndex)], juce::dontSendNotification);
}

void LocalChoiceControl::setTitleLongPressAction(std::function<void()> action, const int delayMs)
{
    if (titleButton != nullptr)
        titleButton->setLongPressAction(std::move(action), delayMs);
}

void LocalChoiceControl::setChoiceEnabled(const int choiceIndex, const bool shouldEnable)
{
    if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
        return;

    const auto displayIndex = choiceToDisplayIndex[static_cast<size_t>(choiceIndex)];

    if (! juce::isPositiveAndBelow(displayIndex, static_cast<int>(displayOrder.size())))
        return;

    comboBox.setItemEnabled(displayIndex + 1, shouldEnable);
}

void LocalChoiceControl::setTitleBorderVisible(const bool shouldShow)
{
    if (titleButton != nullptr)
        titleButton->setBorderVisible(shouldShow);
}

void LocalChoiceControl::setTitleMouseEnabled(const bool shouldEnable)
{
    if (titleButton != nullptr)
        titleButton->setInterceptsMouseClicks(shouldEnable, shouldEnable);
}

void LocalChoiceControl::clearOverrideText()
{
    if (overrideText.isEmpty())
        return;

    overrideText.clear();
    comboBox.setColour(juce::ComboBox::textColourId, uiWhite);

    const auto displayIndex = comboBox.getSelectedItemIndex();

    if (! juce::isPositiveAndBelow(displayIndex, static_cast<int>(displayOrder.size())))
        return;

    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.setSelectedItemIndex(displayIndex, juce::dontSendNotification);
}

int LocalChoiceControl::getSelectedChoiceIndex() const noexcept
{
    const auto displayIndex = comboBox.getSelectedItemIndex();

    if (! juce::isPositiveAndBelow(displayIndex, static_cast<int>(displayOrder.size())))
        return 0;

    return displayOrder[static_cast<size_t>(displayIndex)];
}

void LocalChoiceControl::setSelectedChoiceIndex(const int choiceIndex, const bool sendNotification)
{
    if (! juce::isPositiveAndBelow(choiceIndex, static_cast<int>(choiceToDisplayIndex.size())))
        return;

    const auto displayIndex = choiceToDisplayIndex[static_cast<size_t>(choiceIndex)];
    const juce::ScopedValueSetter<bool> scopedIgnore(ignoreCallbacks, true);
    comboBox.setSelectedItemIndex(displayIndex,
                                  sendNotification ? juce::sendNotificationSync
                                                   : juce::dontSendNotification);

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

    titleButton = std::make_unique<BoxTextButton>(uiGrey500);
    titleButton->setButtonText(titleText);
    titleButton->setTextJustification(juce::Justification::centred);
    titleButton->setLongPressAction([this]
    {
        setValue(defaultValue, true);
        clearKeyboardFocus(*this);
    });

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setInterceptsMouseClicks(false, false);
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

    valueBox = std::make_unique<ValueBoxComponent>(slider, nullptr);
    valueBox->displayTextProvider = [this]
    {
        return formatDisplayValue(slider.getValue());
    };
    valueBox->editorTextProvider = [this]
    {
        return formatEditorValue();
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
    if (valueBox != nullptr)
        valueBox->setInteractionEnabled(shouldEnable);
}

void LocalParameterControl::setTitleBorderVisible(const bool shouldShow)
{
    if (titleButton != nullptr)
        titleButton->setBorderVisible(shouldShow);
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
