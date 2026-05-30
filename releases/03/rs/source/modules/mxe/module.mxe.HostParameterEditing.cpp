#include "module.mxe.HostParameterEditing.h"

#include <cmath>

namespace mxe::editor
{
bool setNormalisedParameterValueForHost(juce::RangedAudioParameter& parameter,
                                        const float normalisedValue,
                                        juce::UndoManager* const undoManager)
{
    const auto value = juce::jlimit(0.0f, 1.0f, normalisedValue);
    const auto wasDifferent = std::abs(parameter.getValue() - value) > 1.0e-6f;

    if (undoManager != nullptr)
        undoManager->beginNewTransaction();

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost(value);
    parameter.endChangeGesture();

    return wasDifferent;
}
} // namespace mxe::editor
