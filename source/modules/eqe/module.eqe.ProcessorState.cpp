#include "module.eqe.ProcessorSupport.h"

#include <array>
#include <memory>
#include <vector>
#include <cmath>

namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* label;
};

inline constexpr auto eqeFilterOrder = std::to_array<ParameterOrderEntry>({
    { "type", "TYPE" },
    { "place", "PLACE" },
    { "order", "ORDER" },
    { "freq", "FREQ" },
    { "bw", "BW" },
    { "gain", "GAIN" },
    { "bypass", "BYPASS" },
});

bool supportsPersistentEqePresets() noexcept
{
    return true;
}

void ensureDefaultPresetFilesExist(EqeModuleProcessor& processor)
{
    if (loadFilterPresetsXml() == nullptr)
    {
        if (processor.saveFilterPreset("default"))
            processor.setDefaultFilterPreset("default");
    }
}

void setParameterValue(juce::AudioProcessorValueTreeState& state,
                       const juce::String& parameterId,
                       const float value)
{
    auto* parameter = state.getParameter(parameterId);

    if (parameter == nullptr)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    parameter->endChangeGesture();
}

float readParameterValue(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId)
{
    if (auto* parameter = state.getParameter(parameterId))
        return parameter->convertFrom0to1(parameter->getValue());

    return 0.0f;
}

void ensureStateParameterElement(juce::XmlElement& stateElement,
                                 juce::AudioProcessorValueTreeState& parameters,
                                 const juce::String& parameterId)
{
    auto* parameter = parameters.getParameter(parameterId);

    if (parameter == nullptr)
        return;

    auto* parameterElement = [&stateElement, &parameterId]() -> juce::XmlElement*
    {
        for (auto* child : stateElement.getChildIterator())
        {
            if (child->hasTagName("PARAM") && child->getStringAttribute("id").equalsIgnoreCase(parameterId))
                return child;
        }

        return nullptr;
    }();

    if (parameterElement == nullptr)
    {
        parameterElement = stateElement.createNewChildElement("PARAM");
        parameterElement->setAttribute("id", parameterId);
    }

    const auto defaultPlainValue = parameter->convertFrom0to1(parameter->getDefaultValue());
    const auto storedText = parameterElement->getStringAttribute("value").trim();
    auto plainValue = storedText.isNotEmpty() ? static_cast<float>(storedText.getDoubleValue())
                                              : defaultPlainValue;
    const auto& range = parameter->getNormalisableRange();
    plainValue = range.snapToLegalValue(juce::jlimit(range.start, range.end, plainValue));
    parameterElement->setAttribute("value", plainValue);
}

void addDefaultStateParameterElement(juce::XmlElement& stateElement,
                                     juce::AudioProcessorValueTreeState& parameters,
                                     const juce::String& parameterId)
{
    auto* parameter = parameters.getParameter(parameterId);

    if (parameter == nullptr)
        return;

    auto* parameterElement = stateElement.createNewChildElement("PARAM");
    parameterElement->setAttribute("id", parameterId);
    parameterElement->setAttribute("value", parameter->convertFrom0to1(parameter->getDefaultValue()));
}

juce::XmlElement* findStateParameterElement(juce::XmlElement& stateElement,
                                            const juce::String& parameterId)
{
    for (auto* child : stateElement.getChildIterator())
    {
        if (child->hasTagName("PARAM") && child->getStringAttribute("id").equalsIgnoreCase(parameterId))
            return child;
    }

    return nullptr;
}

void removeStateParameterElement(juce::XmlElement& stateElement,
                                 const juce::String& parameterId)
{
    if (auto* parameterElement = findStateParameterElement(stateElement, parameterId))
        stateElement.removeChildElement(parameterElement, true);
}

float readRestoredParameterValue(juce::XmlElement& stateElement,
                                 juce::AudioProcessorValueTreeState& parameters,
                                 const juce::String& parameterId)
{
    if (auto* parameterElement = findStateParameterElement(stateElement, parameterId))
        return static_cast<float>(parameterElement->getDoubleAttribute("value", 0.0));

    if (auto* parameter = parameters.getParameter(parameterId))
        return parameter->convertFrom0to1(parameter->getDefaultValue());

    return 0.0f;
}

