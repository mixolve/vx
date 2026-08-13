#include "shell.EditorParameterControls.h"
#include "shell.SetupSupport.h"
#include "../modules/fft/module.fft.FftProcessor.h"

#include <array>
#include <cmath>

void VxAudioProcessorEditor::rebindFftModeControls(FftModuleProcessor& fftProcessor)
{
    auto& state = fftProcessor.getValueTreeState();
    const auto phaseMode = fftProcessor.getDisplaySettings().phaseMode;
    const auto rebindIfNeeded = [&state] (ParameterControl* control, const char* parameterId)
    {
        if (control != nullptr && ! control->isBoundTo(parameterId))
            control->rebind(state, parameterId);
    };

    rebindIfNeeded(fftDualMonoLeftThresholdControl.get(),
                   phaseMode ? FftModuleProcessor::paramPhaseThresholdId
                             : FftModuleProcessor::paramDualMonoLeftThresholdId);
    rebindIfNeeded(fftDualMonoLeftAdaptiveControl.get(),
                   phaseMode ? FftModuleProcessor::paramPhaseAdaptiveId
                             : FftModuleProcessor::paramDualMonoLeftAdaptiveId);
    rebindIfNeeded(fftAdaptiveOffsetControl.get(),
                   phaseMode ? FftModuleProcessor::paramPhaseAdaptiveOffsetId
                             : FftModuleProcessor::paramSpectralAdaptiveOffsetId);
    rebindIfNeeded(fftAdaptiveAttackControl.get(),
                   phaseMode ? FftModuleProcessor::paramPhaseAdaptiveAttackId
                             : FftModuleProcessor::paramSpectralAdaptiveAttackId);
    rebindIfNeeded(fftAdaptiveHoldControl.get(),
                   phaseMode ? FftModuleProcessor::paramPhaseAdaptiveHoldId
                             : FftModuleProcessor::paramSpectralAdaptiveHoldId);
    rebindIfNeeded(fftAdaptiveReleaseControl.get(),
                   phaseMode ? FftModuleProcessor::paramPhaseAdaptiveReleaseId
                             : FftModuleProcessor::paramSpectralAdaptiveReleaseId);
    rebindIfNeeded(fftDspSlopeControl.get(),
                   phaseMode ? FftModuleProcessor::paramPhaseSlopeId
                             : FftModuleProcessor::paramDspSlopeId);
    rebindIfNeeded(fftDualMonoRightThresholdControl.get(),
                   FftModuleProcessor::paramDualMonoRightThresholdId);
    rebindIfNeeded(fftDualMonoRightAdaptiveControl.get(),
                   FftModuleProcessor::paramDualMonoRightAdaptiveId);
}

