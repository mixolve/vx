#pragma once

#include "../module.multiband.Processor.h"

#include <JuceHeader.h>

#include <array>
#include <cmath>
#include <vector>

namespace tls::dsp
{
using ava::multiband::detail::dbToAmp;
using ava::multiband::detail::wrapIndex;

inline double roundToParameterStep(const double value)
{
    return std::floor((value * 100.0) + 0.5) * 0.01;
}

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
        bool gainMidMute = false;
        float gainSide = 0.0f;
        bool gainSideMute = false;
        float gainL = 0.0f;
        bool gainLMute = false;
        float gainR = 0.0f;
        bool gainRMute = false;
        float gainLr = 0.0f;
        bool gainLrMute = false;
        int gainLOrder = 0;
        int gainROrder = 1;
        int gainMidOrder = 2;
        int gainSideOrder = 3;
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
        bool listenLc = false;
        bool listenRc = false;
        bool listenMc = false;
        bool listenSc = false;
        bool listenLl = false;
        bool listenRr = false;
        bool listenSs = false;
        float depStereoMs = 0.0f;
        float depLeftMs = 0.0f;
        float depRightMs = 0.0f;
        float depPhaseL = 0.0f;
        float depPhaseR = 0.0f;
    };

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setParameters(const Parameters& newParameters);
    StereoSample processSample(double leftInput, double rightInput);
    static int getMaximumLatencySamples(double sampleRate) noexcept;
    int getLatencySamples() const noexcept;
    bool isNeutral() const noexcept;

private:
    enum class GainOperation
    {
        left,
        right,
        mid,
        side
    };

    enum class ListenMode
    {
        leftCenter,
        rightCenter,
        midCenter,
        sideCenter,
        leftLeft,
        rightRight,
        neutral,
        sideStereo
    };

    struct DerivedParameters
    {
        double midGain = 1.0;
        double sideGain = 1.0;
        double leftGain = 1.0;
        double rightGain = 1.0;
        double linkedGain = 1.0;
        std::array<GainOperation, 4> gainOrder {
            GainOperation::left,
            GainOperation::right,
            GainOperation::mid,
            GainOperation::side
        };
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
        ListenMode listenMode = ListenMode::neutral;
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
    static constexpr double depMaxCombinedDelayMs = 200.0;

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

using MultibandProcessor = ava::multiband::Processor<DspCore>;
} // namespace tls::dsp
