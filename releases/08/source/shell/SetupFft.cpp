#include "ChoiceControl.h"
#include "LocalParameterControl.h"
#include "ParameterControl.h"
#include "SetupSupport.h"
#include "../modules/fft/Processor.h"

#include <cmath>

void AvaAudioProcessorEditor::rebindFftModeControls(FftModuleProcessor& fftProcessor)
{
    auto& state = fftProcessor.getValueTreeState();
    const auto phaseMode = fftProcessor.isPhaseCorrMode();
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

void AvaAudioProcessorEditor::refreshFftAnalyserControls(FftModuleProcessor& fftProcessor)
{
    rebindFftModeControls(fftProcessor);
    const juce::ScopedValueSetter<bool> scopedIgnore(suppressFftAnalyserControlChangeHandlers, true);

    if (fftAnalyserTimeControl != nullptr)
        fftAnalyserTimeControl->setValue(fftProcessor.getAnalyserParameterValue(FftModuleProcessor::paramTimeId), false);

    if (fftAnalyserRangeControl != nullptr)
    {
        const auto phaseMode = fftProcessor.isPhaseCorrMode();
        const auto parameterId = phaseMode ? FftModuleProcessor::paramPhaseReductionRangeId
                                           : FftModuleProcessor::paramSpectralReductionRangeId;
        fftAnalyserRangeControl->setValueRange(phaseMode ? 0.0 : -99.0,
                                               phaseMode ? 100.0 : 0.0,
                                               0.01,
                                               phaseMode);
        fftAnalyserRangeControl->setDefaultValue(phaseMode ? 50.0 : -36.0);
        fftAnalyserRangeControl->setValue(fftProcessor.getAnalyserParameterValue(parameterId), false);
    }
}

void AvaAudioProcessorEditor::setupFftControls(juce::AudioProcessorValueTreeState& fftState,
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
            header.setBorderVisible(true);
            header.setFillVisible(false);
            header.setDividerLineVisible(false);
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

        fftDspOverlapControl = std::make_unique<ChoiceControl>(fftState,
                                                               FftModuleProcessor::paramDspOverlapId,
                                                               "OVERLAP");
        filterContent.addAndMakeVisible(*fftDspOverlapControl);

        fftDynamicProcessorHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*fftDynamicProcessorHeader, "DYNAMIC PROCESSOR");
        filterContent.addAndMakeVisible(*fftDynamicProcessorHeader);

        fftDynamicModeControl = std::make_unique<ChoiceControl>(fftState,
                                                                 FftModuleProcessor::paramDynamicModeId,
                                                                 "MODE");
        fftDynamicModeControl->onValueChanged = [this]
        {
            updateSectionStates();
            resized();
            clearKeyboardFocus(*this);

            juce::Component::SafePointer<AvaAudioProcessorEditor> safeEditor(this);
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
        filterContent.addAndMakeVisible(*fftDynamicModeControl);

        fftAttackControl = std::make_unique<ParameterControl>(fftState,
                                                              FftModuleProcessor::paramAttackId,
                                                              "ATTACK",
                                                              2);
        filterContent.addAndMakeVisible(*fftAttackControl);

        fftReleaseControl = std::make_unique<ParameterControl>(fftState,
                                                               FftModuleProcessor::paramReleaseId,
                                                               "RELEASE",
                                                               2);
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
        addAndMakeVisible(*fftDeltaButton);

        fftDualMonoLeftThresholdControl = std::make_unique<ParameterControl>(fftState,
                                                                             FftModuleProcessor::paramDualMonoLeftThresholdId,
                                                                             "L.THRESH",
                                                                             2);
        filterContent.addAndMakeVisible(*fftDualMonoLeftThresholdControl);

        fftDualMonoLeftAdaptiveControl = std::make_unique<ParameterControl>(fftState,
                                                                            FftModuleProcessor::paramDualMonoLeftAdaptiveId,
                                                                            "L.ADAP",
                                                                            2);
        filterContent.addAndMakeVisible(*fftDualMonoLeftAdaptiveControl);

        fftDualMonoRightThresholdControl = std::make_unique<ParameterControl>(fftState,
                                                                              FftModuleProcessor::paramDualMonoRightThresholdId,
                                                                              "R.THRESH",
                                                                              2);
        filterContent.addAndMakeVisible(*fftDualMonoRightThresholdControl);

        fftDualMonoRightAdaptiveControl = std::make_unique<ParameterControl>(fftState,
                                                                             FftModuleProcessor::paramDualMonoRightAdaptiveId,
                                                                             "R.ADAP",
                                                                             2);
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

        fftAdaptiveSettingsHeader = std::make_unique<BoxTextButton>(uiAccent);
        configureSectionHeader(*fftAdaptiveSettingsHeader, "ADAP SETTINGS");
        filterContent.addAndMakeVisible(*fftAdaptiveSettingsHeader);

        fftAdaptiveOffsetControl = std::make_unique<ParameterControl>(fftState,
                                                                       FftModuleProcessor::paramSpectralAdaptiveOffsetId,
                                                                       "OFFSET",
                                                                       2);
        filterContent.addAndMakeVisible(*fftAdaptiveOffsetControl);

        fftAdaptiveAttackControl = std::make_unique<ParameterControl>(fftState,
                                                                       FftModuleProcessor::paramSpectralAdaptiveAttackId,
                                                                       "ATTACK",
                                                                       2);
        filterContent.addAndMakeVisible(*fftAdaptiveAttackControl);

        fftAdaptiveHoldControl = std::make_unique<ParameterControl>(fftState,
                                                                     FftModuleProcessor::paramSpectralAdaptiveHoldId,
                                                                     "HOLD",
                                                                     2);
        filterContent.addAndMakeVisible(*fftAdaptiveHoldControl);

        fftAdaptiveReleaseControl = std::make_unique<ParameterControl>(fftState,
                                                                        FftModuleProcessor::paramSpectralAdaptiveReleaseId,
                                                                        "RELEASE",
                                                                        2);
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
        addAndMakeVisible(*fftDynamicBypassButton);

        fftAnalyserRangeControl = std::make_unique<LocalParameterControl>("RANGE",
                                                                           2,
                                                                           -99.0,
                                                                           0.0,
                                                                           0.01,
                                                                           -36.0);
        fftAnalyserRangeControl->onValueChanged = [this, &fftProcessor, refreshFftAnalyserState]
        {
            if (suppressFftAnalyserControlChangeHandlers)
                return;

            fftProcessor.setAnalyserParameterValue(
                fftProcessor.isPhaseCorrMode() ? FftModuleProcessor::paramPhaseReductionRangeId
                                                : FftModuleProcessor::paramSpectralReductionRangeId,
                static_cast<float>(fftAnalyserRangeControl->getValue()));
            refreshFftAnalyserState();
        };
        filterContent.addAndMakeVisible(*fftAnalyserRangeControl);

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
        filterContent.addAndMakeVisible(*fftAnalyserTimeControl);

        refreshFftAnalyserControls(fftProcessor);

}
