#include "module.mxe.EditorSection.h"

#include "module.mxe.EditorUiState.h"

namespace mxe::editor
{
SectionComponent::SectionComponent(juce::AudioProcessorValueTreeState& state,
                                   const std::function<juce::String(const char*)>& parameterIdProvider,
                                   const SectionSpec& spec,
                                   const juce::Colour accent,
                                   std::function<void()> onLayoutChangeIn,
                                   std::function<void(bool)> onExpandedChangedIn,
                                   std::function<ValueConstraint(const char*)> valueConstraintProvider,
                                   std::function<void(const char*)> soloClickProvider,
                                   std::function<bool(const char*)> soloActiveProvider,
                                   std::function<bool(const char*)> soloEnabledProvider,
                                   std::function<size_t()> extraRowsBeforeControlsProviderIn,
                                   std::function<size_t()> enabledControlCountProviderIn,
                                   std::function<size_t()> extraRowsAfterControlsProviderIn,
                                   juce::AudioProcessorValueTreeState* valueTreeStateIn,
                                   juce::String expandedStateKeyIn)
    : valueTreeState(valueTreeStateIn),
      onLayoutChange(std::move(onLayoutChangeIn)),
      onExpandedChanged(std::move(onExpandedChangedIn)),
      expandedStateKey(std::move(expandedStateKeyIn)),
      extraRowsBeforeControlsProvider(std::move(extraRowsBeforeControlsProviderIn)),
      enabledControlCountProvider(std::move(enabledControlCountProviderIn)),
      extraRowsAfterControlsProvider(std::move(extraRowsAfterControlsProviderIn)),
      staysExpandedOnSelfClick(spec.staysExpandedOnSelfClick),
      headerButton(accent)
{
    headerButton.setButtonText(spec.title);
    headerButton.setClickingTogglesState(true);
    headerButton.setToggleState(loadExpandedState(spec.startsExpanded), juce::dontSendNotification);
    headerButton.onClick = [this]
    {
        if (! headerButton.getToggleState() && staysExpandedOnSelfClick)
            headerButton.setToggleState(true, juce::dontSendNotification);

        updateExpandedState();

        if (onExpandedChanged)
            onExpandedChanged(headerButton.getToggleState());

        persistExpandedState();

        if (onLayoutChange)
            onLayoutChange();
    };
    addAndMakeVisible(headerButton);

    controls.reserve(spec.controls.size());

    for (const auto& controlSpec : spec.controls)
    {
        auto control = std::make_unique<ParameterControl>(state,
                                                          parameterIdProvider(controlSpec.suffix),
                                                          controlSpec,
                                                          accent,
                                                          valueConstraintProvider ? valueConstraintProvider(controlSpec.suffix) : ValueConstraint {},
                                                          soloClickProvider ? std::function<void()> { [soloClickProvider, suffix = controlSpec.suffix] { soloClickProvider(suffix); } } : std::function<void()> {},
                                                          soloActiveProvider ? std::function<bool()> { [soloActiveProvider, suffix = controlSpec.suffix] { return soloActiveProvider(suffix); } } : std::function<bool()> {},
                                                          soloEnabledProvider ? std::function<bool()> { [soloEnabledProvider, suffix = controlSpec.suffix] { return soloEnabledProvider(suffix); } } : std::function<bool()> {});
        addAndMakeVisible(*control);
        controls.push_back(std::move(control));
    }

    updateExpandedState();
}

bool SectionComponent::loadExpandedState(const bool defaultExpanded) const
{
    if (valueTreeState == nullptr || expandedStateKey.isEmpty())
        return defaultExpanded;

    return mxe::editor::uiState::getBool(valueTreeState->state, juce::Identifier { expandedStateKey }, defaultExpanded);
}

void SectionComponent::persistExpandedState()
{
    if (valueTreeState == nullptr || expandedStateKey.isEmpty())
        return;

    mxe::editor::uiState::setBool(valueTreeState->state, juce::Identifier { expandedStateKey }, headerButton.getToggleState());
    mxe::editor::uiState::setBool(valueTreeState->state, mxe::editor::uiState::hasUiStateId(), true);
}

int SectionComponent::getPreferredHeight() const
{
    auto height = buttonHeight;

    if (! headerButton.getToggleState())
        return height;

    auto hasRows = false;
    const auto extraRowsBeforeControls = getExtraRowsBeforeControls();
    const auto extraRowsAfterControls = getExtraRowsAfterControls();

    if (extraRowsBeforeControls == 0 && controls.empty() && extraRowsAfterControls == 0)
        return height;

    height += contentFrameInsetY;
    height += sectionHeaderToContentGap;

    for (size_t rowIndex = 0; rowIndex < extraRowsBeforeControls; ++rowIndex)
    {
        if (hasRows)
            height += sectionRowGap;

        height += buttonHeight;
        hasRows = true;
    }

    for (const auto& control : controls)
    {
        if (hasRows)
            height += sectionRowGap;

        height += control->getPreferredHeight();
        hasRows = true;
    }

    for (size_t rowIndex = 0; rowIndex < extraRowsAfterControls; ++rowIndex)
    {
        if (hasRows)
            height += sectionRowGap;

        height += buttonHeight;
        hasRows = true;
    }

    height += contentFrameInsetY;
    return height;
}

bool SectionComponent::isExpanded() const noexcept
{
    return headerButton.getToggleState();
}

void SectionComponent::refreshExternalState()
{
    updateExpandedState();

    for (auto& control : controls)
        control->refreshExternalState();
}

juce::Rectangle<int> SectionComponent::getExtraControlBounds(const size_t extraControlIndex) const
{
    return getExtraControlBounds(extraControlIndex, {});
}

juce::Rectangle<int> SectionComponent::getExtraControlBounds(const size_t extraControlIndex, const ExtraControlPlacement placement) const
{
    if (! headerButton.getToggleState())
        return {};

    const auto extraRowsBeforeControls = getExtraRowsBeforeControls();
    const auto extraRowsAfterControls = getExtraRowsAfterControls();
    const auto existingControlsBeforeExtra = juce::jmin(controls.size(), placement.afterVisibleControlCount);

    auto bounds = getLocalBounds()
        .withTrimmedLeft(contentFrameInsetX)
        .withTrimmedRight(contentFrameInsetX)
        .withTrimmedBottom(contentFrameInsetY);

    bounds.removeFromTop(buttonHeight);

    if (extraRowsBeforeControls == 0 && controls.empty() && extraRowsAfterControls == 0)
        return {};

    bounds.removeFromTop(contentFrameInsetY);

    bounds.removeFromTop(sectionHeaderToContentGap);

    auto hasRows = false;
    const auto skipRow = [&bounds, &hasRows] (const int rowHeight)
    {
        if (hasRows)
            bounds.removeFromTop(sectionRowGap);

        bounds.removeFromTop(rowHeight);
        hasRows = true;
    };
    const auto takeButtonRow = [&bounds, &hasRows]
    {
        if (hasRows)
            bounds.removeFromTop(sectionRowGap);

        hasRows = true;
        return bounds.removeFromTop(buttonHeight);
    };

    if (placement.afterVisibleControlCount == 0)
    {
        for (size_t index = 0; index < extraControlIndex; ++index)
            skipRow(buttonHeight);

        return takeButtonRow();
    }

    for (size_t rowIndex = 0; rowIndex < extraRowsBeforeControls; ++rowIndex)
        skipRow(buttonHeight);

    const auto controlsBeforeExtra = existingControlsBeforeExtra;

    for (size_t controlIndex = 0; controlIndex < controlsBeforeExtra; ++controlIndex)
        skipRow(controls[controlIndex]->getPreferredHeight());

    for (size_t index = 0; index < extraControlIndex; ++index)
        skipRow(buttonHeight);

    return takeButtonRow();
}

void SectionComponent::setExpanded(const bool shouldBeExpanded)
{
    if (headerButton.getToggleState() == shouldBeExpanded)
        return;

    headerButton.setToggleState(shouldBeExpanded, juce::dontSendNotification);
    updateExpandedState();
    persistExpandedState();
    repaint();
}

void SectionComponent::paint(juce::Graphics& graphics)
{
    if (! headerButton.getToggleState())
        return;

    const auto extraRowsBeforeControls = getExtraRowsBeforeControls();
    const auto extraRowsAfterControls = getExtraRowsAfterControls();

    if (extraRowsBeforeControls == 0 && controls.empty() && extraRowsAfterControls == 0)
        return;

    const auto frameBounds = getLocalBounds().withTrimmedTop(contentFrameInsetY);

    if (frameBounds.isEmpty())
        return;

    graphics.setColour(uiAccent);
    graphics.drawRect(frameBounds, frameLineThickness);
}

void SectionComponent::resized()
{
    auto bounds = getLocalBounds();

    bounds = bounds.withTrimmedLeft(contentFrameInsetX).withTrimmedRight(contentFrameInsetX);

    headerButton.setBounds(bounds.removeFromTop(buttonHeight));

    if (! headerButton.getToggleState())
        return;

    const auto extraRowsBeforeControls = getExtraRowsBeforeControls();
    const auto extraRowsAfterControls = getExtraRowsAfterControls();

    if (extraRowsBeforeControls == 0 && controls.empty() && extraRowsAfterControls == 0)
        return;

    bounds.removeFromTop(contentFrameInsetY);
    bounds = bounds.withTrimmedBottom(contentFrameInsetY);

    auto hasRows = false;
    bounds.removeFromTop(sectionHeaderToContentGap);

    for (size_t rowIndex = 0; rowIndex < extraRowsBeforeControls; ++rowIndex)
    {
        if (hasRows)
            bounds.removeFromTop(sectionRowGap);

        bounds.removeFromTop(buttonHeight);
        hasRows = true;
    }

    for (size_t controlIndex = 0; controlIndex < controls.size(); ++controlIndex)
    {
        auto& control = controls[controlIndex];
        const auto height = control->getPreferredHeight();

        if (hasRows)
            bounds.removeFromTop(sectionRowGap);

        control->setBounds(bounds.removeFromTop(height));
        hasRows = true;
    }
}

void SectionComponent::updateExpandedState()
{
    const auto expanded = headerButton.getToggleState();
    const auto enabledControlCount = getEnabledControlCount();

    for (size_t controlIndex = 0; controlIndex < controls.size(); ++controlIndex)
    {
        controls[controlIndex]->setVisible(expanded);
        controls[controlIndex]->setControlEnabled(controlIndex < enabledControlCount);
    }

    repaint();
}

size_t SectionComponent::getExtraRowsBeforeControls() const
{
    return extraRowsBeforeControlsProvider != nullptr ? extraRowsBeforeControlsProvider() : 0;
}

size_t SectionComponent::getExtraRowsAfterControls() const
{
    return extraRowsAfterControlsProvider != nullptr ? extraRowsAfterControlsProvider() : 0;
}

size_t SectionComponent::getEnabledControlCount() const
{
    if (enabledControlCountProvider)
        return enabledControlCountProvider();

    return controls.size();
}
} // namespace mxe::editor
