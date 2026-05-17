#include "EditorBellSection.h"
#include "EditorPresetSections.h"

#include <cmath>

EqeAudioProcessorEditor::~EqeAudioProcessorEditor()
{
    commitPendingHistorySnapshot(true);
    unregisterParameterListeners();
    storeEditorStateToValueTree();
    stopTimer();
    setLookAndFeel(nullptr);
}

void EqeAudioProcessorEditor::timerCallback()
{
    commitPendingHistorySnapshot();

    const auto clipValue = audioProcessor.getGlobalClipIndicator();

    if (globalHeader != nullptr)
        globalHeader->setLeadingDotLevel(clipValue);

    if (clipControl == nullptr)
        return;

    if (std::abs(clipControl->getValue() - clipValue) > 1.0e-6)
        clipControl->setValue(clipValue, false);

#if ! JUCE_IOS
    refreshVisualizerResponse();
#endif
}

void EqeAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    clearKeyboardFocus(*this);
}

void EqeAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto directionalDelta = wheel.isReversed ? wheel.deltaY : -wheel.deltaY;
    const auto scrollAmount = wheel.isSmooth
        ? static_cast<int>(std::round(directionalDelta * 220.0f))
        : static_cast<int>(std::round((directionalDelta < 0.0f ? -1.0f : 1.0f) * 48.0f));

    if (scrollAmount == 0)
        return;

    auto scrollViewport = [scrollAmount] (juce::Viewport& viewport, const int contentHeight)
    {
        const auto maxOffset = juce::jmax(0, contentHeight - viewport.getHeight());

        if (maxOffset <= 0)
            return false;

        const auto currentY = viewport.getViewPositionY();
        const auto nextY = juce::jlimit(0, maxOffset, currentY + scrollAmount);
        viewport.setViewPosition(0, nextY);
        return true;
    };

    if (globalViewport.getBounds().contains(event.getPosition()))
    {
        if (scrollViewport(globalViewport, getGlobalContentHeight()))
            return;
    }

    if (! filterViewport.getBounds().contains(event.getPosition()))
        return;

    scrollViewport(filterViewport, getFilterContentHeight());
}

bool EqeAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::upKey)
    {
        selectAdjacentBellSection(-1);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        selectAdjacentBellSection(1);
        return true;
    }

    return false;
}
