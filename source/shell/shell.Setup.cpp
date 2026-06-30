#include "shell.EditorFilterSection.h"
#include "shell.MultibandComponent.h"
#include "shell.SetupSupport.h"
#include "../modules/multiband/mie/module.mie.ParameterIds.h"
#include "../modules/multiband/mie/module.mie.PluginProcessor.h"
#include "../modules/multiband/mxe/module.mxe.ParameterIds.h"
#include "../modules/multiband/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/multiband/tse/module.tse.TseProcessor.h"

#include <array>

namespace
{
using BandControlSpec = MultibandModuleComponent::BandControlSpec;
using ControlKind = MultibandModuleComponent::ControlKind;

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
        parameterControl("wide", "WIDE", 1, -1, 2),
        parameterControl("gainL", "GAIN-L", 1),
        parameterControl("gainR", "GAIN-R", 1),
        parameterControl("gainLr", "GAIN-LR", 1),
        toggleControl("rectPlus", "RECT+", "", "", "rect", 2),
        toggleControl("rectMinus", "RECT-", "", "", "rect"),
        toggleControl("rectFoldPlus", "RECTF+", "", "", "rect"),
        toggleControl("rectFoldMinus", "RECTF-", "", "", "rect"),
        parameterControl("panL", "PAN-L", 1, -1, 2),
        parameterControl("panR", "PAN-R", 1),
        parameterControl("law", "LAW", 2),
        parameterControl("shear", "SHEAR", 1, -1, 2),
        toggleControl("shearMode", "TO-L", "TO-L", "TO-R"),
        parameterControl("midBal", "MID-BAL", 1, -1, 2),
        parameterControl("sideBal", "SIDE-BAL", 1),
        parameterControl("ortDegRotation", "ORT-DEG", 1, -1, 2),
        toggleControl("ortFlipR", "ORT-FLIP-R"),
        toggleControl("listenL", "L", "", "", "listen", 2),
        toggleControl("listenR", "R", "", "", "listen"),
        toggleControl("listenM", "M", "", "", "listen"),
        toggleControl("listenS", "S", "", "", "listen"),
        toggleControl("listenInPlace", "INPL"),
        parameterControl("depStereo", "D-STEREO", 2, -1, 2),
        parameterControl("depRight", "D-RIGHT", 2),
        parameterControl("depBuffer", "BUFFER", 2),
        parameterControl("depPhaseL", "PHASE-L", 1),
        parameterControl("depPhaseR", "PHASE-R", 1),
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
        parameterControl("moRph", "MORPH", 1, 0),
        parameterControl("peakHoldHz", "PEAK-HOLD", 1, 0),
        parameterControl("TensionFlooR", "TEN-FLOOR", 1, 0),
        parameterControl("TensionHysT", "TEN-HYST", 1, 0),
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
        toggleControl("delTa", "DELTA"),
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
        for (auto filterIndex = 0; filterIndex < spePhaseFilterControlCount; ++filterIndex)
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
        if (auto* eqeProcessor = audioProcessor.getActiveEqeModuleProcessor())
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

    auto rebindMxeEditor = [this]
    {
        if (! mxeModuleLoaded)
        {
            shell_setup_support::removeOwnedChild(*this, mxeModuleEditor);
            return;
        }

        auto* mxeProcessor = audioProcessor.getMxeModuleProcessor();

        if (mxeProcessor == nullptr)
            return;

        auto* currentMxeEditor = dynamic_cast<MultibandModuleComponent*>(mxeModuleEditor.get());

        if (currentMxeEditor == nullptr || currentMxeEditor->getProcessorIdentity() != mxeProcessor)
        {
            shell_setup_support::removeOwnedChild(*this, mxeModuleEditor);
            mxeModuleEditor = std::make_unique<MultibandModuleComponent>(makeMxeMultibandConfig(*mxeProcessor));
            addAndMakeVisible(*mxeModuleEditor);
        }

        mxeModuleEditor->setVisible(mxeModuleLoaded);
    };

    auto rebindMieEditor = [this]
    {
        if (! mieModuleLoaded)
        {
            shell_setup_support::removeOwnedChild(*this, mieModuleEditor);
            return;
        }

        auto* mieProcessor = audioProcessor.getMieModuleProcessor();

        if (mieProcessor == nullptr)
            return;

        auto* currentMieEditor = dynamic_cast<MultibandModuleComponent*>(mieModuleEditor.get());

        if (currentMieEditor == nullptr || currentMieEditor->getProcessorIdentity() != mieProcessor)
        {
            shell_setup_support::removeOwnedChild(*this, mieModuleEditor);
            mieModuleEditor = std::make_unique<MultibandModuleComponent>(makeMieMultibandConfig(*mieProcessor));
            addAndMakeVisible(*mieModuleEditor);
        }

        mieModuleEditor->setVisible(mieModuleLoaded);
    };

    auto rebindTseEditor = [this]
    {
        if (! tseModuleLoaded)
        {
            shell_setup_support::removeOwnedChild(*this, tseModuleEditor);
            return;
        }

        auto* tseProcessor = audioProcessor.getTseModuleProcessor();

        if (tseProcessor == nullptr)
            return;

        auto* currentTseEditor = dynamic_cast<MultibandModuleComponent*>(tseModuleEditor.get());

        if (currentTseEditor == nullptr || currentTseEditor->getProcessorIdentity() != tseProcessor)
        {
            shell_setup_support::removeOwnedChild(*this, tseModuleEditor);
            tseModuleEditor = std::make_unique<MultibandModuleComponent>(makeTseMultibandConfig(*tseProcessor,
                                                                                                *this,
                                                                                                tseModuleEditor));
            addAndMakeVisible(*tseModuleEditor);
        }

        tseModuleEditor->setVisible(tseModuleLoaded);
    };

    refreshModuleStateListeners();

    rebindEqeEditorSections();
    rebindSpeEditorSections();
    rebindMieEditor();
    rebindMxeEditor();
    rebindTseEditor();
}
