#include "PromptComponents.h"

#include <utility>

namespace
{
constexpr int promptPanelPadding = uiGap;
constexpr int promptItemHeight = 30;
constexpr int promptItemGap = uiGap;

class FloatingChoicePrompt final : public PromptComponent
{
public:
    using SelectCallback = std::function<void(int)>;
    using DismissCallback = std::function<void()>;
    using CloseCallback = std::function<void()>;

    FloatingChoicePrompt(juce::Rectangle<int> anchorBoundsIn,
                         juce::StringArray choicesIn,
                         int selectedIndexIn,
                         std::vector<bool> itemEnabledStatesIn,
                         juce::Justification itemJustificationIn,
                         SelectCallback selectCallback,
                         DismissCallback dismissCallback,
                         CloseCallback closeCallback,
                         juce::StringArray itemTooltipsIn)
        : anchorBounds(std::move(anchorBoundsIn)),
          choices(std::move(choicesIn)),
          selectedIndex(selectedIndexIn),
          itemEnabledStates(std::move(itemEnabledStatesIn)),
          itemJustification(itemJustificationIn),
          itemTooltips(std::move(itemTooltipsIn)),
          onSelect(std::move(selectCallback)),
          onDismiss(std::move(dismissCallback)),
          onClose(std::move(closeCallback))
    {
        setOpaque(false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(true, true);
        choiceViewport.setViewedComponent(&choiceContent, false);
        choiceViewport.setScrollBarsShown(false, false);
        choiceViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
        choiceViewport.setWantsKeyboardFocus(false);
        choiceViewport.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(choiceViewport);

        if (itemEnabledStates.size() != static_cast<size_t>(choices.size()))
            itemEnabledStates.assign(static_cast<size_t>(choices.size()), true);

        itemButtons.reserve(static_cast<size_t>(choices.size()));

        for (int index = 0; index < choices.size(); ++index)
        {
            auto button = std::make_unique<BoxTextButton>(uiAccent);
            button->setButtonText(choices[index]);
            if (juce::isPositiveAndBelow(index, itemTooltips.size()))
                button->setTooltip(itemTooltips[index]);
            button->setTextJustification(itemJustification);
            button->setAlwaysAccentOutline(index == selectedIndex);
            const auto isEnabled = itemEnabled(index);
            button->setEnabled(isEnabled);
            button->setAlpha(1.0f);
            button->onClick = [safeThis = juce::Component::SafePointer<FloatingChoicePrompt>(this), index]
            {
                if (safeThis != nullptr)
                    safeThis->choose(index);
            };
            choiceContent.addAndMakeVisible(*button);
            itemButtons.push_back(std::move(button));
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillAll();

        g.setColour(uiGrey700);
        g.fillRect(panelBounds);

        g.setColour(uiGrey500);
        g.drawRect(panelBounds, 1);
    }

    void resized() override
    {
        const auto visibleBounds = getVisibleBounds();
        const auto itemCount = static_cast<int>(choices.size());
        const auto itemBlockHeight = (itemCount * promptItemHeight)
            + (juce::jmax(0, itemCount - 1) * promptItemGap);
        const auto availableWidth = juce::jmax(0, visibleBounds.getWidth() - (promptPanelPadding * 2));
        const auto promptWidth = juce::jmax(1, juce::jmin(availableWidth, anchorBounds.getWidth()));
        const auto promptHeight = juce::jmin(juce::jmax(anchorBounds.getHeight(), itemBlockHeight + (promptPanelPadding * 2)),
                                             juce::jmax(0, visibleBounds.getHeight() - (promptPanelPadding * 2)));

        panelBounds = juce::Rectangle<int>(promptWidth, promptHeight);
        panelBounds.setX(anchorBounds.getX());
        panelBounds.setY(anchorBounds.getY());
        panelBounds = panelBounds.constrainedWithin(visibleBounds.reduced(promptPanelPadding));

        const auto viewportBounds = panelBounds.reduced(promptPanelPadding);
        choiceViewport.setBounds(viewportBounds);
        choiceContent.setSize(viewportBounds.getWidth(), juce::jmax(viewportBounds.getHeight(), itemBlockHeight));

        auto contentBounds = choiceContent.getLocalBounds();
        for (size_t index = 0; index < itemButtons.size(); ++index)
        {
            auto itemBounds = contentBounds.removeFromTop(promptItemHeight);
            itemButtons[index]->setBounds(itemBounds);

            if (index + 1 < itemButtons.size())
                contentBounds.removeFromTop(promptItemGap);
        }

        if (! initialScrollApplied)
        {
            initialScrollApplied = true;

            if (juce::isPositiveAndBelow(selectedIndex, static_cast<int>(itemButtons.size())))
            {
                const auto itemY = selectedIndex * (promptItemHeight + promptItemGap);
                const auto maxOffset = juce::jmax(0, choiceContent.getHeight() - choiceViewport.getHeight());
                const auto targetY = itemY - juce::jmax(0, choiceViewport.getHeight() - promptItemHeight) / 2;
                choiceViewport.setViewPosition(0, juce::jlimit(0, maxOffset, targetY));
            }
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (! panelBounds.contains(event.getPosition()))
            cancel();
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! panelBounds.contains(event.getPosition()))
            return;

        const auto directionalDelta = wheel.isReversed ? wheel.deltaY : -wheel.deltaY;
        const auto scrollAmount = wheel.isSmooth
            ? static_cast<int>(std::round(directionalDelta * 220.0f))
            : static_cast<int>(std::round((directionalDelta < 0.0f ? -1.0f : 1.0f) * 48.0f));

        if (scrollAmount == 0)
            return;

        const auto maxOffset = juce::jmax(0, choiceContent.getHeight() - choiceViewport.getHeight());

        if (maxOffset <= 0)
            return;

        choiceViewport.setViewPosition(0, juce::jlimit(0, maxOffset, choiceViewport.getViewPositionY() + scrollAmount));
    }

private:
    juce::Rectangle<int> getVisibleBounds() const
    {
        auto bounds = getLocalBounds();


        return bounds;
    }

    void choose(const int index)
    {
        if (closePending)
            return;

        if (! itemEnabled(index))
            return;

        auto deferredSelectCallback = std::move(onSelect);
        const auto choiceIndex = index;
        requestClose();

        juce::MessageManager::callAsync([choiceIndex,
                                         callback = std::move(deferredSelectCallback)]() mutable
                                        {
                                            if (callback != nullptr)
                                                callback(choiceIndex);
                                        });
    }

    void requestClose()
    {
        if (closePending)
            return;

        closePending = true;

        if (onDismiss != nullptr)
            onDismiss();

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<FloatingChoicePrompt>(this)]
                                        {
                                            if (safeThis == nullptr || safeThis->onClose == nullptr)
                                                return;

                                            safeThis->onClose();
                                        });
    }

