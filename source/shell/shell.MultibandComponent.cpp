#include "shell.MultibandComponent.h"

#include "shell.EditorControls.h"
#include "shell.UiParameterControls.h"
#include "shell.UiStyle.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

namespace
{
enum class TimeTarget
{
    hold,
    release
};

inline constexpr size_t multibandCrossoverSlotCount = 5;

inline constexpr std::array<const char*, multibandCrossoverSlotCount> crossoverSuffixes {
    "xover1", "xover2", "xover3", "xover4", "xover5"
};

inline constexpr std::array<const char*, multibandCrossoverSlotCount> crossoverLabels {
    "XOVER-1", "XOVER-2", "XOVER-3", "XOVER-4", "XOVER-5"
};

std::unique_ptr<BoxTextButton> makeTextButton(const juce::String& text, const juce::Colour accent = uiAccent)
{
    auto button = std::make_unique<BoxTextButton>(accent);
    button->setButtonText(text);
    button->setTextJustification(juce::Justification::centred);
    button->setCancelClickOnLeave(true);
    return button;
}

std::unique_ptr<BoxTextButton> makeTimeModeButton()
{
    return makeTextButton("M", uiGrey500);
}

juce::String makeStatePropertyName(const juce::String& moduleKey, const juce::String& property)
{
    return "shell.multiband." + moduleKey + "." + property;
}

juce::Identifier makeStatePropertyId(const juce::String& moduleKey, const juce::String& property)
{
    return juce::Identifier { makeStatePropertyName(moduleKey, property) };
}

bool getBool(const juce::ValueTree& state,
             const juce::String& moduleKey,
             const juce::String& property,
             const bool defaultValue)
{
    const auto id = makeStatePropertyId(moduleKey, property);
    return state.hasProperty(id) ? static_cast<bool>(state.getProperty(id)) : defaultValue;
}

int getInt(const juce::ValueTree& state,
           const juce::String& moduleKey,
           const juce::String& property,
           const int defaultValue)
{
    const auto id = makeStatePropertyId(moduleKey, property);
    return state.hasProperty(id) ? static_cast<int>(state.getProperty(id)) : defaultValue;
}

void setBool(juce::ValueTree& state,
             const juce::String& moduleKey,
             const juce::String& property,
             const bool value)
{
    state.setProperty(makeStatePropertyId(moduleKey, property), value, nullptr);
}

void setInt(juce::ValueTree& state,
            const juce::String& moduleKey,
            const juce::String& property,
            const int value)
{
    state.setProperty(makeStatePropertyId(moduleKey, property), value, nullptr);
}

float readRawParameter(juce::AudioProcessorValueTreeState& state,
                       const juce::String& parameterId,
                       const float fallback) noexcept
{
    if (auto* value = state.getRawParameterValue(parameterId))
        return value->load(std::memory_order_relaxed);

    return fallback;
}
} // namespace

class MultibandModuleComponent::BandPageComponent final : public juce::Component,
                                                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    BandPageComponent(MultibandModuleComponent& ownerIn,
                      const size_t bandIndexIn,
                      juce::Colour accent)
        : owner(ownerIn),
          bandIndex(bandIndexIn),
          accentColour(accent),
          soloButton(accent)
    {
        soloButton.setButtonText("SOLO");
        soloButton.setTextJustification(juce::Justification::centred);
        soloButton.setClickingTogglesState(false);
        soloButton.onClick = [this]
        {
            owner.toggleManualSolo(bandIndex);
            refreshSoloButtonState();
        };
        addAndMakeVisible(soloButton);

        addControlSpecs(owner.config.bandControls);
        addControlSpecs(owner.config.bandTailControls);
        refreshSoloButtonState();
        updateTimeModeControls();
    }

    ~BandPageComponent() override
    {
        for (const auto& parameterId : listenedParameterIds)
            owner.valueTreeState.removeParameterListener(parameterId, this);
    }

    void refreshExternalState()
    {
        refreshSoloButtonState();
        updateToggleLabels();
        updateTimeModeControls();
    }

    int getPreferredHeight() const
    {
        auto height = rowHeight;

        for (const auto& row : rows)
            height += row->getTopGap() + row->getPreferredHeight();

        return height + moduleContentBottomGap;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        soloButton.setBounds(bounds.removeFromTop(rowHeight));

        for (auto& row : rows)
        {
            if (! bounds.isEmpty())
                bounds.removeFromTop(juce::jmin(row->getTopGap(), bounds.getHeight()));

            row->setBounds(bounds.removeFromTop(row->getPreferredHeight()));
        }
    }

