#pragma once

#include <JuceHeader.h>

#include <memory>

class FftModuleProcessor;

namespace shell_setup_support
{
juce::String getMixolveInfoMarkdown();
std::unique_ptr<juce::Component> createFftAnalyserComponent(FftModuleProcessor& processor);
void refreshFftAnalyserComponent(juce::Component* component);
void removeOwnedChild(juce::Component& owner, std::unique_ptr<juce::Component>& child);
}
