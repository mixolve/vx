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
    globalGainLrParam = parameters.getRawParameterValue(paramGlobalGainLrId);
    globalWideParam = parameters.getRawParameterValue(paramGlobalWideId);
    outGainParam = parameters.getRawParameterValue(paramOutGainId);
    globalBypassParam = parameters.getRawParameterValue(paramGlobalBypassId);
    globalBypassOutGainOnlyParam = parameters.getRawParameterValue(paramGlobalBypassOutGainOnlyId);
    mxeBypassParam = parameters.getRawParameterValue(paramMxeBypassId);
    mxeBypassOutGainOnlyParam = parameters.getRawParameterValue(paramMxeBypassOutGainOnlyId);
    mxeInGainLrParam = parameters.getRawParameterValue(paramMxeInGainLrId);
    mxeInGainLParam = parameters.getRawParameterValue(paramMxeInGainLId);
    mxeInGainRParam = parameters.getRawParameterValue(paramMxeInGainRId);
    mxeWideParam = parameters.getRawParameterValue(paramMxeWideId);
    mxeOutGainParam = parameters.getRawParameterValue(paramMxeOutGainId);

    for (int bandIndex = 0; bandIndex < mxeBandCount; ++bandIndex)
    {
        const auto bandNumber = bandIndex + 1;
        mxeBandBypassParams[static_cast<size_t>(bandIndex)] = parameters.getRawParameterValue(
            "mxe_band_" + juce::String(bandNumber) + "_bypass");
        mxeBandBypassWithGainParams[static_cast<size_t>(bandIndex)] = parameters.getRawParameterValue(
            "mxe_band_" + juce::String(bandNumber) + "_bypass_wt_gain");
        mxeBandInGainLrParams[static_cast<size_t>(bandIndex)] = parameters.getRawParameterValue(
            "mxe_band_" + juce::String(bandNumber) + "_in_gain_lr");
        mxeBandInGainLParams[static_cast<size_t>(bandIndex)] = parameters.getRawParameterValue(
            "mxe_band_" + juce::String(bandNumber) + "_in_gain_l");
        mxeBandInGainRParams[static_cast<size_t>(bandIndex)] = parameters.getRawParameterValue(
            "mxe_band_" + juce::String(bandNumber) + "_in_gain_r");
        mxeBandWideParams[static_cast<size_t>(bandIndex)] = parameters.getRawParameterValue(
            "mxe_band_" + juce::String(bandNumber) + "_wide");
        mxeBandOutGainParams[static_cast<size_t>(bandIndex)] = parameters.getRawParameterValue(
            "mxe_band_" + juce::String(bandNumber) + "_out_gain");
    }

    if (mxeModuleProcessors[0] != nullptr)
        connectMxeProcessor(*mxeModuleProcessors[0]);
}

VxAudioProcessor::~VxAudioProcessor() = default;

