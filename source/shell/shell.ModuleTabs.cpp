#include "shell.EditorControls.h"
#include "shell.EditorFilterSection.h"
#include "shell.EditorPresetSections.h"

#include <utility>

void VxAudioProcessorEditor::showModulePicker()
{
    if (moduleAddButton == nullptr)
        return;

    const auto canLoadModule = audioProcessor.getActiveModule() == VxAudioProcessor::ActiveModule::none;

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
                       [this] (const int selectedIndex)
                       {
                           if (selectedIndex == 0)
                               loadTlsModule();
                           else if (selectedIndex == 1)
                               loadEqlModule();
                           else if (selectedIndex == 2)
                               loadFftModule();
                           else if (selectedIndex == 3)
                               loadDynModule();
                           else if (selectedIndex == 4)
                               loadTrsModule();
                     },
                     {},
                     {},
                     {});
}

void VxAudioProcessorEditor::closeActiveModule()
{
    if (audioProcessor.getActiveModule() == VxAudioProcessor::ActiveModule::none)
        return;

    detachModuleEditorBindings();

    if (! audioProcessor.clearLoadedModule())
        return;

    setLoadedModuleFlags(VxAudioProcessor::ActiveModule::none);

    hostParametersExpanded = false;
    for (int slotIndex = 0; slotIndex < static_cast<int>(hostSlotAssignments.size()); ++slotIndex)
        clearHostSlot(slotIndex);

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
        hideComponent(section->moveUpButton.get());
        hideComponent(section->moveDownButton.get());
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
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadEqlModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::eql))
        return;

    setLoadedModuleFlags(VxAudioProcessor::ActiveModule::eql);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    enforceSingleExpandedFilterSection();
    syncEditorWidthToBounds();
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadTlsModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::tls))
        return;

    setLoadedModuleFlags(VxAudioProcessor::ActiveModule::tls);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadDynModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::dyn))
        return;

    setLoadedModuleFlags(VxAudioProcessor::ActiveModule::dyn);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadTrsModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::trs))
        return;

    setLoadedModuleFlags(VxAudioProcessor::ActiveModule::trs);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::refreshModuleTabButton()
{
    if (moduleTabButton == nullptr)
    {
        moduleTabButton = std::make_unique<BoxTextButton>(uiClip);
        moduleTabButton->setTextJustification(juce::Justification::centred);
        moduleTabButton->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*moduleTabButton);
    }

    moduleTabButton->setButtonText(juce::String(VxAudioProcessor::stateIdForModule(audioProcessor.getActiveModule())).toUpperCase());
    moduleTabButton->setVisible(audioProcessor.getActiveModule() != VxAudioProcessor::ActiveModule::none);
}

void VxAudioProcessorEditor::toggleHostParametersSection()
{
    hostParametersExpanded = ! hostParametersExpanded;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}
