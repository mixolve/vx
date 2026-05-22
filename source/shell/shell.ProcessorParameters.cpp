#include "shell.Processor.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"
#include "../modules/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/tse/module.tse.TseProcessor.h"

#include <cmath>
#include <memory>
#include <vector>

VxAudioProcessor::VxAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "vx_state", createParameterLayout())
{
    eqeModuleProcessors.resize(1);
    eqeModuleProcessors[0] = std::make_unique<EqeModuleProcessor>(*this);
    speModuleProcessors.resize(1);
    speModuleProcessors[0] = std::make_unique<SpeModuleProcessor>(*this);
    mxeModuleProcessors.resize(1);
    mxeModuleProcessors[0] = std::make_unique<MxeAudioProcessor>();
    tseModuleProcessors.resize(1);
    tseModuleProcessors[0] = std::make_unique<TseModuleProcessor>(*this);

    globalGainLParam = parameters.getRawParameterValue(paramGlobalGainLId);
    globalGainRParam = parameters.getRawParameterValue(paramGlobalGainRId);
    globalWideParam = parameters.getRawParameterValue(paramGlobalWideId);
    outGainParam = parameters.getRawParameterValue(paramOutGainId);
    globalBypassParam = parameters.getRawParameterValue(paramGlobalBypassId);
    globalBypassOutGainOnlyParam = parameters.getRawParameterValue(paramGlobalBypassOutGainOnlyId);
}

VxAudioProcessor::~VxAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout VxAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramGlobalGainLId, 1 },
        "GLOBAL - GAIN.L",
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramGlobalGainRId, 1 },
        "GLOBAL - GAIN.R",
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramGlobalWideId, 1 },
        "GLOBAL - WIDE",
        juce::NormalisableRange<float> { 0.0f, 400.0f, 0.01f },
        100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return juce::String::formatted("%.0f", static_cast<double>(value));
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramOutGainId, 1 },
        "GLOBAL - GAIN.OUT",
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramGlobalBypassId, 1 },
        "GLOBAL - BYPASS",
        false));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramGlobalBypassOutGainOnlyId, 1 },
        "GLOBAL - BYPASS.WT-GAIN",
        false));

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
