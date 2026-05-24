#include "shell.EditorControls.h"

#include <utility>

namespace
{
std::unique_ptr<BoxTextButton> makeModuleMoveButton(const char* tooltip, const BoxTextButton::ArrowDirection direction)
{
    auto button = std::make_unique<BoxTextButton>(uiGrey500);
    button->setButtonText({});
    button->setTooltip(tooltip);
    button->setArrowDirection(direction);
    button->setCancelClickOnLeave(true);
    return button;
}
}

void VxAudioProcessorEditor::showModulePicker()
{
    if (moduleAddButton == nullptr)
        return;

    auto anchorBounds = moduleAddButton->getBounds();
    anchorBounds.setSize(juce::jmax(120, anchorBounds.getWidth()), anchorBounds.getHeight());
    anchorBounds.setCentre(moduleAddButton->getBounds().getCentre());

    showChoicePrompt(anchorBounds,
                     { "EQE", "SPE", "MXE", "TSE" },
                     -1,
                     { audioProcessor.getLoadedModuleCount() < VxAudioProcessor::maxModuleSlotCount,
                       audioProcessor.getLoadedModuleCount() < VxAudioProcessor::maxModuleSlotCount,
                       audioProcessor.getLoadedModuleCount() < VxAudioProcessor::maxModuleSlotCount,
                       audioProcessor.getLoadedModuleCount() < VxAudioProcessor::maxModuleSlotCount },
                     juce::Justification::centred,
                       [this] (const int selectedIndex)
                       {
                           if (selectedIndex == 0)
                               loadEqeModule();
                           else if (selectedIndex == 1)
                               loadSpeModule();
                           else if (selectedIndex == 2)
                               loadMxeModule();
                           else if (selectedIndex == 3)
                               loadTseModule();
                       });
}

