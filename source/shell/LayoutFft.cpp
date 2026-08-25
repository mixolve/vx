#include "EditorFilterSection.h"
#include "UiConstants.h"
#include "EditorPresetSections.h"

void AvaAudioProcessorEditor::layoutFftModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    if (fftAttackControl == nullptr
        || fftReleaseControl == nullptr
        || fftKneeControl == nullptr
        || fftRatioControl == nullptr
        || fftFloorControl == nullptr
        || fftAnalyserRangeControl == nullptr
        || fftAnalyserTimeControl == nullptr
        || fftGeneralProcessorHeader == nullptr
        || fftDspFftSizeControl == nullptr
        || fftDspOverlapControl == nullptr
        || fftDspSlopeControl == nullptr
        || fftPhaseImpactControl == nullptr
        || fftDualMonoLeftThresholdControl == nullptr
        || fftDualMonoLeftAdaptiveControl == nullptr
        || fftDualMonoRightThresholdControl == nullptr
        || fftDualMonoRightAdaptiveControl == nullptr
        || fftDynamicProcessorHeader == nullptr
        || fftDynamicModeControl == nullptr
        || fftDualMonoLinkButton == nullptr
        || fftAdaptiveSettingsHeader == nullptr
        || fftAdaptiveOffsetControl == nullptr
        || fftAdaptiveAttackControl == nullptr
        || fftAdaptiveHoldControl == nullptr
        || fftAdaptiveReleaseControl == nullptr
        || fftDeltaButton == nullptr)
    {
        return;
    }

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

    auto viewportBounds = bounds;

    if (! viewportBounds.isEmpty())
        viewportBounds.removeFromBottom(viewportToPotentiometerGap);

    auto fixedButtonsBounds = viewportBounds.removeFromBottom(rowHeight);
    fixedButtonsBounds.removeFromLeft(editorInsetX);
    fixedButtonsBounds.removeFromRight(editorInsetX);

    fftDeltaButton->setBounds(fftDeltaButton->isVisible() ? fixedButtonsBounds : juce::Rectangle<int>{});

    if (! viewportBounds.isEmpty())
        viewportBounds.removeFromBottom(verticalGap);

    auto analyserBounds = viewportBounds.removeFromTop(fftInlineAnalyserHeight);
    analyserBounds.removeFromLeft(editorInsetX);
    analyserBounds.removeFromRight(editorInsetX);

    if (fftAnalyserComponent != nullptr)
        fftAnalyserComponent->setBounds(analyserBounds);

    if (! viewportBounds.isEmpty())
        viewportBounds.removeFromTop(verticalGap);

    const auto contentHeight = getFftMainContentHeight() + moduleContentBottomGap;
    filterViewport.setBounds(viewportBounds);
    filterViewport.setVisible(true);
    filterContent.setSize(viewportBounds.getWidth(),
                          juce::jmax(viewportBounds.getHeight(), contentHeight));

    auto mainBounds = filterContent.getLocalBounds();
    placeControl(mainBounds, *fftAnalyserRangeControl);
    placeControl(mainBounds, *fftAnalyserTimeControl);
    placeButton(mainBounds, *fftGeneralProcessorHeader);
    placeControl(mainBounds, *fftDspFftSizeControl);
    placeControl(mainBounds, *fftDspOverlapControl);
    placeButton(mainBounds, *fftDynamicProcessorHeader);
    placeControl(mainBounds, *fftDynamicModeControl);
    placeControl(mainBounds, *fftAttackControl);
    placeControl(mainBounds, *fftReleaseControl);
    placeControl(mainBounds, *fftKneeControl);
    placeControl(mainBounds, *fftRatioControl);
    placeControl(mainBounds, *fftDspSlopeControl);

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeControl(mainBounds, *fftDualMonoLeftThresholdControl);
    placeControl(mainBounds, *fftDualMonoLeftAdaptiveControl);
    placeControl(mainBounds, *fftFloorControl);
    placeControl(mainBounds, *fftPhaseImpactControl);
    placeControl(mainBounds, *fftDualMonoRightThresholdControl);
    placeControl(mainBounds, *fftDualMonoRightAdaptiveControl);
    placeButton(mainBounds, *fftDualMonoLinkButton);

    if (! mainBounds.isEmpty())
        mainBounds.removeFromTop(verticalGap);

    placeButton(mainBounds, *fftAdaptiveSettingsHeader);
    placeControl(mainBounds, *fftAdaptiveOffsetControl);
    placeControl(mainBounds, *fftAdaptiveAttackControl);
    placeControl(mainBounds, *fftAdaptiveHoldControl);
    placeControl(mainBounds, *fftAdaptiveReleaseControl);
}
