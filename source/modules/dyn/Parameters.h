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
    linkUpDown,
    linkLeftRight,
    linkOpposite,
    leftUpThreshold,
    leftUpTension,
    leftUpRelease,
    leftUpOutput,
    leftDownThreshold,
    leftDownTension,
    leftDownRelease,
    leftDownOutput,
    rightUpThreshold,
    rightUpTension,
    rightUpRelease,
    rightUpOutput,
    rightDownThreshold,
    rightDownTension,
    rightDownRelease,
    rightDownOutput,
    delta,
    count
};

inline constexpr size_t numParameterSlots = static_cast<size_t>(ParameterSlot::count);

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
    { "linkUpDown", "Link UP/DN (Dual-Mono)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "linkLeftRight", "Link L/R (Stereo)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "linkOpposite", "Link Opp", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "leftUpThreshold", "L.UP.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "leftUpTension", "L.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "leftUpRelease", "L.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "leftUpOutput", "L.UP.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "leftDownThreshold", "L.DN.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "leftDownTension", "L.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "leftDownRelease", "L.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "leftDownOutput", "L.DN.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "rightUpThreshold", "R.UP.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "rightUpTension", "R.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "rightUpRelease", "R.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "rightUpOutput", "R.UP.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "rightDownThreshold", "R.DN.THR", ParameterType::floating, -48.0f, 0.0f, 0.01f, 0.0f, "dB" },
    { "rightDownTension", "R.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "rightDownRelease", "R.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "rightDownOutput", "R.DN.OUT", ParameterType::floating, -48.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "delta", "Delta", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
});

static_assert(parameterSpecs.size() == numParameterSlots);

constexpr size_t toIndex(const ParameterSlot slot)
{
    return static_cast<size_t>(slot);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace dyn::parameters
