#include "Processor.h"

#include "ParameterIds.h"

void DynAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto stateXml = valueTreeState.copyState().createXml())
        copyXmlToBinary(*stateXml, destData);
}

void DynAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(valueTreeState.state.getType()))
        {
            valueTreeState.replaceState(juce::ValueTree::fromXml(*xmlState));

            using dyn::parameters::makeCrossoverRangeParameterId;
            using dyn::parameters::parameterSpecs;
            using dyn::parameters::ParameterSlot;
            using dyn::parameters::toIndex;

            for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
            {
                auto* linkLeftRight = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(
                    makeCrossoverRangeParameterId(rangeIndex, parameterSpecs[toIndex(ParameterSlot::linkLeftRight)].suffix)));
                auto* linkUpDown = dynamic_cast<juce::RangedAudioParameter*>(valueTreeState.getParameter(
                    makeCrossoverRangeParameterId(rangeIndex, parameterSpecs[toIndex(ParameterSlot::linkUpDown)].suffix)));

                if (linkLeftRight == nullptr || linkUpDown == nullptr)
                    continue;

                const auto linkLeftRightOn = linkLeftRight->convertFrom0to1(linkLeftRight->getValue()) >= 0.5f;
                const auto linkUpDownOn = linkUpDown->convertFrom0to1(linkUpDown->getValue()) >= 0.5f;

                if (linkLeftRightOn && linkUpDownOn)
                    linkUpDown->setValueNotifyingHost(linkUpDown->convertTo0to1(0.0f));

                const auto effectiveLinkLrOn = linkLeftRight->convertFrom0to1(linkLeftRight->getValue()) >= 0.5f;
                const auto effectiveLinkUpDnOn = linkUpDown->convertFrom0to1(linkUpDown->getValue()) >= 0.5f;

                if (effectiveLinkLrOn)
                {
                    syncAllFieldParameters(rangeIndex, ParameterSlot::leftUpThreshold, ParameterSlot::leftDownThreshold, ParameterSlot::rightUpThreshold, ParameterSlot::rightDownThreshold);
                    syncAllFieldParameters(rangeIndex, ParameterSlot::leftUpTension, ParameterSlot::leftDownTension, ParameterSlot::rightUpTension, ParameterSlot::rightDownTension);
                    syncAllFieldParameters(rangeIndex, ParameterSlot::leftUpRelease, ParameterSlot::leftDownRelease, ParameterSlot::rightUpRelease, ParameterSlot::rightDownRelease);
                    syncAllFieldParameters(rangeIndex, ParameterSlot::leftUpOutput, ParameterSlot::leftDownOutput, ParameterSlot::rightUpOutput, ParameterSlot::rightDownOutput);
                }
                else if (effectiveLinkUpDnOn)
                {
                    syncUpDownParameterPairs(rangeIndex, ParameterSlot::leftUpThreshold, ParameterSlot::leftDownThreshold, ParameterSlot::rightUpThreshold, ParameterSlot::rightDownThreshold);
                    syncUpDownParameterPairs(rangeIndex, ParameterSlot::leftUpTension, ParameterSlot::leftDownTension, ParameterSlot::rightUpTension, ParameterSlot::rightDownTension);
                    syncUpDownParameterPairs(rangeIndex, ParameterSlot::leftUpRelease, ParameterSlot::leftDownRelease, ParameterSlot::rightUpRelease, ParameterSlot::rightDownRelease);
                    syncUpDownParameterPairs(rangeIndex, ParameterSlot::leftUpOutput, ParameterSlot::leftDownOutput, ParameterSlot::rightUpOutput, ParameterSlot::rightDownOutput);
                }
            }

            if (numRanges > 0)
            {
                const auto syncGlobalFromRange0 = [&] (const ParameterSlot slot)
                {
                    const auto* source = rawRangeParameters[0][toIndex(slot)];

                    if (source == nullptr)
                        return;

                    const auto sourceValue = source->load(std::memory_order_relaxed);

                    for (size_t targetRange = 0; targetRange < numRanges; ++targetRange)
                        setRangeParameterValue(targetRange, slot, sourceValue);
                };

                syncGlobalFromRange0(ParameterSlot::morph);
                syncGlobalFromRange0(ParameterSlot::peakHoldMs);
                syncGlobalFromRange0(ParameterSlot::lookahead);
                syncGlobalFromRange0(ParameterSlot::tensionFloor);
                syncGlobalFromRange0(ParameterSlot::tensionHysteresis);
                syncGlobalFromRange0(ParameterSlot::releaseForm);

                if (readRangeParameterValue(0, ParameterSlot::releaseForm) < 0.5f)
                {
                    for (size_t targetRange = 0; targetRange < numRanges; ++targetRange)
                        setRangeParameterValue(targetRange, ParameterSlot::releaseCurve, 0.0f);
                }
                else
                {
                    syncGlobalFromRange0(ParameterSlot::releaseCurve);
                }
            }

            markParametersDirty();
            syncParameters(true);
        }
    }
}
