#include "shell.Processor.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"
#include "../modules/mxe/module.mxe.PluginProcessor.h"
#include "../modules/spe/module.spe.SpeProcessor.h"
#include "../modules/tse/module.tse.TseProcessor.h"

#include <cmath>
#include <memory>
#include <vector>

namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* label;
};

inline constexpr auto shellGlobalMiscOrder = std::to_array<ParameterOrderEntry>({
    { "bypass", "BYPASS" },
    { "bypass_wt_gain", "BYPASS.WT-GAIN" },
    { "in_gain_lr", "IN-GAIN-LR" },
    { "in_gain_l", "IN-GAIN-L" },
    { "in_gain_r", "IN-GAIN-R" },
    { "in_wide", "IN-WIDE" },
    { "out_gain", "OUT-GAIN" },
});
}

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
    mxeModuleProcessors[0] = std::make_unique<MxeAudioProcessor>(*this);
    tseModuleProcessors.resize(1);
    tseModuleProcessors[0] = std::make_unique<TseModuleProcessor>(*this);

    globalGainLParam = parameters.getRawParameterValue(paramGlobalGainLId);
    globalGainRParam = parameters.getRawParameterValue(paramGlobalGainRId);
    globalGainLrParam = parameters.getRawParameterValue(paramGlobalGainLrId);
    globalWideParam = parameters.getRawParameterValue(paramGlobalWideId);
    outGainParam = parameters.getRawParameterValue(paramOutGainId);
    globalBypassParam = parameters.getRawParameterValue(paramGlobalBypassId);
    globalBypassOutGainOnlyParam = parameters.getRawParameterValue(paramGlobalBypassOutGainOnlyId);

    #if JUCE_DEBUG
    {
        auto hostSlotParameterCount = 0;

        for (int slotIndex = 0; slotIndex < hostAutomationSlotCount; ++slotIndex)
            if (getParameters().contains(parameters.getParameter(getHostSlotParameterId(slotIndex))))
                ++hostSlotParameterCount;

        juce::Logger::writeToLog("Vx host parameter export: total=" + juce::String(getParameters().size())
                                 + " host-slots=" + juce::String(hostSlotParameterCount));
    }
    #endif
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

juce::String VxAudioProcessor::getHostSlotParameterName(const int slotIndex)
{
    return "SLOT-" + getHostSlotLetterLabel(slotIndex);
}

juce::AudioProcessorValueTreeState::ParameterLayout VxAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    const auto makeShellGlobalName = [] (const juce::String& parameterName)
    {
        return "GLOBAL - MISC - " + parameterName;
    };

    for (const auto& entry : shellGlobalMiscOrder)
    {
        const auto key = juce::String(entry.key);
        const auto name = makeShellGlobalName(entry.label);

        if (key == "bypass")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramGlobalBypassId, 1 },
                name,
                false,
                juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));
            continue;
        }

        if (key == "bypass_wt_gain")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { paramGlobalBypassOutGainOnlyId, 1 },
                name,
                false,
                juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));
            continue;
        }

        if (key == "in_gain_lr")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramGlobalGainLrId, 1 },
                name,
                juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
                0.0f,
                juce::AudioParameterFloatAttributes().withAutomatable(false).withMeta(true).withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "in_gain_l")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramGlobalGainLId, 1 },
                name,
                juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
                0.0f,
                juce::AudioParameterFloatAttributes().withAutomatable(false).withMeta(true).withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "in_gain_r")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramGlobalGainRId, 1 },
                name,
                juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
                0.0f,
                juce::AudioParameterFloatAttributes().withAutomatable(false).withMeta(true).withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
            continue;
        }

        if (key == "in_wide")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramGlobalWideId, 1 },
                name,
                juce::NormalisableRange<float> { 0.0f, 400.0f, 0.01f },
                100.0f,
                juce::AudioParameterFloatAttributes().withAutomatable(false).withMeta(true).withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return juce::String::formatted("%.0f", static_cast<double>(value));
                    })));
            continue;
        }

        if (key == "out_gain")
        {
            parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { paramOutGainId, 1 },
                name,
                juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
                0.0f,
                juce::AudioParameterFloatAttributes().withAutomatable(false).withMeta(true).withStringFromValueFunction(
                    [] (float value, int)
                    {
                        return formatDecibelValue(value);
                    })));
        }
    }

    for (int slotIndex = 0; slotIndex < hostAutomationSlotCount; ++slotIndex)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getHostSlotParameterId(slotIndex), 2 },
            getHostSlotParameterName(slotIndex),
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
