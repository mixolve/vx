#pragma once

#include "shell.EditorParameterControls.h"

#include <array>

struct VxAudioProcessorEditor::BellSection
{
    using FilterType = EqeModuleProcessor::FilterType;

    explicit BellSection(juce::AudioProcessorValueTreeState& state, int bandIndex);

    void rebind(juce::AudioProcessorValueTreeState& state);
    FilterType getFilterType() const noexcept;
    int getPlace() const noexcept;
    double getFrequency() const noexcept;
    bool isBandwidthInactiveAtCurrentSlope() const noexcept;
    bool isSlopeInactive() const noexcept;
    bool isGainInactive() const noexcept;

    void setStoredValues(FilterType type,
                         double frequency,
                         double bandwidth,
                         double slope,
                         int lrms,
                         bool isCustom = false) noexcept;
    int getStoredLrms(FilterType type) const noexcept;
    void captureCurrentValuesForType(FilterType type, bool markCustom = true) noexcept;
    void captureCurrentValuesForCurrentType(bool markCustom = true) noexcept;
    void copyStoredValuesFrom(const BellSection& other) noexcept;

    std::unique_ptr<BoxTextButton> moveUpButton;
    std::unique_ptr<BoxTextButton> header;
    std::unique_ptr<BoxTextButton> moveDownButton;
    std::unique_ptr<ChoiceControl> typeControl;
    std::unique_ptr<ChoiceControl> ttssControl;
    std::unique_ptr<ChoiceControl> lrmsControl;
    std::unique_ptr<ChoiceControl> slopeControl;
    std::unique_ptr<ParameterControl> frequencyControl;
    std::unique_ptr<ParameterControl> bandwidthControl;
    std::unique_ptr<ParameterControl> gainControl;
    std::unique_ptr<BoxTextButton> bypassButton;
    std::unique_ptr<BoxTextButton> deleteButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::AudioParameterChoice* typeParameter = nullptr;
    juce::AudioParameterChoice* lrmsParameter = nullptr;
    juce::AudioParameterFloat* frequencyParameter = nullptr;
    juce::AudioParameterFloat* bandwidthParameter = nullptr;
    juce::AudioParameterChoice* slopeParameter = nullptr;
    juce::AudioParameterFloat* gainParameter = nullptr;
    int bandIndex = 0;
    std::array<double, 6> storedFrequencies { 40.0, 120.0, 632.0, 632.0, 5000.0, 12000.0 };
    std::array<double, 6> storedBandwidths { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
    std::array<double, 6> storedSlopes { 12.0, 12.0, 12.0, 12.0, 12.0, 12.0 };
    std::array<int, 6> storedLrms { 0, 0, 0, 0, 0, 0 };
    std::array<bool, 6> storedValuesCustom { false, false, false, false, false, false };
    FilterType lastFilterType = FilterType::bell;
    bool suppressStoredValueCapture = false;

};