private:
    struct RowBase : public juce::Component
    {
        virtual int getPreferredHeight() const = 0;
        virtual void refreshExternalState() {}
        int getTopGap() const noexcept { return juce::jmax(0, topGapMultiplier) * verticalGap; }

        int topGapMultiplier = 1;
    };

    struct ParameterRow final : public RowBase
    {
        ParameterRow(MultibandModuleComponent& ownerIn,
                     juce::String parameterId,
                     const BandControlSpec& spec)
            : owner(ownerIn),
              parameterIdToReset(std::move(parameterId))
        {
            control = std::make_unique<ParameterControl>(
                owner.valueTreeState,
                parameterIdToReset,
                spec.label,
                spec.decimals);
            topGapMultiplier = spec.topGapMultiplier;
            addAndMakeVisible(*control);
        }

        int getPreferredHeight() const override { return rowHeight; }

        void resized() override
        {
            if (control != nullptr)
                control->setBounds(getLocalBounds());
        }

        MultibandModuleComponent& owner;
        juce::String parameterIdToReset;
        std::unique_ptr<ParameterControl> control;
    };

    struct ToggleRow final : public RowBase
    {
        ToggleRow(BandPageComponent& pageIn,
                  MultibandModuleComponent& ownerIn,
                  juce::String parameterId,
                  const BandControlSpec& spec)
            : page(pageIn),
              owner(ownerIn),
              parameterIdToToggle(std::move(parameterId)),
              exclusiveGroup(spec.exclusiveGroup != nullptr ? spec.exclusiveGroup : ""),
              enabledLabel(spec.enabledLabel != nullptr && juce::String(spec.enabledLabel).isNotEmpty() ? spec.enabledLabel : spec.label),
              disabledLabel(spec.disabledLabel != nullptr && juce::String(spec.disabledLabel).isNotEmpty() ? spec.disabledLabel : spec.label)
        {
            button = makeTextButton(spec.label);
            topGapMultiplier = spec.topGapMultiplier;
            button->setClickingTogglesState(true);
            attachment = std::make_unique<ButtonAttachment>(owner.valueTreeState, parameterIdToToggle, *button);
            button->onStateChange = [this] { updateLabel(); };
            button->onClick = [this]
            {
                if (button != nullptr && button->getToggleState() && exclusiveGroup.isNotEmpty())
                    page.clearExclusiveToggleGroup(parameterIdToToggle, exclusiveGroup);

                updateLabel();
                owner.clearFocus();
            };
            addAndMakeVisible(*button);
            updateLabel();
        }

        int getPreferredHeight() const override { return rowHeight; }

        void refreshExternalState() override
        {
            updateLabel();
        }

        void resized() override
        {
            if (button != nullptr)
                button->setBounds(getLocalBounds());
        }

        void updateLabel()
        {
            if (button != nullptr)
                button->setButtonText(button->getToggleState() ? disabledLabel : enabledLabel);
        }

        BandPageComponent& page;
        MultibandModuleComponent& owner;
        juce::String parameterIdToToggle;
        juce::String exclusiveGroup;
        juce::String enabledLabel;
        juce::String disabledLabel;
        std::unique_ptr<BoxTextButton> button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct TimeRow final : public RowBase
    {
        TimeRow(MultibandModuleComponent& ownerIn,
                juce::String valueParameterId,
                juce::String modeParameterId,
                juce::String syncParameterId,
                const BandControlSpec& spec)
            : owner(ownerIn),
              valueParameterIdToEdit(std::move(valueParameterId)),
              modeParameterIdToEdit(std::move(modeParameterId)),
              syncParameterIdToEdit(std::move(syncParameterId))
        {
            control = std::make_unique<ParameterControl>(
                owner.valueTreeState,
                valueParameterIdToEdit,
                spec.label,
                spec.decimals);
            topGapMultiplier = spec.topGapMultiplier;
            addAndMakeVisible(*control);

            modeButton = makeTimeModeButton();
            modeButton->onClick = [this]
            {
                owner.setParameterPlainValue(modeParameterIdToEdit, isHostSyncMode() ? 0.0f : 1.0f);
                updateModeControl();
                owner.clearFocus();
            };
            addAndMakeVisible(*modeButton);
            updateModeControl();
        }

        int getPreferredHeight() const override { return rowHeight; }

        void refreshExternalState() override
        {
            updateModeControl();
        }

        void resized() override
        {
            auto rowBounds = getLocalBounds();
            const auto leftWidth = getScaledParameterNameWidth(rowBounds.getWidth());
            const auto rightWidth = juce::jmax(0, rowBounds.getWidth() - leftWidth - parameterGap);
            auto titleBounds = rowBounds.removeFromLeft(leftWidth);
            rowBounds.removeFromLeft(juce::jmin(parameterGap, rowBounds.getWidth()));

            auto rightBounds = rowBounds.withWidth(rightWidth);
            auto buttonBounds = rightBounds.removeFromRight(rowHeight);

            if (! rightBounds.isEmpty())
                rightBounds.removeFromRight(juce::jmin(parameterGap, rightBounds.getWidth()));

            if (control != nullptr)
            {
                control->setTitleWidthOverride(titleBounds.getWidth());
                control->setBounds(titleBounds.withRight(rightBounds.getRight()));
            }

            if (modeButton != nullptr)
                modeButton->setBounds(buttonBounds);
        }

        bool isHostSyncMode() const noexcept
        {
            return readRawParameter(owner.valueTreeState, modeParameterIdToEdit, 0.0f) >= 0.5f;
        }

        int getSyncChoiceIndex() const noexcept
        {
            const auto choices = owner.config.getHostSyncChoices != nullptr
                ? owner.config.getHostSyncChoices()
                : juce::StringArray {};
            const auto fallback = owner.config.getDefaultHostSyncChoiceIndex != nullptr
                ? static_cast<float>(owner.config.getDefaultHostSyncChoiceIndex())
                : 0.0f;
            const auto rawValue = readRawParameter(owner.valueTreeState, syncParameterIdToEdit, fallback);
            return choices.isEmpty() ? 0
                                     : juce::jlimit(0, choices.size() - 1, static_cast<int>(std::round(rawValue)));
        }

        juce::String getSyncChoiceText() const
        {
            const auto choices = owner.config.getHostSyncChoices != nullptr
                ? owner.config.getHostSyncChoices()
                : juce::StringArray {};

            return choices.isEmpty() ? juce::String {} : choices[getSyncChoiceIndex()];
        }

        void showSyncPrompt()
        {
            if (owner.config.showChoicePrompt == nullptr || control == nullptr)
                return;

            const auto choices = owner.config.getHostSyncChoices != nullptr
                ? owner.config.getHostSyncChoices()
                : juce::StringArray {};

            if (choices.isEmpty())
                return;

            std::vector<bool> itemEnabledStates(static_cast<size_t>(choices.size()), true);
            owner.config.showChoicePrompt(owner.getLocalArea(control.get(), control->getValueBounds()),
                                          choices,
                                          getSyncChoiceIndex(),
                                          std::move(itemEnabledStates),
                                          juce::Justification::centred,
                                          [this] (const int choiceIndex)
                                          {
                                              owner.setParameterPlainValue(syncParameterIdToEdit, static_cast<float>(choiceIndex));
                                              updateModeControl();
                                              owner.clearFocus();
                                          },
                                          {},
                                          [this]
                                          {
                                              owner.clearFocus();
                                          });
        }

        void updateModeControl()
        {
            if (control == nullptr || modeButton == nullptr)
                return;

            const auto hostSync = isHostSyncMode();
            modeButton->setButtonText(hostSync ? "T" : "M");
            modeButton->setAlwaysAccentOutline(hostSync);

            if (hostSync)
            {
                control->setOverrideText(getSyncChoiceText());
                control->setInteractionEnabled(false);
                control->setValueClickAction([this] { showSyncPrompt(); });
            }
            else
            {
                control->clearOverrideText();
                control->setValueClickAction(nullptr);
                control->setInteractionEnabled(true);
            }
        }

        MultibandModuleComponent& owner;
        juce::String valueParameterIdToEdit;
        juce::String modeParameterIdToEdit;
        juce::String syncParameterIdToEdit;
        std::unique_ptr<ParameterControl> control;
        std::unique_ptr<BoxTextButton> modeButton;
    };

    juce::String getBandParameterId(const BandControlSpec& spec) const
    {
        const auto sourceBand = spec.sourceBandIndex >= 0
            ? static_cast<size_t>(juce::jlimit(0, static_cast<int>(numBands - 1), spec.sourceBandIndex))
            : bandIndex;
        return owner.config.makeBandParameterId(sourceBand, spec.suffix);
    }

    void addControlSpecs(const std::vector<BandControlSpec>& specs)
    {
        for (const auto& spec : specs)
        {
            if (spec.kind == ControlKind::toggle)
            {
                auto row = std::make_unique<ToggleRow>(*this, owner, getBandParameterId(spec), spec);
                addAndMakeVisible(*row);
                rows.push_back(std::move(row));
                continue;
            }

            if (spec.kind == ControlKind::time)
            {
                const auto valueId = getBandParameterId(spec);
                const auto modeId = owner.config.makeBandParameterId(bandIndex, spec.modeSuffix);
                const auto syncId = owner.config.makeBandParameterId(bandIndex, spec.syncSuffix);
                auto row = std::make_unique<TimeRow>(owner, valueId, modeId, syncId, spec);
                listenedParameterIds.push_back(modeId);
                listenedParameterIds.push_back(syncId);
                owner.valueTreeState.addParameterListener(modeId, this);
                owner.valueTreeState.addParameterListener(syncId, this);
                addAndMakeVisible(*row);
                rows.push_back(std::move(row));
                continue;
            }

            auto row = std::make_unique<ParameterRow>(owner, getBandParameterId(spec), spec);
            addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }
    }

    void clearExclusiveToggleGroup(const juce::String& activeParameterId, const juce::String& exclusiveGroup)
    {
        if (exclusiveGroup.isEmpty())
            return;

        for (auto& row : rows)
        {
            auto* toggleRow = dynamic_cast<ToggleRow*>(row.get());

            if (toggleRow == nullptr
                || toggleRow->exclusiveGroup != exclusiveGroup
                || toggleRow->parameterIdToToggle == activeParameterId)
            {
                continue;
            }

            owner.setParameterPlainValue(toggleRow->parameterIdToToggle, 0.0f);
        }
    }

    void refreshSoloButtonState()
    {
        const auto enabled = ! owner.autoSoloEnabled && owner.getActiveBandCount() > 1;
        soloButton.setEnabled(enabled);
        soloButton.setAlpha(enabled ? 1.0f : 0.45f);
        soloButton.setToggleState(enabled && owner.manualSoloMask[bandIndex], juce::dontSendNotification);
    }

    void updateToggleLabels()
    {
        for (auto& row : rows)
            row->refreshExternalState();
    }

    void updateTimeModeControls()
    {
        for (auto& row : rows)
            row->refreshExternalState();
    }

    void parameterChanged(const juce::String& parameterID, float) override
    {
        if (std::find(listenedParameterIds.begin(), listenedParameterIds.end(), parameterID) == listenedParameterIds.end())
            return;

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<BandPageComponent>(this)]
        {
            if (safeThis != nullptr)
                safeThis->updateTimeModeControls();
        });
    }

    MultibandModuleComponent& owner;
    size_t bandIndex = 0;
    juce::Colour accentColour;
    BoxTextButton soloButton;
    std::vector<std::unique_ptr<RowBase>> rows;
    std::vector<juce::String> listenedParameterIds;
};

