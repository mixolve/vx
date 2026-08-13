#include "shell.EditorParameterControls.h"
#include "../modules/fft/module.fft.FftProcessor.h"

#include <array>

void VxAudioProcessorEditor::loadFftModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::fft))
        return;

    setLoadedModuleFlags(VxAudioProcessor::ActiveModule::fft);

    hostParametersExpanded = false;
    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

int VxAudioProcessorEditor::getFftMainContentHeight() const
{
    if (! fftModuleLoaded)
        return 0;

    const auto visibleHeight = [] (const auto* component, const int height)
    {
        return component != nullptr && component->isVisible() ? height : 0;
    };
    const std::array<int, 24> heights {
        visibleHeight(fftGeneralProcessorHeader.get(), rowHeight),
        visibleHeight(fftDspFftSizeControl.get(), fftDspFftSizeControl != nullptr ? fftDspFftSizeControl->getPreferredHeight() : 0),
        visibleHeight(fftDspHopDivisorControl.get(), fftDspHopDivisorControl != nullptr ? fftDspHopDivisorControl->getPreferredHeight() : 0),
        visibleHeight(fftDynamicProcessorHeader.get(), rowHeight),
        visibleHeight(fftDynamicModeButton.get(), rowHeight),
        visibleHeight(fftAttackControl.get(), fftAttackControl != nullptr ? fftAttackControl->getPreferredHeight() : 0),
        visibleHeight(fftReleaseControl.get(), fftReleaseControl != nullptr ? fftReleaseControl->getPreferredHeight() : 0),
        visibleHeight(fftKneeControl.get(), fftKneeControl != nullptr ? fftKneeControl->getPreferredHeight() : 0),
        visibleHeight(fftRatioControl.get(), fftRatioControl != nullptr ? fftRatioControl->getPreferredHeight() : 0),
        visibleHeight(fftFloorControl.get(), fftFloorControl != nullptr ? fftFloorControl->getPreferredHeight() : 0),
        visibleHeight(fftDspSlopeControl.get(), fftDspSlopeControl != nullptr ? fftDspSlopeControl->getPreferredHeight() : 0),
        visibleHeight(fftDualMonoLeftThresholdControl.get(), fftDualMonoLeftThresholdControl != nullptr ? fftDualMonoLeftThresholdControl->getPreferredHeight() : 0),
        visibleHeight(fftDualMonoLeftAdaptiveControl.get(), fftDualMonoLeftAdaptiveControl != nullptr ? fftDualMonoLeftAdaptiveControl->getPreferredHeight() : 0),
        visibleHeight(fftPhaseImpactControl.get(), fftPhaseImpactControl != nullptr ? fftPhaseImpactControl->getPreferredHeight() : 0),
        visibleHeight(fftDualMonoRightThresholdControl.get(), fftDualMonoRightThresholdControl != nullptr ? fftDualMonoRightThresholdControl->getPreferredHeight() : 0),
        visibleHeight(fftDualMonoRightAdaptiveControl.get(), fftDualMonoRightAdaptiveControl != nullptr ? fftDualMonoRightAdaptiveControl->getPreferredHeight() : 0),
        visibleHeight(fftDualMonoLinkButton.get(), rowHeight),
        visibleHeight(fftAdaptiveSettingsButton.get(), rowHeight),
        visibleHeight(fftAdaptiveOffsetControl.get(), fftAdaptiveOffsetControl != nullptr ? fftAdaptiveOffsetControl->getPreferredHeight() : 0),
        visibleHeight(fftAdaptiveAttackControl.get(), fftAdaptiveAttackControl != nullptr ? fftAdaptiveAttackControl->getPreferredHeight() : 0),
        visibleHeight(fftAdaptiveHoldControl.get(), fftAdaptiveHoldControl != nullptr ? fftAdaptiveHoldControl->getPreferredHeight() : 0),
        visibleHeight(fftAdaptiveReleaseControl.get(), fftAdaptiveReleaseControl != nullptr ? fftAdaptiveReleaseControl->getPreferredHeight() : 0),
        visibleHeight(fftDynamicBypassButton.get(), rowHeight),
        visibleHeight(fftDeltaButton.get(), rowHeight)
    };
    auto totalHeight = 0;
    auto visibleRows = 0;

    for (const auto height : heights)
    {
        if (height <= 0)
            continue;

        totalHeight += height;
        ++visibleRows;
    }

    return totalHeight + (verticalGap * juce::jmax(0, visibleRows - 1));
}

int VxAudioProcessorEditor::getFftAnalyserContentHeight() const
{
    if (! fftModuleLoaded)
        return 0;

    auto sumHeights = [] (std::initializer_list<int> heights)
    {
        auto totalHeight = 0;
        auto visibleRows = 0;

        for (const auto height : heights)
        {
            if (height <= 0)
                continue;

            totalHeight += height;
            ++visibleRows;
        }

        return totalHeight + (verticalGap * juce::jmax(0, visibleRows - 1));
    };

    const auto visibleHeight = [] (const auto* component, const int height)
    {
        return component != nullptr && component->isVisible() ? height : 0;
    };

    return sumHeights({ visibleHeight(fftAnalyserSettingsHeader.get(), rowHeight),
                        visibleHeight(fftAnalyserFftSizeControl.get(), fftAnalyserFftSizeControl != nullptr ? fftAnalyserFftSizeControl->getPreferredHeight() : 0),
                        visibleHeight(fftAnalyserOverlapControl.get(), fftAnalyserOverlapControl != nullptr ? fftAnalyserOverlapControl->getPreferredHeight() : 0),
                        visibleHeight(fftAnalyserLeftControl.get(), fftAnalyserLeftControl != nullptr ? fftAnalyserLeftControl->getPreferredHeight() : 0),
                        visibleHeight(fftAnalyserRightControl.get(), fftAnalyserRightControl != nullptr ? fftAnalyserRightControl->getPreferredHeight() : 0),
                        visibleHeight(fftAnalyserRangeLowControl.get(), fftAnalyserRangeLowControl != nullptr ? fftAnalyserRangeLowControl->getPreferredHeight() : 0),
                        visibleHeight(fftAnalyserRangeHighControl.get(), fftAnalyserRangeHighControl != nullptr ? fftAnalyserRangeHighControl->getPreferredHeight() : 0),
                        visibleHeight(fftAnalyserSlopeControl.get(), fftAnalyserSlopeControl != nullptr ? fftAnalyserSlopeControl->getPreferredHeight() : 0),
                        visibleHeight(fftAnalyserTimeControl.get(), fftAnalyserTimeControl != nullptr ? fftAnalyserTimeControl->getPreferredHeight() : 0) });
}
