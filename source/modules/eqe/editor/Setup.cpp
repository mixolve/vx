#include "EditorBellSection.h"
#include "EditorPresetSections.h"
#include "ProcessorSupport.h"

#include <optional>

namespace
{
class SectionFrameComponent final : public juce::Component
{
public:
    SectionFrameComponent()
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(uiAccent);
        g.drawRect(getLocalBounds(), 1);
    }
};

juce::String getMixolveInfoMarkdown()
{
    return juce::String::fromUTF8(BinaryData::about_md, BinaryData::about_mdSize);
}

#if ! JUCE_IOS

constexpr auto visualizerMinFrequency = 20.0f;
constexpr auto visualizerMarkerDiameter = 20.0f;
constexpr auto visualizerMarkerGap = 8.0f;
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
    explicit EqResponseVisualizerComponent(EqeAudioProcessor& processorIn,
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
                    const std::array<float, EqeAudioProcessor::visualizerScopeSize>& valuesDb,
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
        const auto labelWidth = juce::jmax(1, labelFont.getStringWidth(labelText) + 12);
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

    const std::array<float, EqeAudioProcessor::visualizerScopeSize>& getCurveValues(const VisualizerCurveId curve) const noexcept
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

    EqeAudioProcessor& processor;
    VisualizerDisplaySettings displaySettings;
    std::array<float, EqeAudioProcessor::visualizerScopeSize> stereoCurveDb {};
    std::array<float, EqeAudioProcessor::visualizerScopeSize> leftCurveDb {};
    std::array<float, EqeAudioProcessor::visualizerScopeSize> rightCurveDb {};
    std::array<float, EqeAudioProcessor::visualizerScopeSize> midCurveDb {};
    std::array<float, EqeAudioProcessor::visualizerScopeSize> sideCurveDb {};
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
#endif
}

#if ! JUCE_IOS
void EqeAudioProcessorEditor::refreshVisualizerResponse()
{
    auto* visualizer = getVisualizerComponent(visualizerComponent.get());

    if (visualizer == nullptr)
        return;

    visualizer->setDisplaySettings({ visualizerRangeLowDb,
                                     visualizerRangeHighDb,
                                     visualizerCursorEnabled,
                                     visualizerShowStereo,
                                     visualizerShowLeft,
                                     visualizerShowRight,
                                     visualizerShowMid,
                                     visualizerShowSide });

    std::vector<VisualizerMarker> markers;
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

        auto curve = VisualizerCurveId::stereo;

        switch (section->getPlace())
        {
            case 1: curve = VisualizerCurveId::left; break;
            case 2: curve = VisualizerCurveId::right; break;
            case 3: curve = VisualizerCurveId::mid; break;
            case 4: curve = VisualizerCurveId::side; break;
            case 0:
            default: curve = VisualizerCurveId::stereo; break;
        }

        markers.push_back({ bellIndex,
                            displayIndex + 1,
                            juce::jlimit(minimumVisibleFilterFrequency,
                                         maximumVisibleFilterFrequency,
                                         static_cast<float>(section->getFrequency())),
                            curve,
                            filtersExpanded && expandedBellIndex == bellIndex });
    }

    visualizer->setMarkers(std::move(markers));
    visualizer->refreshResponse();
}
#endif

