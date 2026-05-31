#include "shell.EditorBellSection.h"
#include "shell.Constants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

juce::Rectangle<int> VxAudioProcessorEditor::getGlobalHeaderBounds() const noexcept
{
    return shellGlobalHeader != nullptr ? shellGlobalHeader->getBounds()
                                    : juce::Rectangle<int>();
}

juce::Rectangle<int> VxAudioProcessorEditor::getInfoPromptAnchorBounds() const noexcept
{
    if (shellGlobalHeader != nullptr
        && shellGlobalHeader->isVisible()
        && ! shellGlobalHeader->getBounds().isEmpty())
        return shellGlobalHeader->getBounds();

    if (footerTab != nullptr && ! footerTab->getBounds().isEmpty())
        return footerTab->getBounds();

    return {};
}

juce::Rectangle<int> VxAudioProcessorEditor::getInfoPromptVisibleBounds() const noexcept
{
    auto visibleBounds = getLocalBounds();

    if (shellGlobalHeader != nullptr
        && shellGlobalHeader->isVisible()
        && ! shellGlobalHeader->getBounds().isEmpty())
    {
        visibleBounds.setTop(shellGlobalHeader->getY());
    }
    else if (eqeModuleFrame != nullptr
             && eqeModuleFrame->isVisible()
             && ! eqeModuleFrame->getBounds().isEmpty())
    {
        visibleBounds.setTop(eqeModuleFrame->getY() + moduleFrameInsetY);
    }

    if (footerTab != nullptr && ! footerTab->getBounds().isEmpty())
        visibleBounds.setBottom(juce::jmax(visibleBounds.getY(), footerTab->getBottom()));

    if (visibleBounds.getHeight() <= 0)
        return getLocalBounds();

    return visibleBounds;
}

int VxAudioProcessorEditor::getFilterContentHeight() const
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
            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->ttssControl->getPreferredHeight();
            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->lrmsControl->getPreferredHeight();
            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->frequencyControl->getPreferredHeight();

            if (bellSections[static_cast<size_t>(bellIndex)]->bandwidthControl->isVisible())
                totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->bandwidthControl->getPreferredHeight();

            if (bellSections[static_cast<size_t>(bellIndex)]->slopeControl->isVisible())
                totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->slopeControl->getPreferredHeight();

            totalHeight += verticalGap + bellSections[static_cast<size_t>(bellIndex)]->gainControl->getPreferredHeight();
            totalHeight += verticalGap + rowHeight;
            totalHeight += verticalGap + rowHeight;
        }

        totalHeight += verticalGap;
    }

    return totalHeight;
}

int VxAudioProcessorEditor::getGlobalContentHeight() const
{
    if (! globalExpanded)
        return 0;

    if (eqeInGainLrControl == nullptr
        || eqeInGainLControl == nullptr
        || eqeInGainRControl == nullptr
        || eqeWideControl == nullptr
        || eqeOutGainControl == nullptr)
        return 0;

    return (rowHeight * 7)
        + eqeInGainLrControl->getPreferredHeight()
        + eqeInGainLControl->getPreferredHeight()
        + eqeInGainRControl->getPreferredHeight()
        + eqeWideControl->getPreferredHeight()
        + eqeOutGainControl->getPreferredHeight()
        + (verticalGap * 11);
}

int VxAudioProcessorEditor::getActiveGlobalContentHeight() const
{
    if (! globalExpanded)
        return 0;

    if (mxeModuleLoaded || tseModuleLoaded)
        return 0;

    return speModuleLoaded ? getSpeMiscContentHeight()
                           : getGlobalContentHeight();
}

int VxAudioProcessorEditor::getActiveFilterContentHeight() const
{
    if (mxeModuleLoaded || tseModuleLoaded)
        return 0;

    if (speModuleLoaded)
    {
        if (speMainExpanded)
            return getSpeMainContentHeight();

        if (visualizerExpanded)
            return getSpeAnalyserContentHeight();

        return 0;
    }

    if (filtersExpanded)
        return getFilterContentHeight();

    if (presetsExpanded && presetsSection != nullptr)
        return presetsSection->getPresetRowPreferredHeight();

    return 0;
}

