#pragma once

#include <JuceHeader.h>

void preserveEditorWindowAndVisualizerState(juce::XmlElement& targetStateElement,
                                            const juce::ValueTree& sourceState);

void stripEditorWindowAndVisualizerState(juce::XmlElement& stateElement);
void stripEditorWindowAndVisualizerState(juce::ValueTree& state);
