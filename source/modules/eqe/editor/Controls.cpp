#include "EditorControls.h"

#include <cmath>

namespace
{
int getTextPixelWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, text, 0.0f, 0.0f);
    return static_cast<int>(std::ceil(glyphs.getBoundingBox(0, -1, true).getWidth()));
}

void drawNeutralPopupItem(juce::Graphics& g,
                          const juce::Rectangle<int>& area,
                          const bool isSeparator,
                          const bool isEnabled,
                          const bool isHighlighted,
                          const juce::String& text,
                          const juce::String& shortcutKeyText,
                          const juce::Drawable* icon,
                          const juce::Colour* textColour,
                          const juce::Justification justification)
{
    juce::ignoreUnused(shortcutKeyText, icon);

    if (isSeparator)
    {
        g.setColour(uiGrey500);
        g.fillRect(area.withHeight(1).withCentre(area.getCentre()));
        return;
    }

    g.setColour(isHighlighted ? uiGrey700 : uiGrey800);
    g.fillRect(area);

    g.setColour(uiGrey500);
    g.drawRect(area, 1);

    g.setColour(textColour != nullptr ? *textColour
                                      : (isEnabled ? uiWhite : uiGrey500));
    g.setFont(makeUiFont());
    g.drawFittedText(text,
                     area.reduced(8, 0),
                     justification,
                     1,
                     1.0f);
}
}

CopyPasteTextEditor::CopyPasteTextEditor()
{
    setLookAndFeel(&popupLookAndFeel);
}

CopyPasteTextEditor::~CopyPasteTextEditor()
{
    setLookAndFeel(nullptr);
}

juce::Font CopyPasteTextEditor::PopupLookAndFeel::getPopupMenuFont()
{
    return makeUiFont();
}

void CopyPasteTextEditor::PopupLookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                                                               int width,
                                                                               int height,
                                                                               const juce::PopupMenu::Options&)
{
    g.setColour(uiGrey800);
    g.fillRect(0, 0, width, height);

    g.setColour(uiGrey500);
    g.drawRect(0, 0, width, height, 1);
}

int CopyPasteTextEditor::PopupLookAndFeel::getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&)
{
    return 1;
}

void CopyPasteTextEditor::PopupLookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& text,
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

    idealWidth = juce::jmax(72, getTextPixelWidth(makeUiFont(), text) + 18);
    idealHeight = 30;
}

void CopyPasteTextEditor::PopupLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                                             const juce::Rectangle<int>& area,
                                                             bool isSeparator,
                                                             bool isActive,
                                                             bool isHighlighted,
                                                             bool,
                                                             bool,
                                                             const juce::String& text,
                                                             const juce::String& shortcutKeyText,
                                                             const juce::Drawable* icon,
                                                             const juce::Colour* textColour)
{
    drawNeutralPopupItem(g,
                         area,
                         isSeparator,
                         isActive,
                         isHighlighted,
                         text,
                         shortcutKeyText,
                         icon,
                         textColour,
                         juce::Justification::centredLeft);
}

void CopyPasteTextEditor::PopupLookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g,
                                                                         const juce::Rectangle<int>& area,
                                                                         bool isHighlighted,
                                                                         const juce::PopupMenu::Item& item,
                                                                         const juce::PopupMenu::Options&)
{
    drawNeutralPopupItem(g,
                         area,
                         item.isSeparator,
                         item.isEnabled,
                         isHighlighted,
                         item.text,
                         item.shortcutKeyDescription,
                         item.image.get(),
                         item.colour.isTransparent() ? nullptr : &item.colour,
                         juce::Justification::centredLeft);
}

void CopyPasteTextEditor::addPopupMenuItems(juce::PopupMenu& menuToAddTo, const juce::MouseEvent*)
{
    const auto hasSelection = ! getHighlightedRegion().isEmpty();
    const auto canPaste = ! isReadOnly();

    menuToAddTo.addItem(juce::StandardApplicationCommandIDs::copy, "Copy", hasSelection, false);
    menuToAddTo.addItem(juce::StandardApplicationCommandIDs::paste, "Paste", canPaste, false);
}

void CopyPasteTextEditor::performPopupMenuAction(const int menuItemID)
{
    if (menuItemID == juce::StandardApplicationCommandIDs::copy)
    {
        copyToClipboard();
        return;
    }

    if (menuItemID == juce::StandardApplicationCommandIDs::paste)
        pasteFromClipboard();
}

