#include "shell.EditorParameterControls.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

#include <array>
#include <cmath>
#include <limits>

void VxAudioProcessorEditor::setupSpeControls(juce::AudioProcessorValueTreeState& speState,
                                              SpeModuleProcessor& speProcessor)
{
        auto refreshAnalyserControls = [this, &speProcessor]
        {
            static constexpr std::array<const char*, 5> fftSizeLabels { "1024", "2048", "4096", "8192", "16384" };
            static constexpr std::array<const char*, 5> overlapLabels { "2x", "4x", "8x", "16x", "32x" };

            if (speAnalyserFftSizeControl != nullptr)
            {
                const auto fftIndex = juce::jlimit(0,
                                                   static_cast<int>(fftSizeLabels.size()) - 1,
                                                   juce::roundToInt(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramFftSizeId)));
                speAnalyserFftSizeControl->setValue(static_cast<double>(fftIndex), false);
                speAnalyserFftSizeControl->setOverrideText(fftSizeLabels[static_cast<size_t>(fftIndex)]);
            }

            if (speAnalyserOverlapControl != nullptr)
            {
                const auto overlapIndex = juce::jlimit(0,
                                                       static_cast<int>(overlapLabels.size()) - 1,
                                                       juce::roundToInt(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramOverlapId)));
                speAnalyserOverlapControl->setValue(static_cast<double>(overlapIndex), false);
                speAnalyserOverlapControl->setOverrideText(overlapLabels[static_cast<size_t>(overlapIndex)]);
            }

            if (speAnalyserLeftControl != nullptr)
                speAnalyserLeftControl->setValue(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramLeftId), false);

            if (speAnalyserRightControl != nullptr)
                speAnalyserRightControl->setValue(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramRightId), false);

            if (speAnalyserRangeLowControl != nullptr)
                speAnalyserRangeLowControl->setValue(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramRangeLowId), false);

            if (speAnalyserRangeHighControl != nullptr)
                speAnalyserRangeHighControl->setValue(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramRangeHighId), false);

            if (speAnalyserSlopeControl != nullptr)
                speAnalyserSlopeControl->setValue(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramSlopeId), false);

            if (speAnalyserTimeControl != nullptr)
                speAnalyserTimeControl->setValue(speProcessor.getAnalyserParameterValue(SpeModuleProcessor::paramTimeId), false);
        };

        const auto parseDiscreteAnalyserChoice = [] (const juce::String& text, const std::initializer_list<double> values)
        {
            const auto targetValue = text.retainCharacters("0123456789.-").getDoubleValue();
            auto bestIndex = 0;
            auto bestDistance = std::numeric_limits<double>::max();
            auto choiceIndex = 0;

            for (const auto choiceValue : values)
            {
                const auto distance = std::abs(choiceValue - targetValue);

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = choiceIndex;
                }

                ++choiceIndex;
            }

            return static_cast<double>(bestIndex);
        };

        speInputGainControl = std::make_unique<ParameterControl>(speState,
                                                                 SpeModuleProcessor::paramInputGainLrId,
                                                                 "IN-GAIN-LR",
                                                                 2);
        speInputGainControl->setTitleLongPressAction([this]
        {
            if (speInputGainControl != nullptr)
                speInputGainControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        globalContent.addAndMakeVisible(*speInputGainControl);

        speInputGainLControl = std::make_unique<ParameterControl>(speState,
                                                                  SpeModuleProcessor::paramInputGainLId,
                                                                  "IN-GAIN-L",
                                                                  2);
        speInputGainLControl->setTitleLongPressAction([this]
        {
            if (speInputGainLControl != nullptr)
                speInputGainLControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        globalContent.addAndMakeVisible(*speInputGainLControl);

        speInputGainRControl = std::make_unique<ParameterControl>(speState,
                                                                  SpeModuleProcessor::paramInputGainRId,
                                                                  "IN-GAIN-R",
                                                                  2);
        speInputGainRControl->setTitleLongPressAction([this]
        {
            if (speInputGainRControl != nullptr)
                speInputGainRControl->setValue(0.0, true);

            clearKeyboardFocus(*this);
        });
        globalContent.addAndMakeVisible(*speInputGainRControl);

        speWideControl = std::make_unique<ParameterControl>(speState,
                                                            SpeModuleProcessor::paramWideId,
                                                            "WIDE",
                                                            0);
        speWideControl->setTitleLongPressAction([this]
        {
            if (speWideControl != nullptr)
                speWideControl->setValue(100.0, true);

            clearKeyboardFocus(*this);
        });
        globalContent.addAndMakeVisible(*speWideControl);

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
                                                                             "L.THRESH",
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
                                                                            "L.ADAP",
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
                                                                                  "L.OFFSET",
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
                                                                              "R.THRESH",
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
                                                                             "R.ADAP",
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
                                                                                   "R.OFFSET",
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

        speAnalyserFftSizeControl = std::make_unique<LocalParameterControl>("FFT-SIZE",
                                                                            0,
                                                                            0.0,
                                                                            4.0,
                                                                            1.0,
                                                                            3.0);
        speAnalyserFftSizeControl->setTextToValueParser([parseDiscreteAnalyserChoice] (const juce::String& text)
        {
            return parseDiscreteAnalyserChoice(text, { 1024.0, 2048.0, 4096.0, 8192.0, 16384.0 });
        });
        speAnalyserFftSizeControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramFftSizeId,
                                                   static_cast<float>(speAnalyserFftSizeControl->getValue()));
            refreshAnalyserControls();
        };
        filterContent.addAndMakeVisible(*speAnalyserFftSizeControl);

        speAnalyserOverlapControl = std::make_unique<LocalParameterControl>("OVERLAP",
                                                                            0,
                                                                            0.0,
                                                                            4.0,
                                                                            1.0,
                                                                            4.0);
        speAnalyserOverlapControl->setTextToValueParser([parseDiscreteAnalyserChoice] (const juce::String& text)
        {
            return parseDiscreteAnalyserChoice(text, { 2.0, 4.0, 8.0, 16.0, 32.0 });
        });
        speAnalyserOverlapControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramOverlapId,
                                                   static_cast<float>(speAnalyserOverlapControl->getValue()));
            refreshAnalyserControls();
        };
        filterContent.addAndMakeVisible(*speAnalyserOverlapControl);

        speAnalyserLeftControl = std::make_unique<LocalParameterControl>("LEFT",
                                                                         0,
                                                                         0.0,
                                                                         1000.0,
                                                                         1.0,
                                                                         21.0,
                                                                         0.0,
                                                                         false,
                                                                         true);
        speAnalyserLeftControl->setTitleLongPressAction([this]
        {
            if (speAnalyserLeftControl != nullptr)
                speAnalyserLeftControl->setValue(21.0, true);

            clearKeyboardFocus(*this);
        });
        speAnalyserLeftControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramLeftId,
                                                   static_cast<float>(speAnalyserLeftControl->getValue()));
            refreshAnalyserControls();
        };
        filterContent.addAndMakeVisible(*speAnalyserLeftControl);

        speAnalyserRightControl = std::make_unique<LocalParameterControl>("RIGHT",
                                                                          0,
                                                                          1000.0,
                                                                          24000.0,
                                                                          1.0,
                                                                          20000.0,
                                                                          0.0,
                                                                          false,
                                                                          true);
        speAnalyserRightControl->setTitleLongPressAction([this]
        {
            if (speAnalyserRightControl != nullptr)
                speAnalyserRightControl->setValue(20000.0, true);

            clearKeyboardFocus(*this);
        });
        speAnalyserRightControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramRightId,
                                                   static_cast<float>(speAnalyserRightControl->getValue()));
            refreshAnalyserControls();
        };
        filterContent.addAndMakeVisible(*speAnalyserRightControl);

        speAnalyserRangeLowControl = std::make_unique<LocalParameterControl>("LOW",
                                                                             2,
                                                                             -120.0,
                                                                             -24.0,
                                                                             0.1,
                                                                             -60.0);
        speAnalyserRangeLowControl->setTitleLongPressAction([this]
        {
            if (speAnalyserRangeLowControl != nullptr)
                speAnalyserRangeLowControl->setValue(-60.0, true);

            clearKeyboardFocus(*this);
        });
        speAnalyserRangeLowControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramRangeLowId,
                                                   static_cast<float>(speAnalyserRangeLowControl->getValue()));
            refreshAnalyserControls();
        };
        filterContent.addAndMakeVisible(*speAnalyserRangeLowControl);

        speAnalyserRangeHighControl = std::make_unique<LocalParameterControl>("HIGH",
                                                                              2,
                                                                              -48.0,
                                                                              20.0,
                                                                              0.1,
                                                                              10.0);
        speAnalyserRangeHighControl->setTitleLongPressAction([this]
        {
            if (speAnalyserRangeHighControl != nullptr)
                speAnalyserRangeHighControl->setValue(10.0, true);

            clearKeyboardFocus(*this);
        });
        speAnalyserRangeHighControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramRangeHighId,
                                                   static_cast<float>(speAnalyserRangeHighControl->getValue()));
            refreshAnalyserControls();
        };
        filterContent.addAndMakeVisible(*speAnalyserRangeHighControl);

        speAnalyserSlopeControl = std::make_unique<LocalParameterControl>("SLOPE",
                                                                          2,
                                                                          0.0,
                                                                          6.0,
                                                                          0.01,
                                                                          4.0);
        speAnalyserSlopeControl->setTitleLongPressAction([this]
        {
            if (speAnalyserSlopeControl != nullptr)
                speAnalyserSlopeControl->setValue(4.0, true);

            clearKeyboardFocus(*this);
        });
        speAnalyserSlopeControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramSlopeId,
                                                   static_cast<float>(speAnalyserSlopeControl->getValue()));
            refreshAnalyserControls();
        };
        filterContent.addAndMakeVisible(*speAnalyserSlopeControl);

        speAnalyserTimeControl = std::make_unique<LocalParameterControl>("TIME",
                                                                         0,
                                                                         0.0,
                                                                         1000.0,
                                                                         1.0,
                                                                         50.0);
        speAnalyserTimeControl->setTitleLongPressAction([this]
        {
            if (speAnalyserTimeControl != nullptr)
                speAnalyserTimeControl->setValue(50.0, true);

            clearKeyboardFocus(*this);
        });
        speAnalyserTimeControl->onValueChanged = [this, &speProcessor, refreshAnalyserControls]
        {
            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramTimeId,
                                                   static_cast<float>(speAnalyserTimeControl->getValue()));
            refreshAnalyserControls();
        };
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

        refreshAnalyserControls();

}
