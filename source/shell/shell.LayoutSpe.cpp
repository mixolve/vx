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

    if (speAnalyserComponent != nullptr)
    {
        auto inlineAnalyserBounds = bounds.removeFromTop(juce::jmin(bounds.getHeight(), speInlineAnalyserHeight));
        inlineAnalyserBounds.removeFromLeft(editorInsetX);
        inlineAnalyserBounds.removeFromRight(editorInsetX);
        speAnalyserComponent->setBounds(inlineAnalyserBounds);
    }

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);

    placeSpeHeader(*globalHeader);
    if (globalExpanded)
    {
        const auto miscContentHeight = getSpeMiscContentHeight();
        const auto minimumMainViewportHeight = speMainExpanded
            ? juce::jmin(getSpeMainContentHeight(), (rowHeight * 3) + (verticalGap * 2))
            : 0;
        const auto reservedBelowMisc = addFilterToFooterGap
            + (verticalGap * 2)
            + rowHeight
            + rowHeight
            + minimumMainViewportHeight;
        const auto miscViewportHeight = juce::jmin(juce::jmax(0, bounds.getHeight() - reservedBelowMisc),
                                                   miscContentHeight);
        auto globalViewportBounds = bounds.removeFromTop(miscViewportHeight);
        globalViewport.setBounds(globalViewportBounds);
        globalViewport.setVisible(true);
        globalContent.setSize(globalViewportBounds.getWidth(), juce::jmax(globalViewportBounds.getHeight(), miscContentHeight));

        auto miscBounds = globalContent.getLocalBounds();
        placeButton(miscBounds, *speModuleCloseButton);
        placeButton(miscBounds, *speDeltaButton);
        placeButton(miscBounds, *speBypassButton);
        placeButton(miscBounds, *speBypassWithGainButton);
        placeControl(miscBounds, *speInputGainControl);
        placeControl(miscBounds, *speInputGainLControl);
        placeControl(miscBounds, *speInputGainRControl);
        placeControl(miscBounds, *speWideControl);
        placeControl(miscBounds, *speMakeupControl);

        if (! bounds.isEmpty())
            bounds.removeFromTop(verticalGap);
    }
    else
    {
        globalViewport.setBounds({});
        globalViewport.setVisible(false);
        globalContent.setSize(0, 0);
    }

    if (! bounds.isEmpty())
        bounds.removeFromBottom(addFilterToFooterGap);

    auto analyserSectionBounds = bounds.removeFromBottom(juce::jmin(bounds.getHeight(),
                                                                    rowHeight + (visualizerExpanded
                                                                                 ? verticalGap + speInlineAnalyserHeight + verticalGap + getSpeAnalyserContentHeight()
                                                                                 : 0)));
    auto analyserContentBounds = analyserSectionBounds;
    auto analyserHeaderBounds = analyserContentBounds.removeFromTop(rowHeight);
    analyserHeaderBounds.removeFromLeft(editorInsetX);
    analyserHeaderBounds.removeFromRight(editorInsetX);
    visualizerHeader->setBounds(analyserHeaderBounds);

    placeSpeHeader(*speMainHeader);

    if (speMainExpanded)
    {
        auto mainViewportBounds = bounds;

        if (visualizerHeader != nullptr && visualizerHeader->isVisible())
        {
            const auto maxMainViewportBottom = visualizerHeader->getY() - verticalGap;

            if (maxMainViewportBottom > mainViewportBounds.getY())
                mainViewportBounds.setBottom(juce::jmin(mainViewportBounds.getBottom(), maxMainViewportBottom));
        }

        filterViewport.setBounds(mainViewportBounds);
        filterViewport.setVisible(true);
        filterContent.setSize(mainViewportBounds.getWidth(),
                              juce::jmax(mainViewportBounds.getHeight(), getSpeMainContentHeight()));

        auto mainBounds = filterContent.getLocalBounds();
        placeButton(mainBounds, *speDualMonoLinkButton);
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
    }
    else if (visualizerExpanded)
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

    if (speMainExpanded && filterViewport.isVisible())
    {
        auto mainViewportContentBounds = filterViewport.getBounds();
        mainViewportContentBounds.removeFromLeft(editorInsetX);
        mainViewportContentBounds.removeFromRight(editorInsetX);
        includeBounds(mainFrameBounds, mainViewportContentBounds);
    }

    if (! mainFrameBounds.isEmpty() && visualizerHeader != nullptr && visualizerHeader->isVisible())
    {
        const auto maxMainFrameBottom = visualizerHeader->getY() - (internalFrameInsetY * 2) - 1;

        if (maxMainFrameBottom > mainFrameBounds.getY())
            mainFrameBounds.setBottom(juce::jmin(mainFrameBounds.getBottom(), maxMainFrameBottom));
    }

    placeSectionFrame(speMainSectionFrame.get(), speMainExpanded, mainFrameBounds);

    placeSectionFrame(filtersSectionFrame.get(), false, {});
    placeSectionFrame(presetsSectionFrame.get(), false, {});

    juce::Rectangle<int> analyserFrameBounds;
    includeComponentBounds(analyserFrameBounds, visualizerHeader.get());
    if (visualizerExpanded)
        includeComponentBounds(analyserFrameBounds, speAnalyserComponent.get());

    if (visualizerExpanded && filterViewport.isVisible())
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

        includeComponentBounds(moduleFrameBounds, speAnalyserComponent.get());

        includeComponentBounds(moduleFrameBounds, globalHeader.get());
        includeComponentBounds(moduleFrameBounds, speMainHeader.get());
        includeBounds(moduleFrameBounds, globalFrameBounds);
        includeBounds(moduleFrameBounds, mainFrameBounds);
        includeComponentBounds(moduleFrameBounds, visualizerHeader.get());
        includeBounds(moduleFrameBounds, analyserFrameBounds);
    }

    placeModuleFrame(eqeModuleFrame.get(), true, moduleFrameBounds);
}
