#pragma once

#include <JuceHeader.h>

#include "module.tse.TseProcessor.h"

#include <memory>

class BoxTextButton;
class ParameterControl;

class TseModuleComponent final : public juce::Component,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Callbacks
    {
        std::function<void(const juce::Rectangle<int>&,
                           const juce::StringArray&,
                           int,
                           std::vector<bool>,
                           juce::Justification,
                           std::function<void(int)>,
                           std::function<void()>,
                           std::function<void()>)> showChoicePrompt;
        std::function<void()> requestCloseActiveModule;
        std::function<void()> clearKeyboardFocus;
    };

    TseModuleComponent(TseModuleProcessor& processorToEdit, Callbacks callbacksIn = {});
    ~TseModuleComponent() override;

    TseModuleProcessor& getProcessor() const noexcept { return processor; }

    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    juce::Rectangle<int> getContentBounds() const noexcept;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    enum class TimeTarget
    {
        hold,
        release
    };
    enum class Section
    {
        misc,
        stereo
    };

    int getPreferredContentHeight() const noexcept;
    void scrollViewport(const juce::MouseWheelDetails& wheel);
    void handleSectionClick(Section section);
    void updateSectionVisibility();
    bool loadSectionExpandedState(const char* stateKey) const;
    void persistSectionExpandedState(const char* stateKey, bool expanded);
    void updateBranchButtonLabels();
    void updateTimeModeControls();
    void handleTimeModeButton(TimeTarget target);
    void showTimeSyncPrompt(TimeTarget target);
    void setParameterPlainValue(const juce::String& parameterId, float plainValue);
    bool isHostSyncMode(TimeTarget target) const noexcept;
    int getSyncChoiceIndex(TimeTarget target) const noexcept;
    juce::String getSyncChoiceText(TimeTarget target) const;
    static const char* getModeParameterId(TimeTarget target) noexcept;
    static const char* getSyncParameterId(TimeTarget target) noexcept;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void clearFocus();

    TseModuleProcessor& processor;
    juce::AudioProcessorValueTreeState& valueTreeState;
    Callbacks callbacks;
    juce::Component content;
    juce::Viewport viewport;

    std::unique_ptr<BoxTextButton> miscHeader;
    std::unique_ptr<BoxTextButton> stereoHeader;
    std::unique_ptr<juce::Component> miscSectionFrame;
    std::unique_ptr<juce::Component> stereoSectionFrame;
    std::unique_ptr<BoxTextButton> closeModuleButton;
    std::unique_ptr<BoxTextButton> bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<BoxTextButton> bypassWithGainButton;
    std::unique_ptr<ButtonAttachment> bypassWithGainAttachment;
    std::unique_ptr<BoxTextButton> transOnButton;
    std::unique_ptr<ButtonAttachment> transOnAttachment;
    std::unique_ptr<ParameterControl> transGainControl;
    std::unique_ptr<BoxTextButton> sustainOnButton;
    std::unique_ptr<ButtonAttachment> sustainOnAttachment;
    std::unique_ptr<ParameterControl> sustainGainControl;
    std::unique_ptr<ParameterControl> holdControl;
    std::unique_ptr<BoxTextButton> holdModeButton;
    std::unique_ptr<ParameterControl> releaseControl;
    std::unique_ptr<ParameterControl> releaseCurveControl;
    std::unique_ptr<BoxTextButton> releaseModeButton;
    std::unique_ptr<ParameterControl> thresholdControl;
    std::unique_ptr<ParameterControl> kneeControl;
    std::unique_ptr<ParameterControl> retriggerControl;
    std::unique_ptr<ParameterControl> inGainLrControl;
    std::unique_ptr<ParameterControl> inGainLControl;
    std::unique_ptr<ParameterControl> inGainRControl;
    std::unique_ptr<ParameterControl> wideControl;
    std::unique_ptr<ParameterControl> lookaheadControl;
    std::unique_ptr<ParameterControl> outGainControl;
    bool miscExpanded = false;
    bool stereoExpanded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TseModuleComponent)
};