int VxAudioProcessorEditor::getShellGlobalContentHeight() const
{
    if (! shellGlobalExpanded)
        return 0;

    const auto baseHeaderHeight = (rowHeight * 3) + (verticalGap * 2);
    const auto miscExpandedHeight = (rowHeight * 9)
        + clipControl->getPreferredHeight()
        + outGainControl->getPreferredHeight()
        + globalInGainLrControl->getPreferredHeight()
        + gainLControl->getPreferredHeight()
        + gainRControl->getPreferredHeight()
        + wideControl->getPreferredHeight()
        + (verticalGap * 12);

    if (shellGlobalHostExpanded)
        return miscExpandedHeight;

    if (! shellGlobalMiscExpanded)
        return baseHeaderHeight;

    return miscExpandedHeight;
}

void VxAudioProcessorEditor::updateVisualizerPanelBounds()
{
    lastCollapsedEditorWidth = juce::jmax(minimumEditorWidth, getWidth());

    if (visualizerComponent != nullptr)
        visualizerComponent->setBounds({});

    if (speAnalyserComponent != nullptr)
        speAnalyserComponent->setBounds({});
}

void VxAudioProcessorEditor::layoutShellGlobalSection(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    shellGlobalHostViewport.setVisible(false);
    shellGlobalHostViewport.setBounds({});
    shellGlobalHostContent.setSize(0, 0);

    auto shellGlobalBounds = bounds.removeFromTop(rowHeight);
    shellGlobalBounds.removeFromLeft(editorInsetX);
    shellGlobalBounds.removeFromRight(editorInsetX);
    shellGlobalHeader->setBounds(shellGlobalBounds);

    if (! bounds.isEmpty())
        bounds.removeFromTop(globalToFilterGap);

    if (! shellGlobalExpanded)
    {
        placeSectionFrame(shellGlobalSectionFrame.get(), false, {});
        return;
    }

    const auto minimumBelowShellGlobal = addFilterToFooterGap;
    const auto shellGlobalViewportHeight = juce::jmax(0, bounds.getHeight() - minimumBelowShellGlobal);
    auto shellGlobalContentBounds = bounds.removeFromTop(shellGlobalViewportHeight);

    if (! shellGlobalContentBounds.isEmpty())
    {
        juce::Rectangle<int> shellMiscFrameBounds;
        auto placeShellTab = [&shellGlobalContentBounds, editorInsetX] (BoxTextButton* tab)
        {
            if (tab == nullptr)
                return;

            auto tabBounds = shellGlobalContentBounds.removeFromTop(rowHeight);
            tabBounds.removeFromLeft(editorInsetX);
            tabBounds.removeFromRight(editorInsetX);
            tab->setBounds(tabBounds);
        };

        placeShellTab(shellGlobalMiscHeader.get());

        if (! shellGlobalContentBounds.isEmpty())
            shellGlobalContentBounds.removeFromTop(verticalGap);

        placeShellTab(shellGlobalHostHeader.get());

        if (! shellGlobalContentBounds.isEmpty())
            shellGlobalContentBounds.removeFromTop(verticalGap);

        if (! shellGlobalContentBounds.isEmpty())
            shellGlobalContentBounds.removeFromBottom(verticalGap);

        auto placeShellGlobalControl = [&shellGlobalContentBounds, editorInsetX] (auto& control)
        {
            auto controlBounds = shellGlobalContentBounds.removeFromTop(control.getPreferredHeight());
            controlBounds.removeFromLeft(editorInsetX);
            controlBounds.removeFromRight(editorInsetX);
            control.setBounds(controlBounds);

            if (! shellGlobalContentBounds.isEmpty())
                shellGlobalContentBounds.removeFromTop(verticalGap);
        };

        auto placeShellGlobalButton = [&shellGlobalContentBounds, editorInsetX] (BoxTextButton& button)
        {
            auto buttonBounds = shellGlobalContentBounds.removeFromTop(rowHeight);
            buttonBounds.removeFromLeft(editorInsetX);
            buttonBounds.removeFromRight(editorInsetX);
            button.setBounds(buttonBounds);

            if (! shellGlobalContentBounds.isEmpty())
                shellGlobalContentBounds.removeFromTop(verticalGap);
        };

        if (! shellGlobalMiscExpanded && ! shellGlobalHostExpanded)
        {
            includeComponentBounds(shellMiscFrameBounds, shellGlobalMiscHeader.get());
            includeComponentBounds(shellMiscFrameBounds, shellGlobalHostHeader.get());

            placeSectionFrame(shellGlobalSectionFrame.get(), shellGlobalExpanded, shellMiscFrameBounds);

            if (! bounds.isEmpty())
                bounds.removeFromTop(globalToFilterGap);

            return;
        }

        if (shellGlobalHostExpanded)
        {
            const auto slotCount = static_cast<int>(hostSlotButtons.size());
            const auto hostContentHeight = slotCount > 0
                ? (slotCount * rowHeight) + ((slotCount - 1) * verticalGap)
                : 0;

            auto hostViewportBounds = shellGlobalContentBounds;
            hostViewportBounds.removeFromLeft(editorInsetX);
            hostViewportBounds.removeFromRight(editorInsetX);
            hostViewportBounds.setHeight(juce::jmin(hostViewportBounds.getHeight(), hostContentHeight));
            shellGlobalHostViewport.setVisible(true);
            shellGlobalHostViewport.setBounds(hostViewportBounds);

            shellGlobalHostContent.setSize(hostViewportBounds.getWidth(),
                                           juce::jmax(hostViewportBounds.getHeight(), hostContentHeight));

            auto hostContentBounds = shellGlobalHostContent.getLocalBounds();

            for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
            {
                auto* slotButton = hostSlotButtons[static_cast<size_t>(slotIndex)].get();

                if (slotButton == nullptr)
                    continue;

                auto slotBounds = hostContentBounds.removeFromTop(rowHeight);
                slotButton->setBounds(slotBounds);

                if (! hostContentBounds.isEmpty())
                    hostContentBounds.removeFromTop(verticalGap);
            }
        }
        else if (shellGlobalMiscExpanded)
        {
            placeShellGlobalButton(*moduleAddButton);
            placeShellGlobalControl(*clipControl);
            placeShellGlobalButton(*globalBypassButton);
            placeShellGlobalButton(*globalBypassOutGainOnlyButton);
            placeShellGlobalControl(*globalInGainLrControl);
            placeShellGlobalControl(*gainLControl);
            placeShellGlobalControl(*gainRControl);
            placeShellGlobalControl(*wideControl);
            placeShellGlobalControl(*outGainControl);
            placeShellGlobalButton(*undoButton);
            placeShellGlobalButton(*redoButton);
        }

        includeComponentBounds(shellMiscFrameBounds, shellGlobalMiscHeader.get());
        includeComponentBounds(shellMiscFrameBounds, shellGlobalHostHeader.get());
        includeComponentBounds(shellMiscFrameBounds, moduleAddButton.get());
        includeComponentBounds(shellMiscFrameBounds, clipControl.get());
        includeComponentBounds(shellMiscFrameBounds, globalBypassButton.get());
        includeComponentBounds(shellMiscFrameBounds, globalBypassOutGainOnlyButton.get());
        includeComponentBounds(shellMiscFrameBounds, outGainControl.get());
        includeComponentBounds(shellMiscFrameBounds, wideControl.get());
        includeComponentBounds(shellMiscFrameBounds, globalInGainLrControl.get());
        includeComponentBounds(shellMiscFrameBounds, gainLControl.get());
        includeComponentBounds(shellMiscFrameBounds, gainRControl.get());
        includeComponentBounds(shellMiscFrameBounds, undoButton.get());
        includeComponentBounds(shellMiscFrameBounds, redoButton.get());

        includeComponentBounds(shellMiscFrameBounds, &shellGlobalHostViewport);

        placeSectionFrame(shellGlobalSectionFrame.get(), shellGlobalExpanded, shellMiscFrameBounds);

        if (! bounds.isEmpty())
            bounds.removeFromTop(globalToFilterGap);
    }
}

