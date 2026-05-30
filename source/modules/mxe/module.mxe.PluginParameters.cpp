#include "module.mxe.PluginParameters.h"

#include "module.mxe.MultibandProcessor.h"
#include "module.mxe.ParameterIds.h"
#include "module.mxe.ValueFormatting.h"

#include <array>
#include <algorithm>
#include <memory>

namespace mxe::parameters
{
namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* label;
};

inline constexpr auto mxeMiscFullbandOrder = std::to_array<ParameterOrderEntry>({
    { "module_bypass", "BYPASS" },
    { "module_bypass_wt_gain", "BYPASS.WT-GAIN" },
    { "inGnVisible", "IN-GAIN-LR" },
    { "inLeft", "IN-GAIN-L" },
    { "inRight", "IN-GAIN-R" },
    { "wideVisible", "IN-WIDE" },
    { "outGnVisible", "OUT-GAIN" },
});

inline constexpr auto mxeCrossoverFullbandOrder = std::to_array<ParameterOrderEntry>({
    { "active_split_count", "SPLIT COUNT" },
    { "xover1", "CROSSOVER 1" },
    { "xover2", "CROSSOVER 2" },
    { "xover3", "CROSSOVER 3" },
    { "xover4", "CROSSOVER 4" },
    { "xover5", "CROSSOVER 5" },
});

inline constexpr auto mxeMiscBandOrder = std::to_array<ParameterOrderEntry>({
    { "delTa", "DELTA" },
    { "inGn", "IN-GAIN-LR" },
    { "inLeft", "IN-GAIN-L" },
    { "inRight", "IN-GAIN-R" },
    { "wide", "IN-WIDE" },
    { "outGn", "OUT-GAIN" },
});

inline constexpr auto mxeMainBandOrder = std::to_array<ParameterOrderEntry>({
    { "moRph", "Morph" },
    { "peakHoldHz", "Peak Hold" },
    { "TensionFlooR", "Tension Floor" },
    { "TensionHysT", "Tension Hysteresis" },
    { "linkUpDn", "Link UP/DN (Dual-Mono)" },
    { "linkLr", "Link L/R (Stereo)" },
    { "linkOpp", "Link Opp" },
    { "thLU", "L.UP.THR" },
    { "tensLU", "L.UP.TENS" },
    { "relLU", "L.UP.REL" },
    { "outLU", "L.UP.OUT" },
    { "thLD", "L.DN.THR" },
    { "tensLD", "L.DN.TENS" },
    { "relLD", "L.DN.REL" },
    { "outLD", "L.DN.OUT" },
    { "thRU", "R.UP.THR" },
    { "tensRU", "R.UP.TENS" },
    { "relRU", "R.UP.REL" },
    { "outRU", "R.UP.OUT" },
    { "thRD", "R.DN.THR" },
    { "tensRD", "R.DN.TENS" },
    { "relRD", "R.DN.REL" },
    { "outRD", "R.DN.OUT" },
});

constexpr size_t numBands = mxe::dsp::MultibandProcessor::numBands;

juce::String makeFullbandHostName(const juce::String& sectionName, const juce::String& parameterName)
{
    return "GLOBAL - " + sectionName + " - " + parameterName;
}

juce::String makeBandHostName(const size_t bandIndex, const juce::String& sectionName, const juce::String& parameterName)
{
    return "BAND " + juce::String(static_cast<int>(bandIndex + 1)) + " - " + sectionName + " - " + parameterName;
}