NoTickComboBox::NoTickComboBox()
{
    setScrollWheelEnabled(false);
}

void NoTickComboBox::showPopup()
{
    if (! isEnabled())
        return;

    if (promptStylePopupEnabled)
    {
        if (auto* owner = findParentComponentOfClass<EqeAudioProcessorEditor>())
        {
            const auto itemCount = getNumItems();

            if (itemCount > 0)
            {
                juce::StringArray itemTexts;
                std::vector<bool> itemEnabledStates;
                itemTexts.ensureStorageAllocated(itemCount);
                itemEnabledStates.reserve(static_cast<size_t>(itemCount));

                for (int index = 0; index < itemCount; ++index)
                {
                    itemTexts.add(getItemText(index));
                    itemEnabledStates.push_back(isItemEnabled(getItemId(index)));
                }

                const auto anchorBounds = owner->getLocalArea(this, getLocalBounds());
                const auto currentSelectedItemIndex = getSelectedItemIndex();

                owner->showChoicePrompt(anchorBounds,
                                        itemTexts,
                                        currentSelectedItemIndex,
                                        std::move(itemEnabledStates),
                                        popupMenuTextJustification,
                                        [safePointer = juce::Component::SafePointer<NoTickComboBox>(this), currentSelectedItemIndex] (int selectedIndex)
                                        {
                                            if (safePointer == nullptr)
                                                return;

                                            const auto shouldReSelectCurrentItem = selectedIndex == currentSelectedItemIndex;
                                            safePointer->setSelectedItemIndex(selectedIndex, juce::sendNotificationSync);

                                            if (shouldReSelectCurrentItem && safePointer->onReselectedCurrentItem != nullptr)
                                                safePointer->onReselectedCurrentItem();

                                            if (auto* handler = safePointer->getAccessibilityHandler())
                                                handler->grabFocus();
                                        });
                return;
            }
        }
    }

    auto menu = *getRootMenu();

    if (menu.getNumItems() == 0)
        menu.addItem(1, getTextWhenNoChoicesAvailable(), false, false);

    auto* label = dynamic_cast<juce::Label*>(getChildComponent(0));

    if (label == nullptr)
    {
        juce::ComboBox::showPopup();
        return;
    }

    auto& lf = getLookAndFeel();
    menu.setLookAndFeel(&lf);
    const auto currentSelectedId = getSelectedId();
    menu.showMenuAsync(lf.getOptionsForComboBoxPopupMenu(*this, *label),
                       juce::ModalCallbackFunction::create(
                           [safePointer = juce::Component::SafePointer<NoTickComboBox>(this), currentSelectedId] (int result)
                           {
                               if (safePointer == nullptr)
                                   return;

                               safePointer->hidePopup();

                               if (result != 0)
                               {
                                   const auto shouldReSelectCurrentItem = result == currentSelectedId;
                                   safePointer->setSelectedId(result);

                                   if (shouldReSelectCurrentItem && safePointer->onReselectedCurrentItem != nullptr)
                                       safePointer->onReselectedCurrentItem();
                               }

                               if (auto* handler = safePointer->getAccessibilityHandler())
                                   handler->grabFocus();
                           }));
}

void NoTickComboBox::mouseDown(const juce::MouseEvent& event)
{
    if (! isEnabled())
        return;

    pointerDown = true;
    dragDetected = false;
    pressHighlight = true;
    repaint();

    if (event.mods.isPopupMenu())
    {
        showPopup();
        pointerDown = false;
        pressHighlight = false;
        return;
    }
}

void NoTickComboBox::mouseDrag(const juce::MouseEvent& event)
{
    if (pointerDown && ! dragDetected && event.mouseWasDraggedSinceMouseDown())
    {
        dragDetected = true;

        if (pressHighlight)
        {
            pressHighlight = false;
            repaint();
        }
    }
}

void NoTickComboBox::mouseUp(const juce::MouseEvent& event)
{
    if (! isEnabled())
        return;

    const auto shouldOpenPopup = pointerDown
        && ! dragDetected
        && ! event.mouseWasDraggedSinceMouseDown()
        && ! event.mods.isPopupMenu()
        && contains(event.getPosition());

    pointerDown = false;
    dragDetected = false;
    pressHighlight = false;
    repaint();

    if (shouldOpenPopup)
        showPopup();
}

