#include "shell.Processor.h"
#include "../modules/multiband/tls/module.tls.PluginProcessor.h"
#include "../modules/multiband/dyn/module.dyn.PluginProcessor.h"
#include "../modules/fft/module.fft.FftProcessor.h"
#include "../modules/multiband/trs/module.trs.TrsProcessor.h"

#include <memory>
#include <vector>

AvaAudioProcessor::AvaAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "ava_state", createParameterLayout())
{
    globalBypassParam = parameters.getRawParameterValue(paramGlobalBypassId);
    registerTlsDirectHostParameterListeners();
}

AvaAudioProcessor::~AvaAudioProcessor()
{
    unregisterTlsDirectHostParameterListeners();
}

juce::String AvaAudioProcessor::getHostSlotParameterId(const int slotIndex)
{
    const auto clampedSlot = juce::jlimit(0, hostAutomationSlotCount - 1, slotIndex);
    return juce::String(paramHostSlotPrefix) + juce::String::formatted("%02d", clampedSlot + 1);
}

juce::String AvaAudioProcessor::getHostSlotLetterLabel(const int slotIndex)
{
    const auto index = juce::jlimit(0, hostAutomationSlotCount - 1, slotIndex);
    const auto first = index / 26;
    const auto second = index % 26;

    return juce::String::charToString(static_cast<juce_wchar>('A' + first))
        + juce::String::charToString(static_cast<juce_wchar>('A' + second));
}

juce::String AvaAudioProcessor::getTlsDirectHostParameterId(const size_t bandIndex, const char* suffix)
{
    if (bandIndex == tlsWidebandListenHostIndex)
        return "tls_host_" + juce::String(suffix);

    return "tls_host_band" + juce::String(static_cast<int>(bandIndex + 1)) + "_" + juce::String(suffix);
}

juce::String AvaAudioProcessor::getTlsDirectHostParameterName(const size_t bandIndex, const char* suffix)
{
    const auto parameterSuffix = juce::String(suffix);

    if (bandIndex == tlsWidebandListenHostIndex)
        return "TLS / LISTEN / " + parameterSuffix.fromFirstOccurrenceOf("listen", false, false).toUpperCase();

    const auto bandName = "TLS / BAND " + juce::String(static_cast<int>(bandIndex + 1));

    return bandName + " / SOLO";
}

juce::AudioProcessorValueTreeState::ParameterLayout AvaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramGlobalBypassId, 1 },
        "AVA / GLOBAL / BYPASS",
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

    for (size_t bandIndex = 0; bandIndex < tlsDirectHostBandCount; ++bandIndex)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { getTlsDirectHostParameterId(bandIndex, "solo"), 1 },
            getTlsDirectHostParameterName(bandIndex, "solo"),
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(true)));
    }

    for (const auto* suffix : tlsWidebandListenParameterSuffixes)
        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { getTlsDirectHostParameterId(tlsWidebandListenHostIndex, suffix), 1 },
            getTlsDirectHostParameterName(tlsWidebandListenHostIndex, suffix),
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(true)));

    return { parameterLayout.begin(), parameterLayout.end() };
}

const juce::String AvaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AvaAudioProcessor::acceptsMidi() const
{
    return false;
}

bool AvaAudioProcessor::producesMidi() const
{
    return false;
}

bool AvaAudioProcessor::isMidiEffect() const
{
    return false;
}

double AvaAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AvaAudioProcessor::getNumPrograms()
{
    return 1;
}

int AvaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AvaAudioProcessor::setCurrentProgram(int)
{
}

const juce::String AvaAudioProcessor::getProgramName(int)
{
    return {};
}

void AvaAudioProcessor::changeProgramName(int, const juce::String&)
{
}
