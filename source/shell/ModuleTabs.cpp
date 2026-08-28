#include "EditorControls.h"
#include "EditorFilterSection.h"
#include "EditorPresetSections.h"
#include "../modules/dyn/Processor.h"
#include "../modules/eql/Processor.h"
#include "../modules/fft/Processor.h"
#include "../modules/tls/Processor.h"
#include "../modules/trs/Processor.h"

#include <algorithm>
#include <utility>
#include <vector>

void AvaAudioProcessorEditor::showModulePicker()
{
    if (moduleAddButton == nullptr)
        return;

    const auto canLoadModule = audioProcessor.getActiveModule() == AvaAudioProcessor::ActiveModule::none;

    if (! canLoadModule)
        return;

    auto anchorBounds = moduleAddButton->getBounds();
    anchorBounds.setSize(juce::jmax(120, anchorBounds.getWidth()), anchorBounds.getHeight());
    anchorBounds.setCentre(moduleAddButton->getBounds().getCentre());

    showChoicePrompt(anchorBounds,
                     { "TLS", "EQL", "FFT", "DYN", "TRS" },
                     -1,
                     { canLoadModule, canLoadModule, canLoadModule, canLoadModule, canLoadModule },
                     juce::Justification::centred,
                       [safeEditor = juce::Component::SafePointer<AvaAudioProcessorEditor>(this)] (const int selectedIndex)
                       {
                           if (safeEditor == nullptr)
                               return;

                           if (selectedIndex == 0)
                               safeEditor->loadTlsModule();
                           else if (selectedIndex == 1)
                               safeEditor->loadEqlModule();
                           else if (selectedIndex == 2)
                               safeEditor->loadFftModule();
                           else if (selectedIndex == 3)
                               safeEditor->loadDynModule();
                           else if (selectedIndex == 4)
                               safeEditor->loadTrsModule();
                       },
                     {},
                     {});
}

void AvaAudioProcessorEditor::closeActiveModule()
{
    if (audioProcessor.getActiveModule() == AvaAudioProcessor::ActiveModule::none)
        return;

    hostSlotMoveSourceIndex = -1;
    for (const auto& field : hostSlotNameFields)
        if (field != nullptr)
            field->setDragTargetOutlineVisible(false);

    std::vector<juce::String> closingModuleParameterIds;
    const auto collectParameterIds = [&closingModuleParameterIds] (const auto* processor)
    {
        if (processor == nullptr)
            return;

        for (const auto parameterState : processor->getValueTreeState().state)
        {
            const auto parameterId = parameterState.getProperty("id").toString();

            if (parameterId.isNotEmpty())
                closingModuleParameterIds.push_back(parameterId);
        }
    };

    switch (audioProcessor.getActiveModule())
    {
        case AvaAudioProcessor::ActiveModule::eql: collectParameterIds(audioProcessor.getEqlModuleProcessor()); break;
        case AvaAudioProcessor::ActiveModule::fft: collectParameterIds(audioProcessor.getFftModuleProcessor()); break;
        case AvaAudioProcessor::ActiveModule::tls: collectParameterIds(audioProcessor.getTlsModuleProcessor()); break;
        case AvaAudioProcessor::ActiveModule::dyn: collectParameterIds(audioProcessor.getDynModuleProcessor()); break;
        case AvaAudioProcessor::ActiveModule::trs: collectParameterIds(audioProcessor.getTrsModuleProcessor()); break;
        case AvaAudioProcessor::ActiveModule::none: break;
    }

    detachModuleEditorBindings();

    if (! audioProcessor.clearLoadedModule())
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::none);

    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
    {
        const auto& assignment = hostSlotAssignments[static_cast<size_t>(slotIndex)];

        if (std::find(closingModuleParameterIds.begin(),
                      closingModuleParameterIds.end(),
                      assignment.parameterId) != closingModuleParameterIds.end())
        {
            clearHostSlot(slotIndex);
        }
    }

    filterViewport.setVisible(false);
    filterViewport.setBounds({});
    filterContent.setSize(0, 0);

    auto hideComponent = [] (juce::Component* component)
    {
        if (component == nullptr)
            return;

        component->setVisible(false);
        component->setBounds({});
    };

    hideComponent(addFilterButton.get());
    hideComponent(sortPlaceButton.get());
    hideComponent(sortFreqButton.get());
    hideComponent(sortDuoButton.get());

    if (presetsSection != nullptr)
    {
        hideComponent(&presetsSection->presetCombo);
        hideComponent(presetsSection->adButton.get());
        hideComponent(presetsSection->saveButton.get());
        hideComponent(presetsSection->renameButton.get());
        hideComponent(presetsSection->defaultButton.get());
        hideComponent(presetsSection->deleteButton.get());
    }

    for (auto& section : filterSections)
    {
        if (section == nullptr)
            continue;

        section->expanded = false;
        hideComponent(section->header.get());
        hideComponent(section->typeControl.get());
        hideComponent(section->placeControl.get());
        hideComponent(section->slopeControl.get());
        hideComponent(section->frequencyControl.get());
        hideComponent(section->bandwidthControl.get());
        hideComponent(section->gainControl.get());
        hideComponent(section->bypassButton.get());
    }

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    ensureModuleTitle();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::loadEqlModule()
{
    if (! audioProcessor.loadModule(AvaAudioProcessor::ActiveModule::eql))
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::eql);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    enforceSingleExpandedFilterSection();
    syncEditorWidthToBounds();
    ensureModuleTitle();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::loadTlsModule()
{
    if (! audioProcessor.loadModule(AvaAudioProcessor::ActiveModule::tls))
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::tls);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    ensureModuleTitle();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::loadDynModule()
{
    if (! audioProcessor.loadModule(AvaAudioProcessor::ActiveModule::dyn))
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::dyn);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    ensureModuleTitle();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::loadTrsModule()
{
    if (! audioProcessor.loadModule(AvaAudioProcessor::ActiveModule::trs))
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::trs);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    ensureModuleTitle();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::ensureModuleTitle()
{
    if (moduleTitle == nullptr)
    {
        moduleTitle = std::make_unique<BoxTextButton>(uiGrey500);
        moduleTitle->setTextJustification(juce::Justification::centred);
        moduleTitle->setAlwaysAccentOutline(false);
        moduleTitle->setToggleAccentVisible(false);
        moduleTitle->setLongPressAction([this]
        {
            juce::MessageManager::callAsync([safeEditor = juce::Component::SafePointer<AvaAudioProcessorEditor>(this)]
            {
                if (safeEditor == nullptr)
                    return;

                safeEditor->closeActiveModule();
                clearKeyboardFocus(*safeEditor);
            });
        }, 500, "CLOSE?");
        addAndMakeVisible(*moduleTitle);
    }
}

void AvaAudioProcessorEditor::toggleHostParametersSection()
{
    hostParametersExpanded = ! hostParametersExpanded;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}
