#include "ProcessorSupport.h"

juce::String formatDecibelValue(const float value)
{
    return juce::String::formatted("%.2f dB", static_cast<double>(value));
}

juce::String formatFrequencyValue(const float value)
{
    return juce::String::formatted("%.2f Hz", static_cast<double>(value));
}

juce::String formatBandwidthValue(const float value)
{
    return juce::String::formatted("%.2f oct", static_cast<double>(value));
}

juce::String makeFilterTypeName(const int filterIndex)
{
    return "FILTER " + juce::String(filterIndex + 1) + " - TYPE";
}

juce::String makeFilterLrmsName(const int filterIndex)
{
    return "FILTER " + juce::String(filterIndex + 1) + " - PLACE";
}

juce::String makeFilterParameterId(const char* suffix, const int filterIndex)
{
    return "filter_" + juce::String(filterIndex + 1) + "_" + suffix;
}

juce::String makeBellParameterName(const char* suffix, const int bellIndex)
{
    return "FILTER " + juce::String(bellIndex + 1) + " - " + suffix;
}

int clampActiveBellCount(const int bellCount)
{
    return juce::jlimit(0, EqeAudioProcessor::maxBellFilterCount, bellCount);
}

juce::String filterTypeDisplayPrefix(const EqeAudioProcessor::FilterType type)
{
    switch (type)
    {
        case EqeAudioProcessor::FilterType::lowShelf: return "LSH";
        case EqeAudioProcessor::FilterType::lowCut: return "LCT";
        case EqeAudioProcessor::FilterType::highCut: return "HCT";
        case EqeAudioProcessor::FilterType::highShelf: return "HSH";
        case EqeAudioProcessor::FilterType::tilt: return "FTL";
        case EqeAudioProcessor::FilterType::bell:
        default: return "BEL";
    }
}
