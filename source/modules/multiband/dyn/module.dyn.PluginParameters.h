#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstddef>

namespace dyn::parameters
{
enum class ParameterType
{
    floating,
    boolean,
    choice,
};

enum class ParameterSlot : size_t
{
    morph,
    peakHoldMs,
    lookahead,
    tensionFloor,
    tensionHysteresis,
    releaseForm,
    releaseCurve,
    linkUpDn,
    linkLr,
    linkOpp,
    thLU,
    tensLU,
    relLU,
    outLU,
    thLD,
    tensLD,
    relLD,
    outLD,
    thRU,
    tensRU,
    relRU,
    outRU,
    thRD,
    tensRD,
    relRD,
    outRD,
    delta,
    count
};

enum class CrossoverSlot : size_t
{
    xover1,
    xover2,
    xover3,
    xover4,
    xover5,
    count
};

inline constexpr size_t numParameterSlots = static_cast<size_t>(ParameterSlot::count);
inline constexpr size_t numCrossoverSlots = static_cast<size_t>(CrossoverSlot::count);

struct ParameterSpec
{
    const char* suffix = "";
    const char* name = "";
    ParameterType type = ParameterType::floating;
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.01f;
    float defaultValue = 0.0f;
    const char* label = "";
};

inline constexpr auto parameterSpecs = std::to_array<ParameterSpec>({
    { "morph", "Morph", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "peak_hold", "Peak Hold", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "ms" },
    { "lookahead", "Lookahead", ParameterType::floating, 0.0f, 24.0f, 0.01f, 0.0f, "ms" },
    { "tension_floor", "Tension Floor", ParameterType::floating, -96.0f, 0.0f, 0.01f, -96.0f, "dB" },
    { "tension_hysteresis", "Tension Hysteresis", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "release_form", "Release Form", ParameterType::choice, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "release_curve", "Release Curve", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "linkUpDn", "Link UP/DN (Dual-Mono)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "linkLr", "Link L/R (Stereo)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "linkOpp", "Link Opp", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "thLU", "L.UP.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "tensLU", "L.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "relLU", "L.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "outLU", "L.UP.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "thLD", "L.DN.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "tensLD", "L.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "relLD", "L.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "outLD", "L.DN.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "thRU", "R.UP.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "tensRU", "R.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "relRU", "R.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "outRU", "R.UP.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "thRD", "R.DN.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "tensRD", "R.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "relRD", "R.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "outRD", "R.DN.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "delta", "Delta", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
});

inline constexpr auto crossoverSpecs = std::to_array<ParameterSpec>({
    { "xover1", "Crossover 1", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 134.0f, "Hz" },
    { "xover2", "Crossover 2", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 523.0f, "Hz" },
    { "xover3", "Crossover 3", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 2093.0f, "Hz" },
    { "xover4", "Crossover 4", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 5000.0f, "Hz" },
    { "xover5", "Crossover 5", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 10000.0f, "Hz" },
});

static_assert(parameterSpecs.size() == numParameterSlots);
static_assert(crossoverSpecs.size() == numCrossoverSlots);

constexpr size_t toIndex(const ParameterSlot slot)
{
    return static_cast<size_t>(slot);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace dyn::parameters
