#pragma once

#include "module.eql.Processor.h"

#include <memory>

inline constexpr auto minimumBiquadQ = 0.025f;
inline constexpr auto maximumBiquadQ = 40.0f;
inline constexpr auto minimumBellBandwidth = 0.01f;
inline constexpr auto maximumBellBandwidth = 8.0f;
inline constexpr auto minimumVisibleFilterFrequency = 20.0f;
inline constexpr auto maximumVisibleFilterFrequency = 30000.0f;
inline constexpr auto maximumLowCutFrequency = 20000.0f;
inline constexpr auto minimumDesignFilterFrequency = 2.0;
inline constexpr auto highFrequencyExtensionStart = 20000.0;
inline constexpr auto lowFrequencyExtensionEnd = 25.0;
inline constexpr auto defaultFilterFrequencyHz = 632.46f;
inline constexpr auto nyquistSafetyFactor = 0.98;
inline constexpr auto flatTiltStageCount = 16;
inline const juce::StringArray filterTypeChoices { "LCT", "LSH", "BEL", "FTL", "HSH", "HCT", "VOL" };
inline const juce::StringArray filterPlaceChoices { "LR", "LL", "RR", "MM", "SS", "PHS", "PHL", "PHR" };

inline constexpr auto filterPresetsRootTag = "FILTER_PRESETS";
inline constexpr auto presetTag = "PRESET";
inline constexpr auto presetStorageVendorFolder = "mixolve";
inline constexpr auto presetStorageProductFolder = "ava";
inline constexpr auto eqlPresetStorageModuleFolder = "eql";
inline constexpr auto presetStorageRootFolder = "presets";
inline constexpr auto eqlAppGroupIdentifier = "group.com.mixolve.ava";

struct ShelfSlopeBlend
{
    int lowerOrder = 1;
    int upperOrder = 1;
    double blend = 0.0;
};

float defaultFilterFrequency();
float defaultFilterBandwidth();
float defaultFilterSlope();

juce::String formatDecibelValue(float value);
juce::String formatFrequencyValue(float value);
juce::String formatBandwidthValue(float value);
juce::String makeFilterParameterId(const char* suffix, int filterIndex);
juce::StringArray getBellSlopeDisplayChoicesForType(EqlModuleProcessor::FilterType type) noexcept;
int clampActiveFilterCount(int filterCount);
juce::String filterTypeDisplayPrefix(EqlModuleProcessor::FilterType type);

std::unique_ptr<juce::XmlElement> loadFilterPresetsXml();
std::unique_ptr<juce::XmlElement> createEmptyFilterPresetsXml();
bool writeFilterPresetsXml(const juce::XmlElement& rootElement);
juce::XmlElement* findPresetElement(juce::XmlElement& rootElement, const juce::String& presetName);
std::unique_ptr<juce::XmlElement> createSerializableStateXml(juce::AudioProcessorValueTreeState& parameters,
                                                             int activeFilterCount);
std::unique_ptr<juce::XmlElement> createSerializableStateXml(EqlModuleProcessor& processor);
juce::File getEqlAppGroupContainerDirectory();
void syncEqlPresetStorageWithSharedContainer();

bool isShelfFilterType(EqlModuleProcessor::FilterType type) noexcept;
bool isCutFilterType(EqlModuleProcessor::FilterType type) noexcept;
bool isTiltFilterType(EqlModuleProcessor::FilterType type) noexcept;
bool isVolumeFilterType(EqlModuleProcessor::FilterType type) noexcept;
bool isPhasePlaceChoice(int choiceIndex) noexcept;
ShelfSlopeBlend mapBellSlopeToBlend(double slope) noexcept;
ShelfSlopeBlend mapShelfSlopeToBlend(double slope) noexcept;
ShelfSlopeBlend mapCutSlopeToBlend(EqlModuleProcessor::FilterType type, double slope) noexcept;
double computeButterworthStageQ(int biquadStageIndex, int order) noexcept;
double computeDesignFrequency(double displayedFrequency, double sampleRate) noexcept;
double mapBandwidthToShelfShape(double octaveBandwidth) noexcept;
