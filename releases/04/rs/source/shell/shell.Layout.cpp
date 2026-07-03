#include "shell.EditorFilterSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

juce::Rectangle<int> VxAudioProcessorEditor::getInfoPromptAnchorBounds() const noexcept
{
    if (footerTab != nullptr && ! footerTab->getBounds().isEmpty())
        return footerTab->getBounds();

    if (clipButton != nullptr
        && clipButton->isVisible()
        && ! clipButton->getBounds().isEmpty())
        return clipButton->getBounds();

    return {};
}

juce::Rectangle<int> VxAudioProcessorEditor::getInfoPromptVisibleBounds() const noexcept
{
    if (footerTab != nullptr && ! footerTab->getBounds().isEmpty())
    {
        auto visibleBounds = getLocalBounds();

        visibleBounds.setLeft(footerTab->getX());
        visibleBounds.setRight(footerTab->getRight());
        visibleBounds.setBottom(footerTab->getBottom());

        if (clipButton != nullptr
            && clipButton->isVisible()
            && ! clipButton->getBounds().isEmpty())
        {
            visibleBounds.setTop(clipButton->getY());
        }

        if (visibleBounds.getHeight() > 0 && visibleBounds.getWidth() > 0)
            return visibleBounds;
    }

    return getLocalBounds();
}

int VxAudioProcessorEditor::getFilterContentHeight() const
{
    if (addFilterButton == nullptr)
        return 0;

    for (const auto& section : filterSections)
    {
        if (section == nullptr)
            return 0;
    }

    const auto activeFilterCount = getActiveFilterCount();
    auto totalHeight = 0;

    for (int displayIndex = 0; displayIndex < activeFilterCount; ++displayIndex)
    {
        const auto filterIndex = getFilterIndexForOrderPosition(displayIndex);
        if (filterIndex < 0)
            continue;

        totalHeight += rowHeight;

        const auto* section = filterSections[static_cast<size_t>(filterIndex)].get();

        if (section != nullptr && section->expanded)
        {
            totalHeight += verticalGap + section->typeControl->getPreferredHeight();
            totalHeight += verticalGap + section->placeControl->getPreferredHeight();
            totalHeight += verticalGap + section->slopeControl->getPreferredHeight();
            totalHeight += verticalGap + section->frequencyControl->getPreferredHeight();
            totalHeight += verticalGap + section->bandwidthControl->getPreferredHeight();
            totalHeight += verticalGap + section->gainControl->getPreferredHeight();
        }

        totalHeight += verticalGap;
    }

    return totalHeight + verticalGap;
}

int VxAudioProcessorEditor::getActiveFilterContentHeight() const
{
    if (mieModuleLoaded || mxeModuleLoaded || tseModuleLoaded)
        return 0;

    if (speModuleLoaded)
    {
        return getSpeMainContentHeight() + moduleContentBottomGap;
    }

    if (eqeModuleLoaded)
        return getFilterContentHeight();

    return 0;
}

void VxAudioProcessorEditor::resetAnalyserPanelBounds()
{
    if (speAnalyserComponent != nullptr)
        speAnalyserComponent->setBounds({});

    speAnalyserViewport.setBounds({});
    speAnalyserContent.setSize(0, 0);
}

