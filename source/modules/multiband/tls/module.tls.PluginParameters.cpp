#include "module.tls.PluginParameters.h"

#include "module.tls.DspCore.h"
#include "module.tls.ParameterIds.h"

#include <array>
#include <algorithm>
#include <memory>

namespace tls::parameters
{
namespace
{
struct ParameterOrderEntry
{
    const char* key;
    const char* block;
    const char* label;
};

inline constexpr auto tlsCrossoverFullbandOrder = std::to_array<ParameterOrderEntry>({
    { "active_split_count", "CROSSOVER", "SPLIT COUNT" },
    { "xover1", "CROSSOVER", "CROSSOVER 1" },
    { "xover2", "CROSSOVER", "CROSSOVER 2" },
    { "xover3", "CROSSOVER", "CROSSOVER 3" },
    { "xover4", "CROSSOVER", "CROSSOVER 4" },
    { "xover5", "CROSSOVER", "CROSSOVER 5" },
});

inline constexpr auto tlsBandOrder = std::to_array<ParameterOrderEntry>({
    { "gainMid", "GAIN", "MID" },
    { "gainMidMute", "GAIN", "MID MUTE" },
    { "gainSide", "GAIN", "SIDE" },
    { "gainSideMute", "GAIN", "SIDE MUTE" },
    { "gainL", "GAIN", "LEFT" },
    { "gainLMute", "GAIN", "LEFT MUTE" },
    { "gainR", "GAIN", "RIGHT" },
    { "gainRMute", "GAIN", "RIGHT MUTE" },
    { "gainLr", "GAIN", "STEREO" },
    { "gainLrMute", "GAIN", "STEREO MUTE" },
    { "gainLOrder", "GAIN ORDER", "LEFT" },
    { "gainROrder", "GAIN ORDER", "RIGHT" },
    { "gainMidOrder", "GAIN ORDER", "MID" },
    { "gainSideOrder", "GAIN ORDER", "SIDE" },
    { "halfPositive", "RECTIFICATION", "HPOS" },
    { "halfNegative", "RECTIFICATION", "HNEG" },
    { "fullPositive", "RECTIFICATION", "FPOS" },
    { "fullNegative", "RECTIFICATION", "FNEG" },
    { "left", "PANORAMA", "LEFT" },
    { "right", "PANORAMA", "RIGHT" },
    { "law", "PANORAMA", "LAW" },
    { "impact", "SHEAR", "IMPACT" },
    { "impactDirection", "SHEAR", "DIRECTION" },
    { "mid", "MS BALANCE", "MID" },
    { "side", "MS BALANCE", "SIDE" },
    { "degree", "ORTHOGONAL", "DEGREE" },
    { "flipRight", "ORTHOGONAL", "FLIP RIGHT" },
    { "listenLc", "LISTEN", "LC" },
    { "listenRc", "LISTEN", "RC" },
    { "listenMc", "LISTEN", "MC" },
    { "listenSc", "LISTEN", "SC" },
    { "listenLl", "LISTEN", "LL" },
    { "listenRr", "LISTEN", "RR" },
    { "listenSs", "LISTEN", "SS" },
    { "depStereo", "DELAY", "STEREO" },
    { "depLeft", "DELAY", "LEFT" },
    { "depRight", "DELAY", "RIGHT" },
    { "depPhaseL", "PHASE", "LEFT" },
    { "depPhaseR", "PHASE", "RIGHT" },
});

constexpr size_t numBands = tls::dsp::MultibandProcessor::numBands;

juce::String makeFullbandHostName(const juce::String& blockName, const juce::String& parameterName)
{
    return "TLS / " + blockName + " / " + parameterName;
}

juce::String makeBandHostName(const size_t bandIndex, const juce::String& blockName, const juce::String& parameterName)
{
    return "TLS / " + blockName + " / BAND "
        + juce::String(static_cast<int>(bandIndex + 1)) + " " + parameterName;
}

juce::String formatParameterValue(const float value, const int decimalPlaces, const bool muteAtMinimum, const float minimum)
{
    if (muteAtMinimum && value <= minimum)
        return "MUTED";

    return juce::String(value, juce::jmax(0, decimalPlaces));
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
                          const int displayDecimals,
                          const bool muteAtMinimum,
                          const bool isAutomatable,
                          const bool isMeta = false) -> Parameter
    {
        const auto minimum = min;
        auto range = juce::NormalisableRange<float> { min, max, step };
        auto attributes = juce::AudioParameterFloatAttributes()
                              .withLabel(label)
                              .withAutomatable(isAutomatable)
                              .withMeta(isMeta)
                              .withStringFromValueFunction([displayDecimals, muteAtMinimum, minimum] (float value, int)
                              {
                                  return formatParameterValue(value, displayDecimals, muteAtMinimum, minimum);
                              })
                              .withValueFromStringFunction([minimum] (const juce::String& text)
                              {
                                  if (text.trim().equalsIgnoreCase("MUTED"))
                                      return minimum;

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
                                      "TLS / SOLO / BAND " + juce::String(static_cast<int>(bandIndex + 1)),
                                      false,
                                      isHostEditableParameter));

    layout.add(std::move(soloGroup));

    auto fullbandGroup = std::make_unique<juce::AudioProcessorParameterGroup>(makeFullbandGroupId(),
                                                                              makeFullbandGroupName(),
                                                                              " | ");

    auto fullbandCrossoverGroup = std::make_unique<juce::AudioProcessorParameterGroup>("fullband_crossover",
                                                                                        "CROSSOVER",
                                                                                        " | ");

    for (const auto& entry : tlsCrossoverFullbandOrder)
    {
        const auto key = juce::String(entry.key);

        if (key == "active_split_count")
        {
            fullbandCrossoverGroup->addChild(floatParam(makeActiveSplitCountParameterId(),
                                                        makeFullbandHostName(entry.block, entry.label),
                                                        0.0f,
                                                        5.0f,
                                                        1.0f,
                                                        0.0f,
                                                        "",
                                                        0,
                                                        false,
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
                                                        makeFullbandHostName(entry.block, entry.label),
                                                        crossoverIt->min,
                                                        crossoverIt->max,
                                                        crossoverIt->step,
                                                        crossoverIt->defaultValue,
                                                        crossoverIt->label,
                                                        crossoverIt->displayDecimals,
                                                        crossoverIt->muteAtMinimum,
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

        for (const auto& entry : tlsBandOrder)
        {
            const auto it = std::find_if(parameterSpecs.begin(), parameterSpecs.end(), [&entry] (const auto& spec)
            {
                return juce::String(spec.suffix) == entry.key;
            });

            if (it == parameterSpecs.end())
                continue;

            const auto parameterId = makeBandParameterId(bandIndex, it->suffix);
            const auto parameterName = makeBandHostName(bandIndex, entry.block, entry.label);

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
                                           it->displayDecimals,
                                           it->muteAtMinimum,
                                           false));
        }

        layout.add(std::move(group));
    }

    return layout;
}
} // namespace tls::parameters