void VxAudioProcessorEditor::refreshFftAnalyserControls(FftModuleProcessor& fftProcessor)
{
    rebindFftModeControls(fftProcessor);
    const juce::ScopedValueSetter<bool> scopedIgnore(suppressFftAnalyserControlChangeHandlers, true);
    static constexpr std::array<const char*, 5> fftSizeLabels { "1024", "2048", "4096", "8192", "16384" };
    static constexpr std::array<const char*, 5> overlapLabels { "2", "4", "8", "16", "32" };
    const auto phaseMode = fftProcessor.getDisplaySettings().phaseMode;

    if (fftAnalyserFftSizeControl != nullptr)
    {
        const auto fftIndex = juce::jlimit(0,
                                           static_cast<int>(fftSizeLabels.size()) - 1,
                                           juce::roundToInt(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramFftSizeId)));
        fftAnalyserFftSizeControl->setSelectedChoiceIndex(fftIndex, false);
    }

    if (fftAnalyserOverlapControl != nullptr)
    {
        const auto overlapIndex = juce::jlimit(0,
                                               static_cast<int>(overlapLabels.size()) - 1,
                                               juce::roundToInt(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramOverlapId)));
        fftAnalyserOverlapControl->setSelectedChoiceIndex(overlapIndex, false);
    }

    if (fftAnalyserLeftControl != nullptr)
        fftAnalyserLeftControl->setValue(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramLeftId), false);

    if (fftAnalyserRightControl != nullptr)
        fftAnalyserRightControl->setValue(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramRightId), false);

    if (fftAnalyserRangeLowControl != nullptr)
    {
        fftAnalyserRangeLowControl->setValueRange(phaseMode ? -1.0 : -120.0,
                                                   phaseMode ? 1.0 : -24.0,
                                                   phaseMode ? 0.01 : 0.1);
        fftAnalyserRangeLowControl->setValue(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramRangeLowId), false);
    }

    if (fftAnalyserRangeHighControl != nullptr)
    {
        fftAnalyserRangeHighControl->setValueRange(phaseMode ? -1.0 : -48.0,
                                                    phaseMode ? 1.0 : 20.0,
                                                    phaseMode ? 0.01 : 0.1);
        fftAnalyserRangeHighControl->setValue(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramRangeHighId), false);
    }

    if (fftAnalyserSlopeControl != nullptr)
    {
        fftAnalyserSlopeControl->setVisible(! phaseMode);
        fftAnalyserSlopeControl->setTitleText("SLOPE");
        fftAnalyserSlopeControl->setInteractionEnabled(true);
        fftAnalyserSlopeControl->clearOverrideText();
        fftAnalyserSlopeControl->setValue(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramSlopeId), false);
    }

    if (fftAnalyserTimeControl != nullptr)
        fftAnalyserTimeControl->setValue(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramTimeId), false);
}

