#include "shell.EditorParameterControls.h"

void AvaAudioProcessorEditor::setupShellControls()
{
    globalBypassButton = std::make_unique<BoxTextButton>(uiAccent);
    globalBypassButton->setButtonText("B");
    globalBypassButton->setTooltip("CLICK: BYPASS -- LONG PRESS: CLOSE MODULE");
    globalBypassButton->setTextJustification(juce::Justification::centred);
    globalBypassButton->setClickingTogglesState(true);
    globalBypassAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                                 AvaAudioProcessor::paramGlobalBypassId,
                                                                 *globalBypassButton);
    globalBypassButton->onClickWithModifiers = [this] (const juce::ModifierKeys& modifiers)
    {
        if (! modifiers.isCtrlDown())
            return false;

        if (auto* parameter = valueTreeState.getParameter(AvaAudioProcessor::paramGlobalBypassId))
            return handleHostSlotAssignRequest(AvaAudioProcessor::paramGlobalBypassId, "B", parameter->getValue());

        return false;
    };
    globalBypassButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    globalBypassButton->setLongPressAction([this]
    {
        closeActiveModule();
        clearKeyboardFocus(*this);
    }, 500, "C");
    addAndMakeVisible(*globalBypassButton);

    undoButton = std::make_unique<BoxTextButton>(uiGrey500);
    undoButton->setButtonText("U");
    undoButton->setTooltip("UNDO");
    undoButton->setTextJustification(juce::Justification::centred);
    undoButton->onClick = [this]
    {
        performUndo();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*undoButton);

    redoButton = std::make_unique<BoxTextButton>(uiGrey500);
    redoButton->setButtonText("R");
    redoButton->setTooltip("REDO");
    redoButton->setTextJustification(juce::Justification::centred);
    redoButton->onClick = [this]
    {
        performRedo();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*redoButton);

    abCompareButton = std::make_unique<BoxTextButton>(uiGrey500);
    abCompareButton->setButtonText("AB");
    abCompareButton->setTooltip("A/B COMPARE");
    abCompareButton->setTextJustification(juce::Justification::centred);
    abCompareButton->setClickingTogglesState(false);
    abCompareButton->setABCompareHighlightIndex(0);
    abCompareButton->onClickWithModifiers = [this] (const juce::ModifierKeys& modifiers)
    {
        if (! modifiers.isCtrlDown())
            return false;

        copyCurrentABStateToOtherSlot();
        return true;
    };
    abCompareButton->onClick = [this]
    {
        switchABState();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*abCompareButton);

    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotButtons.size()); ++slotIndex)
    {
        auto moveUpButton = std::make_unique<BoxTextButton>(uiGrey500);
        moveUpButton->setButtonText({});
        moveUpButton->setTooltip("MOVE HOST PARAMETER UP");
        moveUpButton->setArrowDirection(BoxTextButton::ArrowDirection::up);
        moveUpButton->setCancelClickOnLeave(true);
        moveUpButton->onClick = [this, slotIndex]
        {
            moveHostSlotAssignment(slotIndex, -1);
            clearKeyboardFocus(*this);
        };

        auto slotNameField = std::make_unique<BoxTextButton>(uiGrey500);
        slotNameField->setButtonText(AvaAudioProcessor::getHostSlotLetterLabel(slotIndex));
        slotNameField->setTextJustification(juce::Justification::centred);
        slotNameField->setClearsParameterFocusOnMouseDown(false);

        auto slotButton = std::make_unique<BoxTextButton>(uiGrey500);
        slotButton->setTextJustification(juce::Justification::centredLeft);
        slotButton->setButtonText({});
        slotButton->setPressFillEnabled(false);
        slotButton->setFillVisible(false);
        slotButton->setClearsParameterFocusOnMouseDown(false);
        slotButton->setInterceptsMouseClicks(false, false);

        auto moveDownButton = std::make_unique<BoxTextButton>(uiGrey500);
        moveDownButton->setButtonText({});
        moveDownButton->setTooltip("MOVE HOST PARAMETER DOWN");
        moveDownButton->setArrowDirection(BoxTextButton::ArrowDirection::down);
        moveDownButton->setCancelClickOnLeave(true);
        moveDownButton->onClick = [this, slotIndex]
        {
            moveHostSlotAssignment(slotIndex, 1);
            clearKeyboardFocus(*this);
        };

        hostParametersContent.addAndMakeVisible(*moveUpButton);
        hostParametersContent.addAndMakeVisible(*slotNameField);
        hostParametersContent.addAndMakeVisible(*slotButton);
        hostParametersContent.addAndMakeVisible(*moveDownButton);
        hostSlotMoveUpButtons[static_cast<size_t>(slotIndex)] = std::move(moveUpButton);
        hostSlotNameFields[static_cast<size_t>(slotIndex)] = std::move(slotNameField);
        hostSlotButtons[static_cast<size_t>(slotIndex)] = std::move(slotButton);
        hostSlotMoveDownButtons[static_cast<size_t>(slotIndex)] = std::move(moveDownButton);
    }

    sortPlaceButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortPlaceButton->setButtonText("SP");
    sortPlaceButton->setTooltip("SORT BY PLACE");
    sortPlaceButton->setTextJustification(juce::Justification::centred);
    sortPlaceButton->onClick = [this]
    {
        sortFilterSectionsByPlace();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*sortPlaceButton);

    sortFreqButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortFreqButton->setButtonText("SF");
    sortFreqButton->setTooltip("SORT BY FREQUENCY");
    sortFreqButton->setTextJustification(juce::Justification::centred);
    sortFreqButton->onClick = [this]
    {
        sortFilterSectionsByFrequency();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*sortFreqButton);

    sortDuoButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortDuoButton->setButtonText("SD");
    sortDuoButton->setTooltip("SORT BY PLACE AND FREQUENCY");
    sortDuoButton->setTextJustification(juce::Justification::centred);
    sortDuoButton->onClick = [this]
    {
        sortFilterSectionsByDuo();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*sortDuoButton);

}

