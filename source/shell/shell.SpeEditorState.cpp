#include "shell.EditorParameterControls.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

#include <array>

namespace
{
template <typename TypeControls, typename PlaceControls, typename FrequencyControls>
juce::String formatSpeFilterHeaderText(const int filterIndex,
                                       const int filterControlCount,
                                       const TypeControls& typeControls,
                                       const PlaceControls& placeControls,
                                       const FrequencyControls& frequencyControls)
{
    static constexpr std::array<const char*, 5> typeNames { "LSH", "BEL", "FTL", "HSH", "FUL" };
    static constexpr std::array<const char*, 3> placeNames { "RTL", "LTR", "50" };

    if (! juce::isPositiveAndBelow(filterIndex, filterControlCount))
        return {};

    const auto* typeControl = typeControls[static_cast<size_t>(filterIndex)].get();
    const auto* placeControl = placeControls[static_cast<size_t>(filterIndex)].get();
    const auto* frequencyControl = frequencyControls[static_cast<size_t>(filterIndex)].get();

    const auto typeIndex = typeControl != nullptr ? juce::jlimit(0, static_cast<int>(typeNames.size()) - 1, typeControl->getSelectedChoiceIndex()) : 1;
    const auto placeIndex = placeControl != nullptr ? juce::jlimit(0, static_cast<int>(placeNames.size()) - 1, placeControl->getSelectedChoiceIndex()) : 0;
    const auto frequency = typeIndex == 4 ? 0 : (frequencyControl != nullptr ? juce::roundToInt(frequencyControl->getValue()) : 632);

    return juce::String::formatted("%02d-%s-%s-%05d",
                                   filterIndex + 1,
                                   typeNames[static_cast<size_t>(typeIndex)],
                                   placeNames[static_cast<size_t>(placeIndex)],
                                   juce::jlimit(0, 99999, frequency));
}

template <typename TypeControls>
int getSpeFilterTypeIndex(const int filterIndex,
                          const int filterControlCount,
                          const TypeControls& typeControls) noexcept
{
    if (! juce::isPositiveAndBelow(filterIndex, filterControlCount))
        return -1;

    const auto* typeControl = typeControls[static_cast<size_t>(filterIndex)].get();
    return typeControl != nullptr ? typeControl->getSelectedChoiceIndex() : 1;
}

bool shouldEnableSpeFilterOrder(const int typeIndex) noexcept
{
    return typeIndex >= 0 && typeIndex != 2 && typeIndex != 4;
}

bool shouldEnableSpeFilterFrequency(const int typeIndex) noexcept
{
    return typeIndex >= 0 && typeIndex != 4;
}

bool shouldEnableSpeFilterBandwidth(const int typeIndex) noexcept
{
    return typeIndex == 1;
}
} // namespace

void VxAudioProcessorEditor::loadSpeModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::spe))
        return;

    setLoadedModuleFlags(VxAudioProcessor::ActiveModule::spe);

    hostParametersExpanded = false;
    rebindActiveModuleEditors();
    syncEditorWidthToBounds();
    refreshModuleTabButton();
    storeEditorStateToValueTree();
    updateSectionStates();
    resized();
    scheduleHistorySnapshot();
}

