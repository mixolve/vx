#pragma once

#include "shell.Editor.h"

#include <functional>
#include <memory>
#include <vector>

class SpeModuleProcessor;

namespace shell_setup_support
{
struct VisualizerMarkerData
{
    int bellIndex = -1;
    int displayNumber = 0;
    float frequency = 0.0f;
    int place = 0;
    bool selected = false;
};

juce::String getMixolveInfoMarkdown();
std::unique_ptr<juce::Component> createSectionFrameComponent(juce::Colour outline);
std::unique_ptr<juce::Component> createEqResponseVisualizerComponent(VxAudioProcessor& processor,
                                                                     std::function<void(int)> markerSelectCallback);
void refreshEqResponseVisualizerComponent(juce::Component* component,
                                          float rangeLowDb,
                                          float rangeHighDb,
                                          bool cursorEnabled,
                                          bool showStereo,
                                          bool showLeft,
                                          bool showRight,
                                          bool showMid,
                                          bool showSide,
                                          std::vector<VisualizerMarkerData> markers);
std::unique_ptr<juce::Component> createSpeAnalyserComponent(SpeModuleProcessor& processor);
void refreshSpeAnalyserComponent(juce::Component* component);
void removeOwnedChild(juce::Component& owner, std::unique_ptr<juce::Component>& child);
}
