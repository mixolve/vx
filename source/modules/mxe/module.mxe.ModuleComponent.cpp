#include "module.mxe.ModuleComponent.h"

#include "module.mxe.EditorControls.h"
#include "module.mxe.EditorPages.h"
#include "module.mxe.EditorTheme.h"
#include "module.mxe.EditorUiState.h"
#include "module.mxe.HostParameterEditing.h"
#include "module.mxe.ParameterIds.h"

#include <cmath>

namespace
{
using mxe::editor::BandPageComponent;
using mxe::editor::FullbandPageComponent;
using mxe::editor::bandColour;
using mxe::editor::buttonHeight;
using mxe::editor::makeBandName;
using mxe::editor::monitorButtonGap;
using mxe::editor::monitorRowOffsetY;
using mxe::parameters::makeActiveSplitCountParameterId;
using mxe::parameters::makeBandParameterId;
using mxe::parameters::makeFullbandParameterId;
using mxe::parameters::makeSoloParameterId;

} // namespace

MxeModuleComponent::MxeModuleComponent(MxeAudioProcessor& processorToEdit, Callbacks callbacksIn)
    : audioProcessor(processorToEdit),
      callbacks(std::move(callbacksIn))
{
    auto& processorState = processorToEdit.getValueTreeState();
    valueTreeState = &processorState;
    activeSplitCountParameter = dynamic_cast<juce::RangedAudioParameter*>(processorState.getParameter(makeActiveSplitCountParameterId()));
    jassert(activeSplitCountParameter != nullptr);

    loadUiState();

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processorState.getParameter(makeSoloParameterId(bandIndex)));
        jassert(parameter != nullptr);
        soloParameters[bandIndex] = parameter;

        auto button = std::make_unique<mxe::editor::BoxTextButton>(mxe::editor::uiAccent);
        button->setButtonText(makeBandName(bandIndex));
        button->setClickingTogglesState(false);
        button->onClick = [this, bandIndex] { selectBand(bandIndex); };
        addAndMakeVisible(*button);
        monitorButtons[bandIndex] = std::move(button);

        auto page = std::make_unique<BandPageComponent>(
            processorState,
            bandIndex,
            [bandIndex] (const char* suffix)
            {
                return makeBandParameterId(bandIndex, suffix);
            },
            bandColour(bandIndex),
            [this, bandIndex]
            {
                toggleManualSolo(bandIndex);
            },
            [this, bandIndex]
            {
                return manualSoloMask[bandIndex];
            },
            [this]
            {
                return ! autoSoloEnabled && getActiveBandCount() > 1;
            });
        bandPages[bandIndex] = std::move(page);
    }

    size_t activeSoloCount = 0;

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (soloParameters[bandIndex] != nullptr && soloParameters[bandIndex]->getValue() >= 0.5f)
        {
            if (! uiStateLoaded && activeSoloCount == 0)
                visibleBandIndex = bandIndex;

            ++activeSoloCount;

            if (! uiStateLoaded)
                manualSoloMask[bandIndex] = true;
        }
    }

    if (! uiStateLoaded)
        allBandsActive = activeSoloCount != 1;

    visibleBandIndex = juce::jmin(visibleBandIndex, getActiveBandCount() - 1);

    auto allButton = std::make_unique<mxe::editor::BoxTextButton>(mxe::editor::uiAccent);
    allButton->setButtonText("A");
    allButton->setClickingTogglesState(false);
    allButton->onClick = [this] { setAllBandsMonitoring(); };
    addAndMakeVisible(*allButton);
    monitorButtons[numBands] = std::move(allButton);

    allBandsPage = std::make_unique<FullbandPageComponent>(
        processorState,
        [] (const char* suffix)
        {
            return makeFullbandParameterId(suffix);
        },
        autoSoloEnabled,
        [this]
        {
            return getActiveSplitCount();
        },
        [this] (const int delta)
        {
            changeActiveSplitCount(delta);
        },
        [this] (const bool shouldBeEnabled)
        {
            setAutoSoloEnabled(shouldBeEnabled);
        },
        [this]
        {
            if (callbacks.requestCloseActiveModule != nullptr)
                callbacks.requestCloseActiveModule();
        });

    pageViewport.setInterceptsMouseClicks(false, true);
    pageViewport.setScrollBarsShown(false, false);
    pageViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    pageViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(pageViewport);

    updateMonitorButtons();
    updateBandPageVisibility();
}