void AvaAudioProcessorEditor::updateTooltipTogglePrompt()
{
    if (tooltipWindow != nullptr)
    {
        tooltipWindow->hideTip();
        tooltipWindow->setHintsEnabled(tooltipsEnabled);
        tooltipWindow->setHoverDelayMs(1500);
    }

    if (hostButton == nullptr)
        return;

    hostButton->setLongPressAction([this]
    {
        tooltipsEnabled = ! tooltipsEnabled;
        updateTooltipTogglePrompt();
        clearKeyboardFocus(*this);
    }, 500, tooltipsEnabled ? "OFF" : "ON");
}

void AvaAudioProcessorEditor::clearHostSlot(const int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, static_cast<int>(hostSlotAssignments.size())))
        return;

    auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];
    assignment.parameterId.clear();
    assignment.parameterName.clear();

    if (auto* slotParameter = valueTreeState.getParameter(AvaAudioProcessor::getHostSlotParameterId(slotIndex));
        slotParameter != nullptr)
    {
        slotParameter->beginChangeGesture();
        slotParameter->setValueNotifyingHost(0.0f);
        slotParameter->endChangeGesture();
    }

    refreshHostSlotButtons();
    storeEditorStateToValueTree();
}

void AvaAudioProcessorEditor::moveHostSlotAssignment(const int slotIndex, const int direction)
{
    const auto slotCount = static_cast<int>(hostSlotAssignments.size());

    if (! juce::isPositiveAndBelow(slotIndex, slotCount) || direction == 0)
        return;

    if (hostSlotAssignments[static_cast<size_t>(slotIndex)].parameterId.isEmpty())
        return;

    auto destinationIndex = slotIndex + (direction < 0 ? -1 : 1);

    while (juce::isPositiveAndBelow(destinationIndex, slotCount)
           && hostSlotAssignments[static_cast<size_t>(destinationIndex)].parameterId.isEmpty())
    {
        destinationIndex += direction < 0 ? -1 : 1;
    }

    if (! juce::isPositiveAndBelow(destinationIndex, slotCount))
        return;

    auto* sourceParameter = valueTreeState.getParameter(AvaAudioProcessor::getHostSlotParameterId(slotIndex));
    auto* destinationParameter = valueTreeState.getParameter(AvaAudioProcessor::getHostSlotParameterId(destinationIndex));

    if (sourceParameter == nullptr || destinationParameter == nullptr)
        return;

    const auto sourceValue = sourceParameter->getValue();
    const auto destinationValue = destinationParameter->getValue();
    std::swap(hostSlotAssignments[static_cast<size_t>(slotIndex)],
              hostSlotAssignments[static_cast<size_t>(destinationIndex)]);

    const juce::ScopedValueSetter<bool> syncGuard(suppressHostSlotAutomationSync, true);
    sourceParameter->beginChangeGesture();
    sourceParameter->setValueNotifyingHost(destinationValue);
    sourceParameter->endChangeGesture();
    destinationParameter->beginChangeGesture();
    destinationParameter->setValueNotifyingHost(sourceValue);
    destinationParameter->endChangeGesture();

    refreshHostSlotButtons();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::refreshHostSlotButtons()
{
    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
    {
        auto* slotNameField = hostSlotNameFields[static_cast<size_t>(slotIndex)].get();
        auto* slotButton = hostSlotButtons[static_cast<size_t>(slotIndex)].get();

        if (slotNameField == nullptr || slotButton == nullptr)
            continue;

        const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];
        const auto isAssigned = assignment.parameterId.isNotEmpty();
        auto hasAssignedSlotBefore = false;
        auto hasAssignedSlotAfter = false;

        for (auto otherIndex = 0; otherIndex < slotIndex; ++otherIndex)
            hasAssignedSlotBefore = hasAssignedSlotBefore
                || hostSlotAssignments[static_cast<size_t>(otherIndex)].parameterId.isNotEmpty();

        for (auto otherIndex = slotIndex + 1; otherIndex < static_cast<int>(hostSlotAssignments.size()); ++otherIndex)
            hasAssignedSlotAfter = hasAssignedSlotAfter
                || hostSlotAssignments[static_cast<size_t>(otherIndex)].parameterId.isNotEmpty();

        if (auto* moveUpButton = hostSlotMoveUpButtons[static_cast<size_t>(slotIndex)].get())
        {
            const auto canMoveUp = isAssigned && hasAssignedSlotBefore;
            moveUpButton->setEnabled(canMoveUp);
            moveUpButton->setAlpha(1.0f);
        }

        if (auto* moveDownButton = hostSlotMoveDownButtons[static_cast<size_t>(slotIndex)].get())
        {
            const auto canMoveDown = isAssigned && hasAssignedSlotAfter;
            moveDownButton->setEnabled(canMoveDown);
            moveDownButton->setAlpha(1.0f);
        }

        if (isAssigned)
        {
            slotNameField->setLongPressAction([this, slotIndex]
            {
                clearHostSlot(slotIndex);
                clearKeyboardFocus(*this);
            }, 500, "CL");
        }
        else
        {
            slotNameField->setLongPressAction({});
        }

        slotButton->setLongPressAction({});

        if (assignment.parameterId.isEmpty())
            slotButton->setButtonText({});
        else
        {
            auto parameterName = assignment.parameterName.isNotEmpty() ? assignment.parameterName
                                                                        : assignment.parameterId;

            if (auto* parameter = findHostAssignableParameter(assignment.parameterId))
            {
                const auto currentName = parameter->getName(256).trim();

                if (currentName.isNotEmpty())
                    parameterName = currentName;
            }

            slotButton->setButtonText(parameterName);
        }
    }
}