void VxAudioProcessor::connectMxeProcessor(MxeAudioProcessor& processor)
{
    MxeAudioProcessor::ExternalParameterConnection externalParameterConnection;
    externalParameterConnection.moduleBypass = mxeBypassParam;
    externalParameterConnection.moduleBypassWithGain = mxeBypassOutGainOnlyParam;
    externalParameterConnection.inGainLr = mxeInGainLrParam;
    externalParameterConnection.inGainL = mxeInGainLParam;
    externalParameterConnection.inGainR = mxeInGainRParam;
    externalParameterConnection.wide = mxeWideParam;
    externalParameterConnection.outGain = mxeOutGainParam;

    for (size_t bandIndex = 0; bandIndex < externalParameterConnection.bandParameters.size(); ++bandIndex)
    {
        externalParameterConnection.bandParameters[bandIndex].bypass = mxeBandBypassParams[bandIndex];
        externalParameterConnection.bandParameters[bandIndex].bypassWithGain = mxeBandBypassWithGainParams[bandIndex];
        externalParameterConnection.bandParameters[bandIndex].inGainLr = mxeBandInGainLrParams[bandIndex];
        externalParameterConnection.bandParameters[bandIndex].inGainL = mxeBandInGainLParams[bandIndex];
        externalParameterConnection.bandParameters[bandIndex].inGainR = mxeBandInGainRParams[bandIndex];
        externalParameterConnection.bandParameters[bandIndex].wide = mxeBandWideParams[bandIndex];
        externalParameterConnection.bandParameters[bandIndex].outGain = mxeBandOutGainParams[bandIndex];
    }

    externalParameterConnection.pushMirroredParameterValue = [this] (const MxeAudioProcessor::ExternalParameterChange& change)
    {
        juce::String parameterId;

        switch (change.target)
        {
            case MxeAudioProcessor::ExternalParameterTarget::moduleBypass:
                parameterId = paramMxeBypassId;
                break;

            case MxeAudioProcessor::ExternalParameterTarget::moduleBypassWithGain:
                parameterId = paramMxeBypassOutGainOnlyId;
                break;

            case MxeAudioProcessor::ExternalParameterTarget::fullbandInGainLr:
                parameterId = paramMxeInGainLrId;
                break;

            case MxeAudioProcessor::ExternalParameterTarget::fullbandInGainL:
                parameterId = paramMxeInGainLId;
                break;

            case MxeAudioProcessor::ExternalParameterTarget::fullbandInGainR:
                parameterId = paramMxeInGainRId;
                break;

            case MxeAudioProcessor::ExternalParameterTarget::fullbandWide:
                parameterId = paramMxeWideId;
                break;

            case MxeAudioProcessor::ExternalParameterTarget::fullbandOutGain:
                parameterId = paramMxeOutGainId;
                break;

            case MxeAudioProcessor::ExternalParameterTarget::bandInGainLr:
                parameterId = "mxe_band_" + juce::String(static_cast<int>(change.bandIndex + 1)) + "_in_gain_lr";
                break;

            case MxeAudioProcessor::ExternalParameterTarget::bandInGainL:
                parameterId = "mxe_band_" + juce::String(static_cast<int>(change.bandIndex + 1)) + "_in_gain_l";
                break;

            case MxeAudioProcessor::ExternalParameterTarget::bandInGainR:
                parameterId = "mxe_band_" + juce::String(static_cast<int>(change.bandIndex + 1)) + "_in_gain_r";
                break;

            case MxeAudioProcessor::ExternalParameterTarget::bandWide:
                parameterId = "mxe_band_" + juce::String(static_cast<int>(change.bandIndex + 1)) + "_wide";
                break;

            case MxeAudioProcessor::ExternalParameterTarget::bandOutGain:
                parameterId = "mxe_band_" + juce::String(static_cast<int>(change.bandIndex + 1)) + "_out_gain";
                break;
        }

        if (parameterId.isEmpty())
            return;

        setMxeShellParameterValue(parameterId, change.value);
    };

    processor.setExternalParameterConnection(std::move(externalParameterConnection));
}

void VxAudioProcessor::setMxeShellParameterValue(const juce::String& parameterId, const float value)
{
    const auto applyValue = [this, parameterId, value]
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(parameterId));

        if (parameter == nullptr)
            return;

        const auto normalisedValue = parameter->convertTo0to1(value);

        if (std::abs(parameter->getValue() - normalisedValue) <= 1.0e-6f)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalisedValue);
        parameter->endChangeGesture();
    };

    if (auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
        messageManager != nullptr && messageManager->isThisTheMessageThread())
    {
        applyValue();
        return;
    }

    juce::MessageManager::callAsync([this, parameterId, value]
    {
        setMxeShellParameterValue(parameterId, value);
    });
}

