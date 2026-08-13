#include "module.eql.ProcessorSupport.h"

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

inline constexpr auto eqlFilterOrder = std::to_array<ParameterOrderEntry>({
    { "type", "TYPE" },
    { "place", "PLACE" },
    { "order", "ORDER" },
    { "freq", "FREQ" },
    { "bw", "BW" },
    { "gain", "GAIN" },
    { "bypass", "BYPASS" },
});

void ensureDefaultPresetFilesExist(EqlModuleProcessor& processor)
{
    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml != nullptr && findPresetElement(*presetsXml, "default") != nullptr)
        return;

    if (processor.saveFilterPreset("default"))
        processor.setDefaultFilterPreset("default");
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

template <typename Callback>
void forEachFilterParameterId(const int filterIndex, Callback&& callback)
{
    callback(EqlModuleProcessor::getFilterTypeParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterPlaceParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterSlopeParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterFrequencyParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterBandwidthParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterGainParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterBypassParamId(filterIndex));
}

template <typename Callback>
void forEachFilterShapeParameterId(const int filterIndex, Callback&& callback)
{
    callback(EqlModuleProcessor::getFilterSlopeParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterFrequencyParamId(filterIndex));
    callback(EqlModuleProcessor::getFilterBandwidthParamId(filterIndex));
}

struct FilterParameterValues
{
    float type = 0.0f;
    float place = 0.0f;
    float frequency = 0.0f;
    float bandwidth = 0.0f;
    float slope = 0.0f;
    float gain = 0.0f;
    float bypass = 0.0f;
};

FilterParameterValues makeDefaultFilterParameterValues(const EqlModuleProcessor::FilterType type)
{
    return {
        static_cast<float>(EqlModuleProcessor::choiceIndexFromFilterType(type)),
        0.0f,
        defaultFilterFrequencyForType(type),
        defaultFilterBandwidth(),
        static_cast<float>(EqlModuleProcessor::getBellSlopeChoiceIndexForValue(defaultFilterSlope())),
        0.0f,
        0.0f
    };
}

FilterParameterValues readFilterParameterValues(juce::AudioProcessorValueTreeState& state, const int filterIndex)
{
    return {
        readParameterValue(state, EqlModuleProcessor::getFilterTypeParamId(filterIndex)),
        readParameterValue(state, EqlModuleProcessor::getFilterPlaceParamId(filterIndex)),
        readParameterValue(state, EqlModuleProcessor::getFilterFrequencyParamId(filterIndex)),
        readParameterValue(state, EqlModuleProcessor::getFilterBandwidthParamId(filterIndex)),
        readParameterValue(state, EqlModuleProcessor::getFilterSlopeParamId(filterIndex)),
        readParameterValue(state, EqlModuleProcessor::getFilterGainParamId(filterIndex)),
        readParameterValue(state, EqlModuleProcessor::getFilterBypassParamId(filterIndex))
    };
}

void setFilterParameterValues(juce::AudioProcessorValueTreeState& state,
                              const int filterIndex,
                              const FilterParameterValues& values)
{
    setParameterValue(state, EqlModuleProcessor::getFilterTypeParamId(filterIndex), values.type);
    setParameterValue(state, EqlModuleProcessor::getFilterPlaceParamId(filterIndex), values.place);
    setParameterValue(state, EqlModuleProcessor::getFilterFrequencyParamId(filterIndex), values.frequency);
    setParameterValue(state, EqlModuleProcessor::getFilterBandwidthParamId(filterIndex), values.bandwidth);
    setParameterValue(state, EqlModuleProcessor::getFilterSlopeParamId(filterIndex), values.slope);
    setParameterValue(state, EqlModuleProcessor::getFilterGainParamId(filterIndex), values.gain);
    setParameterValue(state, EqlModuleProcessor::getFilterBypassParamId(filterIndex), values.bypass);
}

