#include "shell.EditorBellSection.h"
#include "shell.Constants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::layoutEqeModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    auto moduleViewportBounds = bounds;
    if (! moduleViewportBounds.isEmpty())
        moduleViewportBounds.removeFromBottom(addFilterToFooterGap);
    moduleViewportBounds.removeFromLeft(editorInsetX);
    moduleViewportBounds.removeFromRight(editorInsetX);

    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToFooterGap);

    if (! bounds.isEmpty())
        bounds.removeFromBottom(moduleFrameInsetY);

    auto presetsBounds = bounds.removeFromBottom(juce::jmin(bounds.getHeight(),
                                                            rowHeight + (presetsExpanded ? verticalGap + presetsSection->getPresetRowPreferredHeight() : 0)));

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

        auto placeGlobalControl = [&globalBounds, editorInsetX] (auto& control)
        {
            auto controlBounds = globalBounds.removeFromTop(control.getPreferredHeight());
            controlBounds.removeFromLeft(editorInsetX);
            controlBounds.removeFromRight(editorInsetX);
            control.setBounds(controlBounds);

            if (! globalBounds.isEmpty())
                globalBounds.removeFromTop(verticalGap);
        };

        placeGlobalButton(*moduleCloseButton);
        placeGlobalButton(*eqeBypassButton);
        placeGlobalButton(*eqeBypassWithGainButton);
        placeGlobalControl(*eqeInGainLrControl);
        placeGlobalControl(*eqeInGainLControl);
        placeGlobalControl(*eqeInGainRControl);
        placeGlobalControl(*eqeWideControl);
        placeGlobalControl(*eqeOutGainControl);
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

    filterViewport.setBounds({});
    filterViewport.setVisible(false);
    filterContent.setSize(0, 0);

    if (filtersExpanded)
    {
        filterViewport.setBounds(bounds);
        filterViewport.setVisible(true);
        filterContent.setSize(bounds.getWidth(), juce::jmax(bounds.getHeight(), getFilterContentHeight()));
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

            auto placeFilterActionButton = [&filterBounds, editorInsetX] (BoxTextButton& button)
            {
                auto rowBounds = filterBounds.removeFromTop(rowHeight);
                rowBounds.removeFromLeft(editorInsetX);
                rowBounds.removeFromRight(editorInsetX);
                button.setBounds(rowBounds);

                if (! filterBounds.isEmpty())
                    filterBounds.removeFromTop(verticalGap);
            };

            placeBellHeader(section);

            if (! globalExpanded && ! presetsExpanded && expandedBellIndex == bellIndex)
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
                placeFilterActionButton(*section.bypassButton);
                placeFilterActionButton(*section.deleteButton);
            }
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

        filterViewport.setBounds(presetsContentBounds);
        filterViewport.setVisible(true);
        filterContent.setSize(presetsContentBounds.getWidth(),
                              juce::jmax(presetsContentBounds.getHeight(), presetsSection->getPresetRowPreferredHeight()));

        auto presetRowsBounds = filterContent.getLocalBounds();

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
    if (filtersExpanded && filterViewport.isVisible())
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

    if (presetsExpanded && filterViewport.isVisible())
    {
        auto presetsViewportContentBounds = filterViewport.getBounds();
        presetsViewportContentBounds.removeFromLeft(editorInsetX);
        presetsViewportContentBounds.removeFromRight(editorInsetX);
        includeBounds(presetsFrameBounds, presetsViewportContentBounds);
    }

    placeSectionFrame(presetsSectionFrame.get(), presetsExpanded, presetsFrameBounds);

    auto moduleFrameBounds = shellGlobalExpanded ? buildShellGlobalFrameBounds()
                                                 : juce::Rectangle<int>();

    if (! shellGlobalExpanded && eqeModuleExpanded)
    {
        includeModuleTabRowBounds(moduleFrameBounds);
        includeBounds(moduleFrameBounds, moduleViewportBounds);

        includeComponentBounds(moduleFrameBounds, globalHeader.get());
        includeComponentBounds(moduleFrameBounds, filtersHeader.get());
        includeComponentBounds(moduleFrameBounds, addFilterButton.get());
        includeComponentBounds(moduleFrameBounds, presetsSection != nullptr ? presetsSection->header.get() : nullptr);
        includeBounds(moduleFrameBounds, globalFrameBounds);
        includeBounds(moduleFrameBounds, filtersFrameBounds);
        includeBounds(moduleFrameBounds, presetsFrameBounds);
    }

    placeModuleFrame(eqeModuleFrame.get(), shellGlobalExpanded || eqeModuleExpanded, moduleFrameBounds);
}
