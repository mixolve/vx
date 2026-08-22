#include "EditorFilterSection.h"
#include "SetupSupport.h"
#include "../modules/tls/Processor.h"
#include "../modules/dyn/Processor.h"
#include "../modules/fft/Processor.h"
#include "../modules/trs/Processor.h"

juce::RangedAudioParameter* AvaAudioProcessorEditor::findHostAssignableParameter(const juce::String& parameterId) const noexcept
{
    const auto trimmedParameterId = parameterId.trim();

    if (trimmedParameterId.isEmpty())
        return nullptr;

    if (auto* parameter = valueTreeState.getParameter(trimmedParameterId))
        return parameter;

    const auto findInModule = [&trimmedParameterId] (auto* processor) -> juce::RangedAudioParameter*
    {
        if (processor == nullptr)
            return nullptr;

        return processor->getValueTreeState().getParameter(trimmedParameterId);
    };

    switch (audioProcessor.getActiveModule())
    {
        case AvaAudioProcessor::ActiveModule::eql: return findInModule(audioProcessor.getEqlModuleProcessor());
        case AvaAudioProcessor::ActiveModule::fft: return findInModule(audioProcessor.getFftModuleProcessor());
        case AvaAudioProcessor::ActiveModule::tls: return findInModule(audioProcessor.getTlsModuleProcessor());
        case AvaAudioProcessor::ActiveModule::dyn: return findInModule(audioProcessor.getDynModuleProcessor());
        case AvaAudioProcessor::ActiveModule::trs: return findInModule(audioProcessor.getTrsModuleProcessor());
        case AvaAudioProcessor::ActiveModule::none: break;
    }

    return nullptr;
}

void AvaAudioProcessorEditor::syncHostSlotAssignmentValue(const int slotIndex, const float normalizedValue)
{
    if (! juce::isPositiveAndBelow(slotIndex, static_cast<int>(hostSlotAssignments.size())))
        return;

    const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

    if (assignment.parameterId.isEmpty())
        return;

    if (auto* assignedParameter = findHostAssignableParameter(assignment.parameterId))
    {
        assignedParameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
    }
}

void AvaAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == FftModuleProcessor::paramDynamicModeId)
    {
        juce::Component::SafePointer<AvaAudioProcessorEditor> safeEditor(this);
        juce::MessageManager::callAsync([safeEditor]
        {
            if (safeEditor == nullptr)
                return;

            if (auto* processor = safeEditor->audioProcessor.getFftModuleProcessor())
                safeEditor->refreshFftAnalyserControls(*processor);

            safeEditor->updateSectionStates();
            safeEditor->resized();
            safeEditor->repaint();
        });
    }

    if (! suppressHostSlotAutomationSync)
    {
        juce::ScopedValueSetter<bool> scopedSyncGuard(suppressHostSlotAutomationSync, true);

        for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
        {
            const auto slotParameterId = AvaAudioProcessor::getHostSlotParameterId(slotIndex);

            if (parameterID == slotParameterId)
            {
                syncHostSlotAssignmentValue(slotIndex, newValue);
                scheduleHistorySnapshot();
                return;
            }
        }

        for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
        {
            const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

            if (assignment.parameterId != parameterID)
                continue;

            auto* assignedParameter = findHostAssignableParameter(parameterID);
            auto* slotParameter = valueTreeState.getParameter(AvaAudioProcessor::getHostSlotParameterId(slotIndex));

            if (assignedParameter == nullptr || slotParameter == nullptr)
                continue;

            const auto normalizedSourceValue = juce::jlimit(0.0f, 1.0f, assignedParameter->convertTo0to1(newValue));
            slotParameter->setValueNotifyingHost(normalizedSourceValue);
            break;
        }
    }

    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::resyncEditorFromProcessorState()
{
    restoreEditorStateFromValueTree();
    refreshModuleStateListeners();
    ensureModuleTitle();
    updateSectionStates();
    syncFocusedParameterControl();
    resized();
    repaint();
}