int VxAudioProcessorEditor::getSpeMainContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    auto sumHeights = [] (const std::vector<int>& heights)
    {
        auto totalHeight = 0;

        for (const auto height : heights)
            totalHeight += height;

        return totalHeight + (verticalGap * juce::jmax(0, static_cast<int>(heights.size()) - 1));
    };

    std::vector<int> heights {
        rowHeight,
        speDspFftSizeControl != nullptr ? speDspFftSizeControl->getPreferredHeight() : 0,
        speDspHopDivisorControl != nullptr ? speDspHopDivisorControl->getPreferredHeight() : 0,
        rowHeight,
        speAttackControl != nullptr ? speAttackControl->getPreferredHeight() : 0,
        speReleaseControl != nullptr ? speReleaseControl->getPreferredHeight() : 0,
        speKneeControl != nullptr ? speKneeControl->getPreferredHeight() : 0,
        speRatioControl != nullptr ? speRatioControl->getPreferredHeight() : 0,
        speDspSlopeControl != nullptr ? speDspSlopeControl->getPreferredHeight() : 0,
        speDualMonoLeftThresholdControl != nullptr ? speDualMonoLeftThresholdControl->getPreferredHeight() : 0,
        speDualMonoLeftAdaptiveControl != nullptr ? speDualMonoLeftAdaptiveControl->getPreferredHeight() : 0,
        speDualMonoLeftAdaptiveOffsetControl != nullptr ? speDualMonoLeftAdaptiveOffsetControl->getPreferredHeight() : 0,
        speDualMonoRightThresholdControl != nullptr ? speDualMonoRightThresholdControl->getPreferredHeight() : 0,
        speDualMonoRightAdaptiveControl != nullptr ? speDualMonoRightAdaptiveControl->getPreferredHeight() : 0,
        speDualMonoRightAdaptiveOffsetControl != nullptr ? speDualMonoRightAdaptiveOffsetControl->getPreferredHeight() : 0,
        rowHeight,
        rowHeight
    };

    const auto appendSpeFilterHeights = [&heights] (const int activeFilterCount,
                                                    const auto& expandedStates,
                                                    const auto& typeControls,
                                                    const auto& placeControls,
                                                    const auto& slopeControls,
                                                    const auto& frequencyControls,
                                                    const auto& bandwidthControls,
                                                    const auto& impactControls)
    {
        for (auto filterIndex = 0; filterIndex < activeFilterCount; ++filterIndex)
        {
            heights.push_back(rowHeight);

            if (expandedStates[static_cast<size_t>(filterIndex)])
            {
                heights.push_back(typeControls[static_cast<size_t>(filterIndex)] != nullptr ? typeControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
                heights.push_back(placeControls[static_cast<size_t>(filterIndex)] != nullptr ? placeControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
                heights.push_back(slopeControls[static_cast<size_t>(filterIndex)] != nullptr ? slopeControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
                heights.push_back(frequencyControls[static_cast<size_t>(filterIndex)] != nullptr ? frequencyControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
                heights.push_back(bandwidthControls[static_cast<size_t>(filterIndex)] != nullptr ? bandwidthControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
                heights.push_back(impactControls[static_cast<size_t>(filterIndex)] != nullptr ? impactControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            }
        }
    };

    appendSpeFilterHeights(getActiveSpePhaseFilterCount(),
                           spePhaseExpanded,
                           spePhaseTypeControls,
                           spePhasePlaceControls,
                           spePhaseSlopeControls,
                           spePhaseFrequencyControls,
                           spePhaseBandwidthControls,
                           spePhaseImpactControls);

    heights.push_back(rowHeight);
    heights.push_back(rowHeight);

    appendSpeFilterHeights(getActiveSpeAmplitudeFilterCount(),
                           speAmplitudeExpanded,
                           speAmplitudeTypeControls,
                           speAmplitudePlaceControls,
                           speAmplitudeSlopeControls,
                           speAmplitudeFrequencyControls,
                           speAmplitudeBandwidthControls,
                           speAmplitudeImpactControls);

    heights.push_back(0);
    heights.push_back(rowHeight);
    return sumHeights(heights);
}

int VxAudioProcessorEditor::getActiveSpePhaseFilterCount() const noexcept
{
    if (const auto* speProcessor = audioProcessor.getSpeModuleProcessor())
        return speProcessor->getActivePhaseFilterCount();

    return 0;
}

bool VxAudioProcessorEditor::shouldEnableSpePhaseOrder(const int filterIndex) const noexcept
{
    return shouldEnableSpeFilterOrder(getSpeFilterTypeIndex(filterIndex, speFilterControlCount, spePhaseTypeControls));
}

bool VxAudioProcessorEditor::shouldEnableSpePhaseFrequency(const int filterIndex) const noexcept
{
    return shouldEnableSpeFilterFrequency(getSpeFilterTypeIndex(filterIndex, speFilterControlCount, spePhaseTypeControls));
}

bool VxAudioProcessorEditor::shouldEnableSpePhaseBandwidth(const int filterIndex) const noexcept
{
    return shouldEnableSpeFilterBandwidth(getSpeFilterTypeIndex(filterIndex, speFilterControlCount, spePhaseTypeControls));
}

bool VxAudioProcessorEditor::shouldShowSpePhaseImpact(const int filterIndex) const noexcept
{
    return juce::isPositiveAndBelow(filterIndex, speFilterControlCount);
}

int VxAudioProcessorEditor::getActiveSpeAmplitudeFilterCount() const noexcept
{
    if (const auto* speProcessor = audioProcessor.getSpeModuleProcessor())
        return speProcessor->getActiveAmplitudeFilterCount();

    return 0;
}

bool VxAudioProcessorEditor::shouldEnableSpeAmplitudeOrder(const int filterIndex) const noexcept
{
    return shouldEnableSpeFilterOrder(getSpeFilterTypeIndex(filterIndex, speFilterControlCount, speAmplitudeTypeControls));
}

bool VxAudioProcessorEditor::shouldEnableSpeAmplitudeFrequency(const int filterIndex) const noexcept
{
    return shouldEnableSpeFilterFrequency(getSpeFilterTypeIndex(filterIndex, speFilterControlCount, speAmplitudeTypeControls));
}

bool VxAudioProcessorEditor::shouldEnableSpeAmplitudeBandwidth(const int filterIndex) const noexcept
{
    return shouldEnableSpeFilterBandwidth(getSpeFilterTypeIndex(filterIndex, speFilterControlCount, speAmplitudeTypeControls));
}

bool VxAudioProcessorEditor::shouldShowSpeAmplitudeImpact(const int filterIndex) const noexcept
{
    return juce::isPositiveAndBelow(filterIndex, speFilterControlCount);
}

void VxAudioProcessorEditor::enforceSingleExpandedSpePhaseFilter(const int preferredFilterIndex)
{
    const auto activeCount = getActiveSpePhaseFilterCount();
    const auto targetFilterIndex = juce::isPositiveAndBelow(preferredFilterIndex, activeCount) ? preferredFilterIndex : -1;

    for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
        spePhaseExpanded[static_cast<size_t>(filterIndex)] = filterIndex == targetFilterIndex;
}

void VxAudioProcessorEditor::enforceSingleExpandedSpeAmplitudeFilter(const int preferredFilterIndex)
{
    const auto activeCount = getActiveSpeAmplitudeFilterCount();
    const auto targetFilterIndex = juce::isPositiveAndBelow(preferredFilterIndex, activeCount) ? preferredFilterIndex : -1;

    for (auto filterIndex = 0; filterIndex < speFilterControlCount; ++filterIndex)
        speAmplitudeExpanded[static_cast<size_t>(filterIndex)] = filterIndex == targetFilterIndex;
}

juce::String VxAudioProcessorEditor::getSpePhaseFilterHeaderText(const int filterIndex) const
{
    return formatSpeFilterHeaderText(filterIndex,
                                     speFilterControlCount,
                                     spePhaseTypeControls,
                                     spePhasePlaceControls,
                                     spePhaseFrequencyControls);
}

juce::String VxAudioProcessorEditor::getSpeAmplitudeFilterHeaderText(const int filterIndex) const
{
    return formatSpeFilterHeaderText(filterIndex,
                                     speFilterControlCount,
                                     speAmplitudeTypeControls,
                                     speAmplitudePlaceControls,
                                     speAmplitudeFrequencyControls);
}

int VxAudioProcessorEditor::getSpeAnalyserContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    auto sumHeights = [] (std::initializer_list<int> heights)
    {
        auto totalHeight = 0;

        for (const auto height : heights)
            totalHeight += height;

        return totalHeight + (verticalGap * juce::jmax(0, static_cast<int>(heights.size()) - 1));
    };

    return sumHeights({ speAnalyserSettingsHeader != nullptr ? rowHeight : 0,
                        speAnalyserFftSizeControl != nullptr ? speAnalyserFftSizeControl->getPreferredHeight() : 0,
                        speAnalyserOverlapControl != nullptr ? speAnalyserOverlapControl->getPreferredHeight() : 0,
                        speAnalyserLeftControl != nullptr ? speAnalyserLeftControl->getPreferredHeight() : 0,
                        speAnalyserRightControl != nullptr ? speAnalyserRightControl->getPreferredHeight() : 0,
                        speAnalyserRangeLowControl != nullptr ? speAnalyserRangeLowControl->getPreferredHeight() : 0,
                        speAnalyserRangeHighControl != nullptr ? speAnalyserRangeHighControl->getPreferredHeight() : 0,
                        speAnalyserSlopeControl != nullptr ? speAnalyserSlopeControl->getPreferredHeight() : 0,
                        speAnalyserTimeControl != nullptr ? speAnalyserTimeControl->getPreferredHeight() : 0 });
}

int VxAudioProcessorEditor::getSpeSectionContentHeight() const
{
    if (! speModuleLoaded)
        return 0;

    return getSpeMainContentHeight() + moduleContentBottomGap;
}