void NoTickComboBox::mouseExit(const juce::MouseEvent&)
{
    if (! pointerDown || ! pressHighlight)
        return;

    pressHighlight = false;
    repaint();
}

void BoxTextButton::setCancelClickOnLeave(const bool shouldEnable) noexcept
{
    cancelClickOnLeave = shouldEnable;
}

void NoTickComboBox::setPopupMenuTextJustification(const juce::Justification justification) noexcept
{
    popupMenuTextJustification = justification;
}

juce::Justification NoTickComboBox::getPopupMenuTextJustification() const noexcept
{
    return popupMenuTextJustification;
}

void NoTickComboBox::setPromptStylePopupEnabled(const bool shouldEnable) noexcept
{
    promptStylePopupEnabled = shouldEnable;
}

bool NoTickComboBox::isPromptStylePopupEnabled() const noexcept
{
    return promptStylePopupEnabled;
}

void NoTickComboBox::setChoiceEnabled(const int choiceIndex, const bool shouldEnable)
{
    if (choiceIndex < 0)
        return;

    const auto itemIndex = choiceIndex + 1;

    if (itemIndex <= 0 || itemIndex > getNumItems())
        return;

    setItemEnabled(itemIndex, shouldEnable);
}

bool NoTickComboBox::isPressedHighlightEnabled() const noexcept
{
    return pressHighlight;
}

ValueBoxComponent::ValueBoxComponent(juce::Slider& sliderToControl, juce::RangedAudioParameter* parameterToControl)
    : slider(sliderToControl),
      parameter(parameterToControl)
{
    setMouseCursor(juce::MouseCursor::IBeamCursor);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

ValueBoxComponent::~ValueBoxComponent()
{
    stopGlobalEditTracking();
}

void ValueBoxComponent::setInteractionEnabled(const bool shouldEnable)
{
    if (interactionEnabled == shouldEnable)
        return;

    interactionEnabled = shouldEnable;
    setMouseCursor(interactionEnabled ? juce::MouseCursor::IBeamCursor
                                      : juce::MouseCursor::NormalCursor);

    if (! interactionEnabled && editor != nullptr)
        hideEditor(true);
}

void ValueBoxComponent::setOutlineColour(const juce::Colour colour)
{
    if (outlineColour == colour)
        return;

    outlineColour = colour;

    if (editor != nullptr)
        editor->setColour(juce::TextEditor::outlineColourId, outlineColour);

    repaint();
}

void ValueBoxComponent::setHighlightColour(const juce::Colour colour)
{
    if (highlightColour == colour)
        return;

    highlightColour = colour;

    if (editor != nullptr)
        editor->setColour(juce::TextEditor::highlightColourId, highlightColour);
}

void ValueBoxComponent::setPromptActive(const bool shouldBeActive)
{
    if (promptActive == shouldBeActive)
        return;

    promptActive = shouldBeActive;

    if (! shouldBeActive)
    {
        pressHighlight = false;
        pointerDown = false;
        dragDetected = false;
    }

    repaint();
}

void ValueBoxComponent::paint(juce::Graphics& g)
{
    const auto displayText = displayTextProvider != nullptr ? displayTextProvider()
                                                            : slider.getTextFromValue(slider.getValue());
    auto backgroundColour = uiGrey800;
    auto borderColour = outlineColour;

    if (promptActive)
    {
        backgroundColour = uiGrey700;
        borderColour = uiAccent;
    }
    else if (pressHighlight)
    {
        backgroundColour = uiGrey700;
    }

    g.setColour(backgroundColour);
    g.fillRect(getLocalBounds());

    g.setColour(borderColour);
    g.drawRect(getLocalBounds(), 1);

    g.setColour(getDisplayTextColour(displayText));
    g.setFont(makeUiFont());
    g.drawFittedText(displayText,
                     getLocalBounds().reduced(4, 0),
                     juce::Justification::centred,
                     1,
                     1.0f);
}

void ValueBoxComponent::resized()
{
    if (editor != nullptr)
        editor->setBounds(getLocalBounds());
}

void ValueBoxComponent::mouseDown(const juce::MouseEvent& event)
{
    auto* clickedComponent = event.originalComponent;
    const auto clickIsInsideThisValueBox = clickedComponent != nullptr
        && (clickedComponent == this || isParentOf(clickedComponent));

    if (editor != nullptr && ! clickIsInsideThisValueBox)
    {
        hideEditor(false);
        return;
    }

    if (! clickIsInsideThisValueBox)
        return;

    if (! interactionEnabled || ! event.mods.isLeftButtonDown())
        return;

    pointerDown = true;
    dragDetected = false;
    pressHighlight = true;
    repaint();
}

void ValueBoxComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (! pointerDown)
        return;

    if (! dragDetected && event.mouseWasDraggedSinceMouseDown())
        dragDetected = true;

    const auto shouldHighlight = contains(event.getPosition());

    if (pressHighlight != shouldHighlight)
    {
        pressHighlight = shouldHighlight;
        repaint();
    }
}