void resetFilterParameterValues(juce::AudioProcessorValueTreeState& state, const int filterIndex)
{
    forEachFilterParameterId(filterIndex,
                             [&state] (const juce::String& parameterId)
                             {
                                 if (auto* parameter = state.getParameter(parameterId))
                                     parameter->setValueNotifyingHost(parameter->getDefaultValue());
                             });
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

void copyXmlAttributesToValueTreeProperties(const juce::XmlElement& sourceElement,
                                            juce::ValueTree& targetState)
{
    for (int propertyIndex = targetState.getNumProperties(); --propertyIndex >= 0;)
        targetState.removeProperty(targetState.getPropertyName(propertyIndex), nullptr);

    for (int attributeIndex = 0; attributeIndex < sourceElement.getNumAttributes(); ++attributeIndex)
    {
        const auto attributeName = sourceElement.getAttributeName(attributeIndex);

        if (attributeName.isNotEmpty())
            targetState.setProperty(attributeName, sourceElement.getAttributeValue(attributeIndex), nullptr);
    }
}

void applyParameterValuesFromStateElement(juce::AudioProcessorValueTreeState& parameters,
                                          const juce::XmlElement& stateElement)
{
    for (auto* child : stateElement.getChildIterator())
    {
        if (! child->hasTagName("PARAM"))
            continue;

        const auto parameterId = child->getStringAttribute("id").trim();
        auto* parameter = parameters.getParameter(parameterId);

        if (parameter == nullptr)
            continue;

        const auto& range = parameter->getNormalisableRange();
        auto plainValue = static_cast<float>(child->getDoubleAttribute("value",
                                                                       parameter->convertFrom0to1(parameter->getDefaultValue())));
        plainValue = range.snapToLegalValue(juce::jlimit(range.start, range.end, plainValue));
        parameter->setValue(parameter->convertTo0to1(plainValue));
    }
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

EqlModuleProcessor::FilterType getRestoredFilterType(juce::XmlElement& stateElement,
                                                     juce::AudioProcessorValueTreeState& parameters,
                                                     const int filterIndex)
{
    const auto typeChoice = static_cast<int>(std::lround(readRestoredParameterValue(stateElement,
                                                                                    parameters,
                                                                                    EqlModuleProcessor::getFilterTypeParamId(filterIndex))));
    return EqlModuleProcessor::filterTypeFromChoiceIndex(typeChoice);
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

    for (int filterIndex = 0; filterIndex < EqlModuleProcessor::maxFilterCount; ++filterIndex)
    {
        forEachFilterParameterId(filterIndex,
                                 [&completeStateElement, &parameters] (const juce::String& parameterId)
                                 {
                                     addDefaultStateParameterElement(*completeStateElement, parameters, parameterId);
                                 });
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

void removeUnknownStateAttributes(juce::XmlElement& stateElement)
{
    for (int attributeIndex = stateElement.getNumAttributes(); --attributeIndex >= 0;)
    {
        const auto attributeName = stateElement.getAttributeName(attributeIndex);

        if (attributeName != EqlModuleProcessor::activeFilterCountStateKey
            && attributeName != EqlModuleProcessor::filterPresetLastSelectedStateKey
            && attributeName != EqlModuleProcessor::filterPresetDefaultSelectedStateKey)
            stateElement.removeAttribute(attributeName);
    }
}

int countRestoredCurrentFilters(juce::XmlElement& stateElement)
{
    const auto requestedFilterCount = clampActiveFilterCount(stateElement.getIntAttribute(EqlModuleProcessor::activeFilterCountStateKey, 0));
    auto restoredFilterCount = 0;

    for (int filterIndex = 0; filterIndex < requestedFilterCount; ++filterIndex)
    {
        if (findStateParameterElement(stateElement, EqlModuleProcessor::getFilterTypeParamId(filterIndex)) == nullptr)
            break;

        restoredFilterCount = filterIndex + 1;
    }

    return restoredFilterCount;
}

void normalizeRestoredStateElement(juce::XmlElement& stateElement,
                                   juce::AudioProcessorValueTreeState& parameters)
{
    removeUnknownStateParameterElements(stateElement, parameters);
    removeUnknownStateAttributes(stateElement);

    const auto restoredFilterCount = countRestoredCurrentFilters(stateElement);
    stateElement.setAttribute(EqlModuleProcessor::activeFilterCountStateKey, restoredFilterCount);

    for (int filterIndex = 0; filterIndex < EqlModuleProcessor::maxFilterCount; ++filterIndex)
    {
        const auto typeId = EqlModuleProcessor::getFilterTypeParamId(filterIndex);
        const auto placeId = EqlModuleProcessor::getFilterPlaceParamId(filterIndex);
        const auto gainId = EqlModuleProcessor::getFilterGainParamId(filterIndex);
        const auto bypassId = EqlModuleProcessor::getFilterBypassParamId(filterIndex);

        if (filterIndex >= restoredFilterCount)
        {
            forEachFilterParameterId(filterIndex,
                                     [&stateElement] (const juce::String& parameterId)
                                     {
                                         removeStateParameterElement(stateElement, parameterId);
                                     });
            continue;
        }

        ensureStateParameterElement(stateElement, parameters, typeId);
        ensureStateParameterElement(stateElement, parameters, placeId);
        ensureStateParameterElement(stateElement, parameters, gainId);
        ensureStateParameterElement(stateElement, parameters, bypassId);

        if (getRestoredFilterType(stateElement, parameters, filterIndex) == EqlModuleProcessor::FilterType::volume)
        {
            forEachFilterShapeParameterId(filterIndex,
                                          [&stateElement] (const juce::String& parameterId)
                                          {
                                              removeStateParameterElement(stateElement, parameterId);
                                          });
            continue;
        }

        forEachFilterShapeParameterId(filterIndex,
                                      [&stateElement, &parameters] (const juce::String& parameterId)
                                      {
                                          ensureStateParameterElement(stateElement, parameters, parameterId);
                                      });
    }
}
}

EqlModuleProcessor::EqlModuleProcessor()
    : parameters(internalParameterHost, nullptr, "eql_state", createParameterLayout())
{
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        filterTypeParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterTypeParamId(filterIndex));
        filterPlaceParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterPlaceParamId(filterIndex));
        filterFrequencyParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterFrequencyParamId(filterIndex));
        filterBandwidthParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterBandwidthParamId(filterIndex));
        filterSlopeChoiceParams[static_cast<size_t>(filterIndex)] = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter(getFilterSlopeParamId(filterIndex)));
        filterGainParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterGainParamId(filterIndex));
        filterBypassParams[static_cast<size_t>(filterIndex)] = parameters.getRawParameterValue(getFilterBypassParamId(filterIndex));
    }

    registerParameterListeners();

    ensureDefaultPresetFilesExist(*this);
}