void AvaAudioProcessorEditor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                                       const juce::Identifier& property)
{
    if (treeWhosePropertyHasChanged == valueTreeState.state
        && property == juce::Identifier(AvaAudioProcessor::activeModuleStateKey)
        && ! suppressProcessorStateResync)
        resyncEditorFromProcessorState();

    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::valueTreeRedirected(juce::ValueTree& treeWhichHasBeenChanged)
{
    if (treeWhichHasBeenChanged == valueTreeState.state)
        resyncEditorFromProcessorState();

    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::registerParameterListeners()
{
    for (int slotIndex = 0; slotIndex < AvaAudioProcessor::hostAutomationSlotCount; ++slotIndex)
        valueTreeState.addParameterListener(AvaAudioProcessor::getHostSlotParameterId(slotIndex), this);

    if (! shellStateListenerRegistered)
    {
        valueTreeState.state.addListener(this);
        shellStateListenerRegistered = true;
    }

    refreshModuleStateListeners();
}

void AvaAudioProcessorEditor::registerObservedModuleParameterListeners(juce::AudioProcessorValueTreeState& moduleValueTreeState)
{
    auto observedListeners = ObservedModuleParameterListeners {};
    observedListeners.valueTreeState = &moduleValueTreeState;
    observedListeners.parameterIds.reserve(static_cast<size_t>(moduleValueTreeState.state.getNumChildren()));

    for (const auto parameterState : moduleValueTreeState.state)
    {
        const auto parameterId = parameterState.getProperty("id").toString();

        if (parameterId.isEmpty())
            continue;

        moduleValueTreeState.addParameterListener(parameterId, this);
        observedListeners.parameterIds.push_back(parameterId);
    }

    observedModuleParameterListeners.push_back(std::move(observedListeners));
}

void AvaAudioProcessorEditor::unregisterParameterListeners()
{
    for (int slotIndex = 0; slotIndex < AvaAudioProcessor::hostAutomationSlotCount; ++slotIndex)
        valueTreeState.removeParameterListener(AvaAudioProcessor::getHostSlotParameterId(slotIndex), this);

    if (shellStateListenerRegistered)
    {
        valueTreeState.state.removeListener(this);
        shellStateListenerRegistered = false;
    }

    clearModuleStateListeners();
}

void AvaAudioProcessorEditor::refreshModuleStateListeners()
{
    clearModuleStateListeners();

    observedModuleStates.reserve(1);
    observedModuleParameterListeners.reserve(1);

    juce::ValueTree moduleState;

    auto observeModule = [this, &moduleState] (auto* processor)
    {
        if (processor == nullptr)
            return;

        auto& moduleValueTreeState = processor->getValueTreeState();
        moduleState = moduleValueTreeState.state;
        registerObservedModuleParameterListeners(moduleValueTreeState);
    };

    switch (audioProcessor.getActiveModule())
    {
        case AvaAudioProcessor::ActiveModule::eql:
            observeModule(audioProcessor.getEqlModuleProcessor());
            break;

        case AvaAudioProcessor::ActiveModule::fft:
            observeModule(audioProcessor.getFftModuleProcessor());
            break;

        case AvaAudioProcessor::ActiveModule::tls:
            observeModule(audioProcessor.getTlsModuleProcessor());
            break;

        case AvaAudioProcessor::ActiveModule::dyn:
            observeModule(audioProcessor.getDynModuleProcessor());
            break;

        case AvaAudioProcessor::ActiveModule::trs:
            observeModule(audioProcessor.getTrsModuleProcessor());
            break;

        case AvaAudioProcessor::ActiveModule::none:
            break;
    }

    if (moduleState.isValid())
    {
        moduleState.addListener(this);
        observedModuleStates.push_back(moduleState);
    }
}

void AvaAudioProcessorEditor::clearModuleStateListeners()
{
    for (auto& observedListeners : observedModuleParameterListeners)
    {
        if (observedListeners.valueTreeState == nullptr)
            continue;

        for (const auto& parameterId : observedListeners.parameterIds)
            observedListeners.valueTreeState->removeParameterListener(parameterId, this);
    }

    observedModuleParameterListeners.clear();

    for (auto& state : observedModuleStates)
        if (state.isValid())
            state.removeListener(this);

    observedModuleStates.clear();
}

void AvaAudioProcessorEditor::detachModuleEditorBindings()
{
    clearModuleStateListeners();

    for (auto& section : filterSections)
        if (section != nullptr)
            section->detach();

    fftDeltaAttachment.reset();
    fftDualMonoLinkAttachment.reset();
    fftDynamicBypassAttachment.reset();

    if (fftAttackControl != nullptr) fftAttackControl->detach();
    if (fftReleaseControl != nullptr) fftReleaseControl->detach();
    if (fftKneeControl != nullptr) fftKneeControl->detach();
    if (fftRatioControl != nullptr) fftRatioControl->detach();
    if (fftFloorControl != nullptr) fftFloorControl->detach();
    if (fftDynamicModeControl != nullptr) fftDynamicModeControl->detach();
    if (fftDspFftSizeControl != nullptr) fftDspFftSizeControl->detach();
    if (fftDspOverlapControl != nullptr) fftDspOverlapControl->detach();
    if (fftDspSlopeControl != nullptr) fftDspSlopeControl->detach();
    if (fftPhaseImpactControl != nullptr) fftPhaseImpactControl->detach();
    if (fftDualMonoLeftThresholdControl != nullptr) fftDualMonoLeftThresholdControl->detach();
    if (fftDualMonoLeftAdaptiveControl != nullptr) fftDualMonoLeftAdaptiveControl->detach();
    if (fftDualMonoRightThresholdControl != nullptr) fftDualMonoRightThresholdControl->detach();
    if (fftDualMonoRightAdaptiveControl != nullptr) fftDualMonoRightAdaptiveControl->detach();
    if (fftAdaptiveOffsetControl != nullptr) fftAdaptiveOffsetControl->detach();
    if (fftAdaptiveAttackControl != nullptr) fftAdaptiveAttackControl->detach();
    if (fftAdaptiveHoldControl != nullptr) fftAdaptiveHoldControl->detach();
    if (fftAdaptiveReleaseControl != nullptr) fftAdaptiveReleaseControl->detach();
    shell_setup_support::removeOwnedChild(*this, fftAnalyserComponent);
    shell_setup_support::removeOwnedChild(*this, tlsModuleEditor);
    shell_setup_support::removeOwnedChild(*this, dynModuleEditor);
    shell_setup_support::removeOwnedChild(*this, trsModuleEditor);
}
