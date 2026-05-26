#include "shell.ShellState.h"

#include "shell.Processor.h"

#include <array>

namespace
{
const std::array<juce::Identifier, 15>& editorWindowAndVisualizerStateProperties()
{
    static const std::array<juce::Identifier, 15> properties
    {
        VxAudioProcessor::editorWidthStateKey,
        VxAudioProcessor::editorHeightStateKey,
        "editor_eqe_module_expanded",
        "editor_visualizer_expanded",
        "editor_visualizer_visible",
        "editor_visualizer_range_low",
        "editor_visualizer_range_high",
        "editor_visualizer_cursor_enabled",
        "editor_visualizer_show_stereo",
        "editor_visualizer_show_left",
        "editor_visualizer_show_right",
        "editor_visualizer_show_mid",
        "editor_visualizer_show_side",
        "editor_last_collapsed_width",
        "editor_last_visualizer_width"
    };

    return properties;
}

void copyStateProperty(juce::XmlElement& targetStateElement,
                       const juce::ValueTree& sourceState,
                       const juce::Identifier& propertyId)
{
    if (sourceState.hasProperty(propertyId))
        targetStateElement.setAttribute(propertyId.toString(), sourceState.getProperty(propertyId).toString());
    else
        targetStateElement.removeAttribute(propertyId.toString());
}

void removeStateAttribute(juce::XmlElement& stateElement, const juce::Identifier& propertyId)
{
    stateElement.removeAttribute(propertyId.toString());
}
}

void preserveEditorWindowAndVisualizerState(juce::XmlElement& targetStateElement,
                                            const juce::ValueTree& sourceState)
{
    for (const auto& propertyId : editorWindowAndVisualizerStateProperties())
        copyStateProperty(targetStateElement, sourceState, propertyId);
}

void stripEditorWindowAndVisualizerState(juce::XmlElement& stateElement)
{
    for (const auto& propertyId : editorWindowAndVisualizerStateProperties())
        removeStateAttribute(stateElement, propertyId);
}

void stripEditorWindowAndVisualizerState(juce::ValueTree& state)
{
    for (const auto& propertyId : editorWindowAndVisualizerStateProperties())
        state.removeProperty(propertyId, nullptr);
}
