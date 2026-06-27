#include "module.eqe.ProcessorSupport.h"

#include <array>
#include <memory>
#include <vector>

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

void normalizeRestoredStateElement(juce::XmlElement& stateElement,
                                   juce::AudioProcessorValueTreeState& parameters)
{
    rewriteFilterParameterIds(stateElement, false);
    removeUnknownStateParameterElements(stateElement, parameters);

    const auto restoredBellCount = clampActiveBellCount(stateElement.getIntAttribute(EqeModuleProcessor::activeBellCountStateKey, 0));
    stateElement.setAttribute(EqeModuleProcessor::activeBellCountStateKey, restoredBellCount);

    for (int bellIndex = 0; bellIndex < EqeModuleProcessor::maxBellFilterCount; ++bellIndex)
    {
        ensureStateParameterElement(stateElement, parameters, EqeModuleProcessor::getFilterTypeParamId(bellIndex));
        ensureStateParameterElement(stateElement, parameters, EqeModuleProcessor::getFilterLrmsParamId(bellIndex));
        ensureStateParameterElement(stateElement, parameters, EqeModuleProcessor::getBellSlopeParamId(bellIndex));
        ensureStateParameterElement(stateElement, parameters, EqeModuleProcessor::getBellFrequencyParamId(bellIndex));
        ensureStateParameterElement(stateElement, parameters, EqeModuleProcessor::getBellBandwidthParamId(bellIndex));
        ensureStateParameterElement(stateElement, parameters, EqeModuleProcessor::getBellGainParamId(bellIndex));
        ensureStateParameterElement(stateElement, parameters, EqeModuleProcessor::getBellBypassParamId(bellIndex));
    }
}
}

EqeModuleProcessor::EqeModuleProcessor(juce::AudioProcessor& ownerProcessor)
    : parameters(internalParameterHost, nullptr, "eqe_state", createParameterLayout())
{
    juce::ignoreUnused(ownerProcessor);
    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        filterTypeParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getFilterTypeParamId(bellIndex));
        filterLrmsParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getFilterLrmsParamId(bellIndex));
        bellFrequencyParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellFrequencyParamId(bellIndex));
        bellBandwidthParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellBandwidthParamId(bellIndex));
        bellSlopeChoiceParams[static_cast<size_t>(bellIndex)] = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter(getBellSlopeParamId(bellIndex)));
        bellGainParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellGainParamId(bellIndex));
        bellBypassParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellBypassParamId(bellIndex));
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
    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        for (const auto& entry : eqeFilterOrder)
        {
            const auto key = juce::String(entry.key);
            const auto name = "FILTER " + juce::String(bellIndex + 1) + " - " + juce::String(entry.label);

            if (key == "type")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterTypeParamId(bellIndex), 1 },
                    name,
                    filterTypeChoices,
                    EqeModuleProcessor::choiceIndexFromFilterType(EqeModuleProcessor::FilterType::bell),
                    juce::AudioParameterChoiceAttributes()));
                continue;
            }

            if (key == "place")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getFilterLrmsParamId(bellIndex), 1 },
                    name,
                    filterLrmsChoices,
                    0,
                    juce::AudioParameterChoiceAttributes()));
                continue;
            }

            if (key == "order")
            {
                parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID { getBellSlopeParamId(bellIndex), 1 },
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
                    juce::ParameterID { getBellFrequencyParamId(bellIndex), 1 },
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
                    juce::ParameterID { getBellBandwidthParamId(bellIndex), 1 },
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
                    juce::ParameterID { getBellGainParamId(bellIndex), 1 },
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
                    juce::ParameterID { getBellBypassParamId(bellIndex), 1 },
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
    if (parameterID == activeBellCountStateKey)
        return;

    markEqeFiltersDirty();
}

void EqeModuleProcessor::registerParameterListeners()
{
    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        parameters.addParameterListener(getFilterTypeParamId(bellIndex), this);
        parameters.addParameterListener(getFilterLrmsParamId(bellIndex), this);
        parameters.addParameterListener(getBellFrequencyParamId(bellIndex), this);
        parameters.addParameterListener(getBellBandwidthParamId(bellIndex), this);
        parameters.addParameterListener(getBellSlopeParamId(bellIndex), this);
        parameters.addParameterListener(getBellGainParamId(bellIndex), this);
        parameters.addParameterListener(getBellBypassParamId(bellIndex), this);
    }
}

