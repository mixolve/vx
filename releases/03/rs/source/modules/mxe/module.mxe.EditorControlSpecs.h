#pragma once

#include "module.mxe.EditorTheme.h"

#include <array>
#include <functional>
#include <span>

namespace mxe::editor
{
using ValueConstraint = std::function<float(float)>;

struct ControlSpec
{
    const char* suffix = "";
    const char* label = "";
    bool isToggle = false;
    bool valueInputOnly = false;
    float dragNormalisedPerPixel = valueBoxDragNormalisedPerPixel;
    float wheelMultiplier = wheelStepMultiplier;
    uint32_t labelColour = 0;
};

struct SectionSpec
{
    const char* title = "";
    std::span<const ControlSpec> controls;
    bool startsExpanded = false;
    bool staysExpandedOnSelfClick = false;
};

inline constexpr auto globalControls = std::to_array<ControlSpec>({
    { "inGn", "INPUT-GAIN" },
    { "inRight", "IN-RIGHT" },
    { "inLeft", "IN-LEFT" },
    { "wide", "IN-WIDE" },
    { "moRph", "MORPH" },
    { "peakHoldHz", "PEAK-HOLD" },
    { "TensionFlooR", "TEN-FLOOR" },
    { "TensionHysT", "TEN-HYST" },
    { "delTa", "DELTA", true, false },
});

inline constexpr auto fullbandControls = std::to_array<ControlSpec>({
    { "inGnVisible", "IN-GAIN" },
    { "outGnVisible", "OUT-GAIN" },
});

inline constexpr auto crossoverControls = std::to_array<ControlSpec>({
    { "xover1", "XOVER-1", false, false, crossoverDragNormalisedPerPixel, crossoverWheelStepMultiplier },
    { "xover2", "XOVER-2", false, false, crossoverDragNormalisedPerPixel, crossoverWheelStepMultiplier },
    { "xover3", "XOVER-3", false, false, crossoverDragNormalisedPerPixel, crossoverWheelStepMultiplier },
    { "xover4", "XOVER-4", false, false, crossoverDragNormalisedPerPixel, crossoverWheelStepMultiplier },
    { "xover5", "XOVER-5", false, false, crossoverDragNormalisedPerPixel, crossoverWheelStepMultiplier },
});

inline constexpr std::array<ControlSpec, 0> moduleMiscControls {};

inline const auto moduleMiscSection = SectionSpec { "MISC", moduleMiscControls, false, false };
inline const auto globalSection = SectionSpec { "MISC", globalControls, false, false };
inline const auto fullbandSection = SectionSpec { "FULLBAND", fullbandControls, false, false };
inline const auto crossoverSection = SectionSpec { "CROSSOVER", crossoverControls, false, false };
} // namespace mxe::editor
