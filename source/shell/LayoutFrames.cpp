#include "EditorFilterSection.h"
#include "../crossover/ModuleComponent.h"
#include "UiConstants.h"
#include "EditorPresetSections.h"

void AvaAudioProcessorEditor::layoutNoModuleState(juce::Rectangle<int>& bounds)
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

void AvaAudioProcessorEditor::layoutCrossoverSection(juce::Rectangle<int>& bounds)
{
    auto* editor = dynamic_cast<CrossoverModuleComponent*>(crossoverEditor.get());

    if (editor == nullptr)
        return;

    const auto reservedModuleButtonHeight = moduleAddButton != nullptr && moduleAddButton->isVisible()
        ? rowHeight + verticalGap
        : 0;
    const auto reservedPotentiometerGap = editor->isCrossoverSettingsSelected()
        ? viewportToPotentiometerGap
        : 0;
    const auto availableHeight = juce::jmax(0,
                                            bounds.getHeight()
                                                - reservedModuleButtonHeight
                                                - reservedPotentiometerGap);
    const auto sectionHeight = juce::jmin(availableHeight, editor->getPreferredHeight());
    editor->setBounds(bounds.removeFromTop(sectionHeight));
    editor->setVisible(sectionHeight > 0);

    if (! bounds.isEmpty())
        bounds.removeFromTop(verticalGap);
}

void AvaAudioProcessorEditor::layoutModuleEditorContent(juce::Rectangle<int>& bounds)
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
