#include "module.eqe.ProcessorSupport.h"

#include <cmath>
#include <vector>

void EqeModuleProcessor::copyVisualiserResponse(std::array<float, visualizerScopeSize>& stereoDb,
                                                std::array<float, visualizerScopeSize>& leftDb,
                                                std::array<float, visualizerScopeSize>& rightDb,
                                                std::array<float, visualizerScopeSize>& midDb,
                                                std::array<float, visualizerScopeSize>& sideDb,
                                                double& sampleRateOut) noexcept
{
    sampleRateOut = currentSampleRate;
    stereoDb.fill(0.0f);
    leftDb.fill(0.0f);
    rightDb.fill(0.0f);
    midDb.fill(0.0f);
    sideDb.fill(0.0f);

    if (currentSampleRate <= 0.0)
        return;

    const auto nyquist = (currentSampleRate * 0.5) * nyquistSafetyFactor;
    const auto activeCount = getActiveBellCount();

    struct DisplayBandState
    {
        bool active = false;
        int lrmsMode = 0;
        FilterType type = FilterType::bell;
        BellOrderFilter bellLower;
        BellOrderFilter bellUpper;
        double bellBlend = 0.0;
        BiquadCascade cascade;
    };

    std::vector<DisplayBandState> bandStates(static_cast<size_t>(activeCount));

    for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(bellIndex);
        const auto bandBypassed = bellBypassParams[bandArrayIndex] != nullptr
            && bellBypassParams[bandArrayIndex]->load(std::memory_order_relaxed) >= 0.5f;

        if (bandBypassed)
            continue;

        auto& bandState = bandStates[bandArrayIndex];
        bandState.lrmsMode = filterLrmsParams[bandArrayIndex] != nullptr
            ? juce::jlimit(0, 4, static_cast<int>(std::lround(filterLrmsParams[bandArrayIndex]->load(std::memory_order_relaxed))))
            : 0;
        bandState.type = getFilterTypeForBand(bandArrayIndex);

        const auto frequency = bellFrequencyParams[bandArrayIndex] != nullptr
            ? juce::jlimit(minimumVisibleFilterFrequency,
                           maximumVisibleFilterFrequency,
                           bellFrequencyParams[bandArrayIndex]->load(std::memory_order_relaxed))
            : defaultTiltFrequency;
        const auto designFrequency = computeDesignFrequency(static_cast<double>(frequency), currentSampleRate);
        const auto slope = bellSlopeChoiceParams[bandArrayIndex] != nullptr
            ? static_cast<double>(EqeModuleProcessor::getBellSlopeValueForChoiceIndex(bellSlopeChoiceParams[bandArrayIndex]->getIndex()))
            : static_cast<double>(fixedSlopeDbPerOct);
        const auto bandwidth = bellBandwidthParams[bandArrayIndex] != nullptr
            ? static_cast<double>(juce::jlimit(minimumBellBandwidth,
                                               maximumBellBandwidth,
                                               bellBandwidthParams[bandArrayIndex]->load(std::memory_order_relaxed)))
            : 1.0;
        const auto gainDb = bellGainParams[bandArrayIndex] != nullptr
            ? static_cast<double>(juce::jlimit(-48.0f,
                                               48.0f,
                                               bellGainParams[bandArrayIndex]->load(std::memory_order_relaxed)))
            : 0.0;

        if (bandState.type == FilterType::bell)
        {
            if (bellSlopeChoiceParams[bandArrayIndex] != nullptr
                && bellSlopeChoiceParams[bandArrayIndex]->getIndex() == 0)
            {
                continue;
            }

            if (std::abs(gainDb) < 1.0e-6)
                continue;

            const auto bellSlopeBlend = mapBellSlopeToBlend(slope);
            bandState.bellBlend = bellSlopeBlend.blend;

            if (bellSlopeBlend.lowerOrder > 0)
            {
                updateBellOrderFilter(bandState.bellLower,
                                      bellSlopeBlend.lowerOrder,
                                      designFrequency,
                                      bandwidth,
                                      juce::Decibels::decibelsToGain(gainDb));
            }

            if (bellSlopeBlend.upperOrder > 0)
            {
                updateBellOrderFilter(bandState.bellUpper,
                                      bellSlopeBlend.upperOrder,
                                      designFrequency,
                                      bandwidth,
                                      juce::Decibels::decibelsToGain(gainDb));
            }

            bandState.active = true;
            continue;
        }

        if (std::abs(gainDb) < 1.0e-6 && ! isCutFilterType(bandState.type))
            continue;

        if (isTiltFilterType(bandState.type))
        {
            updateTiltFilter(bandState.cascade, designFrequency, gainDb);
            bandState.active = bandState.cascade.stageCount > 0;
            continue;
        }

        if (isShelfFilterType(bandState.type))
        {
            const auto shelfSlopeBlend = mapShelfSlopeToBlend(slope);
            BiquadCascade lowerFilter;
            BiquadCascade upperFilter;

            updateShelfOrderFilterRaw(lowerFilter,
                                      bandState.type,
                                      shelfSlopeBlend.lowerOrder,
                                      designFrequency,
                                      bandwidth,
                                      juce::Decibels::decibelsToGain(gainDb));
            updateShelfOrderFilterRaw(upperFilter,
                                      bandState.type,
                                      shelfSlopeBlend.upperOrder,
                                      designFrequency,
                                      bandwidth,
                                      juce::Decibels::decibelsToGain(gainDb));
            updateInterpolatedCascadeFilter(bandState.cascade,
                                            lowerFilter,
                                            upperFilter,
                                            shelfSlopeBlend.blend);
            bandState.active = bandState.cascade.stageCount > 0;
            continue;
        }

        if (isCutFilterType(bandState.type))
        {
            buildCutBlendFilter(bandState.cascade,
                                bandState.type,
                                designFrequency,
                                bandwidth,
                                slope);
            bandState.active = bandState.cascade.stageCount > 0;
        }
    }

    for (size_t pointIndex = 0; pointIndex < stereoDb.size(); ++pointIndex)
    {
        const auto denominator = stereoDb.size() > 1 ? static_cast<double>(stereoDb.size() - 1) : 1.0;
        const auto proportion = static_cast<double>(pointIndex) / denominator;
        const auto frequency = juce::mapToLog10(static_cast<float>(proportion),
                                                minimumVisibleFilterFrequency,
                                                static_cast<float>(nyquist));

        std::complex<double> matrixLL(1.0, 0.0);
        std::complex<double> matrixLR(0.0, 0.0);
        std::complex<double> matrixRL(0.0, 0.0);
        std::complex<double> matrixRR(1.0, 0.0);

        for (int bellIndex = 0; bellIndex < activeCount; ++bellIndex)
        {
            const auto& bandState = bandStates[static_cast<size_t>(bellIndex)];

            if (! bandState.active)
                continue;

            std::complex<double> response(1.0, 0.0);

            if (bandState.type == FilterType::bell)
            {
                const auto lowerResponse = evaluateBellResponseAt(bandState.bellLower, frequency);
                const auto upperResponse = evaluateBellResponseAt(bandState.bellUpper, frequency);
                response = lowerResponse + ((upperResponse - lowerResponse) * bandState.bellBlend);
            }
            else
            {
                response = evaluateCascadeResponseAt(bandState.cascade, frequency);
            }

            switch (bandState.lrmsMode)
            {
                case 1:
                    matrixLL *= response;
                    matrixLR *= response;
                    break;

                case 2:
                    matrixRL *= response;
                    matrixRR *= response;
                    break;

                case 3:
                {
                    const auto halfSum = 0.5 * (response + 1.0);
                    const auto halfDiff = 0.5 * (response - 1.0);
                    const auto nextLL = (halfSum * matrixLL) + (halfDiff * matrixRL);
                    const auto nextLR = (halfSum * matrixLR) + (halfDiff * matrixRR);
                    const auto nextRL = (halfDiff * matrixLL) + (halfSum * matrixRL);
                    const auto nextRR = (halfDiff * matrixLR) + (halfSum * matrixRR);
                    matrixLL = nextLL;
                    matrixLR = nextLR;
                    matrixRL = nextRL;
                    matrixRR = nextRR;
                    break;
                }

                case 4:
                {
                    const auto halfSum = 0.5 * (response + 1.0);
                    const auto halfDiff = 0.5 * (1.0 - response);
                    const auto nextLL = (halfSum * matrixLL) + (halfDiff * matrixRL);
                    const auto nextLR = (halfSum * matrixLR) + (halfDiff * matrixRR);
                    const auto nextRL = (halfDiff * matrixLL) + (halfSum * matrixRL);
                    const auto nextRR = (halfDiff * matrixLR) + (halfSum * matrixRR);
                    matrixLL = nextLL;
                    matrixLR = nextLR;
                    matrixRL = nextRL;
                    matrixRR = nextRR;
                    break;
                }

                default:
                    matrixLL *= response;
                    matrixLR *= response;
                    matrixRL *= response;
                    matrixRR *= response;
                    break;
            }
        }

        const auto matrixMid = 0.5 * (matrixLL + matrixLR + matrixRL + matrixRR);
        const auto matrixSide = 0.5 * (matrixLL - matrixLR - matrixRL + matrixRR);
        const auto leftMagnitude = static_cast<float>(std::sqrt(std::norm(matrixLL)
                                                                + std::norm(matrixLR)));
        const auto rightMagnitude = static_cast<float>(std::sqrt(std::norm(matrixRL)
                                                                 + std::norm(matrixRR)));
        const auto stereoMagnitude = static_cast<float>(std::sqrt((std::norm(matrixLL)
                                                                   + std::norm(matrixLR)
                                                                   + std::norm(matrixRL)
                                                                   + std::norm(matrixRR)) * 0.5));

        stereoDb[pointIndex] = juce::Decibels::gainToDecibels(stereoMagnitude, -120.0f);
        leftDb[pointIndex] = juce::Decibels::gainToDecibels(leftMagnitude, -120.0f);
        rightDb[pointIndex] = juce::Decibels::gainToDecibels(rightMagnitude, -120.0f);
        midDb[pointIndex] = juce::Decibels::gainToDecibels(static_cast<float>(std::abs(matrixMid)), -120.0f);
        sideDb[pointIndex] = juce::Decibels::gainToDecibels(static_cast<float>(std::abs(matrixSide)), -120.0f);
    }
}