void ValueBoxComponent::mouseUp(const juce::MouseEvent& event)
{
    if (! pointerDown)
        return;

    const auto shouldOpenPrompt = contains(event.getPosition())
        && ! event.mods.isPopupMenu();

    pointerDown = false;
    dragDetected = false;

    if (shouldOpenPrompt)
        showEditor();

    pressHighlight = false;
    repaint();
}

void ValueBoxComponent::mouseExit(const juce::MouseEvent&)
{
    if (! pointerDown)
        return;

    if (pressHighlight)
    {
        pressHighlight = false;
        repaint();
    }
}

void ValueBoxComponent::showEditor()
{
    if (editor != nullptr)
        return;

    if (auto* owner = findParentComponentOfClass<EqeAudioProcessorEditor>())
    {
        const auto editorText = editorTextProvider != nullptr ? editorTextProvider()
                                                              : slider.getTextFromValue(slider.getValue());
        auto safeThis = juce::Component::SafePointer<ValueBoxComponent>(this);

        setPromptActive(true);
        pressHighlight = false;
        repaint();

        owner->showTextPrompt(editorText,
                              [safeThis] (const juce::String& enteredText)
                              {
                                  if (safeThis == nullptr)
                                      return false;

                                  safeThis->applyEnteredText(enteredText);
                                  return true;
                              },
                              [safeThis]
                              {
                                  if (safeThis != nullptr)
                                      safeThis->setPromptActive(false);
                              },
                              [safeThis]
                              {
                                  if (safeThis != nullptr)
                                      safeThis->setPromptActive(false);
                              });
        return;
    }

    const auto editorText = editorTextProvider != nullptr ? editorTextProvider()
                                                          : slider.getTextFromValue(slider.getValue());
    auto textEditor = std::make_unique<CopyPasteTextEditor>();
    textEditor->setName(slider.getName());
    textEditor->setFont(makeUiFont());
    textEditor->setPopupMenuEnabled(true);
    textEditor->setJustification(juce::Justification::centred);
    textEditor->setColour(juce::TextEditor::textColourId, uiWhite);
    textEditor->setColour(juce::TextEditor::backgroundColourId, uiGrey800);
    textEditor->setColour(juce::TextEditor::outlineColourId, outlineColour);
    textEditor->setColour(juce::TextEditor::focusedOutlineColourId, outlineColour);
    textEditor->setColour(juce::TextEditor::highlightColourId, highlightColour);
    textEditor->setText(editorText, false);
    textEditor->onReturnKey = [this] { hideEditor(false); };
    textEditor->onEscapeKey = [this] { hideEditor(true); };
    textEditor->onFocusLost = [this] { hideEditor(false); };

    addAndMakeVisible(*textEditor);
    editor = std::move(textEditor);
    startGlobalEditTracking();
    resized();
    editor->grabKeyboardFocus();
    editor->selectAll();
}

void ValueBoxComponent::applyEnteredText(const juce::String& enteredText)
{
    const auto enteredValue = textToValueParser != nullptr
        ? textToValueParser(enteredText)
        : slider.getValueFromText(enteredText);
    const auto clampedValue = juce::jlimit(static_cast<double>(slider.getMinimum()),
                                           static_cast<double>(slider.getMaximum()),
                                           enteredValue);

    if (parameter != nullptr)
    {
        parameter->beginChangeGesture();
        slider.setValue(clampedValue, juce::sendNotificationSync);
        parameter->endChangeGesture();
    }
    else
    {
        slider.setValue(clampedValue, juce::sendNotificationSync);
    }

    if (auto* parent = getParentComponent())
        parent->repaint();

    repaint();
}

