#pragma once

#include <JuceHeader.h>
#include "shell.Constants.h"

#include <cstddef>

namespace mxe::editor
{
inline constexpr int uiGap = ::uiGap;
inline constexpr int contentFrameInsetX = ::internalFrameInsetX;
inline constexpr int contentFrameInsetY = ::internalFrameInsetY;
inline constexpr int frameLineThickness = ::frameLineThickness;
inline constexpr int initialEditorWidth = 306;
inline constexpr int initialEditorHeight = 612;
inline constexpr int minimumEditorWidth = 306;
inline constexpr int minimumEditorHeight = 612;
inline constexpr float editorInsetSideRatio = 0.04f;
inline constexpr float editorInsetTopRatio = 0.06f;
inline constexpr float editorInsetBottomRatio = 0.04f;
inline constexpr int fixedEditorInsetX = 12;
inline constexpr int parameterGap = uiGap;
inline constexpr int verticalGap = uiGap;
inline constexpr int buttonHeight = 30;
inline constexpr int footerHeight = buttonHeight;
inline constexpr int pageColumnWidth = initialEditorWidth - (fixedEditorInsetX * 2);
inline constexpr int sectionWidth = pageColumnWidth;
inline constexpr int sectionContentInsetX = fixedEditorInsetX;
inline constexpr int sectionRowGap = uiGap;
inline constexpr int sectionHeaderToContentGap = sectionRowGap;
inline constexpr int controlRowWidth = sectionWidth - (sectionContentInsetX * 2);
inline constexpr int parameterColumnWidth = (controlRowWidth - parameterGap) / 2;
inline constexpr int parameterValueWidth = parameterColumnWidth;
inline constexpr int parameterNameWidth = parameterColumnWidth;
inline constexpr int monitorButtonGap = parameterGap;
inline constexpr int monitorRowOffsetY = 0;
inline constexpr float uiFontSize = 22.0f;
inline constexpr float valueBoxDragNormalisedPerPixel = 0.0050f;
inline constexpr float smoothWheelStepThreshold = 0.030f;
inline constexpr float wheelStepMultiplier = 2.0f;
inline constexpr float crossoverDragNormalisedPerPixel = 0.0005f;
inline constexpr float crossoverWheelStepMultiplier = 20.0f;
inline constexpr float crossoverMinGapHz = 1.0f;

inline const juce::Colour uiWhite { 0xffffffff };
inline const juce::Colour uiBlack { 0xff000000 };
inline const juce::Colour uiAccent { 0xff9999ff };
inline const juce::Colour uiGrey800 { 0xff242424 };
inline const juce::Colour uiGrey700 { 0xff363636 };
inline const juce::Colour uiGrey500 { 0xff707070 };

int getEditorInsetX(int width) noexcept;
int getNestedEditorInsetX(int width) noexcept;
int getScaledParameterNameWidth(int rowWidth) noexcept;
juce::FontOptions makeUiFont(int styleFlags = juce::Font::plain, float height = uiFontSize);
juce::String formatValueBoxText(double value);
juce::String makeBandName(size_t bandIndex);
juce::Colour bandColour(size_t bandIndex);
} // namespace mxe::editor
