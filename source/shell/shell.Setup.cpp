#include "shell.EditorFilterSection.h"
#include "shell.MultibandComponent.h"
#include "shell.SetupSupport.h"
#include "../modules/multiband/tls/module.tls.ParameterIds.h"
#include "../modules/multiband/tls/module.tls.PluginProcessor.h"
#include "../modules/multiband/dyn/module.dyn.ParameterIds.h"
#include "../modules/multiband/dyn/module.dyn.PluginProcessor.h"
#include "../modules/fft/module.fft.FftProcessor.h"
#include "../modules/multiband/trs/module.trs.TrsProcessor.h"

namespace
{
using BandControlSpec = MultibandModuleComponent::BandControlSpec;
using ControlKind = MultibandModuleComponent::ControlKind;

BandControlSpec headingControl(const char* label, const int topGapMultiplier = 1)
{
    BandControlSpec spec;
    spec.kind = ControlKind::heading;
    spec.label = label;
    spec.topGapMultiplier = topGapMultiplier;
    return spec;
}

BandControlSpec parameterControl(const char* suffix,
                                 const char* label,
                                 const int decimals,
                                 const int sourceBandIndex = -1,
                                 const int topGapMultiplier = 1)
{
    BandControlSpec spec;
    spec.kind = ControlKind::parameter;
    spec.suffix = suffix;
    spec.label = label;
    spec.decimals = decimals;
    spec.sourceBandIndex = sourceBandIndex;
    spec.topGapMultiplier = topGapMultiplier;
    return spec;
}

BandControlSpec parameterToggleControl(const char* suffix,
                                       const char* label,
                                       const int decimals,
                                       const char* toggleSuffix,
                                       const char* toggleLabel,
                                       const char* reorderGroup = "",
                                       const char* orderSuffix = "",
                                       const bool fixedOrder = false)
{
    auto spec = parameterControl(suffix, label, decimals);
    spec.auxiliaryToggleSuffix = toggleSuffix;
    spec.auxiliaryToggleLabel = toggleLabel;
    spec.reorderGroup = reorderGroup;
    spec.orderSuffix = orderSuffix;
    spec.fixedOrder = fixedOrder;
    return spec;
}

BandControlSpec toggleControl(const char* suffix,
                              const char* label,
                              const char* enabledLabel = "",
                              const char* disabledLabel = "",
                              const char* exclusiveGroup = "",
                              const int topGapMultiplier = 1,
                              const int controlsInRow = 1)
{
    BandControlSpec spec;
    spec.kind = ControlKind::toggle;
    spec.suffix = suffix;
    spec.label = label;
    spec.enabledLabel = enabledLabel;
    spec.disabledLabel = disabledLabel;
    spec.exclusiveGroup = exclusiveGroup;
    spec.topGapMultiplier = topGapMultiplier;
    spec.controlsInRow = controlsInRow;
    return spec;
}

BandControlSpec inactiveControl(const char* label)
{
    BandControlSpec spec;
    spec.kind = ControlKind::inactive;
    spec.label = label;
    return spec;
}

BandControlSpec readoutControl(const char* suffix,
                               const char* label,
                               const char* modeSuffix,
                               const int topGapMultiplier = 1)
{
    BandControlSpec spec;
    spec.kind = ControlKind::readout;
    spec.suffix = suffix;
    spec.label = label;
    spec.modeSuffix = modeSuffix;
    spec.topGapMultiplier = topGapMultiplier;
    return spec;
}

BandControlSpec timeControl(const char* suffix,
                            const char* label,
                            const char* modeSuffix,
                            const char* syncSuffix)
{
    BandControlSpec spec;
    spec.kind = ControlKind::time;
    spec.suffix = suffix;
    spec.label = label;
    spec.decimals = 0;
    spec.modeSuffix = modeSuffix;
    spec.syncSuffix = syncSuffix;
    return spec;
}

MultibandModuleComponent::Config makeTlsMultibandConfig(TlsAudioProcessor& processor)
{
    MultibandModuleComponent::Config config;
    config.processorIdentity = &processor;
    config.moduleKey = "tls";
    config.valueTreeState = &processor.getValueTreeState();
    config.undoManager = &processor.getUndoManager();
    config.markParametersDirty = [&processor] { processor.markParametersDirty(); };
    config.makeBandParameterId = [] (const size_t bandIndex, const char* suffix)
    {
        return tls::parameters::makeBandParameterId(bandIndex, suffix);
    };
    config.makeFullbandParameterId = [] (const char* suffix)
    {
        return tls::parameters::makeFullbandParameterId(suffix);
    };
    config.makeSoloParameterId = [] (const size_t bandIndex)
    {
        return tls::parameters::makeSoloParameterId(bandIndex);
    };
    config.makeActiveSplitCountParameterId = []
    {
        return tls::parameters::makeActiveSplitCountParameterId();
    };
    config.crossoverDecimals = 2;
    config.bandControls = {
        headingControl("LISTEN", 2),
        toggleControl("listenLc", "LC", "", "", "listen", 1, 4),
        toggleControl("listenRc", "RC", "", "", "listen"),
        toggleControl("listenMc", "MC", "", "", "listen"),
        toggleControl("listenSc", "SC", "", "", "listen"),
        toggleControl("listenLl", "LL", "", "", "listen", 1, 4),
        toggleControl("listenRr", "RR", "", "", "listen"),
        inactiveControl("MM"),
        toggleControl("listenSs", "SS", "", "", "listen"),

        headingControl("GAIN", 2),
        parameterToggleControl("gainLr", "STEREO", 2, "gainLrMute", "MUTE", "gain", "", true),
        parameterToggleControl("gainL", "LEFT", 2, "gainLMute", "MUTE", "gain", "gainLOrder"),
        parameterToggleControl("gainR", "RIGHT", 2, "gainRMute", "MUTE", "gain", "gainROrder"),
        parameterToggleControl("gainMid", "MID", 2, "gainMidMute", "MUTE", "gain", "gainMidOrder"),
        parameterToggleControl("gainSide", "SIDE", 2, "gainSideMute", "MUTE", "gain", "gainSideOrder"),

        headingControl("DELAY", 2),
        parameterControl("depStereo", "STEREO", 2),
        parameterControl("depLeft", "LEFT", 2),
        parameterControl("depRight", "RIGHT", 2),

        headingControl("PHASE", 2),
        parameterControl("depPhaseL", "PHASE L", 2),
        parameterControl("depPhaseR", "PHASE R", 2),

        headingControl("PANORAMA", 2),
        parameterControl("left", "LEFT", 2),
        parameterControl("right", "RIGHT", 2),
        parameterControl("law", "LAW", 2),

        headingControl("SHEAR", 2),
        parameterControl("impact", "IMPACT", 2),
        toggleControl("impactDirection", "TO LEFT CHANNEL", "TO LEFT CHANNEL", "TO RIGHT CHANNEL"),

        headingControl("MS BALANCE", 2),
        parameterControl("mid", "MID", 2),
        parameterControl("side", "SIDE", 2),

        headingControl("ORTHOGONAL", 2),
        parameterControl("degree", "DEGREE", 2),
        toggleControl("flipRight", "FLIP RIGHT"),
        readoutControl("degree", "POSITION", "flipRight"),

        headingControl("RECTIFICATION", 2),
        toggleControl("halfPositive", "HPOS", "", "", "rectification", 1, 4),
        toggleControl("halfNegative", "HNEG", "", "", "rectification"),
        toggleControl("fullPositive", "FPOS", "", "", "rectification"),
        toggleControl("fullNegative", "FNEG", "", "", "rectification"),
    };
    return config;
}

MultibandModuleComponent::Config makeDynMultibandConfig(DynAudioProcessor& processor)
{
    MultibandModuleComponent::Config config;
    config.processorIdentity = &processor;
    config.moduleKey = "dyn";
    config.valueTreeState = &processor.getValueTreeState();
    config.undoManager = &processor.getUndoManager();
    config.markParametersDirty = [&processor] { processor.markParametersDirty(); };
    config.makeBandParameterId = [] (const size_t bandIndex, const char* suffix)
    {
        return dyn::parameters::makeBandParameterId(bandIndex, suffix);
    };
    config.makeFullbandParameterId = [] (const char* suffix)
    {
        return dyn::parameters::makeFullbandParameterId(suffix);
    };
    config.makeSoloParameterId = [] (const size_t bandIndex)
    {
        return dyn::parameters::makeSoloParameterId(bandIndex);
    };
    config.makeActiveSplitCountParameterId = []
    {
        return dyn::parameters::makeActiveSplitCountParameterId();
    };
    config.bandControls = {
        parameterControl("morph", "MORPH", 1, 0),
        parameterControl("peak_hold_frequency", "PEAK-HOLD", 1, 0),
        parameterControl("tension_floor", "TEN-FLOOR", 1, 0),
        parameterControl("tension_hysteresis", "TEN-HYST", 1, 0),
        toggleControl("linkUpDn", "LINK-UPDN (DUAL-MONO)"),
        toggleControl("linkLr", "LINK-LR (STEREO)"),
        toggleControl("linkOpp", "LINK-OPP"),
        parameterControl("thLU", "L.UP.THR", 1),
        parameterControl("tensLU", "L.UP.TENS", 1),
        parameterControl("relLU", "L.UP.REL", 1),
        parameterControl("outLU", "L.UP.OUT", 1),
        parameterControl("thLD", "L.DN.THR", 1),
        parameterControl("tensLD", "L.DN.TENS", 1),
        parameterControl("relLD", "L.DN.REL", 1),
        parameterControl("outLD", "L.DN.OUT", 1),
        parameterControl("thRU", "R.UP.THR", 1),
        parameterControl("tensRU", "R.UP.TENS", 1),
        parameterControl("relRU", "R.UP.REL", 1),
        parameterControl("outRU", "R.UP.OUT", 1),
        parameterControl("thRD", "R.DN.THR", 1),
        parameterControl("tensRD", "R.DN.TENS", 1),
        parameterControl("relRD", "R.DN.REL", 1),
        parameterControl("outRD", "R.DN.OUT", 1),
    };
    config.bandTailControls = {
        toggleControl("delta", "DELTA"),
    };
    return config;
}

MultibandModuleComponent::Config makeTrsMultibandConfig(TrsModuleProcessor& processor,
                                                        VxAudioProcessorEditor& editor,
                                                        std::unique_ptr<juce::Component>& editorHolder)
{
    MultibandModuleComponent::Config config;
    config.processorIdentity = &processor;
    config.moduleKey = "trs";
    config.valueTreeState = &processor.getValueTreeState();
    config.makeBandParameterId = [] (const size_t bandIndex, const char* suffix)
    {
        return TrsModuleProcessor::makeBandParameterId(bandIndex, suffix);
    };
    config.makeFullbandParameterId = [] (const char* suffix)
    {
        return TrsModuleProcessor::makeFullbandParameterId(suffix);
    };
    config.makeSoloParameterId = [] (const size_t bandIndex)
    {
        return TrsModuleProcessor::makeSoloParameterId(bandIndex);
    };
    config.makeActiveSplitCountParameterId = []
    {
        return TrsModuleProcessor::makeActiveSplitCountParameterId();
    };
    config.bandControls = {
        toggleControl(TrsModuleProcessor::paramTransOnId, "TRANS.ON", "TRANS.ON", "TRANS.OFF"),
        toggleControl(TrsModuleProcessor::paramSusOnId, "SUS.ON", "SUS.ON", "SUS.OFF"),
        parameterControl(TrsModuleProcessor::paramTransGainId, "TRANS.GAIN", 1),
        parameterControl(TrsModuleProcessor::paramSusGainId, "SUS.GAIN", 1),
        timeControl(TrsModuleProcessor::paramTimeHoldId,
                    "HOLD",
                    TrsModuleProcessor::paramTimeHoldModeId,
                    TrsModuleProcessor::paramTimeHoldSyncId),
        timeControl(TrsModuleProcessor::paramTimeReleaseId,
                    "RELEASE",
                    TrsModuleProcessor::paramTimeReleaseModeId,
                    TrsModuleProcessor::paramTimeReleaseSyncId),
        parameterControl(TrsModuleProcessor::paramTimeReleaseCurveId, "REL-CURVE", 0),
        parameterControl(TrsModuleProcessor::paramSensLevelId, "SENS.LVL", 1),
        parameterControl(TrsModuleProcessor::paramSensKneeId, "SENS.KNEE", 1),
        parameterControl(TrsModuleProcessor::paramSensRetriggerId, "SENS.RETR", 0),
        parameterControl(TrsModuleProcessor::paramLookaheadId, "LOOKAHEAD", 2),
    };
    config.getHostSyncChoices = []
    {
        return TrsModuleProcessor::getHostSyncChoices();
    };
    config.getDefaultHostSyncChoiceIndex = []
    {
        return TrsModuleProcessor::getDefaultHostSyncChoiceIndex();
    };
    config.showChoicePrompt = [&editor, &editorHolder] (const juce::Rectangle<int>& anchorBounds,
                                                        const juce::StringArray& choices,
                                                        const int selectedIndex,
                                                        std::vector<bool> itemEnabledStates,
                                                        const juce::Justification itemJustification,
                                                        std::function<void(int)> onSelect,
                                                        std::function<void()> onClose,
                                                        std::function<void()> onDismiss)
    {
        editor.showChoicePrompt(editor.getLocalArea(editorHolder.get(), anchorBounds),
                                choices,
                                selectedIndex,
                                std::move(itemEnabledStates),
                                itemJustification,
                                std::move(onSelect),
                                std::move(onClose),
                                std::move(onDismiss));
    };
    config.clearKeyboardFocus = [&editor]
    {
        clearKeyboardFocus(editor);
    };
    return config;
}
} // namespace

