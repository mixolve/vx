#pragma once

#include "UiConstants.h"

#include <JuceHeader.h>

#if JUCE_IOS
inline constexpr int initialEditorWidth = 360;
inline constexpr int minimumEditorWidth = 250;
inline constexpr int maximumEditorWidth = 4096;
#else
inline constexpr int initialEditorWidth = 300;
inline constexpr int minimumEditorWidth = initialEditorWidth;
inline constexpr int maximumEditorWidth = initialEditorWidth;
#endif
inline constexpr int initialEditorHeight = 650;
inline constexpr int minimumEditorHeight = 650;
inline constexpr int maximumEditorHeight = 4096;
inline constexpr float editorInsetSideRatio = 0.04f;
inline constexpr float editorInsetTopRatio = 0.06f;
inline constexpr float editorInsetBottomRatio = 0.04f;
inline constexpr int parameterGap = uiGap;
inline constexpr int verticalGap = uiGap;
inline constexpr int moduleContentBottomGap = verticalGap;
inline constexpr int viewportToPotentiometerGap = verticalGap * 2;
inline constexpr int footerHeight = 30;
inline constexpr int globalToFilterGap = uiGap;
inline constexpr int addFilterToFooterGap = uiGap;
inline constexpr int addFilterToPresetsGap = uiGap;
inline constexpr int rowHeight = 30;
inline constexpr int fftInlineAnalyserHeight = rowHeight * fftInlineAnalyserHeightRows;
inline constexpr int presetRowGap = uiGap;
inline constexpr float uiFontSize = 22.0f;

inline const auto uiWhite = juce::Colour(0xffffffff);
inline const auto uiAccent = juce::Colour(0xff9999ff);
inline const auto uiClip = juce::Colour(0xffffcc99);
inline const auto uiPopup = juce::Colour(0xbf444444);
inline const auto uiPopupPanel = juce::Colour(0xff444444);
inline const auto uiGrey800 = juce::Colour(0xff333333);
inline const auto uiGrey700 = juce::Colour(0xff666666);
inline const auto uiGrey500 = juce::Colour(0xff666666);
inline const auto analyserLeftColour = juce::Colour(0xff99cc99);
inline const auto analyserRightColour = juce::Colour(0xffff9999);
inline const auto analyserPhaseColour = juce::Colour(0xffffcc99);

int getEditorInsetX(int width);
int getEditorInsetTop(int height);
int getEditorInsetBottom(int height);
juce::Typeface::Ptr getUiRegularTypeface();
juce::FontOptions makeUiFontOptions();
juce::Font makeUiFont();
int getTextPixelWidth(const juce::Font& font, const juce::String& text);
bool drawLoopingText(juce::Graphics& graphics,
                     const juce::String& text,
                     const juce::Rectangle<int>& bounds,
                     const juce::Font& font,
                     juce::Justification justification = juce::Justification::centred);
juce::String formatFixedDecimalValue(double value, int decimalPlaces);
juce::Colour getDisplayTextColour(const juce::String& text);
bool tryParseNoteFrequency(const juce::String& text, double& frequency);
double parseNumericInput(const juce::String& text);
double parseFrequencyInput(const juce::String& text);
bool supportsNoteFrequencyInput(const juce::String& parameterId);
double findNearestChoiceIndex(double targetValue, const juce::StringArray& choices, const juce::String& enteredText);
void clearKeyboardFocus(juce::Component& component);
int getScaledParameterNameWidth(int rowWidth) noexcept;
