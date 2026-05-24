#pragma once

#include <JuceHeader.h>

#include <cmath>

namespace mxe::dsp
{
inline constexpr double epsilon = 1.0e-9;

struct SmoothedValue
{
    double current = 1.0;
    double target = 1.0;
    double step = 0.0;
    int remainingSteps = 0;

    void snapTo(const double value) noexcept
    {
        current = value;
        target = value;
        step = 0.0;
        remainingSteps = 0;
    }

    void setTarget(const double value) noexcept
    {
        target = value;
    }

    void beginBlock(const int numSamples) noexcept
    {
        if (numSamples <= 1 || std::abs(target - current) <= epsilon)
        {
            current = target;
            step = 0.0;
            remainingSteps = 0;
            return;
        }

        remainingSteps = numSamples - 1;
        step = (target - current) / static_cast<double>(remainingSteps);
    }

    void advance() noexcept
    {
        if (remainingSteps <= 0)
        {
            current = target;
            return;
        }

        current += step;
        --remainingSteps;

        if (remainingSteps == 0)
            current = target;
    }
};

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
