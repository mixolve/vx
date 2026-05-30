#pragma once

#include <JuceHeader.h>

#include "module.mxe.PluginProcessor.h"

#include <array>
#include <functional>
#include <memory>

namespace mxe::editor
{
class BandPageComponent;
class BoxTextButton;
class FullbandPageComponent;
} // namespace mxe::editor

class MxeModuleComponent final : public juce::Component
{
public:
    struct Callbacks
    {
        std::function<void(const juce::String&,
                           std::function<bool(const juce::String&)>,
                           std::function<void()>,
                           std::function<void()>)> showTextPrompt;
        std::function<void()> requestCloseActiveModule;
    };

    MxeModuleComponent(MxeAudioProcessor&, Callbacks = {});
    ~MxeModuleComponent() override;

    MxeAudioProcessor& getAudioProcessor() const noexcept { return audioProcessor; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void scrollPageViewport(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel);
    void refreshCurrentPageLayout();
    void refreshExternalState();
    void showTextPrompt(const juce::String& currentText,
                        std::function<bool(const juce::String&)> onCommit,
                        std::function<void()> onClose = {},
                        std::function<void()> onDismiss = {});
    juce::Rectangle<int> getContentBounds() const noexcept;

private:
    static constexpr size_t numBands = mxe::dsp::MultibandProcessor::numBands;
    static constexpr size_t numMonitorButtons = numBands + 1;

    void loadUiState();
    void saveUiState();

    void selectBand(size_t bandIndex);
    void toggleManualSolo(size_t bandIndex);
    void setAllBandsMonitoring();
    void setAutoSoloEnabled(bool shouldBeEnabled);
    void changeActiveSplitCount(int delta);
    void syncMonitorParameters();
    void updateMonitorButtons();
    void updateBandPageVisibility();
    juce::Rectangle<int> getTextPromptAnchorBounds() const noexcept;
    juce::Component* getCurrentPageComponent() const noexcept;
    int getCurrentPagePreferredHeight() const noexcept;
    void updatePageViewport();
    size_t getActiveSplitCount() const;
    size_t getActiveBandCount() const;

    std::array<std::unique_ptr<juce::Button>, numMonitorButtons> monitorButtons;
    std::array<juce::RangedAudioParameter*, numBands> soloParameters {};
    juce::RangedAudioParameter* activeSplitCountParameter = nullptr;
    MxeAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState* valueTreeState = nullptr;
    std::array<std::unique_ptr<mxe::editor::BandPageComponent>, numBands> bandPages;
    std::unique_ptr<mxe::editor::FullbandPageComponent> allBandsPage;
    juce::Viewport pageViewport;
    Callbacks callbacks;
    bool uiStateLoaded = false;
    size_t visibleBandIndex = 0;
    std::array<bool, numBands> manualSoloMask {};
    bool allBandsActive = true;
    bool autoSoloEnabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MxeModuleComponent)
};
