#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::setupPresetControls()
{
    presetsSection = std::make_unique<PresetsSection>();
    presetsSection->onPresetSelected = [this]
    {
        if (presetsSection == nullptr)
            return;

        const auto presetName = presetsSection->getSelectedPresetName();

        if (presetName.isEmpty())
            return;

        const juce::ScopedValueSetter<bool> suppressHandlers(suppressFilterSectionValueChangeHandlers, true);

        if (auto* eqlProcessor = getActiveEqlProcessor(); eqlProcessor != nullptr && eqlProcessor->loadFilterPreset(presetName))
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
        clearKeyboardFocus(*this);
    };
    presetsSection->deleteButton->setLongPressAction([this]
    {
        deleteSelectedFilterPreset();
        clearKeyboardFocus(*this);
    }, 500, "S?");
    addAndMakeVisible(presetsSection->presetCombo);
    addAndMakeVisible(*presetsSection->adButton);
    addAndMakeVisible(*presetsSection->saveButton);
    addAndMakeVisible(*presetsSection->renameButton);
    addAndMakeVisible(*presetsSection->defaultButton);
    addAndMakeVisible(*presetsSection->deleteButton);
    presetsSection->onRenameRequested = [this] (const juce::String& currentName)
    {
        auto promptBounds = moduleTabButton != nullptr ? moduleTabButton->getBounds()
                                                       : juce::Rectangle<int>();

        if (! promptBounds.isEmpty())
            promptBounds.setY(juce::roundToInt(static_cast<float>(getHeight()) * editorInsetTopRatio));

        showTextPrompt(currentName,
                       [this, currentName] (const juce::String& newName)
                       {
                           if (! renameFilterPreset(currentName, newName))
                               return false;

                           clearKeyboardFocus(*this);
                           return true;
                       },
                       promptBounds);
    };
    if (auto* eqlProcessor = getActiveEqlProcessor())
        refreshFilterPresetList(eqlProcessor->getLastFilterPresetName());
    else
        refreshFilterPresetList({});
}
