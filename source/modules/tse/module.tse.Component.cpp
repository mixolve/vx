#include "module.tse.Component.h"

#include "shell.Constants.h"
#include "shell.EditorControls.h"
#include "shell.EditorParameterControls.h"
#include "shell.EditorStyle.h"

#include <atomic>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
constexpr auto miscExpandedStateKey = "tse.misc.expanded";
constexpr auto stereoExpandedStateKey = "tse.stereo.expanded";

class SectionFrameComponent final : public juce::Component
{
public:
    explicit SectionFrameComponent(const juce::Colour outline)
        : outlineColour(outline)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.setColour(outlineColour);
        graphics.drawRect(getLocalBounds(), frameLineThickness);
    }

private:
    juce::Colour outlineColour;
};

std::unique_ptr<BoxTextButton> makeHeaderButton(const juce::String& text)
{
    auto button = std::make_unique<BoxTextButton>(uiAccent);
    button->setButtonText(text);
    button->setTextJustification(juce::Justification::centred);
    return button;
}

std::unique_ptr<BoxTextButton> makeTimeModeButton()
{
    auto button = std::make_unique<BoxTextButton>(uiGrey500);
    button->setButtonText("M");
    button->setTextJustification(juce::Justification::centred);
    button->setCancelClickOnLeave(true);
    return button;
}

void resetControl(ParameterControl* control, const double value, std::function<void()> clearFocus)
{
    if (control != nullptr)
        control->setValue(value, true);

    if (clearFocus != nullptr)
        clearFocus();
}

float readRawParameter(juce::AudioProcessorValueTreeState& state,
                       const juce::String& parameterId,
                       const float fallback) noexcept
{
    if (auto* value = state.getRawParameterValue(parameterId))
        return value->load(std::memory_order_relaxed);

    return fallback;
}

void includeComponentBounds(juce::Rectangle<int>& sectionBounds,
                            const juce::Component* component)
{
    if (component == nullptr || ! component->isVisible() || component->getBounds().isEmpty())
        return;

    sectionBounds = sectionBounds.isEmpty() ? component->getBounds()
                                           : sectionBounds.getUnion(component->getBounds());
}

void placeSectionFrame(juce::Component* frame,
                       const bool shouldShow,
                       juce::Rectangle<int> sectionBounds,
                       const juce::Rectangle<int>& contentBounds)
{
    if (frame == nullptr)
        return;

    frame->setVisible(shouldShow && ! sectionBounds.isEmpty());

    if (! frame->isVisible())
    {
        frame->setBounds({});
        return;
    }

    frame->setBounds(sectionBounds.expanded(internalFrameInsetX, internalFrameInsetY).getIntersection(contentBounds));
    frame->toFront(false);
}

}

