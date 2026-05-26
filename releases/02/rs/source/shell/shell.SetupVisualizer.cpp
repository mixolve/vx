#include "shell.EditorBellSection.h"
#include "shell.SetupSupport.h"
#include "../modules/eqe/module.eqe.ProcessorSupport.h"
#include "../modules/spe/module.spe.SpeProcessor.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace
{
class SectionFrameComponent final : public juce::Component
{
public:
    explicit SectionFrameComponent(const juce::Colour outline)
        : outlineColour(outline)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(outlineColour);
        g.drawRect(getLocalBounds(), frameLineThickness);
    }

private:
    juce::Colour outlineColour;
};

constexpr auto visualizerMinFrequency = 20.0f;
constexpr auto visualizerMarkerDiameter = 20.0f;
constexpr auto visualizerMarkerGap = uiGapFloat;
constexpr auto visualizerMarkerMargin = visualizerMarkerDiameter;
constexpr auto visualizerMarkerAnchorDiameter = 6.0f;

juce::String formatVisualizerCursorFrequency(const float frequency)
{
    return formatFixedDecimalValue(frequency, 2);
}

struct VisualizerDisplaySettings
{
    float rangeLowDb = -24.0f;
    float rangeHighDb = 24.0f;
    bool cursorEnabled = true;
    bool showStereo = true;
    bool showLeft = true;
    bool showRight = true;
    bool showMid = true;
    bool showSide = true;
};

enum class VisualizerCurveId
{
    stereo,
    left,
    right,
    mid,
    side
};

VisualizerCurveId mapPlaceToVisualizerCurve(const int place) noexcept
{
    switch (place)
    {
        case 1: return VisualizerCurveId::left;
        case 2: return VisualizerCurveId::right;
        case 3: return VisualizerCurveId::mid;
        case 4: return VisualizerCurveId::side;
        case 0:
        default: return VisualizerCurveId::stereo;
    }
}

struct VisualizerMarker
{
    int bellIndex = -1;
    int displayNumber = 0;
    float frequency = visualizerMinFrequency;
    VisualizerCurveId curve = VisualizerCurveId::stereo;
    bool selected = false;
};

struct VisualizerMarkerLayout
{
    const VisualizerMarker* marker = nullptr;
    juce::Rectangle<float> markerBounds;
    juce::Point<float> graphPoint;
    bool markerAboveGraph = true;
};