MxeModuleComponent::~MxeModuleComponent() = default;

void MxeModuleComponent::showTextPrompt(const juce::String& currentText,
                                        std::function<bool(const juce::String&)> onCommit,
                                        std::function<void()> onClose,
                                        std::function<void()> onDismiss)
{
    if (callbacks.showTextPrompt != nullptr)
        callbacks.showTextPrompt(currentText, std::move(onCommit), std::move(onClose), std::move(onDismiss));
}

void MxeModuleComponent::loadUiState()
{
    if (valueTreeState == nullptr)
        return;

    auto& state = valueTreeState->state;
    autoSoloEnabled = mxe::editor::uiState::getBool(state, mxe::editor::uiState::autoSoloEnabledId(), true);
    allBandsActive = mxe::editor::uiState::getBool(state, mxe::editor::uiState::allBandsActiveId(), true);
    visibleBandIndex = static_cast<size_t>(juce::jlimit(0,
                                                       static_cast<int>(numBands - 1),
                                                       mxe::editor::uiState::getInt(state, mxe::editor::uiState::visibleBandIndexId(), 0)));

    manualSoloMask = {};

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        const auto bandVisible = mxe::editor::uiState::getBool(state,
                                                                mxe::editor::uiState::makeManualSoloId(bandIndex),
                                                                false);
        manualSoloMask[bandIndex] = bandVisible;
    }

    uiStateLoaded = mxe::editor::uiState::getBool(state, mxe::editor::uiState::hasUiStateId(), false);
}

void MxeModuleComponent::saveUiState()
{
    if (valueTreeState == nullptr)
        return;

    auto& state = valueTreeState->state;
    mxe::editor::uiState::setBool(state, mxe::editor::uiState::autoSoloEnabledId(), autoSoloEnabled);
    mxe::editor::uiState::setBool(state, mxe::editor::uiState::allBandsActiveId(), allBandsActive);
    mxe::editor::uiState::setInt(state, mxe::editor::uiState::visibleBandIndexId(), static_cast<int>(visibleBandIndex));
    mxe::editor::uiState::setBool(state, mxe::editor::uiState::hasUiStateId(), true);

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        state.setProperty(mxe::editor::uiState::makeManualSoloId(bandIndex), manualSoloMask[bandIndex], nullptr);
    }
}

void MxeModuleComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colours::black);
}

void MxeModuleComponent::resized()
{
    auto bounds = getContentBounds();

    auto monitorRow = bounds.removeFromTop(buttonHeight);
    const auto buttonCount = static_cast<int>(monitorButtons.size());
    const auto totalGapWidth = monitorButtonGap * (buttonCount - 1);
    const auto baseButtonWidth = (monitorRow.getWidth() - totalGapWidth) / buttonCount;
    auto remainder = (monitorRow.getWidth() - totalGapWidth) - (baseButtonWidth * buttonCount);

    for (auto& button : monitorButtons)
    {
        const auto buttonWidth = baseButtonWidth + (remainder > 0 ? 1 : 0);

        if (button != nullptr)
            button->setBounds(monitorRow.removeFromLeft(buttonWidth).translated(0, monitorRowOffsetY));
        else
            monitorRow.removeFromLeft(buttonWidth);

        monitorRow.removeFromLeft(monitorButtonGap);
        remainder = juce::jmax(0, remainder - 1);
    }

    if (! bounds.isEmpty())
        bounds.removeFromTop(mxe::editor::verticalGap);

    auto pageBounds = bounds;
    pageBounds.expand(mxe::editor::contentFrameInsetX, 0);
    pageViewport.setBounds(pageBounds);
    updatePageViewport();

    saveUiState();
}

void MxeModuleComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    scrollPageViewport(event, wheel);
}

