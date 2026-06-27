#include "shell.ShellState.h"

#include "shell.Processor.h"

#include <array>

namespace
{
const std::array<juce::Identifier, 12> editorWindowAndVisualizerStateProperties
{
    VxAudioProcessor::editorWidthStateKey,
    VxAudioProcessor::editorHeightStateKey,
    "editor_visualizer_expanded",
    "editor_visualizer_range_low",
    "editor_visualizer_range_high",
    "editor_visualizer_cursor_enabled",
    "editor_visualizer_show_stereo",
    "editor_visualizer_show_left",
    "editor_visualizer_show_right",
    "editor_visualizer_show_mid",
    "editor_visualizer_show_side",
    "editor_last_collapsed_width"
};

}

void preserveEditorWindowAndVisualizerState(juce::XmlElement& targetStateElement,
                                            const juce::ValueTree& sourceState)
{
    for (const auto& propertyId : editorWindowAndVisualizerStateProperties)
    {
        if (sourceState.hasProperty(propertyId))
            targetStateElement.setAttribute(propertyId.toString(), sourceState.getProperty(propertyId).toString());
        else
            targetStateElement.removeAttribute(propertyId.toString());
    }
}
