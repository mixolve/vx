#include "ProcessorSupport.h"
#include "PresetManagement.h"
#include "StateRestoration.h"

#include <memory>

namespace
{
inline constexpr auto defaultPresetAttribute = "is_default";

juce::String getStoredDefaultPresetName(const juce::XmlElement& presetsXml)
{
    for (auto* child : presetsXml.getChildIterator())
    {
        if (! child->hasTagName(presetTag) || ! child->getBoolAttribute(defaultPresetAttribute))
            continue;

        const auto presetName = child->getStringAttribute("name").trim();

        if (presetName.isNotEmpty())
            return presetName;
    }

    return {};
}

bool selectStoredDefaultPreset(juce::XmlElement& presetsXml, const juce::String& presetName)
{
    auto found = false;

    for (auto* child : presetsXml.getChildIterator())
    {
        if (! child->hasTagName(presetTag))
            continue;

        const auto selected = child->getStringAttribute("name").trim().equalsIgnoreCase(presetName);

        if (selected)
        {
            child->setAttribute(defaultPresetAttribute, true);
            found = true;
        }
        else
        {
            child->removeAttribute(defaultPresetAttribute);
        }
    }

    return found;
}
} // namespace

namespace eql_presets
{
void ensureDefaultPresetExists(EqlModuleProcessor& processor)
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml != nullptr && findPresetElement(*presetsXml, "default") != nullptr)
        return;

    if (processor.saveFilterPreset("default"))
        processor.setDefaultFilterPreset("default");
}
} // namespace eql_presets

using namespace eql_state;

juce::String EqlModuleProcessor::getDefaultFilterPresetName() const
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return {};

    const auto storedDefault = getStoredDefaultPresetName(*presetsXml);

    if (storedDefault.isNotEmpty() && findPresetElement(*presetsXml, storedDefault) != nullptr)
        return storedDefault;

    const auto defaultSelected = parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim();

    if (defaultSelected.isNotEmpty() && findPresetElement(*presetsXml, defaultSelected) != nullptr)
        return defaultSelected;

    if (findPresetElement(*presetsXml, "default") != nullptr)
        return "default";

    for (auto* child : presetsXml->getChildIterator())
    {
        if (child->hasTagName(presetTag))
        {
            const auto presetName = child->getStringAttribute("name").trim();

            if (presetName.isNotEmpty())
                return presetName;
        }
    }

    return {};
}

juce::String EqlModuleProcessor::getLastFilterPresetName() const
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return {};

    const auto lastSelected = parameters.state.getProperty(filterPresetLastSelectedStateKey).toString().trim();

    if (lastSelected.isNotEmpty() && findPresetElement(*presetsXml, lastSelected) != nullptr)
        return lastSelected;

    return getDefaultFilterPresetName();
}

juce::StringArray EqlModuleProcessor::getFilterPresetNames() const
{
    juce::StringArray presetNames;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr)
        return presetNames;

    for (auto* child : presetsXml->getChildIterator())
    {
        if (! child->hasTagName(presetTag))
            continue;

        const auto presetName = child->getStringAttribute("name").trim();

        if (presetName.isNotEmpty())
            presetNames.addIfNotAlreadyThere(presetName);
    }

    presetNames.sort(true);
    return presetNames;
}

bool EqlModuleProcessor::saveFilterPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        presetsXml = createEmptyFilterPresetsXml();

    const auto wasDefaultPreset = getDefaultFilterPresetName().equalsIgnoreCase(trimmedName);

    if (auto* existingPreset = findPresetElement(*presetsXml, trimmedName))
        presetsXml->removeChildElement(existingPreset, true);

    auto* presetElement = presetsXml->createNewChildElement(presetTag);
    presetElement->setAttribute("name", trimmedName);

    if (wasDefaultPreset)
        presetElement->setAttribute(defaultPresetAttribute, true);

    if (auto stateXml = createSerializableStateXml(*this))
        presetElement->addChildElement(stateXml.release());
    else
        return false;

    if (! writeFilterPresetsXml(*presetsXml))
        return false;

    parameters.state.setProperty(filterPresetLastSelectedStateKey, trimmedName, nullptr);
    return true;
}

