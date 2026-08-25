#include "Processor.h"

EqlModuleProcessor::EqlModuleProcessor()
    : parameters(parameterHost, nullptr, "eql_state", createParameterLayout())
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

    setParameterListenersEnabled(true);
}

EqlModuleProcessor::~EqlModuleProcessor()
{
    setParameterListenersEnabled(false);
}

juce::AudioProcessorValueTreeState& EqlModuleProcessor::getValueTreeState() noexcept
{
    return parameters;
}

const juce::AudioProcessorValueTreeState& EqlModuleProcessor::getValueTreeState() const noexcept
{
    return parameters;
}
