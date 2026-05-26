#include "module.eqe.ProcessorSupport.h"

#include <memory>
#include <vector>

namespace
{
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
}

EqeModuleProcessor::EqeModuleProcessor(juce::AudioProcessor& ownerProcessor)
    : parameters(ownerProcessor, nullptr, "eqe_state", createParameterLayout())
{
    bypassParam = parameters.getRawParameterValue(paramBypassId);
    bypassWithGainParam = parameters.getRawParameterValue(paramBypassWithGainId);
    inGainLrParam = parameters.getRawParameterValue(paramInGainLrId);
    inGainLParam = parameters.getRawParameterValue(paramInGainLId);
    inGainRParam = parameters.getRawParameterValue(paramInGainRId);
    wideParam = parameters.getRawParameterValue(paramWideId);
    outGainParam = parameters.getRawParameterValue(paramOutGainId);

    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        filterTypeParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getFilterTypeParamId(bellIndex));
        filterTtssParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getFilterTtssParamId(bellIndex));
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

        const auto defaultFilterPresetName = getDefaultFilterPresetName();

        if (defaultFilterPresetName.isNotEmpty())
        {
            if (! loadFilterPreset(defaultFilterPresetName))
            {
                if (const auto lastFilterPresetName = getLastFilterPresetName(); lastFilterPresetName.isNotEmpty())
                    loadFilterPreset(lastFilterPresetName);
            }
            else if (parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim().isEmpty())
            {
                parameters.state.setProperty(filterPresetDefaultSelectedStateKey, defaultFilterPresetName, nullptr);
            }
        }
        else if (const auto lastFilterPresetName = getLastFilterPresetName(); lastFilterPresetName.isNotEmpty())
        {
            loadFilterPreset(lastFilterPresetName);
        }
    }
}

EqeModuleProcessor::~EqeModuleProcessor()
{
    unregisterParameterListeners();
}