bool EqlModuleProcessor::renameFilterPreset(const juce::String& sourcePresetName, const juce::String& newPresetName)
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return false;

    auto* sourcePresetElement = findPresetElement(*presetsXml, sourcePresetName.trim());

    if (sourcePresetElement == nullptr)
        return false;

    const auto renamedDefault = sourcePresetElement->getBoolAttribute(defaultPresetAttribute)
        || getDefaultFilterPresetName().equalsIgnoreCase(sourcePresetName.trim());
    sourcePresetElement->setAttribute("name", newPresetName.trim());

    if (! writeFilterPresetsXml(*presetsXml))
        return false;

    parameters.state.setProperty(filterPresetLastSelectedStateKey, newPresetName.trim(), nullptr);

    if (renamedDefault)
        parameters.state.setProperty(filterPresetDefaultSelectedStateKey, newPresetName.trim(), nullptr);

    return true;
}

bool EqlModuleProcessor::setDefaultFilterPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! selectStoredDefaultPreset(*presetsXml, trimmedName))
        return false;

    if (! writeFilterPresetsXml(*presetsXml))
        return false;

    parameters.state.setProperty(filterPresetDefaultSelectedStateKey, trimmedName, nullptr);
    return true;
}

bool EqlModuleProcessor::loadInitialFilterPreset() noexcept
{
    const auto defaultFilterPresetName = getDefaultFilterPresetName();

    if (defaultFilterPresetName.isNotEmpty())
    {
        if (loadFilterPreset(defaultFilterPresetName))
        {
            if (parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim().isEmpty())
                parameters.state.setProperty(filterPresetDefaultSelectedStateKey, defaultFilterPresetName, nullptr);

            return true;
        }
    }

    if (const auto lastFilterPresetName = getLastFilterPresetName(); lastFilterPresetName.isNotEmpty())
        return loadFilterPreset(lastFilterPresetName);

    return false;
}

bool EqlModuleProcessor::loadFilterPreset(const juce::String& presetName) noexcept
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr)
        return false;

    auto* presetElement = findPresetElement(*presetsXml, trimmedName);

    if (presetElement == nullptr)
        return false;

    auto* stateElement = presetElement->getChildByName(parameters.state.getType().toString());

    if (stateElement == nullptr)
        return false;

    const auto previousDefaultPresetName = parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim();
    auto normalizedStateElement = std::make_unique<juce::XmlElement>(*stateElement);
    normalizeRestoredStateElement(*normalizedStateElement, parameters);

    const auto wasPrepared = prepared.exchange(false, std::memory_order_acq_rel);
    const juce::ScopedLock lock(filterProcessLock);
    const auto restoredFilterCount = clampActiveFilterCount(normalizedStateElement->getIntAttribute(activeFilterCountStateKey, 0));
    auto restoredState = juce::ValueTree::fromXml(*createCompleteRestoredStateElement(*normalizedStateElement, parameters));
    parameters.replaceState(restoredState);
    setActiveFilterCount(restoredFilterCount);
    resetFilters();

    if (wasPrepared && currentSampleRate > 0.0)
    {
        updateFilters();
        eqlFiltersDirty.store(false, std::memory_order_release);
        prepared.store(true, std::memory_order_release);
    }
    else
    {
        markEqlFiltersDirty();
    }

    parameters.state.setProperty(filterPresetLastSelectedStateKey, trimmedName, nullptr);

    if (previousDefaultPresetName.isNotEmpty() && findPresetElement(*presetsXml, previousDefaultPresetName) != nullptr)
        parameters.state.setProperty(filterPresetDefaultSelectedStateKey, previousDefaultPresetName, nullptr);

    return true;
}

bool EqlModuleProcessor::deleteFilterPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty() || getFilterPresetNames().size() <= 1)
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr)
        return false;

    auto* presetElement = findPresetElement(*presetsXml, trimmedName);

    if (presetElement == nullptr)
        return false;

    const auto removedDefault = presetElement->getBoolAttribute(defaultPresetAttribute)
        || getDefaultFilterPresetName().equalsIgnoreCase(trimmedName);
    presetsXml->removeChildElement(presetElement, true);

    if (removedDefault)
    {
        auto replacementDefault = juce::String {};

        if (findPresetElement(*presetsXml, "default") != nullptr)
            replacementDefault = "default";
        else
            for (auto* child : presetsXml->getChildIterator())
                if (child->hasTagName(presetTag))
                {
                    replacementDefault = child->getStringAttribute("name").trim();
                    break;
                }

        if (replacementDefault.isNotEmpty())
        {
            selectStoredDefaultPreset(*presetsXml, replacementDefault);
            parameters.state.setProperty(filterPresetDefaultSelectedStateKey, replacementDefault, nullptr);
        }
    }

    return writeFilterPresetsXml(*presetsXml);
}
