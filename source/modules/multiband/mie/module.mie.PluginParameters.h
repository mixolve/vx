#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstddef>

namespace mie::parameters
{
enum class ParameterType
{
    floating,
    boolean,
};

enum class ParameterSlot : size_t
{
    gainMid,
    gainSide,
    gainL,
    gainR,
    gainLr,
    halfPositive,
    halfNegative,
    fullPositive,
    fullNegative,
    left,
    right,
    law,
    impact,
    impactDirection,
    mid,
    side,
    degree,
    flipRight,
    listenL,
    listenR,
    listenM,
    listenS,
    listenInPlace,
    depStereo,
    depRight,
    depBuffer,
    depPhaseL,
    depPhaseR,
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
    int displayDecimals = 1;
    bool muteAtMinimum = false;
};

inline constexpr auto parameterSpecs = std::to_array<ParameterSpec>({
    { "gainMid", "MID", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB", 1, true },
    { "gainSide", "SIDE", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB", 1, true },
    { "gainL", "LEFT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "gainR", "RIGHT", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "gainLr", "STEREO", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "halfPositive", "HALF POSITIVE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "halfNegative", "HALF NEGATIVE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "fullPositive", "FULL POSITIVE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "fullNegative", "FULL NEGATIVE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "left", "LEFT", ParameterType::floating, -100.0f, 100.0f, 0.1f, -100.0f, "%" },
    { "right", "RIGHT", ParameterType::floating, -100.0f, 100.0f, 0.1f, 100.0f, "%" },
    { "law", "LAW", ParameterType::floating, 0.0f, 6.0f, 0.01f, 0.0f, "dB", 2 },
    { "impact", "IMPACT", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "impactDirection", "TO LEFT CHANNEL/TO RIGHT CHANNEL", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "mid", "MID", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "side", "SIDE", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "degree", "DEGREE", ParameterType::floating, 0.0f, 359.9f, 0.1f, 0.0f, "deg" },
    { "flipRight", "FLIP RIGHT", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenL", "LEFT", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenR", "RIGHT", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenM", "MID", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenS", "SIDE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenInPlace", "IN PLACE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "depStereo", "DELAY ST", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "ms", 2 },
    { "depRight", "DELAY R", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "ms", 2 },
    { "depBuffer", "BUFFER", ParameterType::floating, 0.0f, 200.0f, 0.01f, 0.0f, "ms", 2 },
    { "depPhaseL", "PHASE L", ParameterType::floating, -180.0f, 180.0f, 0.1f, 0.0f, "deg" },
    { "depPhaseR", "PHASE R", ParameterType::floating, -180.0f, 180.0f, 0.1f, 0.0f, "deg" },
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
} // namespace mie::parameters
