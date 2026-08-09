#include "shell.EditorFilterSection.h"
#include "shell.MultibandComponent.h"
#include "shell.SetupSupport.h"
#include "../modules/multiband/mie/module.mie.ParameterIds.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.ParameterIds.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"

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

BandControlSpec toggleControl(const char* suffix,
                              const char* label,
                              const char* enabledLabel = "",
                              const char* disabledLabel = "",
                              const char* exclusiveGroup = "",
                              const int topGapMultiplier = 1)
{
    BandControlSpec spec;
    spec.kind = ControlKind::toggle;
    spec.suffix = suffix;
    spec.label = label;
    spec.enabledLabel = enabledLabel;
    spec.disabledLabel = disabledLabel;
    spec.exclusiveGroup = exclusiveGroup;
    spec.topGapMultiplier = topGapMultiplier;
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

MultibandModuleComponent::Config makeMieMultibandConfig(MieAudioProcessor& processor)
{
    MultibandModuleComponent::Config config;
    config.processorIdentity = &processor;
    config.moduleKey = "mie";
    config.valueTreeState = &processor.getValueTreeState();
    config.undoManager = &processor.getUndoManager();
    config.markParametersDirty = [&processor] { processor.markParametersDirty(); };
    config.makeBandParameterId = [] (const size_t bandIndex, const char* suffix)
    {
        return mie::parameters::makeBandParameterId(bandIndex, suffix);
    };
    config.makeFullbandParameterId = [] (const char* suffix)
    {
        return mie::parameters::makeFullbandParameterId(suffix);
    };
    config.makeSoloParameterId = [] (const size_t bandIndex)
    {
        return mie::parameters::makeSoloParameterId(bandIndex);
    };
    config.makeActiveSplitCountParameterId = []
    {
        return mie::parameters::makeActiveSplitCountParameterId();
    };
    config.bandControls = {
        headingControl("GAIN", 2),
        parameterControl("gainMid", "MID", 1),
        parameterControl("gainSide", "SIDE", 1),
        parameterControl("gainL", "LEFT", 1),
        parameterControl("gainR", "RIGHT", 1),
        parameterControl("gainLr", "STEREO", 1),

        headingControl("RECTIFICATION", 2),
        toggleControl("halfPositive", "HALF POSITIVE", "", "", "rectification"),
        toggleControl("halfNegative", "HALF NEGATIVE", "", "", "rectification"),
        toggleControl("fullPositive", "FULL POSITIVE", "", "", "rectification"),
        toggleControl("fullNegative", "FULL NEGATIVE", "", "", "rectification"),

        headingControl("PANORAMA", 2),
        parameterControl("left", "LEFT", 1),
        parameterControl("right", "RIGHT", 1),
        parameterControl("law", "LAW", 2),

        headingControl("SHEAR", 2),
        parameterControl("impact", "IMPACT", 1),
        toggleControl("impactDirection", "TO LEFT CHANNEL", "TO LEFT CHANNEL", "TO RIGHT CHANNEL"),

        headingControl("MS BALANCE", 2),
        parameterControl("mid", "MID", 1),
        parameterControl("side", "SIDE", 1),

        headingControl("ORTHOGONAL", 2),
        parameterControl("degree", "DEGREE", 1),
        toggleControl("flipRight", "FLIP RIGHT"),
        readoutControl("degree", "POSITION", "flipRight"),

        headingControl("LISTEN", 2),
        toggleControl("listenL", "LEFT", "", "", "listen"),
        toggleControl("listenR", "RIGHT", "", "", "listen"),
        toggleControl("listenM", "MID", "", "", "listen"),
        toggleControl("listenS", "SIDE", "", "", "listen"),
        toggleControl("listenInPlace", "IN PLACE"),

        headingControl("DELAY & PHASE", 2),
        parameterControl("depStereo", "DELAY ST", 2),
        parameterControl("depRight", "DELAY R", 2),
        parameterControl("depBuffer", "BUFFER", 2),
        parameterControl("depPhaseL", "PHASE L", 1),
        parameterControl("depPhaseR", "PHASE R", 1),
    };
    return config;
}

MultibandModuleComponent::Config makeMxeMultibandConfig(MxeAudioProcessor& processor)
{
    MultibandModuleComponent::Config config;
    config.processorIdentity = &processor;
    config.moduleKey = "mxe";
    config.valueTreeState = &processor.getValueTreeState();
    config.undoManager = &processor.getUndoManager();
    config.markParametersDirty = [&processor] { processor.markParametersDirty(); };
    config.makeBandParameterId = [] (const size_t bandIndex, const char* suffix)
    {
        return mxe::parameters::makeBandParameterId(bandIndex, suffix);
    };
    config.makeFullbandParameterId = [] (const char* suffix)
    {
        return mxe::parameters::makeFullbandParameterId(suffix);
    };
    config.makeSoloParameterId = [] (const size_t bandIndex)
    {
        return mxe::parameters::makeSoloParameterId(bandIndex);
    };
    config.makeActiveSplitCountParameterId = []
    {
        return mxe::parameters::makeActiveSplitCountParameterId();
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

MultibandModuleComponent::Config makeTseMultibandConfig(TseModuleProcessor& processor,
                                                        VxAudioProcessorEditor& editor,
                                                        std::unique_ptr<juce::Component>& editorHolder)
{
    MultibandModuleComponent::Config config;
    config.processorIdentity = &processor;
    config.moduleKey = "tse";
    config.valueTreeState = &processor.getValueTreeState();
    config.makeBandParameterId = [] (const size_t bandIndex, const char* suffix)
    {
        return TseModuleProcessor::makeBandParameterId(bandIndex, suffix);
    };
    config.makeFullbandParameterId = [] (const char* suffix)
    {
        return TseModuleProcessor::makeFullbandParameterId(suffix);
    };
    config.makeSoloParameterId = [] (const size_t bandIndex)
    {
        return TseModuleProcessor::makeSoloParameterId(bandIndex);
    };
    config.makeActiveSplitCountParameterId = []
    {
        return TseModuleProcessor::makeActiveSplitCountParameterId();
    };
    config.bandControls = {
        toggleControl(TseModuleProcessor::paramTransOnId, "TRANS.ON", "TRANS.ON", "TRANS.OFF"),
        toggleControl(TseModuleProcessor::paramSusOnId, "SUS.ON", "SUS.ON", "SUS.OFF"),
        parameterControl(TseModuleProcessor::paramTransGainId, "TRANS.GAIN", 1),
        parameterControl(TseModuleProcessor::paramSusGainId, "SUS.GAIN", 1),
        timeControl(TseModuleProcessor::paramTimeHoldId,
                    "HOLD",
                    TseModuleProcessor::paramTimeHoldModeId,
                    TseModuleProcessor::paramTimeHoldSyncId),
        timeControl(TseModuleProcessor::paramTimeReleaseId,
                    "RELEASE",
                    TseModuleProcessor::paramTimeReleaseModeId,
                    TseModuleProcessor::paramTimeReleaseSyncId),
        parameterControl(TseModuleProcessor::paramTimeReleaseCurveId, "REL-CURVE", 0),
        parameterControl(TseModuleProcessor::paramSensLevelId, "SENS.LVL", 1),
        parameterControl(TseModuleProcessor::paramSensKneeId, "SENS.KNEE", 1),
        parameterControl(TseModuleProcessor::paramSensRetriggerId, "SENS.RETR", 0),
        parameterControl(TseModuleProcessor::paramLookaheadId, "LOOKAHEAD", 2),
    };
    config.getHostSyncChoices = []
    {
        return TseModuleProcessor::getHostSyncChoices();
    };
    config.getDefaultHostSyncChoiceIndex = []
    {
        return TseModuleProcessor::getDefaultHostSyncChoiceIndex();
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
    auto rebindSpeControls = [this] (juce::AudioProcessorValueTreeState& speState,
                                     SpeModuleProcessor& speProcessor)
    {
        if (speAttackControl != nullptr) speAttackControl->rebind(speState);
        if (speReleaseControl != nullptr) speReleaseControl->rebind(speState);
        if (speKneeControl != nullptr) speKneeControl->rebind(speState);
        if (speRatioControl != nullptr) speRatioControl->rebind(speState);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->rebind(speState);
        if (speDspHopDivisorControl != nullptr) speDspHopDivisorControl->rebind(speState);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->rebind(speState);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->rebind(speState);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->rebind(speState);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->rebind(speState);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->rebind(speState);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->rebind(speState);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->rebind(speState);
        for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
        {
            if (spePhaseTypeControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseTypeControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (spePhasePlaceControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhasePlaceControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (spePhaseSlopeControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseSlopeControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (spePhaseFrequencyControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (spePhaseBandwidthControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (spePhaseImpactControls[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseImpactControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (speAmplitudeTypeControls[static_cast<size_t>(filterIndex)] != nullptr)
                speAmplitudeTypeControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (speAmplitudePlaceControls[static_cast<size_t>(filterIndex)] != nullptr)
                speAmplitudePlaceControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)] != nullptr)
                speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)] != nullptr)
                speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)] != nullptr)
                speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)]->rebind(speState);
            if (speAmplitudeImpactControls[static_cast<size_t>(filterIndex)] != nullptr)
                speAmplitudeImpactControls[static_cast<size_t>(filterIndex)]->rebind(speState);
        }
        refreshSpeAnalyserControls(speProcessor);
    };

    auto rebindSpeAttachments = [this] (juce::AudioProcessorValueTreeState& speState)
    {
        if (speDeltaButton != nullptr)
            speDeltaAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                    SpeModuleProcessor::paramDeltaId,
                                                                    *speDeltaButton);
        if (speDualMonoLinkButton != nullptr)
            speDualMonoLinkAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                           SpeModuleProcessor::paramDualMonoLinkId,
                                                                           *speDualMonoLinkButton);
        for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
        {
            if (spePhaseBypassButtons[static_cast<size_t>(filterIndex)] != nullptr)
                spePhaseBypassAttachments[static_cast<size_t>(filterIndex)] = std::make_unique<ButtonAttachment>(
                    speState,
                    SpeModuleProcessor::getPhaseFilterBypassParamId(filterIndex),
                    *spePhaseBypassButtons[static_cast<size_t>(filterIndex)]);

            if (speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)] != nullptr)
                speAmplitudeBypassAttachments[static_cast<size_t>(filterIndex)] = std::make_unique<ButtonAttachment>(
                    speState,
                    SpeModuleProcessor::getAmplitudeFilterBypassParamId(filterIndex),
                    *speAmplitudeBypassButtons[static_cast<size_t>(filterIndex)]);
        }
    };

    auto rebuildSpeAnalyser = [this] (SpeModuleProcessor& speProcessor)
    {
        shell_setup_support::removeOwnedChild(speAnalyserContent, speAnalyserComponent);
        speAnalyserComponent = shell_setup_support::createSpeAnalyserComponent(speProcessor);
        speAnalyserContent.addAndMakeVisible(*speAnalyserComponent);
        speAnalyserComponent->setVisible(speModuleLoaded);
    };

    auto rebindEqeEditorSections = [this]
    {
        if (auto* eqeProcessor = getActiveEqeProcessor())
        {
            auto& eqeState = eqeProcessor->getValueTreeState();

            if (filterSections.front() == nullptr || addFilterButton == nullptr)
            {
                setupEqeControls(eqeState);
            }
            else
            {
                for (auto& section : filterSections)
                    if (section != nullptr)
                        section->rebind(eqeState);
            }

            refreshFilterPresetList(eqeProcessor->getLastFilterPresetName());
        }
    };

    auto rebindSpeEditorSections = [this, &rebindSpeControls, &rebindSpeAttachments, &rebuildSpeAnalyser]
    {
        if (auto* speProcessor = audioProcessor.getSpeModuleProcessor())
        {
            auto& speState = speProcessor->getValueTreeState();

            if (speAttackControl == nullptr)
                setupSpeControls(speState, *speProcessor);
            else
            {
                rebindSpeControls(speState, *speProcessor);
                rebindSpeAttachments(speState);
            }

            rebuildSpeAnalyser(*speProcessor);
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

    rebindEqeEditorSections();
    rebindSpeEditorSections();
    rebindMultibandEditor(mieModuleLoaded,
                          audioProcessor.getMieModuleProcessor(),
                          mieModuleEditor,
                          [] (auto& processor) { return makeMieMultibandConfig(processor); });
    rebindMultibandEditor(mxeModuleLoaded,
                          audioProcessor.getMxeModuleProcessor(),
                          mxeModuleEditor,
                          [] (auto& processor) { return makeMxeMultibandConfig(processor); });
    rebindMultibandEditor(tseModuleLoaded,
                          audioProcessor.getTseModuleProcessor(),
                          tseModuleEditor,
                          [this] (auto& processor) { return makeTseMultibandConfig(processor, *this, tseModuleEditor); });
}