void VxAudioProcessorEditor::layoutFooter(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    auto footerBounds = bounds.removeFromBottom(footerHeight);
    footerBounds.removeFromLeft(editorInsetX);
    footerBounds.removeFromRight(editorInsetX);
    footerTab->setBounds(footerBounds);
}

void VxAudioProcessorEditor::layoutModuleTabRows(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    auto placeModuleRow = [&bounds, editorInsetX] (BoxTextButton& moveUpButton, BoxTextButton& tab, BoxTextButton& moveDownButton)
    {
        auto rowBounds = bounds.removeFromTop(rowHeight);
        rowBounds.removeFromLeft(editorInsetX);
        rowBounds.removeFromRight(editorInsetX);

        auto moveUpBounds = rowBounds.removeFromLeft(rowHeight);
        rowBounds.removeFromLeft(parameterGap);
        auto moveDownBounds = rowBounds.removeFromRight(rowHeight);
        rowBounds.removeFromRight(parameterGap);

        moveUpButton.setBounds(moveUpBounds);
        tab.setBounds(rowBounds);
        moveDownButton.setBounds(moveDownBounds);

        if (! bounds.isEmpty())
            bounds.removeFromTop(verticalGap);
    };

    for (auto& row : moduleTabRows)
    {
        if (row == nullptr)
            continue;

        row->moveUpButton->setBounds({});
        row->tabButton->setBounds({});
        row->moveDownButton->setBounds({});

        if (row->tabButton->isVisible())
            placeModuleRow(*row->moveUpButton, *row->tabButton, *row->moveDownButton);
    }
}