EqeAudioProcessorEditor::EqeAudioProcessorEditor(EqeAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      audioProcessor(processorToEdit),
      valueTreeState(processorToEdit.getValueTreeState()),
      lookAndFeel(std::make_unique<EqeLookAndFeel>())
{
#if JUCE_IOS
    juce::Desktop::getInstance().setOrientationsEnabled(juce::Desktop::upright);
#endif

    setLookAndFeel(lookAndFeel.get());
    setOpaque(true);
    setResizable(true, true);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(false);
    globalViewport.setViewedComponent(&globalContent, false);
    globalViewport.setScrollBarsShown(false, false);
    globalViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    globalViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(globalViewport);
    filterViewport.setViewedComponent(&filterContent, false);
    filterViewport.setScrollBarsShown(false, false);
    filterViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    filterViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(filterViewport);

    moduleAddButton = std::make_unique<BoxTextButton>(uiAccent);
    moduleAddButton->setButtonText("+");
    moduleAddButton->setTextJustification(juce::Justification::centred);
    moduleAddButton->onClick = [this]
    {
        showModulePicker();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*moduleAddButton);

    globalHeader = std::make_unique<BoxTextButton>(uiAccent);
    globalHeader->setButtonText("GLOBAL");
    globalHeader->setLeadingDot(uiClip, audioProcessor.getGlobalClipIndicator());
    globalHeader->setClickingTogglesState(true);
    globalHeader->onClick = [this]
    {
        openGlobalSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*globalHeader);

    clipControl = std::make_unique<LocalParameterControl>("CLIP",
                                                          0,
                                                          0.0,
                                                          1.0,
                                                          1.0,
                                                          0.0);
    clipControl->setInteractionEnabled(false);
    clipControl->setValue(audioProcessor.getGlobalClipIndicator(), false);
    clipControl->setTitleLongPressAction([this]
    {
        audioProcessor.resetGlobalClipIndicator();

        if (clipControl != nullptr)
            clipControl->setValue(0.0, false);

        if (globalHeader != nullptr)
            globalHeader->setLeadingDotLevel(0.0f);

        clearKeyboardFocus(*this);
    });
    globalContent.addAndMakeVisible(*clipControl);

    outGainControl = std::make_unique<ParameterControl>(valueTreeState,
                                                        EqeAudioProcessor::paramOutGainId,
                                                        "GAIN.OUT",
                                                        2);
    outGainControl->setTitleLongPressAction([this]
    {
        if (outGainControl != nullptr)
            outGainControl->setValue(0.0, true);

        clearKeyboardFocus(*this);
    });
    globalContent.addAndMakeVisible(*outGainControl);

    gainLControl = std::make_unique<ParameterControl>(valueTreeState,
                                                      EqeAudioProcessor::paramGlobalGainLId,
                                                      "GAIN.L",
                                                      2);
    gainLControl->setTitleLongPressAction([this]
    {
        if (gainLControl != nullptr)
            gainLControl->setValue(0.0, true);

        clearKeyboardFocus(*this);
    });
    globalContent.addAndMakeVisible(*gainLControl);

    gainRControl = std::make_unique<ParameterControl>(valueTreeState,
                                                      EqeAudioProcessor::paramGlobalGainRId,
                                                      "GAIN.R",
                                                      2);
    gainRControl->setTitleLongPressAction([this]
    {
        if (gainRControl != nullptr)
            gainRControl->setValue(0.0, true);

        clearKeyboardFocus(*this);
    });
    globalContent.addAndMakeVisible(*gainRControl);

    wideControl = std::make_unique<ParameterControl>(valueTreeState,
                                                     EqeAudioProcessor::paramGlobalWideId,
                                                     "WIDE",
                                                     0);
    wideControl->setTitleLongPressAction([this]
    {
        if (wideControl != nullptr)
            wideControl->setValue(100.0, true);

        clearKeyboardFocus(*this);
    });
    globalContent.addAndMakeVisible(*wideControl);

    globalBypassButton = std::make_unique<BoxTextButton>(uiAccent);
    globalBypassButton->setButtonText("BYPASS");
    globalBypassButton->setTextJustification(juce::Justification::centred);
    globalBypassButton->setClickingTogglesState(true);
    globalBypassAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                                 EqeAudioProcessor::paramGlobalBypassId,
                                                                 *globalBypassButton);
    globalBypassButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*globalBypassButton);

    globalBypassOutGainOnlyButton = std::make_unique<BoxTextButton>(uiAccent);
    globalBypassOutGainOnlyButton->setButtonText("BYPASS.WT-GN");
    globalBypassOutGainOnlyButton->setTextJustification(juce::Justification::centred);
    globalBypassOutGainOnlyButton->setClickingTogglesState(true);
    globalBypassOutGainOnlyAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                                            EqeAudioProcessor::paramGlobalBypassOutGainOnlyId,
                                                                            *globalBypassOutGainOnlyButton);
    globalBypassOutGainOnlyButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*globalBypassOutGainOnlyButton);

    clearFiltersButton = std::make_unique<BoxTextButton>(uiAccent);
    clearFiltersButton->setButtonText("DELETE-ALL");
    clearFiltersButton->setTextJustification(juce::Justification::centred);
    clearFiltersButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    clearFiltersButton->setLongPressAction([this]
    {
        clearAllFilters();
        clearKeyboardFocus(*this);
    }, 500, "SURE?");
    globalContent.addAndMakeVisible(*clearFiltersButton);

    undoButton = std::make_unique<BoxTextButton>(uiGrey500);
    undoButton->setButtonText("UNDO");
    undoButton->setTextJustification(juce::Justification::centred);
    undoButton->onClick = [this]
    {
        performUndo();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*undoButton);

    redoButton = std::make_unique<BoxTextButton>(uiGrey500);
    redoButton->setButtonText("REDO");
    redoButton->setTextJustification(juce::Justification::centred);
    redoButton->onClick = [this]
    {
        performRedo();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*redoButton);

    sortPlaceButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortPlaceButton->setButtonText("SORT.PLACE");
    sortPlaceButton->setTextJustification(juce::Justification::centred);
    sortPlaceButton->onClick = [this]
    {
        sortBellSectionsByPlace();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*sortPlaceButton);

    sortFreqButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortFreqButton->setButtonText("SORT.FREQ");
    sortFreqButton->setTextJustification(juce::Justification::centred);
    sortFreqButton->onClick = [this]
    {
        sortBellSectionsByFrequency();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*sortFreqButton);

    sortDuoButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortDuoButton->setButtonText("SORT.DUO");
    sortDuoButton->setTextJustification(juce::Justification::centred);
    sortDuoButton->onClick = [this]
    {
        sortBellSectionsByDuo();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*sortDuoButton);

    filtersHeader = std::make_unique<BoxTextButton>(uiAccent);
    filtersHeader->setButtonText("FILTERS");
    filtersHeader->setClickingTogglesState(true);
    filtersHeader->onClick = [this]
    {
        openFiltersSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*filtersHeader);

