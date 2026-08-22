#pragma once

#include <JuceHeader.h>

#include <memory>

class CrossoverModuleComponent;

class CrossoverModulePage : public juce::Component
{
public:
    ~CrossoverModulePage() override = default;

    virtual void refreshExternalState() = 0;
    virtual int getPreferredHeight() const = 0;
};

std::unique_ptr<CrossoverModulePage> makeCrossoverRangePage(CrossoverModuleComponent& owner,
                                                            size_t rangeIndex,
                                                            juce::Colour accent);
std::unique_ptr<CrossoverModulePage> makeCrossoverSettingsPage(CrossoverModuleComponent& owner);
