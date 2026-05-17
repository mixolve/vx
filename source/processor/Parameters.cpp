#include "ProcessorSupport.h"

#include <cmath>
#include <memory>
#include <vector>

namespace
{
void ensureDefaultPresetFilesExist(EqeAudioProcessor& processor)
{
    if (loadFilterPresetsXml() == nullptr)
    {
        if (processor.saveFilterPreset("default"))
            processor.setDefaultFilterPreset("default");
    }
}
}

EqeAudioProcessor::EqeAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "eqe_state", createParameterLayout())
{
    globalGainLParam = parameters.getRawParameterValue(paramGlobalGainLId);
    globalGainRParam = parameters.getRawParameterValue(paramGlobalGainRId);
    globalWideParam = parameters.getRawParameterValue(paramGlobalWideId);
    outGainParam = parameters.getRawParameterValue(paramOutGainId);
    globalBypassParam = parameters.getRawParameterValue(paramGlobalBypassId);
    globalBypassOutGainOnlyParam = parameters.getRawParameterValue(paramGlobalBypassOutGainOnlyId);

    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        filterTypeParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getFilterTypeParamId(bellIndex));
        filterLrmsParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getFilterLrmsParamId(bellIndex));
        bellFrequencyParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellFrequencyParamId(bellIndex));
        bellBandwidthParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellBandwidthParamId(bellIndex));
        bellSlopeChoiceParams[static_cast<size_t>(bellIndex)] = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter(getBellSlopeParamId(bellIndex)));
        bellGainParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellGainParamId(bellIndex));
        bellBypassParams[static_cast<size_t>(bellIndex)] = parameters.getRawParameterValue(getBellBypassParamId(bellIndex));
    }

    ensureDefaultPresetFilesExist(*this);

    const auto defaultFilterPresetName = getDefaultFilterPresetName();

    if (defaultFilterPresetName.isNotEmpty())
    {
        if (! loadFilterPreset(defaultFilterPresetName))
        {
            if (const auto lastFilterPresetName = getLastFilterPresetName(); lastFilterPresetName.isNotEmpty())
                loadFilterPreset(lastFilterPresetName);
        }
        else if (parameters.state.getProperty(filterPresetDefaultSelectedStateKey).toString().trim().isEmpty())
        {
            parameters.state.setProperty(filterPresetDefaultSelectedStateKey, defaultFilterPresetName, nullptr);
        }
    }
    else if (const auto lastFilterPresetName = getLastFilterPresetName(); lastFilterPresetName.isNotEmpty())
    {
        loadFilterPreset(lastFilterPresetName);
    }
}

EqeAudioProcessor::~EqeAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout EqeAudioProcessor::createParameterLayout()
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
        "GLOBAL - BYPASS.WT-GN",
        false));

    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getFilterTypeParamId(bellIndex), 1 },
            makeFilterTypeName(bellIndex),
            filterTypeChoices,
            EqeAudioProcessor::choiceIndexFromFilterType(EqeAudioProcessor::FilterType::bell)));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getFilterLrmsParamId(bellIndex), 1 },
            makeFilterLrmsName(bellIndex),
            filterLrmsChoices,
            0));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { getBellSlopeParamId(bellIndex), 1 },
            makeBellParameterName("ORDER", bellIndex),
            EqeAudioProcessor::getBellSlopeChoices(),
            EqeAudioProcessor::getBellSlopeChoiceIndexForValue(EqeAudioProcessor::fixedSlopeDbPerOct)));

        auto filterFrequencyRange = juce::NormalisableRange<float> { minimumVisibleFilterFrequency, maximumVisibleFilterFrequency, 0.01f };
        filterFrequencyRange.setSkewForCentre(defaultTiltFrequency);

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getBellFrequencyParamId(bellIndex), 1 },
            makeBellParameterName("FREQ", bellIndex),
            filterFrequencyRange,
            defaultTiltFrequency,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatFrequencyValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getBellBandwidthParamId(bellIndex), 1 },
            makeBellParameterName("BW", bellIndex),
            juce::NormalisableRange<float> { minimumBellBandwidth, maximumBellBandwidth, 0.001f },
            1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatBandwidthValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { getBellGainParamId(bellIndex), 1 },
            makeBellParameterName("GAIN", bellIndex),
            juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
            0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatDecibelValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { getBellBypassParamId(bellIndex), 1 },
            makeBellParameterName("BYPASS", bellIndex),
            false));
    }

    return { parameterLayout.begin(), parameterLayout.end() };
}

const juce::String EqeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EqeAudioProcessor::acceptsMidi() const
{
    return false;
}

bool EqeAudioProcessor::producesMidi() const
{
    return false;
}

bool EqeAudioProcessor::isMidiEffect() const
{
    return false;
}

double EqeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EqeAudioProcessor::getNumPrograms()
{
    return 1;
}

int EqeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EqeAudioProcessor::setCurrentProgram(int)
{
}

const juce::String EqeAudioProcessor::getProgramName(int)
{
    return {};
}

void EqeAudioProcessor::changeProgramName(int, const juce::String&)
{
}
