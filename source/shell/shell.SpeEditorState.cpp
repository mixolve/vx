#include "shell.EditorParameterControls.h"

void VxAudioProcessorEditor::loadSpeModule()
{
    if (! audioProcessor.addModuleToChain(VxAudioProcessor::ActiveModule::spe))
        return;

    speModuleLoaded = true;
    eqeModuleLoaded = false;
    mxeModuleLoaded = false;
    tseModuleLoaded = false;
    eqeModuleExpanded = true;

    shellGlobalExpanded = false;
    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;
    visualizerVisible = false;
    expandedBellIndex = -1;

    rebindActiveModuleEditors();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

int VxAudioProcessorEditor::getSpeMiscContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    auto sumHeights = [] (std::initializer_list<int> heights)
    {
        auto totalHeight = 0;

        for (const auto height : heights)
            totalHeight += height;

        return totalHeight + (verticalGap * juce::jmax(0, static_cast<int>(heights.size()) - 1));
    };

    return sumHeights({ speInputGainControl != nullptr ? speInputGainControl->getPreferredHeight() : 0,
                        speMakeupControl != nullptr ? speMakeupControl->getPreferredHeight() : 0,
                        rowHeight,
                        rowHeight,
                        rowHeight,
                        rowHeight });
}

int VxAudioProcessorEditor::getSpeMainContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    auto sumHeights = [] (std::initializer_list<int> heights)
    {
        auto totalHeight = 0;

        for (const auto height : heights)
            totalHeight += height;

        return totalHeight + (verticalGap * juce::jmax(0, static_cast<int>(heights.size()) - 1));
    };

    return sumHeights({ speAttackControl != nullptr ? speAttackControl->getPreferredHeight() : 0,
                        speReleaseControl != nullptr ? speReleaseControl->getPreferredHeight() : 0,
                        speKneeControl != nullptr ? speKneeControl->getPreferredHeight() : 0,
                        speRatioControl != nullptr ? speRatioControl->getPreferredHeight() : 0,
                        speDspFftSizeControl != nullptr ? speDspFftSizeControl->getPreferredHeight() : 0,
                        speDspSlopeControl != nullptr ? speDspSlopeControl->getPreferredHeight() : 0 });
}

int VxAudioProcessorEditor::getSpeAnalyserContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    auto sumHeights = [] (std::initializer_list<int> heights)
    {
        auto totalHeight = 0;

        for (const auto height : heights)
            totalHeight += height;

        return totalHeight + (verticalGap * juce::jmax(0, static_cast<int>(heights.size()) - 1));
    };

    return sumHeights({ speAnalyserFftSizeControl != nullptr ? speAnalyserFftSizeControl->getPreferredHeight() : 0,
                        speAnalyserOverlapControl != nullptr ? speAnalyserOverlapControl->getPreferredHeight() : 0,
                        speAnalyserLeftControl != nullptr ? speAnalyserLeftControl->getPreferredHeight() : 0,
                        speAnalyserRightControl != nullptr ? speAnalyserRightControl->getPreferredHeight() : 0,
                        speAnalyserRangeLowControl != nullptr ? speAnalyserRangeLowControl->getPreferredHeight() : 0,
                        speAnalyserRangeHighControl != nullptr ? speAnalyserRangeHighControl->getPreferredHeight() : 0,
                        speAnalyserSlopeControl != nullptr ? speAnalyserSlopeControl->getPreferredHeight() : 0,
                        speAnalyserTimeControl != nullptr ? speAnalyserTimeControl->getPreferredHeight() : 0,
                        rowHeight });
}

int VxAudioProcessorEditor::getSpeSectionContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    auto sumHeights = [] (std::initializer_list<int> heights)
    {
        auto totalHeight = 0;

        for (const auto height : heights)
            totalHeight += height;

        return totalHeight + (verticalGap * juce::jmax(0, static_cast<int>(heights.size()) - 1));
    };

    if (globalExpanded)
        return getSpeMiscContentHeight();

    if (speMainExpanded)
        return getSpeMainContentHeight();

    if (filtersExpanded)
        return sumHeights({ speThresholdControl != nullptr ? speThresholdControl->getPreferredHeight() : 0,
                            speStereoAdaptiveControl != nullptr ? speStereoAdaptiveControl->getPreferredHeight() : 0,
                            speStereoAdaptiveOffsetControl != nullptr ? speStereoAdaptiveOffsetControl->getPreferredHeight() : 0,
                            rowHeight });

    if (presetsExpanded)
        return sumHeights({ speDualMonoLeftThresholdControl != nullptr ? speDualMonoLeftThresholdControl->getPreferredHeight() : 0,
                            speDualMonoLeftAdaptiveControl != nullptr ? speDualMonoLeftAdaptiveControl->getPreferredHeight() : 0,
                            speDualMonoLeftAdaptiveOffsetControl != nullptr ? speDualMonoLeftAdaptiveOffsetControl->getPreferredHeight() : 0,
                            speDualMonoRightThresholdControl != nullptr ? speDualMonoRightThresholdControl->getPreferredHeight() : 0,
                            speDualMonoRightAdaptiveControl != nullptr ? speDualMonoRightAdaptiveControl->getPreferredHeight() : 0,
                            speDualMonoRightAdaptiveOffsetControl != nullptr ? speDualMonoRightAdaptiveOffsetControl->getPreferredHeight() : 0,
                            rowHeight });

    if (visualizerExpanded)
        return getSpeAnalyserContentHeight();

    return 0;
}
