#include "module.mie.PluginParameters.h"

#include "module.mie.MultibandProcessor.h"
#include "module.mie.ParameterIds.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <memory>

namespace mie::parameters
{
namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* label;
};

inline constexpr auto mieCrossoverFullbandOrder = std::to_array<ParameterOrderEntry>({
    { "active_split_count", "SPLIT COUNT" },
    { "xover1", "CROSSOVER 1" },
    { "xover2", "CROSSOVER 2" },
    { "xover3", "CROSSOVER 3" },
    { "xover4", "CROSSOVER 4" },
    { "xover5", "CROSSOVER 5" },
});

inline constexpr auto mieBandOrder = std::to_array<ParameterOrderEntry>({
    { "wide", "WIDE" },
    { "gainL", "GAIN-L" },
    { "gainR", "GAIN-R" },
    { "gainLr", "GAIN-LR" },
});

constexpr size_t numBands = mie::dsp::MultibandProcessor::numBands;

juce::String makeFullbandHostName(const juce::String& sectionName, const juce::String& parameterName)
{
    return "GLOBAL - " + sectionName + " - " + parameterName;
}

juce::String makeBandHostName(const size_t bandIndex, const juce::String& sectionName, const juce::String& parameterName)
{
    return "BAND " + juce::String(static_cast<int>(bandIndex + 1)) + " - " + sectionName + " - " + parameterName;
}

double roundToDisplayStep(const double value) noexcept
{
    auto rounded = std::floor((value * 10.0) + 0.5) * 0.1;

    if (std::abs(rounded) < 0.05)
        rounded = 0.0;

    return rounded;
}

juce::String formatParameterValue(const float value)
{
    return juce::String::formatted("%.1f", roundToDisplayStep(value));
}

constexpr bool isHostEditableParameter = false;
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
                          const bool isAutomatable,
                          const bool isMeta = false) -> Parameter
    {
        auto range = juce::NormalisableRange<float> { min, max, step };
        auto attributes = juce::AudioParameterFloatAttributes()
                              .withLabel(label)
                              .withAutomatable(isAutomatable)
                              .withMeta(isMeta)
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
                         const bool isAutomatable,
                         const bool isMeta = false) -> Parameter
    {
        auto attributes = juce::AudioParameterBoolAttributes()
                              .withAutomatable(isAutomatable)
                              .withMeta(isMeta);
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

    auto fullbandCrossoverGroup = std::make_unique<juce::AudioProcessorParameterGroup>("fullband_crossover",
                                                                                        "CROSSOVER",
                                                                                        " | ");

    for (const auto& entry : mieCrossoverFullbandOrder)
    {
        const auto key = juce::String(entry.key);

        if (key == "active_split_count")
        {
            fullbandCrossoverGroup->addChild(floatParam(makeActiveSplitCountParameterId(),
                                                        makeFullbandHostName("CROSSOVER", entry.label),
                                                        0.0f,
                                                        5.0f,
                                                        1.0f,
                                                        0.0f,
                                                        "",
                                                        false,
                                                        true));
            continue;
        }

        const auto crossoverIt = std::find_if(crossoverSpecs.begin(), crossoverSpecs.end(), [&entry] (const auto& spec)
        {
            return juce::String(spec.suffix) == entry.key;
        });

        if (crossoverIt != crossoverSpecs.end())
        {
            fullbandCrossoverGroup->addChild(floatParam(makeFullbandParameterId(crossoverIt->suffix),
                                                        makeFullbandHostName("CROSSOVER", entry.label),
                                                        crossoverIt->min,
                                                        crossoverIt->max,
                                                        crossoverIt->step,
                                                        crossoverIt->defaultValue,
                                                        crossoverIt->label,
                                                        false,
                                                        true));
        }
    }

    fullbandGroup->addChild(std::move(fullbandCrossoverGroup));

    layout.add(std::move(fullbandGroup));

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>(makeBandGroupId(bandIndex),
                                                                          makeBandGroupName(bandIndex),
                                                                          " | ");

        for (const auto& entry : mieBandOrder)
        {
            const auto it = std::find_if(parameterSpecs.begin(), parameterSpecs.end(), [&entry] (const auto& spec)
            {
                return juce::String(spec.suffix) == entry.key;
            });

            if (it == parameterSpecs.end())
                continue;

            const auto parameterId = makeBandParameterId(bandIndex, it->suffix);
            const auto parameterName = makeBandHostName(bandIndex, "MIE", entry.label);

            if (it->type == ParameterType::boolean)
                group->addChild(boolParam(parameterId, parameterName, it->defaultValue >= 0.5f, false));
            else
                group->addChild(floatParam(parameterId,
                                           parameterName,
                                           it->min,
                                           it->max,
                                           it->step,
                                           it->defaultValue,
                                           it->label,
                                           false));
        }

        layout.add(std::move(group));
    }

    return layout;
}
} // namespace mie::parameters