EqlModuleProcessor::~EqlModuleProcessor()
{
    unregisterParameterListeners();
}

void EqlModuleProcessor::appendEqlParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& parameterLayout)
{
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        for (const auto& entry : eqlFilterOrder)
        {
            const auto key = juce::String(entry.key);
            const auto name = "EQL / FILTER " + juce::String(filterIndex + 1) + " / " + juce::String(entry.label);

            if (key == "type")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterTypeParamId(filterIndex), 1 },
                    name,
                    filterTypeChoices,
                    EqlModuleProcessor::choiceIndexFromFilterType(EqlModuleProcessor::FilterType::bell),
                    juce::AudioParameterChoiceAttributes()));
                continue;
            }

            if (key == "place")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterPlaceParamId(filterIndex), 1 },
                    name,
                    filterPlaceChoices,
                    0,
                    juce::AudioParameterChoiceAttributes()));
                continue;
            }

            if (key == "order")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterSlopeParamId(filterIndex), 1 },
                    name,
                    EqlModuleProcessor::getBellSlopeChoices(),
                    EqlModuleProcessor::getBellSlopeChoiceIndexForValue(EqlModuleProcessor::fixedSlopeDbPerOct),
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

juce::AudioProcessorValueTreeState::ParameterLayout EqlModuleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    appendEqlParameters(parameterLayout);
    return { parameterLayout.begin(), parameterLayout.end() };
}

void EqlModuleProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == activeFilterCountStateKey)
        return;

    if (suppressEqlFilterDirty.load(std::memory_order_acquire))
        return;

    markEqlFiltersDirty();
}

void EqlModuleProcessor::registerParameterListeners()
{
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        parameters.addParameterListener(getFilterTypeParamId(filterIndex), this);
        parameters.addParameterListener(getFilterPlaceParamId(filterIndex), this);
        parameters.addParameterListener(getFilterFrequencyParamId(filterIndex), this);
        parameters.addParameterListener(getFilterBandwidthParamId(filterIndex), this);
        parameters.addParameterListener(getFilterSlopeParamId(filterIndex), this);
        parameters.addParameterListener(getFilterGainParamId(filterIndex), this);
        parameters.addParameterListener(getFilterBypassParamId(filterIndex), this);
    }
}

void EqlModuleProcessor::unregisterParameterListeners()
{
    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        parameters.removeParameterListener(getFilterTypeParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterPlaceParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterFrequencyParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterBandwidthParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterSlopeParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterGainParamId(filterIndex), this);
        parameters.removeParameterListener(getFilterBypassParamId(filterIndex), this);
    }
}

juce::AudioProcessorValueTreeState& EqlModuleProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& EqlModuleProcessor::getValueTreeState() const noexcept
{
    return parameters;
}

void EqlModuleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = createSerializableStateXml(*this))
        juce::AudioProcessor::copyXmlToBinary(*stateXml, destData);
}

void EqlModuleProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    if (auto stateXml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes))
    {
        if (stateXml->hasTagName(parameters.state.getType()))
        {
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
                eqlFiltersDirty.store(false, std::memory_order_release);
                prepared.store(true, std::memory_order_release);
            }
            else
            {
                markEqlFiltersDirty();
            }
        }
    }
}

