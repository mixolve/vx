#include "shell.EditorFilterSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

#include <array>

void VxAudioProcessorEditor::layoutEqeModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    if (! bounds.isEmpty())
        bounds.removeFromBottom(viewportToPotentiometerGap);

    juce::Rectangle<int> presetsBounds;

    if (presetsSection != nullptr)
    {
        const auto presetHeight = presetsSection->getPresetRowPreferredHeight();
        presetsBounds = bounds.removeFromBottom(juce::jmin(bounds.getHeight(), presetHeight));

        if (! bounds.isEmpty())
            bounds.removeFromBottom(verticalGap);
    }

    auto actionRowBounds = bounds.removeFromBottom(rowHeight);
    actionRowBounds.removeFromLeft(editorInsetX);
    actionRowBounds.removeFromRight(editorInsetX);

    std::array<BoxTextButton*, 4> actionButtons {
        addFilterButton.get(),
        sortPlaceButton.get(),
        sortFreqButton.get(),
        sortDuoButton.get()
    };

    auto remainingActionBounds = actionRowBounds;
    const auto actionButtonCount = static_cast<int>(actionButtons.size());
    const auto actionTotalGap = parameterGap * (actionButtonCount - 1);
    const auto actionBaseWidth = juce::jmax(0, (remainingActionBounds.getWidth() - actionTotalGap) / actionButtonCount);

    for (int buttonIndex = 0; buttonIndex < actionButtonCount; ++buttonIndex)
    {
        auto* button = actionButtons[static_cast<size_t>(buttonIndex)];

        if (button == nullptr)
            continue;

        const auto isLastButton = buttonIndex + 1 == actionButtonCount;
        auto buttonBounds = isLastButton ? remainingActionBounds
                                         : remainingActionBounds.removeFromLeft(actionBaseWidth);
        button->setBounds(buttonBounds);

        if (! isLastButton)
            remainingActionBounds.removeFromLeft(parameterGap);
    }

    if (! bounds.isEmpty())
        bounds.removeFromBottom(globalToFilterGap);

    filterViewport.setBounds(bounds);
    filterViewport.setVisible(true);
    filterContent.setSize(bounds.getWidth(), juce::jmax(bounds.getHeight(), getFilterContentHeight()));

    auto contentBounds = filterContent.getLocalBounds();

    const auto activeFilterCount = getActiveFilterCount();

    for (int displayIndex = 0; displayIndex < activeFilterCount; ++displayIndex)
    {
        const auto filterIndex = getFilterIndexForOrderPosition(displayIndex);

        if (filterIndex < 0)
            continue;

        auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section == nullptr)
            continue;

        auto headerBounds = contentBounds.removeFromTop(rowHeight);
        headerBounds.removeFromLeft(editorInsetX);
        headerBounds.removeFromRight(editorInsetX);

        auto moveUpBounds = headerBounds.removeFromLeft(rowHeight);
        headerBounds.removeFromLeft(parameterGap);
        auto bypassBounds = headerBounds.removeFromLeft(rowHeight);
        headerBounds.removeFromLeft(parameterGap);
        auto moveDownBounds = headerBounds.removeFromRight(rowHeight);
        headerBounds.removeFromRight(parameterGap);

        section->moveUpButton->setBounds(moveUpBounds);
        section->bypassButton->setBounds(bypassBounds);
        section->header->setBounds(headerBounds);
        section->moveDownButton->setBounds(moveDownBounds);

        if (! contentBounds.isEmpty())
            contentBounds.removeFromTop(verticalGap);

        if (! section->expanded)
            continue;

        auto placeFilterControl = [&contentBounds, editorInsetX] (auto& control)
        {
            auto controlBounds = contentBounds.removeFromTop(control.getPreferredHeight());
            controlBounds.removeFromLeft(editorInsetX);
            controlBounds.removeFromRight(editorInsetX);
            control.setBounds(controlBounds);

            if (! contentBounds.isEmpty())
                contentBounds.removeFromTop(verticalGap);
        };

        placeFilterControl(*section->typeControl);
        placeFilterControl(*section->placeControl);
        placeFilterControl(*section->slopeControl);
        placeFilterControl(*section->frequencyControl);
        placeFilterControl(*section->bandwidthControl);
        placeFilterControl(*section->gainControl);
    }

    if (presetsSection != nullptr)
    {
        auto presetContentBounds = presetsBounds;

        auto presetNameRowBounds = presetContentBounds.removeFromTop(rowHeight);
        presetNameRowBounds.removeFromLeft(editorInsetX);
        presetNameRowBounds.removeFromRight(editorInsetX);
        presetsSection->presetCombo.setBounds(presetNameRowBounds);

        if (! presetContentBounds.isEmpty())
            presetContentBounds.removeFromTop(verticalGap);

        auto presetButtonRowBounds = presetContentBounds.removeFromTop(rowHeight);
        presetButtonRowBounds.removeFromLeft(editorInsetX);
        presetButtonRowBounds.removeFromRight(editorInsetX);

        const auto presetButtonCount = 5;
        const auto totalGapWidth = presetRowGap * (presetButtonCount - 1);
        const auto availableButtonWidth = juce::jmax(0, presetButtonRowBounds.getWidth() - totalGapWidth);
        const auto baseButtonWidth = availableButtonWidth / presetButtonCount;
        const auto buttonWidthRemainder = availableButtonWidth % presetButtonCount;

        auto placePresetButton = [&presetButtonRowBounds, baseButtonWidth, buttonWidthRemainder] (BoxTextButton& button, const int index)
        {
            const auto buttonWidth = baseButtonWidth + (index < buttonWidthRemainder ? 1 : 0);
            auto buttonBounds = presetButtonRowBounds.removeFromLeft(buttonWidth);
            button.setBounds(buttonBounds);

            if (index + 1 < presetButtonCount)
                presetButtonRowBounds.removeFromLeft(presetRowGap);
        };

        placePresetButton(*presetsSection->adButton, 0);
        placePresetButton(*presetsSection->saveButton, 1);
        placePresetButton(*presetsSection->renameButton, 2);
        placePresetButton(*presetsSection->defaultButton, 3);
        placePresetButton(*presetsSection->deleteButton, 4);
    }

}
