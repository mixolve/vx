#include "ProcessorSupport.h"
#include "FilterParameters.h"
#include "StateRestoration.h"

#include <cmath>
#include <memory>

namespace eql_state
{
static void ensureStateParameterElement(juce::XmlElement& stateElement,
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

static void addDefaultStateParameterElement(juce::XmlElement& stateElement,
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

static juce::XmlElement* findStateParameterElement(juce::XmlElement& stateElement,
                                            const juce::String& parameterId)
{
    for (auto* child : stateElement.getChildIterator())
    {
        if (child->hasTagName("PARAM") && child->getStringAttribute("id").equalsIgnoreCase(parameterId))
            return child;
    }

    return nullptr;
}

static void removeStateParameterElement(juce::XmlElement& stateElement,
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

static float readRestoredParameterValue(juce::XmlElement& stateElement,
                                 juce::AudioProcessorValueTreeState& parameters,
                                 const juce::String& parameterId)
{
    if (auto* parameterElement = findStateParameterElement(stateElement, parameterId))
        return static_cast<float>(parameterElement->getDoubleAttribute("value", 0.0));

    if (auto* parameter = parameters.getParameter(parameterId))
        return parameter->convertFrom0to1(parameter->getDefaultValue());

    return 0.0f;
}

static EqlModuleProcessor::FilterType getRestoredFilterType(juce::XmlElement& stateElement,
                                                     juce::AudioProcessorValueTreeState& parameters,
                                                     const int filterIndex)
{
    const auto typeChoice = static_cast<int>(std::lround(readRestoredParameterValue(stateElement,
                                                                                    parameters,
                                                                                    EqlModuleProcessor::getFilterTypeParamId(filterIndex))));
    return EqlModuleProcessor::filterTypeFromChoiceIndex(typeChoice);
}

static void copyParameterElementValue(juce::XmlElement& targetStateElement,
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

static void removeUnknownStateParameterElements(juce::XmlElement& stateElement,
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

static void removeUnknownStateAttributes(juce::XmlElement& stateElement)
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

static int countRestoredCurrentFilters(juce::XmlElement& stateElement)
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
} // namespace eql_state

using namespace eql_state;

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