EqeModuleProcessor::FilterType getRestoredFilterType(juce::XmlElement& stateElement,
                                                     juce::AudioProcessorValueTreeState& parameters,
                                                     const int filterIndex)
{
    const auto typeChoice = static_cast<int>(std::lround(readRestoredParameterValue(stateElement,
                                                                                    parameters,
                                                                                    EqeModuleProcessor::getFilterTypeParamId(filterIndex))));
    return EqeModuleProcessor::filterTypeFromChoiceIndex(typeChoice);
}

void copyParameterElementValue(juce::XmlElement& targetStateElement,
                               juce::AudioProcessorValueTreeState& parameters,
                               const juce::XmlElement& sourceParameterElement)
{
    if (! sourceParameterElement.hasTagName("PARAM"))
        return;

    const auto parameterId = sourceParameterElement.getStringAttribute("id").trim();
    auto* parameter = parameters.getParameter(parameterId);

    if (parameter == nullptr)
        return;

    auto* targetParameterElement = findStateParameterElement(targetStateElement, parameterId);

    if (targetParameterElement == nullptr)
    {
        addDefaultStateParameterElement(targetStateElement, parameters, parameterId);
        targetParameterElement = findStateParameterElement(targetStateElement, parameterId);
    }

    if (targetParameterElement == nullptr)
        return;

    const auto& range = parameter->getNormalisableRange();
    const auto value = range.snapToLegalValue(juce::jlimit(range.start,
                                                           range.end,
                                                           static_cast<float>(sourceParameterElement.getDoubleAttribute("value", 0.0))));
    targetParameterElement->setAttribute("value", value);
}

std::unique_ptr<juce::XmlElement> createCompleteRestoredStateElement(const juce::XmlElement& sparseStateElement,
                                                                     juce::AudioProcessorValueTreeState& parameters)
{
    auto completeStateElement = std::make_unique<juce::XmlElement>(sparseStateElement);
    completeStateElement->deleteAllChildElements();

    for (int filterIndex = 0; filterIndex < EqeModuleProcessor::maxFilterCount; ++filterIndex)
    {
        addDefaultStateParameterElement(*completeStateElement, parameters, EqeModuleProcessor::getFilterTypeParamId(filterIndex));
        addDefaultStateParameterElement(*completeStateElement, parameters, EqeModuleProcessor::getFilterLrmsParamId(filterIndex));
        addDefaultStateParameterElement(*completeStateElement, parameters, EqeModuleProcessor::getFilterSlopeParamId(filterIndex));
        addDefaultStateParameterElement(*completeStateElement, parameters, EqeModuleProcessor::getFilterFrequencyParamId(filterIndex));
        addDefaultStateParameterElement(*completeStateElement, parameters, EqeModuleProcessor::getFilterBandwidthParamId(filterIndex));
        addDefaultStateParameterElement(*completeStateElement, parameters, EqeModuleProcessor::getFilterGainParamId(filterIndex));
        addDefaultStateParameterElement(*completeStateElement, parameters, EqeModuleProcessor::getFilterBypassParamId(filterIndex));
    }

    for (auto* child : sparseStateElement.getChildIterator())
        if (child != nullptr)
            copyParameterElementValue(*completeStateElement, parameters, *child);

    return completeStateElement;
}

void removeUnknownStateParameterElements(juce::XmlElement& stateElement,
                                         juce::AudioProcessorValueTreeState& parameters)
{
    for (int childIndex = stateElement.getNumChildElements(); --childIndex >= 0;)
    {
        auto* child = stateElement.getChildElement(childIndex);

        if (child == nullptr)
            continue;

        if (child->hasTagName("PARAM"))
        {
            const auto parameterId = child->getStringAttribute("id").trim();

            if (parameterId.isEmpty() || parameters.getParameter(parameterId) == nullptr)
                stateElement.removeChildElement(child, true);

            continue;
        }

        removeUnknownStateParameterElements(*child, parameters);
    }
}

