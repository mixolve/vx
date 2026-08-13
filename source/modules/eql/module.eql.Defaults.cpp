#include "module.eql.ProcessorSupport.h"

float defaultFilterFrequencyForType(const EqlModuleProcessor::FilterType type)
{
    switch (type)
    {
        case EqlModuleProcessor::FilterType::lowShelf: return 120.0f;
        case EqlModuleProcessor::FilterType::highShelf: return 5000.0f;
        case EqlModuleProcessor::FilterType::lowCut: return 40.0f;
        case EqlModuleProcessor::FilterType::highCut: return 12000.0f;
        case EqlModuleProcessor::FilterType::tilt: return defaultTiltFrequency;
        case EqlModuleProcessor::FilterType::volume: return defaultTiltFrequency;
        case EqlModuleProcessor::FilterType::bell:
        default: return 632.0f;
    }
}

float defaultFilterBandwidth()
{
    return 1.0f;
}

float defaultFilterSlope()
{
    return EqlModuleProcessor::fixedSlopeDbPerOct;
}