#if ! JUCE_IOS
    visualizerHeader = std::make_unique<BoxTextButton>(uiAccent);
    visualizerHeader->setButtonText("VISUALIZER");
    visualizerHeader->setClickingTogglesState(true);
    visualizerHeader->onClick = [this]
    {
        openVisualizerSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*visualizerHeader);

    visualizerRangeLowControl = std::make_unique<LocalParameterControl>("LOW",
                                                                        1,
                                                                        -120.0,
                                                                        60.0,
                                                                        0.1,
                                                                        -24.0);
    visualizerRangeLowControl->setValue(visualizerRangeLowDb, false);
    visualizerRangeLowControl->onValueChanged = [this]
    {
        if (suppressVisualizerControlChangeHandlers)
            return;

        visualizerRangeLowDb = static_cast<float>(visualizerRangeLowControl->getValue());

        if (visualizerRangeHighDb < visualizerRangeLowDb + 6.0f)
        {
            const juce::ScopedValueSetter<bool> scopedIgnore(suppressVisualizerControlChangeHandlers, true);
            visualizerRangeHighDb = visualizerRangeLowDb + 6.0f;
            visualizerRangeHighControl->setValue(visualizerRangeHighDb, false);
        }

        refreshVisualizerResponse();
        storeEditorStateToValueTree();
    };
    addAndMakeVisible(*visualizerRangeLowControl);

    visualizerRangeHighControl = std::make_unique<LocalParameterControl>("HIGH",
                                                                         1,
                                                                         -120.0,
                                                                         60.0,
                                                                         0.1,
                                                                         24.0);
    visualizerRangeHighControl->setValue(visualizerRangeHighDb, false);
    visualizerRangeHighControl->onValueChanged = [this]
    {
        if (suppressVisualizerControlChangeHandlers)
            return;

        visualizerRangeHighDb = static_cast<float>(visualizerRangeHighControl->getValue());

        if (visualizerRangeHighDb < visualizerRangeLowDb + 6.0f)
        {
            const juce::ScopedValueSetter<bool> scopedIgnore(suppressVisualizerControlChangeHandlers, true);
            visualizerRangeLowDb = visualizerRangeHighDb - 6.0f;
            visualizerRangeLowControl->setValue(visualizerRangeLowDb, false);
        }

        refreshVisualizerResponse();
        storeEditorStateToValueTree();
    };
    addAndMakeVisible(*visualizerRangeHighControl);

    visualizerCursorButton = std::make_unique<BoxTextButton>(uiGrey500);
    visualizerCursorButton->setButtonText(visualizerCursorEnabled ? "CURSOR-OFF" : "CURSOR-ON");
    visualizerCursorButton->setTextJustification(juce::Justification::centred);
    visualizerCursorButton->setClickingTogglesState(true);
    visualizerCursorButton->setToggleState(visualizerCursorEnabled, juce::dontSendNotification);
    visualizerCursorButton->onClick = [this]
    {
        visualizerCursorEnabled = visualizerCursorButton->getToggleState();
        visualizerCursorButton->setButtonText(visualizerCursorEnabled ? "CURSOR-OFF" : "CURSOR-ON");
        refreshVisualizerResponse();
        storeEditorStateToValueTree();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*visualizerCursorButton);

    auto makeVisualizerLegendButton = [this] (const juce::String& text, const juce::Colour colour, bool& stateValue)
    {
        auto button = std::make_unique<BoxTextButton>(colour);
        button->setButtonText(text);
        button->setTextJustification(juce::Justification::centred);
        button->setClickingTogglesState(true);
        button->setToggleState(stateValue, juce::dontSendNotification);
        button->onClick = [this, &stateValue, buttonPtr = button.get()]
        {
            stateValue = buttonPtr->getToggleState();
            refreshVisualizerResponse();
            storeEditorStateToValueTree();
            clearKeyboardFocus(*this);
        };
        return button;
    };

    visualizerShowStereoButton = makeVisualizerLegendButton("LR", visualizerStereoColour, visualizerShowStereo);
    addAndMakeVisible(*visualizerShowStereoButton);

    visualizerShowLeftButton = makeVisualizerLegendButton("L", visualizerLeftColour, visualizerShowLeft);
    addAndMakeVisible(*visualizerShowLeftButton);

    visualizerShowRightButton = makeVisualizerLegendButton("R", visualizerRightColour, visualizerShowRight);
    addAndMakeVisible(*visualizerShowRightButton);

    visualizerShowMidButton = makeVisualizerLegendButton("M", visualizerMidColour, visualizerShowMid);
    addAndMakeVisible(*visualizerShowMidButton);

    visualizerShowSideButton = makeVisualizerLegendButton("S", visualizerSideColour, visualizerShowSide);
    addAndMakeVisible(*visualizerShowSideButton);

    visualizerVisibilityButton = std::make_unique<BoxTextButton>(uiGrey500);
    visualizerVisibilityButton->setButtonText("SHOW");
    visualizerVisibilityButton->setClickingTogglesState(true);
    visualizerVisibilityButton->onClick = [this]
    {
        visualizerVisible = visualizerVisibilityButton->getToggleState();
        updateEditorWidthForVisualizerVisibility();
        updateSectionStates();
        resized();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*visualizerVisibilityButton);

    visualizerComponent = std::make_unique<EqResponseVisualizerComponent>(audioProcessor,
                                                                          [this] (const int bellIndex)
                                                                          {
                                                                              selectBellSection(bellIndex);
                                                                              clearKeyboardFocus(*this);
                                                                          });
    addAndMakeVisible(*visualizerComponent);