class MultibandModuleComponent::FullbandPageComponent final : public juce::Component
{
public:
    explicit FullbandPageComponent(MultibandModuleComponent& ownerIn)
        : owner(ownerIn)
    {
        autoSoloButton = makeTextButton("AUTO-SOLO");
        autoSoloButton->setClickingTogglesState(true);
        autoSoloButton->setToggleState(owner.autoSoloEnabled, juce::dontSendNotification);
        autoSoloButton->onClick = [this]
        {
            if (! isAutoSoloAvailable())
            {
                refreshAutoSoloButtonState();
                return;
            }

            owner.setAutoSoloEnabled(autoSoloButton->getToggleState());
            owner.clearFocus();
        };
        addAndMakeVisible(*autoSoloButton);

        addCrossoverButton = makeTextButton("XOV-ADD");
        addCrossoverButton->onClick = [this]
        {
            owner.changeActiveSplitCount(1);
            owner.clearFocus();
        };
        addAndMakeVisible(*addCrossoverButton);

        removeCrossoverButton = makeTextButton("XOV-DEL");
        removeCrossoverButton->onClick = [this]
        {
            owner.changeActiveSplitCount(-1);
            owner.clearFocus();
        };
        addAndMakeVisible(*removeCrossoverButton);

        for (size_t index = 0; index < crossoverControls.size(); ++index)
        {
            auto control = std::make_unique<ParameterControl>(
                owner.valueTreeState,
                owner.config.makeFullbandParameterId(crossoverSuffixes[index]),
                crossoverLabels[index],
                0);
            addAndMakeVisible(*control);
            crossoverControls[index] = std::move(control);
        }

        refreshExternalState();
    }

