#pragma once

#include <JuceHeader.h>

void preserveEditorWindowAndVisualizerState(juce::XmlElement& targetStateElement,
                                            const juce::ValueTree& sourceState);
