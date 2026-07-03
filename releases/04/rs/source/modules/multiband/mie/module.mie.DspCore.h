#pragma once

#include "module.mie.DspSupport.h"
#include "../module.multiband.Processor.h"

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
        bool rectPlus = false;
        bool rectMinus = false;
        bool rectFoldPlus = false;
        bool rectFoldMinus = false;
        float panL = -100.0f;
        float panR = 100.0f;
        float law = 0.0f;
        float shear = 0.0f;
        bool shearToRight = false;
        float midBal = 0.0f;
        float sideBal = 0.0f;
        float ortDegRotation = 0.0f;
        bool ortFlipR = false;
        bool listenL = false;
        bool listenR = false;
        bool listenM = false;
        bool listenS = false;
        bool listenInPlace = false;
        float depStereoMs = 0.0f;
        float depRightMs = 0.0f;
        float depBufferMs = 0.0f;
        float depPhaseL = 0.0f;
        float depPhaseR = 0.0f;
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
        double gLL = 1.0;
        double gLR = 0.0;
        double gRL = 0.0;
        double gRR = 1.0;
        double shearAmount = 0.0;
        double midBalance = 0.0;
        double sideBalance = 0.0;
        double ortM11 = 1.0;
        double ortM12 = 0.0;
        double ortM21 = 0.0;
        double ortM22 = 1.0;
        int listenMode = -1;
        int depLookaheadSamples = 0;
        int depDelayLeftSamples = 0;
        int depDelayRightSamples = 0;
        int latencySamples = 0;
        double depPhaseCosL = 1.0;
        double depPhaseSinL = 0.0;
        double depPhaseCosR = 1.0;
        double depPhaseSinR = 0.0;
    };

    static constexpr int depPhaseTaps = 129;
    static constexpr int depPhaseMid = (depPhaseTaps - 1) / 2;
    static constexpr int depPhaseBufferSize = 256;
    static constexpr double depMaxLookaheadMs = 200.0;

    static int msToSamples(double ms, double sampleRate) noexcept;
    void initialiseDepPhaseCoefficients();
    void clearState();
    void updateDerivedParameters();
    Parameters parameters;
    DerivedParameters derived;

    double currentSampleRate = 44100.0;
    int depDelayBufferSize = 1;
    int depDelayWritePosition = 0;
    int depPhaseWritePosition = 0;
    std::vector<double> depDelayLeft;
    std::vector<double> depDelayRight;
    std::array<double, depPhaseBufferSize> depPhaseLeft {};
    std::array<double, depPhaseBufferSize> depPhaseRight {};
    std::array<double, depPhaseTaps> depPhaseCoefficients {};
};

using MultibandProcessor = vx::multiband::Processor<DspCore>;
} // namespace mie::dsp
