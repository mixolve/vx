#pragma once

#include <cstddef>

namespace spe
{
inline constexpr std::size_t analyserScopeSize = 512;

struct DisplaySettings
{
    float leftFrequencyHz = 21.0f;
    float rightFrequencyHz = 20000.0f;
    float rangeLowDb = -60.0f;
    float rangeHighDb = 10.0f;
    float thresholdDb = 12.0f;
    float slopeDbPerOct = 4.0f;
};

struct AnalysisSettings
{
    int fftSize = 8192;
    int overlapFactor = 32;
    float averagingTimeMs = 50.0f;
};
}
