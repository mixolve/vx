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
    ratio,
    knee,
    peakHoldMs,
    lookahead,
    tensionFloor,
    tensionHysteresis,
    releaseForm,
    releaseCurve,
    adaptiveOffset,
    adaptiveAttack,
    adaptiveHold,
    adaptiveRelease,
    linkUpDown,
    linkLeftRight,
    linkOpposite,
    leftUpThreshold,
    leftUpAdaptive,
    leftUpTension,
    leftUpRelease,
    leftUpOutput,
    leftDownThreshold,
    leftDownAdaptive,
    leftDownTension,
    leftDownRelease,
    leftDownOutput,
    rightUpThreshold,
    rightUpAdaptive,
    rightUpTension,
    rightUpRelease,
    rightUpOutput,
    rightDownThreshold,
    rightDownAdaptive,
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
    { "ratio", "Ratio", ParameterType::floating, 1.0f, 100.0f, 0.01f, 100.0f, ":1" },
    { "knee", "Knee", ParameterType::floating, 0.0f, 24.0f, 0.01f, 0.0f, "dB" },
    { "peak_hold", "Peak Hold", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "ms" },
    { "lookahead", "Lookahead", ParameterType::floating, 0.0f, 24.0f, 0.01f, 0.0f, "ms" },
    { "tension_floor", "Tension Floor", ParameterType::floating, -96.0f, 0.0f, 0.01f, -96.0f, "dB" },
    { "tension_hysteresis", "Tension Hysteresis", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "release_form", "Release Form", ParameterType::choice, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "release_curve", "Release Curve", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "adaptive_offset", "Adaptive Offset", ParameterType::floating, 0.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "adaptive_attack", "Adaptive Attack", ParameterType::floating, 0.0f, 200.0f, 1.0f, 30.0f, "ms" },
    { "adaptive_hold", "Adaptive Hold", ParameterType::floating, 0.0f, 2000.0f, 1.0f, 0.0f, "ms" },
    { "adaptive_release", "Adaptive Release", ParameterType::floating, 0.0f, 2000.0f, 1.0f, 300.0f, "ms" },
    { "linkUpDown", "Link UP/DN (Dual-Mono)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "linkLeftRight", "Link L/R (Stereo)", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "linkOpposite", "Link Opp", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 1.0f, "" },
    { "leftUpThreshold", "L.UP.THR", ParameterType::floating, -96.0f, 12.0f, 0.01f, 0.0f, "dB" },
    { "leftUpAdaptive", "L.UP.ADAP", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "leftUpTension", "L.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "leftUpRelease", "L.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "leftUpOutput", "L.UP.OUT", ParameterType::floating, -96.0f, 96.0f, 0.01f, 0.0f, "dB" },
    { "leftDownThreshold", "L.DN.THR", ParameterType::floating, -96.0f, 12.0f, 0.01f, 0.0f, "dB" },
    { "leftDownAdaptive", "L.DN.ADAP", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "leftDownTension", "L.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "leftDownRelease", "L.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "leftDownOutput", "L.DN.OUT", ParameterType::floating, -96.0f, 96.0f, 0.01f, 0.0f, "dB" },
    { "rightUpThreshold", "R.UP.THR", ParameterType::floating, -96.0f, 12.0f, 0.01f, 0.0f, "dB" },
    { "rightUpAdaptive", "R.UP.ADAP", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "rightUpTension", "R.UP.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "rightUpRelease", "R.UP.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "rightUpOutput", "R.UP.OUT", ParameterType::floating, -96.0f, 96.0f, 0.01f, 0.0f, "dB" },
    { "rightDownThreshold", "R.DN.THR", ParameterType::floating, -96.0f, 12.0f, 0.01f, 0.0f, "dB" },
    { "rightDownAdaptive", "R.DN.ADAP", ParameterType::floating, 0.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "rightDownTension", "R.DN.TENS", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "rightDownRelease", "R.DN.REL", ParameterType::floating, 0.0f, 1000.0f, 0.01f, 10.0f, "ms" },
    { "rightDownOutput", "R.DN.OUT", ParameterType::floating, -96.0f, 96.0f, 0.01f, 0.0f, "dB" },
    { "delta", "Delta", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
});

static_assert(parameterSpecs.size() == numParameterSlots);

constexpr size_t toIndex(const ParameterSlot slot)
{
    return static_cast<size_t>(slot);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace dyn::parameters
