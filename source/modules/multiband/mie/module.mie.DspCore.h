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
        float gainMid = 0.0f;
        float gainSide = 0.0f;
        float gainL = 0.0f;
        float gainR = 0.0f;
        float gainLr = 0.0f;
        bool halfPositive = false;
        bool halfNegative = false;
        bool fullPositive = false;
        bool fullNegative = false;
        float left = -100.0f;
        float right = 100.0f;
        float law = 0.0f;
        float impact = 0.0f;
        bool impactToRight = false;
        float mid = 0.0f;
        float side = 0.0f;
        float degree = 0.0f;
        bool flipRight = false;
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
    bool isNeutral() const noexcept;

private:
    struct DerivedParameters
    {
        double midGain = 1.0;
        double sideGain = 1.0;
        double leftGain = 1.0;
        double rightGain = 1.0;
        double linkedGain = 1.0;
        double gLL = 1.0;
        double gLR = 0.0;
        double gRL = 0.0;
        double gRR = 1.0;
        double impactAmount = 0.0;
        double midAmount = 0.0;
        double sideAmount = 0.0;
        double orthogonalM11 = 1.0;
        double orthogonalM12 = 0.0;
        double orthogonalM21 = 0.0;
        double orthogonalM22 = 1.0;
        int listenMode = -1;
        int depDelayLeftSamples = 0;
        int depDelayRightSamples = 0;
        bool depDelayEnabled = false;
        bool depPhaseEnabled = false;
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
