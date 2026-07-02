#include "shell.EditorParameterControls.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

#include <array>

void VxAudioProcessorEditor::loadSpeModule()
{
    if (! audioProcessor.loadModule(VxAudioProcessor::ActiveModule::spe))
        return;

    speModuleLoaded = true;
    eqeModuleLoaded = false;
    mieModuleLoaded = false;
    mxeModuleLoaded = false;
    tseModuleLoaded = false;

    shellGlobalHostExpanded = false;
    rebindActiveModuleEditors();
    updateEditorWidthState();
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

    const auto activePhaseFilterCount = getActiveSpePhaseFilterCount();
    for (auto filterIndex = 0; filterIndex < activePhaseFilterCount; ++filterIndex)
    {
        heights.push_back(rowHeight);

        if (spePhaseExpanded[static_cast<size_t>(filterIndex)])
        {
            heights.push_back(spePhaseTypeControls[static_cast<size_t>(filterIndex)] != nullptr ? spePhaseTypeControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(spePhasePlaceControls[static_cast<size_t>(filterIndex)] != nullptr ? spePhasePlaceControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(spePhaseSlopeControls[static_cast<size_t>(filterIndex)] != nullptr ? spePhaseSlopeControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(spePhaseFrequencyControls[static_cast<size_t>(filterIndex)] != nullptr ? spePhaseFrequencyControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(spePhaseBandwidthControls[static_cast<size_t>(filterIndex)] != nullptr ? spePhaseBandwidthControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(spePhaseImpactControls[static_cast<size_t>(filterIndex)] != nullptr ? spePhaseImpactControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
        }
    }

    heights.push_back(rowHeight);
    heights.push_back(rowHeight);

    const auto activeAmplitudeFilterCount = getActiveSpeAmplitudeFilterCount();
    for (auto filterIndex = 0; filterIndex < activeAmplitudeFilterCount; ++filterIndex)
    {
        heights.push_back(rowHeight);

        if (speAmplitudeExpanded[static_cast<size_t>(filterIndex)])
        {
            heights.push_back(speAmplitudeTypeControls[static_cast<size_t>(filterIndex)] != nullptr ? speAmplitudeTypeControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(speAmplitudePlaceControls[static_cast<size_t>(filterIndex)] != nullptr ? speAmplitudePlaceControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)] != nullptr ? speAmplitudeSlopeControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)] != nullptr ? speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)] != nullptr ? speAmplitudeBandwidthControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
            heights.push_back(speAmplitudeImpactControls[static_cast<size_t>(filterIndex)] != nullptr ? speAmplitudeImpactControls[static_cast<size_t>(filterIndex)]->getPreferredHeight() : 0);
        }
    }

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
    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return false;

    const auto* typeControl = spePhaseTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto typeIndex = typeControl != nullptr ? typeControl->getSelectedChoiceIndex() : 1;
    return typeIndex != 2 && typeIndex != 4;
}

bool VxAudioProcessorEditor::shouldEnableSpePhaseFrequency(const int filterIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return false;

    const auto* typeControl = spePhaseTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto typeIndex = typeControl != nullptr ? typeControl->getSelectedChoiceIndex() : 1;
    return typeIndex != 4;
}

bool VxAudioProcessorEditor::shouldEnableSpePhaseBandwidth(const int filterIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return false;

    const auto* typeControl = spePhaseTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto typeIndex = typeControl != nullptr ? typeControl->getSelectedChoiceIndex() : 1;
    return typeIndex == 1;
}

bool VxAudioProcessorEditor::shouldShowSpePhaseImpact(const int filterIndex) const noexcept
{
    return juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount);
}

int VxAudioProcessorEditor::getActiveSpeAmplitudeFilterCount() const noexcept
{
    if (const auto* speProcessor = audioProcessor.getSpeModuleProcessor())
        return speProcessor->getActiveAmplitudeFilterCount();

    return 0;
}

bool VxAudioProcessorEditor::shouldEnableSpeAmplitudeOrder(const int filterIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return false;

    const auto* typeControl = speAmplitudeTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto typeIndex = typeControl != nullptr ? typeControl->getSelectedChoiceIndex() : 1;
    return typeIndex != 2 && typeIndex != 4;
}

bool VxAudioProcessorEditor::shouldEnableSpeAmplitudeFrequency(const int filterIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return false;

    const auto* typeControl = speAmplitudeTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto typeIndex = typeControl != nullptr ? typeControl->getSelectedChoiceIndex() : 1;
    return typeIndex != 4;
}

bool VxAudioProcessorEditor::shouldEnableSpeAmplitudeBandwidth(const int filterIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return false;

    const auto* typeControl = speAmplitudeTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto typeIndex = typeControl != nullptr ? typeControl->getSelectedChoiceIndex() : 1;
    return typeIndex == 1;
}

bool VxAudioProcessorEditor::shouldShowSpeAmplitudeImpact(const int filterIndex) const noexcept
{
    return juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount);
}

