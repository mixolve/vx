#include "EqlHeaderRenderer.h"
#include "UiStyle.h"

#include <utility>
#include <vector>

namespace eql_header_renderer
{
const juce::Colour leftTokenColour { 0xFF99CC99 };
const juce::Colour rightTokenColour { 0xFFFF9999 };
const juce::Colour midTokenColour { 0xFF99CCCC };
const juce::Colour sideTokenColour { 0xFFFFCC99 };
const juce::Colour volumeTokenColour { 0xFF9999FF };

static juce::Colour colourForEqlFilterHeaderToken(const juce::String& token)
{
    if (token == "LL")
        return leftTokenColour;
    if (token == "RR")
        return rightTokenColour;
    if (token == "MM")
        return midTokenColour;
    if (token == "SS")
        return sideTokenColour;
    if (token == "VOL")
        return volumeTokenColour;

    return uiWhite;
}

struct ColouredTextSegment
{
    juce::String text;
    juce::Colour colour;
};

bool drawChannelTokenHighlight(juce::Graphics& graphics,
                               const juce::String& text,
                               const juce::Rectangle<int>& bounds,
                               const juce::Font& font,
                               const juce::Justification justification)
{
    if (! text.containsChar('.'))
        return false;

    juce::AttributedString attributed;
    attributed.setJustification(justification);

    auto foundToken = false;
    auto startIndex = 0;

    while (startIndex < text.length())
    {
        const auto dotIndex = text.indexOfChar(startIndex, '.');
        const auto tokenEnd = dotIndex >= 0 ? dotIndex : text.length();
        const auto token = text.substring(startIndex, tokenEnd);
        auto colour = uiWhite;

        if (token == "L")
        {
            colour = leftTokenColour;
            foundToken = true;
        }
        else if (token == "R")
        {
            colour = rightTokenColour;
            foundToken = true;
        }

        attributed.append(token, font, colour);

        if (dotIndex >= 0)
            attributed.append(".", font, uiWhite);

        startIndex = tokenEnd + 1;
    }

    if (! foundToken)
        return false;

    juce::TextLayout layout;
    layout.createLayout(attributed, static_cast<float>(bounds.getWidth()));
    layout.draw(graphics, bounds.toFloat());
    return true;
}

static void drawFittedSingleLineSegments(juce::Graphics& graphics,
                                  const std::vector<ColouredTextSegment>& segments,
                                  const juce::Rectangle<int>& bounds,
                                  const juce::Font& font,
                                  const juce::Justification justification)
{
    auto totalWidth = 0.0f;

    for (const auto& segment : segments)
        totalWidth += static_cast<float>(getTextPixelWidth(font, segment.text));

    if (totalWidth <= 0.0f || bounds.isEmpty())
        return;

    auto segmentsToDraw = segments;
    auto fittedWidth = totalWidth;

    if (totalWidth > static_cast<float>(bounds.getWidth()))
    {
        static const juce::String ellipsis { "..." };
        const auto ellipsisWidth = static_cast<float>(getTextPixelWidth(font, ellipsis));
        auto remainingWidth = juce::jmax(0.0f, static_cast<float>(bounds.getWidth()) - ellipsisWidth);
        std::vector<ColouredTextSegment> truncatedSegments;

        for (const auto& segment : segments)
        {
            const auto segmentWidth = static_cast<float>(getTextPixelWidth(font, segment.text));

            if (segmentWidth <= remainingWidth)
            {
                truncatedSegments.push_back(segment);
                remainingWidth -= segmentWidth;
                continue;
            }

            auto keptLength = 0;

            for (auto length = 1; length <= segment.text.length(); ++length)
            {
                const auto candidate = segment.text.substring(0, length);

                if (static_cast<float>(getTextPixelWidth(font, candidate)) > remainingWidth)
                    break;

                keptLength = length;
            }

            if (keptLength > 0)
                truncatedSegments.push_back({ segment.text.substring(0, keptLength), segment.colour });

            break;
        }

        truncatedSegments.push_back({ ellipsis, uiWhite });
        segmentsToDraw = std::move(truncatedSegments);

        fittedWidth = 0.0f;
        for (const auto& segment : segmentsToDraw)
            fittedWidth += static_cast<float>(getTextPixelWidth(font, segment.text));
    }

    const auto horizontalFlags = justification.getOnlyHorizontalFlags();
    auto x = static_cast<float>(bounds.getX());

    if ((horizontalFlags & juce::Justification::horizontallyCentred) != 0)
        x += (static_cast<float>(bounds.getWidth()) - fittedWidth) * 0.5f;
    else if ((horizontalFlags & juce::Justification::right) != 0)
        x += static_cast<float>(bounds.getWidth()) - fittedWidth;

    const auto y = static_cast<float>(bounds.getY());
    const auto height = static_cast<float>(bounds.getHeight());

    for (const auto& segment : segmentsToDraw)
    {
        const auto segmentWidth = static_cast<float>(getTextPixelWidth(font, segment.text));
        graphics.setColour(segment.colour);
        graphics.setFont(font);
        graphics.drawText(segment.text,
                          juce::Rectangle<float>(x, y, segmentWidth + 1.0f, height),
                          juce::Justification::centredLeft,
                          false);
        x += segmentWidth;
    }
}

bool drawFilterHeaderHighlight(juce::Graphics& graphics,
                                  const juce::String& text,
                                  const juce::Rectangle<int>& bounds,
                                  const juce::Font& font,
                                  const juce::Justification justification)
{
    if (! text.containsChar('-'))
        return false;

    std::vector<ColouredTextSegment> segments;

    auto foundToken = false;
    auto startIndex = 0;

    while (startIndex < text.length())
    {
        const auto separatorIndex = text.indexOfChar(startIndex, '-');
        const auto tokenEnd = separatorIndex >= 0 ? separatorIndex : text.length();
        const auto token = text.substring(startIndex, tokenEnd);

        if (token == "PHL")
        {
            segments.push_back({ "PH", uiWhite });
            segments.push_back({ "L", leftTokenColour });
            foundToken = true;
        }
        else if (token == "PHR")
        {
            segments.push_back({ "PH", uiWhite });
            segments.push_back({ "R", rightTokenColour });
            foundToken = true;
        }
        else
        {
            const auto tokenColour = colourForEqlFilterHeaderToken(token);

            if (tokenColour != uiWhite)
                foundToken = true;

            segments.push_back({ token, tokenColour });
        }

        if (separatorIndex >= 0)
            segments.push_back({ "-", uiWhite });

        startIndex = tokenEnd + 1;
    }

    if (! foundToken)
        return false;

    drawFittedSingleLineSegments(graphics, segments, bounds, font, justification);
    return true;
}

} // namespace eql_header_renderer
