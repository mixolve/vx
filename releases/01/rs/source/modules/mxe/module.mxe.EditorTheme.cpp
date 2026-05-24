#include "module.mxe.EditorTheme.h"

#include "module.mxe.ValueFormatting.h"

#include <cmath>

namespace mxe::editor
{
namespace
{
juce::Typeface::Ptr getUiRegularTypeface()
{
#if JUCE_TARGET_HAS_BINARY_DATA
    static const auto regularTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::SometypeMonoRegular_ttf,
                                                                                BinaryData::SometypeMonoRegular_ttfSize);
    return regularTypeface;
#else
    return {};
#endif
}

juce::Typeface::Ptr getUiBoldTypeface()
{
#if JUCE_TARGET_HAS_BINARY_DATA
    static const auto boldTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::SometypeMonoBold_ttf,
                                                                             BinaryData::SometypeMonoBold_ttfSize);
    return boldTypeface;
#else
    return {};
#endif
}
}

int getEditorInsetX(const int width) noexcept
{
    return juce::roundToInt(static_cast<float>(juce::jmax(0, width)) * editorInsetSideRatio);
}

int getNestedEditorInsetX(const int width) noexcept
{
    const auto insetScale = editorInsetSideRatio / (1.0f - (editorInsetSideRatio * 2.0f));
    return juce::roundToInt(static_cast<float>(juce::jmax(0, width)) * insetScale);
}

int getScaledParameterNameWidth(const int rowWidth) noexcept
{
    const auto usableWidth = juce::jmax(0, rowWidth - parameterGap);
    return usableWidth / 2;
}

juce::FontOptions makeUiFont(const int styleFlags, const float height)
{
#if JUCE_TARGET_HAS_BINARY_DATA
    if (auto typeface = ((styleFlags & juce::Font::bold) != 0 ? getUiBoldTypeface()
                                                              : getUiRegularTypeface()))
        return juce::FontOptions(typeface).withHeight(height);
#endif

    return juce::FontOptions("Sometype Mono", height, styleFlags);
}

juce::String formatValueBoxText(const double value)
{
    return mxe::formatting::formatDspValue(value);
}

juce::String makeBandName(const size_t bandIndex)
{
    return juce::String(static_cast<int>(bandIndex + 1));
}

juce::Colour bandColour(const size_t bandIndex)
{
    juce::ignoreUnused(bandIndex);
    return uiAccent;
}
} // namespace mxe::editor