juce::AudioProcessorValueTreeState::ParameterLayout VxAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    const auto makeShellGlobalName = [] (const juce::String& parameterName)
    {
        return "GLOBAL - MISC - " + parameterName;
    };
    const auto makeMxeMiscName = [] (const juce::String& parameterName)
    {
        return "MXE - MISC - " + parameterName;
    };
    const auto makeMxeBandName = [] (const int bandNumber, const juce::String& parameterName)
    {
        return "MXE - BAND " + juce::String(bandNumber) + " - " + parameterName;
    };

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramGlobalBypassId, 1 },
        makeShellGlobalName("BYPASS"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramGlobalBypassOutGainOnlyId, 1 },
        makeShellGlobalName("BYPASS.WT-GAIN"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramGlobalGainLrId, 1 },
        makeShellGlobalName("IN-GAIN-LR"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramGlobalGainLId, 1 },
        makeShellGlobalName("IN-GAIN-L"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramGlobalGainRId, 1 },
        makeShellGlobalName("IN-GAIN-R"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramGlobalWideId, 1 },
        makeShellGlobalName("WIDE"),
        juce::NormalisableRange<float> { 0.0f, 400.0f, 0.01f },
        100.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return juce::String::formatted("%.0f", static_cast<double>(value));
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramOutGainId, 1 },
        makeShellGlobalName("OUT-GAIN"),
        juce::NormalisableRange<float> { -48.0f, 48.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramMxeBypassId, 1 },
        makeMxeMiscName("BYPASS"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramMxeBypassOutGainOnlyId, 1 },
        makeMxeMiscName("BYPASS.WT-GAIN"),
        false,
        juce::AudioParameterBoolAttributes().withAutomatable(true)));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMxeInGainLrId, 1 },
        makeMxeMiscName("IN-GAIN-LR"),
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMxeInGainLId, 1 },
        makeMxeMiscName("IN-GAIN-L"),
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMxeInGainRId, 1 },
        makeMxeMiscName("IN-GAIN-R"),
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMxeWideId, 1 },
        makeMxeMiscName("WIDE"),
        juce::NormalisableRange<float> { 0.0f, 400.0f, 0.1f },
        100.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return juce::String::formatted("%.0f", static_cast<double>(value));
            })));

    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramMxeOutGainId, 1 },
        makeMxeMiscName("OUT-GAIN"),
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
            [] (float value, int)
            {
                return formatDecibelValue(value);
            })));

    for (int bandIndex = 0; bandIndex < mxeBandCount; ++bandIndex)
    {
        const auto bandNumber = bandIndex + 1;

        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "mxe_band_" + juce::String(bandNumber) + "_bypass", 1 },
            makeMxeBandName(bandNumber, "BYPASS"),
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(true)));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "mxe_band_" + juce::String(bandNumber) + "_bypass_wt_gain", 1 },
            makeMxeBandName(bandNumber, "BYPASS.WT-GAIN"),
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(true)));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "mxe_band_" + juce::String(bandNumber) + "_in_gain_lr", 1 },
            makeMxeBandName(bandNumber, "IN-GAIN-LR"),
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatDecibelValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "mxe_band_" + juce::String(bandNumber) + "_in_gain_l", 1 },
            makeMxeBandName(bandNumber, "IN-GAIN-L"),
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatDecibelValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "mxe_band_" + juce::String(bandNumber) + "_in_gain_r", 1 },
            makeMxeBandName(bandNumber, "IN-GAIN-R"),
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatDecibelValue(value);
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "mxe_band_" + juce::String(bandNumber) + "_wide", 1 },
            makeMxeBandName(bandNumber, "WIDE"),
            juce::NormalisableRange<float> { 0.0f, 400.0f, 0.1f },
            100.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return juce::String::formatted("%.0f", static_cast<double>(value));
                })));

        parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "mxe_band_" + juce::String(bandNumber) + "_out_gain", 1 },
            makeMxeBandName(bandNumber, "OUT-GAIN"),
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(true).withStringFromValueFunction(
                [] (float value, int)
                {
                    return formatDecibelValue(value);
                })));
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