    int getPreferredHeight() const
    {
        auto height = rowHeight;

        for (const auto& control : crossoverControls)
        {
            if (control != nullptr)
                height += verticalGap + control->getPreferredHeight();
        }

        height += verticalGap + rowHeight;

        if (owner.config.showAutoSolo)
            height += verticalGap + rowHeight;

        return height + moduleContentBottomGap;
    }

    void refreshExternalState()
    {
        const auto activeSplitCount = owner.getActiveSplitCount();
        const auto canAdd = activeSplitCount < crossoverControls.size();
        const auto canRemove = activeSplitCount > 0;

        if (addCrossoverButton != nullptr)
        {
            addCrossoverButton->setEnabled(canAdd);
            addCrossoverButton->setAlpha(canAdd ? 1.0f : 0.45f);
        }

        if (removeCrossoverButton != nullptr)
        {
            removeCrossoverButton->setEnabled(canRemove);
            removeCrossoverButton->setAlpha(canRemove ? 1.0f : 0.45f);
        }

        refreshAutoSoloButtonState();

        for (size_t index = 0; index < crossoverControls.size(); ++index)
        {
            if (auto* control = crossoverControls[index].get())
            {
                const auto enabled = index < activeSplitCount;
                control->setEnabled(enabled);
                control->setAlpha(enabled ? 1.0f : 0.45f);
                control->setInteractionEnabled(enabled);

                if (enabled)
                    control->clearOverrideText();
                else
                    control->setOverrideText("OFF");
            }
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto placeButton = [&bounds] (BoxTextButton* button)
        {
            if (button == nullptr)
                return;

            button->setBounds(bounds.removeFromTop(rowHeight));

            if (! bounds.isEmpty())
                bounds.removeFromTop(verticalGap);
        };

        auto placeControl = [&bounds] (ParameterControl* control)
        {
            if (control == nullptr)
                return;

            control->setBounds(bounds.removeFromTop(control->getPreferredHeight()));

            if (! bounds.isEmpty())
                bounds.removeFromTop(verticalGap);
        };

        placeButton(addCrossoverButton.get());

        for (auto& control : crossoverControls)
            placeControl(control.get());

        placeButton(removeCrossoverButton.get());

        if (owner.config.showAutoSolo)
            placeButton(autoSoloButton.get());
        else if (autoSoloButton != nullptr)
            autoSoloButton->setBounds({});

        refreshExternalState();
    }

private:
    bool isAutoSoloAvailable() const
    {
        return owner.getActiveSplitCount() > 0;
    }

    void refreshAutoSoloButtonState()
    {
        if (autoSoloButton == nullptr)
            return;

        autoSoloButton->setVisible(owner.config.showAutoSolo);
        autoSoloButton->setToggleState(owner.autoSoloEnabled, juce::dontSendNotification);

        const auto available = owner.config.showAutoSolo && isAutoSoloAvailable();
        autoSoloButton->setEnabled(available);
        autoSoloButton->setAlpha(available ? 1.0f : 0.45f);
    }

    MultibandModuleComponent& owner;
    std::unique_ptr<BoxTextButton> autoSoloButton;
    std::unique_ptr<BoxTextButton> addCrossoverButton;
    std::unique_ptr<BoxTextButton> removeCrossoverButton;
    std::array<std::unique_ptr<ParameterControl>, numCrossoverSlots> crossoverControls;
};

MultibandModuleComponent::MultibandModuleComponent(Config configIn)
    : config(std::move(configIn)),
      valueTreeState(*config.valueTreeState)
{
    jassert(config.valueTreeState != nullptr);
    jassert(config.makeBandParameterId != nullptr);
    jassert(config.makeFullbandParameterId != nullptr);
    jassert(config.makeSoloParameterId != nullptr);
    jassert(config.makeActiveSplitCountParameterId != nullptr);

    activeSplitCountParameter = dynamic_cast<juce::RangedAudioParameter*>(
        valueTreeState.getParameter(config.makeActiveSplitCountParameterId()));
    jassert(activeSplitCountParameter != nullptr);

    loadUiState();

    size_t activeSoloCount = 0;

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(
            valueTreeState.getParameter(config.makeSoloParameterId(bandIndex)));
        jassert(parameter != nullptr);
        soloParameters[bandIndex] = parameter;

        if (parameter != nullptr && parameter->getValue() >= 0.5f)
        {
            if (! uiStateLoaded && activeSoloCount == 0)
                visibleBandIndex = bandIndex;

            ++activeSoloCount;

            if (! uiStateLoaded)
                manualSoloMask[bandIndex] = true;
        }

        auto button = makeTextButton(juce::String(static_cast<int>(bandIndex + 1)));
        button->setClickingTogglesState(false);
        button->onClick = [this, bandIndex] { selectBand(bandIndex); };
        addAndMakeVisible(*button);
        monitorButtons[bandIndex] = std::move(button);

        auto page = std::make_unique<BandPageComponent>(*this, bandIndex, uiAccent);
        bandPages[bandIndex] = std::move(page);
    }