bool containsUnsupportedEqeStateData(const juce::XmlElement& stateElement)
{
    for (int attributeIndex = 0; attributeIndex < stateElement.getNumAttributes(); ++attributeIndex)
    {
        const auto attributeName = stateElement.getAttributeName(attributeIndex);

        if (attributeName.startsWithIgnoreCase("editor_")
            || attributeName == EqeModuleProcessor::filterPresetLastSelectedStateKey
            || attributeName == EqeModuleProcessor::filterPresetDefaultSelectedStateKey)
            return true;
    }

    for (auto* child : stateElement.getChildIterator())
    {
        if (child == nullptr)
            continue;

        if (child->hasTagName("PARAM"))
        {
            const auto parameterId = child->getStringAttribute("id").trim();

            if (parameterId == "out_gain"
                || parameterId == "global_bypass"
                || parameterId == "global_bypass_out_gain_only"
                || parameterId.startsWithIgnoreCase("global_gain")
                || parameterId.startsWithIgnoreCase("eqe_")
                || parameterId.endsWithIgnoreCase("_ttss"))
                return true;
        }

        if (containsUnsupportedEqeStateData(*child))
            return true;
    }

    return false;
}

void normalizeRestoredStateElement(juce::XmlElement& stateElement,
                                   juce::AudioProcessorValueTreeState& parameters)
{
    rewriteFilterParameterIds(stateElement, false);
    removeUnknownStateParameterElements(stateElement, parameters);

    const auto restoredFilterCount = clampActiveFilterCount(stateElement.getIntAttribute(EqeModuleProcessor::activeFilterCountStateKey, 0));
    stateElement.setAttribute(EqeModuleProcessor::activeFilterCountStateKey, restoredFilterCount);

    for (int filterIndex = 0; filterIndex < EqeModuleProcessor::maxFilterCount; ++filterIndex)
    {
        const auto typeId = EqeModuleProcessor::getFilterTypeParamId(filterIndex);
        const auto placeId = EqeModuleProcessor::getFilterLrmsParamId(filterIndex);
        const auto slopeId = EqeModuleProcessor::getFilterSlopeParamId(filterIndex);
        const auto frequencyId = EqeModuleProcessor::getFilterFrequencyParamId(filterIndex);
        const auto bandwidthId = EqeModuleProcessor::getFilterBandwidthParamId(filterIndex);
        const auto gainId = EqeModuleProcessor::getFilterGainParamId(filterIndex);
        const auto bypassId = EqeModuleProcessor::getFilterBypassParamId(filterIndex);

        if (filterIndex >= restoredFilterCount)
        {
            removeStateParameterElement(stateElement, typeId);
            removeStateParameterElement(stateElement, placeId);
            removeStateParameterElement(stateElement, slopeId);
            removeStateParameterElement(stateElement, frequencyId);
            removeStateParameterElement(stateElement, bandwidthId);
            removeStateParameterElement(stateElement, gainId);
            removeStateParameterElement(stateElement, bypassId);
            continue;
        }

        ensureStateParameterElement(stateElement, parameters, typeId);
        ensureStateParameterElement(stateElement, parameters, placeId);
        ensureStateParameterElement(stateElement, parameters, gainId);
        ensureStateParameterElement(stateElement, parameters, bypassId);

        if (getRestoredFilterType(stateElement, parameters, filterIndex) == EqeModuleProcessor::FilterType::volume)
        {
            removeStateParameterElement(stateElement, slopeId);
            removeStateParameterElement(stateElement, frequencyId);
            removeStateParameterElement(stateElement, bandwidthId);
            continue;
        }

        ensureStateParameterElement(stateElement, parameters, slopeId);
        ensureStateParameterElement(stateElement, parameters, frequencyId);
        ensureStateParameterElement(stateElement, parameters, bandwidthId);
    }
}
}