bool AvaAudioProcessorEditor::handleHostSlotAssignRequest(const juce::String& parameterId,
                                                         const juce::String& parameterName,
                                                         const float normalizedValue)
{
    const auto trimmedParameterId = parameterId.trim();

    if (trimmedParameterId.isEmpty())
        return false;

    auto targetSlot = -1;

    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
    {
        if (hostSlotAssignments[static_cast<size_t>(slotIndex)].parameterId == trimmedParameterId)
        {
            targetSlot = slotIndex;
            break;
        }
    }

    if (targetSlot < 0)
    {
        for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
        {
            if (hostSlotAssignments[static_cast<size_t>(slotIndex)].parameterId.isEmpty())
            {
                targetSlot = slotIndex;
                break;
            }
        }
    }

    if (targetSlot < 0)
        return false;

    auto& assignment = hostSlotAssignments[static_cast<size_t>(targetSlot)];
    assignment.parameterId = trimmedParameterId;
    assignment.parameterName = parameterName.trim();

    if (auto* parameter = findHostAssignableParameter(trimmedParameterId))
    {
        const auto currentName = parameter->getName(256).trim();

        if (currentName.isNotEmpty())
            assignment.parameterName = currentName;
    }

    if (assignment.parameterName.isEmpty())
        assignment.parameterName = trimmedParameterId;

    if (auto* slotParameter = valueTreeState.getParameter(AvaAudioProcessor::getHostSlotParameterId(targetSlot));
        slotParameter != nullptr)
    {
        const auto clampedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
        slotParameter->beginChangeGesture();
        slotParameter->setValueNotifyingHost(clampedValue);
        slotParameter->endChangeGesture();

        syncHostSlotAssignmentValue(targetSlot, clampedValue);
    }

    refreshHostSlotButtons();
    storeEditorStateToValueTree();
    if (hostParametersExpanded)
    {
        updateSectionStates();
        resized();
    }
    return true;
}
