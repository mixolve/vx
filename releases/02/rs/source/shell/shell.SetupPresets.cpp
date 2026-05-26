#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::setupPresetControls()
{
    presetsSection = std::make_unique<PresetsSection>();
    presetsSection->header->onClick = [this]
    {
        openPresetsSection();
        clearKeyboardFocus(*this);
    };
    presetsSection->onPresetSelected = [this]
    {
        if (presetsSection == nullptr)
            return;

        const auto presetName = presetsSection->getSelectedPresetName();

        if (presetName.isEmpty())
            return;

        const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);

        if (auto* eqeProcessor = getActiveEqeProcessor(); eqeProcessor != nullptr && eqeProcessor->loadFilterPreset(presetName))
        {
            reloadFilterPresetFromProcessor();
            refreshFilterPresetList(presetName);
        }
    };
    presetsSection->adButton->onClick = [this]
    {
        addFilterPreset();
        clearKeyboardFocus(*this);
    };
    presetsSection->saveButton->onClick = [this]
    {
        saveFilterPreset();
        clearKeyboardFocus(*this);
    };
    presetsSection->renameButton->onClick = [this]
    {
        if (presetsSection != nullptr)
            presetsSection->beginRename();
    };
    presetsSection->defaultButton->onClick = [this]
    {
        setDefaultFilterPreset();
        clearKeyboardFocus(*this);
    };
    presetsSection->deleteButton->onClick = [this]
    {
        deleteSelectedFilterPreset();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*presetsSection->header);
    addAndMakeVisible(presetsSection->presetCombo);
    addAndMakeVisible(*presetsSection->adButton);
    addAndMakeVisible(*presetsSection->saveButton);
    addAndMakeVisible(*presetsSection->renameButton);
    addAndMakeVisible(*presetsSection->defaultButton);
    addAndMakeVisible(*presetsSection->deleteButton);
    presetsSection->onRenameRequested = [this] (const juce::String& currentName)
    {
        showTextPrompt(currentName,
                       [this, currentName] (const juce::String& newName)
                       {
                           if (! renameFilterPreset(currentName, newName))
                               return false;

                           clearKeyboardFocus(*this);
                           return true;
                       });
    };
    refreshFilterPresetList(getActiveEqeProcessor() != nullptr ? getActiveEqeProcessor()->getLastFilterPresetName()
                                                               : juce::String {});
}
