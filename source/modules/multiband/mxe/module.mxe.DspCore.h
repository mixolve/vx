#pragma once

#include "module.mxe.DspSupport.h"
#include "../module.multiband.Processor.h"

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace mxe::dsp
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
        float thLU = 0.0f;
        float tensLU = 0.0f;
        float relLU = 0.0f;
        float outLU = 0.0f;
        float thLD = 0.0f;
        float tensLD = 0.0f;
        float relLD = 0.0f;
        float outLD = 0.0f;
        float thRU = 0.0f;
        float tensRU = 0.0f;
        float relRU = 0.0f;
        float outRU = 0.0f;
        float thRD = 0.0f;
        float tensRD = 0.0f;
        float relRD = 0.0f;
        float outRD = 0.0f;
        float moRph = 0.0f;
        float peakHoldHz = 100.0f;
        float TensionFlooR = -96.0f;
        float TensionHysT = 0.0f;
        bool delTa = false;
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
        static constexpr size_t numBranches = 4;

        std::array<double, numBranches> thresholds { 1.0, 1.0, 1.0, 1.0 };
        std::array<double, numBranches> tensions { 0.0, 0.0, 0.0, 0.0 };
        std::array<double, numBranches> releaseCoeffs { 0.0, 0.0, 0.0, 0.0 };
        std::array<double, numBranches> branchOutGains { 1.0, 1.0, 1.0, 1.0 };
        double morph = 0.0;
        double clipKneeDb = 0.0;
        double tensionFloor = 0.0;
        double tensionHysteresis = 0.0;
        bool delta = false;
        int holdSamples = 0;
        int lookaheadSamples = 0;
        int totalLookaheadSamples = 0;
        int bufferSize = 1;
        int dryBufferSize = 1;
        int latencySamples = 0;
    };

    static double safeAbs(double value);
    static double clamp1(double value);
    static double satShape(double value, double kneeDb);
    static double tensionTarget(double env, double threshold, double floorThreshold, double floorHysteresis, double tension);

    void resizeLookaheadBuffers();
    void clearState();
    void updateDerivedParameters();
    Parameters parameters;
    DerivedParameters derived;

    enum BranchIndex : size_t
    {
        branchLu = 0,
        branchLd,
        branchRu,
        branchRd,
        branchCount
    };

    double currentSampleRate = 44100.0;
    int maxBuf = 1;

    std::array<int, branchCount> holdPeak { 0, 0, 0, 0 };
    std::array<int, branchCount> holdBase { 0, 0, 0, 0 };
    std::array<double, branchCount> envPeak { 0.0, 0.0, 0.0, 0.0 };
    std::array<double, branchCount> envBase { 0.0, 0.0, 0.0, 0.0 };
    std::array<double, branchCount> baseGainState { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, branchCount> gainReductionState { 1.0, 1.0, 1.0, 1.0 };
    int bufPos = 0;
    int bufPosDry = 0;

    std::array<std::vector<double>, branchCount> dmBase;
    std::array<std::vector<double>, branchCount> dmGain;
    std::vector<double> dryL;
    std::vector<double> dryR;
};

using MultibandProcessor = vx::multiband::Processor<DspCore>;
} // namespace mxe::dsp