EqeModuleProcessor::EqeModuleProcessor(juce::AudioProcessor& ownerProcessor)
    : parameters(internalParameterHost, nullptr, "eqe_state", createParameterLayout())
{
    juce::ignoreUnused(ownerProcessor);
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        filterTypeParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterTypeParamId(filterIndex));
        filterLrmsParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterLrmsParamId(filterIndex));
        filterFrequencyParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterFrequencyParamId(filterIndex));
        filterBandwidthParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterBandwidthParamId(filterIndex));
        filterSlopeChoiceParams[static_cast<size_t>(filterIndex)] = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter(getFilterSlopeParamId(filterIndex)));
        filterGainParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterGainParamId(filterIndex));
        filterBypassParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterBypassParamId(filterIndex));
    }

    registerParameterListeners();

    if (supportsPersistentEqePresets())
    {
        ensureDefaultPresetFilesExist(*this);
    }
}

EqeModuleProcessor::~EqeModuleProcessor()
{
    unregisterParameterListeners();
}

void EqeModuleProcessor::appendEqeParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& parameterLayout)
{
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        for (const auto& entry : eqeFilterOrder)
        {
            const auto key = juce::String(entry.key);
            const auto name = "FILTER " + juce::String(filterIndex + 1) + " - " + juce::String(entry.label);

            if (key == "type")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterTypeParamId(filterIndex), 1 },
                    name,
                    filterTypeChoices,
                    EqeModuleProcessor::choiceIndexFromFilterType(EqeModuleProcessor::FilterType::bell),
                    juce::AudioParameterChoiceAttributes()));
                continue;
            }

            if (key == "place")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterLrmsParamId(filterIndex), 1 },
                    name,
                    filterLrmsChoices,
                    0,
                    juce::AudioParameterChoiceAttributes()));
                continue;
            }

            if (key == "order")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterSlopeParamId(filterIndex), 1 },
                    name,
                    EqeModuleProcessor::getBellSlopeChoices(),
                    EqeModuleProcessor::getBellSlopeChoiceIndexForValue(EqeModuleProcessor::fixedSlopeDbPerOct),
                    juce::AudioParameterChoiceAttributes()));
                continue;
            }

            if (key == "freq")
            {
                auto filterFrequencyRange = juce::NormalisableRange<float> { minimumVisibleFilterFrequency, maximumVisibleFilterFrequency, 0.01f };
                filterFrequencyRange.setSkewForCentre(defaultTiltFrequency);

                parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID { getFilterFrequencyParamId(filterIndex), 1 },
                    name,
                    filterFrequencyRange,
                    defaultTiltFrequency,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                        [] (float value, int)
                        {
                            return formatFrequencyValue(value);
                        })));
                continue;
            }

            if (key == "bw")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID { getFilterBandwidthParamId(filterIndex), 1 },
                    name,
                    juce::NormalisableRange<float> { minimumBellBandwidth, maximumBellBandwidth, 0.001f },
                    1.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                        [] (float value, int)
                        {
                            return formatBandwidthValue(value);
                        })));
                continue;
            }

            if (key == "gain")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID { getFilterGainParamId(filterIndex), 1 },
                    name,
                    juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
                    0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                        [] (float value, int)
                        {
                            return formatDecibelValue(value);
                        })));
                continue;
            }

            if (key == "bypass")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                    juce::ParameterID { getFilterBypassParamId(filterIndex), 1 },
                    name,
                    false,
                    juce::AudioParameterBoolAttributes()));
            }
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout EqeModuleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    appendEqeParameters(parameterLayout);
    return { parameterLayout.begin(), parameterLayout.end() };
}

void EqeModuleProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == activeFilterCountStateKey)
        return;

    markEqeFiltersDirty();
}

