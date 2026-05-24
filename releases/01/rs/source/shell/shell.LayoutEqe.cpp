#include "shell.EditorBellSection.h"
#include "shell.Constants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::layoutEqeModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToFooterGap);

    auto presetsHeight = rowHeight;
    if (presetsExpanded)
    {
        presetsHeight += verticalGap;
        presetsHeight += presetsSection->getPresetRowPreferredHeight();
    }

    auto visualizerSectionHeight = rowHeight;
    if (visualizerExpanded)
    {
        visualizerSectionHeight += verticalGap;
        visualizerSectionHeight += visualizerRangeLowControl->getPreferredHeight()
                                + verticalGap
                                + visualizerRangeHighControl->getPreferredHeight()
                                + verticalGap
                                + rowHeight
                                + verticalGap
                                + rowHeight
                                + verticalGap
                                + rowHeight;
    }

    auto visualizerSectionBounds = bounds.removeFromBottom(visualizerSectionHeight);

    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToPresetsGap);

    auto presetsBounds = bounds.removeFromBottom(presetsHeight);

    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToPresetsGap);

    if (filtersExpanded && eqeModuleLoaded)
    {
        auto addButtonBounds = bounds.removeFromBottom(rowHeight);
        addButtonBounds.removeFromLeft(editorInsetX);
        addButtonBounds.removeFromRight(editorInsetX);
        addFilterButton->setBounds(addButtonBounds);

        if (! bounds.isEmpty())
            bounds.removeFromBottom(globalToFilterGap);
    }
    else
    {
        addFilterButton->setBounds({});
    }

    auto placeHeader = [&bounds, editorInsetX] (BoxTextButton& header)
    {
        auto headerBounds = bounds.removeFromTop(rowHeight);
        headerBounds.removeFromLeft(editorInsetX);
        headerBounds.removeFromRight(editorInsetX);
        header.setBounds(headerBounds);

        if (! bounds.isEmpty())
            bounds.removeFromTop(verticalGap);
    };

    placeHeader(*globalHeader);

    if (globalExpanded)
    {
        const auto minimumBelowGlobal = rowHeight + verticalGap + globalToFilterGap;
        const auto globalContentHeight = getGlobalContentHeight();
        const auto viewportHeight = juce::jmin(juce::jmax(0, bounds.getHeight() - minimumBelowGlobal),
                                              globalContentHeight);
        auto globalViewportBounds = bounds.removeFromTop(viewportHeight);
        globalViewport.setBounds(globalViewportBounds);
        globalContent.setSize(globalViewportBounds.getWidth(), juce::jmax(globalViewportBounds.getHeight(), globalContentHeight));

        auto globalBounds = globalContent.getLocalBounds();

        auto placeGlobalButton = [&globalBounds, editorInsetX] (BoxTextButton& button)
        {
            auto buttonBounds = globalBounds.removeFromTop(rowHeight);
            buttonBounds.removeFromLeft(editorInsetX);
            buttonBounds.removeFromRight(editorInsetX);
            button.setBounds(buttonBounds);

            if (! globalBounds.isEmpty())
                globalBounds.removeFromTop(verticalGap);
        };

        placeGlobalButton(*moduleCloseButton);
        placeGlobalButton(*eqeBypassButton);
        placeGlobalButton(*eqeBypassWithGainButton);
        placeGlobalButton(*clearFiltersButton);
        placeGlobalButton(*sortPlaceButton);
        placeGlobalButton(*sortFreqButton);
        placeGlobalButton(*sortDuoButton);

        if (! bounds.isEmpty())
            bounds.removeFromTop(globalToFilterGap);
    }
    else
    {
        globalViewport.setBounds({});
        globalContent.setSize(0, 0);
    }

    placeHeader(*filtersHeader);

    filterViewport.setBounds(filtersExpanded ? bounds : juce::Rectangle<int>());
    filterContent.setSize(bounds.getWidth(), getFilterContentHeight());
    auto filterBounds = filterContent.getLocalBounds();

    const auto activeBellCount = getActiveBellCount();

    for (int displayIndex = 0; displayIndex < activeBellCount; ++displayIndex)
    {
        const auto bellIndex = getBellIndexForOrderPosition(displayIndex);

        if (bellIndex < 0)
            continue;

        auto& section = *bellSections[static_cast<size_t>(bellIndex)];
        auto placeBellHeader = [&filterBounds, editorInsetX] (BellSection& bellSection)
        {
            auto headerBounds = filterBounds.removeFromTop(rowHeight);
            headerBounds.removeFromLeft(editorInsetX);
            headerBounds.removeFromRight(editorInsetX);

            auto moveUpBounds = headerBounds.removeFromLeft(rowHeight);
            headerBounds.removeFromLeft(parameterGap);
            auto moveDownBounds = headerBounds.removeFromRight(rowHeight);
            headerBounds.removeFromRight(parameterGap);

            bellSection.moveUpButton->setBounds(moveUpBounds);
            bellSection.header->setBounds(headerBounds);
            bellSection.moveDownButton->setBounds(moveDownBounds);

            if (! filterBounds.isEmpty())
                filterBounds.removeFromTop(verticalGap);
        };

        auto placeFilterControl = [&filterBounds, editorInsetX] (auto& control)
        {
            auto controlBounds = filterBounds.removeFromTop(control.getPreferredHeight());
            controlBounds.removeFromLeft(editorInsetX);
            controlBounds.removeFromRight(editorInsetX);
            control.setBounds(controlBounds);

            if (! filterBounds.isEmpty())
                filterBounds.removeFromTop(verticalGap);
        };

        auto placeFilterActionButtons = [&filterBounds, editorInsetX] (BoxTextButton& leftButton, BoxTextButton& rightButton)
        {
            auto rowBounds = filterBounds.removeFromTop(rowHeight);
            rowBounds.removeFromLeft(editorInsetX);
            rowBounds.removeFromRight(editorInsetX);

            const auto leftWidth = getScaledParameterNameWidth(rowBounds.getWidth());
            auto leftBounds = rowBounds.removeFromLeft(leftWidth);
            rowBounds.removeFromLeft(parameterGap);
            leftButton.setBounds(leftBounds);
            rightButton.setBounds(rowBounds);

            if (! filterBounds.isEmpty())
                filterBounds.removeFromTop(verticalGap);
        };

        placeBellHeader(section);

        if (filtersExpanded && ! globalExpanded && ! presetsExpanded && expandedBellIndex == bellIndex)
        {
            placeFilterControl(*section.typeControl);
            placeFilterControl(*section.ttssControl);
            placeFilterControl(*section.lrmsControl);
            if (section.slopeControl->isVisible())
                placeFilterControl(*section.slopeControl);
            placeFilterControl(*section.frequencyControl);
            if (section.bandwidthControl->isVisible())
                placeFilterControl(*section.bandwidthControl);
            placeFilterControl(*section.gainControl);
            placeFilterActionButtons(*section.bypassButton, *section.deleteButton);
        }
    }

    auto presetsContentBounds = presetsBounds;
    auto presetsHeaderBounds = presetsContentBounds.removeFromTop(rowHeight);
    presetsHeaderBounds.removeFromLeft(editorInsetX);
    presetsHeaderBounds.removeFromRight(editorInsetX);
    presetsSection->header->setBounds(presetsHeaderBounds);

    if (presetsExpanded)
    {
        if (! presetsContentBounds.isEmpty())
            presetsContentBounds.removeFromTop(verticalGap);

        auto presetRowsBounds = presetsContentBounds.removeFromTop(presetsSection->getPresetRowPreferredHeight());

        auto presetNameRowBounds = presetRowsBounds.removeFromTop(rowHeight);
        presetNameRowBounds.removeFromLeft(editorInsetX);
        presetNameRowBounds.removeFromRight(editorInsetX);
        presetsSection->presetCombo.setBounds(presetNameRowBounds);

        if (! presetRowsBounds.isEmpty())
            presetRowsBounds.removeFromTop(verticalGap);

        auto presetButtonRowBounds = presetRowsBounds.removeFromTop(rowHeight);
        presetButtonRowBounds.removeFromLeft(editorInsetX);
        presetButtonRowBounds.removeFromRight(editorInsetX);

        const auto buttonCount = 5;
        const auto totalGapWidth = presetRowGap * (buttonCount - 1);
        const auto availableButtonWidth = juce::jmax(0, presetButtonRowBounds.getWidth() - totalGapWidth);
        const auto baseButtonWidth = availableButtonWidth / buttonCount;
        const auto buttonWidthRemainder = availableButtonWidth % buttonCount;

        auto placePresetButton = [&presetButtonRowBounds, baseButtonWidth, buttonWidthRemainder] (BoxTextButton& button, const int index)
        {
            const auto buttonWidth = baseButtonWidth + (index < buttonWidthRemainder ? 1 : 0);
            auto buttonBounds = presetButtonRowBounds.removeFromLeft(buttonWidth);
            button.setBounds(buttonBounds);

            if (index < 4)
                presetButtonRowBounds.removeFromLeft(presetRowGap);
        };

        placePresetButton(*presetsSection->adButton, 0);
        placePresetButton(*presetsSection->saveButton, 1);
        placePresetButton(*presetsSection->renameButton, 2);
        placePresetButton(*presetsSection->defaultButton, 3);
        placePresetButton(*presetsSection->deleteButton, 4);
    }

    auto visualizerContentBounds = visualizerSectionBounds;
    auto visualizerHeaderBounds = visualizerContentBounds.removeFromTop(rowHeight);
    visualizerHeaderBounds.removeFromLeft(editorInsetX);
    visualizerHeaderBounds.removeFromRight(editorInsetX);
    visualizerHeader->setBounds(visualizerHeaderBounds);

    if (visualizerExpanded)
    {
        if (! visualizerContentBounds.isEmpty())
            visualizerContentBounds.removeFromTop(verticalGap);

        auto rangeLowBounds = visualizerContentBounds.removeFromTop(visualizerRangeLowControl->getPreferredHeight());
        rangeLowBounds.removeFromLeft(editorInsetX);
        rangeLowBounds.removeFromRight(editorInsetX);
        visualizerRangeLowControl->setBounds(rangeLowBounds);

        if (! visualizerContentBounds.isEmpty())
            visualizerContentBounds.removeFromTop(verticalGap);

        auto rangeHighBounds = visualizerContentBounds.removeFromTop(visualizerRangeHighControl->getPreferredHeight());
        rangeHighBounds.removeFromLeft(editorInsetX);
        rangeHighBounds.removeFromRight(editorInsetX);
        visualizerRangeHighControl->setBounds(rangeHighBounds);

        if (! visualizerContentBounds.isEmpty())
            visualizerContentBounds.removeFromTop(verticalGap);

        auto cursorButtonBounds = visualizerContentBounds.removeFromTop(rowHeight);
        cursorButtonBounds.removeFromLeft(editorInsetX);
        cursorButtonBounds.removeFromRight(editorInsetX);
        visualizerCursorButton->setBounds(cursorButtonBounds);

        if (! visualizerContentBounds.isEmpty())
            visualizerContentBounds.removeFromTop(verticalGap);

        auto legendBounds = visualizerContentBounds.removeFromTop(rowHeight);
        legendBounds.removeFromLeft(editorInsetX);
        legendBounds.removeFromRight(editorInsetX);

        const auto legendGap = parameterGap;
        const auto legendButtonWidth = juce::jmax(0, (legendBounds.getWidth() - (legendGap * 4)) / 5);
        auto lrLegendBounds = legendBounds.removeFromLeft(legendButtonWidth);
        legendBounds.removeFromLeft(legendGap);
        auto leftLegendBounds = legendBounds.removeFromLeft(legendButtonWidth);
        legendBounds.removeFromLeft(legendGap);
        auto rightLegendBounds = legendBounds.removeFromLeft(legendButtonWidth);
        legendBounds.removeFromLeft(legendGap);
        auto midLegendBounds = legendBounds.removeFromLeft(legendButtonWidth);
        legendBounds.removeFromLeft(legendGap);

        visualizerShowStereoButton->setBounds(lrLegendBounds);
        visualizerShowLeftButton->setBounds(leftLegendBounds);
        visualizerShowRightButton->setBounds(rightLegendBounds);
        visualizerShowMidButton->setBounds(midLegendBounds);
        visualizerShowSideButton->setBounds(legendBounds);

        if (! visualizerContentBounds.isEmpty())
            visualizerContentBounds.removeFromTop(verticalGap);

        auto visibilityButtonBounds = visualizerContentBounds.removeFromTop(rowHeight);
        visibilityButtonBounds.removeFromLeft(editorInsetX);
        visibilityButtonBounds.removeFromRight(editorInsetX);
        visualizerVisibilityButton->setBounds(visibilityButtonBounds);
    }

    juce::Rectangle<int> globalFrameBounds;
    includeComponentBounds(globalFrameBounds, globalHeader.get());
    if (globalViewport.isVisible())
    {
        auto globalViewportContentBounds = globalViewport.getBounds();
        globalViewportContentBounds.removeFromLeft(editorInsetX);
        globalViewportContentBounds.removeFromRight(editorInsetX);
        includeBounds(globalFrameBounds, globalViewportContentBounds);
    }
    placeSectionFrame(globalSectionFrame.get(), globalExpanded, globalFrameBounds);

    juce::Rectangle<int> filtersFrameBounds;
    includeComponentBounds(filtersFrameBounds, filtersHeader.get());
    if (filterViewport.isVisible())
    {
        auto filterViewportContentBounds = filterViewport.getBounds();
        filterViewportContentBounds.removeFromLeft(editorInsetX);
        filterViewportContentBounds.removeFromRight(editorInsetX);
        includeBounds(filtersFrameBounds, filterViewportContentBounds);
    }
    includeComponentBounds(filtersFrameBounds, addFilterButton.get());
    placeSectionFrame(filtersSectionFrame.get(), filtersExpanded, filtersFrameBounds);

    juce::Rectangle<int> presetsFrameBounds;
    includeComponentBounds(presetsFrameBounds, presetsSection != nullptr ? presetsSection->header.get() : nullptr);
    includeComponentBounds(presetsFrameBounds, &presetsSection->presetCombo);
    includeComponentBounds(presetsFrameBounds, presetsSection->adButton.get());
    includeComponentBounds(presetsFrameBounds, presetsSection->saveButton.get());
    includeComponentBounds(presetsFrameBounds, presetsSection->renameButton.get());
    includeComponentBounds(presetsFrameBounds, presetsSection->defaultButton.get());
    includeComponentBounds(presetsFrameBounds, presetsSection->deleteButton.get());
    placeSectionFrame(presetsSectionFrame.get(), presetsExpanded, presetsFrameBounds);

    juce::Rectangle<int> visualizerFrameBounds;
    includeComponentBounds(visualizerFrameBounds, visualizerHeader.get());
    includeComponentBounds(visualizerFrameBounds, visualizerRangeLowControl.get());
    includeComponentBounds(visualizerFrameBounds, visualizerRangeHighControl.get());
    includeComponentBounds(visualizerFrameBounds, visualizerCursorButton.get());
    includeComponentBounds(visualizerFrameBounds, visualizerShowStereoButton.get());
    includeComponentBounds(visualizerFrameBounds, visualizerShowLeftButton.get());
    includeComponentBounds(visualizerFrameBounds, visualizerShowRightButton.get());
    includeComponentBounds(visualizerFrameBounds, visualizerShowMidButton.get());
    includeComponentBounds(visualizerFrameBounds, visualizerShowSideButton.get());
    includeComponentBounds(visualizerFrameBounds, visualizerVisibilityButton.get());
    placeSectionFrame(visualizerSectionFrame.get(), visualizerExpanded, visualizerFrameBounds);

    auto moduleFrameBounds = shellGlobalExpanded ? buildShellGlobalFrameBounds()
                                                 : juce::Rectangle<int>();

    if (! shellGlobalExpanded && eqeModuleExpanded)
    {
        includeModuleTabRowBounds(moduleFrameBounds);

        includeComponentBounds(moduleFrameBounds, globalHeader.get());
        includeComponentBounds(moduleFrameBounds, filtersHeader.get());
        includeComponentBounds(moduleFrameBounds, addFilterButton.get());
        includeComponentBounds(moduleFrameBounds, presetsSection != nullptr ? presetsSection->header.get() : nullptr);
        includeBounds(moduleFrameBounds, globalFrameBounds);
        includeBounds(moduleFrameBounds, filtersFrameBounds);
        includeBounds(moduleFrameBounds, presetsFrameBounds);
        includeComponentBounds(moduleFrameBounds, visualizerHeader.get());
        includeBounds(moduleFrameBounds, visualizerFrameBounds);
    }

    placeModuleFrame(eqeModuleFrame.get(), shellGlobalExpanded || eqeModuleExpanded, moduleFrameBounds);
}
