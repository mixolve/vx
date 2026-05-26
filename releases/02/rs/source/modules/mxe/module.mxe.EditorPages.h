#pragma once

#include "module.mxe.EditorSection.h"

#include <functional>
#include <memory>

namespace mxe::editor
{
class BandPageComponent final : public juce::Component
{
public:
    BandPageComponent(juce::AudioProcessorValueTreeState& state,
                      size_t bandIndex,
                      const std::function<juce::String(const char*)>& parameterIdProvider,
                      juce::Colour accent,
                      std::function<void()> onSoloClickIn = {},
                      std::function<bool()> isSoloActiveIn = {},
                      std::function<bool()> isSoloEnabledIn = {});

    void refreshExternalState();
    int getPreferredHeight() const;
    void resized() override;

private:
    void handleSectionExpanded(int sectionIndex, bool expanded);
    void refreshLayout();

    juce::AudioProcessorValueTreeState* valueTreeState = nullptr;
    size_t bandIndex = 0;
    std::function<void()> onSoloClick;
    std::function<bool()> isSoloActive;
    std::function<bool()> isSoloEnabled;
    SectionComponent misc;
    SectionComponent main;
    SectionComponent halfWave;
    SectionComponent dm;
    SectionComponent ff;
};

class FullbandPageComponent final : public juce::Component
{
public:
    FullbandPageComponent(juce::AudioProcessorValueTreeState& state,
                          const std::function<juce::String(const char*)>& parameterIdProvider,
                          bool autoSoloEnabled,
                          std::function<size_t()> activeSplitCountProviderIn,
                          std::function<void(int)> onActiveSplitCountChangeIn,
                          std::function<void(bool)> onAutoSoloChangedIn,
                          std::function<void()> onCloseModuleIn);

    int getPreferredHeight() const;
    void refreshExternalState();
    void resized() override;

private:
    bool isAutoSoloAvailable() const;
    void refreshAutoSoloButtonState();
    void refreshCrossoverButtonState();
    void persistAutoSoloState();
    void refreshLayout();

    juce::AudioProcessorValueTreeState* valueTreeState = nullptr;
    std::function<size_t()> activeSplitCountProvider;
    SectionComponent misc;
    SectionComponent crossover;
    BoxTextButton autoSoloButton;
    BoxTextButton closeModuleButton;
    BoxTextButton addCrossoverButton;
    BoxTextButton removeCrossoverButton;
    std::function<void(int)> onActiveSplitCountChange;
    std::function<void(bool)> onAutoSoloChanged;
    std::function<void()> onCloseModule;
};
} // namespace mxe::editor
