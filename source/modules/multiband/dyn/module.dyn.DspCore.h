#pragma once

#include "../module.multiband.Processor.h"

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace dyn::dsp
{
using ava::multiband::detail::dbToAmp;
using ava::multiband::detail::epsilon;
using ava::multiband::detail::roundToParameterStep;
using ava::multiband::detail::wrapIndex;

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
        float morph = 0.0f;
        float peakHoldMs = 0.0f;
        float lookaheadMs = 0.0f;
        float tensionFloor = -96.0f;
        float tensionHysteresis = 0.0f;
        int releaseForm = 0;
        float releaseCurve = 0.0f;
        bool delta = false;
    };

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setParameters(const Parameters& newParameters);
    StereoSample processSample(double leftInput, double rightInput);
    static int getMaximumLatencySamples(double sampleRate) noexcept;
    int getLatencySamples() const noexcept;
    bool isNeutral() const noexcept;

private:
    struct DerivedParameters
    {
        static constexpr size_t numBranches = 4;

        std::array<double, numBranches> thresholds { 1.0, 1.0, 1.0, 1.0 };
        std::array<double, numBranches> tensions { 0.0, 0.0, 0.0, 0.0 };
        std::array<int, numBranches> releaseSamples { 0, 0, 0, 0 };
        int peakHoldSamples = 0;
        std::array<double, numBranches> branchOutGains { 1.0, 1.0, 1.0, 1.0 };
        double morph = 0.0;
        double clipKneeDb = 0.0;
        double tensionFloor = 0.0;
        double tensionHysteresis = 0.0;
        bool releaseLogarithmic = false;
        double releaseCurve = 0.0;
        bool delta = false;
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
    struct ReleaseState
    {
        double start = 0.0;
        double target = 0.0;
        int elapsedSamples = 0;
        int durationSamples = 0;
        bool active = false;
    };

    static double releaseTowards(ReleaseState& state,
                                 double current,
                                 double target,
                                 int durationSamples,
                                 bool logarithmic,
                                 double curve);

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

    static constexpr size_t channelCount = 2;

    double currentSampleRate = 44100.0;
    int maxBuf = 1;

    std::array<double, branchCount> envBase { 0.0, 0.0, 0.0, 0.0 };
    std::array<double, branchCount> baseGainState { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, channelCount> cleanEnvPeak { 0.0, 0.0 };
    std::array<double, channelCount> cleanGainState { 1.0, 1.0 };
    std::array<double, channelCount> cleanHalfPeak { 0.0, 0.0 };
    std::array<bool, channelCount> cleanInputPositive { true, true };
    std::array<int, channelCount> cleanHoldSamples { 0, 0 };
    std::array<ReleaseState, branchCount> baseReleaseStates;
    std::array<ReleaseState, channelCount> cleanReleaseStates;
    int bufPos = 0;
    int bufPosDry = 0;

    std::array<std::vector<double>, branchCount> dmBase;
    std::vector<double> dryL;
    std::vector<double> dryR;
};

using MultibandProcessor = ava::multiband::Processor<DspCore>;
} // namespace dyn::dsp
