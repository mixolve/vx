#include "shell.EditorControls.h"

VxAudioProcessorEditor::VxLookAndFeel::VxLookAndFeel()
{
    if (auto typeface = getUiRegularTypeface())
        setDefaultSansSerifTypeface(typeface);
}

juce::Typeface::Ptr VxAudioProcessorEditor::VxLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
#if JUCE_TARGET_HAS_BINARY_DATA
    const auto useBold = font.isBold();
    if (auto typeface = useBold ? getUiBoldTypeface()
                                : getUiRegularTypeface())
        return typeface;
#else
    juce::ignoreUnused(font);
#endif
    return LookAndFeel_V4::getTypefaceForFont(font);
}

juce::Font VxAudioProcessorEditor::VxLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return makeUiFont();
}

juce::Font VxAudioProcessorEditor::VxLookAndFeel::getPopupMenuFont()
{
    return makeUiFont();
}

void VxAudioProcessorEditor::VxLookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                                                                 int width,
                                                                                 int height,
                                                                                 const juce::PopupMenu::Options&)
{
    g.setColour(uiGrey800);
    g.fillRect(0, 0, width, height);

    g.setColour(uiAccent);
    g.drawRect(0, 0, width, height, 1);
}

int VxAudioProcessorEditor::VxLookAndFeel::getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&)
{
    return 1;
}

void VxAudioProcessorEditor::VxLookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& text,
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

    idealWidth = juce::jmax(80, getTextPixelWidth(makeUiFont(), text) + 16);
    idealHeight = 30;
}

void VxAudioProcessorEditor::VxLookAndFeel::drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour(uiGrey700);
    g.fillPath(path);

    g.setColour(uiGrey500);
    g.strokePath(path, juce::PathStrokeType(1.0f));
}

int VxAudioProcessorEditor::VxLookAndFeel::getCallOutBoxBorderSize(const juce::CallOutBox&)
{
    return 1;
}

float VxAudioProcessorEditor::VxLookAndFeel::getCallOutBoxCornerSize(const juce::CallOutBox&)
{
    return 0.0f;
}

void VxAudioProcessorEditor::VxLookAndFeel::drawComboBox(juce::Graphics& g,
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

void VxAudioProcessorEditor::VxLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
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

void VxAudioProcessorEditor::VxLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
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

void VxAudioProcessorEditor::VxLookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g,
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

    g.setColour(uiAccent);
    g.drawRect(area, 1);

    g.setColour(item.isEnabled ? uiWhite : uiGrey500);
    g.setFont(makeUiFont());
    const auto isNoTickTarget = dynamic_cast<NoTickComboBox*>(options.getTargetComponent()) != nullptr;
    const auto justification = isNoTickTarget ? dynamic_cast<NoTickComboBox*>(options.getTargetComponent())->getPopupMenuTextJustification()
                                              : juce::Justification::centred;
    g.drawFittedText(item.text,
                     area.reduced(isNoTickTarget ? 8 : 6, 0),
                     justification,
                     1,
                     1.0f);
}
