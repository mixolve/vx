#include "shell.SetupSupport.h"
#include "shell.Editor.h"
#include "shell.UiStyle.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

#include <cmath>

namespace
{
class SpeSpectrumAnalyserComponent final : public juce::Component
{
public:
    explicit SpeSpectrumAnalyserComponent(SpeModuleProcessor& processorIn)
        : processor(processorIn)
    {
        setInterceptsMouseClicks(false, false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void refreshResponse()
    {
        processor.copyAnalyserData(scopeData, sampleRate);
        processor.copyGainReductionData(leftGainReductionData, rightGainReductionData);
        displaySettings = processor.getDisplaySettings();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto plotBounds = bounds;

        g.setColour(uiGrey800);
        g.fillRect(bounds);
        g.fillRect(plotBounds);

        if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
            return;

        juce::Path postSpectrumPath;
        juce::Path leftReductionPath;
        juce::Path rightReductionPath;
        juce::Path leftThresholdPath;
        juce::Path rightThresholdPath;

        const auto sourceMaximumHz = juce::jlimit(21.0f,
                                                  22000.0f,
                                                  static_cast<float>(sampleRate * 0.5));
        const auto minimumHz = displaySettings.leftFrequencyHz <= 0.0f
            ? 20.0f
            : juce::jlimit(20.0f, sourceMaximumHz - 1.0f, displaySettings.leftFrequencyHz);
        const auto maximumHz = juce::jlimit(minimumHz + 1.0f,
                                            sourceMaximumHz,
                                            displaySettings.rightFrequencyHz);

        for (auto index = 0; index < static_cast<int>(SpeModuleProcessor::analyserScopeSize); ++index)
        {
            const auto proportion = static_cast<float>(index) / static_cast<float>(SpeModuleProcessor::analyserScopeSize - 1);
            const auto x = plotBounds.getX() + proportion * plotBounds.getWidth();
            const auto frequency = juce::mapToLog10(proportion, minimumHz, maximumHz);
            const auto sampledDecibels = sampleScopeAtFrequency(scopeData, frequency, sourceMaximumHz);
            const auto sampledLeftReductionDb = juce::jmax(0.0f,
                                                           sampleScopeAtFrequency(leftGainReductionData,
                                                                                  frequency,
                                                                                  sourceMaximumHz));
            const auto sampledRightReductionDb = juce::jmax(0.0f,
                                                            sampleScopeAtFrequency(rightGainReductionData,
                                                                                   frequency,
                                                                                   sourceMaximumHz));
            const auto octavesFromSlopeReference = std::log2(frequency / 632.455532f);
            const auto displaySlopeOffset = displaySettings.slopeDbPerOct * octavesFromSlopeReference;
            const auto postDecibels = sampledDecibels + displaySlopeOffset;
            const auto postY = decibelsToY(postDecibels, plotBounds);
            const auto leftThresholdY = decibelsToY(displaySettings.leftThresholdDb, plotBounds);
            const auto rightThresholdY = decibelsToY(displaySettings.rightThresholdDb, plotBounds);
            const auto leftReductionY = decibelsToY(displaySettings.leftThresholdDb - sampledLeftReductionDb, plotBounds);
            const auto rightReductionY = decibelsToY(displaySettings.rightThresholdDb - sampledRightReductionDb, plotBounds);

            if (index == 0)
            {
                postSpectrumPath.startNewSubPath(x, postY);
                leftThresholdPath.startNewSubPath(x, leftThresholdY);
                rightThresholdPath.startNewSubPath(x, rightThresholdY);
                leftReductionPath.startNewSubPath(x, leftThresholdY);
                leftReductionPath.lineTo(x, leftReductionY);
                rightReductionPath.startNewSubPath(x, rightThresholdY);
                rightReductionPath.lineTo(x, rightReductionY);
            }
            else
            {
                postSpectrumPath.lineTo(x, postY);
                leftThresholdPath.lineTo(x, leftThresholdY);
                rightThresholdPath.lineTo(x, rightThresholdY);
                leftReductionPath.lineTo(x, leftReductionY);
                rightReductionPath.lineTo(x, rightReductionY);
            }
        }

        juce::Path spectrumFillPath(postSpectrumPath);
        spectrumFillPath.lineTo(plotBounds.getRight(), plotBounds.getBottom());
        spectrumFillPath.lineTo(plotBounds.getX(), plotBounds.getBottom());
        spectrumFillPath.closeSubPath();

        for (auto index = static_cast<int>(SpeModuleProcessor::analyserScopeSize) - 1; index >= 0; --index)
        {
            const auto proportion = static_cast<float>(index) / static_cast<float>(SpeModuleProcessor::analyserScopeSize - 1);
            const auto x = plotBounds.getX() + proportion * plotBounds.getWidth();
            leftReductionPath.lineTo(x, decibelsToY(displaySettings.leftThresholdDb, plotBounds));
            rightReductionPath.lineTo(x, decibelsToY(displaySettings.rightThresholdDb, plotBounds));
        }

        leftReductionPath.closeSubPath();
        rightReductionPath.closeSubPath();

        g.setColour(uiGrey500);
        g.drawRect(plotBounds, 1.0f);
        g.setColour(uiAccent);
        g.fillPath(spectrumFillPath);
        g.setColour(analyserLeftColour);
        g.fillPath(leftReductionPath);
        g.setColour(analyserRightColour);
        g.fillPath(rightReductionPath);
        g.setColour(analyserLeftColour);
        g.strokePath(leftThresholdPath, juce::PathStrokeType(1.0f));
        g.setColour(analyserRightColour);
        g.strokePath(rightThresholdPath, juce::PathStrokeType(1.0f));
    }

private:
    static float sampleScopeAtFrequency(const std::array<float, SpeModuleProcessor::analyserScopeSize>& data,
                                        const float frequency,
                                        const float sourceMaximumHz)
    {
        const auto clampedFrequency = juce::jlimit(20.0f, sourceMaximumHz, frequency);
        const auto sourceProportion = std::log10(clampedFrequency / 20.0f)
                                    / std::log10(sourceMaximumHz / 20.0f);
        const auto scopePosition = juce::jlimit(0.0f,
                                                static_cast<float>(SpeModuleProcessor::analyserScopeSize - 1),
                                                sourceProportion * static_cast<float>(SpeModuleProcessor::analyserScopeSize - 1));
        const auto lowerIndex = juce::jlimit(0,
                                             static_cast<int>(SpeModuleProcessor::analyserScopeSize) - 1,
                                             static_cast<int>(std::floor(scopePosition)));
        const auto upperIndex = juce::jlimit(0,
                                             static_cast<int>(SpeModuleProcessor::analyserScopeSize) - 1,
                                             lowerIndex + 1);
        const auto interpolation = scopePosition - static_cast<float>(lowerIndex);
        return juce::jmap(interpolation,
                          data[static_cast<size_t>(lowerIndex)],
                          data[static_cast<size_t>(upperIndex)]);
    }

    float decibelsToY(const float decibels, const juce::Rectangle<float> bounds) const
    {
        const auto low = juce::jmin(displaySettings.rangeLowDb, displaySettings.rangeHighDb - 6.0f);
        const auto high = juce::jmax(displaySettings.rangeHighDb, low + 6.0f);
        return juce::jmap(juce::jlimit(low, high, decibels),
                          low,
                          high,
                          bounds.getBottom(),
                          bounds.getY());
    }

    SpeModuleProcessor& processor;
    std::array<float, SpeModuleProcessor::analyserScopeSize> scopeData {};
    std::array<float, SpeModuleProcessor::analyserScopeSize> leftGainReductionData {};
    std::array<float, SpeModuleProcessor::analyserScopeSize> rightGainReductionData {};
    SpeModuleProcessor::DisplaySettings displaySettings;
    double sampleRate = 44100.0;
};

SpeSpectrumAnalyserComponent* getSpeAnalyserComponent(juce::Component* component) noexcept
{
    return dynamic_cast<SpeSpectrumAnalyserComponent*>(component);
}
}

namespace shell_setup_support
{
juce::String getMixolveInfoMarkdown()
{
    return juce::String::fromUTF8(BinaryData::about_md, BinaryData::about_mdSize);
}

std::unique_ptr<juce::Component> createSpeAnalyserComponent(SpeModuleProcessor& processor)
{
    return std::make_unique<SpeSpectrumAnalyserComponent>(processor);
}

void refreshSpeAnalyserComponent(juce::Component* component)
{
    if (auto* analyser = getSpeAnalyserComponent(component))
        analyser->refreshResponse();
}

void removeOwnedChild(juce::Component& owner, std::unique_ptr<juce::Component>& child)
{
    if (child == nullptr)
        return;

    owner.removeChildComponent(child.get());
    child.reset();
}
}

void VxAudioProcessorEditor::refreshSpeAnalyserResponse()
{
    if (speModuleLoaded)
        shell_setup_support::refreshSpeAnalyserComponent(speAnalyserComponent.get());
}
