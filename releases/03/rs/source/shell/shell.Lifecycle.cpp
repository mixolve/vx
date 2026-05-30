#include "shell.EditorBellSection.h"
#include "shell.EditorPresetSections.h"
#include "../modules/mxe/module.mxe.ModuleComponent.h"
#include "../modules/mxe/module.mxe.EditorControls.h"

#include <cmath>

VxAudioProcessorEditor::~VxAudioProcessorEditor()
{
    commitPendingHistorySnapshot(true);
    unregisterParameterListeners();
    storeEditorStateToValueTree();
    stopTimer();
    setLookAndFeel(nullptr);
}

void VxAudioProcessorEditor::timerCallback()
{
    commitPendingHistorySnapshot();
    syncFocusedParameterControl();

    const auto clipValue = audioProcessor.getGlobalClipIndicator();

    if (shellGlobalHeader != nullptr)
        shellGlobalHeader->setLeadingDotLevel(clipValue);

    if (clipControl == nullptr)
    {
        if (auto* mxeEditor = dynamic_cast<MxeModuleComponent*>(mxeModuleEditor.get()))
            mxeEditor->refreshExternalState();

        return;
    }

    if (std::abs(clipControl->getValue() - clipValue) > 1.0e-6)
        clipControl->setValue(clipValue, false);

    if (auto* mxeEditor = dynamic_cast<MxeModuleComponent*>(mxeModuleEditor.get()))
        mxeEditor->refreshExternalState();

    refreshVisualizerResponse();
}

void VxAudioProcessorEditor::syncFocusedParameterControl()
{
    if (focusedParameterControl == nullptr)
        return;

    shell_parameter_focus::clearFocusIfNotShowing();
    mxe::editor::parameter_focus::clearFocusIfNotShowing();

    auto* nextTarget = shell_parameter_focus::getFocusedValueSlider();

    if (nextTarget == nullptr)
        nextTarget = mxe::editor::parameter_focus::getFocusedValueSlider();

    if (nextTarget != focusedParameterTargetSlider)
    {
        const auto preservedGlobalScrollY = globalViewport.getViewPositionY();
        const auto preservedFilterScrollY = filterViewport.getViewPositionY();

        focusedParameterTargetSlider = nextTarget;

        if (focusedParameterTargetSlider != nullptr)
        {
            focusedParameterControlPinnedX = juce::jmax(0, getWidth() - getEditorInsetX(getWidth()) - rowHeight);

            const auto& targetRange = focusedParameterTargetSlider->getNormalisableRange();
            const juce::ScopedValueSetter<bool> scopedIgnore(suppressFocusedParameterControlChangeHandlers, true);
            focusedParameterControl->setValue(targetRange.convertTo0to1(static_cast<float>(focusedParameterTargetSlider->getValue())),
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

        const auto globalMaxOffset = juce::jmax(0, getActiveGlobalContentHeight() - globalViewport.getHeight());
        globalViewport.setViewPosition(0, juce::jlimit(0, globalMaxOffset, preservedGlobalScrollY));

        const auto filterMaxOffset = juce::jmax(0, getActiveFilterContentHeight() - filterViewport.getHeight());
        filterViewport.setViewPosition(0, juce::jlimit(0, filterMaxOffset, preservedFilterScrollY));
    }

    if (focusedParameterTargetSlider == nullptr || focusedParameterControl->isMouseButtonDown())
        return;

    const auto& targetRange = focusedParameterTargetSlider->getNormalisableRange();
    const auto targetValue = targetRange.convertTo0to1(static_cast<float>(focusedParameterTargetSlider->getValue()));

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
    const auto directionalDelta = wheel.isReversed ? wheel.deltaY : -wheel.deltaY;
    const auto scrollAmount = wheel.isSmooth
        ? static_cast<int>(std::round(directionalDelta * focusedParameterScrollSensitivity))
        : static_cast<int>(std::round((directionalDelta < 0.0f ? -1.0f : 1.0f) * juce::jmax(1.0f, focusedParameterScrollSensitivity / 4.0f)));

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

    if (shellGlobalHostViewport.getBounds().contains(event.getPosition()))
    {
        if (scrollViewport(shellGlobalHostViewport, shellGlobalHostContent.getHeight()))
            return;
    }

    if (globalViewport.getBounds().contains(event.getPosition()))
    {
        if (scrollViewport(globalViewport, getActiveGlobalContentHeight()))
            return;
    }

    if (! filterViewport.getBounds().contains(event.getPosition()))
        return;

    scrollViewport(filterViewport, getActiveFilterContentHeight());
}

bool VxAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
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
