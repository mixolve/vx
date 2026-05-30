#pragma once

#include <JuceHeader.h>

#include <cmath>

namespace mxe::formatting
{
inline double roundToDspDisplayStep(const double value) noexcept
{
    auto rounded = std::floor((value * 10.0) + 0.5) * 0.1;

    if (std::abs(rounded) < 0.05)
        rounded = 0.0;

    return rounded;
}

inline juce::String formatDspValue(const double value)
{
    return juce::String::formatted("%.1f", roundToDspDisplayStep(value));
}
} // namespace mxe::formatting