void VxAudioProcessorEditor::finalizeLayout() noexcept
{
    shell_parameter_focus::clearFocusIfNotShowing();

    eqeModuleFrame->toFront(false);

    for (const auto& row : moduleTabRows)
    {
        if (row == nullptr)
            continue;

        row->moveUpButton->toFront(false);
        row->tabButton->toFront(false);
        row->moveDownButton->toFront(false);
    }

    shellGlobalHeader->toFront(false);
    footerTab->toFront(false);

    if (focusedParameterControl != nullptr)
    {
        auto focusedControlBounds = focusedParameterControl->getBounds();
        focusedControlBounds.setY(shellGlobalHeader->getY());
        focusedControlBounds.setHeight(juce::jmax(0, footerTab->getBottom() - shellGlobalHeader->getY()));

        const auto editorInsetX = getEditorInsetX(getWidth());
        const auto maxFocusedX = juce::jmax(0, getWidth() - editorInsetX - rowHeight);
        auto focusedX = juce::jmax(0, getWidth() - (editorInsetX * 2) - rowHeight + moduleFrameInsetX);

        if (eqeModuleFrame != nullptr && eqeModuleFrame->isVisible() && ! eqeModuleFrame->getBounds().isEmpty())
            focusedX = eqeModuleFrame->getRight() + parameterGap;
        else if (shellGlobalSectionFrame != nullptr && shellGlobalSectionFrame->isVisible() && ! shellGlobalSectionFrame->getBounds().isEmpty())
            focusedX = shellGlobalSectionFrame->getRight() + parameterGap;

        focusedControlBounds.setX(juce::jlimit(0, maxFocusedX, focusedX));
        focusedParameterControl->setBounds(focusedControlBounds);

        focusedParameterControl->toFront(false);
    }

    if (textPromptOverlay != nullptr)
    {
        textPromptOverlay->setBounds(getLocalBounds());
        textPromptOverlay->toFront(true);
    }

    storeEditorStateToValueTree();
}

