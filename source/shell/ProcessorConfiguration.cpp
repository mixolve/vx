#include "Processor.h"
#include "../modules/eql/ProcessorBank.h"
#include "../modules/fft/ProcessorBank.h"
#include "../modules/tls/Processor.h"
#include "../modules/dyn/Processor.h"
#include "../modules/fft/Processor.h"
#include "../modules/trs/Processor.h"

#include <memory>
#include <utility>
#include <vector>

AvaAudioProcessor::AvaAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "ava_state", createParameterLayout())
{
    globalBypassParam = parameters.getRawParameterValue(paramGlobalBypassId);
    parameters.addParameterListener(paramCrossoverActiveSplitCountId, this);

    for (int slotIndex = 0; slotIndex < hostAutomationSlotCount; ++slotIndex)
        parameters.addParameterListener(getHostSlotParameterId(slotIndex), this);
}

AvaAudioProcessor::~AvaAudioProcessor()
{
    cancelPendingUpdate();
    parameters.removeParameterListener(paramCrossoverActiveSplitCountId, this);

    for (int slotIndex = 0; slotIndex < hostAutomationSlotCount; ++slotIndex)
        parameters.removeParameterListener(getHostSlotParameterId(slotIndex), this);

    clearActiveModuleStateListeners();
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

juce::String AvaAudioProcessor::getHostSlotTargetStateKey(const int slotIndex)
{
    const auto clampedSlot = juce::jlimit(0, hostAutomationSlotCount - 1, slotIndex);
    return "editor_host_slot_param_" + juce::String::formatted("%02d", clampedSlot + 1);
}

juce::String AvaAudioProcessor::getCrossoverParameterId(const char* suffix)
{
    return juce::String(paramCrossoverPrefix) + suffix;
}

juce::String AvaAudioProcessor::getCrossoverSoloParameterId(const size_t rangeIndex)
{
    return juce::String(paramCrossoverPrefix) + "soloRange" + juce::String(static_cast<int>(rangeIndex + 1));
}

ava::crossover::Settings AvaAudioProcessor::getCrossoverSettings() const noexcept
{
    ava::crossover::Settings settings;

    if (const auto* value = parameters.getRawParameterValue(paramCrossoverActiveSplitCountId))
        settings.activeSplitCount = static_cast<size_t>(juce::jlimit(0, 5, juce::roundToInt(value->load(std::memory_order_relaxed))));

    for (size_t index = 0; index < settings.splitFrequencies.size(); ++index)
    {
        const auto parameterId = getCrossoverParameterId(("xover" + juce::String(static_cast<int>(index + 1))).toRawUTF8());

        if (const auto* value = parameters.getRawParameterValue(parameterId))
            settings.splitFrequencies[index] = value->load(std::memory_order_relaxed);
    }

    for (size_t index = 0; index < settings.soloMask.size(); ++index)
    {
        if (const auto* value = parameters.getRawParameterValue(getCrossoverSoloParameterId(index)))
            settings.soloMask[index] = value->load(std::memory_order_relaxed) >= 0.5f;
    }

    return settings;
}

juce::RangedAudioParameter* AvaAudioProcessor::findHostSlotTarget(const juce::String& parameterId) noexcept
{
    const auto trimmedParameterId = parameterId.trim();

    if (trimmedParameterId.isEmpty())
        return nullptr;

    if (auto* parameter = parameters.getParameter(trimmedParameterId))
        return parameter;

    const auto findInModule = [&trimmedParameterId] (auto* processor) -> juce::RangedAudioParameter*
    {
        return processor != nullptr ? processor->getValueTreeState().getParameter(trimmedParameterId) : nullptr;
    };

    switch (activeModule.load(std::memory_order_acquire))
    {
        case ActiveModule::eql: return findInModule(getEqlModuleProcessor());
        case ActiveModule::fft: return findInModule(getFftModuleProcessor());
        case ActiveModule::tls: return findInModule(getTlsModuleProcessor());
        case ActiveModule::dyn: return findInModule(getDynModuleProcessor());
        case ActiveModule::trs: return findInModule(getTrsModuleProcessor());
        case ActiveModule::none: break;
    }

    return nullptr;
}

void AvaAudioProcessor::applyHostSlotValue(const int slotIndex, const float normalizedValue) noexcept
{
    const auto targetParameterId = parameters.state.getProperty(getHostSlotTargetStateKey(slotIndex)).toString().trim();

    if (auto* targetParameter = findHostSlotTarget(targetParameterId))
        targetParameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
}

juce::AudioProcessorValueTreeState::ParameterLayout AvaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;

    for (int slotIndex = 0; slotIndex < hostAutomationSlotCount; ++slotIndex)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getHostSlotParameterId(slotIndex), 2 },
            getHostSlotLetterLabel(slotIndex),
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f },
            0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true)));
    }

    parameterLayout.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { paramCrossoverActiveSplitCountId, 1 },
        "CROSSOVER / COUNT",
        0,
        5,
        0,
        juce::AudioParameterIntAttributes().withAutomatable(false).withMeta(true)));

    constexpr std::array<float, 5> crossoverDefaults { 134.0f, 523.0f, 2093.0f, 5000.0f, 10000.0f };

    for (size_t index = 0; index < crossoverDefaults.size(); ++index)
    {
        const auto suffix = "xover" + juce::String(static_cast<int>(index + 1));
        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getCrossoverParameterId(suffix.toRawUTF8()), 1 },
            "CROSSOVER / XOVER-" + juce::String(static_cast<int>(index + 1)),
            juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.01f },
            crossoverDefaults[index],
            juce::AudioParameterFloatAttributes().withAutomatable(false).withMeta(true)));
    }

    for (size_t rangeIndex = 0; rangeIndex < 6; ++rangeIndex)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { getCrossoverSoloParameterId(rangeIndex), 1 },
            "CROSSOVER / SOLO / " + juce::String(static_cast<int>(rangeIndex + 1)),
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));
    }

    for (const auto& [suffix, label] : std::array {
             std::pair { "listenLc", "LC" },
             std::pair { "listenRc", "RC" },
             std::pair { "listenMc", "MC" },
             std::pair { "listenSc", "SC" },
             std::pair { "listenLl", "LL" },
             std::pair { "listenRr", "RR" },
             std::pair { "listenSs", "SS" }
         })
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { getCrossoverParameterId(suffix), 1 },
            "CROSSOVER / LISTEN / " + juce::String(label),
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));
    }

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { getCrossoverParameterId("autoSolo"), 1 },
        "CROSSOVER / AUTO-SOLO",
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramGlobalBypassId, 1 },
        "AVA / GLOBAL / B",
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));

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
