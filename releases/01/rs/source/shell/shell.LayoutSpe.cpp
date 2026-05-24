#include "shell.EditorBellSection.h"
#include "shell.Constants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::layoutSpeModuleSections(juce::Rectangle<int>& bounds, const int editorInsetX)
{
    auto placeSpeHeader = [&bounds, editorInsetX] (BoxTextButton& header)
    {
        auto headerBounds = bounds.removeFromTop(rowHeight);
        headerBounds.removeFromLeft(editorInsetX);
        headerBounds.removeFromRight(editorInsetX);
        header.setBounds(headerBounds);

        if (! bounds.isEmpty())
            bounds.removeFromTop(verticalGap);
    };

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

    auto sumHeights = [] (std::initializer_list<int> heights)
    {
        auto total = 0;

        for (const auto height : heights)
            total += height;

        return total + (verticalGap * juce::jmax(0, static_cast<int>(heights.size()) - 1));
    };

    const auto stereoSectionHeight = rowHeight + verticalGap + (filtersExpanded
        ? sumHeights({ speThresholdControl->getPreferredHeight(),
                       speStereoAdaptiveControl->getPreferredHeight(),
                       speStereoAdaptiveOffsetControl->getPreferredHeight(),
                       rowHeight })
        : 0);
    const auto mainSectionHeight = rowHeight + verticalGap + (speMainExpanded
        ? getSpeMainContentHeight()
        : 0);
    const auto dualMonoSectionHeight = rowHeight + verticalGap + (presetsExpanded
        ? sumHeights({ speDualMonoLeftThresholdControl->getPreferredHeight(),
                       speDualMonoLeftAdaptiveControl->getPreferredHeight(),
                       speDualMonoLeftAdaptiveOffsetControl->getPreferredHeight(),
                       speDualMonoRightThresholdControl->getPreferredHeight(),
                       speDualMonoRightAdaptiveControl->getPreferredHeight(),
                       speDualMonoRightAdaptiveOffsetControl->getPreferredHeight(),
                       rowHeight })
        : 0);
    const auto analyserReservedHeight = rowHeight + (visualizerExpanded ? verticalGap : 0);

    placeSpeHeader(*globalHeader);
    if (globalExpanded)
    {
        const auto miscContentHeight = getSpeMiscContentHeight();
        const auto reservedBelowMisc = verticalGap + mainSectionHeight + stereoSectionHeight + dualMonoSectionHeight + analyserReservedHeight;
        const auto miscViewportHeight = juce::jmin(juce::jmax(0, bounds.getHeight() - reservedBelowMisc),
                                                   miscContentHeight);
        auto globalViewportBounds = bounds.removeFromTop(miscViewportHeight);
        globalViewport.setBounds(globalViewportBounds);
        globalViewport.setVisible(true);
        globalContent.setSize(globalViewportBounds.getWidth(), juce::jmax(globalViewportBounds.getHeight(), miscContentHeight));

        auto miscBounds = globalContent.getLocalBounds();
        placeButton(miscBounds, *speModuleCloseButton);
        placeButton(miscBounds, *speBypassButton);
        placeButton(miscBounds, *speBypassWithGainButton);
        placeControl(miscBounds, *speInputGainControl);
        placeControl(miscBounds, *speMakeupControl);
        placeButton(miscBounds, *speDeltaButton);

        if (! bounds.isEmpty())
            bounds.removeFromTop(verticalGap);
    }
    else
    {
        globalViewport.setBounds({});
        globalViewport.setVisible(false);
        globalContent.setSize(0, 0);
    }

    placeSpeHeader(*speMainHeader);
    if (speMainExpanded)
    {
        placeControl(bounds, *speAttackControl);
        placeControl(bounds, *speReleaseControl);
        placeControl(bounds, *speKneeControl);
        placeControl(bounds, *speRatioControl);
        placeControl(bounds, *speDspFftSizeControl);
        placeControl(bounds, *speDspSlopeControl);
    }

    placeSpeHeader(*filtersHeader);
    if (filtersExpanded)
    {
        placeControl(bounds, *speThresholdControl);
        placeControl(bounds, *speStereoAdaptiveControl);
        placeControl(bounds, *speStereoAdaptiveOffsetControl);
        placeButton(bounds, *speStereoBypassButton);
    }

    placeSpeHeader(*presetsSection->header);
    if (presetsExpanded)
    {
        placeControl(bounds, *speDualMonoLeftThresholdControl);
        placeControl(bounds, *speDualMonoLeftAdaptiveControl);
        placeControl(bounds, *speDualMonoLeftAdaptiveOffsetControl);
        placeControl(bounds, *speDualMonoRightThresholdControl);
        placeControl(bounds, *speDualMonoRightAdaptiveControl);
        placeControl(bounds, *speDualMonoRightAdaptiveOffsetControl);
        placeButton(bounds, *speDualMonoBypassButton);
    }

    auto analyserSectionBounds = bounds.removeFromBottom(juce::jmin(bounds.getHeight(), rowHeight + (visualizerExpanded ? verticalGap + getSpeSectionContentHeight() : 0)));
    auto analyserContentBounds = analyserSectionBounds;
    auto analyserHeaderBounds = analyserContentBounds.removeFromTop(rowHeight);
    analyserHeaderBounds.removeFromLeft(editorInsetX);
    analyserHeaderBounds.removeFromRight(editorInsetX);
    visualizerHeader->setBounds(analyserHeaderBounds);

    if (visualizerExpanded)
    {
        if (! analyserContentBounds.isEmpty())
            analyserContentBounds.removeFromTop(verticalGap);

        const auto analyserContentHeight = getSpeAnalyserContentHeight();
        auto analyserViewportBounds = analyserContentBounds.removeFromTop(juce::jmin(analyserContentBounds.getHeight(),
                                                                                    analyserContentHeight));
        filterViewport.setBounds(analyserViewportBounds);
        filterViewport.setVisible(true);
        filterContent.setSize(analyserViewportBounds.getWidth(), juce::jmax(analyserViewportBounds.getHeight(), analyserContentHeight));

        auto analyserBounds = filterContent.getLocalBounds();
        placeControl(analyserBounds, *speAnalyserFftSizeControl);
        placeControl(analyserBounds, *speAnalyserOverlapControl);
        placeControl(analyserBounds, *speAnalyserLeftControl);
        placeControl(analyserBounds, *speAnalyserRightControl);
        placeControl(analyserBounds, *speAnalyserRangeLowControl);
        placeControl(analyserBounds, *speAnalyserRangeHighControl);
        placeControl(analyserBounds, *speAnalyserSlopeControl);
        placeControl(analyserBounds, *speAnalyserTimeControl);
        placeButton(analyserBounds, *speAnalyserVisibilityButton);
    }
    else
    {
        filterViewport.setBounds({});
        filterViewport.setVisible(false);
        filterContent.setSize(0, 0);
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

    juce::Rectangle<int> mainFrameBounds;
    includeComponentBounds(mainFrameBounds, speMainHeader.get());
    includeComponentBounds(mainFrameBounds, speAttackControl.get());
    includeComponentBounds(mainFrameBounds, speReleaseControl.get());
    includeComponentBounds(mainFrameBounds, speKneeControl.get());
    includeComponentBounds(mainFrameBounds, speRatioControl.get());
    includeComponentBounds(mainFrameBounds, speDspFftSizeControl.get());
    includeComponentBounds(mainFrameBounds, speDspSlopeControl.get());
    placeSectionFrame(speMainSectionFrame.get(), speMainExpanded, mainFrameBounds);

    juce::Rectangle<int> stereoFrameBounds;
    includeComponentBounds(stereoFrameBounds, filtersHeader.get());
    includeComponentBounds(stereoFrameBounds, speThresholdControl.get());
    includeComponentBounds(stereoFrameBounds, speStereoAdaptiveControl.get());
    includeComponentBounds(stereoFrameBounds, speStereoAdaptiveOffsetControl.get());
    includeComponentBounds(stereoFrameBounds, speStereoBypassButton.get());
    placeSectionFrame(filtersSectionFrame.get(), filtersExpanded, stereoFrameBounds);

    juce::Rectangle<int> dualMonoFrameBounds;
    includeComponentBounds(dualMonoFrameBounds, presetsSection != nullptr ? presetsSection->header.get() : nullptr);
    includeComponentBounds(dualMonoFrameBounds, speDualMonoLeftThresholdControl.get());
    includeComponentBounds(dualMonoFrameBounds, speDualMonoLeftAdaptiveControl.get());
    includeComponentBounds(dualMonoFrameBounds, speDualMonoLeftAdaptiveOffsetControl.get());
    includeComponentBounds(dualMonoFrameBounds, speDualMonoRightThresholdControl.get());
    includeComponentBounds(dualMonoFrameBounds, speDualMonoRightAdaptiveControl.get());
    includeComponentBounds(dualMonoFrameBounds, speDualMonoRightAdaptiveOffsetControl.get());
    includeComponentBounds(dualMonoFrameBounds, speDualMonoBypassButton.get());
    placeSectionFrame(presetsSectionFrame.get(), presetsExpanded, dualMonoFrameBounds);

    juce::Rectangle<int> analyserFrameBounds;
    includeComponentBounds(analyserFrameBounds, visualizerHeader.get());
    if (filterViewport.isVisible())
    {
        auto analyserViewportContentBounds = filterViewport.getBounds();
        analyserViewportContentBounds.removeFromLeft(editorInsetX);
        analyserViewportContentBounds.removeFromRight(editorInsetX);
        includeBounds(analyserFrameBounds, analyserViewportContentBounds);
    }
    placeSectionFrame(visualizerSectionFrame.get(), visualizerExpanded, analyserFrameBounds);

    auto moduleFrameBounds = shellGlobalExpanded ? buildShellGlobalFrameBounds()
                                                 : juce::Rectangle<int>();

    if (! shellGlobalExpanded)
    {
        includeModuleTabRowBounds(moduleFrameBounds);

        includeComponentBounds(moduleFrameBounds, globalHeader.get());
        includeComponentBounds(moduleFrameBounds, speMainHeader.get());
        includeComponentBounds(moduleFrameBounds, filtersHeader.get());
        includeComponentBounds(moduleFrameBounds, presetsSection != nullptr ? presetsSection->header.get() : nullptr);
        includeBounds(moduleFrameBounds, globalFrameBounds);
        includeBounds(moduleFrameBounds, mainFrameBounds);
        includeBounds(moduleFrameBounds, stereoFrameBounds);
        includeBounds(moduleFrameBounds, dualMonoFrameBounds);
        includeComponentBounds(moduleFrameBounds, visualizerHeader.get());
        includeBounds(moduleFrameBounds, analyserFrameBounds);
    }

    placeModuleFrame(eqeModuleFrame.get(), true, moduleFrameBounds);
}
