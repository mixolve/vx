#include "shell.EditorControls.h"

#include <utility>

void VxAudioProcessorEditor::showModulePicker()
{
    if (moduleAddButton == nullptr)
        return;

    if (audioProcessor.getLoadedModuleCount() != 0)
        return;

    auto anchorBounds = moduleAddButton->getBounds();
    anchorBounds.setSize(juce::jmax(120, anchorBounds.getWidth()), anchorBounds.getHeight());
    anchorBounds.setCentre(moduleAddButton->getBounds().getCentre());

    showChoicePrompt(anchorBounds,
                     { "MIE", "EQE", "SPE", "MXE", "TSE" },
                     -1,
                     { audioProcessor.getLoadedModuleCount() == 0,
                       audioProcessor.getLoadedModuleCount() == 0,
                       audioProcessor.getLoadedModuleCount() == 0,
                       audioProcessor.getLoadedModuleCount() == 0,
                       audioProcessor.getLoadedModuleCount() == 0 },
                     juce::Justification::centred,
                       [this] (const int selectedIndex)
                       {
                           if (selectedIndex == 0)
                               loadMieModule();
                           else if (selectedIndex == 1)
                               loadEqeModule();
                           else if (selectedIndex == 2)
                               loadSpeModule();
                           else if (selectedIndex == 3)
                               loadMxeModule();
                           else if (selectedIndex == 4)
                               loadTseModule();
                       });
}

void VxAudioProcessorEditor::loadEqeModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::eqe))
        return;

    eqeModuleLoaded = true;
    speModuleLoaded = false;
    mieModuleLoaded = false;
    mxeModuleLoaded = false;
    tseModuleLoaded = false;

    shellGlobalHostExpanded = false;
    visualizerExpanded = false;

    rebindActiveModuleEditors();
    updateEditorWidthForVisualizerVisibility();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadMieModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::mie))
        return;

    mieModuleLoaded = true;
    eqeModuleLoaded = false;
    speModuleLoaded = false;
    mxeModuleLoaded = false;
    tseModuleLoaded = false;

    shellGlobalHostExpanded = false;
    visualizerExpanded = false;

    rebindActiveModuleEditors();
    updateEditorWidthForVisualizerVisibility();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadMxeModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::mxe))
        return;

    mxeModuleLoaded = true;
    eqeModuleLoaded = false;
    speModuleLoaded = false;
    mieModuleLoaded = false;
    tseModuleLoaded = false;

    shellGlobalHostExpanded = false;
    visualizerExpanded = false;

    rebindActiveModuleEditors();
    updateEditorWidthForVisualizerVisibility();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadTseModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::tse))
        return;

    tseModuleLoaded = true;
    eqeModuleLoaded = false;
    speModuleLoaded = false;
    mieModuleLoaded = false;
    mxeModuleLoaded = false;

    shellGlobalHostExpanded = false;
    visualizerExpanded = false;

    rebindActiveModuleEditors();
    updateEditorWidthForVisualizerVisibility();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::rebuildModuleTabRows()
{
    const auto rowCount = audioProcessor.getLoadedModuleCount();

    while (static_cast<int>(moduleTabRows.size()) > rowCount)
    {
        auto& row = moduleTabRows.back();
        removeChildComponent(row->tabButton.get());
        moduleTabRows.pop_back();
    }

    while (static_cast<int>(moduleTabRows.size()) < rowCount)
    {
        auto row = std::make_unique<ModuleTabRow>();
        row->tabButton = std::make_unique<BoxTextButton>(uiClip);
        row->tabButton->setTextJustification(juce::Justification::centred);
        row->tabButton->setInterceptsMouseClicks(false, false);

        addAndMakeVisible(*row->tabButton);
        moduleTabRows.push_back(std::move(row));
    }

    for (int rowIndex = 0; rowIndex < static_cast<int>(moduleTabRows.size()); ++rowIndex)
    {
        auto& row = *moduleTabRows[static_cast<size_t>(rowIndex)];
        row.slotIndex = rowIndex;
        row.tabButton->setButtonText(audioProcessor.getLoadedModuleLabelAtPosition(rowIndex));
    }
}

void VxAudioProcessorEditor::openShellGlobalHostSection()
{
    shellGlobalHostExpanded = ! shellGlobalHostExpanded;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}