    if (! uiStateLoaded)
        allBandsActive = activeSoloCount != 1;

    visibleBandIndex = juce::jmin(visibleBandIndex, getActiveBandCount() - 1);

    auto allButton = makeTextButton("A");
    allButton->setClickingTogglesState(false);
    allButton->onClick = [this] { setAllBandsMonitoring(); };
    addAndMakeVisible(*allButton);
    monitorButtons[numBands] = std::move(allButton);

    allBandsPage = std::make_unique<FullbandPageComponent>(*this);

    pageViewport.setInterceptsMouseClicks(false, true);
    pageViewport.setScrollBarsShown(false, false);
    pageViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    pageViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(pageViewport);

    updateMonitorButtons();
    updatePageVisibility();
}

MultibandModuleComponent::~MultibandModuleComponent()
{
    pageViewport.setViewedComponent(nullptr, false);
}

void MultibandModuleComponent::loadUiState()
{
    auto& state = valueTreeState.state;
    autoSoloEnabled = getBool(state, config.moduleKey, "autoSoloEnabled", true);
    allBandsActive = getBool(state, config.moduleKey, "allBandsActive", true);
    visibleBandIndex = static_cast<size_t>(juce::jlimit(0,
                                                       static_cast<int>(numBands - 1),
                                                       getInt(state, config.moduleKey, "visibleBandIndex", 0)));

    manualSoloMask = {};

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        manualSoloMask[bandIndex] = getBool(state,
                                            config.moduleKey,
                                            "manualSolo." + juce::String(static_cast<int>(bandIndex)),
                                            false);
    }

    uiStateLoaded = getBool(state, config.moduleKey, "hasUiState", false);
}

