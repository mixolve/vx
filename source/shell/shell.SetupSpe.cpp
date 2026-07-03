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

        const auto assignSpeButtonHostSlot = [this] (BoxTextButton& button,
                                                     juce::String parameterId,
                                                     juce::String parameterName)
        {
            auto targetParameterId = std::move(parameterId);
            auto targetParameterName = std::move(parameterName);

            button.onClickWithModifiers = [this,
                                           assignedParameterId = std::move(targetParameterId),
                                           assignedParameterName = std::move(targetParameterName)] (const juce::ModifierKeys& modifiers)
            {
                if (! modifiers.isCtrlDown())
                    return false;

                if (auto* parameter = findHostAssignableParameter(assignedParameterId))
                    return handleHostSlotAssignRequest(assignedParameterId, assignedParameterName, parameter->getValue());

                return false;
            };
        };

        const auto configureSectionHeader = [] (BoxTextButton& header, const juce::String& text)
        {
            header.setButtonText(text);
            header.setClickingTogglesState(false);
            header.setBorderVisible(false);
            header.setFillVisible(false);
            header.setDividerLineVisible(true);
            header.setPressFillEnabled(false);
            header.setTextJustification(juce::Justification::centredLeft);
            header.setInterceptsMouseClicks(false, false);
        };

        const auto refreshSpeLayoutPreservingScroll = [this] (const int preservedScrollY)
        {
            updateSectionStates();
            resized();
            const auto maxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
            filterViewport.setViewPosition(0, juce::jlimit(0, maxOffset, preservedScrollY));
        };

        speFftProcessorHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*speFftProcessorHeader, "FFT PROCESSOR");
        filterContent.addAndMakeVisible(*speFftProcessorHeader);

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

        speDspHopDivisorControl = std::make_unique<ParameterControl>(speState,
                                                                     SpeModuleProcessor::paramDspHopDivisorId,
                                                                     "HOP-DIV",
                                                                     0);
        speDspHopDivisorControl->setValueClickAction([this, showDiscreteChoicePrompt]
        {
            auto* activeSpeProcessor = audioProcessor.getSpeModuleProcessor();

            if (activeSpeProcessor == nullptr)
                return;

            auto& activeSpeState = activeSpeProcessor->getValueTreeState();
            auto* hopDivisorParameter = activeSpeState.getRawParameterValue(SpeModuleProcessor::paramDspHopDivisorId);

            if (speDspHopDivisorControl == nullptr || hopDivisorParameter == nullptr)
                return;

            const juce::StringArray options { "/2", "/4", "/8", "/16", "/32" };
            const auto currentIndex = juce::roundToInt(hopDivisorParameter->load(std::memory_order_relaxed));

            showDiscreteChoicePrompt(*speDspHopDivisorControl,
                                     speDspHopDivisorControl->getValueBounds(),
                                     options,
                                     currentIndex,
                                     [this] (const int selectedIndex)
                                     {
                                         if (speDspHopDivisorControl != nullptr)
                                             speDspHopDivisorControl->setValue(static_cast<double>(selectedIndex), true);
                                     });
        });
        filterContent.addAndMakeVisible(*speDspHopDivisorControl);

        speDynamicProcessorHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*speDynamicProcessorHeader, "DYNAMIC PROCESSOR");
        filterContent.addAndMakeVisible(*speDynamicProcessorHeader);

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
        assignSpeButtonHostSlot(*speDeltaButton, SpeModuleProcessor::paramDeltaId, "DELTA");
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
        assignSpeButtonHostSlot(*speDualMonoLinkButton, SpeModuleProcessor::paramDualMonoLinkId, "LINK-LR");
        speDualMonoLinkButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*speDualMonoLinkButton);

        spePhaseProcessorHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*spePhaseProcessorHeader, "PHASE PROCESSOR");
        filterContent.addAndMakeVisible(*spePhaseProcessorHeader);

        spePhaseAddButton = std::make_unique<BoxTextButton>(uiGrey500);
        spePhaseAddButton->setButtonText("ADD");
        spePhaseAddButton->setTooltip("ADD PHASE FILTER");
        spePhaseAddButton->onClick = [this, refreshSpeLayoutPreservingScroll]
        {
            const auto preservedScrollY = filterViewport.getViewPositionY();
            auto* activeSpeProcessor = audioProcessor.getSpeModuleProcessor();

            if (activeSpeProcessor != nullptr && activeSpeProcessor->addPhaseFilter())
            {
                enforceSingleExpandedSpePhaseFilter(getActiveSpePhaseFilterCount() - 1);
                refreshSpeLayoutPreservingScroll(preservedScrollY);
                scheduleHistorySnapshot();
            }

            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*spePhaseAddButton);

        for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
        {
            spePhaseBypassButtons[static_cast<size_t>(filterIndex)] = std::make_unique<BoxTextButton>(uiAccent);
            spePhaseBypassButtons[static_cast<size_t>(filterIndex)]->setButtonText("B");
            spePhaseBypassButtons[static_cast<size_t>(filterIndex)]->setTooltip("BYPASS PHASE FILTER");
            spePhaseBypassButtons[static_cast<size_t>(filterIndex)]->setTextJustification(juce::Justification::centred);
            spePhaseBypassButtons[static_cast<size_t>(filterIndex)]->setClickingTogglesState(true);
            spePhaseBypassAttachments[static_cast<size_t>(filterIndex)] = std::make_unique<ButtonAttachment>(
                speState,
                SpeModuleProcessor::getPhaseFilterBypassParamId(filterIndex),
                *spePhaseBypassButtons[static_cast<size_t>(filterIndex)]);
            assignSpeButtonHostSlot(*spePhaseBypassButtons[static_cast<size_t>(filterIndex)],
                                    SpeModuleProcessor::getPhaseFilterBypassParamId(filterIndex),
                                    "B");
            spePhaseBypassButtons[static_cast<size_t>(filterIndex)]->onClick = [this]
            {
                clearKeyboardFocus(*this);
            };
            spePhaseBypassButtons[static_cast<size_t>(filterIndex)]->setLongPressAction([this, filterIndex, refreshSpeLayoutPreservingScroll]
            {
                const auto preservedScrollY = filterViewport.getViewPositionY();
                auto* activeSpeProcessor = audioProcessor.getSpeModuleProcessor();

                if (activeSpeProcessor != nullptr && activeSpeProcessor->removePhaseFilter(filterIndex))
                {
                    enforceSingleExpandedSpePhaseFilter(-1);
                    refreshSpeLayoutPreservingScroll(preservedScrollY);
                    scheduleHistorySnapshot();
                }

                clearKeyboardFocus(*this);
            }, 500, "D");
            filterContent.addAndMakeVisible(*spePhaseBypassButtons[static_cast<size_t>(filterIndex)]);

            spePhaseHeaderButtons[static_cast<size_t>(filterIndex)] = std::make_unique<BoxTextButton>(uiAccent);
            spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->setButtonText({});
            spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->setTextJustification(juce::Justification::centred);
            spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->setClickingTogglesState(true);
            spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->setCancelClickOnLeave(true);
            spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]->onClick = [this, filterIndex, refreshSpeLayoutPreservingScroll]
            {
                const auto preservedScrollY = filterViewport.getViewPositionY();
                enforceSingleExpandedSpePhaseFilter(spePhaseExpanded[static_cast<size_t>(filterIndex)] ? -1 : filterIndex);
                refreshSpeLayoutPreservingScroll(preservedScrollY);
                clearKeyboardFocus(*this);
            };
            filterContent.addAndMakeVisible(*spePhaseHeaderButtons[static_cast<size_t>(filterIndex)]);

            spePhaseTypeControls[static_cast<size_t>(filterIndex)] = std::make_unique<ChoiceControl>(
                speState,
                SpeModuleProcessor::getPhaseFilterTypeParamId(filterIndex),
                "TYPE",
                std::vector<int> { 0, 1, 2, 3, 4 });
            spePhaseTypeControls[static_cast<size_t>(filterIndex)]->onValueChanged = [this]
            {
                updateSectionStates();
            };
            filterContent.addAndMakeVisible(*spePhaseTypeControls[static_cast<size_t>(filterIndex)]);

            spePhasePlaceControls[static_cast<size_t>(filterIndex)] = std::make_unique<ChoiceControl>(
                speState,
                SpeModuleProcessor::getPhaseFilterPlaceParamId(filterIndex),
                "PLACE",
                std::vector<int> { 0, 1, 2 });
            spePhasePlaceControls[static_cast<size_t>(filterIndex)]->onValueChanged = [this]
            {
                updateSectionStates();
            };
            filterContent.addAndMakeVisible(*spePhasePlaceControls[static_cast<size_t>(filterIndex)]);

            spePhaseSlopeControls[static_cast<size_t>(filterIndex)] = std::make_unique<ChoiceControl>(
                speState,
                SpeModuleProcessor::getPhaseFilterSlopeParamId(filterIndex),
                "ORDER",
                std::vector<int> { 0, 1, 2, 3, 4, 5 });
            filterContent.addAndMakeVisible(*spePhaseSlopeControls[static_cast<size_t>(filterIndex)]);

            spePhaseFrequencyControls[static_cast<size_t>(filterIndex)] = std::make_unique<ParameterControl>(
                speState,
                SpeModuleProcessor::getPhaseFilterFrequencyParamId(filterIndex),
                "FREQ",
                2);
            spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->onValueChanged = [this]
            {
                updateSectionStates();
            };
            filterContent.addAndMakeVisible(*spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]);

            spePhaseBandwidthControls[static_cast<size_t>(filterIndex)] = std::make_unique<ParameterControl>(
                speState,
                SpeModuleProcessor::getPhaseFilterBandwidthParamId(filterIndex),
                "BW",
                2);
            filterContent.addAndMakeVisible(*spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]);

            spePhaseImpactControls[static_cast<size_t>(filterIndex)] = std::make_unique<ParameterControl>(
                speState,
                SpeModuleProcessor::getPhaseFilterImpactParamId(filterIndex),
                "IMPACT",
                0);
            filterContent.addAndMakeVisible(*spePhaseImpactControls[static_cast<size_t>(filterIndex)]);
        }

        speAmplitudeProcessorHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*speAmplitudeProcessorHeader, "AMPLITUDE PROCESSOR");
        filterContent.addAndMakeVisible(*speAmplitudeProcessorHeader);

        speAmplitudeAddButton = std::make_unique<BoxTextButton>(uiGrey500);
        speAmplitudeAddButton->setButtonText("ADD");
        speAmplitudeAddButton->setTooltip("ADD AMPLITUDE FILTER");
        speAmplitudeAddButton->onClick = [this, refreshSpeLayoutPreservingScroll]
        {
            const auto preservedScrollY = filterViewport.getViewPositionY();
            auto* activeSpeProcessor = audioProcessor.getSpeModuleProcessor();

            if (activeSpeProcessor != nullptr && activeSpeProcessor->addAmplitudeFilter())
            {
                enforceSingleExpandedSpeAmplitudeFilter(getActiveSpeAmplitudeFilterCount() - 1);
                refreshSpeLayoutPreservingScroll(preservedScrollY);
                scheduleHistorySnapshot();
            }

            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*speAmplitudeAddButton);

        for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
        {
            speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)] = std::make_unique<BoxTextButton>(uiAccent);
            speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]->setButtonText("B");
            speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]->setTooltip("BYPASS AMPLITUDE FILTER");
            speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]->setTextJustification(juce::Justification::centred);
            speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]->setClickingTogglesState(true);
            speAmplitudeBypassAttachments[static_cast<size_t>(filterIndex)] = std::make_unique<ButtonAttachment>(
                speState,
                SpeModuleProcessor::getAmplitudeFilterBypassParamId(filterIndex),
                *speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]);
            assignSpeButtonHostSlot(*speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)],
                                    SpeModuleProcessor::getAmplitudeFilterBypassParamId(filterIndex),
                                    "B");
            speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]->onClick = [this]
            {
                clearKeyboardFocus(*this);
            };
            speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]->setLongPressAction([this, filterIndex, refreshSpeLayoutPreservingScroll]
            {
                const auto preservedScrollY = filterViewport.getViewPositionY();
                auto* activeSpeProcessor = audioProcessor.getSpeModuleProcessor();

                if (activeSpeProcessor != nullptr && activeSpeProcessor->removeAmplitudeFilter(filterIndex))
                {
                    enforceSingleExpandedSpeAmplitudeFilter(-1);
                    refreshSpeLayoutPreservingScroll(preservedScrollY);
                    scheduleHistorySnapshot();
                }

                clearKeyboardFocus(*this);
            }, 500, "D");
            filterContent.addAndMakeVisible(*speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]);

            speAmplitudeHeaderButtons[static_cast<size_t>(filterIndex)] = std::make_unique<BoxTextButton>(uiAccent);
            speAmplitudeHeaderButtons[static_cast<size_t>(filterIndex)]->setButtonText({});
            speAmplitudeHeaderButtons[static_cast<size_t>(filterIndex)]->setTextJustification(juce::Justification::centred);
            speAmplitudeHeaderButtons[static_cast<size_t>(filterIndex)]->setClickingTogglesState(true);
            speAmplitudeHeaderButtons[static_cast<size_t>(filterIndex)]->setCancelClickOnLeave(true);
            speAmplitudeHeaderButtons[static_cast<size_t>(filterIndex)]->onClick = [this, filterIndex, refreshSpeLayoutPreservingScroll]
            {
                const auto preservedScrollY = filterViewport.getViewPositionY();
                enforceSingleExpandedSpeAmplitudeFilter(speAmplitudeExpanded[static_cast<size_t>(filterIndex)] ? -1 : filterIndex);
                refreshSpeLayoutPreservingScroll(preservedScrollY);
                clearKeyboardFocus(*this);
            };
            filterContent.addAndMakeVisible(*speAmplitudeHeaderButtons[static_cast<size_t>(filterIndex)]);

            speAmplitudeTypeControls[static_cast<size_t>(filterIndex)] = std::make_unique<ChoiceControl>(
                speState,
                SpeModuleProcessor::getAmplitudeFilterTypeParamId(filterIndex),
                "TYPE",
                std::vector<int> { 0, 1, 2, 3, 4 });
            speAmplitudeTypeControls[static_cast<size_t>(filterIndex)]->onValueChanged = [this]
            {
                updateSectionStates();
            };
            filterContent.addAndMakeVisible(*speAmplitudeTypeControls[static_cast<size_t>(filterIndex)]);

            speAmplitudePlaceControls[static_cast<size_t>(filterIndex)] = std::make_unique<ChoiceControl>(
                speState,
                SpeModuleProcessor::getAmplitudeFilterPlaceParamId(filterIndex),
                "PLACE",
                std::vector<int> { 0, 1, 2 });
            speAmplitudePlaceControls[static_cast<size_t>(filterIndex)]->onValueChanged = [this]
            {
                updateSectionStates();
            };
            filterContent.addAndMakeVisible(*speAmplitudePlaceControls[static_cast<size_t>(filterIndex)]);

            speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)] = std::make_unique<ChoiceControl>(
                speState,
                SpeModuleProcessor::getAmplitudeFilterSlopeParamId(filterIndex),
                "ORDER",
                std::vector<int> { 0, 1, 2, 3, 4, 5 });
            filterContent.addAndMakeVisible(*speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)]);

            speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)] = std::make_unique<ParameterControl>(
                speState,
                SpeModuleProcessor::getAmplitudeFilterFrequencyParamId(filterIndex),
                "FREQ",
                2);
            speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)]->onValueChanged = [this]
            {
                updateSectionStates();
            };
            filterContent.addAndMakeVisible(*speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)]);

            speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)] = std::make_unique<ParameterControl>(
                speState,
                SpeModuleProcessor::getAmplitudeFilterBandwidthParamId(filterIndex),
                "BW",
                2);
            filterContent.addAndMakeVisible(*speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)]);

            speAmplitudeImpactControls[static_cast<size_t>(filterIndex)] = std::make_unique<ParameterControl>(
                speState,
                SpeModuleProcessor::getAmplitudeFilterImpactParamId(filterIndex),
                "IMPACT",
                0);
            filterContent.addAndMakeVisible(*speAmplitudeImpactControls[static_cast<size_t>(filterIndex)]);
        }

        speAnalyserSettingsHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*speAnalyserSettingsHeader, "ANALYZER SETTINGS");
        speAnalyserContent.addAndMakeVisible(*speAnalyserSettingsHeader);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserFftSizeControl);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserOverlapControl);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserLeftControl);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserRightControl);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserRangeLowControl);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserRangeHighControl);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserSlopeControl);

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
        speAnalyserContent.addAndMakeVisible(*speAnalyserTimeControl);

        refreshSpeAnalyserControls(speProcessor);

}