juce::String formatParameterValue(const float value)
{
    return mxe::formatting::formatDspValue(value);
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

    auto fullbandMiscGroup = std::make_unique<juce::AudioProcessorParameterGroup>("fullband_misc",
                                                                                  "MISC",
                                                                                  " | ");

    for (const auto& entry : mxeMiscFullbandOrder)
    {
        const auto key = juce::String(entry.key);

        if (key == "module_bypass")
        {
            fullbandMiscGroup->addChild(boolParam(makeModuleBypassParameterId(),
                                                  makeFullbandHostName("MISC", entry.label),
                                                  false,
                                                  false,
                                                  true));
            continue;
        }

        if (key == "module_bypass_wt_gain")
        {
            fullbandMiscGroup->addChild(boolParam(makeModuleBypassWithGainParameterId(),
                                                  makeFullbandHostName("MISC", entry.label),
                                                  false,
                                                  false,
                                                  true));
            continue;
        }

        const auto fullbandVisibleIt = std::find_if(fullbandVisibleSpecs.begin(), fullbandVisibleSpecs.end(), [&entry] (const auto& spec)
        {
            return juce::String(spec.suffix) == entry.key;
        });

        if (fullbandVisibleIt != fullbandVisibleSpecs.end())
        {
            fullbandMiscGroup->addChild(floatParam(makeFullbandParameterId(fullbandVisibleIt->suffix),
                                                   makeFullbandHostName("MISC", entry.label),
                                                   fullbandVisibleIt->min,
                                                   fullbandVisibleIt->max,
                                                   fullbandVisibleIt->step,
                                                   fullbandVisibleIt->defaultValue,
                                                   fullbandVisibleIt->label,
                                                   false,
                                                   true));
            continue;
        }

        const auto fullbandAutomationIt = std::find_if(fullbandAutomationSpecs.begin(), fullbandAutomationSpecs.end(), [&entry] (const auto& spec)
        {
            return juce::String(spec.suffix) == entry.key;
        });

        if (fullbandAutomationIt != fullbandAutomationSpecs.end())
        {
            fullbandMiscGroup->addChild(floatParam(makeFullbandParameterId(fullbandAutomationIt->suffix),
                                                   makeFullbandHostName("MISC", entry.label),
                                                   fullbandAutomationIt->min,
                                                   fullbandAutomationIt->max,
                                                   fullbandAutomationIt->step,
                                                   fullbandAutomationIt->defaultValue,
                                                   fullbandAutomationIt->label,
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
            fullbandMiscGroup->addChild(floatParam(makeFullbandParameterId(crossoverIt->suffix),
                                                   makeFullbandHostName("MISC", entry.label),
                                                   crossoverIt->min,
                                                   crossoverIt->max,
                                                   crossoverIt->step,
                                                   crossoverIt->defaultValue,
                                                   crossoverIt->label,
                                                   false,
                                                   true));
            continue;
        }

    }

    auto fullbandCrossoverGroup = std::make_unique<juce::AudioProcessorParameterGroup>("fullband_crossover",
                                                                                        "CROSSOVER",
                                                                                        " | ");

    for (const auto& entry : mxeCrossoverFullbandOrder)
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

    fullbandGroup->addChild(std::move(fullbandMiscGroup));
    fullbandGroup->addChild(std::move(fullbandCrossoverGroup));

    layout.add(std::move(fullbandGroup));

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>(makeBandGroupId(bandIndex),
                                                                          makeBandGroupName(bandIndex),
                                                                          " | ");

        auto miscGroup = std::make_unique<juce::AudioProcessorParameterGroup>(makeBandGroupId(bandIndex) + "_misc",
                                                                              "MISC",
                                                                              " | ");

        for (const auto& entry : mxeMiscBandOrder)
        {
            const auto it = std::find_if(parameterSpecs.begin(), parameterSpecs.end(), [&entry] (const auto& spec)
            {
                return juce::String(spec.suffix) == entry.key;
            });

            if (it == parameterSpecs.end())
                continue;

            const auto parameterId = makeBandParameterId(bandIndex, it->suffix);
            const auto parameterName = makeBandHostName(bandIndex, "MISC", entry.label);

            if (it->type == ParameterType::boolean)
                miscGroup->addChild(boolParam(parameterId, parameterName, it->defaultValue >= 0.5f, false));
            else
                miscGroup->addChild(floatParam(parameterId,
                                               parameterName,
                                               it->min,
                                               it->max,
                                               it->step,
                                               it->defaultValue,
                                               it->label,
                                               false));
        }

        auto mainGroup = std::make_unique<juce::AudioProcessorParameterGroup>(makeBandGroupId(bandIndex) + "_main",
                                                                              "MAIN",
                                                                              " | ");

        for (const auto& entry : mxeMainBandOrder)
        {
            const auto it = std::find_if(parameterSpecs.begin(), parameterSpecs.end(), [&entry] (const auto& spec)
            {
                return juce::String(spec.suffix) == entry.key;
            });

            if (it == parameterSpecs.end())
                continue;

            const auto parameterId = makeBandParameterId(bandIndex, it->suffix);
            const auto parameterName = makeBandHostName(bandIndex, "MAIN", entry.label);

            if (it->type == ParameterType::boolean)
                mainGroup->addChild(boolParam(parameterId, parameterName, it->defaultValue >= 0.5f, false));
            else
                mainGroup->addChild(floatParam(parameterId,
                                               parameterName,
                                               it->min,
                                               it->max,
                                               it->step,
                                               it->defaultValue,
                                               it->label,
                                               false));
        }

        group->addChild(std::move(miscGroup));
        group->addChild(std::move(mainGroup));
        layout.add(std::move(group));
    }

    return layout;
}
} // namespace mxe::parameters
