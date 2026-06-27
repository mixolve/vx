#include "shell.EditorBellSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::layoutSpeModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    if (speAttackControl == nullptr
        || speReleaseControl == nullptr
        || speKneeControl == nullptr
        || speRatioControl == nullptr
        || speDspFftSizeControl == nullptr
        || speDspSlopeControl == nullptr
        || speDualMonoLeftThresholdControl == nullptr
        || speDualMonoLeftAdaptiveControl == nullptr
        || speDualMonoLeftAdaptiveOffsetControl == nullptr
        || speDualMonoRightThresholdControl == nullptr
        || speDualMonoRightAdaptiveControl == nullptr
        || speDualMonoRightAdaptiveOffsetControl == nullptr
        || speDualMonoLinkButton == nullptr
        || speDeltaButton == nullptr
        || visualizerHeader == nullptr)
        return;

    auto moduleViewportBounds = bounds;
    if (! moduleViewportBounds.isEmpty())
        moduleViewportBounds.removeFromBottom(addFilterToFooterGap);
    moduleViewportBounds.removeFromLeft(editorInsetX);
    moduleViewportBounds.removeFromRight(editorInsetX);

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

    if (speAnalyserComponent != nullptr)
    {
        auto inlineAnalyserBounds = bounds.removeFromTop(juce::jmin(bounds.getHeight(), speInlineAnalyserHeight));
        inlineAnalyserBounds.removeFromLeft(editorInsetX);
        inlineAnalyserBounds.removeFromRight(editorInsetX);
        speAnalyserComponent->setBounds(inlineAnalyserBounds);
    }

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);

    auto mainViewportBounds = bounds;

    auto contentHeight = getSpeMainContentHeight() + verticalGap + rowHeight;

    if (visualizerExpanded)
        contentHeight += verticalGap + getSpeAnalyserContentHeight();

    contentHeight += verticalGap;

    filterViewport.setBounds(mainViewportBounds);
    filterViewport.setVisible(true);
    filterContent.setSize(mainViewportBounds.getWidth(),
                          juce::jmax(mainViewportBounds.getHeight(), contentHeight));

    auto mainBounds = filterContent.getLocalBounds();
    placeControl(mainBounds, *speAttackControl);
    placeControl(mainBounds, *speReleaseControl);
    placeControl(mainBounds, *speKneeControl);
    placeControl(mainBounds, *speRatioControl);
    placeControl(mainBounds, *speDspFftSizeControl);
    placeControl(mainBounds, *speDspSlopeControl);
    placeControl(mainBounds, *speDualMonoLeftThresholdControl);
    placeControl(mainBounds, *speDualMonoLeftAdaptiveControl);
    placeControl(mainBounds, *speDualMonoLeftAdaptiveOffsetControl);
    placeControl(mainBounds, *speDualMonoRightThresholdControl);
    placeControl(mainBounds, *speDualMonoRightAdaptiveControl);
    placeControl(mainBounds, *speDualMonoRightAdaptiveOffsetControl);
    placeButton(mainBounds, *speDualMonoLinkButton);
    placeButton(mainBounds, *speDeltaButton);
    placeButton(mainBounds, *visualizerHeader);

    if (visualizerExpanded)
    {
        placeControl(mainBounds, *speAnalyserFftSizeControl);
        placeControl(mainBounds, *speAnalyserOverlapControl);
        placeControl(mainBounds, *speAnalyserLeftControl);
        placeControl(mainBounds, *speAnalyserRightControl);
        placeControl(mainBounds, *speAnalyserRangeLowControl);
        placeControl(mainBounds, *speAnalyserRangeHighControl);
        placeControl(mainBounds, *speAnalyserSlopeControl);
        placeControl(mainBounds, *speAnalyserTimeControl);
    }

}
