#include "shell.EditorFilterSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::layoutSpeModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    if (speAttackControl == nullptr
        || speReleaseControl == nullptr
        || speKneeControl == nullptr
        || speRatioControl == nullptr
        || speFftProcessorHeader == nullptr
        || speDspFftSizeControl == nullptr
        || speDspHopDivisorControl == nullptr
        || speDspSlopeControl == nullptr
        || speDualMonoLeftThresholdControl == nullptr
        || speDualMonoLeftAdaptiveControl == nullptr
        || speDualMonoLeftAdaptiveOffsetControl == nullptr
        || speDualMonoRightThresholdControl == nullptr
        || speDualMonoRightAdaptiveControl == nullptr
        || speDualMonoRightAdaptiveOffsetControl == nullptr
        || speDynamicProcessorHeader == nullptr
        || spePhaseProcessorHeader == nullptr
        || spePhaseAddButton == nullptr
        || speDualMonoLinkButton == nullptr
        || speDeltaButton == nullptr)
        return;

    auto placeControl = [editorInsetX] (juce::Rectangle<int>& area, auto& control)
    {
        auto controlBounds = area.removeFromTop(control.getPreferredHeight());
        controlBounds.removeFromLeft(editorInsetX);
        controlBounds.removeFromRight(editorInsetX);
        control.setBounds(controlBounds);

        if (! area.isEmpty())
            area.removeFromTop(verticalGap);
    };

    auto placeButton = [editorInsetX] (juce::Rectangle<int>& area, BoxTextButton& button)
    {
        auto buttonBounds = area.removeFromTop(rowHeight);
        buttonBounds.removeFromLeft(editorInsetX);
        buttonBounds.removeFromRight(editorInsetX);
        button.setBounds(buttonBounds);

        if (! area.isEmpty())
            area.removeFromTop(verticalGap);
    };

    filterViewport.setBounds({});
    filterViewport.setVisible(false);
    filterContent.setSize(0, 0);
    speAnalyserViewport.setBounds({});
    speAnalyserViewport.setVisible(false);
    speAnalyserContent.setSize(0, 0);

    auto analyserViewportBounds = bounds.removeFromTop(juce::jmin(bounds.getHeight(), speInlineAnalyserHeight));
    analyserViewportBounds.removeFromLeft(editorInsetX);
    analyserViewportBounds.removeFromRight(editorInsetX);
    speAnalyserViewport.setBounds(analyserViewportBounds);
    speAnalyserViewport.setVisible(true);

    const auto analyserContentHeight = speInlineAnalyserHeight + verticalGap + getSpeAnalyserContentHeight() + moduleContentBottomGap;
    speAnalyserContent.setSize(analyserViewportBounds.getWidth(),
                               juce::jmax(analyserViewportBounds.getHeight(), analyserContentHeight));

    auto analyserContentBounds = speAnalyserContent.getLocalBounds();

    if (speAnalyserComponent != nullptr)
        speAnalyserComponent->setBounds(analyserContentBounds.removeFromTop(speInlineAnalyserHeight));

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);

    auto mainViewportBounds = bounds;
    if (! mainViewportBounds.isEmpty())
        mainViewportBounds.removeFromBottom(viewportToPotentiometerGap);

    auto contentHeight = getSpeMainContentHeight() + moduleContentBottomGap;

    filterViewport.setBounds(mainViewportBounds);
    filterViewport.setVisible(true);
    filterContent.setSize(mainViewportBounds.getWidth(),
                          juce::jmax(mainViewportBounds.getHeight(), contentHeight));

    auto mainBounds = filterContent.getLocalBounds();
    auto fftHeaderBounds = mainBounds.removeFromTop(rowHeight);
    fftHeaderBounds.removeFromLeft(editorInsetX);
    fftHeaderBounds.removeFromRight(editorInsetX);
    speFftProcessorHeader->setBounds(fftHeaderBounds);

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeControl(mainBounds, *speDspFftSizeControl);
    placeControl(mainBounds, *speDspHopDivisorControl);

    auto dynamicHeaderBounds = mainBounds.removeFromTop(rowHeight);
    dynamicHeaderBounds.removeFromLeft(editorInsetX);
    dynamicHeaderBounds.removeFromRight(editorInsetX);
    speDynamicProcessorHeader->setBounds(dynamicHeaderBounds);

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeControl(mainBounds, *speAttackControl);
    placeControl(mainBounds, *speReleaseControl);
    placeControl(mainBounds, *speKneeControl);
    placeControl(mainBounds, *speRatioControl);
    placeControl(mainBounds, *speDspSlopeControl);
    placeControl(mainBounds, *speDualMonoLeftThresholdControl);
    placeControl(mainBounds, *speDualMonoLeftAdaptiveControl);
    placeControl(mainBounds, *speDualMonoLeftAdaptiveOffsetControl);
    placeControl(mainBounds, *speDualMonoRightThresholdControl);
    placeControl(mainBounds, *speDualMonoRightAdaptiveControl);
    placeControl(mainBounds, *speDualMonoRightAdaptiveOffsetControl);
    placeButton(mainBounds, *speDualMonoLinkButton);

    auto phaseHeaderBounds = mainBounds.removeFromTop(rowHeight);
    phaseHeaderBounds.removeFromLeft(editorInsetX);
    phaseHeaderBounds.removeFromRight(editorInsetX);
    spePhaseProcessorHeader->setBounds(phaseHeaderBounds);

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeButton(mainBounds, *spePhaseAddButton);

    const auto activePhaseFilterCount = getActiveSpePhaseFilterCount();
    for (auto filterIndex = 0; filterIndex < activePhaseFilterCount; ++filterIndex)
    {
        auto* removeButton = spePhaseRemoveButtons[static_cast<size_t>(filterIndex)].get();
        auto* headerButton = spePhaseHeaderButtons[static_cast<size_t>(filterIndex)].get();
        auto* typeControl = spePhaseTypeControls[static_cast<size_t>(filterIndex)].get();
        auto* placeChoiceControl = spePhasePlaceControls[static_cast<size_t>(filterIndex)].get();
        auto* slopeControl = spePhaseSlopeControls[static_cast<size_t>(filterIndex)].get();
        auto* frequencyControl = spePhaseFrequencyControls[static_cast<size_t>(filterIndex)].get();
        auto* bandwidthControl = spePhaseBandwidthControls[static_cast<size_t>(filterIndex)].get();
        auto* impactControl = spePhaseImpactControls[static_cast<size_t>(filterIndex)].get();

        if (removeButton == nullptr
            || headerButton == nullptr
            || typeControl == nullptr
            || placeChoiceControl == nullptr
            || slopeControl == nullptr
            || frequencyControl == nullptr
            || bandwidthControl == nullptr
            || impactControl == nullptr)
            continue;

        auto headerBounds = mainBounds.removeFromTop(rowHeight);
        headerBounds.removeFromLeft(editorInsetX);
        headerBounds.removeFromRight(editorInsetX);

        auto removeBounds = headerBounds.removeFromLeft(rowHeight);
        headerBounds.removeFromLeft(parameterGap);
        removeButton->setBounds(removeBounds);
        headerButton->setBounds(headerBounds);

        if (! mainBounds.isEmpty())
            mainBounds.removeFromTop(verticalGap);

        if (! spePhaseExpanded[static_cast<size_t>(filterIndex)])
            continue;

        placeControl(mainBounds, *typeControl);
        placeControl(mainBounds, *placeChoiceControl);
        placeControl(mainBounds, *slopeControl);
        placeControl(mainBounds, *frequencyControl);
        placeControl(mainBounds, *bandwidthControl);
        placeControl(mainBounds, *impactControl);
    }

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeButton(mainBounds, *speDeltaButton);

    if (! analyserContentBounds.isEmpty())
        analyserContentBounds.removeFromTop(verticalGap);

    auto placeAnalyserControl = [] (juce::Rectangle<int>& area, auto& control)
    {
        control.setBounds(area.removeFromTop(control.getPreferredHeight()));

        if (! area.isEmpty())
            area.removeFromTop(verticalGap);
    };

    placeAnalyserControl(analyserContentBounds, *speAnalyserFftSizeControl);
    placeAnalyserControl(analyserContentBounds, *speAnalyserOverlapControl);
    placeAnalyserControl(analyserContentBounds, *speAnalyserLeftControl);
    placeAnalyserControl(analyserContentBounds, *speAnalyserRightControl);
    placeAnalyserControl(analyserContentBounds, *speAnalyserRangeLowControl);
    placeAnalyserControl(analyserContentBounds, *speAnalyserRangeHighControl);
    placeAnalyserControl(analyserContentBounds, *speAnalyserSlopeControl);
    placeAnalyserControl(analyserContentBounds, *speAnalyserTimeControl);

}
