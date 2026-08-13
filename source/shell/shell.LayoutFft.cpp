#include "shell.EditorFilterSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::layoutFftModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    if (fftAttackControl == nullptr
        || fftReleaseControl == nullptr
        || fftKneeControl == nullptr
        || fftRatioControl == nullptr
        || fftFloorControl == nullptr
        || fftGeneralProcessorHeader == nullptr
        || fftDspFftSizeControl == nullptr
        || fftDspHopDivisorControl == nullptr
        || fftDspSlopeControl == nullptr
        || fftPhaseImpactControl == nullptr
        || fftDualMonoLeftThresholdControl == nullptr
        || fftDualMonoLeftAdaptiveControl == nullptr
        || fftDualMonoRightThresholdControl == nullptr
        || fftDualMonoRightAdaptiveControl == nullptr
        || fftDynamicProcessorHeader == nullptr
        || fftDynamicModeButton == nullptr
        || fftAnalyserSettingsHeader == nullptr
        || fftDualMonoLinkButton == nullptr
        || fftAdaptiveSettingsButton == nullptr
        || fftAdaptiveOffsetControl == nullptr
        || fftAdaptiveAttackControl == nullptr
        || fftAdaptiveHoldControl == nullptr
        || fftAdaptiveReleaseControl == nullptr
        || fftDynamicBypassButton == nullptr
        || fftDeltaButton == nullptr)
        return;

    auto placeControl = [editorInsetX] (juce::Rectangle<int>& area, auto& control)
    {
        if (! control.isVisible())
        {
            control.setBounds({});
            return;
        }

        auto controlBounds = area.removeFromTop(control.getPreferredHeight());
        controlBounds.removeFromLeft(editorInsetX);
        controlBounds.removeFromRight(editorInsetX);
        control.setBounds(controlBounds);

        if (! area.isEmpty())
            area.removeFromTop(verticalGap);
    };

    auto placeButton = [editorInsetX] (juce::Rectangle<int>& area, BoxTextButton& button)
    {
        if (! button.isVisible())
        {
            button.setBounds({});
            return;
        }

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
    fftAnalyserViewport.setBounds({});
    fftAnalyserViewport.setVisible(false);
    fftAnalyserContent.setSize(0, 0);

    auto analyserViewportBounds = bounds.removeFromTop(juce::jmin(bounds.getHeight(), fftInlineAnalyserHeight));
    analyserViewportBounds.removeFromLeft(editorInsetX);
    analyserViewportBounds.removeFromRight(editorInsetX);
    fftAnalyserViewport.setBounds(analyserViewportBounds);
    fftAnalyserViewport.setVisible(true);

    const auto analyserContentHeight = fftInlineAnalyserHeight + verticalGap + getFftAnalyserContentHeight() + moduleContentBottomGap;
    fftAnalyserContent.setSize(analyserViewportBounds.getWidth(),
                               juce::jmax(analyserViewportBounds.getHeight(), analyserContentHeight));

    auto analyserContentBounds = fftAnalyserContent.getLocalBounds();

    if (fftAnalyserComponent != nullptr)
        fftAnalyserComponent->setBounds(analyserContentBounds.removeFromTop(fftInlineAnalyserHeight));

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);

    auto mainViewportBounds = bounds;
    if (! mainViewportBounds.isEmpty())
        mainViewportBounds.removeFromBottom(viewportToPotentiometerGap);

    auto contentHeight = getFftMainContentHeight() + moduleContentBottomGap;

    filterViewport.setBounds(mainViewportBounds);
    filterViewport.setVisible(true);
    filterContent.setSize(mainViewportBounds.getWidth(),
                          juce::jmax(mainViewportBounds.getHeight(), contentHeight));

    auto mainBounds = filterContent.getLocalBounds();
    auto fftHeaderBounds = mainBounds.removeFromTop(rowHeight);
    fftHeaderBounds.removeFromLeft(editorInsetX);
    fftHeaderBounds.removeFromRight(editorInsetX);
    fftGeneralProcessorHeader->setBounds(fftHeaderBounds);

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeControl(mainBounds, *fftDspFftSizeControl);
    placeControl(mainBounds, *fftDspHopDivisorControl);

    auto dynamicHeaderBounds = mainBounds.removeFromTop(rowHeight);
    dynamicHeaderBounds.removeFromLeft(editorInsetX);
    dynamicHeaderBounds.removeFromRight(editorInsetX);
    fftDynamicProcessorHeader->setBounds(dynamicHeaderBounds);

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeButton(mainBounds, *fftDynamicModeButton);
    placeControl(mainBounds, *fftAttackControl);
    placeControl(mainBounds, *fftReleaseControl);
    placeControl(mainBounds, *fftKneeControl);
    placeControl(mainBounds, *fftRatioControl);
    placeControl(mainBounds, *fftFloorControl);
    placeControl(mainBounds, *fftDspSlopeControl);
    placeControl(mainBounds, *fftDualMonoLeftThresholdControl);
    placeControl(mainBounds, *fftDualMonoLeftAdaptiveControl);
    placeControl(mainBounds, *fftPhaseImpactControl);
    placeControl(mainBounds, *fftDualMonoRightThresholdControl);
    placeControl(mainBounds, *fftDualMonoRightAdaptiveControl);
    placeButton(mainBounds, *fftDualMonoLinkButton);
    placeButton(mainBounds, *fftAdaptiveSettingsButton);
    placeControl(mainBounds, *fftAdaptiveOffsetControl);
    placeControl(mainBounds, *fftAdaptiveAttackControl);
    placeControl(mainBounds, *fftAdaptiveHoldControl);
    placeControl(mainBounds, *fftAdaptiveReleaseControl);
    placeButton(mainBounds, *fftDynamicBypassButton);

    placeButton(mainBounds, *fftDeltaButton);

    if (! analyserContentBounds.isEmpty())
        analyserContentBounds.removeFromTop(verticalGap);

    auto placeAnalyserControl = [] (juce::Rectangle<int>& area, auto& control)
    {
        if (! control.isVisible())
        {
            control.setBounds({});
            return;
        }

        control.setBounds(area.removeFromTop(control.getPreferredHeight()));

        if (! area.isEmpty())
            area.removeFromTop(verticalGap);
    };

    fftAnalyserSettingsHeader->setBounds(analyserContentBounds.removeFromTop(rowHeight));

    if (! analyserContentBounds.isEmpty())
        analyserContentBounds.removeFromTop(verticalGap);

    placeAnalyserControl(analyserContentBounds, *fftAnalyserFftSizeControl);
    placeAnalyserControl(analyserContentBounds, *fftAnalyserOverlapControl);
    placeAnalyserControl(analyserContentBounds, *fftAnalyserLeftControl);
    placeAnalyserControl(analyserContentBounds, *fftAnalyserRightControl);
    placeAnalyserControl(analyserContentBounds, *fftAnalyserRangeLowControl);
    placeAnalyserControl(analyserContentBounds, *fftAnalyserRangeHighControl);
    placeAnalyserControl(analyserContentBounds, *fftAnalyserSlopeControl);
    placeAnalyserControl(analyserContentBounds, *fftAnalyserTimeControl);

}