void VxAudioProcessorEditor::enforceSingleExpandedSpePhaseFilter(const int preferredFilterIndex)
{
    const auto activeCount = getActiveSpePhaseFilterCount();
    const auto targetFilterIndex = juce::isPositiveAndBelow(preferredFilterIndex, activeCount) ? preferredFilterIndex : -1;

    for (auto filterIndex = 0; filterIndex < spePhaseFilterControlCount; ++filterIndex)
        spePhaseExpanded[static_cast<size_t>(filterIndex)] = filterIndex == targetFilterIndex;
}

void VxAudioProcessorEditor::enforceSingleExpandedSpeAmplitudeFilter(const int preferredFilterIndex)
{
    const auto activeCount = getActiveSpeAmplitudeFilterCount();
    const auto targetFilterIndex = juce::isPositiveAndBelow(preferredFilterIndex, activeCount) ? preferredFilterIndex : -1;

    for (auto filterIndex = 0; filterIndex < spePhaseFilterControlCount; ++filterIndex)
        speAmplitudeExpanded[static_cast<size_t>(filterIndex)] = filterIndex == targetFilterIndex;
}

juce::String VxAudioProcessorEditor::getSpePhaseFilterHeaderText(const int filterIndex) const
{
    static constexpr std::array<const char*, 5> typeNames { "LSH", "BEL", "FTL", "HSH", "FUL" };
    static constexpr std::array<const char*, 3> placeNames { "RTL", "LTR", "50" };

    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return {};

    const auto* typeControl = spePhaseTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto* placeControl = spePhasePlaceControls[static_cast<size_t>(filterIndex)].get();
    const auto* frequencyControl = spePhaseFrequencyControls[static_cast<size_t>(filterIndex)].get();

    const auto typeIndex = typeControl != nullptr ? juce::jlimit(0, static_cast<int>(typeNames.size()) - 1, typeControl->getSelectedChoiceIndex()) : 1;
    const auto placeIndex = placeControl != nullptr ? juce::jlimit(0, static_cast<int>(placeNames.size()) - 1, placeControl->getSelectedChoiceIndex()) : 0;
    const auto frequency = typeIndex == 4 ? 0 : (frequencyControl != nullptr ? juce::roundToInt(frequencyControl->getValue()) : 632);

    return juce::String::formatted("%02d-%s-%s-%05d",
                                   filterIndex + 1,
                                   typeNames[static_cast<size_t>(typeIndex)],
                                   placeNames[static_cast<size_t>(placeIndex)],
                                   juce::jlimit(0, 99999, frequency));
}

juce::String VxAudioProcessorEditor::getSpeAmplitudeFilterHeaderText(const int filterIndex) const
{
    static constexpr std::array<const char*, 5> typeNames { "LSH", "BEL", "FTL", "HSH", "FUL" };
    static constexpr std::array<const char*, 3> placeNames { "RTL", "LTR", "50" };

    if (! juce::isPositiveAndBelow(filterIndex, spePhaseFilterControlCount))
        return {};

    const auto* typeControl = speAmplitudeTypeControls[static_cast<size_t>(filterIndex)].get();
    const auto* placeControl = speAmplitudePlaceControls[static_cast<size_t>(filterIndex)].get();
    const auto* frequencyControl = speAmplitudeFrequencyControls[static_cast<size_t>(filterIndex)].get();

    const auto typeIndex = typeControl != nullptr ? juce::jlimit(0, static_cast<int>(typeNames.size()) - 1, typeControl->getSelectedChoiceIndex()) : 1;
    const auto placeIndex = placeControl != nullptr ? juce::jlimit(0, static_cast<int>(placeNames.size()) - 1, placeControl->getSelectedChoiceIndex()) : 0;
    const auto frequency = typeIndex == 4 ? 0 : (frequencyControl != nullptr ? juce::roundToInt(frequencyControl->getValue()) : 632);

    return juce::String::formatted("%02d-%s-%s-%05d",
                                   filterIndex + 1,
                                   typeNames[static_cast<size_t>(typeIndex)],
                                   placeNames[static_cast<size_t>(placeIndex)],
                                   juce::jlimit(0, 99999, frequency));
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

    return sumHeights({ speAnalyserFftSizeControl != nullptr ? speAnalyserFftSizeControl->getPreferredHeight() : 0,
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