bool EqlModuleProcessor::applyStateInformationForABCompare(const void* data, const int sizeInBytes)
{
    auto stateXml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes);

    if (stateXml == nullptr || ! stateXml->hasTagName(parameters.state.getType()))
        return false;

    normalizeRestoredStateElement(*stateXml, parameters);

    auto completeStateXml = createCompleteRestoredStateElement(*stateXml, parameters);

    if (completeStateXml == nullptr)
        return false;

    const auto restoredFilterCount = clampActiveFilterCount(completeStateXml->getIntAttribute(activeFilterCountStateKey, 0));
    const auto previousDirtySuppression = suppressEqlFilterDirty.exchange(true, std::memory_order_acq_rel);

    applyParameterValuesFromStateElement(parameters, *completeStateXml);
    copyXmlAttributesToValueTreeProperties(*completeStateXml, parameters.state);
    activeFilterCount.store(restoredFilterCount, std::memory_order_relaxed);
    suppressEqlFilterDirty.store(previousDirtySuppression, std::memory_order_release);
    markEqlFiltersDirty();
    return true;
}

juce::String EqlModuleProcessor::getDefaultFilterPresetName() const
{
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

bool EqlModuleProcessor::renameFilterPreset(const juce::String& sourcePresetName, const juce::String& newPresetName)
{
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

bool EqlModuleProcessor::setDefaultFilterPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();

    if (trimmedName.isEmpty())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || findPresetElement(*presetsXml, trimmedName) == nullptr)
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

    if (auto* presetElement = findPresetElement(*presetsXml, trimmedName))
    {
        presetsXml->removeChildElement(presetElement, true);
        return writeFilterPresetsXml(*presetsXml);
    }

    return false;
}

int EqlModuleProcessor::getActiveFilterCount() const noexcept
{
    return activeFilterCount.load(std::memory_order_relaxed);
}

bool EqlModuleProcessor::addFilter() noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount >= maxFilterCount)
        return false;

    const auto newIndex = currentCount;
    setFilterParameterValues(parameters, newIndex, makeDefaultFilterParameterValues(FilterType::bell));
    setActiveFilterCount(currentCount + 1);
    return true;
}

bool EqlModuleProcessor::removeFilter(const int filterIndex) noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount <= 0 || filterIndex < 0 || filterIndex >= currentCount)
        return false;

    for (int sourceIndex = filterIndex + 1; sourceIndex < currentCount; ++sourceIndex)
    {
        const auto destinationIndex = sourceIndex - 1;
        setFilterParameterValues(parameters, destinationIndex, readFilterParameterValues(parameters, sourceIndex));
    }

    resetFilterParameterValues(parameters, currentCount - 1);

    setActiveFilterCount(currentCount - 1);
    resetFilters();
    updateFilters();
    return true;
}

bool EqlModuleProcessor::clearFilters() noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount <= 0)
        return false;

    for (int filterIndex = 0; filterIndex < currentCount; ++filterIndex)
        resetFilterParameterValues(parameters, filterIndex);

    setActiveFilterCount(0);
    resetFilters();
    updateFilters();
    return true;
}

bool EqlModuleProcessor::applyFilterOrder(const std::vector<int>& orderedFilterIndices) noexcept
{
    const auto currentCount = getActiveFilterCount();

    if (currentCount <= 1 || static_cast<int>(orderedFilterIndices.size()) != currentCount)
        return false;

    std::vector<bool> used(static_cast<size_t>(currentCount), false);

    for (const auto sourceIndex : orderedFilterIndices)
    {
        if (! juce::isPositiveAndBelow(sourceIndex, currentCount))
            return false;

        if (used[static_cast<size_t>(sourceIndex)])
            return false;

        used[static_cast<size_t>(sourceIndex)] = true;
    }

    auto alreadyInOrder = true;

    for (int destinationIndex = 0; destinationIndex < currentCount; ++destinationIndex)
    {
        if (orderedFilterIndices[static_cast<size_t>(destinationIndex)] != destinationIndex)
        {
            alreadyInOrder = false;
            break;
        }
    }

    if (alreadyInOrder)
        return false;

    std::vector<FilterParameterValues> snapshots(static_cast<size_t>(currentCount));

    for (int sourceIndex = 0; sourceIndex < currentCount; ++sourceIndex)
        snapshots[static_cast<size_t>(sourceIndex)] = readFilterParameterValues(parameters, sourceIndex);

    for (int destinationIndex = 0; destinationIndex < currentCount; ++destinationIndex)
    {
        const auto sourceIndex = orderedFilterIndices[static_cast<size_t>(destinationIndex)];
        setFilterParameterValues(parameters,
                                 destinationIndex,
                                 snapshots[static_cast<size_t>(sourceIndex)]);
    }

    resetFilters();
    updateFilters();
    return true;
}

void EqlModuleProcessor::setActiveFilterCount(const int newCount) noexcept
{
    activeFilterCount.store(clampActiveFilterCount(newCount), std::memory_order_relaxed);
    markEqlFiltersDirty();
}

void EqlModuleProcessor::markEqlFiltersDirty() noexcept
{
    eqlFiltersDirty.store(true, std::memory_order_release);
}
