#pragma once

#include <JuceHeader.h>

namespace eql_header_renderer
{
bool drawChannelTokenHighlight(juce::Graphics& graphics,
                               const juce::String& text,
                               const juce::Rectangle<int>& bounds,
                               const juce::Font& font,
                               juce::Justification justification);
bool drawFilterHeaderHighlight(juce::Graphics& graphics,
                               const juce::String& text,
                               const juce::Rectangle<int>& bounds,
                               const juce::Font& font,
                               juce::Justification justification);
}
