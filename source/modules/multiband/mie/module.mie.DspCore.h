#pragma once

#include "module.mie.DspSupport.h"

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace mie::dsp
{
class DspCore
{
public:
    struct StereoSample
    {
        double left = 0.0;
        double right = 0.0;
    };

    struct Parameters
    {
        float wide = 100.0f;
        float gainL = 0.0f;
        float gainR = 0.0f;
        float gainLr = 0.0f;
    };

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setParameters(const Parameters& newParameters);
    void beginBlock(int numSamples);
    StereoSample processSample(double leftInput, double rightInput);
    static int getMaximumLatencySamples(double sampleRate) noexcept;
    int getLatencySamples() const noexcept;

private:
    struct DerivedParameters
    {
        double wideAmount = 1.0;
        double leftGain = 1.0;
        double rightGain = 1.0;
        double linkedGain = 1.0;
    };

    void clearState();
    void updateDerivedParameters();
    Parameters parameters;
    DerivedParameters derived;

    double currentSampleRate = 44100.0;
};
} // namespace mie::dsp
