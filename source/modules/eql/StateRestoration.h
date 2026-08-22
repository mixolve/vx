#pragma once

#include <JuceHeader.h>

#include <memory>

namespace eql_state
{
void copyXmlAttributesToValueTreeProperties(const juce::XmlElement& sourceElement,
                                            juce::ValueTree& targetState);
void applyParameterValuesFromStateElement(juce::AudioProcessorValueTreeState& parameters,
                                          const juce::XmlElement& stateElement);
std::unique_ptr<juce::XmlElement> createCompleteRestoredStateElement(
    const juce::XmlElement& sparseStateElement,
    juce::AudioProcessorValueTreeState& parameters);
void normalizeRestoredStateElement(juce::XmlElement& stateElement,
                                   juce::AudioProcessorValueTreeState& parameters);
}
