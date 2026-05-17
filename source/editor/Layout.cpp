#include "EditorBellSection.h"
#include "EditorPresetSections.h"

#include <algorithm>

void EqeAudioProcessorEditor::updateSectionStates()
{
    const auto activeBellCount = getActiveBellCount();

    if (moduleAddButton != nullptr)
        moduleAddButton->setVisible(! eqeModuleLoaded);

    if (! eqeModuleLoaded)
    {
        if (globalHeader != nullptr)
            globalHeader->setVisible(false);

        if (filtersHeader != nullptr)
            filtersHeader->setVisible(false);

#if ! JUCE_IOS
        if (visualizerHeader != nullptr)
            visualizerHeader->setVisible(false);
#endif

        globalViewport.setVisible(false);
        filterViewport.setVisible(false);

        if (presetsSection != nullptr)
        {
            presetsSection->header->setVisible(false);
            presetsSection->presetCombo.setVisible(false);
            presetsSection->adButton->setVisible(false);
            presetsSection->saveButton->setVisible(false);
            presetsSection->renameButton->setVisible(false);
            presetsSection->defaultButton->setVisible(false);
            presetsSection->deleteButton->setVisible(false);
        }

        if (addFilterButton != nullptr)
            addFilterButton->setVisible(false);

        if (footerTab != nullptr)
            footerTab->setVisible(false);

        if (globalSectionFrame != nullptr)
            globalSectionFrame->setVisible(false);

        if (filtersSectionFrame != nullptr)
            filtersSectionFrame->setVisible(false);

        if (presetsSectionFrame != nullptr)
            presetsSectionFrame->setVisible(false);

#if ! JUCE_IOS
        if (visualizerSectionFrame != nullptr)
            visualizerSectionFrame->setVisible(false);

        if (visualizerComponent != nullptr)
            visualizerComponent->setVisible(false);

        if (visualizerRangeLowControl != nullptr)
            visualizerRangeLowControl->setVisible(false);

        if (visualizerRangeHighControl != nullptr)
            visualizerRangeHighControl->setVisible(false);

        if (visualizerCursorButton != nullptr)
            visualizerCursorButton->setVisible(false);

        if (visualizerShowStereoButton != nullptr)
            visualizerShowStereoButton->setVisible(false);

        if (visualizerShowLeftButton != nullptr)
            visualizerShowLeftButton->setVisible(false);

        if (visualizerShowRightButton != nullptr)
            visualizerShowRightButton->setVisible(false);

        if (visualizerShowMidButton != nullptr)
            visualizerShowMidButton->setVisible(false);

        if (visualizerShowSideButton != nullptr)
            visualizerShowSideButton->setVisible(false);

        if (visualizerVisibilityButton != nullptr)
            visualizerVisibilityButton->setVisible(false);
#endif

        for (auto& section : bellSections)
        {
            if (section == nullptr)
                continue;

            section->moveUpButton->setVisible(false);
            section->moveDownButton->setVisible(false);
            section->header->setVisible(false);
            section->typeControl->setVisible(false);
            section->lrmsControl->setVisible(false);
            section->slopeControl->setVisible(false);
            section->frequencyControl->setVisible(false);
            section->bandwidthControl->setVisible(false);
            section->gainControl->setVisible(false);
            section->bypassButton->setVisible(false);
            section->deleteButton->setVisible(false);
        }

        return;
    }

    if (globalHeader != nullptr)
        globalHeader->setVisible(true);

    if (filtersHeader != nullptr)
        filtersHeader->setVisible(true);

    if (presetsSection != nullptr)
        presetsSection->header->setVisible(true);

    if (footerTab != nullptr)
        footerTab->setVisible(true);

#if ! JUCE_IOS
    if (visualizerHeader != nullptr)
        visualizerHeader->setVisible(true);
#endif

    if (globalHeader != nullptr)
        globalHeader->setButtonText("GLOBAL");

    if (globalHeader != nullptr)
        globalHeader->setToggleState(globalExpanded, juce::dontSendNotification);

    if (filtersHeader != nullptr)
    {
        filtersHeader->setButtonText(juce::String::formatted("FILTERS (%d)", activeBellCount));
        filtersHeader->setToggleState(filtersExpanded, juce::dontSendNotification);
    }

#if ! JUCE_IOS
    if (visualizerHeader != nullptr)
        visualizerHeader->setToggleState(visualizerExpanded, juce::dontSendNotification);
#endif

    if (globalBypassButton != nullptr)
        globalBypassButton->setVisible(globalExpanded);

    if (globalBypassOutGainOnlyButton != nullptr)
        globalBypassOutGainOnlyButton->setVisible(globalExpanded);

    if (undoButton != nullptr)
        undoButton->setVisible(globalExpanded);

    if (redoButton != nullptr)
        redoButton->setVisible(globalExpanded);

    if (clearFiltersButton != nullptr)
    {
        clearFiltersButton->setVisible(globalExpanded);
        clearFiltersButton->setEnabled(activeBellCount > 0);
        clearFiltersButton->setAlpha(activeBellCount > 0 ? 1.0f : 0.45f);
    }

    updateUndoRedoButtons();
    globalViewport.setVisible(globalExpanded);

    if (clipControl != nullptr)
        clipControl->setVisible(globalExpanded);

    if (outGainControl != nullptr)
        outGainControl->setVisible(globalExpanded);

    if (gainLControl != nullptr)
        gainLControl->setVisible(globalExpanded);

    if (gainRControl != nullptr)
        gainRControl->setVisible(globalExpanded);

    if (wideControl != nullptr)
        wideControl->setVisible(globalExpanded);

    const auto canSortFilters = activeBellCount > 1;

    if (sortPlaceButton != nullptr)
    {
        sortPlaceButton->setVisible(globalExpanded);
        sortPlaceButton->setEnabled(canSortFilters);
        sortPlaceButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    if (sortFreqButton != nullptr)
    {
        sortFreqButton->setVisible(globalExpanded);
        sortFreqButton->setEnabled(canSortFilters);
        sortFreqButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    if (sortDuoButton != nullptr)
    {
        sortDuoButton->setVisible(globalExpanded);
        sortDuoButton->setEnabled(canSortFilters);
        sortDuoButton->setAlpha(canSortFilters ? 1.0f : 0.45f);
    }

    filterViewport.setVisible(filtersExpanded);

    if (presetsSection != nullptr)
    {
        presetsSection->header->setToggleState(presetsExpanded, juce::dontSendNotification);
        presetsSection->presetCombo.setVisible(presetsExpanded);
        presetsSection->adButton->setVisible(presetsExpanded);
        presetsSection->saveButton->setVisible(presetsExpanded);
        presetsSection->renameButton->setVisible(presetsExpanded);
        presetsSection->defaultButton->setVisible(presetsExpanded);
        presetsSection->deleteButton->setVisible(presetsExpanded);
    }

#if ! JUCE_IOS
    if (visualizerRangeLowControl != nullptr)
        visualizerRangeLowControl->setVisible(visualizerExpanded);

    if (visualizerRangeHighControl != nullptr)
        visualizerRangeHighControl->setVisible(visualizerExpanded);

    if (visualizerCursorButton != nullptr)
        visualizerCursorButton->setVisible(visualizerExpanded);

    if (visualizerShowStereoButton != nullptr)
        visualizerShowStereoButton->setVisible(visualizerExpanded);

    if (visualizerShowLeftButton != nullptr)
        visualizerShowLeftButton->setVisible(visualizerExpanded);

    if (visualizerShowRightButton != nullptr)
        visualizerShowRightButton->setVisible(visualizerExpanded);

    if (visualizerShowMidButton != nullptr)
        visualizerShowMidButton->setVisible(visualizerExpanded);

    if (visualizerShowSideButton != nullptr)
        visualizerShowSideButton->setVisible(visualizerExpanded);

    if (visualizerVisibilityButton != nullptr)
    {
        visualizerVisibilityButton->setVisible(visualizerExpanded);
        visualizerVisibilityButton->setToggleState(visualizerVisible, juce::dontSendNotification);
        visualizerVisibilityButton->setButtonText(visualizerVisible ? "HIDE" : "SHOW");
    }

    if (visualizerCursorButton != nullptr)
        visualizerCursorButton->setButtonText(visualizerCursorEnabled ? "CURSOR-OFF" : "CURSOR-ON");

    if (globalSectionFrame != nullptr)
        globalSectionFrame->setVisible(globalExpanded);

    if (filtersSectionFrame != nullptr)
        filtersSectionFrame->setVisible(filtersExpanded);

    if (presetsSectionFrame != nullptr)
        presetsSectionFrame->setVisible(presetsExpanded);

    if (visualizerSectionFrame != nullptr)
        visualizerSectionFrame->setVisible(visualizerExpanded);

    if (visualizerComponent != nullptr)
        visualizerComponent->setVisible(visualizerVisible);
#endif

    for (int bellIndex = 0; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

        if (section == nullptr)
            continue;

        const auto orderPosition = getBellOrderPositionForIndex(bellIndex);
        const auto isActive = filtersExpanded && orderPosition >= 0;
        const auto isExpanded = isActive && ! globalExpanded && ! presetsExpanded && expandedBellIndex == bellIndex;
        const auto isBell = section->getFilterType() == EqeAudioProcessor::FilterType::bell;
        const auto bandwidthInactive = section->isBandwidthInactiveAtCurrentSlope();
        const auto slopeInactive = section->isSlopeInactive();
        const auto gainInactive = section->isGainInactive();
        const auto bellOrderOff = section->getFilterType() == EqeAudioProcessor::FilterType::bell
            && section->slopeControl->getSelectedChoiceIndex() == 0;
        const auto canMoveUp = isActive && orderPosition > 0;
        const auto canMoveDown = isActive && orderPosition + 1 < activeBellCount;

        section->moveUpButton->setVisible(isActive);
        section->moveUpButton->setEnabled(canMoveUp);
        section->moveUpButton->setAlpha(canMoveUp ? 1.0f : 0.45f);
        section->moveDownButton->setVisible(isActive);
        section->moveDownButton->setEnabled(canMoveDown);
        section->moveDownButton->setAlpha(canMoveDown ? 1.0f : 0.45f);
        section->header->setButtonText(audioProcessor.getBellHeaderText(bellIndex, orderPosition));
        section->header->setVisible(isActive);
        section->header->setToggleState(isExpanded, juce::dontSendNotification);
        section->typeControl->setVisible(isExpanded);
        section->lrmsControl->setVisible(isExpanded);
        section->slopeControl->setVisible(isExpanded);
        section->slopeControl->setInteractionEnabled(! slopeInactive);
        if (slopeInactive || bellOrderOff)
            section->slopeControl->setOverrideText("OFF");
        else
            section->slopeControl->clearOverrideText();
        section->frequencyControl->setVisible(isExpanded);
        section->bandwidthControl->setVisible(isExpanded);
        section->bandwidthControl->setInteractionEnabled(isBell && ! bandwidthInactive);
        if (isBell && ! bandwidthInactive)
            section->bandwidthControl->clearOverrideText();
        else
            section->bandwidthControl->setOverrideText("OFF");
        section->gainControl->setVisible(isExpanded);
        section->gainControl->setInteractionEnabled(! gainInactive);
        if (gainInactive)
            section->gainControl->setOverrideText("OFF");
        else
            section->gainControl->clearOverrideText();
        section->bypassButton->setVisible(isExpanded);
        section->deleteButton->setVisible(isExpanded);
        section->deleteButton->setEnabled(activeBellCount > 0);
        section->deleteButton->setAlpha(activeBellCount > 0 ? 1.0f : 0.45f);
    }

    if (addFilterButton != nullptr)
    {
        const auto canAddFilter = activeBellCount < EqeAudioProcessor::maxBellFilterCount;
        addFilterButton->setVisible(filtersExpanded);
        addFilterButton->setEnabled(canAddFilter);
        addFilterButton->setAlpha(canAddFilter ? 1.0f : 0.45f);
    }
}

void EqeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

juce::Rectangle<int> EqeAudioProcessorEditor::getGlobalHeaderBounds() const noexcept
{
    return globalHeader != nullptr ? globalHeader->getBounds()
                                   : juce::Rectangle<int>();
}

juce::Rectangle<int> EqeAudioProcessorEditor::getMainPanelBounds() const noexcept
{
    auto bounds = getLocalBounds();

#if ! JUCE_IOS
    if (visualizerVisible)
        bounds = bounds.removeFromLeft(getCurrentMainPanelWidth());
#endif

    return bounds;
}

int EqeAudioProcessorEditor::getFilterContentHeight() const
{
    if (! filtersExpanded)
        return 0;

    if (globalHeader == nullptr || outGainControl == nullptr || addFilterButton == nullptr)
        return 0;

    for (const auto& section : bellSections)
    {
        if (section == nullptr)
            return 0;
    }

    const auto activeBellCount = getActiveBellCount();
    auto totalHeight = 0;

    for (int displayIndex = 0; displayIndex < activeBellCount; ++displayIndex)
    {
        const auto bellIndex = getBellIndexForOrderPosition(displayIndex);
        if (bellIndex < 0)
            continue;

        totalHeight += rowHeight;

        if (! globalExpanded && ! presetsExpanded && expandedBellIndex == bellIndex)
        {
            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->typeControl->getPreferredHeight();
            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->lrmsControl->getPreferredHeight();
            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->frequencyControl->getPreferredHeight();

            if (bellSections[static_cast<size_t>(bellIndex)]->bandwidthControl->isVisible())
                totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->bandwidthControl->getPreferredHeight();

            if (bellSections[static_cast<size_t>(bellIndex)]->slopeControl->isVisible())
                totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->slopeControl->getPreferredHeight();

            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->gainControl->getPreferredHeight();
            totalHeight += verticalGap + rowHeight;
        }

        totalHeight += verticalGap;
    }

    return totalHeight;
}

int EqeAudioProcessorEditor::getGlobalContentHeight() const
{
    if (! globalExpanded)
        return 0;

    return rowHeight * 8
        + gainLControl->getPreferredHeight()
        + gainRControl->getPreferredHeight()
        + wideControl->getPreferredHeight()
        + outGainControl->getPreferredHeight()
        + clipControl->getPreferredHeight()
        + (verticalGap * 11);
}

void EqeAudioProcessorEditor::resized()
{
    if (moduleAddButton == nullptr
        || globalHeader == nullptr
        || filtersHeader == nullptr
        || gainLControl == nullptr
        || gainRControl == nullptr
        || wideControl == nullptr
        || outGainControl == nullptr
        || clipControl == nullptr
        || addFilterButton == nullptr
        || presetsSection == nullptr
        || globalBypassButton == nullptr
        || globalBypassOutGainOnlyButton == nullptr
        || clearFiltersButton == nullptr
        || undoButton == nullptr
        || redoButton == nullptr
        || sortPlaceButton == nullptr
        || sortFreqButton == nullptr
        || sortDuoButton == nullptr)
        return;

    for (const auto& section : bellSections)
    {
        if (section == nullptr)
            return;
    }

    auto bounds = getLocalBounds();

    if (! eqeModuleLoaded)
    {
        const auto buttonSize = rowHeight;
        moduleAddButton->setBounds(juce::Rectangle<int>(buttonSize, buttonSize)
                                       .withCentre(bounds.getCentre()));

        if (textPromptOverlay != nullptr)
        {
            textPromptOverlay->setBounds(getLocalBounds());
            textPromptOverlay->toFront(true);
        }

        storeEditorStateToValueTree();
        return;
    }

#if ! JUCE_IOS
    if (visualizerVisible)
    {
        const auto minimumVisibleWidth = lastCollapsedEditorWidth + visualizerPanelGap + minimumVisualizerWidth;
        const auto totalWidth = juce::jmax(minimumVisibleWidth, getWidth());
        lastCollapsedEditorWidth = juce::jlimit(minimumEditorWidth,
                                                juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                                                juce::jmin(lastCollapsedEditorWidth,
                                                           totalWidth - visualizerPanelGap - minimumVisualizerWidth));
        lastVisualizerWidth = juce::jmax(minimumVisualizerWidth,
                                         totalWidth - visualizerPanelGap - lastCollapsedEditorWidth);
    }
    else
    {
        lastCollapsedEditorWidth = juce::jmax(minimumEditorWidth, getWidth());
    }

    auto mainPanelBounds = visualizerVisible ? bounds.removeFromLeft(getCurrentMainPanelWidth())
                                             : bounds;

    if (visualizerVisible)
    {
        bounds.removeFromLeft(visualizerPanelGap);

        if (visualizerComponent != nullptr)
        {
            auto visualizerBounds = bounds;
            const auto visualizerInsetX = getEditorInsetX(getCurrentMainPanelWidth());
            visualizerBounds.removeFromLeft(visualizerInsetX);
            visualizerBounds.removeFromRight(visualizerInsetX);
            const auto totalHeight = visualizerBounds.getHeight();
            const auto editorInsetTop = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetTopRatio);
            const auto editorInsetBottom = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetBottomRatio);
            visualizerBounds.removeFromTop(editorInsetTop);
            visualizerBounds.removeFromBottom(editorInsetBottom);
            visualizerComponent->setBounds(visualizerBounds);
        }
    }
    else if (visualizerComponent != nullptr)
    {
        visualizerComponent->setBounds({});
    }

    bounds = mainPanelBounds;
#endif
    const auto editorInsetX = getEditorInsetX(bounds.getWidth());

    const auto totalHeight = bounds.getHeight();
    const auto editorInsetTop = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetTopRatio);
    const auto editorInsetBottom = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetBottomRatio);

    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);
    bounds.removeFromBottom(editorInsetBottom);
    bounds.removeFromTop(editorInsetTop);

    auto footerBounds = bounds.removeFromBottom(footerHeight);
    footerBounds.removeFromLeft(editorInsetX);
    footerBounds.removeFromRight(editorInsetX);
    footerTab->setBounds(footerBounds);
    bounds.removeFromBottom(addFilterToFooterGap);

    auto presetsHeight = rowHeight;
    if (presetsExpanded)
    {
        presetsHeight += verticalGap;
        presetsHeight += presetsSection->getPresetRowPreferredHeight();
    }

    auto presetsBounds = bounds.removeFromBottom(presetsHeight);

