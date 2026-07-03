#pragma once

#include <JuceHeader.h>

#include <memory>

class SpeModuleProcessor;

namespace shell_setup_support
{
juce::String getMixolveInfoMarkdown();
std::unique_ptr<juce::Component> createSpeAnalyserComponent(SpeModuleProcessor& processor);
void refreshSpeAnalyserComponent(juce::Component* component);
void removeOwnedChild(juce::Component& owner, std::unique_ptr<juce::Component>& child);
}
