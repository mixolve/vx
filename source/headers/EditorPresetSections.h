#pragma once

#include "EditorParameterControls.h"

struct EqeAudioProcessorEditor::PresetsSection
{
    PresetsSection();

    void beginRename();
    int getPresetRowPreferredHeight() const noexcept;
    juce::String getSelectedPresetName() const;
    juce::String getEnteredPresetName() const;
    void setPresetNames(const juce::StringArray& presetNames, const juce::String& preferredSelection);

    std::unique_ptr<BoxTextButton> header;
    NoTickComboBox presetCombo;
    std::unique_ptr<BoxTextButton> adButton;
    std::unique_ptr<BoxTextButton> saveButton;
    std::unique_ptr<BoxTextButton> renameButton;
    std::unique_ptr<BoxTextButton> defaultButton;
    std::unique_ptr<BoxTextButton> deleteButton;
    bool ignorePresetCallbacks = false;
    juce::String selectedPresetName;
    std::function<void()> onPresetSelected;
    std::function<void(const juce::String&)> onRenameRequested;
};
