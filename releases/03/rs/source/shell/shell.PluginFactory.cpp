#include "shell.Processor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VxAudioProcessor();
}
