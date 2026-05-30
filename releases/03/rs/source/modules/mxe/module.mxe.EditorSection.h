#pragma once

#include "module.mxe.EditorControls.h"

#include <functional>
#include <memory>
#include <vector>

namespace mxe::editor
{
class SectionComponent final : public juce::Component
{
public:
    struct ExtraControlPlacement
    {
        size_t afterVisibleControlCount = 0;
    };

    SectionComponent(juce::AudioProcessorValueTreeState& state,
                     const std::function<juce::String(const char*)>& parameterIdProvider,
                     const SectionSpec& spec,
                     juce::Colour accent,
                     std::function<void()> onLayoutChangeIn,
                     std::function<void(bool)> onExpandedChangedIn,
                     std::function<ValueConstraint(const char*)> valueConstraintProvider = {},
                     std::function<void(const char*)> soloClickProvider = {},
                     std::function<bool(const char*)> soloActiveProvider = {},
                     std::function<bool(const char*)> soloEnabledProvider = {},
                     std::function<size_t()> extraRowsBeforeControlsProviderIn = {},
                     std::function<size_t()> enabledControlCountProviderIn = {},
                     std::function<size_t()> extraRowsAfterControlsProviderIn = {},
                     juce::AudioProcessorValueTreeState* valueTreeStateIn = nullptr,
                     juce::String expandedStateKeyIn = {});

    int getPreferredHeight() const;
    bool isExpanded() const noexcept;
    void refreshExternalState();
    juce::Rectangle<int> getExtraControlBounds(size_t extraControlIndex) const;
    juce::Rectangle<int> getExtraControlBounds(size_t extraControlIndex, ExtraControlPlacement placement) const;
    void setExpanded(bool shouldBeExpanded);
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void updateExpandedState();
    bool loadExpandedState(bool defaultExpanded) const;
    void persistExpandedState();
    size_t getExtraRowsBeforeControls() const;
    size_t getExtraRowsAfterControls() const;
    size_t getEnabledControlCount() const;

    juce::AudioProcessorValueTreeState* valueTreeState = nullptr;
    std::function<void()> onLayoutChange;
    std::function<void(bool)> onExpandedChanged;
    juce::String expandedStateKey;
    std::function<size_t()> extraRowsBeforeControlsProvider;
    std::function<size_t()> enabledControlCountProvider;
    std::function<size_t()> extraRowsAfterControlsProvider;
    const bool staysExpandedOnSelfClick = false;
    BoxTextButton headerButton;
    std::vector<std::unique_ptr<ParameterControl>> controls;
};
} // namespace mxe::editor
