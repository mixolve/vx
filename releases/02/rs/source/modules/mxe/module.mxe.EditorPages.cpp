#include "module.mxe.EditorPages.h"

#include "module.mxe.EditorUiState.h"
#include "module.mxe.ModuleComponent.h"
#include "module.mxe.ParameterIds.h"

namespace mxe::editor
{
namespace
{
inline constexpr auto bandMiscControls = std::to_array<ControlSpec>({
    { "delTa", "DELTA", true, false },
    { "modBypass", "BYPASS", true, false },
    { "modBypassWithGain", "BYPASS.WT-GAIN", true, false },
    { "inGn", "IN-GAIN-LR" },
    { "inLeft", "IN-GAIN-L" },
    { "inRight", "IN-GAIN-R" },
    { "wide", "WIDE" },
    { "outGn", "OUT-GAIN" },
});

inline constexpr auto bandMainControls = std::to_array<ControlSpec>({
    { "moRph", "MORPH" },
    { "peakHoldHz", "PEAK-HOLD" },
    { "TensionFlooR", "TEN-FLOOR" },
    { "TensionHysT", "TEN-HYST" },
});

inline constexpr auto fullbandMiscControls = std::to_array<ControlSpec>({
    { "modBypass", "BYPASS", true, false },
    { "modBypassWithGain", "BYPASS.WT-GAIN", true, false },
    { "inGnVisible", "IN-GAIN-LR" },
    { "autoInLeft", "IN-GAIN-L" },
    { "autoInRight", "IN-GAIN-R" },
    { "wideVisible", "WIDE" },
    { "outGnVisible", "OUT-GAIN" },
});

inline const auto bandMiscSection = SectionSpec { "MISC", bandMiscControls, false, false };
inline const auto bandMainSection = SectionSpec { "MAIN", bandMainControls, false, false };
inline const auto fullbandMiscSection = SectionSpec { "MISC", fullbandMiscControls, false, false };
}

BandPageComponent::BandPageComponent(juce::AudioProcessorValueTreeState& state,
                                     const size_t bandIndexIn,
                                     const std::function<juce::String(const char*)>& parameterIdProvider,
                                     const juce::Colour accent,
                                     std::function<void()> onSoloClickIn,
                                     std::function<bool()> isSoloActiveIn,
                                     std::function<bool()> isSoloEnabledIn)
    : valueTreeState(&state),
      bandIndex(bandIndexIn),
      onSoloClick(std::move(onSoloClickIn)),
      isSoloActive(std::move(isSoloActiveIn)),
      isSoloEnabled(std::move(isSoloEnabledIn)),
      misc(state,
           [parameterIdProvider] (const char* suffix)
           {
               const auto suffixString = juce::String(suffix);

               if (suffixString == "modBypass")
                   return mxe::parameters::makeModuleBypassParameterId();

               if (suffixString == "modBypassWithGain")
                   return mxe::parameters::makeModuleBypassWithGainParameterId();

               return parameterIdProvider(suffix);
           },
           bandMiscSection,
           accent,
           [this] { refreshLayout(); },
           [this] (bool expanded) { handleSectionExpanded(0, expanded); },
           {},
           [this] (const char* suffix)
           {
               if (juce::String(suffix) == "delTa" && onSoloClick)
                   onSoloClick();
           },
           [this] (const char* suffix)
           {
               return juce::String(suffix) == "delTa" && isSoloActive != nullptr && isSoloActive();
           },
            [this] (const char* suffix)
            {
                return juce::String(suffix) == "delTa" && (isSoloEnabled == nullptr || isSoloEnabled());
            },
            {}, {}, {},
            valueTreeState,
            mxe::editor::uiState::makeBandSectionExpandedStateKey(bandIndex, 0)),
      main(state,
           parameterIdProvider,
           bandMainSection,
           accent,
           [this] { refreshLayout(); },
           [this] (bool expanded) { handleSectionExpanded(1, expanded); },
            {}, {}, {}, {}, {}, {}, {},
            valueTreeState,
            mxe::editor::uiState::makeBandSectionExpandedStateKey(bandIndex, 1)),
      halfWave(state,
                parameterIdProvider,
                halfWaveSection,
                accent,
                [this] { refreshLayout(); },
                [this] (bool expanded) { handleSectionExpanded(2, expanded); },
                {}, {}, {}, {}, {}, {}, {},
                valueTreeState,
                mxe::editor::uiState::makeBandSectionExpandedStateKey(bandIndex, 2)),
      dm(state,
         parameterIdProvider,
         dmSection,
         accent,
         [this] { refreshLayout(); },
         [this] (bool expanded) { handleSectionExpanded(3, expanded); },
         {}, {}, {}, {}, {}, {}, {},
         valueTreeState,
         mxe::editor::uiState::makeBandSectionExpandedStateKey(bandIndex, 3)),
      ff(state,
         parameterIdProvider,
         ffSection,
         accent,
         [this] { refreshLayout(); },
         [this] (bool expanded) { handleSectionExpanded(4, expanded); },
         {}, {}, {}, {}, {}, {}, {},
         valueTreeState,
         mxe::editor::uiState::makeBandSectionExpandedStateKey(bandIndex, 4))
{
    addAndMakeVisible(misc);
    addAndMakeVisible(main);
    addAndMakeVisible(halfWave);
    addAndMakeVisible(dm);
    addAndMakeVisible(ff);
}

void BandPageComponent::refreshExternalState()
{
    misc.refreshExternalState();
    main.refreshExternalState();
}

int BandPageComponent::getPreferredHeight() const
{
    return misc.getPreferredHeight()
        + sectionComponentGap
        + main.getPreferredHeight()
        + sectionComponentGap
        + halfWave.getPreferredHeight()
        + sectionComponentGap
        + dm.getPreferredHeight()
        + sectionComponentGap
        + ff.getPreferredHeight();
}

void BandPageComponent::resized()
{
    auto bounds = getLocalBounds();

    auto sectionBounds = bounds.removeFromTop(misc.getPreferredHeight());
    misc.setBounds(sectionBounds);
    bounds.removeFromTop(sectionComponentGap);
    sectionBounds = bounds.removeFromTop(main.getPreferredHeight());
    main.setBounds(sectionBounds);
    bounds.removeFromTop(sectionComponentGap);
    sectionBounds = bounds.removeFromTop(halfWave.getPreferredHeight());
    halfWave.setBounds(sectionBounds);
    bounds.removeFromTop(sectionComponentGap);
    sectionBounds = bounds.removeFromTop(dm.getPreferredHeight());
    dm.setBounds(sectionBounds);
    bounds.removeFromTop(sectionComponentGap);
    sectionBounds = bounds.removeFromTop(ff.getPreferredHeight());
    ff.setBounds(sectionBounds);
}

void BandPageComponent::handleSectionExpanded(const int sectionIndex, const bool expanded)
{
    if (! expanded)
        return;

    if (sectionIndex != 0)
        misc.setExpanded(false);

    if (sectionIndex != 1)
        main.setExpanded(false);

    if (sectionIndex != 2)
        halfWave.setExpanded(false);

    if (sectionIndex != 3)
        dm.setExpanded(false);

    if (sectionIndex != 4)
        ff.setExpanded(false);

    resized();
}

void BandPageComponent::refreshLayout()
{
    const auto preferredHeight = getPreferredHeight();

    if (getHeight() != preferredHeight)
        setSize(getWidth(), preferredHeight);
    else
        resized();

    if (auto* owner = findParentComponentOfClass<MxeModuleComponent>())
        owner->refreshCurrentPageLayout();
}

FullbandPageComponent::FullbandPageComponent(juce::AudioProcessorValueTreeState& state,
                                             const std::function<juce::String(const char*)>& parameterIdProvider,
                                             const bool autoSoloEnabled,
                                             std::function<size_t()> activeSplitCountProviderIn,
                                             std::function<void(int)> onActiveSplitCountChangeIn,
                                             std::function<void(bool)> onAutoSoloChangedIn,
                                             std::function<void()> onCloseModuleIn)
    : valueTreeState(&state),
      activeSplitCountProvider(std::move(activeSplitCountProviderIn)),
      misc(state,
           [parameterIdProvider] (const char* suffix)
           {
               const auto suffixString = juce::String(suffix);

               if (suffixString == "modBypass")
                   return mxe::parameters::makeModuleBypassParameterId();

               if (suffixString == "modBypassWithGain")
                   return mxe::parameters::makeModuleBypassWithGainParameterId();

               return parameterIdProvider(suffix);
           },
           fullbandMiscSection,
           uiAccent,
           [this] { refreshLayout(); },
           [this] (const bool expanded)
           {
                if (expanded)
                    crossover.setExpanded(false);
            },
            {}, {}, {}, {},
            []
            {
                return static_cast<size_t>(2);
            },
            {}, {},
           valueTreeState,
           mxe::editor::uiState::makeFullbandSectionExpandedStateKey(0)),
      crossover(state,
                parameterIdProvider,
                crossoverSection,
                uiAccent,
                [this] { refreshLayout(); },
                 [this] (const bool expanded)
                 {
                     if (expanded)
                         misc.setExpanded(false);
                 },
                [&state, parameterIdProvider] (const char* suffix)
                {
                    const auto suffixString = juce::String(suffix);
                    auto parameterIndex = static_cast<size_t>(0);
                    auto found = false;

                    for (size_t index = 0; index < crossoverControls.size(); ++index)
                    {
                        if (suffixString == crossoverControls[index].suffix)
                        {
                            parameterIndex = index;
                            found = true;
                            break;
                        }
                    }

                    if (! found)
                        return ValueConstraint {};

                    return ValueConstraint { [&state, parameterIdProvider, parameterIndex] (const float value)
                    {
                        auto lowerBound = 20.0f;
                        auto upperBound = 20000.0f;

                        if (parameterIndex > 0)
                        {
                            if (auto* previousParameter = dynamic_cast<juce::RangedAudioParameter*>(
                                    state.getParameter(parameterIdProvider(crossoverControls[parameterIndex - 1].suffix))))
                            {
                                lowerBound = previousParameter->convertFrom0to1(previousParameter->getValue()) + crossoverMinGapHz;
                            }
                        }

                        if (parameterIndex + 1 < crossoverControls.size())
                        {
                            if (auto* nextParameter = dynamic_cast<juce::RangedAudioParameter*>(
                                    state.getParameter(parameterIdProvider(crossoverControls[parameterIndex + 1].suffix))))
                            {
                                upperBound = nextParameter->convertFrom0to1(nextParameter->getValue()) - crossoverMinGapHz;
                            }
                        }

                        return juce::jlimit(lowerBound, juce::jmax(lowerBound, upperBound), value);
                    } };
                },
                {}, {}, {},
                []
                {
                    return static_cast<size_t>(1);
                },
                [this]
                {
                    return activeSplitCountProvider != nullptr
                        ? juce::jmin(crossoverControls.size(), activeSplitCountProvider())
                        : crossoverControls.size();
                },
                []
                {
                    return static_cast<size_t>(1);
                },
                valueTreeState,
                mxe::editor::uiState::makeFullbandSectionExpandedStateKey(1)),
      autoSoloButton(uiAccent),
      closeModuleButton(uiAccent),
      addCrossoverButton(uiAccent),
      removeCrossoverButton(uiAccent),
      onActiveSplitCountChange(std::move(onActiveSplitCountChangeIn)),
      onAutoSoloChanged(std::move(onAutoSoloChangedIn)),
      onCloseModule(std::move(onCloseModuleIn))
{
    const auto restoredAutoSoloEnabled = mxe::editor::uiState::getBool(state.state,
                                                                       mxe::editor::uiState::autoSoloEnabledId(),
                                                                       autoSoloEnabled);

    autoSoloButton.setButtonText("AUTO-SOLO");
    autoSoloButton.setClickingTogglesState(true);
    autoSoloButton.setToggleState(restoredAutoSoloEnabled, juce::dontSendNotification);
    autoSoloButton.onClick = [this]
    {
        if (! isAutoSoloAvailable())
        {
            refreshAutoSoloButtonState();
            return;
        }

        if (onAutoSoloChanged)
            onAutoSoloChanged(autoSoloButton.getToggleState());

        persistAutoSoloState();
    };
    misc.addAndMakeVisible(autoSoloButton);
    refreshAutoSoloButtonState();

    closeModuleButton.setButtonText("CLOSE-MODULE");
    closeModuleButton.setTextJustification(juce::Justification::centred);
    closeModuleButton.onClick = [] {};
    closeModuleButton.setLongPressAction([this]
    {
        if (onCloseModule)
            onCloseModule();
    }, 500, "SURE?");
    misc.addAndMakeVisible(closeModuleButton);

    addAndMakeVisible(misc);
    addAndMakeVisible(crossover);

    addCrossoverButton.setButtonText("XOV-ADD");
    addCrossoverButton.setClickingTogglesState(false);
    addCrossoverButton.onClick = [this]
    {
        if (onActiveSplitCountChange)
            onActiveSplitCountChange(1);
    };
    crossover.addAndMakeVisible(addCrossoverButton);

    removeCrossoverButton.setButtonText("XOV-DEL");
    removeCrossoverButton.setClickingTogglesState(false);
    removeCrossoverButton.onClick = [this]
    {
        if (onActiveSplitCountChange)
            onActiveSplitCountChange(-1);
    };
    crossover.addAndMakeVisible(removeCrossoverButton);
    refreshCrossoverButtonState();
}

bool FullbandPageComponent::isAutoSoloAvailable() const
{
    return activeSplitCountProvider == nullptr || activeSplitCountProvider() > 0;
}

void FullbandPageComponent::refreshAutoSoloButtonState()
{
    const auto available = isAutoSoloAvailable();
    autoSoloButton.setEnabled(available);
    autoSoloButton.setAlpha(available ? 1.0f : 0.45f);
}

void FullbandPageComponent::refreshCrossoverButtonState()
{
    const auto activeSplitCount = activeSplitCountProvider != nullptr
        ? activeSplitCountProvider()
        : crossoverControls.size();
    const auto canAdd = activeSplitCount < crossoverControls.size();
    const auto canRemove = activeSplitCount > 0;

    addCrossoverButton.setEnabled(canAdd);
    addCrossoverButton.setAlpha(canAdd ? 1.0f : 0.45f);
    removeCrossoverButton.setEnabled(canRemove);
    removeCrossoverButton.setAlpha(canRemove ? 1.0f : 0.45f);
}

void FullbandPageComponent::persistAutoSoloState()
{
    if (valueTreeState == nullptr)
        return;

    mxe::editor::uiState::setBool(valueTreeState->state,
                                  mxe::editor::uiState::autoSoloEnabledId(),
                                  autoSoloButton.getToggleState());
    mxe::editor::uiState::setBool(valueTreeState->state, mxe::editor::uiState::hasUiStateId(), true);
}

void FullbandPageComponent::refreshLayout()
{
    const auto preferredHeight = getPreferredHeight();

    if (getHeight() != preferredHeight)
        setSize(getWidth(), preferredHeight);
    else
        resized();

    if (auto* owner = findParentComponentOfClass<MxeModuleComponent>())
        owner->refreshCurrentPageLayout();
}

int FullbandPageComponent::getPreferredHeight() const
{
    auto height = misc.getPreferredHeight()
        + sectionComponentGap
        + crossover.getPreferredHeight();

    return height;
}

void FullbandPageComponent::refreshExternalState()
{
    misc.refreshExternalState();
    refreshAutoSoloButtonState();
    refreshCrossoverButtonState();
    crossover.refreshExternalState();
}

void FullbandPageComponent::resized()
{
    auto bounds = getLocalBounds();
    auto miscBounds = bounds.removeFromTop(misc.getPreferredHeight());
    misc.setBounds(miscBounds);
    auto autoSoloButtonBounds = misc.getExtraControlBounds(0);
    auto closeButtonBounds = misc.getExtraControlBounds(1);
    autoSoloButton.setVisible(! autoSoloButtonBounds.isEmpty());
    autoSoloButton.setBounds(autoSoloButtonBounds);
    closeModuleButton.setVisible(! closeButtonBounds.isEmpty());
    closeModuleButton.setBounds(closeButtonBounds);
    bounds.removeFromTop(sectionComponentGap);
    crossover.setBounds(bounds.removeFromTop(crossover.getPreferredHeight()));
    auto addButtonBounds = crossover.getExtraControlBounds(0, {});
    auto removeButtonBounds = crossover.getExtraControlBounds(0, { crossoverControls.size() });
    addCrossoverButton.setVisible(! addButtonBounds.isEmpty());
    removeCrossoverButton.setVisible(! removeButtonBounds.isEmpty());

    if (! addButtonBounds.isEmpty())
        addCrossoverButton.setBounds(addButtonBounds);
    else
        addCrossoverButton.setBounds({});

    if (! removeButtonBounds.isEmpty())
        removeCrossoverButton.setBounds(removeButtonBounds);
    else
        removeCrossoverButton.setBounds({});

    refreshCrossoverButtonState();
}
} // namespace mxe::editor