void VxAudioProcessorEditor::rebindActiveModuleEditors()
{
    auto rebindFftControls = [this] (juce::AudioProcessorValueTreeState& fftState,
                                     FftModuleProcessor& fftProcessor)
    {
        if (fftAttackControl != nullptr) fftAttackControl->rebind(fftState);
        if (fftReleaseControl != nullptr) fftReleaseControl->rebind(fftState);
        if (fftKneeControl != nullptr) fftKneeControl->rebind(fftState);
        if (fftRatioControl != nullptr) fftRatioControl->rebind(fftState);
        if (fftFloorControl != nullptr) fftFloorControl->rebind(fftState);
        if (fftDspFftSizeControl != nullptr) fftDspFftSizeControl->rebind(fftState);
        if (fftDspHopDivisorControl != nullptr) fftDspHopDivisorControl->rebind(fftState);
        if (fftDspSlopeControl != nullptr) fftDspSlopeControl->rebind(fftState);
        if (fftPhaseImpactControl != nullptr) fftPhaseImpactControl->rebind(fftState);
        if (fftDualMonoLeftThresholdControl != nullptr) fftDualMonoLeftThresholdControl->rebind(fftState, FftModuleProcessor::paramDualMonoLeftThresholdId);
        if (fftDualMonoLeftAdaptiveControl != nullptr) fftDualMonoLeftAdaptiveControl->rebind(fftState);
        if (fftDualMonoRightThresholdControl != nullptr) fftDualMonoRightThresholdControl->rebind(fftState, FftModuleProcessor::paramDualMonoRightThresholdId);
        if (fftDualMonoRightAdaptiveControl != nullptr) fftDualMonoRightAdaptiveControl->rebind(fftState);
        if (fftAdaptiveOffsetControl != nullptr) fftAdaptiveOffsetControl->rebind(fftState, FftModuleProcessor::paramSpectralAdaptiveOffsetId);
        if (fftAdaptiveAttackControl != nullptr) fftAdaptiveAttackControl->rebind(fftState, FftModuleProcessor::paramSpectralAdaptiveAttackId);
        if (fftAdaptiveHoldControl != nullptr) fftAdaptiveHoldControl->rebind(fftState, FftModuleProcessor::paramSpectralAdaptiveHoldId);
        if (fftAdaptiveReleaseControl != nullptr) fftAdaptiveReleaseControl->rebind(fftState, FftModuleProcessor::paramSpectralAdaptiveReleaseId);
        rebindFftModeControls(fftProcessor);
        refreshFftAnalyserControls(fftProcessor);
    };

    auto rebindFftAttachments = [this] (juce::AudioProcessorValueTreeState& fftState)
    {
        if (fftDeltaButton != nullptr)
            fftDeltaAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                    FftModuleProcessor::paramDeltaId,
                                                                    *fftDeltaButton);
        if (fftDualMonoLinkButton != nullptr)
            fftDualMonoLinkAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                           FftModuleProcessor::paramDualMonoLinkId,
                                                                           *fftDualMonoLinkButton);
        if (fftDynamicBypassButton != nullptr)
            fftDynamicBypassAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                            FftModuleProcessor::paramDynamicBypassId,
                                                                            *fftDynamicBypassButton);
        if (fftDynamicModeButton != nullptr)
            fftDynamicModeAttachment = std::make_unique<ButtonAttachment>(fftState,
                                                                          FftModuleProcessor::paramDynamicModeId,
                                                                          *fftDynamicModeButton);
    };

    auto rebuildFftAnalyser = [this] (FftModuleProcessor& fftProcessor)
    {
        shell_setup_support::removeOwnedChild(fftAnalyserContent, fftAnalyserComponent);
        fftAnalyserComponent = shell_setup_support::createFftAnalyserComponent(fftProcessor);
        fftAnalyserContent.addAndMakeVisible(*fftAnalyserComponent);
        fftAnalyserComponent->setVisible(fftModuleLoaded);
    };

    auto rebindEqlEditorSections = [this]
    {
        if (auto* eqlProcessor = getActiveEqlProcessor())
        {
            auto& eqlState = eqlProcessor->getValueTreeState();

            if (filterSections.front() == nullptr || addFilterButton == nullptr)
            {
                setupEqlControls(eqlState);
            }
            else
            {
                for (auto& section : filterSections)
                    if (section != nullptr)
                        section->rebind(eqlState);
            }

            refreshFilterPresetList(eqlProcessor->getLastFilterPresetName());
        }
    };

    auto rebindFftEditorSections = [this, &rebindFftControls, &rebindFftAttachments, &rebuildFftAnalyser]
    {
        if (auto* fftProcessor = audioProcessor.getFftModuleProcessor())
        {
            auto& fftState = fftProcessor->getValueTreeState();

            if (fftAttackControl == nullptr)
                setupFftControls(fftState, *fftProcessor);
            else
            {
                rebindFftControls(fftState, *fftProcessor);
                rebindFftAttachments(fftState);
            }

            rebuildFftAnalyser(*fftProcessor);
        }
    };

    auto rebindMultibandEditor = [this] (const bool moduleLoaded,
                                         auto* processor,
                                         std::unique_ptr<juce::Component>& editor,
                                         auto makeConfig)
    {
        if (! moduleLoaded)
        {
            shell_setup_support::removeOwnedChild(*this, editor);
            return;
        }

        if (processor == nullptr)
            return;

        auto* currentEditor = dynamic_cast<MultibandModuleComponent*>(editor.get());

        if (currentEditor == nullptr || currentEditor->getProcessorIdentity() != processor)
        {
            shell_setup_support::removeOwnedChild(*this, editor);
            auto config = makeConfig(*processor);
            config.assignHostSlot = [this] (const juce::String& parameterId,
                                            const juce::String& parameterName,
                                            const float normalizedValue)
            {
                return handleHostSlotAssignRequest(parameterId, parameterName, normalizedValue);
            };
            editor = std::make_unique<MultibandModuleComponent>(std::move(config));
            addAndMakeVisible(*editor);
        }
        else
        {
            currentEditor->refreshExternalState();
        }

        editor->setVisible(moduleLoaded);
    };

    refreshModuleStateListeners();

    rebindEqlEditorSections();
    rebindFftEditorSections();
    rebindMultibandEditor(tlsModuleLoaded,
                          audioProcessor.getTlsModuleProcessor(),
                          tlsModuleEditor,
                          [] (auto& processor) { return makeTlsMultibandConfig(processor); });
    rebindMultibandEditor(dynModuleLoaded,
                          audioProcessor.getDynModuleProcessor(),
                          dynModuleEditor,
                          [] (auto& processor) { return makeDynMultibandConfig(processor); });
    rebindMultibandEditor(trsModuleLoaded,
                          audioProcessor.getTrsModuleProcessor(),
                          trsModuleEditor,
                          [this] (auto& processor) { return makeTrsMultibandConfig(processor, *this, trsModuleEditor); });
}
