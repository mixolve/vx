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
    thLU,
    mkLU,
    thLD,
    mkLD,
    thRU,
    mkRU,
    thRD,
    mkRD,
    hwBypass,
    linkUpDn,
    LLThResh,
    LLTension,
    LLRelease,
    LLmk,
    RRThResh,
    RRTension,
    RRRelease,
    RRmk,
    DMbypass,
    linkLr,
    FFThResh,
    FFTension,
    FFRelease,
    FFmk,
    FFbypass,
    moRph,
    peakHoldHz,
    TensionFlooR,
    TensionHysT,
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
    { "inGn", "Input Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "inRight", "Input Right", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "inLeft", "Input Left", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "autoInGn", "AUTO INPUT-GAIN", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "autoInRight", "AUTO IN-RIGHT", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "autoInLeft", "AUTO IN-LEFT", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "wide", "Wide", ParameterType::floating, -100.0f, 400.0f, 0.1f, 100.0f, "%" },
    { "outGn", "Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "thLU", "L Up Thresh", ParameterType::floating, -120.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "mkLU", "L Up Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "thLD", "L Down Thresh", ParameterType::floating, -120.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "mkLD", "L Down Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "thRU", "R Up Thresh", ParameterType::floating, -120.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "mkRU", "R Up Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "thRD", "R Down Thresh", ParameterType::floating, -120.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "mkRD", "R Down Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "hwBypass", "HW Bypass", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "linkUpDn", "Link Up/Down", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "LLThResh", "LL Thresh", ParameterType::floating, -120.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "LLTension", "LL Tension", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "LLRelease", "LL Release", ParameterType::floating, 0.0f, 1000.0f, 0.1f, 0.0f, "ms" },
    { "LLmk", "LL Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "RRThResh", "RR Thresh", ParameterType::floating, -120.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "RRTension", "RR Tension", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "RRRelease", "RR Release", ParameterType::floating, 0.0f, 1000.0f, 0.1f, 0.0f, "ms" },
    { "RRmk", "RR Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "DMbypass", "DM Bypass", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "linkLr", "Link L/R", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "FFThResh", "FF Thresh", ParameterType::floating, -120.0f, 0.0f, 0.1f, 0.0f, "dB" },
    { "FFTension", "FF Tension", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "FFRelease", "FF Release", ParameterType::floating, 0.0f, 1000.0f, 0.1f, 0.0f, "ms" },
    { "FFmk", "FF Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "FFbypass", "FF Bypass", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "moRph", "Morph", ParameterType::floating, 0.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "peakHoldHz", "Peak Hold", ParameterType::floating, 21.0f, 3675.1f, 0.1f, 100.0f, "Hz" },
    { "TensionFlooR", "Tension Floor", ParameterType::floating, -96.0f, 0.0f, 0.1f, -96.0f, "dB" },
    { "TensionHysT", "Tension Hysteresis", ParameterType::floating, 0.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "delTa", "Delta", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
});

inline constexpr auto fullbandAutomationSpecs = std::to_array<ParameterSpec>({
    { "autoInGn", "ENV FULLBAND INPUT-GAIN", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "autoInRight", "ENV FULLBAND IN-RIGHT", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "autoInLeft", "ENV FULLBAND IN-LEFT", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
});

inline constexpr auto fullbandVisibleSpecs = std::to_array<ParameterSpec>({
    { "inGnVisible", "Fullband In Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
    { "outGnVisible", "Fullband Out Gain", ParameterType::floating, -24.0f, 24.0f, 0.1f, 0.0f, "dB" },
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
