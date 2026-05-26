#include "shell.EditorBellSection.h"
#include "shell.EditorPresetSections.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"

namespace
{
struct NumberedNameSeed
{
    juce::String prefix;
    bool hasSeparatorBeforeNumber = false;
    int nextNumber = 2;
};

NumberedNameSeed makeNumberedNameSeed(const juce::String& name)
{
    const auto trimmedName = name.trim();
    auto splitIndex = trimmedName.length();

    while (splitIndex > 0 && juce::CharacterFunctions::isDigit(trimmedName[splitIndex - 1]))
        --splitIndex;

    if (splitIndex == trimmedName.length())
        return { trimmedName, true, 2 };

    const auto numberText = trimmedName.substring(splitIndex);
    const auto prefix = trimmedName.substring(0, splitIndex);

    return { prefix, false, numberText.getIntValue() + 1 };
}

juce::String buildNumberedName(const juce::String& prefix,
                               const int number,
                               const bool insertSeparator)
{
    if (prefix.isEmpty())
        return juce::String(number);

    return insertSeparator ? prefix + " " + juce::String(number)
                           : prefix + juce::String(number);
}

juce::String makeUniquePresetName(const juce::String& requestedName,
                                  const juce::StringArray& existingNames,
                                  const juce::String& fallbackPrefix)
{
    auto baseName = requestedName.trim();

    if (baseName.isEmpty())
    {
        auto nextIndex = existingNames.size() + 1;
        auto generatedName = fallbackPrefix + " " + juce::String(nextIndex);

        while (existingNames.contains(generatedName, true))
            generatedName = fallbackPrefix + " " + juce::String(++nextIndex);

        return generatedName;
    }

    if (! existingNames.contains(baseName, true))
        return baseName;

    const auto numberedSeed = makeNumberedNameSeed(baseName);
    auto candidateNumber = numberedSeed.nextNumber;
    auto candidate = buildNumberedName(numberedSeed.prefix, candidateNumber, numberedSeed.hasSeparatorBeforeNumber);

    while (existingNames.contains(candidate, true))
        candidate = buildNumberedName(numberedSeed.prefix,
                                      ++candidateNumber,
                                      numberedSeed.hasSeparatorBeforeNumber);

    return candidate;
}
}