#endif

    presetsSection = std::make_unique<PresetsSection>();
    presetsSection->header->onClick = [this]
    {
        openPresetsSection();
        clearKeyboardFocus(*this);
    };
    presetsSection->onPresetSelected = [this]
    {
        if (presetsSection == nullptr)
            return;

        const auto presetName = presetsSection->getSelectedPresetName();

        if (presetName.isEmpty())
            return;

        const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);

        if (audioProcessor.loadFilterPreset(presetName))
        {
            reloadFilterPresetFromProcessor();
            refreshFilterPresetList(presetName);
        }
    };
    presetsSection->adButton->onClick = [this]
    {
        addFilterPreset();
        clearKeyboardFocus(*this);
    };
    presetsSection->saveButton->onClick = [this]
    {
        saveFilterPreset();
        clearKeyboardFocus(*this);
    };
    presetsSection->renameButton->onClick = [this]
    {
        if (presetsSection != nullptr)
            presetsSection->beginRename();
    };
    presetsSection->defaultButton->onClick = [this]
    {
        setDefaultFilterPreset();
        clearKeyboardFocus(*this);
    };
    presetsSection->deleteButton->onClick = [this]
    {
        deleteSelectedFilterPreset();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*presetsSection->header);
    addAndMakeVisible(presetsSection->presetCombo);
    addAndMakeVisible(*presetsSection->adButton);
    addAndMakeVisible(*presetsSection->saveButton);
    addAndMakeVisible(*presetsSection->renameButton);
    addAndMakeVisible(*presetsSection->defaultButton);
    addAndMakeVisible(*presetsSection->deleteButton);
    presetsSection->onRenameRequested = [this] (const juce::String& currentName)
    {
        showTextPrompt(currentName,
                       [this, currentName] (const juce::String& newName)
                       {
                           if (! renameFilterPreset(currentName, newName))
                               return false;

                           clearKeyboardFocus(*this);
                           return true;
                       });
    };
    refreshFilterPresetList(audioProcessor.getLastFilterPresetName());

    bellDisplayOrder.reserve(EqeAudioProcessor::maxBellFilterCount);
    for (int bellIndex = 0; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
        bellDisplayOrder.push_back(bellIndex);

    restoreEditorStateFromValueTree();

    for (int bellIndex = 0; bellIndex < EqeAudioProcessor::maxBellFilterCount; ++bellIndex)
    {
        auto section = std::make_unique<BellSection>(valueTreeState, bellIndex);
        for (const auto filterType : EqeAudioProcessor::filterTypePresetOrder)
        {
            section->setStoredValues(filterType,
                                     defaultFilterFrequencyForType(filterType),
                                     defaultFilterBandwidthForType(filterType),
                                     defaultFilterSlopeForType(filterType),
                                     0,
                                     false);
        }
        section->captureCurrentValuesForCurrentType(true);
        section->moveUpButton->onClick = [this, bellIndex]
        {
            const auto orderPosition = getBellOrderPositionForIndex(bellIndex);

            if (orderPosition > 0)
                moveBellSection(orderPosition, orderPosition - 1);

            clearKeyboardFocus(*this);
        };
        section->moveDownButton->onClick = [this, bellIndex]
        {
            const auto orderPosition = getBellOrderPositionForIndex(bellIndex);

            if (orderPosition >= 0 && orderPosition + 1 < getActiveBellCount())
                moveBellSection(orderPosition, orderPosition + 1);

            clearKeyboardFocus(*this);
        };
        section->typeControl->setTitleMouseEnabled(false);
        section->lrmsControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            bellSection->lrmsControl->setSelectedChoiceIndex(0, true);
        };
        section->frequencyControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto filterType = bellSection->getFilterType();
            bellSection->frequencyControl->setValue(defaultFilterFrequencyForType(filterType), true);
        };
        section->bandwidthControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto filterType = bellSection->getFilterType();
            bellSection->bandwidthControl->setValue(defaultFilterBandwidthForType(filterType), true);
        };
        section->slopeControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto filterType = bellSection->getFilterType();
            bellSection->slopeControl->setSelectedChoiceIndex(
                EqeAudioProcessor::getBellSlopeChoiceIndexForValue(defaultFilterSlopeForType(filterType)),
                true);
        };
        section->gainControl->onTitleClick = [this, bellIndex]
        {
            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            bellSection->gainControl->setValue(0.0, true);
        };
        section->typeControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get();

            if (bellSection == nullptr)
                return;

            const auto newType = bellSection->getFilterType();
            bellSection->lastFilterType = newType;
            bellSection->slopeControl->setChoices(getBellSlopeDisplayChoicesForType(newType));
            bellSection->slopeControl->setChoiceEnabled(0, newType != EqeAudioProcessor::FilterType::bell);
            bellSection->captureCurrentValuesForCurrentType();
            updateSectionStates();
            resized();
        };
        section->frequencyControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
            {
                bellSection->captureCurrentValuesForCurrentType();

                updateSectionStates();
            }
        };
        section->lrmsControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
            {
                bellSection->captureCurrentValuesForCurrentType();
                updateSectionStates();
            }
        };
        section->bandwidthControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
                bellSection->captureCurrentValuesForCurrentType();

            updateSectionStates();
        };
        section->slopeControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            normalizeSlopeForType(bellIndex);

            if (auto* bellSection = bellSections[static_cast<size_t>(bellIndex)].get())
                bellSection->captureCurrentValuesForCurrentType();

            updateSectionStates();
        };
        section->gainControl->onValueChanged = [this, bellIndex]
        {
            if (suppressBellSectionValueChangeHandlers)
                return;

            juce::ignoreUnused(bellIndex);

            updateSectionStates();
        };
        section->header->onClick = [this, bellIndex]
        {
            openBellSection(bellIndex);
            clearKeyboardFocus(*this);
        };
        section->bypassButton->onClick = [this]
        {
            updateSectionStates();
            clearKeyboardFocus(*this);
        };
        section->deleteButton->onClick = [this, bellIndex]
        {
            const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);
            const auto previousCount = audioProcessor.getActiveBellCount();

            if (audioProcessor.removeBellFilter(bellIndex))
            {
                removeBellSectionStoredValues(bellIndex, previousCount);
                globalExpanded = false;
                filtersExpanded = true;
                presetsExpanded = false;
#if ! JUCE_IOS
                visualizerExpanded = false;
#endif

                const auto newCount = audioProcessor.getActiveBellCount();

                if (newCount <= 0)
                {
                    expandedBellIndex = -1;
                }
                else if (expandedBellIndex > bellIndex)
                {
                    --expandedBellIndex;
                }
                else if (expandedBellIndex == bellIndex)
                {
                    expandedBellIndex = juce::jlimit(0, newCount - 1, bellIndex);
                }

                storeEditorStateToValueTree();

                updateSectionStates();
                resized();
            }

            clearKeyboardFocus(*this);
        };

        filterContent.addAndMakeVisible(*section->moveUpButton);
        filterContent.addAndMakeVisible(*section->header);
        filterContent.addAndMakeVisible(*section->moveDownButton);
        filterContent.addAndMakeVisible(*section->typeControl);
        filterContent.addAndMakeVisible(*section->lrmsControl);
        filterContent.addAndMakeVisible(*section->frequencyControl);
        filterContent.addAndMakeVisible(*section->bandwidthControl);
        filterContent.addAndMakeVisible(*section->slopeControl);
        filterContent.addAndMakeVisible(*section->gainControl);
        filterContent.addAndMakeVisible(*section->bypassButton);
        filterContent.addAndMakeVisible(*section->deleteButton);
        bellSections[static_cast<size_t>(bellIndex)] = std::move(section);

        normalizeSlopeForType(bellIndex);
    }

    addFilterButton = std::make_unique<BoxTextButton>(uiGrey500);
    addFilterButton->setButtonText("ADD");
    addFilterButton->onClick = [this]
    {
        const juce::ScopedValueSetter<bool> suppressHandlers(suppressBellSectionValueChangeHandlers, true);

        if (audioProcessor.addBellFilter())
        {
            const auto newBellIndex = getActiveBellCount() - 1;
            bellDisplayOrder[static_cast<size_t>(newBellIndex)] = newBellIndex;
            resetBellSectionStoredValues(getActiveBellCount() - 1);
            globalExpanded = false;
            filtersExpanded = true;
            presetsExpanded = false;
#if ! JUCE_IOS
            visualizerExpanded = false;
#endif
            expandedBellIndex = getActiveBellCount() - 1;
            storeEditorStateToValueTree();
            updateSectionStates();
            resized();
        }

        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*addFilterButton);

    footerTab = std::make_unique<BoxTextButton>(uiGrey500);
    footerTab->setButtonText("VX by MIXOLVE");
    footerTab->onClick = [this]
    {
        showInfoPrompt(getMixolveInfoMarkdown());
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*footerTab);

    globalSectionFrame = std::make_unique<SectionFrameComponent>();
    filtersSectionFrame = std::make_unique<SectionFrameComponent>();
    presetsSectionFrame = std::make_unique<SectionFrameComponent>();
