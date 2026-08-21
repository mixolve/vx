#include "module.eql.ProcessorSupport.h"

#include <cmath>

void EqlModuleProcessor::resetFilters() noexcept
{
    const juce::ScopedLock lock(filterProcessLock);

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

    for (auto& filter : phaseFirFilters)
        filter.reset();

    for (auto& cachedState : cachedFilterStates)
        cachedState.valid = false;
}

void EqlModuleProcessor::updateFilters()
{
    const juce::ScopedLock lock(filterProcessLock);

    if (currentSampleRate <= 0.0)
        return;

    const auto filterCount = getActiveFilterCount();

    for (int filterIndex = 0; filterIndex < maxFilterCount; ++filterIndex)
    {
        const auto bandArrayIndex = static_cast<size_t>(filterIndex);

        if (filterIndex >= filterCount)
        {
            if (! filterDesignMatches(bandArrayIndex, false, FilterType::bell, 0.0f, 0.0f, 0.0f, 0.0f))
            {
                setBellIdentityResponse(bandArrayIndex);
                setShelfIdentityResponse(bandArrayIndex);
                setCutIdentityResponse(bandArrayIndex);
                setTiltIdentityResponse(bandArrayIndex);
                setPhaseIdentityResponse(bandArrayIndex);
                storeFilterDesignState(bandArrayIndex, false, FilterType::bell, 0.0f, 0.0f, 0.0f, 0.0f);
            }
            continue;
        }

        const auto filterType = getFilterTypeForBand(bandArrayIndex);
        const auto frequency = filterFrequencyParams[bandArrayIndex] != nullptr
            ? juce::jlimit(minimumVisibleFilterFrequency,
                           maximumVisibleFilterFrequency,
                           filterFrequencyParams[bandArrayIndex]->load(std::memory_order_relaxed))
            : defaultFilterFrequencyHz;
        const auto designFrequency = computeDesignFrequency(static_cast<double>(frequency), currentSampleRate);
        const auto slope = filterSlopeChoiceParams[bandArrayIndex] != nullptr
            ? static_cast<float>(EqlModuleProcessor::getBellSlopeValueForChoiceIndex(filterSlopeChoiceParams[bandArrayIndex]->getIndex()))
            : EqlModuleProcessor::fixedSlopeDbPerOct;
        const auto bandwidth = filterBandwidthParams[bandArrayIndex] != nullptr
            ? juce::jlimit(minimumBellBandwidth,
                           maximumBellBandwidth,
                           filterBandwidthParams[bandArrayIndex]->load(std::memory_order_relaxed))
            : 1.0f;
        const auto gainDb = filterGainParams[bandArrayIndex] != nullptr
            ? juce::jlimit(-48.0f,
                           48.0f,
                           filterGainParams[bandArrayIndex]->load(std::memory_order_relaxed))
            : 0.0f;

        if (isVolumeFilterType(filterType))
        {
            if (! filterDesignMatches(bandArrayIndex, true, filterType, 0.0f, 0.0f, 0.0f, gainDb))
            {
                setBellIdentityResponse(bandArrayIndex);
                setShelfIdentityResponse(bandArrayIndex);
                setCutIdentityResponse(bandArrayIndex);
                setTiltIdentityResponse(bandArrayIndex);
                setPhaseIdentityResponse(bandArrayIndex);
                storeFilterDesignState(bandArrayIndex, true, filterType, 0.0f, 0.0f, 0.0f, gainDb);
            }
            continue;
        }

        const auto cachedBandwidth = isTiltFilterType(filterType) ? 0.0f : bandwidth;
        const auto cachedSlope = isTiltFilterType(filterType) ? 0.0f : slope;

        if (isCutFilterType(filterType))
        {
            if (filterDesignMatches(bandArrayIndex, true, filterType, frequency, bandwidth, slope, 0.0f))
                continue;

            setBellIdentityResponse(bandArrayIndex);
            setShelfIdentityResponse(bandArrayIndex);
            setTiltIdentityResponse(bandArrayIndex);
            setPhaseIdentityResponse(bandArrayIndex);
            if (! (cachedFilterStates[bandArrayIndex].valid
                && cachedFilterStates[bandArrayIndex].active
                && cachedFilterStates[bandArrayIndex].type == filterType))
            {
                setCutIdentityResponse(bandArrayIndex);
            }

            rebuildCutBlendFilter(bandArrayIndex,
                                  filterType,
                                  designFrequency,
                                  static_cast<double>(slope));
            updatePhaseFirFilter(phaseFirFilters[bandArrayIndex],
                                 filterType,
                                 designFrequency,
                                 static_cast<double>(bandwidth),
                                 static_cast<double>(slope),
                                 0.0);

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
                setPhaseIdentityResponse(bandArrayIndex);
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

            updatePhaseFirFilter(phaseFirFilters[bandArrayIndex],
                                 filterType,
                                 designFrequency,
                                 static_cast<double>(bandwidth),
                                 static_cast<double>(slope),
                                 static_cast<double>(gainDb));

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
            updatePhaseFirFilter(phaseFirFilters[bandArrayIndex],
                                 filterType,
                                 designFrequency,
                                 static_cast<double>(bandwidth),
                                 static_cast<double>(slope),
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

        updatePhaseFirFilter(phaseFirFilters[bandArrayIndex],
                             filterType,
                             designFrequency,
                             static_cast<double>(bandwidth),
                             static_cast<double>(slope),
                             static_cast<double>(gainDb));

        storeFilterDesignState(bandArrayIndex, true, filterType, frequency, bandwidth, slope, gainDb);
    }
}
