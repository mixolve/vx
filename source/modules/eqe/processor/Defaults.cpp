#include "ProcessorSupport.h"

float defaultFilterFrequencyForType(const EqeAudioProcessor::FilterType type)
{
    switch (type)
    {
        case EqeAudioProcessor::FilterType::lowShelf: return 120.0f;
        case EqeAudioProcessor::FilterType::highShelf: return 5000.0f;
        case EqeAudioProcessor::FilterType::lowCut: return 40.0f;
        case EqeAudioProcessor::FilterType::highCut: return 12000.0f;
        case EqeAudioProcessor::FilterType::tilt: return defaultTiltFrequency;
        case EqeAudioProcessor::FilterType::bell:
        default: return 632.0f;
    }
}

float defaultFilterBandwidthForType(const EqeAudioProcessor::FilterType type)
{
    juce::ignoreUnused(type);
    return 1.0f;
}

float defaultFilterSlopeForType(const EqeAudioProcessor::FilterType type)
{
    juce::ignoreUnused(type);
    return EqeAudioProcessor::fixedSlopeDbPerOct;
}
