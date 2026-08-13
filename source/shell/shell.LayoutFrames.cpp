#include "shell.EditorFilterSection.h"
#include "shell.UiConstants.h"
#include "shell.EditorPresetSections.h"

void VxAudioProcessorEditor::layoutNoModuleState(juce::Rectangle<int>& bounds)
{
    if (moduleAddButton == nullptr || ! moduleAddButton->isVisible())
        return;

    const auto footerWidth = footerTab != nullptr && ! footerTab->getBounds().isEmpty()
        ? footerTab->getWidth()
        : bounds.getWidth();
    const auto buttonWidth = juce::jmin(bounds.getWidth(), footerWidth);
    auto buttonBounds = juce::Rectangle<int>(buttonWidth, rowHeight);
    buttonBounds.setCentre(bounds.getCentre());
    moduleAddButton->setBounds(buttonBounds);
}

void VxAudioProcessorEditor::layoutModuleEditorContent(juce::Rectangle<int>& bounds)
{
    auto contentBounds = bounds;
    contentBounds.removeFromBottom(viewportToPotentiometerGap);

    if (tlsModuleEditor != nullptr)
    {
        tlsModuleEditor->setBounds(contentBounds);
        tlsModuleEditor->setVisible(tlsModuleLoaded);
    }

    if (dynModuleEditor != nullptr)
    {
        dynModuleEditor->setBounds(contentBounds);
        dynModuleEditor->setVisible(dynModuleLoaded);
    }

    if (trsModuleEditor != nullptr)
    {
        trsModuleEditor->setBounds(contentBounds);
        trsModuleEditor->setVisible(trsModuleLoaded);
    }

}