void VxAudioProcessorEditor::layoutGlobalControlsSection(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    hostParametersViewport.setVisible(false);
    hostParametersViewport.setBounds({});
    hostParametersContent.setSize(0, 0);

    auto globalControlsBounds = bounds.removeFromTop(rowHeight);
    globalControlsBounds.removeFromLeft(editorInsetX);
    globalControlsBounds.removeFromRight(editorInsetX);

    std::array<BoxTextButton*, 5> panelButtons {
        undoButton.get(),
        redoButton.get(),
        globalBypassButton.get(),
        clipButton.get(),
        hostButton.get()
    };

    auto visiblePanelButtonCount = 0;

    for (auto* button : panelButtons)
    {
        if (button == nullptr)
            continue;

        button->setBounds({});

        if (button->isVisible())
            ++visiblePanelButtonCount;
    }

    if (visiblePanelButtonCount > 0)
    {
        auto remainingBounds = globalControlsBounds;
        const auto totalGap = parameterGap * (visiblePanelButtonCount - 1);
        const auto baseButtonWidth = juce::jmax(0, (remainingBounds.getWidth() - totalGap) / visiblePanelButtonCount);
        auto placedButtonCount = 0;

        for (auto* button : panelButtons)
        {
            if (button == nullptr || ! button->isVisible())
                continue;

            const auto isLastButton = placedButtonCount + 1 == visiblePanelButtonCount;
            auto buttonBounds = isLastButton ? remainingBounds
                                             : remainingBounds.removeFromLeft(baseButtonWidth);
            button->setBounds(buttonBounds);
            ++placedButtonCount;

            if (! isLastButton)
                remainingBounds.removeFromLeft(parameterGap);
        }
    }

    if (! bounds.isEmpty())
        bounds.removeFromTop(globalToFilterGap);

    if (! hostParametersExpanded)
    {
        return;
    }

    const auto minimumBelowGlobalControls = addFilterToFooterGap;
    const auto hostPanelViewportHeight = juce::jmax(0, bounds.getHeight() - minimumBelowGlobalControls);
    auto hostPanelBounds = bounds.removeFromTop(hostPanelViewportHeight);

    if (! hostPanelBounds.isEmpty())
    {
        if (! hostPanelBounds.isEmpty())
            hostPanelBounds.removeFromBottom(verticalGap);

        const auto slotCount = static_cast<int>(hostSlotButtons.size());
        const auto hostContentHeight = slotCount > 0
            ? (slotCount * rowHeight) + ((slotCount - 1) * verticalGap)
            : 0;

        auto hostViewportBounds = hostPanelBounds;
        hostViewportBounds.removeFromLeft(editorInsetX);
        hostViewportBounds.removeFromRight(editorInsetX);
        hostViewportBounds.setHeight(juce::jmin(hostViewportBounds.getHeight(), hostContentHeight));
        hostParametersViewport.setVisible(true);
        hostParametersViewport.setBounds(hostViewportBounds);

        hostParametersContent.setSize(hostViewportBounds.getWidth(),
                                       juce::jmax(hostViewportBounds.getHeight(), hostContentHeight));

        auto hostContentBounds = hostParametersContent.getLocalBounds();

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

    if (focusedParameterControl == nullptr)
        return;

    if (! bounds.isEmpty())
        bounds.removeFromBottom(verticalGap);

    auto focusedBounds = bounds.removeFromBottom(footerHeight);
    focusedBounds.removeFromLeft(editorInsetX);
    focusedBounds.removeFromRight(editorInsetX);
    focusedParameterControl->setBounds(focusedBounds);
}

void VxAudioProcessorEditor::layoutModuleTabButton(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    if (moduleTabButton == nullptr)
        return;

    moduleTabButton->setBounds({});

    if (! moduleTabButton->isVisible())
        return;

    auto rowBounds = bounds.removeFromTop(rowHeight);
    rowBounds.removeFromLeft(editorInsetX);
    rowBounds.removeFromRight(editorInsetX);
    moduleTabButton->setBounds(rowBounds);

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);
}

void VxAudioProcessorEditor::finalizeLayout() noexcept
{
    updateTooltipBoundsConstraint();
    shell_parameter_focus::clearFocusIfNotShowing();

    if (moduleTabButton != nullptr)
        moduleTabButton->toFront(false);

    clipButton->toFront(false);
    if (undoButton != nullptr) undoButton->toFront(false);
    if (redoButton != nullptr) redoButton->toFront(false);
    if (globalBypassButton != nullptr) globalBypassButton->toFront(false);
    if (moduleAddButton != nullptr) moduleAddButton->toFront(false);
    if (hostButton != nullptr) hostButton->toFront(false);
    footerTab->toFront(false);

    if (focusedParameterControl != nullptr)
        focusedParameterControl->toFront(false);

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
        || clipButton == nullptr
        || presetsSection == nullptr
        || globalBypassButton == nullptr
        || clearFiltersButton == nullptr
        || undoButton == nullptr
        || redoButton == nullptr
        || sortPlaceButton == nullptr
        || sortFreqButton == nullptr
        || sortDuoButton == nullptr
        || footerTab == nullptr)
        return;

    if (eqeModuleLoaded && addFilterButton == nullptr)
        return;

    for (const auto& section : filterSections)
    {
        if (eqeModuleLoaded && section == nullptr)
            return;
    }

    auto bounds = getLocalBounds();

    resetAnalyserPanelBounds();

    const auto editorInsetX = getEditorInsetX(bounds.getWidth());
    const auto totalHeight = bounds.getHeight();
    const auto editorInsetTop = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetTopRatio);
    const auto editorInsetBottom = juce::roundToInt(static_cast<float>(totalHeight) * editorInsetBottomRatio);

    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);

    bounds.removeFromBottom(editorInsetBottom);
    bounds.removeFromTop(editorInsetTop);

    layoutFooter(bounds, editorInsetX);
    layoutGlobalControlsSection(bounds, editorInsetX);

    if (! eqeModuleLoaded && ! speModuleLoaded && ! mieModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
    {
        layoutNoModuleState(bounds);
        finalizeLayout();
        return;
    }

    layoutModuleTabButton(bounds, editorInsetX);

    if (mieModuleLoaded || mxeModuleLoaded || tseModuleLoaded)
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
