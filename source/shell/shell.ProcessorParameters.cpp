#include "shell.Processor.h"
#include "../modules/multiband/tls/module.tls.PluginProcessor.h"
#include "../modules/multiband/dyn/module.dyn.PluginProcessor.h"
#include "../modules/fft/module.fft.FftProcessor.h"
#include "../modules/multiband/trs/module.trs.TrsProcessor.h"

#include <memory>
#include <vector>

VxAudioProcessor::VxAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "vx_state", createParameterLayout())
{
    globalBypassParam = parameters.getRawParameterValue(paramGlobalBypassId);
}

VxAudioProcessor::~VxAudioProcessor() = default;

juce::String VxAudioProcessor::getHostSlotParameterId(const int slotIndex)
{
    const auto clampedSlot = juce::jlimit(0, hostAutomationSlotCount - 1, slotIndex);
    return juce::String(paramHostSlotPrefix) + juce::String::formatted("%02d", clampedSlot + 1);
}

juce::String VxAudioProcessor::getHostSlotLetterLabel(const int slotIndex)
{
    const auto index = juce::jlimit(0, hostAutomationSlotCount - 1, slotIndex);
    const auto first = index / 26;
    const auto second = index % 26;

    return juce::String::charToString(static_cast<juce_wchar>('A' + first))
        + juce::String::charToString(static_cast<juce_wchar>('A' + second));
}

juce::AudioProcessorValueTreeState::ParameterLayout VxAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramGlobalBypassId, 1 },
        "VX / GLOBAL / BYPASS",
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));

    for (int slotIndex = 0; slotIndex < hostAutomationSlotCount; ++slotIndex)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getHostSlotParameterId(slotIndex), 2 },
            getHostSlotLetterLabel(slotIndex),
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f },
            0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true)));
    }

    return { parameterLayout.begin(), parameterLayout.end() };
}

const juce::String VxAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VxAudioProcessor::acceptsMidi() const
{
    return false;
}

bool VxAudioProcessor::producesMidi() const
{
    return false;
}

bool VxAudioProcessor::isMidiEffect() const
{
    return false;
}

double VxAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VxAudioProcessor::getNumPrograms()
{
    return 1;
}

int VxAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VxAudioProcessor::setCurrentProgram(int)
{
}

const juce::String VxAudioProcessor::getProgramName(int)
{
    return {};
}

void VxAudioProcessor::changeProgramName(int, const juce::String&)
{
}