void EqeModuleProcessor::registerParameterListeners()
{
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        parameters.addParameterListener(getFilterTypeParamId(filterIndex), this);
        parameters.addParameterListener(getFilterLrmsParamId(filterIndex), this);
        parameters.addParameterListener(getFilterFrequencyParamId(filterIndex), this);
        parameters.addParameterListener(getFilterBandwidthParamId(filterIndex), this);
        parameters.addParameterListener(getFilterSlopeParamId(filterIndex), this);
        parameters.addParameterListener(getFilterGainParamId(filterIndex), this);
        parameters.addParameterListener(getFilterBypassParamId(filterIndex), this);
    }
}

void EqeModuleProcessor::unregisterParameterListeners()
{
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        parameters.removeParameterListener(getFilterTypeParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterLrmsParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterFrequencyParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterBandwidthParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterSlopeParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterGainParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterBypassParamId(filterIndex), this);
    }
}

juce::AudioProcessorValueTreeState& EqeModuleProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& EqeModuleProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

void EqeModuleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = createSerializableStateXml(*this))
        juce::AudioProcessor::copyXmlToBinary(*stateXml, destData);
}

void EqeModuleProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    if (auto stateXml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes))
    {
        if (stateXml->hasTagName(parameters.state.getType()))
        {
            if (containsUnsupportedEqeStateData(*stateXml))
                return;

            const auto wasPrepared = prepared.exchange(false, std::memory_order_acq_rel);
            const juce::ScopedLock lock(filterProcessLock);
            auto restoredState = juce::ValueTree::fromXml(*stateXml);
            if (auto normalizedStateElement = restoredState.createXml())
            {
                normalizeRestoredStateElement(*normalizedStateElement, parameters);
                restoredState = juce::ValueTree::fromXml(*createCompleteRestoredStateElement(*normalizedStateElement, parameters));
            }

            const auto restoredFilterCount = clampActiveFilterCount(static_cast<int>(restoredState.getProperty(activeFilterCountStateKey, 0)));
            parameters.replaceState(restoredState);
            setActiveFilterCount(restoredFilterCount);
            resetFilters();

            if (wasPrepared && currentSampleRate > 0.0)
            {
                updateFilters();
                eqeFiltersDirty.store(false, std::memory_order_release);
                prepared.store(true, std::memory_order_release);
            }
            else
            {
                markEqeFiltersDirty();
            }
        }
    }
}

juce::String EqeModuleProcessor::getDefaultFilterPresetName() const
{
    if (! supportsPersistentEqePresets())
        return {};

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return {};

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

juce::String EqeModuleProcessor::getLastFilterPresetName() const
{
    if (! supportsPersistentEqePresets())
        return {};

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return {};

    const auto lastSelected = parameters.state.getProperty(filterPresetLastSelectedStateKey).toString().trim();

    if (lastSelected.isNotEmpty() && findPresetElement(*presetsXml, lastSelected) != nullptr)
        return lastSelected;

    return getDefaultFilterPresetName();
}

juce::StringArray EqeModuleProcessor::getFilterPresetNames() const
{
    juce::StringArray presetNames;

    if (! supportsPersistentEqePresets())
        return presetNames;

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

bool EqeModuleProcessor::saveFilterPreset(const juce::String& presetName)
{
    if (! supportsPersistentEqePresets())
        return false;

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

bool EqeModuleProcessor::renameFilterPreset(const juce::String& sourcePresetName, const juce::String& newPresetName)
{
    if (! supportsPersistentEqePresets())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return false;

    auto* sourcePresetElement = findPresetElement(*presetsXml, sourcePresetName.trim());

    if (sourcePresetElement == nullptr)
        return false;

    sourcePresetElement->setAttribute("name", newPresetName.trim());

    if (! writeFilterPresetsXml(*presetsXml))
        return false;

    parameters.state.setProperty(filterPresetLastSelectedStateKey, newPresetName.trim(), nullptr);
    return true;
}

bool EqeModuleProcessor::setDefaultFilterPreset(const juce::String& presetName)
{
    if (! supportsPersistentEqePresets())
        return false;

    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || findPresetElement(*presetsXml, trimmedName) == nullptr)
        return false;

    parameters.state.setProperty(filterPresetDefaultSelectedStateKey, trimmedName, nullptr);
    return true;
}

bool EqeModuleProcessor::loadInitialFilterPreset() noexcept
{
    if (! supportsPersistentEqePresets())
        return false;

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

bool EqeModuleProcessor::loadFilterPreset(const juce::String& presetName) noexcept
{
    if (! supportsPersistentEqePresets())
        return false;

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

    if (containsUnsupportedEqeStateData(*stateElement))
        return false;

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
        eqeFiltersDirty.store(false, std::memory_order_release);
        prepared.store(true, std::memory_order_release);
    }
    else
    {
        markEqeFiltersDirty();
    }

    parameters.state.setProperty(filterPresetLastSelectedStateKey, trimmedName, nullptr);
    return true;
}

bool EqeModuleProcessor::deleteFilterPreset(const juce::String& presetName)
{
    if (! supportsPersistentEqePresets())
        return false;

    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty() || getFilterPresetNames().size() <= 1)
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr)
        return false;

    if (auto* presetElement = findPresetElement(*presetsXml, trimmedName))
    {
        presetsXml->removeChildElement(presetElement, true);
        return writeFilterPresetsXml(*presetsXml);
    }

    return false;
}

