#include "module.eqe.ProcessorSupport.h"

#include <cmath>

void EqeModuleProcessor::resetBellFilters() noexcept
{
    for (auto& bandFilters : bellOrderFilters)
        for (auto& filter : bandFilters)
            filter.reset();

    for (auto& bandFilters : shelfOrderFilters)
        for (auto& filter : bandFilters)
            filter.reset();

    for (auto& filter : tiltFilters)
        filter.reset();

    for (auto& filter : cutBlendFilters)
        filter.reset();

    for (auto& cachedState : cachedFilterStates)
        cachedState.valid = false;
}

void EqeModuleProcessor::updateBellFilters()
{
    if (currentSampleRate <= 0.0)
        return;

    const auto bellCount = getActiveBellCount();

    for (int bellIndex = 0; bellIndex < maxBellFilterCount; ++bellIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(bellIndex);

        if (bellIndex >= bellCount)
        {
            if (! filterDesignMatches(bandArrayIndex, false, FilterType::bell, 0.0f, 0.0f, 0.0f, 0.0f))
            {
                setBellIdentityResponse(bandArrayIndex);
                setShelfIdentityResponse(bandArrayIndex);
                setCutIdentityResponse(bandArrayIndex);
                setTiltIdentityResponse(bandArrayIndex);
                storeFilterDesignState(bandArrayIndex, false, FilterType::bell, 0.0f, 0.0f, 0.0f, 0.0f);
            }
            continue;
        }

        const auto filterType = getFilterTypeForBand(bandArrayIndex);
        const auto frequency = bellFrequencyParams[bandArrayIndex] != nullptr
            ? juce::jlimit(minimumVisibleFilterFrequency,
                           maximumVisibleFilterFrequency,
                           bellFrequencyParams[bandArrayIndex]->load(std::memory_order_relaxed))
            : defaultTiltFrequency;
        const auto designFrequency = computeDesignFrequency(static_cast<double>(frequency), currentSampleRate);
        const auto slope = bellSlopeChoiceParams[bandArrayIndex] != nullptr
            ? static_cast<float>(EqeModuleProcessor::getBellSlopeValueForChoiceIndex(bellSlopeChoiceParams[bandArrayIndex]->getIndex()))
            : EqeModuleProcessor::fixedSlopeDbPerOct;
        const auto bandwidth = bellBandwidthParams[bandArrayIndex] != nullptr
            ? juce::jlimit(minimumBellBandwidth,
                           maximumBellBandwidth,
                           bellBandwidthParams[bandArrayIndex]->load(std::memory_order_relaxed))
            : 1.0f;
        const auto gainDb = bellGainParams[bandArrayIndex] != nullptr
            ? juce::jlimit(-48.0f,
                           48.0f,
                           bellGainParams[bandArrayIndex]->load(std::memory_order_relaxed))
            : 0.0f;
        const auto cachedBandwidth = isTiltFilterType(filterType) ? 0.0f : bandwidth;
        const auto cachedSlope = isTiltFilterType(filterType) ? 0.0f : slope;

        if (isCutFilterType(filterType))
        {
            if (filterDesignMatches(bandArrayIndex, true, filterType, frequency, bandwidth, slope, 0.0f))
                continue;

            setBellIdentityResponse(bandArrayIndex);
            setShelfIdentityResponse(bandArrayIndex);
            setTiltIdentityResponse(bandArrayIndex);
            if (! (cachedFilterStates[bandArrayIndex].valid
                && cachedFilterStates[bandArrayIndex].active
                && cachedFilterStates[bandArrayIndex].type == filterType))
            {
                setCutIdentityResponse(bandArrayIndex);
            }

            rebuildCutBlendFilter(bandArrayIndex,
                                  filterType,
                                  designFrequency,
                                  static_cast<double>(bandwidth),
                                  static_cast<double>(slope));

            storeFilterDesignState(bandArrayIndex, true, filterType, frequency, bandwidth, slope, 0.0f);

            continue;
        }

        if (std::abs(gainDb) < 1.0e-6f)
        {
            if (! filterDesignMatches(bandArrayIndex, true, filterType, frequency, cachedBandwidth, cachedSlope, gainDb))
            {
                setBellIdentityResponse(bandArrayIndex);
                setShelfIdentityResponse(bandArrayIndex);
                setCutIdentityResponse(bandArrayIndex);
                setTiltIdentityResponse(bandArrayIndex);
                storeFilterDesignState(bandArrayIndex, true, filterType, frequency, cachedBandwidth, cachedSlope, gainDb);
            }
            continue;
        }

        if (filterDesignMatches(bandArrayIndex, true, filterType, frequency, cachedBandwidth, cachedSlope, gainDb))
            continue;

        const auto gain = juce::Decibels::decibelsToGain(static_cast<double>(gainDb));

        if (filterType == FilterType::bell)
        {
            setShelfIdentityResponse(bandArrayIndex);
            setCutIdentityResponse(bandArrayIndex);
            setTiltIdentityResponse(bandArrayIndex);

            for (int order = 1; order <= static_cast<int>(maxBellOrder); ++order)
            {
                updateBellOrderFilter(bellOrderFilters[bandArrayIndex][static_cast<size_t>(order - 1)],
                                      order,
                                      designFrequency,
                                      static_cast<double>(bandwidth),
                                      gain);
            }

            storeFilterDesignState(bandArrayIndex, true, filterType, frequency, bandwidth, slope, gainDb);

            continue;
        }

        if (isTiltFilterType(filterType))
        {
            setBellIdentityResponse(bandArrayIndex);
            setShelfIdentityResponse(bandArrayIndex);
            setCutIdentityResponse(bandArrayIndex);
            updateTiltFilter(tiltFilters[bandArrayIndex],
                             designFrequency,
                             static_cast<double>(gainDb));
            storeFilterDesignState(bandArrayIndex, true, filterType, frequency, 0.0f, 0.0f, gainDb);

            continue;
        }

        setBellIdentityResponse(bandArrayIndex);
        setCutIdentityResponse(bandArrayIndex);
        setTiltIdentityResponse(bandArrayIndex);

        for (int order = 1; order <= static_cast<int>(maxShelfOrder); ++order)
        {
            updateShelfOrderFilterRaw(shelfOrderFilters[bandArrayIndex][static_cast<size_t>(order - 1)],
                                      filterType,
                                      order,
                                      designFrequency,
                                      static_cast<double>(bandwidth),
                                      gain);
        }

        storeFilterDesignState(bandArrayIndex, true, filterType, frequency, bandwidth, slope, gainDb);
    }
}
