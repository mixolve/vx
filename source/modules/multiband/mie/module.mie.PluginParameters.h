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
    wide,
    gainL,
    gainR,
    gainLr,
    rectPlus,
    rectMinus,
    rectFoldPlus,
    rectFoldMinus,
    panL,
    panR,
    law,
    shear,
    shearMode,
    midBal,
    sideBal,
    ortDegRotation,
    ortFlipR,
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
};

inline constexpr auto parameterSpecs = std::to_array<ParameterSpec>({
    { "wide", "WIDE", ParameterType::floating, -100.0f, 400.0f, 0.1f, 100.0f, "%" },
    { "gainL", "GAIN-L", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "gainR", "GAIN-R", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "gainLr", "GAIN-LR", ParameterType::floating, -48.0f, 48.0f, 0.1f, 0.0f, "dB" },
    { "rectPlus", "RECT+", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "rectMinus", "RECT-", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "rectFoldPlus", "RECTF+", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "rectFoldMinus", "RECTF-", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "panL", "PAN-L", ParameterType::floating, -100.0f, 100.0f, 0.1f, -100.0f, "%" },
    { "panR", "PAN-R", ParameterType::floating, -100.0f, 100.0f, 0.1f, 100.0f, "%" },
    { "law", "LAW", ParameterType::floating, 0.0f, 6.0f, 0.01f, 0.0f, "dB", 2 },
    { "shear", "SHEAR", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "shearMode", "TO-L/TO-R", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "midBal", "MID-BAL", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "sideBal", "SIDE-BAL", ParameterType::floating, -100.0f, 100.0f, 0.1f, 0.0f, "%" },
    { "ortDegRotation", "ORT-DEG", ParameterType::floating, 0.0f, 360.0f, 0.1f, 0.0f, "deg" },
    { "ortFlipR", "ORT-FLIP-R", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenL", "L", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenR", "R", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenM", "M", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenS", "S", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenInPlace", "INPL", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "depStereo", "D-STEREO", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "ms", 2 },
    { "depRight", "D-RIGHT", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "ms", 2 },
    { "depBuffer", "BUFFER", ParameterType::floating, 0.0f, 200.0f, 0.01f, 0.0f, "ms", 2 },
    { "depPhaseL", "PHASE-L", ParameterType::floating, -180.0f, 180.0f, 0.1f, 0.0f, "deg" },
    { "depPhaseR", "PHASE-R", ParameterType::floating, -180.0f, 180.0f, 0.1f, 0.0f, "deg" },
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
