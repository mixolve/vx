#include "shell.EditorParameterControls.h"

void VxAudioProcessorEditor::setupShellControls()
{
    auto resetGlobalClip = [this]
    {
        audioProcessor.resetGlobalClipIndicator();
        lastClipIndicatorTimeMs = 0;

        if (clipButton != nullptr)
            clipButton->clearTextColourOverride();

        clearKeyboardFocus(*this);
    };

    if (clipButton != nullptr)
        clipButton->setLongPressAction(resetGlobalClip, 500);

    globalBypassButton = std::make_unique<BoxTextButton>(uiAccent);
    globalBypassButton->setButtonText("B");
    globalBypassButton->setTooltip("CLICK: BYPASS -- LONG PRESS: CLOSE MODULE");
    globalBypassButton->setTextJustification(juce::Justification::centred);
    globalBypassButton->setClickingTogglesState(true);
    globalBypassAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                                 VxAudioProcessor::paramGlobalBypassId,
                                                                 *globalBypassButton);
    globalBypassButton->onClickWithModifiers = [this] (const juce::ModifierKeys& modifiers)
    {
        if (! modifiers.isCtrlDown())
            return false;

        if (auto* parameter = valueTreeState.getParameter(VxAudioProcessor::paramGlobalBypassId))
            return handleHostSlotAssignRequest(VxAudioProcessor::paramGlobalBypassId, "B", parameter->getValue());

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

    clearFiltersButton = std::make_unique<BoxTextButton>(uiAccent);
    clearFiltersButton->setButtonText("DL");
    clearFiltersButton->setTooltip("DELETE ALL FILTERS");
    clearFiltersButton->setTextJustification(juce::Justification::centred);
    clearFiltersButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    clearFiltersButton->setLongPressAction([this]
    {
        clearAllFilters();
        clearKeyboardFocus(*this);
    }, 500, "SURE?");
    addAndMakeVisible(*clearFiltersButton);

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

    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotButtons.size()); ++slotIndex)
    {
        auto slotButton = std::make_unique<BoxTextButton>(uiGrey500);
        slotButton->setTextJustification(juce::Justification::centred);
        slotButton->setButtonText("SLOT-" + VxAudioProcessor::getHostSlotLetterLabel(slotIndex));
        slotButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        slotButton->setLongPressAction([this, slotIndex]
        {
            clearHostSlot(slotIndex);
            clearKeyboardFocus(*this);
        }, 500, "CLEAR?");
        hostParametersContent.addAndMakeVisible(*slotButton);
        hostSlotButtons[static_cast<size_t>(slotIndex)] = std::move(slotButton);
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

void VxAudioProcessorEditor::updateTooltipTogglePrompt()
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

void VxAudioProcessorEditor::clearHostSlot(const int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, static_cast<int>(hostSlotAssignments.size())))
        return;

    auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];
    assignment.parameterId.clear();
    assignment.parameterName.clear();

    if (auto* slotParameter = valueTreeState.getParameter(VxAudioProcessor::getHostSlotParameterId(slotIndex));
        slotParameter != nullptr)
    {
        slotParameter->beginChangeGesture();
        slotParameter->setValueNotifyingHost(0.0f);
        slotParameter->endChangeGesture();
    }

    refreshHostSlotButtons();
    storeEditorStateToValueTree();
}

void VxAudioProcessorEditor::refreshHostSlotButtons()
{
    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
    {
        auto* slotButton = hostSlotButtons[static_cast<size_t>(slotIndex)].get();

        if (slotButton == nullptr)
            continue;

        const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

        if (assignment.parameterId.isEmpty())
            slotButton->setButtonText("SLOT-" + VxAudioProcessor::getHostSlotLetterLabel(slotIndex));
        else
            slotButton->setButtonText(assignment.parameterName.isNotEmpty() ? assignment.parameterName
                                                                             : assignment.parameterId);
    }
}

bool VxAudioProcessorEditor::handleHostSlotAssignRequest(const juce::String& parameterId,
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

    if (assignment.parameterName.isEmpty())
        assignment.parameterName = trimmedParameterId;

    if (auto* slotParameter = valueTreeState.getParameter(VxAudioProcessor::getHostSlotParameterId(targetSlot));
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
