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
};

struct SectionSpec
{
    const char* title = "";
    std::span<const ControlSpec> controls;
    bool startsExpanded = false;
    bool staysExpandedOnSelfClick = false;
};

inline constexpr auto halfWaveControls = std::to_array<ControlSpec>({
    { "hwBypass", "BYPASS", true, false },
    { "thLU", "L.UP.THR" },
    { "mkLU", "L.UP.OUT" },
    { "thLD", "L.DN.THR" },
    { "mkLD", "L.DN.OUT" },
    { "thRU", "R.UP.THR" },
    { "mkRU", "R.UP.OUT" },
    { "thRD", "R.DN.THR" },
    { "mkRD", "R.DN.OUT" },
});

inline constexpr auto dmControls = std::to_array<ControlSpec>({
    { "DMbypass", "BYPASS", true, false },
    { "LLThResh", "L.THR" },
    { "LLTension", "L.TENS" },
    { "LLRelease", "L.REL" },
    { "LLmk", "L.OUT" },
    { "RRThResh", "R.THR" },
    { "RRTension", "R.TENS" },
    { "RRRelease", "R.REL" },
    { "RRmk", "R.OUT" },
});

inline constexpr auto ffControls = std::to_array<ControlSpec>({
    { "FFbypass", "BYPASS", true, false },
    { "FFThResh", "THRESH" },
    { "FFTension", "TENSION" },
    { "FFRelease", "RELEASE" },
    { "FFmk", "OUT-GAIN" },
});

inline constexpr auto globalControls = std::to_array<ControlSpec>({
    { "inGn", "INPUT-GAIN" },
    { "inRight", "IN-RIGHT" },
    { "inLeft", "IN-LEFT" },
    { "wide", "WIDE" },
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
inline const auto halfWaveSection = SectionSpec { "HALF-WAVE", halfWaveControls, false, false };
inline const auto dmSection = SectionSpec { "DUAL-MONO", dmControls, false, false };
inline const auto ffSection = SectionSpec { "STEREO", ffControls, false, false };
inline const auto globalSection = SectionSpec { "MISC", globalControls, false, false };
inline const auto fullbandSection = SectionSpec { "FULLBAND", fullbandControls, false, false };
inline const auto crossoverSection = SectionSpec { "CROSSOVER", crossoverControls, false, false };
} // namespace mxe::editor
