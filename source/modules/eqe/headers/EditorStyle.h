#pragma once

#include "Editor.h"

inline constexpr int initialEditorWidth = 306;
inline constexpr int initialEditorHeight = 612;
inline constexpr int minimumEditorWidth = 306;
inline constexpr int minimumEditorHeight = 612;
inline constexpr int maximumEditorWidth = 4096;
inline constexpr int maximumEditorHeight = 4096;
#if ! JUCE_IOS
inline constexpr int visualizerPanelGap = 0;
inline constexpr int minimumVisualizerWidth = 420;
inline constexpr int defaultVisualizerWidth = 900;
#endif
inline constexpr float editorInsetSideRatio = 0.04f;
inline constexpr float editorInsetTopRatio = 0.06f;
inline constexpr float editorInsetBottomRatio = 0.04f;
inline constexpr int parameterGap = 8;
inline constexpr int verticalGap = 8;
inline constexpr int footerHeight = 30;
inline constexpr int globalToFilterGap = 8;
inline constexpr int addFilterToFooterGap = 8;
inline constexpr int addFilterToPresetsGap = 8;
inline constexpr int rowHeight = 30;
inline constexpr int presetRowGap = 8;
inline constexpr float uiFontSize = 22.0f;

inline const auto uiWhite = juce::Colour(0xffffffff);
inline const auto uiAccent = juce::Colour(0xff9999ff);
inline const auto uiClip = juce::Colour(0xffffcc99);
inline const auto uiGrey800 = juce::Colour(0xff242424);
inline const auto uiGrey700 = juce::Colour(0xff363636);
inline const auto uiGrey500 = juce::Colour(0xff707070);
inline const auto visualizerStereoColour = uiWhite;
inline const auto visualizerLeftColour = juce::Colour(0xff99cc99);
inline const auto visualizerRightColour = juce::Colour(0xffff9999);
inline const auto visualizerMidColour = juce::Colour(0xff9999ff);
inline const auto visualizerSideColour = juce::Colour(0xffffcc99);

int getEditorInsetX(int width);
juce::Typeface::Ptr getUiRegularTypeface();
juce::Typeface::Ptr getUiBoldTypeface();
juce::FontOptions makeUiFontOptions(int styleFlags = juce::Font::plain, float height = uiFontSize);
juce::Font makeUiFont(int styleFlags = juce::Font::plain, float height = uiFontSize);
juce::String formatFixedDecimalValue(double value, int decimalPlaces);
juce::Colour getDisplayTextColour(const juce::String& text);
bool tryParseNoteFrequency(const juce::String& text, double& frequency);
double parseNumericInput(const juce::String& text);
double parseFrequencyInput(const juce::String& text);
bool supportsNoteFrequencyInput(const juce::String& parameterId);
double findNearestChoiceIndex(double targetValue, const juce::StringArray& choices, const juce::String& enteredText);
void clearKeyboardFocus(juce::Component& component);
int getScaledParameterNameWidth(int rowWidth) noexcept;
int getScaledParameterValueWidth(int rowWidth) noexcept;