class EqResponseVisualizerComponent final : public juce::Component
{
public:
    explicit EqResponseVisualizerComponent(VxAudioProcessor& processorIn,
                                           std::function<void(int)> markerSelectCallbackIn)
        : processor(processorIn),
          markerSelectCallback(std::move(markerSelectCallbackIn))
    {
        setInterceptsMouseClicks(true, false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void setDisplaySettings(const VisualizerDisplaySettings& newSettings)
    {
        displaySettings = newSettings;
        if (! displaySettings.cursorEnabled)
        {
            mouseFrequencyHz.reset();
            mouseCursorY.reset();
        }
        repaint();
    }

    void refreshResponse()
    {
        processor.copyVisualiserResponse(stereoCurveDb, leftCurveDb, rightCurveDb, midCurveDb, sideCurveDb, sampleRate);
        repaint();
    }

    void setMarkers(std::vector<VisualizerMarker> newMarkers)
    {
        markers = std::move(newMarkers);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto plotBounds = bounds.reduced(10.0f);
        g.setColour(juce::Colours::black);
        g.fillRect(plotBounds);

        if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
            return;

        paintFrame(g, plotBounds);
        paintZeroDbLine(g, plotBounds);

        if (displaySettings.showStereo)
            paintCurve(g, plotBounds, stereoCurveDb, visualizerStereoColour, 1.5f);

        if (displaySettings.showLeft)
            paintCurve(g, plotBounds, leftCurveDb, visualizerLeftColour, 1.0f);

        if (displaySettings.showRight)
            paintCurve(g, plotBounds, rightCurveDb, visualizerRightColour, 1.0f);

        if (displaySettings.showMid)
            paintCurve(g, plotBounds, midCurveDb, visualizerMidColour, 1.0f);

        if (displaySettings.showSide)
            paintCurve(g, plotBounds, sideCurveDb, visualizerSideColour, 1.0f);

        paintMarkers(g, plotBounds);

        if (displaySettings.cursorEnabled)
            paintCursor(g, plotBounds);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (const auto bellIndex = findMarkerAtPosition(event.position, getLocalBounds().toFloat().reduced(10.0f));
            bellIndex >= 0 && markerSelectCallback != nullptr)
        {
            markerSelectCallback(bellIndex);
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        updateMouseFrequency(event.position);
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        updateMouseFrequency(event.position);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        mouseFrequencyHz.reset();
        mouseCursorY.reset();
        repaint();
    }

private:
    void paintFrame(juce::Graphics& g, const juce::Rectangle<float>& plotBounds)
    {
        g.setColour(uiGrey500);
        g.drawRect(plotBounds, 1.0f);
    }

    void paintZeroDbLine(juce::Graphics& g, const juce::Rectangle<float>& plotBounds)
    {
        g.setColour(uiGrey500);
        const auto zeroY = decibelsToY(0.0f, plotBounds);
        g.drawHorizontalLine(juce::roundToInt(zeroY), plotBounds.getX(), plotBounds.getRight());
    }

    void paintCurve(juce::Graphics& g,
                    const juce::Rectangle<float>& plotBounds,
                    const std::array<float, VxAudioProcessor::visualizerScopeSize>& valuesDb,
                    const juce::Colour colour,
                    const float thickness)
    {
        juce::Path path;

        for (size_t index = 0; index < valuesDb.size(); ++index)
        {
            const auto denominator = valuesDb.size() > 1 ? static_cast<float>(valuesDb.size() - 1) : 1.0f;
            const auto proportion = static_cast<float>(index) / denominator;
            const auto frequency = juce::mapToLog10(proportion,
                                                    visualizerMinFrequency,
                                                    getVisibleMaximumFrequency());
            const auto x = frequencyToX(frequency, plotBounds);
            const auto y = decibelsToY(valuesDb[index], plotBounds);

            if (index == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(thickness));
    }

    void paintCursor(juce::Graphics& g, const juce::Rectangle<float>& plotBounds)
    {
        if (! mouseFrequencyHz.has_value() || ! mouseCursorY.has_value())
            return;

        const auto x = frequencyToX(static_cast<float>(*mouseFrequencyHz), plotBounds);
        const auto y = juce::jlimit(plotBounds.getY(), plotBounds.getBottom(), *mouseCursorY);
        g.setColour(uiGrey500);
        g.drawVerticalLine(juce::roundToInt(x), plotBounds.getY(), plotBounds.getBottom());
        g.drawHorizontalLine(juce::roundToInt(y), plotBounds.getX(), plotBounds.getRight());

        const auto labelText = formatVisualizerCursorFrequency(static_cast<float>(*mouseFrequencyHz));
        const auto labelFont = makeUiFont();
        g.setFont(labelFont);
        const auto labelWidth = juce::jmax(1, getTextPixelWidth(labelFont, labelText) + 12);
        const auto labelHeight = juce::jmax(1, juce::roundToInt(labelFont.getHeight() + 6.0f));
        auto labelBounds = juce::Rectangle<int>(labelWidth, labelHeight);
        labelBounds.setCentre(juce::roundToInt(plotBounds.getCentreX()), juce::roundToInt(plotBounds.getBottom()) - (labelHeight / 2));
        labelBounds = labelBounds.constrainedWithin(plotBounds.toNearestInt());

        g.setColour(uiWhite);
        g.drawFittedText(labelText, labelBounds.reduced(4, 0), juce::Justification::centred, 1);
    }

    void paintMarkers(juce::Graphics& g, const juce::Rectangle<float>& plotBounds)
    {
        const auto markerFont = makeUiFont(juce::Font::bold, 14.0f);
        g.setFont(markerFont);

        const auto markerLayouts = buildMarkerLayouts(plotBounds);

        for (const auto& layout : markerLayouts)
            paintMarkerConnector(g, layout);

        for (const auto& layout : markerLayouts)
            if (layout.marker != nullptr && ! layout.marker->selected)
                paintMarkerBubble(g, layout);

        for (const auto& layout : markerLayouts)
            if (layout.marker != nullptr && layout.marker->selected)
                paintMarkerBubble(g, layout);
    }

    void paintMarkerConnector(juce::Graphics& g, const VisualizerMarkerLayout& layout) const
    {
        if (layout.marker == nullptr)
            return;

        const auto markerColour = getMarkerColour(layout.marker->curve);
        const auto connectorTop = juce::jmin(layout.graphPoint.y,
                                             layout.markerAboveGraph ? layout.markerBounds.getBottom()
                                                                     : layout.markerBounds.getY());
        const auto connectorBottom = juce::jmax(layout.graphPoint.y,
                                                layout.markerAboveGraph ? layout.markerBounds.getBottom()
                                                                        : layout.markerBounds.getY());

        g.setColour(markerColour.withAlpha(0.75f));
        g.drawVerticalLine(juce::roundToInt(layout.graphPoint.x), connectorTop, connectorBottom);

        auto graphPointBounds = juce::Rectangle<float>(visualizerMarkerAnchorDiameter,
                                                       visualizerMarkerAnchorDiameter)
                                    .withCentre(layout.graphPoint);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillEllipse(graphPointBounds);
        g.setColour(markerColour);
        g.drawEllipse(graphPointBounds, 1.0f);
    }

    void paintMarkerBubble(juce::Graphics& g, const VisualizerMarkerLayout& layout) const
    {
        if (layout.marker == nullptr)
            return;

        const auto markerColour = getMarkerColour(layout.marker->curve);

        g.setColour(juce::Colours::black.withAlpha(0.85f));
        if (layout.marker->selected)
            g.fillRect(layout.markerBounds);
        else
            g.fillEllipse(layout.markerBounds);

        g.setColour(markerColour);
        if (layout.marker->selected)
            g.drawRect(layout.markerBounds, 1.5f);
        else
            g.drawEllipse(layout.markerBounds, 1.5f);

        g.setColour(uiWhite);
        g.drawFittedText(juce::String(layout.marker->displayNumber),
                         layout.markerBounds.toNearestInt(),
                         juce::Justification::centred,
                         1);
    }

    std::vector<VisualizerMarkerLayout> buildMarkerLayouts(const juce::Rectangle<float>& plotBounds) const
    {
        struct PendingMarker
        {
            const VisualizerMarker* marker = nullptr;
            float x = 0.0f;
            float y = 0.0f;
            bool preferTopLane = true;
        };

        std::vector<PendingMarker> pendingMarkers;
        pendingMarkers.reserve(markers.size());

        for (const auto& marker : markers)
        {
            const auto x = frequencyToX(marker.frequency, plotBounds);
            const auto y = decibelsToY(sampleCurveAtFrequency(marker.curve, marker.frequency), plotBounds);
            pendingMarkers.push_back({ &marker, x, y, y >= plotBounds.getCentreY() });
        }

        std::stable_sort(pendingMarkers.begin(),
                         pendingMarkers.end(),
                         [] (const PendingMarker& left, const PendingMarker& right)
                         {
                             if (std::abs(left.x - right.x) > 0.5f)
                                 return left.x < right.x;

                             return left.marker != nullptr && right.marker != nullptr
                                 ? left.marker->displayNumber < right.marker->displayNumber
                                 : false;
                         });

        std::vector<float> topLaneLastCenters;
        std::vector<float> bottomLaneLastCenters;
        std::vector<VisualizerMarkerLayout> layouts;
        layouts.reserve(pendingMarkers.size());

        const auto chooseLaneIndex = [] (std::vector<float>& laneLastCenters, const float x)
        {
            const auto minimumCenterDistance = visualizerMarkerDiameter + visualizerMarkerGap;

            for (size_t laneIndex = 0; laneIndex < laneLastCenters.size(); ++laneIndex)
            {
                if (x - laneLastCenters[laneIndex] >= minimumCenterDistance)
                {
                    laneLastCenters[laneIndex] = x;
                    return static_cast<int>(laneIndex);
                }
            }

            laneLastCenters.push_back(x);
            return static_cast<int>(laneLastCenters.size() - 1);
        };

        const auto getLaneCentreY = [&plotBounds] (const bool topLane, const int laneIndex)
        {
            const auto radius = visualizerMarkerDiameter * 0.5f;
            const auto laneOffset = visualizerMarkerMargin + radius + (static_cast<float>(laneIndex)
                                                                        * (visualizerMarkerDiameter + visualizerMarkerGap));
            return topLane ? plotBounds.getY() + laneOffset
                           : plotBounds.getBottom() - laneOffset;
        };

        for (const auto& pendingMarker : pendingMarkers)
        {
            auto& laneLastCenters = pendingMarker.preferTopLane ? topLaneLastCenters
                                                                : bottomLaneLastCenters;
            const auto laneIndex = chooseLaneIndex(laneLastCenters, pendingMarker.x);
            const auto markerCentre = juce::Point<float>(pendingMarker.x,
                                                         getLaneCentreY(pendingMarker.preferTopLane, laneIndex));

            layouts.push_back({ pendingMarker.marker,
                                juce::Rectangle<float>(visualizerMarkerDiameter, visualizerMarkerDiameter).withCentre(markerCentre),
                                { pendingMarker.x, pendingMarker.y },
                                pendingMarker.preferTopLane });
        }

        return layouts;
    }

    int findMarkerAtPosition(const juce::Point<float>& position,
                             const juce::Rectangle<float>& plotBounds) const
    {
        const auto markerLayouts = buildMarkerLayouts(plotBounds);

        for (const auto& layout : markerLayouts)
        {
            if (layout.marker != nullptr && layout.markerBounds.expanded(4.0f).contains(position))
                return layout.marker->bellIndex;
        }

        return -1;
    }

    juce::Colour getMarkerColour(const VisualizerCurveId curve) const noexcept
    {
        switch (curve)
        {
            case VisualizerCurveId::left: return visualizerLeftColour;
            case VisualizerCurveId::right: return visualizerRightColour;
            case VisualizerCurveId::mid: return visualizerMidColour;
            case VisualizerCurveId::side: return visualizerSideColour;
            case VisualizerCurveId::stereo:
            default: return visualizerStereoColour;
        }
    }

    const std::array<float, VxAudioProcessor::visualizerScopeSize>& getCurveValues(const VisualizerCurveId curve) const noexcept
    {
        switch (curve)
        {
            case VisualizerCurveId::left: return leftCurveDb;
            case VisualizerCurveId::right: return rightCurveDb;
            case VisualizerCurveId::mid: return midCurveDb;
            case VisualizerCurveId::side: return sideCurveDb;
            case VisualizerCurveId::stereo:
            default: return stereoCurveDb;
        }
    }

    float sampleCurveAtFrequency(const VisualizerCurveId curve, const float frequency) const noexcept
    {
        const auto& valuesDb = getCurveValues(curve);
        const auto maxFrequency = getVisibleMaximumFrequency();
        const auto clampedFrequency = juce::jlimit(visualizerMinFrequency, maxFrequency, frequency);
        const auto proportion = std::log10(clampedFrequency / visualizerMinFrequency)
            / std::log10(maxFrequency / visualizerMinFrequency);
        const auto scaledIndex = juce::jlimit(0.0f,
                                              static_cast<float>(valuesDb.size() - 1),
                                              proportion * static_cast<float>(valuesDb.size() - 1));
        const auto lowerIndex = static_cast<size_t>(std::floor(scaledIndex));
        const auto upperIndex = static_cast<size_t>(juce::jmin(static_cast<int>(valuesDb.size() - 1),
                                                               static_cast<int>(lowerIndex + 1)));
        const auto blend = scaledIndex - static_cast<float>(lowerIndex);
        return valuesDb[lowerIndex] + ((valuesDb[upperIndex] - valuesDb[lowerIndex]) * blend);
    }

    void updateMouseFrequency(const juce::Point<float>& position)
    {
        if (! displaySettings.cursorEnabled)
            return;

        const auto plotBounds = getLocalBounds().toFloat().reduced(10.0f);

        if (! plotBounds.contains(position))
        {
            if (mouseFrequencyHz.has_value())
            {
                mouseFrequencyHz.reset();
                repaint();
            }

            return;
        }

        const auto frequency = xToFrequency(position.x, plotBounds);

        const auto nextY = juce::jlimit(plotBounds.getY(), plotBounds.getBottom(), position.y);
        const auto frequencyChanged = ! mouseFrequencyHz.has_value() || std::abs(*mouseFrequencyHz - static_cast<double>(frequency)) > 1.0e-3;
        const auto yChanged = ! mouseCursorY.has_value() || std::abs(*mouseCursorY - nextY) > 0.5f;

        if (frequencyChanged || yChanged)
        {
            mouseFrequencyHz = frequency;
            mouseCursorY = nextY;
            repaint();
        }
    }

    float getVisibleMaximumFrequency() const noexcept
    {
        return juce::jlimit(1000.0f,
                            maximumVisibleFilterFrequency,
                            static_cast<float>(sampleRate > 0.0 ? sampleRate * 0.5 : 20000.0));
    }

    float frequencyToX(const float frequency, const juce::Rectangle<float>& bounds) const noexcept
    {
        const auto maxFrequency = getVisibleMaximumFrequency();
        const auto clampedFrequency = juce::jlimit(visualizerMinFrequency, maxFrequency, frequency);
        const auto proportion = std::log10(clampedFrequency / visualizerMinFrequency)
            / std::log10(maxFrequency / visualizerMinFrequency);
        return bounds.getX() + (bounds.getWidth() * proportion);
    }

    float xToFrequency(const float x, const juce::Rectangle<float>& bounds) const noexcept
    {
        const auto maxFrequency = getVisibleMaximumFrequency();

        if (bounds.getWidth() <= 0.0f)
            return visualizerMinFrequency;

        const auto proportion = juce::jlimit(0.0f, 1.0f, (x - bounds.getX()) / bounds.getWidth());
        return visualizerMinFrequency * std::pow(maxFrequency / visualizerMinFrequency, proportion);
    }

    float decibelsToY(const float decibels, const juce::Rectangle<float>& bounds) const noexcept
    {
        const auto low = juce::jmin(displaySettings.rangeLowDb, displaySettings.rangeHighDb - 6.0f);
        const auto high = juce::jmax(displaySettings.rangeHighDb, low + 6.0f);
        return juce::jmap(juce::jlimit(low, high, decibels),
                          low,
                          high,
                          bounds.getBottom(),
                          bounds.getY());
    }

    VxAudioProcessor& processor;
    VisualizerDisplaySettings displaySettings;
    std::array<float, VxAudioProcessor::visualizerScopeSize> stereoCurveDb {};
    std::array<float, VxAudioProcessor::visualizerScopeSize> leftCurveDb {};
    std::array<float, VxAudioProcessor::visualizerScopeSize> rightCurveDb {};
    std::array<float, VxAudioProcessor::visualizerScopeSize> midCurveDb {};
    std::array<float, VxAudioProcessor::visualizerScopeSize> sideCurveDb {};
    double sampleRate = 44100.0;
    std::optional<double> mouseFrequencyHz;
    std::optional<float> mouseCursorY;
    std::vector<VisualizerMarker> markers;
    std::function<void(int)> markerSelectCallback;
};

EqResponseVisualizerComponent* getVisualizerComponent(juce::Component* component) noexcept
{
    return dynamic_cast<EqResponseVisualizerComponent*>(component);
}

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
        processor.copyGainReductionData(gainReductionData);
        displaySettings = processor.getDisplaySettings();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto plotBounds = bounds.reduced(10.0f);

        g.setColour(uiGrey800);
        g.fillRect(bounds);
        g.fillRect(plotBounds);

        if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
            return;

        juce::Path postSpectrumPath;
        juce::Path reductionPath;
        juce::Path thresholdPath;

        const auto sourceMaximumHz = juce::jlimit(21.0f,
                                                  22000.0f,
                                                  static_cast<float>(sampleRate * 0.5));
        const auto minimumHz = displaySettings.leftFrequencyHz <= 0.0f
            ? 20.0f
            : juce::jlimit(20.0f, sourceMaximumHz - 1.0f, displaySettings.leftFrequencyHz);
        const auto maximumHz = juce::jlimit(minimumHz + 1.0f,
                                            sourceMaximumHz,
                                            displaySettings.rightFrequencyHz);

        for (auto index = 0; index < static_cast<int>(spe::analyserScopeSize); ++index)
        {
            const auto proportion = static_cast<float>(index) / static_cast<float>(spe::analyserScopeSize - 1);
            const auto x = plotBounds.getX() + proportion * plotBounds.getWidth();
            const auto frequency = juce::mapToLog10(proportion, minimumHz, maximumHz);
            const auto sampledDecibels = sampleScopeAtFrequency(scopeData, frequency, sourceMaximumHz);
            const auto sampledReductionDb = juce::jmax(0.0f,
                                                       sampleScopeAtFrequency(gainReductionData,
                                                                              frequency,
                                                                              sourceMaximumHz));
            const auto octavesFromSlopeReference = std::log2(frequency / 632.455532f);
            const auto displaySlopeOffset = displaySettings.slopeDbPerOct * octavesFromSlopeReference;
            const auto thresholdDbAtFrequency = displaySettings.thresholdDb;
            const auto postDecibels = sampledDecibels + displaySlopeOffset;
            const auto postY = decibelsToY(postDecibels, plotBounds);
            const auto thresholdY = decibelsToY(thresholdDbAtFrequency, plotBounds);
            const auto reductionY = decibelsToY(thresholdDbAtFrequency - sampledReductionDb, plotBounds);

            if (index == 0)
            {
                postSpectrumPath.startNewSubPath(x, postY);
                thresholdPath.startNewSubPath(x, thresholdY);
                reductionPath.startNewSubPath(x, thresholdY);
                reductionPath.lineTo(x, reductionY);
            }
            else
            {
                postSpectrumPath.lineTo(x, postY);
                thresholdPath.lineTo(x, thresholdY);
                reductionPath.lineTo(x, reductionY);
            }
        }

        juce::Path spectrumFillPath(postSpectrumPath);
        spectrumFillPath.lineTo(plotBounds.getRight(), plotBounds.getBottom());
        spectrumFillPath.lineTo(plotBounds.getX(), plotBounds.getBottom());
        spectrumFillPath.closeSubPath();

        for (auto index = static_cast<int>(spe::analyserScopeSize) - 1; index >= 0; --index)
        {
            const auto proportion = static_cast<float>(index) / static_cast<float>(spe::analyserScopeSize - 1);
            const auto x = plotBounds.getX() + proportion * plotBounds.getWidth();
            reductionPath.lineTo(x, decibelsToY(displaySettings.thresholdDb, plotBounds));
        }

        reductionPath.closeSubPath();

        g.setColour(uiGrey500);
        g.drawRect(plotBounds, 1.0f);
        g.setColour(uiAccent);
        g.fillPath(spectrumFillPath);
        g.setColour(uiClip);
        g.fillPath(reductionPath);
        g.strokePath(thresholdPath, juce::PathStrokeType(1.0f));
    }

private:
    static float sampleScopeAtFrequency(const std::array<float, spe::analyserScopeSize>& data,
                                        const float frequency,
                                        const float sourceMaximumHz)
    {
        const auto clampedFrequency = juce::jlimit(20.0f, sourceMaximumHz, frequency);
        const auto sourceProportion = std::log10(clampedFrequency / 20.0f)
                                    / std::log10(sourceMaximumHz / 20.0f);
        const auto scopePosition = juce::jlimit(0.0f,
                                                static_cast<float>(spe::analyserScopeSize - 1),
                                                sourceProportion * static_cast<float>(spe::analyserScopeSize - 1));
        const auto lowerIndex = juce::jlimit(0,
                                             static_cast<int>(spe::analyserScopeSize) - 1,
                                             static_cast<int>(std::floor(scopePosition)));
        const auto upperIndex = juce::jlimit(0,
                                             static_cast<int>(spe::analyserScopeSize) - 1,
                                             lowerIndex + 1);
        const auto interpolation = scopePosition - static_cast<float>(lowerIndex);
        return juce::jmap(interpolation,
                          data[static_cast<size_t>(lowerIndex)],
                          data[static_cast<size_t>(upperIndex)]);
    }

    float decibelsToY(const float decibels, const juce::Rectangle<float> bounds) const
    {
        return juce::jmap(juce::jlimit(displaySettings.rangeLowDb, displaySettings.rangeHighDb, decibels),
                          displaySettings.rangeLowDb,
                          displaySettings.rangeHighDb,
                          bounds.getBottom(),
                          bounds.getY());
    }

    SpeModuleProcessor& processor;
    std::array<float, spe::analyserScopeSize> scopeData {};
    std::array<float, spe::analyserScopeSize> gainReductionData {};
    spe::DisplaySettings displaySettings;
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

std::unique_ptr<juce::Component> createSectionFrameComponent(const juce::Colour outline)
{
    return std::make_unique<SectionFrameComponent>(outline);
}

std::unique_ptr<juce::Component> createEqResponseVisualizerComponent(VxAudioProcessor& processor,
                                                                     std::function<void(int)> markerSelectCallback)
{
    return std::make_unique<EqResponseVisualizerComponent>(processor, std::move(markerSelectCallback));
}

void refreshEqResponseVisualizerComponent(juce::Component* component,
                                          const float rangeLowDb,
                                          const float rangeHighDb,
                                          const bool cursorEnabled,
                                          const bool showStereo,
                                          const bool showLeft,
                                          const bool showRight,
                                          const bool showMid,
                                          const bool showSide,
                                          std::vector<VisualizerMarkerData> markers)
{
    auto* visualizer = getVisualizerComponent(component);

    if (visualizer == nullptr)
        return;

    visualizer->setDisplaySettings({ rangeLowDb,
                                     rangeHighDb,
                                     cursorEnabled,
                                     showStereo,
                                     showLeft,
                                     showRight,
                                     showMid,
                                     showSide });

    std::vector<VisualizerMarker> componentMarkers;
    componentMarkers.reserve(markers.size());

    for (const auto& marker : markers)
    {
        componentMarkers.push_back({ marker.bellIndex,
                                     marker.displayNumber,
                                     marker.frequency,
                                     mapPlaceToVisualizerCurve(marker.place),
                                     marker.selected });
    }

    visualizer->setMarkers(std::move(componentMarkers));
    visualizer->refreshResponse();
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

void VxAudioProcessorEditor::refreshVisualizerResponse()
{
    if (speModuleLoaded && eqeModuleExpanded && visualizerVisible)
        shell_setup_support::refreshSpeAnalyserComponent(speAnalyserComponent.get());

    if (! eqeModuleLoaded || ! eqeModuleExpanded || ! visualizerVisible)
        return;

    std::vector<shell_setup_support::VisualizerMarkerData> markers;
    const auto activeCount = getActiveBellCount();
    markers.reserve(static_cast<size_t>(activeCount));

    for (int displayIndex = 0; displayIndex < activeCount; ++displayIndex)
    {
        const auto bellIndex = getBellIndexForOrderPosition(displayIndex);

        if (bellIndex < 0)
            continue;

        auto* section = bellSections[static_cast<size_t>(bellIndex)].get();

        if (section == nullptr)
            continue;

        markers.push_back({ bellIndex,
                            displayIndex + 1,
                            juce::jlimit(minimumVisibleFilterFrequency,
                                         maximumVisibleFilterFrequency,
                                         static_cast<float>(section->getFrequency())),
                            section->getPlace(),
                            filtersExpanded && expandedBellIndex == bellIndex });
    }

    shell_setup_support::refreshEqResponseVisualizerComponent(visualizerComponent.get(),
                                                              visualizerRangeLowDb,
                                                              visualizerRangeHighDb,
                                                              visualizerCursorEnabled,
                                                              visualizerShowStereo,
                                                              visualizerShowLeft,
                                                              visualizerShowRight,
                                                              visualizerShowMid,
                                                              visualizerShowSide,
                                                              std::move(markers));
}