#if ! JUCE_IOS
    auto visualizerSectionHeight = rowHeight;
    if (visualizerExpanded)
    {
        visualizerSectionHeight += verticalGap;
        visualizerSectionHeight += visualizerRangeLowControl->getPreferredHeight();
        visualizerSectionHeight += verticalGap;
        visualizerSectionHeight += visualizerRangeHighControl->getPreferredHeight();
        visualizerSectionHeight += verticalGap;
        visualizerSectionHeight += rowHeight;
        visualizerSectionHeight += verticalGap;
        visualizerSectionHeight += rowHeight;
        visualizerSectionHeight += verticalGap;
        visualizerSectionHeight += rowHeight;
    }

    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToPresetsGap);

    auto visualizerSectionBounds = bounds.removeFromBottom(visualizerSectionHeight);

    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToPresetsGap);
#else
    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToPresetsGap);
#endif

    if (filtersExpanded)
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
        const auto viewportHeight = juce::jmax(0, bounds.getHeight() - minimumBelowGlobal);
        auto globalViewportBounds = bounds.removeFromTop(viewportHeight);
        globalViewport.setBounds(globalViewportBounds);
        globalContent.setSize(globalViewportBounds.getWidth(), juce::jmax(globalViewportBounds.getHeight(), getGlobalContentHeight()));

        auto globalBounds = globalContent.getLocalBounds();

        auto placeGlobalControl = [&globalBounds, editorInsetX] (auto& control)
        {
            auto controlBounds = globalBounds.removeFromTop(control.getPreferredHeight());
            controlBounds.removeFromLeft(editorInsetX);
            controlBounds.removeFromRight(editorInsetX);
            control.setBounds(controlBounds);

            if (! globalBounds.isEmpty())
                globalBounds.removeFromTop(verticalGap);
        };

        auto placeGlobalButton = [&globalBounds, editorInsetX] (BoxTextButton& button)
        {
            auto buttonBounds = globalBounds.removeFromTop(rowHeight);
            buttonBounds.removeFromLeft(editorInsetX);
            buttonBounds.removeFromRight(editorInsetX);
            button.setBounds(buttonBounds);

            if (! globalBounds.isEmpty())
                globalBounds.removeFromTop(verticalGap);
        };

        placeGlobalControl(*clipControl);
        placeGlobalControl(*outGainControl);
        placeGlobalControl(*gainLControl);
        placeGlobalControl(*gainRControl);
        placeGlobalControl(*wideControl);
        placeGlobalButton(*globalBypassButton);
        placeGlobalButton(*globalBypassOutGainOnlyButton);
        placeGlobalButton(*undoButton);
        placeGlobalButton(*redoButton);
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

