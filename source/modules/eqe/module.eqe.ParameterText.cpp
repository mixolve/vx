#include "module.eqe.ProcessorSupport.h"

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

juce::String makeFilterTtssName(const int filterIndex)
{
    return "FILTER " + juce::String(filterIndex + 1) + " - TTSS";
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
    return juce::jlimit(0, EqeModuleProcessor::maxBellFilterCount, bellCount);
}

juce::String filterTypeDisplayPrefix(const EqeModuleProcessor::FilterType type)
{
    switch (type)
    {
        case EqeModuleProcessor::FilterType::lowShelf: return "LSH";
        case EqeModuleProcessor::FilterType::lowCut: return "LCT";
        case EqeModuleProcessor::FilterType::highCut: return "HCT";
        case EqeModuleProcessor::FilterType::highShelf: return "HSH";
        case EqeModuleProcessor::FilterType::tilt: return "FTL";
        case EqeModuleProcessor::FilterType::bell:
        default: return "BEL";
    }
}