void EqeModuleProcessor::unregisterParameterListeners()
{
    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        parameters.removeParameterListener(getFilterTypeParamId(bellIndex), this);
        parameters.removeParameterListener(getFilterLrmsParamId(bellIndex), this);
        parameters.removeParameterListener(getBellFrequencyParamId(bellIndex), this);
        parameters.removeParameterListener(getBellBandwidthParamId(bellIndex), this);
        parameters.removeParameterListener(getBellSlopeParamId(bellIndex), this);
        parameters.removeParameterListener(getBellGainParamId(bellIndex), this);
        parameters.removeParameterListener(getBellBypassParamId(bellIndex), this);
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
    auto state = parameters.copyState();
    state.setProperty(activeBellCountStateKey, getActiveBellCount(), nullptr);

    if (auto stateXml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary(*stateXml, destData);
}

void EqeModuleProcessor::setStateInformation(const void* data, const int sizeInBytes)
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
                restoredState = juce::ValueTree::fromXml(*normalizedStateElement);
            }

            const auto restoredBellCount = clampActiveBellCount(static_cast<int>(restoredState.getProperty(activeBellCountStateKey, 0)));
            parameters.replaceState(restoredState);
            setActiveBellCount(restoredBellCount);
            resetBellFilters();

            if (wasPrepared && currentSampleRate > 0.0)
            {
                updateBellFilters();
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

bool EqeModuleProcessor::renameFilterPreset(const juce::String& oldPresetName, const juce::String& newPresetName)
{
    if (! supportsPersistentEqePresets())
        return false;

    auto presetsXml = loadFilterPresetsXml();

    if (presetsXml == nullptr || ! presetsXml->hasTagName(filterPresetsRootTag))
        return false;

    auto* oldPresetElement = findPresetElement(*presetsXml, oldPresetName.trim());

    if (oldPresetElement == nullptr)
        return false;

    oldPresetElement->setAttribute("name", newPresetName.trim());

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

    auto normalizedStateElement = std::make_unique<juce::XmlElement>(*stateElement);
    normalizeRestoredStateElement(*normalizedStateElement, parameters);

    const auto wasPrepared = prepared.exchange(false, std::memory_order_acq_rel);
    const juce::ScopedLock lock(filterProcessLock);
    const auto restoredBellCount = clampActiveBellCount(normalizedStateElement->getIntAttribute(activeBellCountStateKey, 0));
    auto restoredState = juce::ValueTree::fromXml(*normalizedStateElement);
    parameters.replaceState(restoredState);
    setActiveBellCount(restoredBellCount);
    resetBellFilters();

    if (wasPrepared && currentSampleRate > 0.0)
    {
        updateBellFilters();
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

int EqeModuleProcessor::getActiveBellCount() const noexcept
{
    return activeBellCount.load(std::memory_order_relaxed);
}

bool EqeModuleProcessor::addBellFilter() noexcept
{
    const auto currentCount = getActiveBellCount();

    if (currentCount >= maxBellFilterCount)
        return false;

    const auto newIndex = currentCount;
    constexpr auto defaultType = FilterType::bell;
    setParameterValue(parameters, getFilterTypeParamId(newIndex), static_cast<float>(choiceIndexFromFilterType(defaultType)));
    setParameterValue(parameters, getFilterLrmsParamId(newIndex), 0.0f);
    setParameterValue(parameters, getBellFrequencyParamId(newIndex), defaultFilterFrequencyForType(defaultType));
    setParameterValue(parameters, getBellBandwidthParamId(newIndex), defaultFilterBandwidthForType(defaultType));
    setParameterValue(parameters, getBellSlopeParamId(newIndex), static_cast<float>(EqeModuleProcessor::getBellSlopeChoiceIndexForValue(defaultFilterSlopeForType(defaultType))));
    setParameterValue(parameters, getBellGainParamId(newIndex), 0.0f);
    setParameterValue(parameters, getBellBypassParamId(newIndex), 0.0f);
    setActiveBellCount(currentCount + 1);
    return true;
}

bool EqeModuleProcessor::removeBellFilter(const int bellIndex) noexcept
{
    const auto currentCount = getActiveBellCount();

    if (currentCount <= 0 || bellIndex < 0 || bellIndex >= currentCount)
        return false;

    for (int sourceIndex = bellIndex + 1; sourceIndex < currentCount; ++sourceIndex)
    {
        const auto destinationIndex = sourceIndex - 1;
        setParameterValue(parameters, getFilterTypeParamId(destinationIndex), readParameterValue(parameters, getFilterTypeParamId(sourceIndex)));
        setParameterValue(parameters, getFilterLrmsParamId(destinationIndex), readParameterValue(parameters, getFilterLrmsParamId(sourceIndex)));
        setParameterValue(parameters, getBellFrequencyParamId(destinationIndex), readParameterValue(parameters, getBellFrequencyParamId(sourceIndex)));
        setParameterValue(parameters, getBellBandwidthParamId(destinationIndex), readParameterValue(parameters, getBellBandwidthParamId(sourceIndex)));
        setParameterValue(parameters, getBellSlopeParamId(destinationIndex), readParameterValue(parameters, getBellSlopeParamId(sourceIndex)));
        setParameterValue(parameters, getBellGainParamId(destinationIndex), readParameterValue(parameters, getBellGainParamId(sourceIndex)));
        setParameterValue(parameters, getBellBypassParamId(destinationIndex), readParameterValue(parameters, getBellBypassParamId(sourceIndex)));
    }

    const auto lastIndex = currentCount - 1;
    if (auto* parameter = parameters.getParameter(getFilterTypeParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterLrmsParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getBellFrequencyParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getBellBandwidthParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getBellSlopeParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getBellGainParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getBellBypassParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());

    setActiveBellCount(currentCount - 1);
    resetBellFilters();
    updateBellFilters();
    return true;
}

bool EqeModuleProcessor::clearBellFilters() noexcept
{
    const auto currentCount = getActiveBellCount();

    if (currentCount <= 0)
        return false;

    for (int bellIndex = 0; bellIndex < currentCount; ++bellIndex)
    {
        if (auto* parameter = parameters.getParameter(getFilterTypeParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getFilterLrmsParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getBellFrequencyParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getBellBandwidthParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getBellSlopeParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getBellGainParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
        if (auto* parameter = parameters.getParameter(getBellBypassParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    }

    setActiveBellCount(0);
    resetBellFilters();
    updateBellFilters();
    return true;
}

bool EqeModuleProcessor::moveBellFilter(const int sourceIndex, const int destinationIndex) noexcept
{
    const auto currentCount = getActiveBellCount();

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
        snapshot.frequency = readParameterValue(parameters, getBellFrequencyParamId(bandIndex));
        snapshot.bandwidth = readParameterValue(parameters, getBellBandwidthParamId(bandIndex));
        snapshot.slope = readParameterValue(parameters, getBellSlopeParamId(bandIndex));
        snapshot.gain = readParameterValue(parameters, getBellGainParamId(bandIndex));
        snapshot.bypass = readParameterValue(parameters, getBellBypassParamId(bandIndex));
    }

    auto movedSnapshot = snapshots[static_cast<size_t>(sourceIndex)];
    snapshots.erase(snapshots.begin() + sourceIndex);
    snapshots.insert(snapshots.begin() + destinationIndex, movedSnapshot);

    for (int bandIndex = 0; bandIndex < currentCount; ++bandIndex)
    {
        const auto& snapshot = snapshots[static_cast<size_t>(bandIndex)];
        setParameterValue(parameters, getFilterTypeParamId(bandIndex), snapshot.type);
        setParameterValue(parameters, getFilterLrmsParamId(bandIndex), snapshot.lrms);
        setParameterValue(parameters, getBellFrequencyParamId(bandIndex), snapshot.frequency);
        setParameterValue(parameters, getBellBandwidthParamId(bandIndex), snapshot.bandwidth);
        setParameterValue(parameters, getBellSlopeParamId(bandIndex), snapshot.slope);
        setParameterValue(parameters, getBellGainParamId(bandIndex), snapshot.gain);
        setParameterValue(parameters, getBellBypassParamId(bandIndex), snapshot.bypass);
    }

    resetBellFilters();
    updateBellFilters();
    return true;
}

void EqeModuleProcessor::setActiveBellCount(const int newCount) noexcept
{
    activeBellCount.store(clampActiveBellCount(newCount), std::memory_order_relaxed);
    markEqeFiltersDirty();
}

void EqeModuleProcessor::markEqeFiltersDirty() noexcept
{
    eqeFiltersDirty.store(true, std::memory_order_release);
}
