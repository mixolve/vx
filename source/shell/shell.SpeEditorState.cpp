#include "shell.EditorParameterControls.h"

void VxAudioProcessorEditor::loadSpeModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::spe))
        return;

    speModuleLoaded = true;
    eqeModuleLoaded = false;
    mieModuleLoaded = false;
    mxeModuleLoaded = false;
    tseModuleLoaded = false;

    shellGlobalHostExpanded = false;
    visualizerExpanded = false;

    rebindActiveModuleEditors();
    updateEditorWidthForVisualizerVisibility();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
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
                        speDspSlopeControl != nullptr ? speDspSlopeControl->getPreferredHeight() : 0,
                        speDualMonoLeftThresholdControl != nullptr ? speDualMonoLeftThresholdControl->getPreferredHeight() : 0,
                        speDualMonoLeftAdaptiveControl != nullptr ? speDualMonoLeftAdaptiveControl->getPreferredHeight() : 0,
                        speDualMonoLeftAdaptiveOffsetControl != nullptr ? speDualMonoLeftAdaptiveOffsetControl->getPreferredHeight() : 0,
                        speDualMonoRightThresholdControl != nullptr ? speDualMonoRightThresholdControl->getPreferredHeight() : 0,
                        speDualMonoRightAdaptiveControl != nullptr ? speDualMonoRightAdaptiveControl->getPreferredHeight() : 0,
                        speDualMonoRightAdaptiveOffsetControl != nullptr ? speDualMonoRightAdaptiveOffsetControl->getPreferredHeight() : 0,
                        rowHeight,
                        rowHeight });
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
                        speAnalyserTimeControl != nullptr ? speAnalyserTimeControl->getPreferredHeight() : 0 });
}

int VxAudioProcessorEditor::getSpeSectionContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    auto contentHeight = getSpeMainContentHeight() + verticalGap + rowHeight;

    if (visualizerExpanded)
        contentHeight += verticalGap + getSpeAnalyserContentHeight();

    return contentHeight + verticalGap;
}
