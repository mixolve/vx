#include "module.mxe.PluginParameters.h"

#include "module.mxe.MultibandProcessor.h"
#include "module.mxe.ParameterIds.h"
#include "module.mxe.ValueFormatting.h"

#include <memory>

namespace mxe::parameters
{
namespace
{
constexpr size_t numBands = mxe::dsp::MultibandProcessor::numBands;

juce::String formatParameterValue(const float value)
{
    return mxe::formatting::formatDspValue(value);
}

constexpr bool isAutomationOnlySlot(const ParameterSlot slot)
{
    return slot == ParameterSlot::autoInGn
        || slot == ParameterSlot::autoInRight
        || slot == ParameterSlot::autoInLeft;
}

juce::String getAutomationTargetName(const ParameterSlot slot)
{
    if (slot == ParameterSlot::autoInGn)
        return "INPUT-GAIN";

    if (slot == ParameterSlot::autoInRight)
        return "IN-RIGHT";

    if (slot == ParameterSlot::autoInLeft)
        return "IN-LEFT";

    jassertfalse;
    return {};
}

juce::String makeBandAutomationParameterName(const size_t bandIndex, const ParameterSlot slot)
{
    return "ENV BAND " + juce::String(static_cast<int>(bandIndex + 1)) + " " + getAutomationTargetName(slot);
}

constexpr bool isHostEditableParameter = true;
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using Parameter = std::unique_ptr<juce::RangedAudioParameter>;

    auto floatParam = [] (const juce::String& id,
                          const juce::String& name,
                          const float min,
                          const float max,
                          const float step,
                          const float defaultValue,
                          const juce::String& label,
                          const bool isAutomatable) -> Parameter
    {
        auto range = juce::NormalisableRange<float> { min, max, step };
        auto attributes = juce::AudioParameterFloatAttributes()
                              .withLabel(label)
                              .withAutomatable(isAutomatable)
                              .withStringFromValueFunction([] (float value, int)
                              {
                                  return formatParameterValue(value);
                              })
                              .withValueFromStringFunction([] (const juce::String& text)
                              {
                                  return text.trim().getFloatValue();
                              });
        return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, range, defaultValue, attributes);
    };

    auto boolParam = [] (const juce::String& id,
                         const juce::String& name,
                         const bool defaultValue,
                         const bool isAutomatable) -> Parameter
    {
        auto attributes = juce::AudioParameterBoolAttributes()
                              .withAutomatable(isAutomatable);
        return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, defaultValue, attributes);
    };

    Layout layout;
    auto soloGroup = std::make_unique<juce::AudioProcessorParameterGroup>("monitor", "Monitor", " | ");

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
        soloGroup->addChild(boolParam(makeSoloParameterId(bandIndex),
                                      "Solo Band " + juce::String(static_cast<int>(bandIndex + 1)),
                                      false,
                                      isHostEditableParameter));

    layout.add(std::move(soloGroup));

    auto fullbandGroup = std::make_unique<juce::AudioProcessorParameterGroup>(makeFullbandGroupId(),
                                                                              makeFullbandGroupName(),
                                                                              " | ");

    for (const auto& spec : fullbandVisibleSpecs)
        fullbandGroup->addChild(floatParam(makeFullbandParameterId(spec.suffix), spec.name, spec.min, spec.max, spec.step, spec.defaultValue, spec.label, isHostEditableParameter));

    for (const auto& spec : fullbandAutomationSpecs)
        fullbandGroup->addChild(floatParam(makeFullbandParameterId(spec.suffix), spec.name, spec.min, spec.max, spec.step, spec.defaultValue, spec.label, isHostEditableParameter));

    for (const auto& spec : crossoverSpecs)
        fullbandGroup->addChild(floatParam(makeFullbandParameterId(spec.suffix), spec.name, spec.min, spec.max, spec.step, spec.defaultValue, spec.label, isHostEditableParameter));

    fullbandGroup->addChild(floatParam(makeActiveSplitCountParameterId(), "Active Crossovers", 0.0f, 5.0f, 1.0f, 0.0f, "", isHostEditableParameter));
    fullbandGroup->addChild(boolParam(makeModuleBypassParameterId(), "MISC - BYPASS", false, isHostEditableParameter));
    fullbandGroup->addChild(boolParam(makeModuleBypassWithGainParameterId(), "MISC - BYPASS.WT-GAIN", false, isHostEditableParameter));

    layout.add(std::move(fullbandGroup));

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>(makeBandGroupId(bandIndex),
                                                                          makeBandGroupName(bandIndex),
                                                                          " | ");

        for (size_t parameterIndex = 0; parameterIndex < parameterSpecs.size(); ++parameterIndex)
        {
            const auto& spec = parameterSpecs[parameterIndex];
            const auto slot = static_cast<ParameterSlot>(parameterIndex);
            const auto parameterId = makeBandParameterId(bandIndex, spec.suffix);
            const auto parameterName = isAutomationOnlySlot(slot) ? makeBandAutomationParameterName(bandIndex, slot)
                                                                  : juce::String(spec.name);

            if (spec.type == ParameterType::boolean)
                group->addChild(boolParam(parameterId, parameterName, spec.defaultValue >= 0.5f, isHostEditableParameter));
            else
                group->addChild(floatParam(parameterId, parameterName, spec.min, spec.max, spec.step, spec.defaultValue, spec.label, isHostEditableParameter));
        }

        layout.add(std::move(group));
    }

    return layout;
}
} // namespace mxe::parameters
