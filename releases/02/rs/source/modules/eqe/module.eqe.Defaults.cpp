#include "module.eqe.ProcessorSupport.h"

float defaultFilterFrequencyForType(const EqeModuleProcessor::FilterType type)
{
    switch (type)
    {
        case EqeModuleProcessor::FilterType::lowShelf: return 120.0f;
        case EqeModuleProcessor::FilterType::highShelf: return 5000.0f;
        case EqeModuleProcessor::FilterType::lowCut: return 40.0f;
        case EqeModuleProcessor::FilterType::highCut: return 12000.0f;
        case EqeModuleProcessor::FilterType::tilt: return defaultTiltFrequency;
        case EqeModuleProcessor::FilterType::bell:
        default: return 632.0f;
    }
}

float defaultFilterBandwidthForType(const EqeModuleProcessor::FilterType type)
{
    juce::ignoreUnused(type);
    return 1.0f;
}

float defaultFilterSlopeForType(const EqeModuleProcessor::FilterType type)
{
    juce::ignoreUnused(type);
    return EqeModuleProcessor::fixedSlopeDbPerOct;
}