TseModuleComponent::TseModuleComponent(TseModuleProcessor& processorToEdit, Callbacks callbacksIn)
    : processor(processorToEdit),
      valueTreeState(processorToEdit.getValueTreeState()),
      callbacks(std::move(callbacksIn))
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(false, false);
    viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    viewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(viewport);

    miscExpanded = loadSectionExpandedState(miscExpandedStateKey);
    stereoExpanded = loadSectionExpandedState(stereoExpandedStateKey);

    miscHeader = makeHeaderButton("MISC");
    miscHeader->setClickingTogglesState(true);
    miscHeader->setToggleState(miscExpanded, juce::dontSendNotification);
    miscHeader->onClick = [this]
    {
        handleSectionClick(Section::misc);
        clearFocus();
    };
    content.addAndMakeVisible(*miscHeader);

    stereoHeader = makeHeaderButton("STEREO");
    stereoHeader->setClickingTogglesState(true);
    stereoHeader->setToggleState(stereoExpanded, juce::dontSendNotification);
    stereoHeader->onClick = [this]
    {
        handleSectionClick(Section::stereo);
        clearFocus();
    };
    content.addAndMakeVisible(*stereoHeader);

    closeModuleButton = makeHeaderButton("CLOSE-MODULE");
    closeModuleButton->onClick = [this]
    {
        clearFocus();
    };
    closeModuleButton->setLongPressAction([this]
    {
        if (callbacks.requestCloseActiveModule != nullptr)
            callbacks.requestCloseActiveModule();

        clearFocus();
    }, 500, "SURE?");
    content.addAndMakeVisible(*closeModuleButton);

    bypassButton = std::make_unique<BoxTextButton>(uiAccent);
    bypassButton->setButtonText("BYPASS");
    bypassButton->setTextJustification(juce::Justification::centred);
    bypassButton->setClickingTogglesState(true);
    bypassAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                          TseModuleProcessor::paramBypassId,
                                                          *bypassButton);
    bypassButton->onClick = [this]
    {
        clearFocus();
    };
    content.addAndMakeVisible(*bypassButton);

    bypassWithGainButton = std::make_unique<BoxTextButton>(uiAccent);
    bypassWithGainButton->setButtonText("BYPASS.WT-GAIN");
    bypassWithGainButton->setTextJustification(juce::Justification::centred);
    bypassWithGainButton->setClickingTogglesState(true);
    bypassWithGainAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                                  TseModuleProcessor::paramBypassWithGainId,
                                                                  *bypassWithGainButton);
    bypassWithGainButton->onClick = [this]
    {
        clearFocus();
    };
    content.addAndMakeVisible(*bypassWithGainButton);

    transOnButton = std::make_unique<BoxTextButton>(uiAccent);
    transOnButton->setButtonText("TRANS.ON");
    transOnButton->setTextJustification(juce::Justification::centred);
    transOnButton->setClickingTogglesState(true);
    transOnAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                           TseModuleProcessor::paramTransOnId,
                                                           *transOnButton);
    transOnButton->onStateChange = [this]
    {
        updateBranchButtonLabels();
    };
    transOnButton->onClick = [this]
    {
        updateBranchButtonLabels();
        clearFocus();
    };
    content.addAndMakeVisible(*transOnButton);

    sustainOnButton = std::make_unique<BoxTextButton>(uiAccent);
    sustainOnButton->setButtonText("SUS.ON");
    sustainOnButton->setTextJustification(juce::Justification::centred);
    sustainOnButton->setClickingTogglesState(true);
    sustainOnAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                             TseModuleProcessor::paramSusOnId,
                                                             *sustainOnButton);
    sustainOnButton->onStateChange = [this]
    {
        updateBranchButtonLabels();
    };
    sustainOnButton->onClick = [this]
    {
        updateBranchButtonLabels();
        clearFocus();
    };
    content.addAndMakeVisible(*sustainOnButton);

    transGainControl = std::make_unique<ParameterControl>(valueTreeState,
                                                          TseModuleProcessor::paramTransGainId,
                                                          "TRANS.GAIN",
                                                          1);
    transGainControl->setTitleLongPressAction([this]
    {
        resetControl(transGainControl.get(), 0.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*transGainControl);

    sustainGainControl = std::make_unique<ParameterControl>(valueTreeState,
                                                            TseModuleProcessor::paramSusGainId,
                                                            "SUS.GAIN",
                                                            1);
    sustainGainControl->setTitleLongPressAction([this]
    {
        resetControl(sustainGainControl.get(), 0.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*sustainGainControl);

    holdControl = std::make_unique<ParameterControl>(valueTreeState,
                                                     TseModuleProcessor::paramTimeHoldId,
                                                     "HOLD",
                                                     0);
    holdControl->setTitleWidthOverride(0);
    holdControl->setTitleLongPressAction([this]
    {
        resetControl(holdControl.get(), 0.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*holdControl);

    holdModeButton = makeTimeModeButton();
    holdModeButton->onClick = [this]
    {
        handleTimeModeButton(TimeTarget::hold);
        clearFocus();
    };
    content.addAndMakeVisible(*holdModeButton);

    releaseControl = std::make_unique<ParameterControl>(valueTreeState,
                                                        TseModuleProcessor::paramTimeReleaseId,
                                                        "RELEASE",
                                                        0);
    releaseControl->setTitleWidthOverride(0);
    releaseControl->setTitleLongPressAction([this]
    {
        resetControl(releaseControl.get(), 10.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*releaseControl);

    releaseCurveControl = std::make_unique<ParameterControl>(valueTreeState,
                                                             TseModuleProcessor::paramTimeReleaseCurveId,
                                                             "REL-CURVE",
                                                             0);
    releaseCurveControl->setTitleLongPressAction([this]
    {
        resetControl(releaseCurveControl.get(), 0.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*releaseCurveControl);

    releaseModeButton = makeTimeModeButton();
    releaseModeButton->onClick = [this]
    {
        handleTimeModeButton(TimeTarget::release);
        clearFocus();
    };
    content.addAndMakeVisible(*releaseModeButton);

    thresholdControl = std::make_unique<ParameterControl>(valueTreeState,
                                                          TseModuleProcessor::paramSensLevelId,
                                                          "SENS.LVL",
                                                          1);
    thresholdControl->setTitleLongPressAction([this]
    {
        resetControl(thresholdControl.get(), -42.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*thresholdControl);

    kneeControl = std::make_unique<ParameterControl>(valueTreeState,
                                                     TseModuleProcessor::paramSensKneeId,
                                                     "SENS.KNEE",
                                                     1);
    kneeControl->setTitleLongPressAction([this]
    {
        resetControl(kneeControl.get(), 0.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*kneeControl);

    retriggerControl = std::make_unique<ParameterControl>(valueTreeState,
                                                          TseModuleProcessor::paramSensRetriggerId,
                                                          "SENS.RETR",
                                                          0);
    retriggerControl->setTitleLongPressAction([this]
    {
        resetControl(retriggerControl.get(), 100.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*retriggerControl);

    lookaheadControl = std::make_unique<ParameterControl>(valueTreeState,
                                                          TseModuleProcessor::paramMiscLookaheadId,
                                                          "LOOKAHEAD",
                                                          2);
    lookaheadControl->setTitleLongPressAction([this]
    {
        resetControl(lookaheadControl.get(), 1.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*lookaheadControl);

    outGainControl = std::make_unique<ParameterControl>(valueTreeState,
                                                        TseModuleProcessor::paramMiscOutGainId,
                                                        "OUT-GAIN",
                                                        1);
    outGainControl->setTitleLongPressAction([this]
    {
        resetControl(outGainControl.get(), 0.0, [this] { clearFocus(); });
    });
    content.addAndMakeVisible(*outGainControl);

    valueTreeState.addParameterListener(TseModuleProcessor::paramTimeHoldModeId, this);
    valueTreeState.addParameterListener(TseModuleProcessor::paramTimeHoldSyncId, this);
    valueTreeState.addParameterListener(TseModuleProcessor::paramTimeReleaseModeId, this);
    valueTreeState.addParameterListener(TseModuleProcessor::paramTimeReleaseSyncId, this);

    miscSectionFrame = std::make_unique<SectionFrameComponent>(uiAccent);
    stereoSectionFrame = std::make_unique<SectionFrameComponent>(uiAccent);
    content.addAndMakeVisible(*miscSectionFrame);
    content.addAndMakeVisible(*stereoSectionFrame);

    updateBranchButtonLabels();
    updateTimeModeControls();
    updateSectionVisibility();
}

TseModuleComponent::~TseModuleComponent()
{
    valueTreeState.removeParameterListener(TseModuleProcessor::paramTimeHoldModeId, this);
    valueTreeState.removeParameterListener(TseModuleProcessor::paramTimeHoldSyncId, this);
    valueTreeState.removeParameterListener(TseModuleProcessor::paramTimeReleaseModeId, this);
    valueTreeState.removeParameterListener(TseModuleProcessor::paramTimeReleaseSyncId, this);
    viewport.setViewedComponent(nullptr, false);
}

void TseModuleComponent::paint(juce::Graphics& graphics)
{
    juce::ignoreUnused(graphics);
}

void TseModuleComponent::resized()
{
    auto bounds = getContentBounds();
    viewport.setBounds(bounds);
    content.setSize(bounds.getWidth(), juce::jmax(bounds.getHeight(), getPreferredContentHeight()));

    auto contentBounds = content.getLocalBounds();
    const auto placeRow = [&contentBounds] (juce::Component* component)
    {
        if (component == nullptr)
            return;

        component->setBounds(contentBounds.removeFromTop(rowHeight));

        if (! contentBounds.isEmpty())
            contentBounds.removeFromTop(verticalGap);
    };
    const auto placeTimeRow = [&contentBounds] (ParameterControl* control, BoxTextButton* modeButton)
    {
        auto rowBounds = contentBounds.removeFromTop(rowHeight);

        if (control != nullptr && modeButton != nullptr)
        {
            const auto leftWidth = getScaledParameterNameWidth(rowBounds.getWidth());
            const auto rightWidth = juce::jmax(0, rowBounds.getWidth() - leftWidth - parameterGap);
            auto titleBounds = rowBounds.removeFromLeft(leftWidth);
            rowBounds.removeFromLeft(juce::jmin(parameterGap, rowBounds.getWidth()));

            auto rightBounds = rowBounds.withWidth(rightWidth);
            auto buttonBounds = rightBounds.removeFromRight(rowHeight);

            if (! rightBounds.isEmpty())
                rightBounds.removeFromRight(juce::jmin(parameterGap, rightBounds.getWidth()));

            auto valueBounds = rightBounds;

            control->setTitleWidthOverride(titleBounds.getWidth());
            control->setBounds(titleBounds.withRight(valueBounds.getRight()));
            modeButton->setBounds(buttonBounds);
        }
        else if (control != nullptr)
        {
            control->setBounds(rowBounds);
        }

        if (! contentBounds.isEmpty())
            contentBounds.removeFromTop(verticalGap);
    };

    placeRow(miscHeader.get());
    if (miscExpanded)
    {
        placeRow(closeModuleButton.get());
        placeRow(bypassButton.get());
        placeRow(bypassWithGainButton.get());
    }

    placeRow(stereoHeader.get());
    if (stereoExpanded)
    {
        placeRow(transOnButton.get());
        placeRow(sustainOnButton.get());
        placeRow(transGainControl.get());
        placeRow(sustainGainControl.get());
        placeTimeRow(holdControl.get(), holdModeButton.get());
        placeTimeRow(releaseControl.get(), releaseModeButton.get());
        placeRow(releaseCurveControl.get());
        placeRow(thresholdControl.get());
        placeRow(kneeControl.get());
        placeRow(retriggerControl.get());
        placeRow(lookaheadControl.get());
        placeRow(outGainControl.get());
    }

    juce::Rectangle<int> miscFrameBounds;
    includeComponentBounds(miscFrameBounds, miscHeader.get());
    includeComponentBounds(miscFrameBounds, closeModuleButton.get());
    includeComponentBounds(miscFrameBounds, bypassButton.get());
    includeComponentBounds(miscFrameBounds, bypassWithGainButton.get());
    placeSectionFrame(miscSectionFrame.get(), miscExpanded, miscFrameBounds, content.getLocalBounds());

    juce::Rectangle<int> stereoFrameBounds;
    includeComponentBounds(stereoFrameBounds, stereoHeader.get());
    includeComponentBounds(stereoFrameBounds, transOnButton.get());
    includeComponentBounds(stereoFrameBounds, sustainOnButton.get());
    includeComponentBounds(stereoFrameBounds, transGainControl.get());
    includeComponentBounds(stereoFrameBounds, sustainGainControl.get());
    includeComponentBounds(stereoFrameBounds, holdControl.get());
    includeComponentBounds(stereoFrameBounds, holdModeButton.get());
    includeComponentBounds(stereoFrameBounds, releaseControl.get());
    includeComponentBounds(stereoFrameBounds, releaseModeButton.get());
    includeComponentBounds(stereoFrameBounds, releaseCurveControl.get());
    includeComponentBounds(stereoFrameBounds, thresholdControl.get());
    includeComponentBounds(stereoFrameBounds, kneeControl.get());
    includeComponentBounds(stereoFrameBounds, retriggerControl.get());
    includeComponentBounds(stereoFrameBounds, lookaheadControl.get());
    includeComponentBounds(stereoFrameBounds, outGainControl.get());
    placeSectionFrame(stereoSectionFrame.get(), stereoExpanded, stereoFrameBounds, content.getLocalBounds());
}

void TseModuleComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! viewport.getBounds().contains(event.getPosition()))
        return;

    scrollViewport(wheel);
}

juce::Rectangle<int> TseModuleComponent::getContentBounds() const noexcept
{
    auto bounds = getLocalBounds();
    const auto editorInsetX = getEditorInsetX(bounds.getWidth());
    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);
    return bounds;
}

int TseModuleComponent::getPreferredContentHeight() const noexcept
{
    auto rowCount = 2;

    if (miscExpanded)
        rowCount += 3;

    if (stereoExpanded)
        rowCount += 12;

    return (rowHeight * rowCount) + (verticalGap * (rowCount - 1));
}

void TseModuleComponent::scrollViewport(const juce::MouseWheelDetails& wheel)
{
    const auto maxScrollY = juce::jmax(0, content.getHeight() - viewport.getHeight());

    if (maxScrollY <= 0)
        return;

    const auto dominantDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY
                                                                                : -wheel.deltaX;

    if (std::abs(dominantDelta) < 1.0e-6f)
        return;

    auto pixelDelta = juce::roundToInt(-dominantDelta * (wheel.isSmooth ? 180.0f : 90.0f));

    if (pixelDelta == 0)
        pixelDelta = dominantDelta < 0.0f ? 24 : -24;

    viewport.setViewPosition(0, juce::jlimit(0, maxScrollY, viewport.getViewPositionY() + pixelDelta));
}

void TseModuleComponent::handleSectionClick(const Section section)
{
    if (section == Section::misc)
    {
        miscExpanded = ! miscExpanded;

        if (miscExpanded)
            stereoExpanded = false;
    }
    else
    {
        stereoExpanded = ! stereoExpanded;

        if (stereoExpanded)
            miscExpanded = false;
    }

    persistSectionExpandedState(miscExpandedStateKey, miscExpanded);
    persistSectionExpandedState(stereoExpandedStateKey, stereoExpanded);
    updateSectionVisibility();
    resized();
}

void TseModuleComponent::updateSectionVisibility()
{
    if (miscHeader != nullptr)
        miscHeader->setToggleState(miscExpanded, juce::dontSendNotification);

    if (stereoHeader != nullptr)
        stereoHeader->setToggleState(stereoExpanded, juce::dontSendNotification);

    if (miscSectionFrame != nullptr) miscSectionFrame->setVisible(miscExpanded);
    if (stereoSectionFrame != nullptr) stereoSectionFrame->setVisible(stereoExpanded);
    if (closeModuleButton != nullptr) closeModuleButton->setVisible(miscExpanded);
    if (bypassButton != nullptr) bypassButton->setVisible(miscExpanded);
    if (bypassWithGainButton != nullptr) bypassWithGainButton->setVisible(miscExpanded);
    if (transOnButton != nullptr) transOnButton->setVisible(stereoExpanded);
    if (sustainOnButton != nullptr) sustainOnButton->setVisible(stereoExpanded);
    if (transGainControl != nullptr) transGainControl->setVisible(stereoExpanded);
    if (sustainGainControl != nullptr) sustainGainControl->setVisible(stereoExpanded);
    if (holdControl != nullptr) holdControl->setVisible(stereoExpanded);
    if (holdModeButton != nullptr) holdModeButton->setVisible(stereoExpanded);
    if (releaseControl != nullptr) releaseControl->setVisible(stereoExpanded);
    if (releaseModeButton != nullptr) releaseModeButton->setVisible(stereoExpanded);
    if (releaseCurveControl != nullptr) releaseCurveControl->setVisible(stereoExpanded);
    if (thresholdControl != nullptr) thresholdControl->setVisible(stereoExpanded);
    if (kneeControl != nullptr) kneeControl->setVisible(stereoExpanded);
    if (retriggerControl != nullptr) retriggerControl->setVisible(stereoExpanded);
    if (lookaheadControl != nullptr) lookaheadControl->setVisible(stereoExpanded);
    if (outGainControl != nullptr) outGainControl->setVisible(stereoExpanded);
}

bool TseModuleComponent::loadSectionExpandedState(const char* stateKey) const
{
    return static_cast<bool>(valueTreeState.state.getProperty(juce::Identifier { stateKey }, false));
}

void TseModuleComponent::persistSectionExpandedState(const char* stateKey, const bool expanded)
{
    valueTreeState.state.setProperty(juce::Identifier { stateKey }, expanded, nullptr);
}

void TseModuleComponent::updateBranchButtonLabels()
{
    if (transOnButton != nullptr)
        transOnButton->setButtonText(transOnButton->getToggleState() ? "TRANS.OFF" : "TRANS.ON");

    if (sustainOnButton != nullptr)
        sustainOnButton->setButtonText(sustainOnButton->getToggleState() ? "SUS.OFF" : "SUS.ON");
}

void TseModuleComponent::updateTimeModeControls()
{
    const auto updateControl = [this] (const TimeTarget target, ParameterControl* control, BoxTextButton* button)
    {
        if (control == nullptr || button == nullptr)
            return;

        const auto hostSync = isHostSyncMode(target);
        button->setButtonText(hostSync ? "T" : "M");
        button->setAlwaysAccentOutline(hostSync);

        if (hostSync)
        {
            control->setOverrideText(getSyncChoiceText(target));
            control->setInteractionEnabled(false);
            control->setValueClickAction([this, target]
            {
                showTimeSyncPrompt(target);
            });
        }
        else
        {
            control->clearOverrideText();
            control->setValueClickAction(nullptr);
            control->setInteractionEnabled(true);
        }
    };

    updateControl(TimeTarget::hold, holdControl.get(), holdModeButton.get());
    updateControl(TimeTarget::release, releaseControl.get(), releaseModeButton.get());
}

void TseModuleComponent::handleTimeModeButton(const TimeTarget target)
{
    const auto nextHostMode = ! isHostSyncMode(target);
    setParameterPlainValue(getModeParameterId(target), nextHostMode ? 1.0f : 0.0f);
    updateTimeModeControls();
}

void TseModuleComponent::showTimeSyncPrompt(const TimeTarget target)
{
    if (callbacks.showChoicePrompt == nullptr)
        return;

    const auto choices = TseModuleProcessor::getHostSyncChoices();
    std::vector<bool> itemEnabledStates(static_cast<size_t>(choices.size()), true);
    const auto selectedIndex = getSyncChoiceIndex(target);
    auto* anchor = target == TimeTarget::hold ? holdControl.get() : releaseControl.get();

    if (anchor == nullptr)
        return;

    callbacks.showChoicePrompt(getLocalArea(anchor, anchor->getValueBounds()),
                               choices,
                               selectedIndex,
                               std::move(itemEnabledStates),
                               juce::Justification::centred,
                               [this, target] (const int choiceIndex)
                               {
                                   setParameterPlainValue(getSyncParameterId(target), static_cast<float>(choiceIndex));
                                   updateTimeModeControls();
                                   clearFocus();
                               },
                               {},
                               [this]
                               {
                                   clearFocus();
                               });
}

void TseModuleComponent::setParameterPlainValue(const juce::String& parameterId, const float plainValue)
{
    if (auto* parameter = valueTreeState.getParameter(parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}

bool TseModuleComponent::isHostSyncMode(const TimeTarget target) const noexcept
{
    return readRawParameter(valueTreeState, getModeParameterId(target), 0.0f) >= 0.5f;
}

int TseModuleComponent::getSyncChoiceIndex(const TimeTarget target) const noexcept
{
    const auto choices = TseModuleProcessor::getHostSyncChoices();
    const auto fallback = static_cast<float>(TseModuleProcessor::getDefaultHostSyncChoiceIndex());
    const auto rawValue = readRawParameter(valueTreeState, getSyncParameterId(target), fallback);
    return juce::jlimit(0, choices.size() - 1, static_cast<int>(std::round(rawValue)));
}

juce::String TseModuleComponent::getSyncChoiceText(const TimeTarget target) const
{
    const auto choices = TseModuleProcessor::getHostSyncChoices();
    return choices[getSyncChoiceIndex(target)];
}

const char* TseModuleComponent::getModeParameterId(const TimeTarget target) noexcept
{
    return target == TimeTarget::hold ? TseModuleProcessor::paramTimeHoldModeId
                                      : TseModuleProcessor::paramTimeReleaseModeId;
}

const char* TseModuleComponent::getSyncParameterId(const TimeTarget target) noexcept
{
    return target == TimeTarget::hold ? TseModuleProcessor::paramTimeHoldSyncId
                                      : TseModuleProcessor::paramTimeReleaseSyncId;
}

void TseModuleComponent::clearFocus()
{
    if (callbacks.clearKeyboardFocus != nullptr)
        callbacks.clearKeyboardFocus();
    else
        clearKeyboardFocus(*this);
}

void TseModuleComponent::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID != TseModuleProcessor::paramTimeHoldModeId
        && parameterID != TseModuleProcessor::paramTimeHoldSyncId
        && parameterID != TseModuleProcessor::paramTimeReleaseModeId
        && parameterID != TseModuleProcessor::paramTimeReleaseSyncId)
        return;

    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<TseModuleComponent>(this)]
    {
        if (safeThis != nullptr)
            safeThis->updateTimeModeControls();
    });
}