#if ! JUCE_IOS
    visualizerSectionFrame = std::make_unique<SectionFrameComponent>();
#endif
    addAndMakeVisible(*globalSectionFrame);
    addAndMakeVisible(*filtersSectionFrame);
    addAndMakeVisible(*presetsSectionFrame);
#if ! JUCE_IOS
    addAndMakeVisible(*visualizerSectionFrame);
#endif

    startTimerHz(30);
    registerParameterListeners();

    updateSectionStates();
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);

    const auto restoredEditorSize = getRestoredEditorSize();
#if ! JUCE_IOS
    lastCollapsedEditorWidth = juce::jlimit(minimumEditorWidth,
                                            juce::jmax(minimumEditorWidth, maximumEditorWidth - minimumVisualizerWidth),
                                            lastCollapsedEditorWidth);
    lastVisualizerWidth = juce::jmax(minimumVisualizerWidth, lastVisualizerWidth);

    if (visualizerVisible)
    {
        const auto minimumVisibleWidth = lastCollapsedEditorWidth + visualizerPanelGap + minimumVisualizerWidth;
        const auto preferredVisibleWidth = lastCollapsedEditorWidth + visualizerPanelGap + lastVisualizerWidth;
        const auto width = juce::jmax(minimumVisibleWidth,
                                      juce::jmax(restoredEditorSize.x, preferredVisibleWidth));
        setResizeLimits(minimumVisibleWidth,
                        minimumEditorHeight,
                        maximumEditorWidth,
                        maximumEditorHeight);
        setSize(width, restoredEditorSize.y);
        audioProcessor.setLastEditorSize(width, restoredEditorSize.y);
    }
    else
    {
        setResizeLimits(minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
        setSize(restoredEditorSize.x, restoredEditorSize.y);
        audioProcessor.setLastEditorSize(restoredEditorSize.x, restoredEditorSize.y);
    }

    suppressEditorSizeStateSave = false;
    refreshVisualizerResponse();
#else
    setSize(restoredEditorSize.x, restoredEditorSize.y);
    audioProcessor.setLastEditorSize(restoredEditorSize.x, restoredEditorSize.y);
    suppressEditorSizeStateSave = false;
#endif

    audioProcessor.getStateInformation(committedHistorySnapshot);
    updateUndoRedoButtons();
}
