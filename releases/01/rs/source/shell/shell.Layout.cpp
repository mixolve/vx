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

juce::Rectangle<int> VxAudioProcessorEditor::getMainPanelBounds() const noexcept
{
    auto bounds = getLocalBounds();

    if ((eqeModuleLoaded || speModuleLoaded || mxeModuleLoaded) && eqeModuleExpanded && visualizerVisible)
        bounds = bounds.removeFromLeft(getCurrentMainPanelWidth());

    return bounds;
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
        }

        totalHeight += verticalGap;
    }

    return totalHeight;
}

int VxAudioProcessorEditor::getGlobalContentHeight() const
{
    if (! globalExpanded)
        return 0;

    return (rowHeight * 7) + (verticalGap * 6);
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
    if (speModuleLoaded && visualizerExpanded)
        return getSpeAnalyserContentHeight();

    if (mxeModuleLoaded || tseModuleLoaded)
        return 0;

    return getFilterContentHeight();
}

int VxAudioProcessorEditor::getShellGlobalContentHeight() const
{
    if (! shellGlobalExpanded)
        return 0;

    if (! shellGlobalMiscExpanded)
        return rowHeight;

    return (rowHeight * 6)
        + clipControl->getPreferredHeight()
        + outGainControl->getPreferredHeight()
        + globalInGainLrControl->getPreferredHeight()
        + gainLControl->getPreferredHeight()
        + gainRControl->getPreferredHeight()
        + wideControl->getPreferredHeight()
        + (verticalGap * 11);
}

void VxAudioProcessorEditor::updateVisualizerPanelBounds(juce::Rectangle<int>& bounds)
{
    if ((eqeModuleLoaded || speModuleLoaded || mxeModuleLoaded) && eqeModuleExpanded && visualizerVisible)
    {
        const auto minimumVisibleWidth = lastCollapsedEditorWidth + visualizerPanelGap + minimumVisualizerWidth;
        const auto totalWidth = juce::jmax(minimumVisibleWidth, getWidth());
        lastCollapsedEditorWidth = juce::jlimit(minimumEditorWidth,
                                                juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                                                juce::jmin(lastCollapsedEditorWidth,
                                                           totalWidth - visualizerPanelGap - minimumVisualizerWidth));
        lastVisualizerWidth = juce::jmax(minimumVisualizerWidth,
                                         totalWidth - visualizerPanelGap - lastCollapsedEditorWidth);

        const auto mainPanelWidth = getCurrentMainPanelWidth();
        auto visualizerBounds = bounds.withTrimmedLeft(mainPanelWidth + visualizerPanelGap);

        if (visualizerComponent != nullptr || speAnalyserComponent != nullptr)
        {
            const auto visualizerInsetX = getEditorInsetX(mainPanelWidth);
            visualizerBounds.removeFromLeft(visualizerInsetX);
            visualizerBounds.removeFromRight(visualizerInsetX);

            const auto totalHeight = visualizerBounds.getHeight();
            const auto editorInsetTop = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetTopRatio);
            const auto editorInsetBottom = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetBottomRatio);
            visualizerBounds.removeFromTop(editorInsetTop);
            visualizerBounds.removeFromBottom(editorInsetBottom);

            if (eqeModuleLoaded && visualizerComponent != nullptr)
                visualizerComponent->setBounds(visualizerBounds);

            if (speModuleLoaded && speAnalyserComponent != nullptr)
                speAnalyserComponent->setBounds(visualizerBounds);
        }

        bounds = bounds.removeFromLeft(mainPanelWidth);
        return;
    }

    lastCollapsedEditorWidth = juce::jmax(minimumEditorWidth, getWidth());

    if (visualizerComponent != nullptr)
        visualizerComponent->setBounds({});

    if (speAnalyserComponent != nullptr)
        speAnalyserComponent->setBounds({});
}

void VxAudioProcessorEditor::layoutShellGlobalSection(juce::Rectangle<int>& bounds, const int editorInsetX)
{
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

    auto shellGlobalContentBounds = bounds.removeFromTop(getShellGlobalContentHeight());

    if (! shellGlobalContentBounds.isEmpty())
    {
        juce::Rectangle<int> shellMiscFrameBounds;

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

        placeShellGlobalButton(*shellGlobalMiscHeader);

        if (! shellGlobalMiscExpanded)
        {
            placeSectionFrame(shellGlobalSectionFrame.get(), false, {});

            if (! bounds.isEmpty())
                bounds.removeFromTop(globalToFilterGap);

            return;
        }

        placeShellGlobalButton(*moduleAddButton);
        placeShellGlobalControl(*clipControl);
        placeShellGlobalButton(*globalBypassButton);
        placeShellGlobalButton(*globalBypassOutGainOnlyButton);
        placeShellGlobalControl(*outGainControl);
        placeShellGlobalControl(*wideControl);
        placeShellGlobalControl(*globalInGainLrControl);
        placeShellGlobalControl(*gainLControl);
        placeShellGlobalControl(*gainRControl);
        placeShellGlobalButton(*undoButton);
        placeShellGlobalButton(*redoButton);

        includeComponentBounds(shellMiscFrameBounds, shellGlobalMiscHeader.get());
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
        || speStereoBypassButton == nullptr
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
    updateVisualizerPanelBounds(bounds);

    const auto editorInsetX = getEditorInsetX(bounds.getWidth());
    const auto totalHeight = bounds.getHeight();
    const auto editorInsetTop = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetTopRatio);
    const auto editorInsetBottom = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetBottomRatio);

    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);
    bounds.removeFromBottom(editorInsetBottom);
    bounds.removeFromTop(editorInsetTop);

    layoutShellGlobalSection(bounds, editorInsetX);
    layoutFooter(bounds, editorInsetX);

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
