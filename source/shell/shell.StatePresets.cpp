#include "shell.EditorFilterSection.h"
#include "shell.EditorPresetSections.h"
#include "../modules/eql/module.eql.ProcessorSupport.h"

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

void VxAudioProcessorEditor::refreshFilterPresetList(const juce::String& preferredSelection)
{
    if (presetsSection == nullptr)
        return;

    const auto* eqlProcessor = getActiveEqlProcessor();
    const auto presetNames = eqlProcessor != nullptr ? eqlProcessor->getFilterPresetNames()
                                                     : juce::StringArray {};
    presetsSection->setPresetNames(presetNames, preferredSelection);

    const auto hasPresetSelection = presetNames.size() > 0
        && presetsSection->getSelectedPresetName().isNotEmpty();
    const auto selectedPresetName = presetsSection->getSelectedPresetName();
    const auto defaultPresetName = eqlProcessor != nullptr ? eqlProcessor->getDefaultFilterPresetName()
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

    filterDisplayOrder.clear();
    filterDisplayOrder.reserve(VxAudioProcessor::maxEqlFilterCount);

    for (int filterIndex = 0; filterIndex < VxAudioProcessor::maxEqlFilterCount; ++filterIndex)
        filterDisplayOrder.push_back(filterIndex);

    for (auto& sectionPtr : filterSections)
    {
        auto* section = sectionPtr.get();

        if (section == nullptr)
            continue;

        const auto loadedType = section->getFilterType();
        section->lastFilterType = loadedType;
        section->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(loadedType));
        section->slopeControl->setChoiceEnabled(0, loadedType != EqlModuleProcessor::FilterType::bell);
        section->updatePlaceChoicesForType(true);

        for (const auto filterType : VxAudioProcessor::filterTypePresetOrder)
        {
            section->setStoredValues(filterType,
                                     defaultFilterFrequencyForType(filterType),
                                     defaultFilterBandwidth(),
                                     defaultFilterSlope(),
                                     0,
                                     false);
        }

        section->captureCurrentValuesForCurrentType(true);
    }

    enforceSingleExpandedFilterSection();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
}

void VxAudioProcessorEditor::commitFilterDisplayOrderToProcessor()
{
    auto* eqlProcessor = getActiveEqlProcessor();
    const auto activeCount = getActiveFilterCount();

    if (eqlProcessor == nullptr || activeCount <= 1)
        return;

    if (static_cast<int>(filterDisplayOrder.size()) < activeCount)
        return;

    auto alreadyInOrder = true;
    std::vector<int> orderedFilterIndices;
    orderedFilterIndices.reserve(static_cast<size_t>(activeCount));

    for (int orderIndex = 0; orderIndex < activeCount; ++orderIndex)
    {
        const auto filterIndex = filterDisplayOrder[static_cast<size_t>(orderIndex)];
        orderedFilterIndices.push_back(filterIndex);

        if (filterIndex != orderIndex)
            alreadyInOrder = false;
    }

    if (alreadyInOrder || ! eqlProcessor->applyFilterOrder(orderedFilterIndices))
        return;

    filterDisplayOrder.clear();
    filterDisplayOrder.reserve(VxAudioProcessor::maxEqlFilterCount);

    for (int filterIndex = 0; filterIndex < VxAudioProcessor::maxEqlFilterCount; ++filterIndex)
        filterDisplayOrder.push_back(filterIndex);

    for (auto& sectionPtr : filterSections)
    {
        auto* section = sectionPtr.get();

        if (section == nullptr)
            continue;

        const auto loadedType = section->getFilterType();
        section->lastFilterType = loadedType;
        section->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(loadedType));
        section->slopeControl->setChoiceEnabled(0, loadedType != EqlModuleProcessor::FilterType::bell);
        section->updatePlaceChoicesForType(true);
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

    auto* eqlProcessor = getActiveEqlProcessor();
    const auto presetName = makeUniquePresetName(presetsSection->getEnteredPresetName(),
                                                 eqlProcessor != nullptr ? eqlProcessor->getFilterPresetNames()
                                                                         : juce::StringArray {},
                                                 "PRESET");

    commitFilterDisplayOrderToProcessor();

    if (eqlProcessor != nullptr && eqlProcessor->saveFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

void VxAudioProcessorEditor::saveFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    auto presetName = presetsSection->getEnteredPresetName();
    auto* eqlProcessor = getActiveEqlProcessor();

    if (eqlProcessor == nullptr)
        return;

    if (presetName.isEmpty())
        presetName = makeUniquePresetName({}, eqlProcessor->getFilterPresetNames(), "PRESET");

    if (presetName.isEmpty())
        return;

    commitFilterDisplayOrderToProcessor();

    if (eqlProcessor->saveFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

bool VxAudioProcessorEditor::renameFilterPreset(const juce::String& sourcePresetName, const juce::String& newPresetName)
{
    const auto trimmedSourceName = sourcePresetName.trim();
    const auto trimmedNewName = newPresetName.trim();

    if (trimmedSourceName.isEmpty() || trimmedNewName.isEmpty())
        return false;

    auto* eqlProcessor = getActiveEqlProcessor();

    if (eqlProcessor == nullptr || ! eqlProcessor->renameFilterPreset(trimmedSourceName, trimmedNewName))
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

    if (auto* eqlProcessor = getActiveEqlProcessor(); eqlProcessor != nullptr && eqlProcessor->setDefaultFilterPreset(presetName))
        refreshFilterPresetList(presetName);
}

void VxAudioProcessorEditor::deleteSelectedFilterPreset()
{
    if (presetsSection == nullptr)
        return;

    auto* eqlProcessor = getActiveEqlProcessor();

    if (eqlProcessor == nullptr || eqlProcessor->getFilterPresetNames().size() <= 1)
        return;

    const auto presetName = presetsSection->getSelectedPresetName();

    if (presetName.isEmpty())
        return;

    if (eqlProcessor->deleteFilterPreset(presetName))
        refreshFilterPresetList();
}
