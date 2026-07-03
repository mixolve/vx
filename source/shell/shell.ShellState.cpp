#include "shell.ShellState.h"

#include "shell.Processor.h"

#include <array>

namespace
{
const std::array<juce::Identifier, 2> editorWindowStateProperties
{
    VxAudioProcessor::editorWidthStateKey,
    VxAudioProcessor::editorHeightStateKey
};

}

void preserveEditorWindowState(juce::XmlElement& targetStateElement,
                               const juce::ValueTree& sourceState)
{
    for (const auto& propertyId : editorWindowStateProperties)
    {
        if (sourceState.hasProperty(propertyId))
            targetStateElement.setAttribute(propertyId.toString(), sourceState.getProperty(propertyId).toString());
        else
            targetStateElement.removeAttribute(propertyId.toString());
    }
}