int EqeModuleProcessor::getActiveFilterCount() const noexcept
{
    return activeFilterCount.load(std::memory_order_relaxed);
}

bool EqeModuleProcessor::addFilter() noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount >= maxFilterCount)
        return false;

    const auto newIndex = currentCount;
    constexpr auto defaultType = FilterType::bell;
    setParameterValue(parameters, getFilterTypeParamId(newIndex), static_cast<float>(choiceIndexFromFilterType(defaultType)));
    setParameterValue(parameters, getFilterLrmsParamId(newIndex), 0.0f);
    setParameterValue(parameters, getFilterFrequencyParamId(newIndex), defaultFilterFrequencyForType(defaultType));
    setParameterValue(parameters, getFilterBandwidthParamId(newIndex), defaultFilterBandwidthForType(defaultType));
    setParameterValue(parameters, getFilterSlopeParamId(newIndex), static_cast<float>(EqeModuleProcessor::getBellSlopeChoiceIndexForValue(defaultFilterSlopeForType(defaultType))));
    setParameterValue(parameters, getFilterGainParamId(newIndex), 0.0f);
    setParameterValue(parameters, getFilterBypassParamId(newIndex), 0.0f);
    setActiveFilterCount(currentCount + 1);
    return true;
}

bool EqeModuleProcessor::removeFilter(const int filterIndex) noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount <= 0 || filterIndex < 0 || filterIndex >= currentCount)
        return false;

    for (int sourceIndex = filterIndex + 1; sourceIndex < currentCount; ++sourceIndex)
    {
        const auto destinationIndex = sourceIndex - 1;
        setParameterValue(parameters, getFilterTypeParamId(destinationIndex), readParameterValue(parameters, getFilterTypeParamId(sourceIndex)));
        setParameterValue(parameters, getFilterLrmsParamId(destinationIndex), readParameterValue(parameters, getFilterLrmsParamId(sourceIndex)));
        setParameterValue(parameters, getFilterFrequencyParamId(destinationIndex), readParameterValue(parameters, getFilterFrequencyParamId(sourceIndex)));
        setParameterValue(parameters, getFilterBandwidthParamId(destinationIndex), readParameterValue(parameters, getFilterBandwidthParamId(sourceIndex)));
        setParameterValue(parameters, getFilterSlopeParamId(destinationIndex), readParameterValue(parameters, getFilterSlopeParamId(sourceIndex)));
        setParameterValue(parameters, getFilterGainParamId(destinationIndex), readParameterValue(parameters, getFilterGainParamId(sourceIndex)));
        setParameterValue(parameters, getFilterBypassParamId(destinationIndex), readParameterValue(parameters, getFilterBypassParamId(sourceIndex)));
    }

    const auto lastIndex = currentCount - 1;
    if (auto* parameter = parameters.getParameter(getFilterTypeParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterLrmsParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterFrequencyParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterBandwidthParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterSlopeParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterGainParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterBypassParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());

    setActiveFilterCount(currentCount - 1);
    resetFilters();
    updateFilters();
    return true;
}

