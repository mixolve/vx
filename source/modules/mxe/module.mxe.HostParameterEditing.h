#pragma once

#include <JuceHeader.h>

namespace mxe::editor
{
bool setNormalisedParameterValueForHost(juce::RangedAudioParameter& parameter,
                                        float normalisedValue,
                                        juce::UndoManager* undoManager = nullptr);
}
