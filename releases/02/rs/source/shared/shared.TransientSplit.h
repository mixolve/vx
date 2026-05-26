#pragma once

#include <JuceHeader.h>

struct TransientSplitSettings
{
    float holdMs = 0.0f;
    float releaseMs = 10.0f;
    float releaseCurve = 0.0f;
    float thresholdDb = -42.0f;
    float kneeDb = 0.0f;
    float retriggerMs = 100.0f;
    float lookaheadMs = 1.0f;
};

class TransientSplitProvider
{
public:
    virtual ~TransientSplitProvider() = default;

    virtual bool canProvideSplit() const noexcept = 0;
    virtual TransientSplitSettings getSplitSettings() const noexcept = 0;
};
