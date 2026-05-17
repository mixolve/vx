#include "EditorPresetSections.h"

namespace
{
void configurePresetCombo(NoTickComboBox& combo)
{
    combo.setEditableText(false);
    combo.setJustificationType(juce::Justification::centred);
    combo.setPopupMenuTextJustification(juce::Justification::centred);
    combo.setColour(juce::ComboBox::backgroundColourId, uiGrey800);
    combo.setColour(juce::ComboBox::outlineColourId, uiGrey500);
    combo.setColour(juce::ComboBox::textColourId, uiWhite);
    combo.setColour(juce::ComboBox::arrowColourId, uiWhite);
    combo.setColour(juce::ComboBox::buttonColourId, uiGrey800);
    combo.setWantsKeyboardFocus(false);
    combo.setMouseClickGrabsKeyboardFocus(false);
    combo.setPromptStylePopupEnabled(true);
}

std::unique_ptr<BoxTextButton> makeSectionButton(const juce::String& text, const juce::Colour accent)
{
    auto button = std::make_unique<BoxTextButton>(accent);
    button->setButtonText(text);
    return button;
}
}

EqeAudioProcessorEditor::PresetsSection::PresetsSection()
    : header(std::make_unique<BoxTextButton>(uiAccent))
{
    configurePresetCombo(presetCombo);

    adButton = makeSectionButton("AD", uiAccent);
    saveButton = makeSectionButton("SV", uiGrey500);
    renameButton = makeSectionButton("RN", uiGrey500);
    defaultButton = makeSectionButton("DF", uiAccent);
    deleteButton = makeSectionButton("DL", uiAccent);

    auto handlePresetSelection = [this]
    {
        selectedPresetName = presetCombo.getText().trim();

        if (onPresetSelected)
            onPresetSelected();
    };

    presetCombo.onChange = [this, handlePresetSelection]
    {
        if (ignorePresetCallbacks)
            return;

        if (presetCombo.getSelectedItemIndex() < 0)
            return;

        handlePresetSelection();
    };

    presetCombo.onReselectedCurrentItem = handlePresetSelection;

    header->setButtonText("PRESETS");
    header->setClickingTogglesState(true);
}

void EqeAudioProcessorEditor::PresetsSection::beginRename()
{
    if (selectedPresetName.isEmpty() || onRenameRequested == nullptr)
        return;

    onRenameRequested(selectedPresetName);
}

int EqeAudioProcessorEditor::PresetsSection::getPresetRowPreferredHeight() const noexcept
{
    return (rowHeight * 2) + verticalGap;
}

juce::String EqeAudioProcessorEditor::PresetsSection::getSelectedPresetName() const
{
    return selectedPresetName;
}

juce::String EqeAudioProcessorEditor::PresetsSection::getEnteredPresetName() const
{
    return presetCombo.getText().trim();
}

void EqeAudioProcessorEditor::PresetsSection::setPresetNames(const juce::StringArray& presetNames,
                                                             const juce::String& preferredSelection)
{
    const juce::ScopedValueSetter<bool> scopedIgnore(ignorePresetCallbacks, true);
    presetCombo.clear(juce::dontSendNotification);

    for (int index = 0; index < presetNames.size(); ++index)
        presetCombo.addItem(presetNames[index], index + 1);

    if (presetNames.isEmpty())
    {
        presetCombo.setText({}, juce::dontSendNotification);
        selectedPresetName.clear();
        return;
    }

    const auto selectedName = preferredSelection.isNotEmpty() ? preferredSelection
                                                              : presetNames[0];
    const auto selectedIndex = presetNames.indexOf(selectedName);
    presetCombo.setSelectedItemIndex(selectedIndex >= 0 ? selectedIndex : 0, juce::dontSendNotification);
    selectedPresetName = presetCombo.getText().trim();
}
