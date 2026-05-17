#include "ProcessorSupport.h"

namespace
{
void preserveEditorWindowAndVisualizerState(juce::XmlElement& targetStateElement,
                                            const juce::ValueTree& sourceState)
{
    const auto copyStateProperty = [&targetStateElement, &sourceState] (const juce::Identifier& propertyId)
    {
        if (sourceState.hasProperty(propertyId))
            targetStateElement.setAttribute(propertyId.toString(), sourceState.getProperty(propertyId).toString());
        else
            targetStateElement.removeAttribute(propertyId.toString());
    };

    copyStateProperty(EqeAudioProcessor::editorWidthStateKey);
    copyStateProperty(EqeAudioProcessor::editorHeightStateKey);
    copyStateProperty(EqeAudioProcessor::activeModuleStateKey);
    copyStateProperty("editor_visualizer_expanded");
    copyStateProperty("editor_visualizer_visible");
    copyStateProperty("editor_visualizer_range_low");
    copyStateProperty("editor_visualizer_range_high");
    copyStateProperty("editor_visualizer_cursor_enabled");
    copyStateProperty("editor_visualizer_show_stereo");
    copyStateProperty("editor_visualizer_show_left");
    copyStateProperty("editor_visualizer_show_right");
    copyStateProperty("editor_visualizer_show_mid");
    copyStateProperty("editor_visualizer_show_side");
    copyStateProperty("editor_last_collapsed_width");
    copyStateProperty("editor_last_visualizer_width");
}

juce::String findFirstPresetName(juce::XmlElement& presetsXml)
{
    for (auto* child : presetsXml.getChildIterator())
    {
        if (! child->hasTagName(presetTag))
            continue;

        const auto presetName = child->getStringAttribute("name").trim();

        if (presetName.isNotEmpty())
            return presetName;
    }

    return {};
}

bool renamePresetElement(juce::XmlElement& presetsXml,
                         const juce::String& oldPresetName,
                         const juce::String& newPresetName,
                         juce::ValueTree& state,
                         const char* lastSelectedStateKey,
                         const char* defaultSelectedStateKey = nullptr)
{
    const auto trimmedOldName = oldPresetName.trim();
    const auto trimmedNewName = newPresetName.trim();

    if (trimmedOldName.isEmpty() || trimmedNewName.isEmpty())
        return false;

    auto* oldPresetElement = findPresetElement(presetsXml, trimmedOldName);

    if (oldPresetElement == nullptr)
        return false;

    if (auto* existingNewPresetElement = findPresetElement(presetsXml, trimmedNewName);
        existingNewPresetElement != nullptr && existingNewPresetElement != oldPresetElement)
    {
        presetsXml.removeChildElement(existingNewPresetElement, true);
    }

    oldPresetElement->setAttribute("name", trimmedNewName);
    const auto currentLastSelected = state.getProperty(lastSelectedStateKey).toString().trim();

    if (currentLastSelected.equalsIgnoreCase(trimmedOldName))
        state.setProperty(lastSelectedStateKey, trimmedNewName, nullptr);

    if (defaultSelectedStateKey != nullptr)
    {
        const auto currentDefaultSelected = state.getProperty(defaultSelectedStateKey).toString().trim();

        if (currentDefaultSelected.equalsIgnoreCase(trimmedOldName))
            state.setProperty(defaultSelectedStateKey, trimmedNewName, nullptr);
    }

    return true;
}

juce::XmlElement* findStateParameterElement(juce::XmlElement& stateElement, const juce::String& parameterId)
{
    for (auto* child : stateElement.getChildIterator())
    {
        if (child->hasTagName("PARAM") && child->getStringAttribute("id").equalsIgnoreCase(parameterId))
            return child;
    }

    return nullptr;
}

float getDefaultParameterValue(juce::RangedAudioParameter& parameter) noexcept
{
    return parameter.convertFrom0to1(parameter.getDefaultValue());
}

float sanitizeParameterValue(juce::RangedAudioParameter& parameter, const juce::String& storedText) noexcept
{
    auto value = storedText.isNotEmpty() ? static_cast<float>(storedText.getDoubleValue())
                                        : getDefaultParameterValue(parameter);

    if (dynamic_cast<juce::AudioParameterChoice*>(&parameter) != nullptr
        && storedText.containsChar('.')
        && value >= 0.0f
        && value <= 1.0f)
    {
        value = parameter.convertFrom0to1(value);
    }

    const auto& range = parameter.getNormalisableRange();
    return range.snapToLegalValue(juce::jlimit(range.start, range.end, value));
}

void ensureStateParameterElement(juce::XmlElement& stateElement,
                                 juce::AudioProcessorValueTreeState& parameters,
                                 const juce::String& parameterId)
{
    auto* parameter = parameters.getParameter(parameterId);

    if (parameter == nullptr)
        return;

    auto* parameterElement = findStateParameterElement(stateElement, parameterId);

    if (parameterElement == nullptr)
    {
        parameterElement = stateElement.createNewChildElement("PARAM");
        parameterElement->setAttribute("id", parameterId);
    }

    parameterElement->setAttribute("value",
                                   sanitizeParameterValue(*parameter,
                                                          parameterElement->getStringAttribute("value").trim()));

    for (int childIndex = stateElement.getNumChildElements() - 1; childIndex >= 0; --childIndex)
    {
        auto* child = stateElement.getChildElement(childIndex);

        if (child != parameterElement
            && child != nullptr
            && child->hasTagName("PARAM")
            && child->getStringAttribute("id").equalsIgnoreCase(parameterId))
        {
            stateElement.removeChildElement(child, true);
        }
    }
}

}

void EqeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto editorWidth = lastEditorWidth.load(std::memory_order_relaxed);
    const auto editorHeight = lastEditorHeight.load(std::memory_order_relaxed);

    if (editorWidth > 0 && editorHeight > 0)
    {
        parameters.state.setProperty(editorWidthStateKey, editorWidth, nullptr);
        parameters.state.setProperty(editorHeightStateKey, editorHeight, nullptr);
    }

    auto state = parameters.copyState();
    state.setProperty(activeBellCountStateKey, getActiveBellCount(), nullptr);

    if (isEqeModuleLoaded())
        state.setProperty(activeModuleStateKey, eqeModuleId, nullptr);
    else
        state.removeProperty(activeModuleStateKey, nullptr);

    if (auto stateXml = state.createXml())
        copyXmlToBinary(*stateXml, destData);
}

void EqeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto stateXml = getXmlFromBinary(data, sizeInBytes))
    {
        if (stateXml->hasTagName(parameters.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml(*stateXml);
            const auto restoredBellCount = static_cast<int>(restoredState.getProperty(activeBellCountStateKey, 0));
            parameters.replaceState(restoredState);
            setEqeModuleLoaded(parameters.state.getProperty(activeModuleStateKey).toString().equalsIgnoreCase(eqeModuleId));
            setLastEditorSize(static_cast<int>(parameters.state.getProperty(editorWidthStateKey, 0)),
                              static_cast<int>(parameters.state.getProperty(editorHeightStateKey, 0)));
            setActiveBellCount(restoredBellCount);
        }
    }
}

juce::AudioProcessorValueTreeState& EqeAudioProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& EqeAudioProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

bool EqeAudioProcessor::isEqeModuleLoaded() const noexcept
{
    return eqeModuleLoaded.load(std::memory_order_relaxed);
}

void EqeAudioProcessor::setEqeModuleLoaded(const bool shouldBeLoaded)
{
    eqeModuleLoaded.store(shouldBeLoaded, std::memory_order_relaxed);

    if (shouldBeLoaded)
        parameters.state.setProperty(activeModuleStateKey, eqeModuleId, nullptr);
    else
        parameters.state.removeProperty(activeModuleStateKey, nullptr);
}

juce::Point<int> EqeAudioProcessor::getLastEditorSize() const noexcept
{
    return { lastEditorWidth.load(std::memory_order_relaxed),
             lastEditorHeight.load(std::memory_order_relaxed) };
}

void EqeAudioProcessor::setLastEditorSize(const int width, const int height) noexcept
{
    lastEditorWidth.store(juce::jmax(0, width), std::memory_order_relaxed);
    lastEditorHeight.store(juce::jmax(0, height), std::memory_order_relaxed);
}

int EqeAudioProcessor::getActiveBellCount() const noexcept
{
    return activeBellCount.load(std::memory_order_relaxed);
}

juce::String EqeAudioProcessor::getDefaultFilterPresetName() const
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return {};

    const auto defaultSelected = parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim();

    if (defaultSelected.isNotEmpty() && findPresetElement(*presetsXml, defaultSelected) != nullptr)
        return defaultSelected;

    if (findPresetElement(*presetsXml, "default") != nullptr)
        return "default";

    return findFirstPresetName(*presetsXml);
}

juce::String EqeAudioProcessor::getLastFilterPresetName() const
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return {};

    const auto lastSelected = parameters.state.getProperty(filterPresetLastSelectedStateKey).toString().trim();

    if (lastSelected.isNotEmpty() && findPresetElement(*presetsXml, lastSelected) != nullptr)
        return lastSelected;

    const auto defaultSelected = parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim();

    if (defaultSelected.isNotEmpty() && findPresetElement(*presetsXml, defaultSelected) != nullptr)
        return defaultSelected;

    if (findPresetElement(*presetsXml, "default") != nullptr)
        return "default";

    return findFirstPresetName(*presetsXml);
}

juce::StringArray EqeAudioProcessor::getFilterPresetNames() const
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