void VxAudioProcessorEditor::resized()
{
    if (moduleAddButton == nullptr
        || shellGlobalHeader == nullptr
        || shellGlobalMiscHeader == nullptr
        || globalHeader == nullptr
        || filtersHeader == nullptr
        || gainLControl == nullptr
        || gainRControl == nullptr
        || globalInGainLrControl == nullptr
        || wideControl == nullptr
        || outGainControl == nullptr
        || clipControl == nullptr
        || addFilterButton == nullptr
        || presetsSection == nullptr
        || globalBypassButton == nullptr
        || globalBypassOutGainOnlyButton == nullptr
        || moduleCloseButton == nullptr
        || speModuleCloseButton == nullptr
        || clearFiltersButton == nullptr
        || undoButton == nullptr
        || redoButton == nullptr
        || sortPlaceButton == nullptr
        || sortFreqButton == nullptr
        || sortDuoButton == nullptr
        || eqeModuleFrame == nullptr
        || shellGlobalSectionFrame == nullptr
        || footerTab == nullptr)
        return;

    for (const auto& section : bellSections)
    {
        if (section == nullptr)
            return;
    }

    auto bounds = getLocalBounds();

    updateVisualizerPanelBounds();

    const auto editorInsetX = getEditorInsetX(bounds.getWidth());
    const auto totalHeight = bounds.getHeight();
    const auto editorInsetTop = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetTopRatio);
    const auto editorInsetBottom = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetBottomRatio);

    if (focusedParameterControl != nullptr)
    {
        auto focusedControlBounds = juce::Rectangle<int>(rowHeight, bounds.getHeight());

        const auto editorInsetX = getEditorInsetX(getWidth());
        const auto maxFocusedX = juce::jmax(0, getWidth() - editorInsetX - rowHeight);
        auto focusedX = juce::jmax(0, getWidth() - (editorInsetX * 2) - rowHeight + moduleFrameInsetX);

        if (eqeModuleFrame != nullptr && eqeModuleFrame->isVisible() && ! eqeModuleFrame->getBounds().isEmpty())
            focusedX = eqeModuleFrame->getRight() + parameterGap;
        else if (shellGlobalSectionFrame != nullptr && shellGlobalSectionFrame->isVisible() && ! shellGlobalSectionFrame->getBounds().isEmpty())
            focusedX = shellGlobalSectionFrame->getRight() + parameterGap;

        focusedControlBounds.setX(juce::jlimit(0, maxFocusedX, focusedX));
        focusedControlBounds.setY(bounds.getY());
        focusedParameterControl->setBounds(focusedControlBounds);
    }

    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);

    if (focusedParameterControl != nullptr)
        bounds.removeFromRight(rowHeight + parameterGap);

    bounds.removeFromBottom(editorInsetBottom);
    bounds.removeFromTop(editorInsetTop);

    const auto showShellGlobalStrip = shellGlobalExpanded || ! eqeModuleExpanded;

    layoutFooter(bounds, editorInsetX);
    if (showShellGlobalStrip)
        layoutShellGlobalSection(bounds, editorInsetX);

    if (shellGlobalExpanded)
    {
        layoutCollapsedModuleState();
        finalizeLayout();
        return;
    }

    if (! eqeModuleLoaded && ! speModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
    {
        layoutNoModuleState(bounds);
        finalizeLayout();
        return;
    }

    layoutModuleTabRows(bounds, editorInsetX);

    if (! eqeModuleExpanded)
    {
        layoutCollapsedModuleState();
        finalizeLayout();
        return;
    }

    if (mxeModuleLoaded || tseModuleLoaded)
    {
        layoutModuleEditorContent(bounds);
        finalizeLayout();
        return;
    }

    if (speModuleLoaded)
        layoutSpeModuleSections(bounds, editorInsetX);
    else
        layoutEqeModuleSections(bounds, editorInsetX);

    finalizeLayout();
}
