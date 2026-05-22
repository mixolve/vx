#include "shell.Editor.h"

bool VxAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VxAudioProcessor::createEditor()
{
    return new VxAudioProcessorEditor(*this);
}