void VxAudioProcessorEditor::setupFftControls(juce::AudioProcessorValueTreeState& fftState,
                                              FftModuleProcessor& fftProcessor)
{
        const auto refreshFftAnalyserState = [this, &fftProcessor]
        {
            refreshFftAnalyserControls(fftProcessor);
            shell_setup_support::refreshFftAnalyserComponent(fftAnalyserComponent.get());
            scheduleHistorySnapshot();
        };

        const auto assignFftButtonHostSlot = [this] (BoxTextButton& button,
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

        fftGeneralProcessorHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*fftGeneralProcessorHeader, "GENERAL PROCESSOR");
        filterContent.addAndMakeVisible(*fftGeneralProcessorHeader);

        fftDspFftSizeControl = std::make_unique<ChoiceControl>(fftState,
                                                               FftModuleProcessor::paramDspFftSizeId,
                                                               "WIN-SIZE");
        filterContent.addAndMakeVisible(*fftDspFftSizeControl);

        fftDspHopDivisorControl = std::make_unique<ChoiceControl>(fftState,
                                                                  FftModuleProcessor::paramDspHopDivisorId,
                                                                  "HOP-DIV");
        filterContent.addAndMakeVisible(*fftDspHopDivisorControl);

        fftDynamicProcessorHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*fftDynamicProcessorHeader, "DYNAMIC PROCESSOR");
        filterContent.addAndMakeVisible(*fftDynamicProcessorHeader);

        fftDynamicModeButton = std::make_unique<BoxTextButton>(uiGrey500);
        fftDynamicModeButton->setButtonText("SPECTRAL");
        fftDynamicModeButton->setTextJustification(juce::Justification::centred);
        fftDynamicModeButton->setClickingTogglesState(true);
        fftDynamicModeAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                      FftModuleProcessor::paramDynamicModeId,
                                                                      *fftDynamicModeButton);
        assignFftButtonHostSlot(*fftDynamicModeButton,
                                FftModuleProcessor::paramDynamicModeId,
                                "MODE");
        fftDynamicModeButton->onClick = [this]
        {
            updateSectionStates();
            resized();
            clearKeyboardFocus(*this);

            juce::Component::SafePointer<VxAudioProcessorEditor> safeEditor(this);
            juce::MessageManager::callAsync([safeEditor]
            {
                if (safeEditor == nullptr)
                    return;

                if (auto* processor = safeEditor->audioProcessor.getFftModuleProcessor())
                {
                    safeEditor->rebindFftModeControls(*processor);
                    safeEditor->refreshFftAnalyserControls(*processor);
                    shell_setup_support::refreshFftAnalyserComponent(safeEditor->fftAnalyserComponent.get());
                }
            });
        };
        filterContent.addAndMakeVisible(*fftDynamicModeButton);

        fftAttackControl = std::make_unique<ParameterControl>(fftState,
                                                              FftModuleProcessor::paramAttackId,
                                                              "ATTACK",
                                                              0);
        filterContent.addAndMakeVisible(*fftAttackControl);

        fftReleaseControl = std::make_unique<ParameterControl>(fftState,
                                                               FftModuleProcessor::paramReleaseId,
                                                               "RELEASE",
                                                               0);
        filterContent.addAndMakeVisible(*fftReleaseControl);

        fftKneeControl = std::make_unique<ParameterControl>(fftState,
                                                            FftModuleProcessor::paramKneeId,
                                                            "KNEE",
                                                            2);
        filterContent.addAndMakeVisible(*fftKneeControl);

        fftRatioControl = std::make_unique<ParameterControl>(fftState,
                                                             FftModuleProcessor::paramRatioId,
                                                             "RATIO",
                                                             2);
        filterContent.addAndMakeVisible(*fftRatioControl);

        fftFloorControl = std::make_unique<ParameterControl>(fftState,
                                                              FftModuleProcessor::paramFloorId,
                                                              "FLOOR",
                                                              2);
        filterContent.addAndMakeVisible(*fftFloorControl);

        fftDspSlopeControl = std::make_unique<ParameterControl>(fftState,
                                                                FftModuleProcessor::paramDspSlopeId,
                                                                "SLOPE",
                                                                2);
        filterContent.addAndMakeVisible(*fftDspSlopeControl);

        fftPhaseImpactControl = std::make_unique<ParameterControl>(fftState,
                                                                    FftModuleProcessor::paramPhaseImpactId,
                                                                    "IMPACT",
                                                                    2);
        const auto formatImpact = [] (const double value)
        {
            if (value <= -99.995)
                return juce::String("LEFT");
            if (value >= 99.995)
                return juce::String("RIGHT");
            if (std::abs(value) <= 0.005)
                return juce::String("BOTH");

            return formatFixedDecimalValue(value, 2);
        };
        fftPhaseImpactControl->setValueTextTransform(
            formatImpact,
            formatImpact,
            [] (const juce::String& text)
            {
                const auto valueText = text.trim();

                if (valueText.equalsIgnoreCase("LEFT"))
                    return -100.0;
                if (valueText.equalsIgnoreCase("BOTH"))
                    return 0.0;
                if (valueText.equalsIgnoreCase("RIGHT"))
                    return 100.0;

                return parseNumericInput(valueText);
            });
        filterContent.addAndMakeVisible(*fftPhaseImpactControl);

        fftDeltaButton = std::make_unique<BoxTextButton>(uiAccent);
        fftDeltaButton->setButtonText("DELTA");
        fftDeltaButton->setTextJustification(juce::Justification::centred);
        fftDeltaButton->setClickingTogglesState(true);
        fftDeltaAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                FftModuleProcessor::paramDeltaId,
                                                                *fftDeltaButton);
        assignFftButtonHostSlot(*fftDeltaButton, FftModuleProcessor::paramDeltaId, "DELTA");
        fftDeltaButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*fftDeltaButton);

        fftDualMonoLeftThresholdControl = std::make_unique<ParameterControl>(fftState,
                                                                             FftModuleProcessor::paramDualMonoLeftThresholdId,
                                                                             "L.THRESH",
                                                                             2);
        filterContent.addAndMakeVisible(*fftDualMonoLeftThresholdControl);

        fftDualMonoLeftAdaptiveControl = std::make_unique<ParameterControl>(fftState,
                                                                            FftModuleProcessor::paramDualMonoLeftAdaptiveId,
                                                                            "L.ADAP",
                                                                            0);
        filterContent.addAndMakeVisible(*fftDualMonoLeftAdaptiveControl);

        fftDualMonoRightThresholdControl = std::make_unique<ParameterControl>(fftState,
                                                                              FftModuleProcessor::paramDualMonoRightThresholdId,
                                                                              "R.THRESH",
                                                                              2);
        filterContent.addAndMakeVisible(*fftDualMonoRightThresholdControl);

        fftDualMonoRightAdaptiveControl = std::make_unique<ParameterControl>(fftState,
                                                                             FftModuleProcessor::paramDualMonoRightAdaptiveId,
                                                                             "R.ADAP",
                                                                             0);
        filterContent.addAndMakeVisible(*fftDualMonoRightAdaptiveControl);

        fftDualMonoLinkButton = std::make_unique<BoxTextButton>(uiAccent);
        fftDualMonoLinkButton->setButtonText("LINK-LR (STEREO)");
        fftDualMonoLinkButton->setTextJustification(juce::Justification::centred);
        fftDualMonoLinkButton->setClickingTogglesState(true);
        fftDualMonoLinkAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                       FftModuleProcessor::paramDualMonoLinkId,
                                                                       *fftDualMonoLinkButton);
        assignFftButtonHostSlot(*fftDualMonoLinkButton, FftModuleProcessor::paramDualMonoLinkId, "LINK-LR");
        fftDualMonoLinkButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*fftDualMonoLinkButton);

        fftAdaptiveSettingsButton = std::make_unique<BoxTextButton>(uiAccent);
        fftAdaptiveSettingsButton->setButtonText("ADAP SETTINGS");
        fftAdaptiveSettingsButton->setTextJustification(juce::Justification::centred);
        fftAdaptiveSettingsButton->setClickingTogglesState(true);
        fftAdaptiveSettingsButton->onClick = [this]
        {
            fftAdaptiveSettingsExpanded = fftAdaptiveSettingsButton->getToggleState();
            updateSectionStates();
            resized();
            storeEditorStateToValueTree();
            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*fftAdaptiveSettingsButton);

        fftAdaptiveOffsetControl = std::make_unique<ParameterControl>(fftState,
                                                                       FftModuleProcessor::paramSpectralAdaptiveOffsetId,
                                                                       "OFFSET",
                                                                       2);
        filterContent.addAndMakeVisible(*fftAdaptiveOffsetControl);

        fftAdaptiveAttackControl = std::make_unique<ParameterControl>(fftState,
                                                                       FftModuleProcessor::paramSpectralAdaptiveAttackId,
                                                                       "ATTACK",
                                                                       0);
        filterContent.addAndMakeVisible(*fftAdaptiveAttackControl);

        fftAdaptiveHoldControl = std::make_unique<ParameterControl>(fftState,
                                                                     FftModuleProcessor::paramSpectralAdaptiveHoldId,
                                                                     "HOLD",
                                                                     0);
        filterContent.addAndMakeVisible(*fftAdaptiveHoldControl);

        fftAdaptiveReleaseControl = std::make_unique<ParameterControl>(fftState,
                                                                        FftModuleProcessor::paramSpectralAdaptiveReleaseId,
                                                                        "RELEASE",
                                                                        0);
        filterContent.addAndMakeVisible(*fftAdaptiveReleaseControl);

        fftDynamicBypassButton = std::make_unique<BoxTextButton>(uiAccent);
        fftDynamicBypassButton->setButtonText("BYPASS");
        fftDynamicBypassButton->setTextJustification(juce::Justification::centred);
        fftDynamicBypassButton->setClickingTogglesState(true);
        fftDynamicBypassAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                        FftModuleProcessor::paramDynamicBypassId,
                                                                        *fftDynamicBypassButton);
        assignFftButtonHostSlot(*fftDynamicBypassButton,
                                FftModuleProcessor::paramDynamicBypassId,
                                "BYPASS");
        fftDynamicBypassButton->onClick = [this]
        {
            clearKeyboardFocus(*this);
        };
        filterContent.addAndMakeVisible(*fftDynamicBypassButton);

        fftAnalyserSettingsHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*fftAnalyserSettingsHeader, "ANALYZER SETTINGS");
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserSettingsHeader);

        fftAnalyserFftSizeControl = std::make_unique<LocalChoiceControl>(
            "FFT-SIZE",
            juce::StringArray { "1024", "2048", "4096", "8192", "16384" },
            2);
        fftAnalyserFftSizeControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramFftSizeId,
                                                   static_cast<float>(fftAnalyserFftSizeControl->getSelectedChoiceIndex()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserFftSizeControl);

        fftAnalyserOverlapControl = std::make_unique<LocalChoiceControl>(
            "OVERLAP",
            juce::StringArray { "2", "4", "8", "16", "32" },
            4);
        fftAnalyserOverlapControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramOverlapId,
                                                   static_cast<float>(fftAnalyserOverlapControl->getSelectedChoiceIndex()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserOverlapControl);

        fftAnalyserLeftControl = std::make_unique<LocalParameterControl>("LEFT",
                                                                         0,
                                                                         0.0,
                                                                         1000.0,
                                                                         1.0,
                                                                         21.0,
                                                                         0.0,
                                                                         false,
                                                                         true);
        fftAnalyserLeftControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramLeftId,
                                                   static_cast<float>(fftAnalyserLeftControl->getValue()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserLeftControl);

        fftAnalyserRightControl = std::make_unique<LocalParameterControl>("RIGHT",
                                                                          0,
                                                                          1000.0,
                                                                          24000.0,
                                                                          1.0,
                                                                          20000.0,
                                                                          0.0,
                                                                          false,
                                                                          true);
        fftAnalyserRightControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramRightId,
                                                   static_cast<float>(fftAnalyserRightControl->getValue()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserRightControl);

        fftAnalyserRangeLowControl = std::make_unique<LocalParameterControl>("LOW",
                                                                             2,
                                                                             -120.0,
                                                                             -24.0,
                                                                             0.1,
                                                                             -60.0);
        fftAnalyserRangeLowControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramRangeLowId,
                                                   static_cast<float>(fftAnalyserRangeLowControl->getValue()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserRangeLowControl);

        fftAnalyserRangeHighControl = std::make_unique<LocalParameterControl>("HIGH",
                                                                              2,
                                                                              -48.0,
                                                                              20.0,
                                                                              0.1,
                                                                              10.0);
        fftAnalyserRangeHighControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramRangeHighId,
                                                   static_cast<float>(fftAnalyserRangeHighControl->getValue()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserRangeHighControl);

        fftAnalyserSlopeControl = std::make_unique<LocalParameterControl>("SLOPE",
                                                                          2,
                                                                          0.0,
                                                                          6.0,
                                                                          0.01,
                                                                          4.5);
        fftAnalyserSlopeControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramSlopeId,
                                                   static_cast<float>(fftAnalyserSlopeControl->getValue()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserSlopeControl);

        fftAnalyserTimeControl = std::make_unique<LocalParameterControl>("TIME",
                                                                         0,
                                                                         0.0,
                                                                         1000.0,
                                                                         1.0,
                                                                         50.0);
        fftAnalyserTimeControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(FftModuleProcessor::paramTimeId,
                                                   static_cast<float>(fftAnalyserTimeControl->getValue()));
            refreshFftAnalyserState();
        };
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserTimeControl);

        refreshFftAnalyserControls(fftProcessor);

}