bool EqeModuleProcessor::clearFilters() noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount <= 0)
        return false;

    for (int filterIndex = 0; filterIndex < currentCount; ++filterIndex)
    {
        if (auto* parameter = parameters.getParameter(getFilterTypeParamId(filterIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getFilterLrmsParamId(filterIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getFilterFrequencyParamId(filterIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getFilterBandwidthParamId(filterIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getFilterSlopeParamId(filterIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getFilterGainParamId(filterIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getFilterBypassParamId(filterIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    }

    setActiveFilterCount(0);
    resetFilters();
    updateFilters();
    return true;
}

bool EqeModuleProcessor::moveFilter(const int sourceIndex, const int destinationIndex) noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount <= 1
        || sourceIndex < 0 || sourceIndex >= currentCount
        || destinationIndex < 0 || destinationIndex >= currentCount
        || sourceIndex == destinationIndex)
        return false;

    struct BandSnapshot
    {
        float type = 0.0f;
        float lrms = 0.0f;
        float frequency = 0.0f;
        float bandwidth = 0.0f;
        float slope = 0.0f;
        float gain = 0.0f;
        float bypass = 0.0f;
    };

    std::vector<BandSnapshot> snapshots(static_cast<size_t>(currentCount));

    for (int bandIndex = 0; bandIndex < currentCount; ++bandIndex)
    {
        auto& snapshot = snapshots[static_cast<size_t>(bandIndex)];
        snapshot.type = readParameterValue(parameters, getFilterTypeParamId(bandIndex));
        snapshot.lrms = readParameterValue(parameters, getFilterLrmsParamId(bandIndex));
        snapshot.frequency = readParameterValue(parameters, getFilterFrequencyParamId(bandIndex));
        snapshot.bandwidth = readParameterValue(parameters, getFilterBandwidthParamId(bandIndex));
        snapshot.slope = readParameterValue(parameters, getFilterSlopeParamId(bandIndex));
        snapshot.gain = readParameterValue(parameters, getFilterGainParamId(bandIndex));
        snapshot.bypass = readParameterValue(parameters, getFilterBypassParamId(bandIndex));
    }

    auto movedSnapshot = snapshots[static_cast<size_t>(sourceIndex)];
    snapshots.erase(snapshots.begin() + sourceIndex);
    snapshots.insert(snapshots.begin() + destinationIndex, movedSnapshot);

    for (int bandIndex = 0; bandIndex < currentCount; ++bandIndex)
    {
        const auto& snapshot = snapshots[static_cast<size_t>(bandIndex)];
        setParameterValue(parameters, getFilterTypeParamId(bandIndex), snapshot.type);
        setParameterValue(parameters, getFilterLrmsParamId(bandIndex), snapshot.lrms);
        setParameterValue(parameters, getFilterFrequencyParamId(bandIndex), snapshot.frequency);
        setParameterValue(parameters, getFilterBandwidthParamId(bandIndex), snapshot.bandwidth);
        setParameterValue(parameters, getFilterSlopeParamId(bandIndex), snapshot.slope);
        setParameterValue(parameters, getFilterGainParamId(bandIndex), snapshot.gain);
        setParameterValue(parameters, getFilterBypassParamId(bandIndex), snapshot.bypass);
    }

    resetFilters();
    updateFilters();
    return true;
}

void EqeModuleProcessor::setActiveFilterCount(const int newCount) noexcept
{
    activeFilterCount.store(clampActiveFilterCount(newCount), std::memory_order_relaxed);
    markEqeFiltersDirty();
}

void EqeModuleProcessor::markEqeFiltersDirty() noexcept
{
    eqeFiltersDirty.store(true, std::memory_order_release);
}
