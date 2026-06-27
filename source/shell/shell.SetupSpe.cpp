#include "shell.EditorParameterControls.h"
#include "shell.SetupSupport.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

#include <array>
#include <cmath>
#include <limits>

void VxAudioProcessorEditor::refreshSpeAnalyserControls(SpeModuleProcessor& speProcessor)
{
        const juce::ScopedValueSetter<bool> scopedIgnore(suppressSpeAnalyserControlChangeHandlers, true);
        static constexpr std::array<const char*, 5> fftSizeLabels { "1024", "2048", "4096", "8192", "16384" };
    static constexpr std::array<const char*, 5> overlapLabels { "2", "4", "8", "16", "32" };

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
}

void VxAudioProcessorEditor::setupSpeControls(juce::AudioProcessorValueTreeState& speState,
                                              SpeModuleProcessor& speProcessor)
{
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

        const auto refreshSpeAnalyserState = [this, &speProcessor]
        {
            refreshSpeAnalyserControls(speProcessor);
            shell_setup_support::refreshSpeAnalyserComponent(speAnalyserComponent.get());
            scheduleHistorySnapshot();
        };

        const auto showDiscreteChoicePrompt = [this] (juce::Component& anchor,
                                                       juce::Rectangle<int> anchorBounds,
                                                       const juce::StringArray& options,
                                                       const int currentIndex,
                                                       std::function<void(int)> onSelectCallback)
        {
            if (anchorBounds.isEmpty())
                anchorBounds = anchor.getLocalBounds();

            std::vector<bool> enabledStates(static_cast<size_t>(options.size()), true);
            const auto clampedCurrentIndex = juce::jlimit(0, juce::jmax(0, options.size() - 1), currentIndex);

            showChoicePrompt(getLocalArea(&anchor, anchorBounds),
                             options,
                             clampedCurrentIndex,
                             std::move(enabledStates),
                             juce::Justification::centred,
                             [this, onSelectFn = std::move(onSelectCallback)] (const int selectedIndex)
                             {
                                 if (selectedIndex >= 0 && onSelectFn != nullptr)
                                     onSelectFn(selectedIndex);

                                 clearKeyboardFocus(*this);
                             });
        };

        speAttackControl = std::make_unique<ParameterControl>(speState,
                                                              SpeModuleProcessor::paramAttackId,
                                                              "ATTACK",
                                                              0);
        filterContent.addAndMakeVisible(*speAttackControl);

        speReleaseControl = std::make_unique<ParameterControl>(speState,
                                                               SpeModuleProcessor::paramReleaseId,
                                                               "RELEASE",
                                                               0);
        filterContent.addAndMakeVisible(*speReleaseControl);

        speKneeControl = std::make_unique<ParameterControl>(speState,
                                                            SpeModuleProcessor::paramKneeId,
                                                            "KNEE",
                                                            2);
        filterContent.addAndMakeVisible(*speKneeControl);

        speRatioControl = std::make_unique<ParameterControl>(speState,
                                                             SpeModuleProcessor::paramRatioId,
                                                             "RATIO",
                                                             2);
        filterContent.addAndMakeVisible(*speRatioControl);

        speDspFftSizeControl = std::make_unique<ParameterControl>(speState,
                                                                  SpeModuleProcessor::paramDspFftSizeId,
                                                                  "WIN-SIZE",
                                                                  0);
        speDspFftSizeControl->setValueClickAction([this, showDiscreteChoicePrompt]
        {
            auto* activeSpeProcessor = audioProcessor.getSpeModuleProcessor();

            if (activeSpeProcessor == nullptr)
                return;

            auto& activeSpeState = activeSpeProcessor->getValueTreeState();
            auto* fftSizeParameter = activeSpeState.getRawParameterValue(SpeModuleProcessor::paramDspFftSizeId);

            if (speDspFftSizeControl == nullptr || fftSizeParameter == nullptr)
                return;

            const juce::StringArray options { "1024", "2048", "4096", "8192", "16384" };
            const auto currentIndex = juce::roundToInt(fftSizeParameter->load(std::memory_order_relaxed));

            showDiscreteChoicePrompt(*speDspFftSizeControl,
                                     speDspFftSizeControl->getValueBounds(),
                                     options,
                                     currentIndex,
                                     [this] (const int selectedIndex)
                                     {
                                         if (speDspFftSizeControl != nullptr)
                                             speDspFftSizeControl->setValue(static_cast<double>(selectedIndex), true);
                                     });
        });
        filterContent.addAndMakeVisible(*speDspFftSizeControl);

        speDspSlopeControl = std::make_unique<ParameterControl>(speState,
                                                                SpeModuleProcessor::paramDspSlopeId,
                                                                "SLOPE",
                                                                2);
        filterContent.addAndMakeVisible(*speDspSlopeControl);

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
        filterContent.addAndMakeVisible(*speDeltaButton);

        speDualMonoLeftThresholdControl = std::make_unique<ParameterControl>(speState,
                                                                             SpeModuleProcessor::paramDualMonoLeftThresholdId,
                                                                             "L.THRESH",
                                                                             2);
        filterContent.addAndMakeVisible(*speDualMonoLeftThresholdControl);

        speDualMonoLeftAdaptiveControl = std::make_unique<ParameterControl>(speState,
                                                                            SpeModuleProcessor::paramDualMonoLeftAdaptiveId,
                                                                            "L.ADAP",
                                                                            0);
        filterContent.addAndMakeVisible(*speDualMonoLeftAdaptiveControl);

        speDualMonoLeftAdaptiveOffsetControl = std::make_unique<ParameterControl>(speState,
                                                                                  SpeModuleProcessor::paramDualMonoLeftAdaptiveOffsetId,
                                                                                  "L.OFFSET",
                                                                                  2);
        filterContent.addAndMakeVisible(*speDualMonoLeftAdaptiveOffsetControl);

        speDualMonoRightThresholdControl = std::make_unique<ParameterControl>(speState,
                                                                              SpeModuleProcessor::paramDualMonoRightThresholdId,
                                                                              "R.THRESH",
                                                                              2);
        filterContent.addAndMakeVisible(*speDualMonoRightThresholdControl);

        speDualMonoRightAdaptiveControl = std::make_unique<ParameterControl>(speState,
                                                                             SpeModuleProcessor::paramDualMonoRightAdaptiveId,
                                                                             "R.ADAP",
                                                                             0);
        filterContent.addAndMakeVisible(*speDualMonoRightAdaptiveControl);

        speDualMonoRightAdaptiveOffsetControl = std::make_unique<ParameterControl>(speState,
                                                                                   SpeModuleProcessor::paramDualMonoRightAdaptiveOffsetId,
                                                                                   "R.OFFSET",
                                                                                   2);
        filterContent.addAndMakeVisible(*speDualMonoRightAdaptiveOffsetControl);

        speDualMonoLinkButton = std::make_unique<BoxTextButton>(uiAccent);
        speDualMonoLinkButton->setButtonText("LINK-LR (STEREO)");
        speDualMonoLinkButton->setTextJustification(juce::Justification::centred);
        speDualMonoLinkButton->setClickingTogglesState(true);
        speDualMonoLinkAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                       SpeModuleProcessor::paramDualMonoLinkId,
                                                                       *speDualMonoLinkButton);
        speDualMonoLinkButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*speDualMonoLinkButton);

        speAnalyserFftSizeControl = std::make_unique<LocalParameterControl>("FFT-SIZE",
                                                                            0,
                                                                            0.0,
                                                                            4.0,
                                                                            1.0,
                                                                            2.0);
        speAnalyserFftSizeControl->setTextToValueParser([parseDiscreteAnalyserChoice] (const juce::String& text)
        {
            return parseDiscreteAnalyserChoice(text, { 1024.0, 2048.0, 4096.0, 8192.0, 16384.0 });
        });
        speAnalyserFftSizeControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramFftSizeId,
                                                   static_cast<float>(speAnalyserFftSizeControl->getValue()));
            refreshSpeAnalyserState();
        };
        speAnalyserFftSizeControl->setValueClickAction([this, showDiscreteChoicePrompt]
        {
            if (speAnalyserFftSizeControl == nullptr)
                return;

            const juce::StringArray options { "1024", "2048", "4096", "8192", "16384" };
            const auto currentIndex = juce::roundToInt(speAnalyserFftSizeControl->getValue());

            showDiscreteChoicePrompt(*speAnalyserFftSizeControl,
                                     speAnalyserFftSizeControl->getValueBounds(),
                                     options,
                                     currentIndex,
                                     [this] (const int selectedIndex)
                                     {
                                         if (speAnalyserFftSizeControl != nullptr)
                                             speAnalyserFftSizeControl->setValue(static_cast<double>(selectedIndex), true);
                                     });
        });
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
        speAnalyserOverlapControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramOverlapId,
                                                   static_cast<float>(speAnalyserOverlapControl->getValue()));
            refreshSpeAnalyserState();
        };
        speAnalyserOverlapControl->setValueClickAction([this, showDiscreteChoicePrompt]
        {
            if (speAnalyserOverlapControl == nullptr)
                return;

            const juce::StringArray options { "2", "4", "8", "16", "32" };
            const auto currentIndex = juce::roundToInt(speAnalyserOverlapControl->getValue());

            showDiscreteChoicePrompt(*speAnalyserOverlapControl,
                                     speAnalyserOverlapControl->getValueBounds(),
                                     options,
                                     currentIndex,
                                     [this] (const int selectedIndex)
                                     {
                                         if (speAnalyserOverlapControl != nullptr)
                                             speAnalyserOverlapControl->setValue(static_cast<double>(selectedIndex), true);
                                     });
        });
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
        speAnalyserLeftControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramLeftId,
                                                   static_cast<float>(speAnalyserLeftControl->getValue()));
            refreshSpeAnalyserState();
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
        speAnalyserRightControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramRightId,
                                                   static_cast<float>(speAnalyserRightControl->getValue()));
            refreshSpeAnalyserState();
        };
        filterContent.addAndMakeVisible(*speAnalyserRightControl);

        speAnalyserRangeLowControl = std::make_unique<LocalParameterControl>("LOW",
                                                                             2,
                                                                             -120.0,
                                                                             -24.0,
                                                                             0.1,
                                                                             -60.0);
        speAnalyserRangeLowControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramRangeLowId,
                                                   static_cast<float>(speAnalyserRangeLowControl->getValue()));
            refreshSpeAnalyserState();
        };
        filterContent.addAndMakeVisible(*speAnalyserRangeLowControl);

        speAnalyserRangeHighControl = std::make_unique<LocalParameterControl>("HIGH",
                                                                              2,
                                                                              -48.0,
                                                                              20.0,
                                                                              0.1,
                                                                              10.0);
        speAnalyserRangeHighControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramRangeHighId,
                                                   static_cast<float>(speAnalyserRangeHighControl->getValue()));
            refreshSpeAnalyserState();
        };
        filterContent.addAndMakeVisible(*speAnalyserRangeHighControl);

        speAnalyserSlopeControl = std::make_unique<LocalParameterControl>("SLOPE",
                                                                          2,
                                                                          0.0,
                                                                          6.0,
                                                                          0.01,
                                                                          4.5);
        speAnalyserSlopeControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramSlopeId,
                                                   static_cast<float>(speAnalyserSlopeControl->getValue()));
            refreshSpeAnalyserState();
        };
        filterContent.addAndMakeVisible(*speAnalyserSlopeControl);

        speAnalyserTimeControl = std::make_unique<LocalParameterControl>("TIME",
                                                                         0,
                                                                         0.0,
                                                                         1000.0,
                                                                         1.0,
                                                                         50.0);
        speAnalyserTimeControl->onValueChanged = [this, &speProcessor, refreshSpeAnalyserState]
        {
            if (suppressSpeAnalyserControlChangeHandlers)
                return;

            speProcessor.setAnalyserParameterValue(SpeModuleProcessor::paramTimeId,
                                                   static_cast<float>(speAnalyserTimeControl->getValue()));
            refreshSpeAnalyserState();
        };
        filterContent.addAndMakeVisible(*speAnalyserTimeControl);

        refreshSpeAnalyserControls(speProcessor);

}
