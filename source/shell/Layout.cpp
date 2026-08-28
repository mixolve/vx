#include "EditorFilterSection.h"
#include "../crossover/ModuleComponent.h"
#include "UiConstants.h"
#include "EditorPresetSections.h"

void AvaAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

juce::Rectangle<int> AvaAudioProcessorEditor::getInfoPromptAnchorBounds() const noexcept
{
    if (footerTab != nullptr && ! footerTab->getBounds().isEmpty())
        return footerTab->getBounds();

    if (clipButton != nullptr
        && clipButton->isVisible()
        && ! clipButton->getBounds().isEmpty())
        return clipButton->getBounds();

    return {};
}

juce::Rectangle<int> AvaAudioProcessorEditor::getInfoPromptVisibleBounds() const noexcept
{
    auto visibleBounds = getLocalBounds();

    if (clipButton != nullptr
        && clipButton->isVisible()
        && ! clipButton->getBounds().isEmpty())
    {
        visibleBounds.setTop(clipButton->getY());
    }

    return visibleBounds;
}

int AvaAudioProcessorEditor::getFilterContentHeight() const
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

int AvaAudioProcessorEditor::getActiveFilterContentHeight() const
{
    if (tlsModuleLoaded || dynModuleLoaded || trsModuleLoaded)
        return 0;

    if (fftModuleLoaded)
    {
        return getFftMainContentHeight() + moduleContentBottomGap;
    }

    if (eqlModuleLoaded)
        return getFilterContentHeight();

    return 0;
}

void AvaAudioProcessorEditor::resetAnalyserPanelBounds()
{
    if (fftAnalyserComponent != nullptr)
        fftAnalyserComponent->setBounds({});
}

void AvaAudioProcessorEditor::layoutGlobalControlsSection(juce::Rectangle<int>& bounds)
{
    hostParametersViewport.setVisible(false);
    hostParametersViewport.setBounds({});
    hostParametersContent.setSize(0, 0);

    auto globalControlsBounds = bounds.removeFromTop(rowHeight);

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
        const auto availableButtonWidth = juce::jmax(0, remainingBounds.getWidth() - totalGap);
        const auto equalButtonWidth = juce::jmax(0, availableButtonWidth / visiblePanelButtonCount);
        auto widthRemainder = juce::jmax(0, availableButtonWidth - (equalButtonWidth * visiblePanelButtonCount));
        auto placedButtonCount = 0;

        for (auto* button : panelButtons)
        {
            if (button == nullptr || ! button->isVisible())
                continue;

            const auto isLastButton = placedButtonCount + 1 == visiblePanelButtonCount;
            auto buttonWidth = equalButtonWidth;

            if (widthRemainder > 0)
            {
                ++buttonWidth;
                --widthRemainder;
            }

            auto buttonBounds = isLastButton ? remainingBounds
                                             : remainingBounds.removeFromLeft(buttonWidth);
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
        hostViewportBounds.setHeight(juce::jmin(hostViewportBounds.getHeight(), hostContentHeight));
        hostParametersViewport.setVisible(true);
        hostParametersViewport.setBounds(hostViewportBounds);

        hostParametersContent.setSize(hostViewportBounds.getWidth(),
                                       juce::jmax(hostViewportBounds.getHeight(), hostContentHeight));

        auto hostContentBounds = hostParametersContent.getLocalBounds();

        for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            auto* slotNameField = hostSlotNameFields[static_cast<size_t>(slotIndex)].get();
            auto* slotButton = hostSlotButtons[static_cast<size_t>(slotIndex)].get();

            if (slotNameField == nullptr || slotButton == nullptr)
                continue;

            auto slotBounds = hostContentBounds.removeFromTop(rowHeight);
            const auto slotNameWidth = (rowHeight * 2) + uiGap + 4;
            auto slotNameBounds = slotBounds.removeFromLeft(slotNameWidth);
            slotBounds.removeFromLeft(parameterGap);
            slotNameField->setBounds(slotNameBounds);
            slotButton->setBounds(slotBounds);

            if (! hostContentBounds.isEmpty())
                hostContentBounds.removeFromTop(verticalGap);
        }

        if (! bounds.isEmpty())
            bounds.removeFromTop(globalToFilterGap);
    }
}

void AvaAudioProcessorEditor::layoutFooter(juce::Rectangle<int>& bounds)
{
    auto footerBounds = bounds.removeFromBottom(footerHeight);
    auto abControlsBounds = footerBounds;

    abSlotAButton->setBounds(abControlsBounds.removeFromLeft(rowHeight));
    abControlsBounds.removeFromLeft(uiGap);
    abSwitchButton->setBounds(abControlsBounds.removeFromLeft(40));
    abControlsBounds.removeFromLeft(uiGap);
    abSlotBButton->setBounds(abControlsBounds.removeFromLeft(rowHeight));
    abControlsBounds.removeFromLeft(uiGap);
    footerTab->setBounds(abControlsBounds);

    if (focusedParameterControl == nullptr)
        return;

    if (! bounds.isEmpty())
        bounds.removeFromBottom(verticalGap);

    auto focusedBounds = bounds.removeFromBottom(footerHeight);
    focusedParameterControl->setBounds(focusedBounds);
}