void EqeModuleProcessor::appendEqeParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& parameterLayout)
{
    const auto makeEqeMiscName = [] (const juce::String& parameterName)
    {
        return "EQE - MISC - " + parameterName;
    };

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramBypassId, 1 },
        makeEqeMiscName("BYPASS"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramBypassWithGainId, 1 },
        makeEqeMiscName("BYPASS.WT-GAIN"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramInGainLrId, 1 },
        makeEqeMiscName("IN-GAIN-LR"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramInGainLId, 1 },
        makeEqeMiscName("IN-GAIN-L"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramInGainRId, 1 },
        makeEqeMiscName("IN-GAIN-R"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramWideId, 1 },
        makeEqeMiscName("WIDE"),
        juce::NormalisableRange<float> { 0.0f, 400.0f, 0.01f },
        100.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return juce::String::formatted("%.0f", static_cast<double>(value));
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramOutGainId, 1 },
        makeEqeMiscName("OUT-GAIN"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getFilterTypeParamId(bellIndex), 1 },
            makeFilterTypeName(bellIndex),
            filterTypeChoices,
            EqeModuleProcessor::choiceIndexFromFilterType(EqeModuleProcessor::FilterType::bell),
            juce::AudioParameterChoiceAttributes().withAutomatable(true)));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getFilterTtssParamId(bellIndex), 1 },
            makeFilterTtssName(bellIndex),
            filterTtssChoices,
            0,
            juce::AudioParameterChoiceAttributes().withAutomatable(true)));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getFilterLrmsParamId(bellIndex), 1 },
            makeFilterLrmsName(bellIndex),
            filterLrmsChoices,
            0,
            juce::AudioParameterChoiceAttributes().withAutomatable(true)));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getBellSlopeParamId(bellIndex), 1 },
            makeBellParameterName("ORDER", bellIndex),
            EqeModuleProcessor::getBellSlopeChoices(),
            EqeModuleProcessor::getBellSlopeChoiceIndexForValue(EqeModuleProcessor::fixedSlopeDbPerOct),
            juce::AudioParameterChoiceAttributes().withAutomatable(true)));

        auto filterFrequencyRange = juce::NormalisableRange<float> { minimumVisibleFilterFrequency, maximumVisibleFilterFrequency, 0.01f };
        filterFrequencyRange.setSkewForCentre(defaultTiltFrequency);

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getBellFrequencyParamId(bellIndex), 1 },
            makeBellParameterName("FREQ", bellIndex),
            filterFrequencyRange,
            defaultTiltFrequency,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatFrequencyValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getBellBandwidthParamId(bellIndex), 1 },
            makeBellParameterName("BW", bellIndex),
            juce::NormalisableRange<float> { minimumBellBandwidth, maximumBellBandwidth, 0.001f },
            1.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatBandwidthValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getBellGainParamId(bellIndex), 1 },
            makeBellParameterName("GAIN", bellIndex),
            juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
            0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatDecibelValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { getBellBypassParamId(bellIndex), 1 },
            makeBellParameterName("BYPASS", bellIndex),
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(true)));
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
        parameters.addParameterListener(getFilterTtssParamId(bellIndex), this);
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
        parameters.removeParameterListener(getFilterTtssParamId(bellIndex), this);
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
            auto restoredState = juce::ValueTree::fromXml(*stateXml);
            const auto restoredBellCount = static_cast<int>(restoredState.getProperty(activeBellCountStateKey, 0));
            parameters.replaceState(restoredState);
            setActiveBellCount(restoredBellCount);
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
    rewriteFilterParameterIds(*normalizedStateElement, false);

    const auto restoredBellCount = clampActiveBellCount(normalizedStateElement->getIntAttribute(activeBellCountStateKey, 0));
    normalizedStateElement->setAttribute(activeBellCountStateKey, restoredBellCount);

    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        ensureStateParameterElement(*normalizedStateElement, parameters, getFilterTypeParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getFilterTtssParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getFilterLrmsParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellSlopeParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellFrequencyParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellBandwidthParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellGainParamId(bellIndex));
        ensureStateParameterElement(*normalizedStateElement, parameters, getBellBypassParamId(bellIndex));
    }

    auto restoredState = juce::ValueTree::fromXml(*normalizedStateElement);
    parameters.replaceState(restoredState);
    setActiveBellCount(restoredBellCount);
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
    setParameterValue(parameters, getFilterTtssParamId(newIndex), 0.0f);
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
        setParameterValue(parameters, getFilterTtssParamId(destinationIndex), readParameterValue(parameters, getFilterTtssParamId(sourceIndex)));
        setParameterValue(parameters, getFilterLrmsParamId(destinationIndex), readParameterValue(parameters, getFilterLrmsParamId(sourceIndex)));
        setParameterValue(parameters, getBellFrequencyParamId(destinationIndex), readParameterValue(parameters, getBellFrequencyParamId(sourceIndex)));
        setParameterValue(parameters, getBellBandwidthParamId(destinationIndex), readParameterValue(parameters, getBellBandwidthParamId(sourceIndex)));
        setParameterValue(parameters, getBellSlopeParamId(destinationIndex), readParameterValue(parameters, getBellSlopeParamId(sourceIndex)));
        setParameterValue(parameters, getBellGainParamId(destinationIndex), readParameterValue(parameters, getBellGainParamId(sourceIndex)));
        setParameterValue(parameters, getBellBypassParamId(destinationIndex), readParameterValue(parameters, getBellBypassParamId(sourceIndex)));
    }

    const auto lastIndex = currentCount - 1;
    if (auto* parameter = parameters.getParameter(getFilterTypeParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    if (auto* parameter = parameters.getParameter(getFilterTtssParamId(lastIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
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
        if (auto* parameter = parameters.getParameter(getFilterTtssParamId(bellIndex))) parameter->setValueNotifyingHost(parameter->getDefaultValue());
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
        float ttss = 0.0f;
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
        snapshot.ttss = readParameterValue(parameters, getFilterTtssParamId(bandIndex));
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
        setParameterValue(parameters, getFilterTtssParamId(bandIndex), snapshot.ttss);
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