bool EqeAudioProcessor::saveFilterPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        presetsXml = createEmptyFilterPresetsXml();

    if (auto* existingPreset = findPresetElement(*presetsXml, trimmedName))
        presetsXml->removeChildElement(existingPreset, true);

    auto* presetElement = presetsXml->createNewChildElement(presetTag);
    presetElement->setAttribute("name", trimmedName);

    if (auto stateXml = createSerializableStateXml(*this))
        presetElement->addChildElement(stateXml.release());
    else
        return false;

    if (! writeFilterPresetsXml(*presetsXml))
        return false;

    parameters.state.setProperty(filterPresetLastSelectedStateKey, trimmedName, nullptr);
    return true;
}

bool EqeAudioProcessor::renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName)
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return false;

    if (! renamePresetElement(*presetsXml,
                              oldPresetName,
                              newPresetName,
                              parameters.state,
                              filterPresetLastSelectedStateKey,
                              filterPresetDefaultSelectedStateKey))
    {
        return false;
    }

    return writeFilterPresetsXml(*presetsXml);
}

bool EqeAudioProcessor::setDefaultFilterPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return false;

    if (findPresetElement(*presetsXml, trimmedName) == nullptr)
        return false;

    if (! writeFilterPresetsXml(*presetsXml))
        return false;

    parameters.state.setProperty(filterPresetDefaultSelectedStateKey, trimmedName, nullptr);
    return true;
}

bool EqeAudioProcessor::loadFilterPreset(const juce::String& presetName) noexcept
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

    const auto preservedDefaultPresetName = parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim();

    auto normalizedStateElement = std::make_unique<juce::XmlElement>(*stateElement);
    rewriteFilterParameterIds(*normalizedStateElement, false);

    normalizedStateElement->removeAttribute(editorWidthStateKey);
    normalizedStateElement->removeAttribute(editorHeightStateKey);
    normalizedStateElement->removeAttribute("editor_visualizer_expanded");
    normalizedStateElement->removeAttribute("editor_visualizer_visible");
    normalizedStateElement->removeAttribute("editor_visualizer_range_low");
    normalizedStateElement->removeAttribute("editor_visualizer_range_high");
    normalizedStateElement->removeAttribute("editor_visualizer_cursor_enabled");
    normalizedStateElement->removeAttribute("editor_visualizer_show_stereo");
    normalizedStateElement->removeAttribute("editor_visualizer_show_left");
    normalizedStateElement->removeAttribute("editor_visualizer_show_right");
    normalizedStateElement->removeAttribute("editor_visualizer_show_mid");
    normalizedStateElement->removeAttribute("editor_visualizer_show_side");
    normalizedStateElement->removeAttribute("editor_last_collapsed_width");
    normalizedStateElement->removeAttribute("editor_last_visualizer_width");

    const auto restoredBellCount = clampActiveBellCount(normalizedStateElement->getIntAttribute(activeBellCountStateKey, 0));
    normalizedStateElement->setAttribute(activeBellCountStateKey, restoredBellCount);

    ensureStateParameterElement(*normalizedStateElement, parameters, paramGlobalGainLId);
    ensureStateParameterElement(*normalizedStateElement, parameters, paramGlobalGainRId);
    ensureStateParameterElement(*normalizedStateElement, parameters, paramOutGainId);
    ensureStateParameterElement(*normalizedStateElement, parameters, paramGlobalBypassId);
    ensureStateParameterElement(*normalizedStateElement, parameters, paramGlobalBypassOutGainOnlyId);

    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        ensureStateParameterElement(*normalizedStateElement, parameters, getFilterTypeParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getFilterLrmsParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellSlopeParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellFrequencyParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellBandwidthParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellGainParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellBypassParamId(bellIndex));
    }

    preserveEditorWindowAndVisualizerState(*normalizedStateElement, parameters.state);

    juce::MemoryBlock stateData;
    copyXmlToBinary(*normalizedStateElement, stateData);
    setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    if (preservedDefaultPresetName.isNotEmpty())
        parameters.state.setProperty(filterPresetDefaultSelectedStateKey, preservedDefaultPresetName, nullptr);
    else
        parameters.state.removeProperty(filterPresetDefaultSelectedStateKey, nullptr);

    parameters.state.setProperty(filterPresetLastSelectedStateKey, trimmedName, nullptr);
    return true;
}

bool EqeAudioProcessor::deleteFilterPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    if (getFilterPresetNames().size() <= 1)
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr)
        return false;

    if (auto* presetElement = findPresetElement(*presetsXml, trimmedName))
    {
        presetsXml->removeChildElement(presetElement, true);

        const auto replacementPresetName = findFirstPresetName(*presetsXml);
        const auto currentLastSelected = parameters.state.getProperty(filterPresetLastSelectedStateKey).toString().trim();
        const auto currentDefaultSelected = parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim();

        if (currentLastSelected.equalsIgnoreCase(trimmedName))
            parameters.state.setProperty(filterPresetLastSelectedStateKey, replacementPresetName, nullptr);

        if (currentDefaultSelected.equalsIgnoreCase(trimmedName))
            parameters.state.setProperty(filterPresetDefaultSelectedStateKey, replacementPresetName, nullptr);

        return writeFilterPresetsXml(*presetsXml);
    }

    return false;
}