void MultibandModuleComponent::saveUiState()
{
    auto& state = valueTreeState.state;
    setBool(state, config.moduleKey, "autoSoloEnabled", autoSoloEnabled);
    setBool(state, config.moduleKey, "allBandsActive", allBandsActive);
    setInt(state, config.moduleKey, "visibleBandIndex", static_cast<int>(visibleBandIndex));
    setBool(state, config.moduleKey, "hasUiState", true);

    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
        setBool(state,
                config.moduleKey,
                "manualSolo." + juce::String(static_cast<int>(bandIndex)),
                manualSoloMask[bandIndex]);
}

void MultibandModuleComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colours::black);
}

void MultibandModuleComponent::resized()
{
    auto bounds = getContentBounds();
    auto monitorRow = bounds.removeFromTop(rowHeight);
    const auto buttonCount = static_cast<int>(monitorButtons.size());
    const auto totalGapWidth = parameterGap * (buttonCount - 1);
    const auto baseButtonWidth = (monitorRow.getWidth() - totalGapWidth) / buttonCount;
    auto remainder = (monitorRow.getWidth() - totalGapWidth) - (baseButtonWidth * buttonCount);

    for (auto& button : monitorButtons)
    {
        const auto buttonWidth = baseButtonWidth + (remainder > 0 ? 1 : 0);

        if (button != nullptr)
            button->setBounds(monitorRow.removeFromLeft(buttonWidth));
        else
            monitorRow.removeFromLeft(buttonWidth);

        monitorRow.removeFromLeft(parameterGap);
        remainder = juce::jmax(0, remainder - 1);
    }

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);

    pageViewport.setBounds(bounds);
    updatePageViewport();
    saveUiState();
}

void MultibandModuleComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    scrollPageViewport(event, wheel);
}

void MultibandModuleComponent::scrollPageViewport(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (pageViewport.getViewedComponent() == nullptr)
        return;

    const auto editorPosition = event.getEventRelativeTo(this).getPosition();

    if (! pageViewport.getBounds().contains(editorPosition))
        return;

    scrollViewportWithWheel(pageViewport, pageViewport.getViewedComponent()->getHeight(), wheel);
}

juce::Rectangle<int> MultibandModuleComponent::getContentBounds() const noexcept
{
    auto bounds = getLocalBounds();
    const auto editorInsetX = getEditorInsetX(bounds.getWidth());
    bounds.removeFromLeft(editorInsetX);
    bounds.removeFromRight(editorInsetX);
    return bounds;
}

void MultibandModuleComponent::refreshCurrentPageLayout()
{
    updatePageViewport();
}

void MultibandModuleComponent::refreshExternalState()
{
    if (config.refreshExternalState != nullptr && ! config.refreshExternalState())
        return;

    if (allBandsPage != nullptr)
        allBandsPage->refreshExternalState();

    updateMonitorButtons();
    refreshCurrentPageLayout();
}

void MultibandModuleComponent::selectBand(const size_t bandIndex)
{
    visibleBandIndex = juce::jmin(bandIndex, getActiveBandCount() - 1);
    allBandsActive = false;

    if (autoSoloEnabled)
        manualSoloMask = {};

    updateMonitorButtons();
    updatePageVisibility();
    syncMonitorParameters();
    saveUiState();
    resized();
    clearFocus();
}

void MultibandModuleComponent::toggleManualSolo(const size_t bandIndex)
{
    if (autoSoloEnabled || getActiveBandCount() <= 1 || bandIndex >= getActiveBandCount())
        return;

    visibleBandIndex = juce::jmin(bandIndex, numBands - 1);
    allBandsActive = false;
    manualSoloMask[visibleBandIndex] = ! manualSoloMask[visibleBandIndex];
    updateMonitorButtons();
    updatePageVisibility();
    syncMonitorParameters();
    saveUiState();
    clearFocus();
}

void MultibandModuleComponent::changeActiveSplitCount(const int delta)
{
    if (activeSplitCountParameter == nullptr)
        return;

    const auto currentValue = static_cast<int>(
        std::round(activeSplitCountParameter->convertFrom0to1(activeSplitCountParameter->getValue())));
    const auto newValue = juce::jlimit(0, static_cast<int>(numBands - 1), currentValue + delta);

    if (newValue == currentValue)
        return;

    setParameterPlainValue(config.makeActiveSplitCountParameterId(), static_cast<float>(newValue));
    visibleBandIndex = juce::jmin(visibleBandIndex, getActiveBandCount() - 1);
    manualSoloMask = {};

    if (allBandsPage != nullptr)
        allBandsPage->refreshExternalState();

    updateMonitorButtons();
    updatePageVisibility();
    syncMonitorParameters();
    saveUiState();
    resized();
}

void MultibandModuleComponent::setAllBandsMonitoring()
{
    allBandsActive = true;
    updateMonitorButtons();
    updatePageVisibility();
    syncMonitorParameters();
    saveUiState();
    clearFocus();
}

void MultibandModuleComponent::setAutoSoloEnabled(const bool shouldBeEnabled)
{
    autoSoloEnabled = shouldBeEnabled;
    manualSoloMask = {};
    updateMonitorButtons();
    syncMonitorParameters();
    saveUiState();
}

