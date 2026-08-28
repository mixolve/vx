#pragma once

inline constexpr int uiGap = 8;
inline constexpr float uiGapFloat = static_cast<float>(uiGap);
inline constexpr int uiGapDouble = uiGap * 2;
inline constexpr int uiGapTriple = uiGap * 3;

inline constexpr int internalFrameInsetX = uiGap;
inline constexpr int internalFrameInsetY = uiGap;
inline constexpr int contentFrameInsetX = internalFrameInsetX;
inline constexpr int contentFrameInsetY = internalFrameInsetY;
inline constexpr int frameLineThickness = 1;
inline constexpr int fftInlineAnalyserHeightRows = 3;
