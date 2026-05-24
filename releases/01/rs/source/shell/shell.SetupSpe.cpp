#include "shell.EditorParameterControls.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

void VxAudioProcessorEditor::setupSpeControls(juce::AudioProcessorValueTreeState& speState)
{
        speInputGainControl = std::make_unique<ParameterControl>(speState,
                                                                 SpeModuleProcessor::paramInputGainId,
                                                                 "IN-GAIN",
                                                                 2);
        speInputGainControl->setTitleLongPressAction([this]
        {
            if (speInputGainControl != nullptr)
                speInputGainControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        globalContent.addAndMakeVisible(*speInputGainControl);

        speAttackControl = std::make_unique<ParameterControl>(speState,
                                                              SpeModuleProcessor::paramAttackId,
                                                              "ATTACK",
                                                              0);
        speAttackControl->setTitleLongPressAction([this]
        {
            if (speAttackControl != nullptr)
                speAttackControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speAttackControl);

        speReleaseControl = std::make_unique<ParameterControl>(speState,
                                                               SpeModuleProcessor::paramReleaseId,
                                                               "RELEASE",
                                                               0);
        speReleaseControl->setTitleLongPressAction([this]
        {
            if (speReleaseControl != nullptr)
                speReleaseControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speReleaseControl);

        speKneeControl = std::make_unique<ParameterControl>(speState,
                                                            SpeModuleProcessor::paramKneeId,
                                                            "KNEE",
                                                            2);
        speKneeControl->setTitleLongPressAction([this]
        {
            if (speKneeControl != nullptr)
                speKneeControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speKneeControl);

        speRatioControl = std::make_unique<ParameterControl>(speState,
                                                             SpeModuleProcessor::paramRatioId,
                                                             "RATIO",
                                                             2);
        speRatioControl->setTitleLongPressAction([this]
        {
            if (speRatioControl != nullptr)
                speRatioControl->setValue(100.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speRatioControl);

        speDspFftSizeControl = std::make_unique<ParameterControl>(speState,
                                                                  SpeModuleProcessor::paramDspFftSizeId,
                                                                  "WIN-SIZE",
                                                                  0);
        addAndMakeVisible(*speDspFftSizeControl);

        speDspSlopeControl = std::make_unique<ParameterControl>(speState,
                                                                SpeModuleProcessor::paramDspSlopeId,
                                                                "SLOPE",
                                                                2);
        speDspSlopeControl->setTitleLongPressAction([this]
        {
            if (speDspSlopeControl != nullptr)
                speDspSlopeControl->setValue(4.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speDspSlopeControl);

        speMakeupControl = std::make_unique<ParameterControl>(speState,
                                                              SpeModuleProcessor::paramMakeupId,
                                                              "OUT-GAIN",
                                                              2);
        speMakeupControl->setTitleLongPressAction([this]
        {
            if (speMakeupControl != nullptr)
                speMakeupControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        globalContent.addAndMakeVisible(*speMakeupControl);

        speDeltaButton = std::make_unique<BoxTextButton>(uiAccent);
        speDeltaButton->setButtonText("DELTA");
        speDeltaButton->setTextJustification(juce::Justification::centred);
        speDeltaButton->setClickingTogglesState(true);
        speDeltaAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                SpeModuleProcessor::paramDeltaId,
                                                                *speDeltaButton);
        speDeltaButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        globalContent.addAndMakeVisible(*speDeltaButton);

        speThresholdControl = std::make_unique<ParameterControl>(speState,
                                                                 SpeModuleProcessor::paramThresholdId,
                                                                 "THRESH",
                                                                 2);
        speThresholdControl->setTitleLongPressAction([this]
        {
            if (speThresholdControl != nullptr)
                speThresholdControl->setValue(12.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speThresholdControl);

        speStereoAdaptiveControl = std::make_unique<ParameterControl>(speState,
                                                                      SpeModuleProcessor::paramStereoAdaptiveId,
                                                                      "ADAP",
                                                                      0);
        speStereoAdaptiveControl->setTitleLongPressAction([this]
        {
            if (speStereoAdaptiveControl != nullptr)
                speStereoAdaptiveControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speStereoAdaptiveControl);

        speStereoAdaptiveOffsetControl = std::make_unique<ParameterControl>(speState,
                                                                            SpeModuleProcessor::paramStereoAdaptiveOffsetId,
                                                                            "OFFSET",
                                                                            2);
        speStereoAdaptiveOffsetControl->setTitleLongPressAction([this]
        {
            if (speStereoAdaptiveOffsetControl != nullptr)
                speStereoAdaptiveOffsetControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speStereoAdaptiveOffsetControl);

        speStereoBypassButton = std::make_unique<BoxTextButton>(uiAccent);
        speStereoBypassButton->setButtonText("BYPASS");
        speStereoBypassButton->setTextJustification(juce::Justification::centred);
        speStereoBypassButton->setClickingTogglesState(true);
        speStereoBypassAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                       SpeModuleProcessor::paramBypassId,
                                                                       *speStereoBypassButton);
        speStereoBypassButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        addAndMakeVisible(*speStereoBypassButton);

        speBypassButton = std::make_unique<BoxTextButton>(uiAccent);
        speBypassButton->setButtonText("BYPASS");
        speBypassButton->setTextJustification(juce::Justification::centred);
        speBypassButton->setClickingTogglesState(true);
        speBypassAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                 SpeModuleProcessor::paramMiscBypassId,
                                                                 *speBypassButton);
        speBypassButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        globalContent.addAndMakeVisible(*speBypassButton);

        speBypassWithGainButton = std::make_unique<BoxTextButton>(uiAccent);
        speBypassWithGainButton->setButtonText("BYPASS.WT-GAIN");
        speBypassWithGainButton->setTextJustification(juce::Justification::centred);
        speBypassWithGainButton->setClickingTogglesState(true);
        speBypassWithGainAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                         SpeModuleProcessor::paramMiscBypassWithGainId,
                                                                         *speBypassWithGainButton);
        speBypassWithGainButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        globalContent.addAndMakeVisible(*speBypassWithGainButton);

        speDualMonoLeftThresholdControl = std::make_unique<ParameterControl>(speState,
                                                                             SpeModuleProcessor::paramDualMonoLeftThresholdId,
                                                                             "LL-THRESH",
                                                                             2);
        speDualMonoLeftThresholdControl->setTitleLongPressAction([this]
        {
            if (speDualMonoLeftThresholdControl != nullptr)
                speDualMonoLeftThresholdControl->setValue(12.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speDualMonoLeftThresholdControl);

        speDualMonoLeftAdaptiveControl = std::make_unique<ParameterControl>(speState,
                                                                            SpeModuleProcessor::paramDualMonoLeftAdaptiveId,
                                                                            "LL-ADAP",
                                                                            0);
        speDualMonoLeftAdaptiveControl->setTitleLongPressAction([this]
        {
            if (speDualMonoLeftAdaptiveControl != nullptr)
                speDualMonoLeftAdaptiveControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speDualMonoLeftAdaptiveControl);

        speDualMonoLeftAdaptiveOffsetControl = std::make_unique<ParameterControl>(speState,
                                                                                  SpeModuleProcessor::paramDualMonoLeftAdaptiveOffsetId,
                                                                                  "LL-OFFSET",
                                                                                  2);
        speDualMonoLeftAdaptiveOffsetControl->setTitleLongPressAction([this]
        {
            if (speDualMonoLeftAdaptiveOffsetControl != nullptr)
                speDualMonoLeftAdaptiveOffsetControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speDualMonoLeftAdaptiveOffsetControl);

        speDualMonoRightThresholdControl = std::make_unique<ParameterControl>(speState,
                                                                              SpeModuleProcessor::paramDualMonoRightThresholdId,
                                                                              "RR-THRESH",
                                                                              2);
        speDualMonoRightThresholdControl->setTitleLongPressAction([this]
        {
            if (speDualMonoRightThresholdControl != nullptr)
                speDualMonoRightThresholdControl->setValue(12.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speDualMonoRightThresholdControl);

        speDualMonoRightAdaptiveControl = std::make_unique<ParameterControl>(speState,
                                                                             SpeModuleProcessor::paramDualMonoRightAdaptiveId,
                                                                             "RR-ADAP",
                                                                             0);
        speDualMonoRightAdaptiveControl->setTitleLongPressAction([this]
        {
            if (speDualMonoRightAdaptiveControl != nullptr)
                speDualMonoRightAdaptiveControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speDualMonoRightAdaptiveControl);

        speDualMonoRightAdaptiveOffsetControl = std::make_unique<ParameterControl>(speState,
                                                                                   SpeModuleProcessor::paramDualMonoRightAdaptiveOffsetId,
                                                                                   "RR-OFFSET",
                                                                                   2);
        speDualMonoRightAdaptiveOffsetControl->setTitleLongPressAction([this]
        {
            if (speDualMonoRightAdaptiveOffsetControl != nullptr)
                speDualMonoRightAdaptiveOffsetControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        addAndMakeVisible(*speDualMonoRightAdaptiveOffsetControl);

        speDualMonoBypassButton = std::make_unique<BoxTextButton>(uiAccent);
        speDualMonoBypassButton->setButtonText("BYPASS");
        speDualMonoBypassButton->setTextJustification(juce::Justification::centred);
        speDualMonoBypassButton->setClickingTogglesState(true);
        speDualMonoBypassAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                         SpeModuleProcessor::paramDualMonoBypassId,
                                                                         *speDualMonoBypassButton);
        speDualMonoBypassButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        addAndMakeVisible(*speDualMonoBypassButton);

        speAnalyserFftSizeControl = std::make_unique<ParameterControl>(speState,
                                                                       SpeModuleProcessor::paramFftSizeId,
                                                                       "FFT-SIZE",
                                                                       0);
        filterContent.addAndMakeVisible(*speAnalyserFftSizeControl);

        speAnalyserOverlapControl = std::make_unique<ParameterControl>(speState,
                                                                       SpeModuleProcessor::paramOverlapId,
                                                                       "OVERLAP",
                                                                       0);
        filterContent.addAndMakeVisible(*speAnalyserOverlapControl);

        speAnalyserLeftControl = std::make_unique<ParameterControl>(speState,
                                                                    SpeModuleProcessor::paramLeftId,
                                                                    "LEFT",
                                                                    0);
        speAnalyserLeftControl->setTitleLongPressAction([this]
        {
            if (speAnalyserLeftControl != nullptr)
                speAnalyserLeftControl->setValue(21.0, true);

            clearKeyboardFocus(*this);
        });
        filterContent.addAndMakeVisible(*speAnalyserLeftControl);

        speAnalyserRightControl = std::make_unique<ParameterControl>(speState,
                                                                     SpeModuleProcessor::paramRightId,
                                                                     "RIGHT",
                                                                     0);
        speAnalyserRightControl->setTitleLongPressAction([this]
        {
            if (speAnalyserRightControl != nullptr)
                speAnalyserRightControl->setValue(20000.0, true);

            clearKeyboardFocus(*this);
        });
        filterContent.addAndMakeVisible(*speAnalyserRightControl);

        speAnalyserRangeLowControl = std::make_unique<ParameterControl>(speState,
                                                                        SpeModuleProcessor::paramRangeLowId,
                                                                        "LOW",
                                                                        2);
        speAnalyserRangeLowControl->setTitleLongPressAction([this]
        {
            if (speAnalyserRangeLowControl != nullptr)
                speAnalyserRangeLowControl->setValue(-60.0, true);

            clearKeyboardFocus(*this);
        });
        filterContent.addAndMakeVisible(*speAnalyserRangeLowControl);

        speAnalyserRangeHighControl = std::make_unique<ParameterControl>(speState,
                                                                         SpeModuleProcessor::paramRangeHighId,
                                                                         "HIGH",
                                                                         2);
        speAnalyserRangeHighControl->setTitleLongPressAction([this]
        {
            if (speAnalyserRangeHighControl != nullptr)
                speAnalyserRangeHighControl->setValue(10.0, true);

            clearKeyboardFocus(*this);
        });
        filterContent.addAndMakeVisible(*speAnalyserRangeHighControl);

        speAnalyserSlopeControl = std::make_unique<ParameterControl>(speState,
                                                                     SpeModuleProcessor::paramSlopeId,
                                                                     "SLOPE",
                                                                     2);
        speAnalyserSlopeControl->setTitleLongPressAction([this]
        {
            if (speAnalyserSlopeControl != nullptr)
                speAnalyserSlopeControl->setValue(4.0, true);

            clearKeyboardFocus(*this);
        });
        filterContent.addAndMakeVisible(*speAnalyserSlopeControl);

        speAnalyserTimeControl = std::make_unique<ParameterControl>(speState,
                                                                    SpeModuleProcessor::paramTimeId,
                                                                    "TIME",
                                                                    0);
        speAnalyserTimeControl->setTitleLongPressAction([this]
        {
            if (speAnalyserTimeControl != nullptr)
                speAnalyserTimeControl->setValue(50.0, true);

            clearKeyboardFocus(*this);
        });
        filterContent.addAndMakeVisible(*speAnalyserTimeControl);

        speAnalyserVisibilityButton = std::make_unique<BoxTextButton>(uiGrey500);
        speAnalyserVisibilityButton->setButtonText("SHOW");
        speAnalyserVisibilityButton->setClickingTogglesState(true);
        speAnalyserVisibilityButton->onClick = [this]
        {
            const auto preservedScrollY = filterViewport.getViewPositionY();
            visualizerVisible = speAnalyserVisibilityButton->getToggleState();
            updateEditorWidthForVisualizerVisibility();
            updateSectionStates();
            resized();
            const auto maxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
            filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, preservedScrollY));
            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*speAnalyserVisibilityButton);

}
