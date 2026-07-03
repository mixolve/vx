#pragma once

#include <JuceHeader.h>

#include <cmath>

namespace mxe::dsp
{
inline constexpr double epsilon = 1.0e-9;

inline double roundToJsfxStep(const double value)
{
    return std::floor((value * 10.0) + 0.5) * 0.1;
}

inline double dbToAmp(const double decibels)
{
    return std::pow(10.0, decibels / 20.0);
}

inline int wrapIndex(int index, const int size)
{
    jassert(size > 0);

    index %= size;

    if (index < 0)
        index += size;

    return index;
}
} // namespace mxe::dsp