void MultibandModuleComponent::syncMonitorParameters()
{
    for (size_t bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto* parameter = soloParameters[bandIndex];

        if (parameter == nullptr)
            continue;

        const auto enabled = bandIndex < getActiveBandCount()
            && ((autoSoloEnabled && ! allBandsActive && bandIndex == visibleBandIndex)
                || (! autoSoloEnabled && manualSoloMask[bandIndex]));
        setParameterNormalisedValue(*parameter, parameter->convertTo0to1(enabled ? 1.0f : 0.0f));
    }

    if (config.markParametersDirty != nullptr)
        config.markParametersDirty();
}

void MultibandModuleComponent::updateMonitorButtons()
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

        if (auto* page = bandPages[bandIndex].get())
            page->refreshExternalState();
    }

    if (auto* button = monitorButtons[numBands].get())
    {
        button->setVisible(true);
        button->setEnabled(true);
        button->setAlpha(1.0f);
        button->setToggleState(allBandsActive, juce::dontSendNotification);
    }
}

void MultibandModuleComponent::updatePageVisibility()
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

juce::Component* MultibandModuleComponent::getCurrentPageComponent() const noexcept
{
    if (allBandsActive)
        return allBandsPage.get();

    return visibleBandIndex < bandPages.size() ? bandPages[visibleBandIndex].get()
                                               : nullptr;
}

int MultibandModuleComponent::getCurrentPagePreferredHeight() const noexcept
{
    if (allBandsActive)
        return allBandsPage != nullptr ? allBandsPage->getPreferredHeight() : 0;

    return visibleBandIndex < bandPages.size() && bandPages[visibleBandIndex] != nullptr
        ? bandPages[visibleBandIndex]->getPreferredHeight()
        : 0;
}

void MultibandModuleComponent::updatePageViewport()
{
    auto* currentPage = getCurrentPageComponent();

    if (currentPage == nullptr)
        return;

    const auto viewportBounds = pageViewport.getLocalBounds();

    if (viewportBounds.isEmpty())
        return;

    const auto preserveScroll = pageViewport.getViewedComponent() == currentPage;
    const auto previousScrollY = preserveScroll ? pageViewport.getViewPositionY() : 0;

    if (! preserveScroll)
        pageViewport.setViewedComponent(currentPage, false);

    const auto pageHeight = juce::jmax(viewportBounds.getHeight(), getCurrentPagePreferredHeight());
    currentPage->setSize(viewportBounds.getWidth(), pageHeight);
    const auto maxScrollY = juce::jmax(0, pageHeight - viewportBounds.getHeight());
    pageViewport.setViewPosition(0, juce::jlimit(0, maxScrollY, previousScrollY));
}

size_t MultibandModuleComponent::getActiveSplitCount() const
{
    if (activeSplitCountParameter == nullptr)
        return 0;

    return static_cast<size_t>(juce::jlimit(0,
                                           static_cast<int>(numBands - 1),
                                           static_cast<int>(std::round(activeSplitCountParameter->convertFrom0to1(
                                               activeSplitCountParameter->getValue())))));
}

size_t MultibandModuleComponent::getActiveBandCount() const
{
    return getActiveSplitCount() + 1;
}

bool MultibandModuleComponent::setParameterPlainValue(const juce::String& parameterId, const float plainValue)
{
    if (auto* parameter = valueTreeState.getParameter(parameterId))
        return setParameterNormalisedValue(*parameter, parameter->convertTo0to1(plainValue));

    return false;
}

bool MultibandModuleComponent::setParameterNormalisedValue(juce::RangedAudioParameter& parameter, const float normalisedValue)
{
    const auto value = juce::jlimit(0.0f, 1.0f, normalisedValue);
    const auto wasDifferent = std::abs(parameter.getValue() - value) > 1.0e-6f;

    if (! wasDifferent)
        return false;

    if (config.undoManager != nullptr)
        config.undoManager->beginNewTransaction();

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost(value);
    parameter.endChangeGesture();

    if (config.markParametersDirty != nullptr)
        config.markParametersDirty();

    return true;
}

void MultibandModuleComponent::clearFocus()
{
    if (config.clearKeyboardFocus != nullptr)
        config.clearKeyboardFocus();
    else
        clearKeyboardFocus(*this);
}