void AvaAudioProcessorEditor::layoutModuleTitle(juce::Rectangle<int>& bounds)
{
    if (moduleTitle == nullptr)
        return;

    moduleTitle->setBounds({});

    if (! moduleTitle->isVisible())
        return;

    auto rowBounds = bounds.removeFromTop(rowHeight);
    moduleTitle->setBounds(rowBounds);

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);
}

void AvaAudioProcessorEditor::finalizeLayout() noexcept
{
    shell_parameter_focus::clearFocusIfNotShowing(*this);

    if (moduleTitle != nullptr)
        moduleTitle->toFront(false);

    clipButton->toFront(false);
    if (undoButton != nullptr) undoButton->toFront(false);
    if (redoButton != nullptr) redoButton->toFront(false);
    if (abSlotAButton != nullptr) abSlotAButton->toFront(false);
    if (abSwitchButton != nullptr) abSwitchButton->toFront(false);
    if (abSlotBButton != nullptr) abSlotBButton->toFront(false);
    if (globalBypassButton != nullptr) globalBypassButton->toFront(false);
    if (moduleAddButton != nullptr) moduleAddButton->toFront(false);
    if (hostButton != nullptr) hostButton->toFront(false);
    if (fftDeltaButton != nullptr) fftDeltaButton->toFront(false);
    footerTab->toFront(false);

    if (focusedParameterControl != nullptr)
        focusedParameterControl->toFront(false);

    if (horizontalResizeHandle != nullptr)
        horizontalResizeHandle->toFront(false);

    if (verticalResizeHandle != nullptr)
        verticalResizeHandle->toFront(false);

    if (textPromptOverlay != nullptr)
    {
        textPromptOverlay->setBounds(getLocalBounds());
        textPromptOverlay->toFront(false);
    }

    storeEditorStateToValueTree();
}

void AvaAudioProcessorEditor::resized()
{
    if (moduleAddButton == nullptr
        || clipButton == nullptr
        || presetsSection == nullptr
        || globalBypassButton == nullptr
        || undoButton == nullptr
        || redoButton == nullptr
        || abSlotAButton == nullptr
        || abSwitchButton == nullptr
        || abSlotBButton == nullptr
        || sortPlaceButton == nullptr
        || sortFreqButton == nullptr
        || sortDuoButton == nullptr
        || footerTab == nullptr)
        return;

    if (eqlModuleLoaded && addFilterButton == nullptr)
        return;

    for (const auto& section : filterSections)
    {
        if (eqlModuleLoaded && section == nullptr)
            return;
    }

    auto bounds = getLocalBounds();

    constexpr int resizeHandleThickness = 8;

    if (horizontalResizeHandle != nullptr)
        horizontalResizeHandle->setBounds(bounds.withLeft(juce::jmax(0, bounds.getRight() - resizeHandleThickness)));

    if (verticalResizeHandle != nullptr)
        verticalResizeHandle->setBounds(bounds.withTop(juce::jmax(0, bounds.getBottom() - resizeHandleThickness)));

    resetAnalyserPanelBounds();

    const auto editorInsetX = getEditorInsetX(bounds.getWidth());
    const auto totalHeight = bounds.getHeight();
    const auto editorInsetTop = getEditorInsetTop(totalHeight);
    const auto editorInsetBottom = getEditorInsetBottom(totalHeight);

    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);

    bounds.removeFromBottom(editorInsetBottom);
    bounds.removeFromTop(editorInsetTop);

    layoutFooter(bounds);
    layoutGlobalControlsSection(bounds);
    layoutCrossoverSection(bounds);

    if (const auto* crossover = dynamic_cast<CrossoverModuleComponent*>(crossoverEditor.get()))
    {
        if (crossover->isCrossoverSettingsSelected())
        {
            finalizeLayout();
            return;
        }
    }

    if (! eqlModuleLoaded && ! fftModuleLoaded && ! tlsModuleLoaded && ! dynModuleLoaded && ! trsModuleLoaded)
    {
        layoutNoModuleState(bounds);
        finalizeLayout();
        return;
    }

    layoutModuleTitle(bounds);

    if (tlsModuleLoaded || dynModuleLoaded || trsModuleLoaded)
    {
        layoutModuleEditorContent(bounds);
        finalizeLayout();
        return;
    }

    if (fftModuleLoaded)
        layoutFftModuleSections(bounds);
    else
        layoutEqlModuleSections(bounds);

    finalizeLayout();
}
