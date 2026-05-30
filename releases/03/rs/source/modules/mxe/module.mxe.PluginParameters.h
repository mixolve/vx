#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstddef>

namespace mxe::parameters
{
enum class ParameterType
{
    floating,
    boolean,
};

enum class ParameterSlot : size_t
{
    inGn,
    inRight,
    inLeft,
    autoInGn,
    autoInRight,
    autoInLeft,
    wide,
    outGn,
    moRph,
    peakHoldHz,
    TensionFlooR,
    TensionHysT,
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
    delTa,
    count
};

enum class FullbandAutomationSlot : size_t
{
    inGn,
    inRight,
    inLeft,
    count
};

enum class FullbandVisibleSlot : size_t
{
    inGn,
    outGn,
    wide,
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
inline constexpr size_t numFullbandVisibleSlots = static_cast<size_t>(FullbandVisibleSlot::count);
inline constexpr size_t numFullbandAutomationSlots = static_cast<size_t>(FullbandAutomationSlot::count);
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
    { "inGn", "Input Gain", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "inRight", "Input Right", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "inLeft", "Input Left", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "autoInGn", "AUTO INPUT-GAIN", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "autoInRight", "AUTO IN-RIGHT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "autoInLeft", "AUTO IN-LEFT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "wide", "IN-WIDE", ParameterType::floating, -100.0f, 400.0f, 0.1f, 100.0f, "%" },
    { "outGn", "Out Gain", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "moRph", "Morph", ParameterType::floating, 0.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "peakHoldHz", "Peak Hold", ParameterType::floating, 21.0f, 3675.1f, 0.1f, 100.0f, "Hz" },
    { "TensionFlooR", "Tension Floor", ParameterType::floating, -96.0f, 0.0f, 0.1f, -96.0f, "dB" },
    { "TensionHysT", "Tension Hysteresis", ParameterType::floating, 0.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "linkUpDn", "Link UP/DN (Dual-Mono)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "linkLr", "Link L/R (Stereo)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "linkOpp", "Link Opp", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "thLU", "L.UP.THR", ParameterType::floating, -48.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "tensLU", "L.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "relLU", "L.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.1f, 10.0f, "ms" },
    { "outLU", "L.UP.OUT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "thLD", "L.DN.THR", ParameterType::floating, -48.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "tensLD", "L.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "relLD", "L.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.1f, 10.0f, "ms" },
    { "outLD", "L.DN.OUT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "thRU", "R.UP.THR", ParameterType::floating, -48.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "tensRU", "R.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "relRU", "R.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.1f, 10.0f, "ms" },
    { "outRU", "R.UP.OUT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "thRD", "R.DN.THR", ParameterType::floating, -48.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "tensRD", "R.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "relRD", "R.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.1f, 10.0f, "ms" },
    { "outRD", "R.DN.OUT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "delTa", "Delta", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
});

inline constexpr auto fullbandAutomationSpecs = std::to_array<ParameterSpec>({
    { "autoInGn", "ENV FULLBAND INPUT-GAIN", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "autoInRight", "ENV FULLBAND IN-RIGHT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "autoInLeft", "ENV FULLBAND IN-LEFT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
});

inline constexpr auto fullbandVisibleSpecs = std::to_array<ParameterSpec>({
    { "inGnVisible", "Fullband In Gain", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "outGnVisible", "Fullband Out Gain", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "wideVisible", "Fullband Wide", ParameterType::floating, -100.0f, 400.0f, 0.1f, 100.0f, "%" },
});

inline constexpr auto crossoverSpecs = std::to_array<ParameterSpec>({
    { "xover1", "Crossover 1", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 134.0f, "Hz" },
    { "xover2", "Crossover 2", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 523.0f, "Hz" },
    { "xover3", "Crossover 3", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 2093.0f, "Hz" },
    { "xover4", "Crossover 4", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 5000.0f, "Hz" },
    { "xover5", "Crossover 5", ParameterType::floating, 20.0f, 20000.0f, 1.0f, 10000.0f, "Hz" },
});

static_assert(parameterSpecs.size() == numParameterSlots);
static_assert(fullbandVisibleSpecs.size() == numFullbandVisibleSlots);
static_assert(fullbandAutomationSpecs.size() == numFullbandAutomationSlots);
static_assert(crossoverSpecs.size() == numCrossoverSlots);

constexpr size_t toIndex(const ParameterSlot slot)
{
    return static_cast<size_t>(slot);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace mxe::parameters
