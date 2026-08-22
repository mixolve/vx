#pragma once

#include "ModuleComponent.h"

#include <JuceHeader.h>

#include <memory>

class AvaAudioProcessor;
class AvaAudioProcessorEditor;
class DynAudioProcessor;
class TlsAudioProcessor;
class TrsModuleProcessor;

namespace crossover_configs
{
CrossoverModuleComponent::Config makeCrossoverConfig(AvaAudioProcessor& processor);
CrossoverModuleComponent::Config makeTlsCrossoverConfig(TlsAudioProcessor& processor);
CrossoverModuleComponent::Config makeDynCrossoverConfig(DynAudioProcessor& processor);
CrossoverModuleComponent::Config makeTrsCrossoverConfig(TrsModuleProcessor& processor,
                                                        AvaAudioProcessorEditor& editor,
                                                        std::unique_ptr<juce::Component>& editorHolder);
} // namespace crossover_configs