#if ! JUCE_IOS
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
#endif

    auto includeBounds = [] (juce::Rectangle<int>& sectionBounds, const juce::Rectangle<int>& boundsToInclude)
    {
        if (boundsToInclude.isEmpty())
            return;

        sectionBounds = sectionBounds.isEmpty() ? boundsToInclude
                                               : sectionBounds.getUnion(boundsToInclude);
    };

    auto includeComponentBounds = [&includeBounds] (juce::Rectangle<int>& sectionBounds, const juce::Component* component)
    {
        if (component == nullptr || ! component->isVisible() || component->getBounds().isEmpty())
            return;

        includeBounds(sectionBounds, component->getBounds());
    };

    auto placeSectionFrame = [this] (juce::Component* frame,
                                     const bool shouldShow,
                                     juce::Rectangle<int> sectionBounds)
    {
        if (frame == nullptr)
            return;

        frame->setVisible(shouldShow && ! sectionBounds.isEmpty());

        if (! frame->isVisible())
        {
            frame->setBounds({});
            return;
        }

        frame->setBounds(sectionBounds.expanded(2).getIntersection(getLocalBounds()));
        frame->toFront(false);
    };

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
    if (presetsSection != nullptr)
    {
        includeComponentBounds(presetsFrameBounds, presetsSection->header.get());
        includeComponentBounds(presetsFrameBounds, &presetsSection->presetCombo);
        includeComponentBounds(presetsFrameBounds, presetsSection->adButton.get());
        includeComponentBounds(presetsFrameBounds, presetsSection->saveButton.get());
        includeComponentBounds(presetsFrameBounds, presetsSection->renameButton.get());
        includeComponentBounds(presetsFrameBounds, presetsSection->defaultButton.get());
        includeComponentBounds(presetsFrameBounds, presetsSection->deleteButton.get());
    }
    placeSectionFrame(presetsSectionFrame.get(), presetsExpanded, presetsFrameBounds);

#if ! JUCE_IOS
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
#endif

    if (textPromptOverlay != nullptr)
    {
        textPromptOverlay->setBounds(getLocalBounds());
        textPromptOverlay->toFront(true);
    }

    storeEditorStateToValueTree();
}
