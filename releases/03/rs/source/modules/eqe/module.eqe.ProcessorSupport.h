#pragma once

#include "module.eqe.Processor.h"

#include <memory>

inline constexpr auto minimumBiquadQ = 0.025f;
inline constexpr auto maximumBiquadQ = 40.0f;
inline constexpr auto minimumBellBandwidth = 0.01f;
inline constexpr auto maximumBellBandwidth = 8.0f;
inline constexpr auto minimumVisibleFilterFrequency = 20.0f;
inline constexpr auto maximumVisibleFilterFrequency = 30000.0f;
inline constexpr auto minimumDesignFilterFrequency = 2.0;
inline constexpr auto highFrequencyExtensionStart = 16000.0;
inline constexpr auto lowFrequencyExtensionEnd = 25.0;
inline constexpr auto defaultTiltFrequency = 632.0f;
inline constexpr auto nyquistSafetyFactor = 0.98;
inline constexpr auto flatTiltStageCount = 16;
inline const juce::StringArray filterTypeChoices { "LCT", "LSH", "BEL", "FTL", "HSH", "HCT" };
inline const juce::StringArray filterTtssChoices { "TS", "TT", "SS" };
inline const juce::StringArray filterLrmsChoices { "LR", "LL", "RR", "MM", "SS" };

inline constexpr auto filterPresetsRootTag = "FILTER_PRESETS";
inline constexpr auto presetTag = "PRESET";
inline constexpr auto presetStorageVendorFolder = "mixolve";
inline constexpr auto presetStorageProductFolder = "vx";
inline constexpr auto eqePresetStorageModuleFolder = "eqe";
inline constexpr auto presetStorageRootFolder = "presets";
inline constexpr auto eqeAppGroupIdentifier = "group.com.mixolve.vx";

struct ShelfSlopeBlend
{
    int lowerOrder = 1;
    int upperOrder = 1;
    double blend = 0.0;
};

float defaultFilterFrequencyForType(EqeModuleProcessor::FilterType type);
float defaultFilterBandwidthForType(EqeModuleProcessor::FilterType type);
float defaultFilterSlopeForType(EqeModuleProcessor::FilterType type);

juce::String formatDecibelValue(float value);
juce::String formatFrequencyValue(float value);
juce::String formatBandwidthValue(float value);
juce::String makeFilterTypeName(int filterIndex);
juce::String makeFilterTtssName(int filterIndex);
juce::String makeFilterLrmsName(int filterIndex);
juce::String makeFilterParameterId(const char* suffix, int filterIndex);
juce::String makeBellParameterName(const char* suffix, int bellIndex);
juce::StringArray getBellSlopeDisplayChoicesForType(EqeModuleProcessor::FilterType type) noexcept;
int clampActiveBellCount(int bellCount);
juce::String filterTypeDisplayPrefix(EqeModuleProcessor::FilterType type);

std::unique_ptr<juce::XmlElement> loadFilterPresetsXml();
std::unique_ptr<juce::XmlElement> createEmptyFilterPresetsXml();
bool writeFilterPresetsXml(const juce::XmlElement& rootElement);
juce::XmlElement* findPresetElement(juce::XmlElement& rootElement, const juce::String& presetName);
std::unique_ptr<juce::XmlElement> createSerializableStateXml(juce::AudioProcessorValueTreeState& parameters,
                                                             int activeBellCount);
std::unique_ptr<juce::XmlElement> createSerializableStateXml(const EqeModuleProcessor& processor);
void rewriteFilterParameterIds(juce::XmlElement& element, bool toStorageFormat);
juce::File getEqeAppGroupContainerDirectory();

bool isShelfFilterType(EqeModuleProcessor::FilterType type) noexcept;
bool isCutFilterType(EqeModuleProcessor::FilterType type) noexcept;
bool isTiltFilterType(EqeModuleProcessor::FilterType type) noexcept;
ShelfSlopeBlend mapBellSlopeToBlend(double slope) noexcept;
ShelfSlopeBlend mapShelfSlopeToBlend(double slope) noexcept;
ShelfSlopeBlend mapCutSlopeToBlend(EqeModuleProcessor::FilterType type, double slope) noexcept;
double computeButterworthStageQ(int biquadStageIndex, int order) noexcept;
double computeDesignFrequency(double displayedFrequency, double sampleRate) noexcept;
double mapBandwidthToShelfShape(double octaveBandwidth) noexcept;