void MxeModuleComponent::scrollPageViewport(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (pageViewport.getViewedComponent() == nullptr)
        return;

    const auto editorPosition = event.getEventRelativeTo(this).getPosition();

    if (! pageViewport.getBounds().contains(editorPosition))
        return;

    const auto viewedHeight = pageViewport.getViewedComponent()->getHeight();
    const auto maxScrollY = juce::jmax(0, viewedHeight - pageViewport.getHeight());

    if (maxScrollY <= 0)
        return;

    const auto dominantDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY
                                                                                : -wheel.deltaX;

    if (std::abs(dominantDelta) < 1.0e-6f)
        return;

    auto pixelDelta = juce::roundToInt(-dominantDelta * (wheel.isSmooth ? 180.0f : 90.0f));

    if (pixelDelta == 0)
        pixelDelta = dominantDelta < 0.0f ? 24 : -24;

    pageViewport.setViewPosition(0, juce::jlimit(0, maxScrollY, pageViewport.getViewPositionY() + pixelDelta));
}

void MxeModuleComponent::refreshCurrentPageLayout()
{
    updatePageViewport();
}

void MxeModuleComponent::selectBand(const size_t bandIndex)
{
    visibleBandIndex = juce::jmin(bandIndex, getActiveBandCount() - 1);
    allBandsActive = false;

    if (autoSoloEnabled)
        manualSoloMask = {};

    updateMonitorButtons();
    updateBandPageVisibility();
    syncMonitorParameters();
    saveUiState();
    resized();
}

void MxeModuleComponent::toggleManualSolo(const size_t bandIndex)
{
    if (autoSoloEnabled || getActiveBandCount() <= 1 || bandIndex >= getActiveBandCount())
        return;

    visibleBandIndex = juce::jmin(bandIndex, numBands - 1);
    allBandsActive = false;
    manualSoloMask[visibleBandIndex] = ! manualSoloMask[visibleBandIndex];
    updateMonitorButtons();
    updateBandPageVisibility();
    syncMonitorParameters();
    saveUiState();
}

void MxeModuleComponent::changeActiveSplitCount(const int delta)
{
    if (activeSplitCountParameter == nullptr)
        return;

    const auto currentValue = static_cast<int>(std::round(activeSplitCountParameter->convertFrom0to1(activeSplitCountParameter->getValue())));
    const auto newValue = juce::jlimit(0, static_cast<int>(numBands - 1), currentValue + delta);

    if (newValue == currentValue)
        return;

    mxe::editor::setNormalisedParameterValueForHost(*activeSplitCountParameter,
                                                    activeSplitCountParameter->convertTo0to1(static_cast<float>(newValue)),
                                                    &audioProcessor.getUndoManager());
    audioProcessor.markParametersDirty();

    visibleBandIndex = juce::jmin(visibleBandIndex, getActiveBandCount() - 1);
    manualSoloMask = {};

    if (allBandsPage != nullptr)
        allBandsPage->refreshExternalState();

    updateMonitorButtons();
    updateBandPageVisibility();
    syncMonitorParameters();
    saveUiState();
    resized();
}

void MxeModuleComponent::setAllBandsMonitoring()
{
    allBandsActive = true;
    updateMonitorButtons();
    updateBandPageVisibility();
    syncMonitorParameters();
    saveUiState();
}

void MxeModuleComponent::setAutoSoloEnabled(const bool shouldBeEnabled)
{
    autoSoloEnabled = shouldBeEnabled;
    manualSoloMask = {};
    updateMonitorButtons();
    syncMonitorParameters();
    saveUiState();
}

void MxeModuleComponent::syncMonitorParameters()
{
    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto* parameter = soloParameters[bandIndex];

        if (parameter == nullptr)
            continue;

        const auto enabled = bandIndex < getActiveBandCount()
            && ((autoSoloEnabled && ! allBandsActive && bandIndex == visibleBandIndex)
                || (! autoSoloEnabled && manualSoloMask[bandIndex]));
        const auto newValue = parameter->convertTo0to1(enabled ? 1.0f : 0.0f);

        mxe::editor::setNormalisedParameterValueForHost(*parameter, newValue, &audioProcessor.getUndoManager());
    }

    audioProcessor.markParametersDirty();
}

