#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

class BoxTextButton;
class ParameterControl;

class MultibandModuleComponent final : public juce::Component
{
public:
    enum class ControlKind
    {
        heading,
        parameter,
        toggle,
        readout,
        time
    };

    struct BandControlSpec
    {
        ControlKind kind = ControlKind::parameter;
        const char* suffix = "";
        const char* label = "";
        int decimals = 1;
        const char* enabledLabel = "";
        const char* disabledLabel = "";
        const char* modeSuffix = "";
        const char* syncSuffix = "";
        const char* exclusiveGroup = "";
        int topGapMultiplier = 1;
        int sourceBandIndex = -1;
    };

    struct Config
    {
        void* processorIdentity = nullptr;
        juce::String moduleKey;
        juce::AudioProcessorValueTreeState* valueTreeState = nullptr;
        juce::UndoManager* undoManager = nullptr;
        std::function<void()> markParametersDirty;
        std::function<bool()> refreshExternalState;
        std::function<bool(const juce::String&, const juce::String&, float)> assignHostSlot;
        std::function<juce::String(size_t, const char*)> makeBandParameterId;
        std::function<juce::String(const char*)> makeFullbandParameterId;
        std::function<juce::String(size_t)> makeSoloParameterId;
        std::function<juce::String()> makeActiveSplitCountParameterId;
        std::vector<BandControlSpec> bandControls;
        std::vector<BandControlSpec> bandTailControls;
        bool showAutoSolo = true;
        std::function<juce::StringArray()> getHostSyncChoices;
        std::function<int()> getDefaultHostSyncChoiceIndex;
        std::function<void(const juce::Rectangle<int>&,
                           const juce::StringArray&,
                           int,
                           std::vector<bool>,
                           juce::Justification,
                           std::function<void(int)>,
                           std::function<void()>,
                           std::function<void()>)> showChoicePrompt;
        std::function<void()> clearKeyboardFocus;
    };

    explicit MultibandModuleComponent(Config configIn);
    ~MultibandModuleComponent() override;

    void* getProcessorIdentity() const noexcept { return config.processorIdentity; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    juce::Rectangle<int> getContentBounds() const noexcept;
    void refreshCurrentPageLayout();
    void refreshExternalState();

private:
    static constexpr size_t numBands = 6;
    static constexpr size_t numCrossoverSlots = 5;
    static constexpr size_t numMonitorButtons = numBands + 1;

    class BandPageComponent;
    class FullbandPageComponent;

    void loadUiState();
    void saveUiState();
    void selectBand(size_t bandIndex);
    void toggleManualSolo(size_t bandIndex);
    void setAllBandsMonitoring();
    void setAutoSoloEnabled(bool shouldBeEnabled);
    void setManualSoloInclusive(bool shouldBeInclusive);
    void changeActiveSplitCount(int delta);
    void syncMonitorParameters();
    void updateMonitorButtons();
    void updatePageVisibility();
    void scrollPageViewport(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel);
    void updatePageViewport();
    juce::Component* getCurrentPageComponent() const noexcept;
    int getCurrentPagePreferredHeight() const noexcept;
    size_t getActiveSplitCount() const;
    size_t getActiveBandCount() const;
    bool constrainCrossoverFrequency(size_t crossoverIndex);
    bool setParameterPlainValue(const juce::String& parameterId, float plainValue);
    bool setParameterNormalisedValue(juce::RangedAudioParameter& parameter, float normalisedValue);
    bool assignButtonHostSlot(const juce::String& parameterId,
                              const juce::String& fallbackName,
                              const BoxTextButton* button,
                              const juce::ModifierKeys& modifiers);
    void clearFocus();

    Config config;
    juce::AudioProcessorValueTreeState& valueTreeState;
    std::array<std::unique_ptr<BoxTextButton>, numMonitorButtons> monitorButtons;
    std::array<juce::RangedAudioParameter*, numBands> soloParameters {};
    juce::RangedAudioParameter* activeSplitCountParameter = nullptr;
    std::array<std::unique_ptr<BandPageComponent>, numBands> bandPages;
    std::unique_ptr<FullbandPageComponent> allBandsPage;
    juce::Viewport pageViewport;
    bool uiStateLoaded = false;
    size_t visibleBandIndex = 0;
    std::array<bool, numBands> manualSoloMask {};
    bool allBandsActive = true;
    bool autoSoloEnabled = false;
    bool manualSoloInclusive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultibandModuleComponent)
};