void ValueBoxComponent::hideEditor(const bool discard)
{
    if (editor == nullptr)
        return;

    if (! discard)
    {
        const auto enteredText = editor->getText().trim();
        const auto enteredValue = textToValueParser != nullptr
            ? textToValueParser(enteredText)
            : slider.getValueFromText(enteredText);
        const auto clampedValue = juce::jlimit(static_cast<double>(slider.getMinimum()),
                                               static_cast<double>(slider.getMaximum()),
                                               enteredValue);

        if (parameter != nullptr)
        {
            parameter->beginChangeGesture();
            slider.setValue(clampedValue, juce::sendNotificationSync);
            parameter->endChangeGesture();
        }
        else
        {
            slider.setValue(clampedValue, juce::sendNotificationSync);
        }
    }

    removeChildComponent(editor.get());
    editor.reset();
    stopGlobalEditTracking();
    clearKeyboardFocus(*this);
    repaint();
}

void ValueBoxComponent::startGlobalEditTracking()
{
    if (isTrackingGlobalClicks)
        return;

    juce::Desktop::getInstance().addGlobalMouseListener(this);
    isTrackingGlobalClicks = true;
}

void ValueBoxComponent::stopGlobalEditTracking()
{
    if (! isTrackingGlobalClicks)
        return;

    juce::Desktop::getInstance().removeGlobalMouseListener(this);
    isTrackingGlobalClicks = false;
}

BoxTextButton::BoxTextButton(const juce::Colour accent)
    : accentColour(accent)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

BoxTextButton::~BoxTextButton()
{
    stopTimer();
}

void BoxTextButton::setManualInteractionEnabled(const bool shouldEnable) noexcept
{
    manualInteractionEnabled = shouldEnable;
}

void BoxTextButton::setAlwaysAccentOutline(const bool shouldAlwaysAccent)
{
    if (alwaysAccentOutline == shouldAlwaysAccent)
        return;

    alwaysAccentOutline = shouldAlwaysAccent;
    repaint();
}

void BoxTextButton::setTextJustification(const juce::Justification justification) noexcept
{
    textJustification = justification;
    repaint();
}

void BoxTextButton::setArrowDirection(const ArrowDirection direction) noexcept
{
    arrowDirection = direction;
    repaint();
}

void BoxTextButton::setBorderVisible(const bool shouldShow) noexcept
{
    if (borderVisible == shouldShow)
        return;

    borderVisible = shouldShow;
    repaint();
}

void BoxTextButton::setLeadingDot(const juce::Colour colour, const float level)
{
    leadingDotVisible = true;
    leadingDotColour = colour;
    setLeadingDotLevel(level);
}

void BoxTextButton::setLeadingDotLevel(const float level)
{
    const auto clampedLevel = juce::jlimit(0.0f, 1.0f, level);

    if (std::abs(leadingDotLevel - clampedLevel) < 1.0e-6f)
        return;

    leadingDotLevel = clampedLevel;
    repaint();
}

void BoxTextButton::setLongPressAction(std::function<void()> action, const int delayMs, juce::String promptText)
{
    longPressAction = std::move(action);
    longPressDelayMs = juce::jmax(1, delayMs);
    longPressPromptText = std::move(promptText);
}

