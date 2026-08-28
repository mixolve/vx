#include "EditorControls.h"

namespace
{
class ScrollingComboBoxLabel final : public juce::Label
{
public:
    void paint(juce::Graphics& graphics) override
    {
        graphics.setColour(findColour(juce::Label::textColourId));
        graphics.setFont(getFont());

        if (drawLoopingText(graphics,
                            getText(),
                            getLocalBounds().reduced(uiGap, 0),
                            getFont(),
                            getJustificationType()))
        {
            scheduleMarqueeRepaint();
        }
    }

private:
    void scheduleMarqueeRepaint()
    {
        if (marqueeRepaintPending || ! isShowing())
            return;

        marqueeRepaintPending = true;
        juce::Timer::callAfterDelay(16, [safeThis = juce::Component::SafePointer<ScrollingComboBoxLabel>(this)]
        {
            if (safeThis == nullptr)
                return;

            safeThis->marqueeRepaintPending = false;
            safeThis->repaint();
        });
    }

    bool marqueeRepaintPending = false;
};
}

AvaAudioProcessorEditor::AvaLookAndFeel::AvaLookAndFeel()
{
    if (auto typeface = getUiRegularTypeface())
        setDefaultSansSerifTypeface(typeface);
}

juce::Typeface::Ptr AvaAudioProcessorEditor::AvaLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
#if JUCE_TARGET_HAS_BINARY_DATA
    juce::ignoreUnused(font);
    if (auto typeface = getUiRegularTypeface())
        return typeface;
#else
    juce::ignoreUnused(font);
#endif
    return LookAndFeel_V4::getTypefaceForFont(font);
}

juce::Font AvaAudioProcessorEditor::AvaLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return makeUiFont();
}

juce::Label* AvaAudioProcessorEditor::AvaLookAndFeel::createComboBoxTextBox(juce::ComboBox&)
{
    auto* label = new ScrollingComboBoxLabel();
    label->setEditable(false, false, false);
    label->setInterceptsMouseClicks(false, false);
    return label;
}

juce::Font AvaAudioProcessorEditor::AvaLookAndFeel::getPopupMenuFont()
{
    return makeUiFont();
}

void AvaAudioProcessorEditor::AvaLookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& graphics,
                                                                                 const int width,
                                                                                 const int height,
                                                                                 const juce::PopupMenu::Options&)
{
    graphics.setColour(uiPopup);
    graphics.fillRect(0, 0, width, height);
}

int AvaAudioProcessorEditor::AvaLookAndFeel::getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&)
{
    return 1;
}

void AvaAudioProcessorEditor::AvaLookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& text,
                                                                                   bool isSeparator,
                                                                                   int,
                                                                                   int& idealWidth,
                                                                                   int& idealHeight,
                                                                                   const juce::PopupMenu::Options&)
{
    if (isSeparator)
    {
        idealWidth = 0;
        idealHeight = 2;
        return;
    }

    idealWidth = juce::jmax(80, getTextPixelWidth(makeUiFont(), text) + uiGapDouble);
    idealHeight = 30;
}

void AvaAudioProcessorEditor::AvaLookAndFeel::drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics& graphics, const juce::Path& path, juce::Image&)
{
    graphics.setColour(uiPopup);
    graphics.fillPath(path);
}

int AvaAudioProcessorEditor::AvaLookAndFeel::getCallOutBoxBorderSize(const juce::CallOutBox&)
{
    return 1;
}

float AvaAudioProcessorEditor::AvaLookAndFeel::getCallOutBoxCornerSize(const juce::CallOutBox&)
{
    return 0.0f;
}


void AvaAudioProcessorEditor::AvaLookAndFeel::drawComboBox(juce::Graphics& g,
                                                           int width,
                                                           int height,
                                                           bool,
                                                           int,
                                                           int,
                                                           int,
                                                           int,
                                                           juce::ComboBox& box)
{
    if (dynamic_cast<NoTickComboBox*>(&box) == nullptr)
    {
        juce::LookAndFeel_V4::drawComboBox(g,
                                           width,
                                           height,
                                           false,
                                           0,
                                           0,
                                           0,
                                           0,
                                           box);
        return;
    }

    const auto* noTickBox = dynamic_cast<NoTickComboBox*>(&box);
    const auto backgroundColour = noTickBox != nullptr && noTickBox->isPressedHighlightEnabled()
        ? uiGrey700
        : box.findColour(juce::ComboBox::backgroundColourId);

    g.setColour(backgroundColour);
    g.fillRect(0, 0, width, height);

    g.setColour(noTickBox != nullptr && noTickBox->isPressedHighlightEnabled() ? uiGrey500
                                                                               : box.findColour(juce::ComboBox::outlineColourId));
    g.drawRect(0, 0, width, height, 1);
}

void AvaAudioProcessorEditor::AvaLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    if (dynamic_cast<NoTickComboBox*>(&box) == nullptr)
    {
        juce::LookAndFeel_V4::positionComboBoxText(box, label);
        label.setFont(getComboBoxFont(box));
        return;
    }

    label.setBounds(1, 1,
                    box.getWidth() - 2,
                    box.getHeight() - 2);

    label.setFont(getComboBoxFont(box));
}

void AvaAudioProcessorEditor::AvaLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                                                const juce::Rectangle<int>& area,
                                                                bool isSeparator,
                                                                bool isActive,
                                                                bool isHighlighted,
                                                                bool isTicked,
                                                                bool hasSubMenu,
                                                                const juce::String& text,
                                                                const juce::String& shortcutKeyText,
                                                                const juce::Drawable* icon,
                                                                const juce::Colour* textColour)
{
    juce::ignoreUnused(isTicked);
    juce::LookAndFeel_V4::drawPopupMenuItem(g,
                                            area,
                                            isSeparator,
                                            isActive,
                                            isHighlighted,
                                            false,
                                            hasSubMenu,
                                            text,
                                            shortcutKeyText,
                                            icon,
                                            textColour);
}

void AvaAudioProcessorEditor::AvaLookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g,
                                                                           const juce::Rectangle<int>& area,
                                                                           bool isHighlighted,
                                                                           const juce::PopupMenu::Item& item,
                                                                           const juce::PopupMenu::Options& options)
{
    if (item.isSeparator)
    {
        g.setColour(uiGrey500);
        g.fillRect(area.withHeight(1).withCentre(area.getCentre()));
        return;
    }

    g.setColour(isHighlighted ? uiGrey700 : uiGrey800);
    g.fillRect(area);

    g.setColour(uiGrey500);
    g.drawRect(area, 1);

    g.setColour(item.isEnabled ? uiWhite : uiGrey500);
    g.setFont(makeUiFont());
    const auto isNoTickTarget = dynamic_cast<NoTickComboBox*>(options.getTargetComponent()) != nullptr;
    const auto justification = isNoTickTarget ? dynamic_cast<NoTickComboBox*>(options.getTargetComponent())->getPopupMenuTextJustification()
                                              : juce::Justification::centred;
    g.drawFittedText(item.text,
                     area.reduced(uiGap, 0),
                     justification,
                     1,
                     1.0f);
}
