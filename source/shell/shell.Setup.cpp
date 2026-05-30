#include "shell.EditorBellSection.h"
#include "shell.SetupSupport.h"
#include "../modules/mxe/module.mxe.ModuleComponent.h"
#include "module.tse.Component.h"
#include "../modules/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/tse/module.tse.TseProcessor.h"

#include <array>

void VxAudioProcessorEditor::rebindActiveModuleEditors()
{
    auto rebindSpeControls = [this] (juce::AudioProcessorValueTreeState& speState,
                                     SpeModuleProcessor& speProcessor)
    {
        if (speInputGainControl != nullptr) speInputGainControl->rebind(speState);
        if (speInputGainLControl != nullptr) speInputGainLControl->rebind(speState);
        if (speInputGainRControl != nullptr) speInputGainRControl->rebind(speState);
        if (speWideControl != nullptr) speWideControl->rebind(speState);
        if (speAttackControl != nullptr) speAttackControl->rebind(speState);
        if (speReleaseControl != nullptr) speReleaseControl->rebind(speState);
        if (speKneeControl != nullptr) speKneeControl->rebind(speState);
        if (speRatioControl != nullptr) speRatioControl->rebind(speState);
        if (speDspFftSizeControl != nullptr) speDspFftSizeControl->rebind(speState);
        if (speDspSlopeControl != nullptr) speDspSlopeControl->rebind(speState);
        if (speMakeupControl != nullptr) speMakeupControl->rebind(speState);
        if (speDualMonoLeftThresholdControl != nullptr) speDualMonoLeftThresholdControl->rebind(speState);
        if (speDualMonoLeftAdaptiveControl != nullptr) speDualMonoLeftAdaptiveControl->rebind(speState);
        if (speDualMonoLeftAdaptiveOffsetControl != nullptr) speDualMonoLeftAdaptiveOffsetControl->rebind(speState);
        if (speDualMonoRightThresholdControl != nullptr) speDualMonoRightThresholdControl->rebind(speState);
        if (speDualMonoRightAdaptiveControl != nullptr) speDualMonoRightAdaptiveControl->rebind(speState);
        if (speDualMonoRightAdaptiveOffsetControl != nullptr) speDualMonoRightAdaptiveOffsetControl->rebind(speState);
        refreshSpeAnalyserControls(speProcessor);
    };

    auto rebindSpeAttachments = [this] (juce::AudioProcessorValueTreeState& speState)
    {
        if (speDeltaButton != nullptr)
            speDeltaAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                    SpeModuleProcessor::paramDeltaId,
                                                                    *speDeltaButton);
        if (speBypassButton != nullptr)
            speBypassAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                     SpeModuleProcessor::paramMiscBypassId,
                                                                     *speBypassButton);
        if (speBypassWithGainButton != nullptr)
            speBypassWithGainAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                            SpeModuleProcessor::paramMiscBypassWithGainId,
                                                                            *speBypassWithGainButton);
        if (speDualMonoLinkButton != nullptr)
            speDualMonoLinkAttachment = std::make_unique<ButtonAttachment>(speState,
                                                                           SpeModuleProcessor::paramDualMonoLinkId,
                                                                           *speDualMonoLinkButton);
    };

    auto rebuildSpeAnalyser = [this] (SpeModuleProcessor& speProcessor)
    {
        shell_setup_support::removeOwnedChild(*this, speAnalyserComponent);
        speAnalyserComponent = shell_setup_support::createSpeAnalyserComponent(speProcessor);
        addAndMakeVisible(*speAnalyserComponent);
        speAnalyserComponent->setVisible(eqeModuleExpanded && speModuleLoaded);
    };

    auto rebindEqeEditorSections = [this]
    {
        if (auto* eqeProcessor = audioProcessor.getActiveEqeModuleProcessor())
        {
            auto& eqeState = eqeProcessor->getValueTreeState();

            if (eqeBypassButton != nullptr)
                eqeBypassAttachment = std::make_unique<ButtonAttachment>(eqeState,
                                                                         EqeModuleProcessor::paramBypassId,
                                                                         *eqeBypassButton);

            if (eqeBypassWithGainButton != nullptr)
                eqeBypassWithGainAttachment = std::make_unique<ButtonAttachment>(eqeState,
                                                                                 EqeModuleProcessor::paramBypassWithGainId,
                                                                                 *eqeBypassWithGainButton);

            if (eqeInGainLrControl != nullptr)
                eqeInGainLrControl->rebind(eqeState);

            if (eqeInGainLControl != nullptr)
                eqeInGainLControl->rebind(eqeState);

            if (eqeInGainRControl != nullptr)
                eqeInGainRControl->rebind(eqeState);

            if (eqeWideControl != nullptr)
                eqeWideControl->rebind(eqeState);

            if (eqeOutGainControl != nullptr)
                eqeOutGainControl->rebind(eqeState);

            for (auto& section : bellSections)
                if (section != nullptr)
                    section->rebind(eqeState);

            refreshFilterPresetList(eqeProcessor->getLastFilterPresetName());
        }
    };

    auto rebindSpeEditorSections = [this, &rebindSpeControls, &rebindSpeAttachments, &rebuildSpeAnalyser]
    {
        if (auto* speProcessor = audioProcessor.getSpeModuleProcessor())
        {
            auto& speState = speProcessor->getValueTreeState();
            rebindSpeControls(speState, *speProcessor);
            rebindSpeAttachments(speState);
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

        auto* currentMxeEditor = dynamic_cast<MxeModuleComponent*>(mxeModuleEditor.get());

        if (currentMxeEditor == nullptr || &currentMxeEditor->getAudioProcessor() != mxeProcessor)
        {
            shell_setup_support::removeOwnedChild(*this, mxeModuleEditor);
            mxeModuleEditor = std::make_unique<MxeModuleComponent>(*mxeProcessor,
                                                                   MxeModuleComponent::Callbacks {
                                                                       [this] (const juce::String& currentText,
                                                                               std::function<bool(const juce::String&)> onCommit,
                                                                               std::function<void()> onClose,
                                                                               std::function<void()> onDismiss)
                                                                       {
                                                                           showTextPrompt(currentText,
                                                                                          std::move(onCommit),
                                                                                          {},
                                                                                          std::move(onClose),
                                                                                          std::move(onDismiss));
                                                                       },
                                                                       [this]
                                                                       {
                                                                           requestCloseActiveModule();
                                                                       }
                                                                   });
            addAndMakeVisible(*mxeModuleEditor);
        }

        mxeModuleEditor->setVisible(eqeModuleExpanded && mxeModuleLoaded);
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

        auto* currentTseEditor = dynamic_cast<TseModuleComponent*>(tseModuleEditor.get());

        if (currentTseEditor == nullptr || &currentTseEditor->getProcessor() != tseProcessor)
        {
            shell_setup_support::removeOwnedChild(*this, tseModuleEditor);
            tseModuleEditor = std::make_unique<TseModuleComponent>(*tseProcessor,
                                                                   TseModuleComponent::Callbacks {
                                                                       [this] (const juce::Rectangle<int>& anchorBounds,
                                                                               const juce::StringArray& choices,
                                                                               const int selectedIndex,
                                                                               std::vector<bool> itemEnabledStates,
                                                                               const juce::Justification itemJustification,
                                                                               std::function<void(int)> onSelect,
                                                                               std::function<void()> onClose,
                                                                               std::function<void()> onDismiss)
                                                                       {
                                                                           showChoicePrompt(getLocalArea(tseModuleEditor.get(), anchorBounds),
                                                                                            choices,
                                                                                            selectedIndex,
                                                                                            std::move(itemEnabledStates),
                                                                                            itemJustification,
                                                                                            std::move(onSelect),
                                                                                            std::move(onClose),
                                                                                            std::move(onDismiss));
                                                                       },
                                                                       [this]
                                                                       {
                                                                           requestCloseActiveModule();
                                                                       },
                                                                       [this]
                                                                       {
                                                                           clearKeyboardFocus(*this);
                                                                       }
                                                                   });
            addAndMakeVisible(*tseModuleEditor);
        }

        tseModuleEditor->setVisible(eqeModuleExpanded && tseModuleLoaded);
    };

    refreshModuleStateListeners();

    rebindEqeEditorSections();
    rebindSpeEditorSections();
    rebindMxeEditor();
    rebindTseEditor();
}
