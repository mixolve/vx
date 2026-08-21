#include "module.eql.ProcessorSupport.h"

float defaultFilterFrequency()
{
    return defaultFilterFrequencyHz;
}

float defaultFilterBandwidth()
{
    return 1.0f;
}

float defaultFilterSlope()
{
    return EqlModuleProcessor::fixedSlopeDbPerOct;
}
