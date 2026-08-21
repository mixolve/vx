#include "shell.EditorControls.h"
#include "shell.EditorFilterSection.h"
#include "shell.EditorPresetSections.h"

#include <utility>

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

void AvaAudioProcessorEditor::closeActiveModule()
{
    if (audioProcessor.getActiveModule() == AvaAudioProcessor::ActiveModule::none)
        return;

    detachModuleEditorBindings();

    if (! audioProcessor.clearLoadedModule())
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::none);

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

void AvaAudioProcessorEditor::loadEqlModule()
{
    if (! audioProcessor.loadModule(AvaAudioProcessor::ActiveModule::eql))
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::eql);

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

void AvaAudioProcessorEditor::loadTlsModule()
{
    if (! audioProcessor.loadModule(AvaAudioProcessor::ActiveModule::tls))
        return;

    setLoadedModuleFlags(AvaAudioProcessor::ActiveModule::tls);

    hostParametersExpanded = false;

    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    refreshModuleTabButton();
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
    refreshModuleTabButton();
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
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void AvaAudioProcessorEditor::refreshModuleTabButton()
{
    if (moduleTabButton == nullptr)
    {
        moduleTabButton = std::make_unique<BoxTextButton>(uiGrey500);
        moduleTabButton->setTextJustification(juce::Justification::centred);
        moduleTabButton->setFillVisible(false);
        moduleTabButton->setAlwaysAccentOutline(false);
        moduleTabButton->setToggleAccentVisible(false);
        moduleTabButton->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*moduleTabButton);
    }

    moduleTabButton->setButtonText(juce::String(AvaAudioProcessor::stateIdForModule(audioProcessor.getActiveModule())).toUpperCase());
    moduleTabButton->setVisible(audioProcessor.getActiveModule() != AvaAudioProcessor::ActiveModule::none);
}

void AvaAudioProcessorEditor::toggleHostParametersSection()
{
    hostParametersExpanded = ! hostParametersExpanded;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}