void VxAudioProcessorEditor::openPresetsSection()
{
    if (! eqeModuleLoaded && ! speModuleLoaded && ! mxeModuleLoaded && ! tseModuleLoaded)
        return;

    const auto clickedExpanded = presetsExpanded
        && ! globalExpanded
        && ! speMainExpanded
        && ! filtersExpanded
        && ! visualizerExpanded;
    presetsExpanded = ! clickedExpanded;
    globalExpanded = false;
    speMainExpanded = false;
    filtersExpanded = false;
    visualizerExpanded = false;
    expandedBellIndex = -1;

    if (presetsExpanded)
        refreshFilterPresetList(getActiveEqeProcessor() != nullptr ? getActiveEqeProcessor()->getLastFilterPresetName()
                                                                   : juce::String {});

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::refreshFilterPresetList(const juce::String& preferredSelection)
{
    if (presetsSection == nullptr)
        return;

    const auto* eqeProcessor = getActiveEqeProcessor();
    const auto presetNames = eqeProcessor != nullptr ? eqeProcessor->getFilterPresetNames()
                                                     : juce::StringArray {};
    presetsSection->setPresetNames(presetNames, preferredSelection);

    const auto hasPresetSelection = presetNames.size() > 0
        && presetsSection->getSelectedPresetName().isNotEmpty();
    const auto selectedPresetName = presetsSection->getSelectedPresetName();
    const auto defaultPresetName = eqeProcessor != nullptr ? eqeProcessor->getDefaultFilterPresetName()
                                                           : juce::String {};
    const auto selectedPresetIsDefault = hasPresetSelection
        && selectedPresetName.equalsIgnoreCase(defaultPresetName);
    const auto canDeletePreset = presetNames.size() > 1 && hasPresetSelection;
    presetsSection->deleteButton->setEnabled(canDeletePreset);
    presetsSection->deleteButton->setAlpha(canDeletePreset ? 1.0f : 0.45f);
    presetsSection->renameButton->setEnabled(hasPresetSelection);
    presetsSection->renameButton->setAlpha(hasPresetSelection ? 1.0f : 0.45f);
    presetsSection->defaultButton->setEnabled(hasPresetSelection);
    presetsSection->defaultButton->setAlpha(hasPresetSelection ? 1.0f : 0.45f);
    presetsSection->defaultButton->setAlwaysAccentOutline(selectedPresetIsDefault);
}

void VxAudioProcessorEditor::reloadFilterPresetFromProcessor()
{
    if (presetsSection == nullptr)
        return;

    restoreEditorStateFromValueTree();

    for (auto& sectionPtr : bellSections)
    {
        auto* section = sectionPtr.get();

        if (section == nullptr)
            continue;

        const auto loadedType = section->getFilterType();
        section->lastFilterType = loadedType;
        section->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(loadedType));
        section->slopeControl->setChoiceEnabled(0, loadedType != EqeModuleProcessor::FilterType::bell);

        for (const auto filterType : VxAudioProcessor::filterTypePresetOrder)
        {
            section->setStoredValues(filterType,
                                     defaultFilterFrequencyForType(filterType),
                                     defaultFilterBandwidthForType(filterType),
                                     defaultFilterSlopeForType(filterType),
                                     0,
                                     false);
        }

        section->captureCurrentValuesForCurrentType(true);
    }

    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::addFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    const auto presetName = makeUniquePresetName(presetsSection->getEnteredPresetName(),
                                                 getActiveEqeProcessor() != nullptr ? getActiveEqeProcessor()->getFilterPresetNames()
                                                                                    : juce::StringArray {},
                                                 "PRESET");

    if (auto* eqeProcessor = getActiveEqeProcessor(); eqeProcessor != nullptr && eqeProcessor->saveFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

void VxAudioProcessorEditor::saveFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    auto presetName = presetsSection->getEnteredPresetName();
    auto* eqeProcessor = getActiveEqeProcessor();

    if (eqeProcessor == nullptr)
        return;

    if (presetName.isEmpty())
        presetName = makeUniquePresetName({}, eqeProcessor->getFilterPresetNames(), "PRESET");

    if (presetName.isEmpty())
        return;

    if (eqeProcessor->saveFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

bool VxAudioProcessorEditor::renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName)
{
    const auto trimmedOldName = oldPresetName.trim();
    const auto trimmedNewName = newPresetName.trim();

    if (trimmedOldName.isEmpty() || trimmedNewName.isEmpty())
        return false;

    auto* eqeProcessor = getActiveEqeProcessor();

    if (eqeProcessor == nullptr || ! eqeProcessor->renameFilterPreset(trimmedOldName, trimmedNewName))
        return false;

    refreshFilterPresetList(trimmedNewName);
    return true;
}

void VxAudioProcessorEditor::setDefaultFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    const auto presetName = presetsSection->getSelectedPresetName();

    if (presetName.isEmpty())
        return;

    if (auto* eqeProcessor = getActiveEqeProcessor(); eqeProcessor != nullptr && eqeProcessor->setDefaultFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

void VxAudioProcessorEditor::deleteSelectedFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    auto* eqeProcessor = getActiveEqeProcessor();

    if (eqeProcessor == nullptr || eqeProcessor->getFilterPresetNames().size() <= 1)
        return;

    const auto presetName = presetsSection->getSelectedPresetName();

    if (presetName.isEmpty())
        return;

    if (eqeProcessor->deleteFilterPreset(presetName))
        refreshFilterPresetList();
}
