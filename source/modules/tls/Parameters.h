#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstddef>

namespace tls::parameters
{
enum class ParameterType
{
    floating,
    boolean,
    choice,
};

enum class ParameterSlot : size_t
{
    gainMid,
    gainMidMute,
    gainSide,
    gainSideMute,
    gainL,
    gainLMute,
    gainR,
    gainRMute,
    gainLr,
    gainLrMute,
    gainLOrder,
    gainROrder,
    gainMidOrder,
    gainSideOrder,
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
    listenLc,
    listenRc,
    listenMc,
    listenSc,
    listenLl,
    listenRr,
    listenSs,
    stereoDelay,
    leftDelay,
    rightDelay,
    leftPhase,
    rightPhase,
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
    int displayDecimals = 2;
    bool muteAtMinimum = false;
};

inline constexpr auto parameterSpecs = std::to_array<ParameterSpec>({
    { "gainMid", "MID", ParameterType::floating, -99.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "gainMidMute", "MID MUTE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "gainSide", "SIDE", ParameterType::floating, -99.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "gainSideMute", "SIDE MUTE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "gainL", "LEFT", ParameterType::floating, -99.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "gainLMute", "LEFT MUTE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "gainR", "RIGHT", ParameterType::floating, -99.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "gainRMute", "RIGHT MUTE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "gainLr", "STEREO", ParameterType::floating, -99.0f, 48.0f, 0.01f, 0.0f, "dB" },
    { "gainLrMute", "STEREO MUTE", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "gainLOrder", "LEFT ORDER", ParameterType::floating, 0.0f, 3.0f, 1.0f, 0.0f, "", 0 },
    { "gainROrder", "RIGHT ORDER", ParameterType::floating, 0.0f, 3.0f, 1.0f, 1.0f, "", 0 },
    { "gainMidOrder", "MID ORDER", ParameterType::floating, 0.0f, 3.0f, 1.0f, 2.0f, "", 0 },
    { "gainSideOrder", "SIDE ORDER", ParameterType::floating, 0.0f, 3.0f, 1.0f, 3.0f, "", 0 },
    { "halfPositive", "HPOS", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "halfNegative", "HNEG", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "fullPositive", "FPOS", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "fullNegative", "FNEG", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "left", "LEFT", ParameterType::floating, -100.0f, 100.0f, 0.01f, -100.0f, "%" },
    { "right", "RIGHT", ParameterType::floating, -100.0f, 100.0f, 0.01f, 100.0f, "%" },
    { "law", "LAW", ParameterType::floating, 0.0f, 6.0f, 0.01f, 0.0f, "dB", 2 },
    { "impact", "IMPACT", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "impactDirection", "DIRECTION", ParameterType::choice, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "mid", "MID", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "side", "SIDE", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "%" },
    { "degree", "DEGREE", ParameterType::floating, 0.0f, 359.99f, 0.01f, 0.0f, "deg" },
    { "flipRight", "FLIP RIGHT", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenLc", "LC", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenRc", "RC", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenMc", "MC", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenSc", "SC", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenLl", "LL", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenRr", "RR", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "listenSs", "SS", ParameterType::boolean, 0.0f, 1.0f, 1.0f, 0.0f, "" },
    { "stereoDelay", "STEREO", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "ms", 2 },
    { "leftDelay", "LEFT", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "ms", 2 },
    { "rightDelay", "RIGHT", ParameterType::floating, -100.0f, 100.0f, 0.01f, 0.0f, "ms", 2 },
    { "leftPhase", "PHASE L", ParameterType::floating, -180.0f, 180.0f, 0.01f, 0.0f, "deg" },
    { "rightPhase", "PHASE R", ParameterType::floating, -180.0f, 180.0f, 0.01f, 0.0f, "deg" },
});

static_assert(parameterSpecs.size() == numParameterSlots);

constexpr size_t toIndex(const ParameterSlot slot)
{
    return static_cast<size_t>(slot);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace tls::parameters