void VxAudioProcessorEditor::loadEqeModule()
{
    if (! audioProcessor.addModuleToChain(VxAudioProcessor::ActiveModule::eqe))
        return;

    eqeModuleLoaded = true;
    speModuleLoaded = false;
    mxeModuleLoaded = false;
    tseModuleLoaded = false;
    eqeModuleExpanded = true;

    shellGlobalExpanded = false;
    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;
    visualizerVisible = false;
    expandedBellIndex = -1;

    rebindActiveModuleEditors();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadMxeModule()
{
    if (! audioProcessor.addModuleToChain(VxAudioProcessor::ActiveModule::mxe))
        return;

    mxeModuleLoaded = true;
    eqeModuleLoaded = false;
    speModuleLoaded = false;
    tseModuleLoaded = false;
    eqeModuleExpanded = true;

    shellGlobalExpanded = false;
    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;
    visualizerVisible = false;
    expandedBellIndex = -1;

    rebindActiveModuleEditors();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::loadTseModule()
{
    if (! audioProcessor.addModuleToChain(VxAudioProcessor::ActiveModule::tse))
        return;

    tseModuleLoaded = true;
    eqeModuleLoaded = false;
    speModuleLoaded = false;
    mxeModuleLoaded = false;
    eqeModuleExpanded = true;

    shellGlobalExpanded = false;
    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;
    visualizerVisible = false;
    expandedBellIndex = -1;

    rebindActiveModuleEditors();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

void VxAudioProcessorEditor::moveModuleSlot(const int slotIndex, const int direction)
{
    if (audioProcessor.moveModuleInChainAtPosition(slotIndex, direction))
    {
        rebuildModuleTabRows();
        storeEditorStateToValueTree();
        updateSectionStates();
        resized();
        scheduleHistorySnapshot();
    }
}

void VxAudioProcessorEditor::openModuleSlot(const int slotIndex)
{
    const auto slot = audioProcessor.getLoadedModuleSlotAtPosition(slotIndex);

    if (slot.module == VxAudioProcessor::ActiveModule::none)
        return;

    const auto activeModule = audioProcessor.getActiveModule();
    const auto activeInstanceIndex = audioProcessor.getActiveModuleInstanceIndex();
    const auto clickingCurrentTab = activeModule == slot.module
        && activeInstanceIndex == slot.instanceIndex
        && ((slot.module == VxAudioProcessor::ActiveModule::eqe && eqeModuleLoaded)
            || (slot.module == VxAudioProcessor::ActiveModule::spe && speModuleLoaded)
            || (slot.module == VxAudioProcessor::ActiveModule::mxe && mxeModuleLoaded)
            || (slot.module == VxAudioProcessor::ActiveModule::tse && tseModuleLoaded));

    if (clickingCurrentTab)
    {
        toggleEqeModule();
        return;
    }

    eqeModuleLoaded = slot.module == VxAudioProcessor::ActiveModule::eqe;
    speModuleLoaded = slot.module == VxAudioProcessor::ActiveModule::spe;
    mxeModuleLoaded = slot.module == VxAudioProcessor::ActiveModule::mxe;
    tseModuleLoaded = slot.module == VxAudioProcessor::ActiveModule::tse;
    audioProcessor.setActiveModule(slot.module, slot.instanceIndex);
    eqeModuleExpanded = true;
    shellGlobalExpanded = false;
    globalExpanded = false;
    filtersExpanded = false;
    speMainExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;
    expandedBellIndex = -1;

    rebindActiveModuleEditors();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::rebuildModuleTabRows()
{
    const auto rowCount = audioProcessor.getLoadedModuleCount();

    while (static_cast<int>(moduleTabRows.size()) > rowCount)
    {
        auto& row = moduleTabRows.back();
        removeChildComponent(row->moveUpButton.get());
        removeChildComponent(row->tabButton.get());
        removeChildComponent(row->moveDownButton.get());
        moduleTabRows.pop_back();
    }

    while (static_cast<int>(moduleTabRows.size()) < rowCount)
    {
        auto row = std::make_unique<ModuleTabRow>();
        row->moveUpButton = makeModuleMoveButton("Move module up", BoxTextButton::ArrowDirection::up);
        row->tabButton = std::make_unique<BoxTextButton>(uiClip);
        row->tabButton->setTextJustification(juce::Justification::centred);
        row->moveDownButton = makeModuleMoveButton("Move module down", BoxTextButton::ArrowDirection::down);

        auto* rowPointer = row.get();
        row->moveUpButton->onClick = [this, rowPointer]
        {
            moveModuleSlot(rowPointer->slotIndex, -1);
            clearKeyboardFocus(*this);
        };
        row->tabButton->onClick = [this, rowPointer]
        {
            openModuleSlot(rowPointer->slotIndex);
            clearKeyboardFocus(*this);
        };
        row->moveDownButton->onClick = [this, rowPointer]
        {
            moveModuleSlot(rowPointer->slotIndex, 1);
            clearKeyboardFocus(*this);
        };

        addAndMakeVisible(*row->moveUpButton);
        addAndMakeVisible(*row->tabButton);
        addAndMakeVisible(*row->moveDownButton);
        moduleTabRows.push_back(std::move(row));
    }

    for (int rowIndex = 0; rowIndex < static_cast<int>(moduleTabRows.size()); ++rowIndex)
    {
        auto& row = *moduleTabRows[static_cast<size_t>(rowIndex)];
        row.slotIndex = rowIndex;
        row.tabButton->setButtonText(audioProcessor.getLoadedModuleLabelAtPosition(rowIndex));
    }
}

void VxAudioProcessorEditor::toggleEqeModule()
{
    if (! eqeModuleLoaded && ! speModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
        return;

    eqeModuleExpanded = ! eqeModuleExpanded;

    if (eqeModuleExpanded)
        shellGlobalExpanded = false;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::openShellGlobalSection()
{
    const auto shouldOpen = ! shellGlobalExpanded;
    shellGlobalExpanded = shouldOpen;

    if (shouldOpen)
    {
        eqeModuleExpanded = false;
        globalExpanded = false;
        speMainExpanded = false;
        filtersExpanded = false;
        presetsExpanded = false;
        visualizerExpanded = false;
        expandedBellIndex = -1;
    }

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::openShellGlobalMiscSection()
{
    if (! shellGlobalExpanded)
        return;

    shellGlobalMiscExpanded = ! shellGlobalMiscExpanded;
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::openGlobalSection()
{
    if (! eqeModuleLoaded && ! speModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
        return;

    const auto clickedExpanded = globalExpanded
        && ! shellGlobalExpanded
        && ! speMainExpanded
        && ! filtersExpanded
        && ! presetsExpanded
        && ! visualizerExpanded
        ;
    shellGlobalExpanded = false;
    globalExpanded = ! clickedExpanded;
    speMainExpanded = false;
    filtersExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;

    if (globalExpanded)
    {
        expandedBellIndex = -1;
        globalViewport.setViewPosition(0, 0);
    }

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::openSpeMainSection()
{
    if (! speModuleLoaded)
        return;

    const auto clickedExpanded = speMainExpanded
        && ! shellGlobalExpanded
        && ! globalExpanded
        && ! filtersExpanded
        && ! presetsExpanded
        && ! visualizerExpanded;
    shellGlobalExpanded = false;
    globalExpanded = false;
    speMainExpanded = ! clickedExpanded;
    filtersExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;
    expandedBellIndex = -1;

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::closeActiveModule()
{
    if (! eqeModuleLoaded && ! speModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
        return;

    auto closingModule = audioProcessor.getActiveModule();

    if (closingModule == VxAudioProcessor::ActiveModule::none)
    {
        if (eqeModuleLoaded)
            closingModule = VxAudioProcessor::ActiveModule::eqe;
        else if (speModuleLoaded)
            closingModule = VxAudioProcessor::ActiveModule::spe;
        else if (mxeModuleLoaded)
            closingModule = VxAudioProcessor::ActiveModule::mxe;
        else if (tseModuleLoaded)
            closingModule = VxAudioProcessor::ActiveModule::tse;
    }

    const auto closingEqe = closingModule == VxAudioProcessor::ActiveModule::eqe;
    const auto closingSpe = closingModule == VxAudioProcessor::ActiveModule::spe;
    const auto closingMxe = closingModule == VxAudioProcessor::ActiveModule::mxe;
    const auto closingTse = closingModule == VxAudioProcessor::ActiveModule::tse;

    eqeModuleLoaded = false;
    speModuleLoaded = false;
    mxeModuleLoaded = false;
    tseModuleLoaded = false;
    eqeModuleExpanded = false;
    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = false;
    presetsExpanded = false;
    visualizerExpanded = false;

    if (visualizerVisible)
    {
        visualizerVisible = false;
        updateEditorWidthForVisualizerVisibility();
    }
    expandedBellIndex = -1;

    const auto closingInstanceIndex = audioProcessor.getActiveModuleInstanceIndex();

    if (closingEqe)
        audioProcessor.removeModuleFromChain(VxAudioProcessor::ActiveModule::eqe, closingInstanceIndex);

    if (closingSpe)
        audioProcessor.removeModuleFromChain(VxAudioProcessor::ActiveModule::spe, closingInstanceIndex);

    if (closingMxe)
        audioProcessor.removeModuleFromChain(VxAudioProcessor::ActiveModule::mxe, closingInstanceIndex);

    if (closingTse)
        audioProcessor.removeModuleFromChain(VxAudioProcessor::ActiveModule::tse, closingInstanceIndex);

    const auto nextActiveModule = audioProcessor.getActiveModule();
    eqeModuleLoaded = nextActiveModule == VxAudioProcessor::ActiveModule::eqe;
    speModuleLoaded = nextActiveModule == VxAudioProcessor::ActiveModule::spe;
    mxeModuleLoaded = nextActiveModule == VxAudioProcessor::ActiveModule::mxe;
    tseModuleLoaded = nextActiveModule == VxAudioProcessor::ActiveModule::tse;

    rebindActiveModuleEditors();
    rebuildModuleTabRows();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}
