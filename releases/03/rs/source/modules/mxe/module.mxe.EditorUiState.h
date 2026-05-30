#pragma once

#include <JuceHeader.h>

#include <cstddef>

namespace mxe::editor::uiState
{
inline const juce::Identifier& autoSoloEnabledId()
{
    static const juce::Identifier id { "mxe.editor.autoSoloEnabled" };
    return id;
}

inline const juce::Identifier& allBandsActiveId()
{
    static const juce::Identifier id { "mxe.editor.allBandsActive" };
    return id;
}

inline const juce::Identifier& visibleBandIndexId()
{
    static const juce::Identifier id { "mxe.editor.visibleBandIndex" };
    return id;
}

inline const juce::Identifier& hasUiStateId()
{
    static const juce::Identifier id { "mxe.editor.hasUiState" };
    return id;
}

inline juce::String makeBandSectionExpandedStateKey(const size_t bandIndex, const size_t sectionIndex)
{
    return juce::String("mxe.editor.bandSection.")
        + juce::String(static_cast<int>(bandIndex))
        + "."
        + juce::String(static_cast<int>(sectionIndex));
}

inline juce::String makeFullbandSectionExpandedStateKey(const size_t sectionIndex)
{
    return juce::String("mxe.editor.fullbandSection.")
        + juce::String(static_cast<int>(sectionIndex));
}

inline juce::Identifier makeManualSoloId(const size_t bandIndex)
{
    return juce::Identifier { juce::String("mxe.editor.manualSolo.") + juce::String(static_cast<int>(bandIndex)) };
}

inline bool getBool(const juce::ValueTree& state,
                    const juce::Identifier& id,
                    const bool defaultValue)
{
    return state.hasProperty(id) ? static_cast<bool>(state.getProperty(id)) : defaultValue;
}

inline int getInt(const juce::ValueTree& state,
                  const juce::Identifier& id,
                  const int defaultValue)
{
    return state.hasProperty(id) ? static_cast<int>(state.getProperty(id)) : defaultValue;
}

inline void setBool(juce::ValueTree& state,
                    const juce::Identifier& id,
                    const bool value)
{
    state.setProperty(id, value, nullptr);
}

inline void setInt(juce::ValueTree& state,
                   const juce::Identifier& id,
                   const int value)
{
    state.setProperty(id, value, nullptr);
}
} // namespace mxe::editor::uiState
