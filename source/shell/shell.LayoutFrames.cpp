#include "shell.EditorBellSection.h"
#include "shell.Constants.h"
#include "shell.EditorPresetSections.h"
#include "../modules/mxe/module.mxe.ModuleComponent.h"
#include "module.tse.Component.h"

void VxAudioProcessorEditor::includeBounds(juce::Rectangle<int>& sectionBounds,
                                           const juce::Rectangle<int>& boundsToInclude) const
{
    if (boundsToInclude.isEmpty())
        return;

    sectionBounds = sectionBounds.isEmpty() ? boundsToInclude
                                           : sectionBounds.getUnion(boundsToInclude);
}

void VxAudioProcessorEditor::includeComponentBounds(juce::Rectangle<int>& sectionBounds,
                                                    const juce::Component* component) const
{
    if (component == nullptr || ! component->isVisible() || component->getBounds().isEmpty())
        return;

    includeBounds(sectionBounds, component->getBounds());
}

namespace
{
void placeFrameWithInsets(juce::Component* frame,
                          const bool shouldShow,
                          juce::Rectangle<int> sectionBounds,
                          const juce::Rectangle<int>& editorBounds,
                          const int insetX,
                          const int insetY)
{
    if (frame == nullptr)
        return;

    frame->setVisible(shouldShow && ! sectionBounds.isEmpty());

    if (! frame->isVisible())
    {
        frame->setBounds({});
        return;
    }

    frame->setBounds(sectionBounds.expanded(insetX, insetY).getIntersection(editorBounds));
    frame->toFront(false);
}
}

void VxAudioProcessorEditor::placeSectionFrame(juce::Component* frame,
                                               const bool shouldShow,
                                               juce::Rectangle<int> sectionBounds) const
{
    placeFrameWithInsets(frame, shouldShow, sectionBounds, getLocalBounds(), internalFrameInsetX, internalFrameInsetY);
}

void VxAudioProcessorEditor::placeModuleFrame(juce::Component* frame,
                                              const bool shouldShow,
                                              juce::Rectangle<int> sectionBounds) const
{
    placeFrameWithInsets(frame, shouldShow, sectionBounds, getLocalBounds(), moduleFrameInsetX, moduleFrameInsetY);
}

juce::Rectangle<int> VxAudioProcessorEditor::buildShellGlobalFrameBounds() const
{
    juce::Rectangle<int> moduleFrameBounds;
    includeComponentBounds(moduleFrameBounds, shellGlobalHeader.get());
    includeComponentBounds(moduleFrameBounds, clipControl.get());
    includeComponentBounds(moduleFrameBounds, outGainControl.get());
    includeComponentBounds(moduleFrameBounds, gainLControl.get());
    includeComponentBounds(moduleFrameBounds, gainRControl.get());
    includeComponentBounds(moduleFrameBounds, wideControl.get());
    includeComponentBounds(moduleFrameBounds, globalBypassButton.get());
    includeComponentBounds(moduleFrameBounds, globalBypassOutGainOnlyButton.get());
    includeComponentBounds(moduleFrameBounds, undoButton.get());
    includeComponentBounds(moduleFrameBounds, redoButton.get());
    includeComponentBounds(moduleFrameBounds, moduleAddButton.get());
    return moduleFrameBounds;
}

void VxAudioProcessorEditor::includeModuleTabRowBounds(juce::Rectangle<int>& sectionBounds) const
{
    for (const auto& row : moduleTabRows)
    {
        if (row == nullptr)
            continue;

        includeComponentBounds(sectionBounds, row->moveUpButton.get());
        includeComponentBounds(sectionBounds, row->tabButton.get());
        includeComponentBounds(sectionBounds, row->moveDownButton.get());
    }
}

void VxAudioProcessorEditor::layoutNoModuleState(juce::Rectangle<int>&)
{
    placeModuleFrame(eqeModuleFrame.get(), shellGlobalExpanded, buildShellGlobalFrameBounds());
}

void VxAudioProcessorEditor::layoutCollapsedModuleState()
{
    placeModuleFrame(eqeModuleFrame.get(), shellGlobalExpanded, buildShellGlobalFrameBounds());
}

void VxAudioProcessorEditor::layoutModuleEditorContent(juce::Rectangle<int>& bounds)
{
    auto contentBounds = bounds;
    contentBounds.removeFromBottom(addFilterToFooterGap);

    if (mxeModuleEditor != nullptr)
    {
        mxeModuleEditor->setBounds(contentBounds);
        mxeModuleEditor->setVisible(mxeModuleLoaded);
    }

    if (tseModuleEditor != nullptr)
    {
        tseModuleEditor->setBounds(contentBounds);
        tseModuleEditor->setVisible(tseModuleLoaded);
    }

    auto moduleFrameBounds = shellGlobalExpanded ? buildShellGlobalFrameBounds()
                                                 : juce::Rectangle<int>();

    if (! shellGlobalExpanded)
    {
        includeModuleTabRowBounds(moduleFrameBounds);

        if (auto* mxeComponent = dynamic_cast<MxeModuleComponent*>(mxeModuleEditor.get());
            mxeComponent != nullptr && mxeModuleLoaded)
            includeBounds(moduleFrameBounds, mxeComponent->getContentBounds().translated(mxeComponent->getX(), mxeComponent->getY()));
        else if (auto* tseComponent = dynamic_cast<TseModuleComponent*>(tseModuleEditor.get());
                 tseComponent != nullptr && tseModuleLoaded)
            includeBounds(moduleFrameBounds, tseComponent->getContentBounds().translated(tseComponent->getX(), tseComponent->getY()));
        else if (mxeModuleLoaded)
            includeComponentBounds(moduleFrameBounds, mxeModuleEditor.get());
        else
            includeComponentBounds(moduleFrameBounds, tseModuleEditor.get());
    }

    placeModuleFrame(eqeModuleFrame.get(), true, moduleFrameBounds);
}