void BoxTextButton::paintButton(juce::Graphics& graphics, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto buttonDown = manualInteractionEnabled ? manualPointerDown
                                                     : pressHighlight;
    const auto accentActive = alwaysAccentOutline || getToggleState();
    const auto fill = buttonDown ? uiGrey700 : uiGrey800;
    const auto outline = accentActive ? accentColour : uiGrey500;

    graphics.setColour(fill);
    graphics.fillRect(getLocalBounds());

    if (borderVisible)
    {
        graphics.setColour(outline);
        graphics.drawRect(getLocalBounds(), 1);
    }

    graphics.setColour(uiWhite);
    if (arrowDirection == ArrowDirection::none)
    {
        const auto font = makeUiFont();
        graphics.setFont(font);

        if (! leadingDotVisible || getButtonText().isEmpty())
        {
            graphics.drawFittedText(getButtonText(), getLocalBounds().reduced(6, 0), textJustification, 1, 1.0f);
            return;
        }

        const auto textBounds = getLocalBounds().toFloat().reduced(6.0f, 0.0f);
        const auto dotDiameter = 6.0f;
        const auto dotGap = 7.0f;
        const auto textWidth = static_cast<float>(getTextPixelWidth(font, getButtonText()));
        const auto groupWidth = dotDiameter + dotGap + textWidth;
        const auto horizontalFlags = textJustification.getOnlyHorizontalFlags();
        auto groupLeft = textBounds.getX();

        if ((horizontalFlags & juce::Justification::horizontallyCentred) != 0)
            groupLeft = textBounds.getCentreX() - (groupWidth * 0.5f);
        else if ((horizontalFlags & juce::Justification::right) != 0)
            groupLeft = textBounds.getRight() - groupWidth;

        const auto dotBounds = juce::Rectangle<float>(groupLeft,
                                                      textBounds.getCentreY() - dotDiameter * 0.5f,
                                                      dotDiameter,
                                                      dotDiameter);

        graphics.setColour(leadingDotColour.withAlpha(leadingDotLevel > 0.5f ? 1.0f : 0.25f));
        graphics.fillEllipse(dotBounds);

        graphics.setColour(uiWhite);
        graphics.drawFittedText(getButtonText(),
                                juce::Rectangle<int>(juce::roundToInt(groupLeft + dotDiameter + dotGap),
                                                     getLocalBounds().getY(),
                                                     juce::roundToInt(textWidth) + 2,
                                                     getLocalBounds().getHeight()),
                                juce::Justification::centredLeft,
                                1,
                                1.0f);
        return;
    }

    auto arrowBounds = getLocalBounds().toFloat().reduced(10.0f, 8.0f);

    if (arrowBounds.getWidth() <= 0.0f || arrowBounds.getHeight() <= 0.0f)
        return;

    const auto centreX = arrowBounds.getCentreX();
    const auto top = arrowBounds.getY();
    const auto bottom = arrowBounds.getBottom();
    const auto headWidth = juce::jmax(3.0f, arrowBounds.getWidth() * 0.35f);
    const auto headHeight = juce::jmax(3.0f, arrowBounds.getHeight() * 0.35f);

    juce::Path arrowPath;

    if (arrowDirection == ArrowDirection::up)
    {
        arrowPath.startNewSubPath(centreX, bottom);
        arrowPath.lineTo(centreX, top + headHeight);
        arrowPath.startNewSubPath(centreX - headWidth, top + headHeight);
        arrowPath.lineTo(centreX, top);
        arrowPath.lineTo(centreX + headWidth, top + headHeight);
    }
    else
    {
        arrowPath.startNewSubPath(centreX, top);
        arrowPath.lineTo(centreX, bottom - headHeight);
        arrowPath.startNewSubPath(centreX - headWidth, bottom - headHeight);
        arrowPath.lineTo(centreX, bottom);
        arrowPath.lineTo(centreX + headWidth, bottom - headHeight);
    }

    graphics.strokePath(arrowPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void BoxTextButton::mouseDown(const juce::MouseEvent& event)
{
    if (! manualInteractionEnabled)
    {
        if (event.mods.isPopupMenu() || ! event.mods.isLeftButtonDown())
            return;

        pointerDown = true;
        dragActive = false;
        pressCanceled = false;
        pressHighlight = true;
        longPressEligible = longPressAction != nullptr;
        longPressArmed = false;
        longPressOriginalText = getButtonText();
        if (longPressEligible)
            startTimer(longPressDelayMs);
        else
            stopTimer();
        repaint();
        return;
    }

    manualPointerDown = true;
    manualDragActive = false;
    pressHighlight = true;
    repaint();
}

void BoxTextButton::mouseDrag(const juce::MouseEvent& event)
{
    if (! manualInteractionEnabled)
    {
        if (! pointerDown)
            return;

        if (cancelClickOnLeave && ! pressCanceled && ! contains(event.getPosition()))
        {
            pressCanceled = true;
            pressHighlight = false;
            longPressEligible = false;
            stopTimer();
            repaint();
        }

        if (! dragActive && event.getDistanceFromDragStart() >= 4)
        {
            dragActive = true;
            longPressEligible = false;
            stopTimer();
        }

        const auto shouldHighlight = contains(event.getPosition());

        if (pressHighlight != shouldHighlight)
        {
            pressHighlight = shouldHighlight;
            repaint();
        }

        if (! shouldHighlight)
        {
            longPressEligible = false;
            stopTimer();
        }

        return;
    }

    if (! manualDragActive && event.getDistanceFromDragStart() >= 4)
    {
        manualDragActive = true;

        if (onDragBegin)
            onDragBegin(event);
    }

    if (manualDragActive && onDragMove)
        onDragMove(event);
}

void BoxTextButton::mouseUp(const juce::MouseEvent& event)
{
    if (! manualInteractionEnabled)
    {
        stopTimer();

        const auto wasLongPressArmed = longPressArmed;
        const auto wasPressCanceled = pressCanceled;
        pointerDown = false;
        dragActive = false;
        pressHighlight = false;
        longPressEligible = false;
        longPressArmed = false;
        pressCanceled = false;
        repaint();

        if (wasLongPressArmed)
        {
            if (getButtonText() != longPressOriginalText)
                setButtonText(longPressOriginalText);

            repaint();

            if (! wasPressCanceled && contains(event.getPosition()) && longPressAction != nullptr)
                longPressAction();

            return;
        }

        if (wasPressCanceled)
            return;

        if (contains(event.getPosition()))
            triggerClick();

        return;
    }

    const auto wasManualDrag = manualDragActive;
    manualPointerDown = false;
    manualDragActive = false;
    pressHighlight = false;
    repaint();

    if (wasManualDrag)
    {
        if (onDragFinish)
            onDragFinish(event);
    }
    else if (contains(event.getPosition()) && onPressed)
    {
        onPressed();
    }
}

void BoxTextButton::mouseExit(const juce::MouseEvent&)
{
    if (manualInteractionEnabled)
    {
        if (! manualPointerDown)
            return;

        manualPointerDown = false;
        repaint();
        return;
    }

    if (! pointerDown || ! pressHighlight)
        return;

    if (cancelClickOnLeave)
    {
        pressCanceled = true;
        longPressEligible = false;
        stopTimer();
    }

    pressHighlight = false;
    if (! longPressArmed)
    {
        longPressEligible = false;
        stopTimer();
    }
    repaint();
}

void BoxTextButton::timerCallback()
{
    stopTimer();

    if (! pointerDown || ! pressHighlight || ! longPressEligible || dragActive || manualInteractionEnabled)
        return;

    longPressEligible = false;
    longPressArmed = true;
    longPressOriginalText = getButtonText();
    setButtonText(longPressPromptText);
    repaint();
}

EqeAudioProcessorEditor::EqeLookAndFeel::EqeLookAndFeel()
{
    if (auto typeface = getUiRegularTypeface())
        setDefaultSansSerifTypeface(typeface);
}

juce::Typeface::Ptr EqeAudioProcessorEditor::EqeLookAndFeel::getTypefaceForFont(const juce::Font& font)
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

juce::Font EqeAudioProcessorEditor::EqeLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return makeUiFont();
}

juce::Font EqeAudioProcessorEditor::EqeLookAndFeel::getPopupMenuFont()
{
    return makeUiFont();
}

void EqeAudioProcessorEditor::EqeLookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                                                                 int width,
                                                                                 int height,
                                                                                 const juce::PopupMenu::Options&)
{
    g.setColour(uiGrey800);
    g.fillRect(0, 0, width, height);

    g.setColour(uiAccent);
    g.drawRect(0, 0, width, height, 1);
}

int EqeAudioProcessorEditor::EqeLookAndFeel::getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options&)
{
    return 1;
}

void EqeAudioProcessorEditor::EqeLookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& text,
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

void EqeAudioProcessorEditor::EqeLookAndFeel::drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour(uiGrey700);
    g.fillPath(path);

    g.setColour(uiGrey500);
    g.strokePath(path, juce::PathStrokeType(1.0f));
}

int EqeAudioProcessorEditor::EqeLookAndFeel::getCallOutBoxBorderSize(const juce::CallOutBox&)
{
    return 1;
}

float EqeAudioProcessorEditor::EqeLookAndFeel::getCallOutBoxCornerSize(const juce::CallOutBox&)
{
    return 0.0f;
}

void EqeAudioProcessorEditor::EqeLookAndFeel::drawComboBox(juce::Graphics& g,
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

void EqeAudioProcessorEditor::EqeLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
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

void EqeAudioProcessorEditor::EqeLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
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

void EqeAudioProcessorEditor::EqeLookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g,
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