void MxeModuleComponent::updateMonitorButtons()
{
    const auto activeBandCount = getActiveBandCount();

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (auto* button = monitorButtons[bandIndex].get())
        {
            const auto isActiveBand = bandIndex < activeBandCount;
            button->setVisible(true);
            button->setEnabled(isActiveBand);
            button->setAlpha(isActiveBand ? 1.0f : 0.45f);
            button->setToggleState(! allBandsActive && bandIndex == visibleBandIndex, juce::dontSendNotification);
        }
    }

    if (auto* button = monitorButtons[numBands].get())
    {
        button->setVisible(true);
        button->setToggleState(allBandsActive, juce::dontSendNotification);
    }

    for (auto& page : bandPages)
    {
        if (page != nullptr)
            page->refreshExternalState();
    }
}

void MxeModuleComponent::updateBandPageVisibility()
{
    const auto activeBandCount = getActiveBandCount();

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (auto* page = bandPages[bandIndex].get())
            page->setVisible(! allBandsActive && bandIndex < activeBandCount && bandIndex == visibleBandIndex);
    }

    if (allBandsPage != nullptr)
        allBandsPage->setVisible(allBandsActive);

    updatePageViewport();
}

juce::Rectangle<int> MxeModuleComponent::getTextPromptAnchorBounds() const noexcept
{
    auto bounds = getContentBounds();
    return bounds.removeFromTop(buttonHeight).translated(0, monitorRowOffsetY);
}

juce::Rectangle<int> MxeModuleComponent::getContentBounds() const noexcept
{
    auto bounds = getLocalBounds();
    const auto editorInsetX = mxe::editor::getNestedEditorInsetX(bounds.getWidth());

    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);
    return bounds;
}

juce::Component* MxeModuleComponent::getCurrentPageComponent() const noexcept
{
    if (allBandsActive)
        return allBandsPage.get();

    return visibleBandIndex < bandPages.size() ? bandPages[visibleBandIndex].get()
                                               : nullptr;
}

int MxeModuleComponent::getCurrentPagePreferredHeight() const noexcept
{
    if (allBandsActive)
        return allBandsPage != nullptr ? allBandsPage->getPreferredHeight() : 0;

    return visibleBandIndex < bandPages.size() && bandPages[visibleBandIndex] != nullptr
        ? bandPages[visibleBandIndex]->getPreferredHeight()
        : 0;
}

void MxeModuleComponent::updatePageViewport()
{
    auto* currentPage = getCurrentPageComponent();

    if (currentPage == nullptr)
    {
        return;
    }

    const auto viewportBounds = pageViewport.getLocalBounds();

    if (viewportBounds.isEmpty())
    {
        return;
    }

    const auto preserveScroll = pageViewport.getViewedComponent() == currentPage;
    const auto previousScrollY = preserveScroll ? pageViewport.getViewPositionY() : 0;

    if (! preserveScroll)
        pageViewport.setViewedComponent(currentPage, false);

    const auto pageHeight = juce::jmax(viewportBounds.getHeight(), getCurrentPagePreferredHeight());
    currentPage->setSize(viewportBounds.getWidth(), pageHeight);

    const auto maxScrollY = juce::jmax(0, pageHeight - viewportBounds.getHeight());
    pageViewport.setViewPosition(0, juce::jlimit(0, maxScrollY, previousScrollY));
}

size_t MxeModuleComponent::getActiveSplitCount() const
{
    if (activeSplitCountParameter == nullptr)
        return numBands - 1;

    return static_cast<size_t>(juce::jlimit(0,
                                           static_cast<int>(numBands - 1),
                                           static_cast<int>(std::round(activeSplitCountParameter->convertFrom0to1(activeSplitCountParameter->getValue())))));
}

size_t MxeModuleComponent::getActiveBandCount() const
{
    return getActiveSplitCount() + 1;
}
