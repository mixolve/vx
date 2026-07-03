#include "shell.EditorFilterSection.h"
#include "shell.EditorPresetSections.h"
#include "shell.MultibandComponent.h"

#include <cmath>

namespace
{
bool usesLogFocusedParameterScale(const juce::Slider& slider) noexcept
{
    const auto minimum = static_cast<double>(slider.getMinimum());
    const auto maximum = static_cast<double>(slider.getMaximum());

    return minimum > 0.0 && maximum / minimum >= 100.0;
}

double sliderValueToFocusedParameterValue(const juce::Slider& slider) noexcept
{
    const auto currentValue = static_cast<double>(slider.getValue());

    if (usesLogFocusedParameterScale(slider))
    {
        const auto minimum = static_cast<double>(slider.getMinimum());
        const auto maximum = static_cast<double>(slider.getMaximum());
        return juce::jlimit(0.0,
                            1.0,
                            std::log(currentValue / minimum) / std::log(maximum / minimum));
    }

    return slider.getNormalisableRange().convertTo0to1(currentValue);
}

double focusedParameterValueToSliderValue(const juce::Slider& slider, const double focusedValue) noexcept
{
    const auto normalisedValue = juce::jlimit(0.0, 1.0, focusedValue);

    if (usesLogFocusedParameterScale(slider))
    {
        const auto minimum = static_cast<double>(slider.getMinimum());
        const auto maximum = static_cast<double>(slider.getMaximum());
        return minimum * std::pow(maximum / minimum, normalisedValue);
    }

    return slider.getNormalisableRange().convertFrom0to1(normalisedValue);
}
}

VxAudioProcessorEditor::~VxAudioProcessorEditor()
{
    commitPendingHistorySnapshot(true);
    unregisterParameterListeners();
    storeEditorStateToValueTree();
    stopTimer();
    tooltipWindow.reset();
    setLookAndFeel(nullptr);
}

void VxAudioProcessorEditor::timerCallback()
{
    commitPendingHistorySnapshot();
    syncFocusedParameterControl();

    const auto clipValue = audioProcessor.getGlobalClipIndicator();
    const auto now = juce::Time::getMillisecondCounter();

    if (clipButton != nullptr)
    {
        if (clipValue > 0.5f)
            lastClipIndicatorTimeMs = now;

        constexpr uint32_t clipIndicatorHoldMs = 500;
        const auto showClipIndicator = lastClipIndicatorTimeMs != 0
            && now - lastClipIndicatorTimeMs < clipIndicatorHoldMs;

        if (showClipIndicator)
            clipButton->setTextColourOverride(juce::Colour(0xffff9999));
        else
            clipButton->clearTextColourOverride();
    }

    if (auto* mieEditor = dynamic_cast<MultibandModuleComponent*>(mieModuleEditor.get()))
        mieEditor->refreshExternalState();

    if (auto* mxeEditor = dynamic_cast<MultibandModuleComponent*>(mxeModuleEditor.get()))
        mxeEditor->refreshExternalState();

    refreshSpeAnalyserResponse();
}

double VxAudioProcessorEditor::getFocusedParameterControlValueForTarget() const noexcept
{
    if (focusedParameterTargetSlider == nullptr)
        return 0.0;

    return sliderValueToFocusedParameterValue(*focusedParameterTargetSlider);
}

double VxAudioProcessorEditor::getFocusedParameterTargetValueForControl() const noexcept
{
    if (focusedParameterControl == nullptr || focusedParameterTargetSlider == nullptr)
        return 0.0;

    return focusedParameterValueToSliderValue(*focusedParameterTargetSlider, focusedParameterControl->getValue());
}

void VxAudioProcessorEditor::syncFocusedParameterControl()
{
    if (focusedParameterControl == nullptr)
        return;

    shell_parameter_focus::clearFocusIfNotShowing();

    auto* nextTarget = shell_parameter_focus::getFocusedValueSlider();

    if (nextTarget != focusedParameterTargetSlider)
    {
        const auto preservedFilterScrollY = filterViewport.getViewPositionY();
        const auto preservedSpeAnalyserScrollY = speAnalyserViewport.getViewPositionY();

        focusedParameterTargetSlider = nextTarget;

        if (focusedParameterTargetSlider != nullptr)
        {
            const juce::ScopedValueSetter<bool> scopedIgnore(suppressFocusedParameterControlChangeHandlers, true);
            focusedParameterControl->setValue(getFocusedParameterControlValueForTarget(),
                                              juce::dontSendNotification);
            focusedParameterControl->setEnabled(true);
            focusedParameterControl->setColour(juce::Slider::backgroundColourId, uiGrey700);
            focusedParameterControl->setColour(juce::Slider::trackColourId, juce::Colour(0xFF99CC99));
            focusedParameterControl->setAlpha(1.0f);
        }
        else
        {
            focusedParameterControl->setEnabled(false);
            focusedParameterControl->setColour(juce::Slider::backgroundColourId, uiGrey800);
            focusedParameterControl->setColour(juce::Slider::trackColourId, uiGrey500);
            focusedParameterControl->setAlpha(1.0f);
        }

        resized();

        const auto filterMaxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
        filterViewport.setViewPosition(0, juce::jlimit(0, filterMaxOffset, preservedFilterScrollY));

        const auto analyserMaxOffset = juce::jmax(0, speAnalyserContent.getHeight() - speAnalyserViewport.getHeight());
        speAnalyserViewport.setViewPosition(0, juce::jlimit(0, analyserMaxOffset, preservedSpeAnalyserScrollY));
    }

    if (focusedParameterTargetSlider == nullptr || focusedParameterControl->isMouseButtonDown())
        return;

    const auto targetValue = getFocusedParameterControlValueForTarget();

    if (std::abs(focusedParameterControl->getValue() - targetValue) > 1.0e-6)
    {
        const juce::ScopedValueSetter<bool> scopedIgnore(suppressFocusedParameterControlChangeHandlers, true);
        focusedParameterControl->setValue(targetValue, juce::dontSendNotification);
    }
}

void VxAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    clearKeyboardFocus(*this);
}

void VxAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (hostParametersViewport.getBounds().contains(event.getPosition()))
    {
        if (scrollViewportWithWheel(hostParametersViewport, hostParametersContent.getHeight(), wheel))
            return;
    }

    if (speAnalyserViewport.getBounds().contains(event.getPosition()))
    {
        if (scrollViewportWithWheel(speAnalyserViewport, speAnalyserContent.getHeight(), wheel))
            return;
    }

    if (! filterViewport.getBounds().contains(event.getPosition()))
        return;

    scrollViewportWithWheel(filterViewport, getActiveFilterContentHeight(), wheel);
}

bool VxAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    juce::ignoreUnused(key);
    return false;
}