    void cancel()
    {
        requestClose();
    }

    bool itemEnabled(const int index) const
    {
        if (! juce::isPositiveAndBelow(index, static_cast<int>(itemEnabledStates.size())))
            return true;

        return itemEnabledStates[static_cast<size_t>(index)];
    }

    juce::Rectangle<int> anchorBounds;
    juce::StringArray choices;
    int selectedIndex = -1;
    std::vector<bool> itemEnabledStates;
    juce::Justification itemJustification = juce::Justification::centred;
    juce::Viewport choiceViewport;
    juce::Component choiceContent;
    std::vector<std::unique_ptr<BoxTextButton>> itemButtons;
    juce::StringArray itemTooltips;
    SelectCallback onSelect;
    DismissCallback onDismiss;
    CloseCallback onClose;
    juce::Rectangle<int> panelBounds;
    bool initialScrollApplied = false;
    bool closePending = false;
};
} // namespace

std::unique_ptr<PromptComponent> makeChoicePrompt(juce::Rectangle<int> anchorBounds,
                                                  juce::StringArray choices,
                                                  const int selectedIndex,
                                                  std::vector<bool> itemEnabledStates,
                                                  const juce::Justification itemJustification,
                                                  std::function<void(int)> onSelect,
                                                  std::function<void()> onDismiss,
                                                  std::function<void()> onClose,
                                                  juce::StringArray itemTooltips)
{
    return std::make_unique<FloatingChoicePrompt>(std::move(anchorBounds),
                                                  std::move(choices),
                                                  selectedIndex,
                                                  std::move(itemEnabledStates),
                                                  itemJustification,
                                                  std::move(onSelect),
                                                  std::move(onDismiss),
                                                  std::move(onClose),
                                                  std::move(itemTooltips));
}
