#include "module.eqe.ProcessorSupport.h"

void EqeModuleProcessor::SecondOrderSection::reset() noexcept
{
    for (auto& channelState : state)
        channelState.fill(0.0);
}

void EqeModuleProcessor::SecondOrderSection::setIdentity() noexcept
{
    b = { 1.0, 0.0, 0.0 };
    a = { 0.0, 0.0 };
    reset();
}

void EqeModuleProcessor::SecondOrderSection::process(juce::AudioBuffer<float>& buffer, const int numChannels) noexcept
{
    const auto channelLimit = juce::jlimit(0, static_cast<int>(maxSupportedChannels), numChannels);

    for (int channel = 0; channel < channelLimit; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        auto& s = state[static_cast<size_t>(channel)];
        auto s1 = s[0];
        auto s2 = s[1];

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            const auto x = static_cast<double>(samples[sampleIndex]);
            const auto y = (b[0] * x) + s1;
            s1 = (b[1] * x) - (a[0] * y) + s2;
            s2 = (b[2] * x) - (a[1] * y);
            samples[sampleIndex] = static_cast<float>(y);
        }

        s[0] = s1;
        s[1] = s2;
    }
}

void EqeModuleProcessor::FourthOrderSection::reset() noexcept
{
    for (auto& channelState : state)
        channelState.fill(0.0);
}

void EqeModuleProcessor::FourthOrderSection::setIdentity() noexcept
{
    b = { 1.0, 0.0, 0.0, 0.0, 0.0 };
    a = { 0.0, 0.0, 0.0, 0.0 };
    reset();
}

void EqeModuleProcessor::FourthOrderSection::process(juce::AudioBuffer<float>& buffer, const int numChannels) noexcept
{
    const auto channelLimit = juce::jlimit(0, static_cast<int>(maxSupportedChannels), numChannels);

    for (int channel = 0; channel < channelLimit; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        auto& s = state[static_cast<size_t>(channel)];
        auto s1 = s[0];
        auto s2 = s[1];
        auto s3 = s[2];
        auto s4 = s[3];

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            const auto x = static_cast<double>(samples[sampleIndex]);
            const auto y = (b[0] * x) + s1;
            s1 = (b[1] * x) - (a[0] * y) + s2;
            s2 = (b[2] * x) - (a[1] * y) + s3;
            s3 = (b[3] * x) - (a[2] * y) + s4;
            s4 = (b[4] * x) - (a[3] * y);
            samples[sampleIndex] = static_cast<float>(y);
        }

        s[0] = s1;
        s[1] = s2;
        s[2] = s3;
        s[3] = s4;
    }
}

void EqeModuleProcessor::BellOrderFilter::reset() noexcept
{
    for (auto& section : sections)
        section.reset();
}

void EqeModuleProcessor::BellOrderFilter::setIdentity() noexcept
{
    sectionCount = 0;

    for (auto& section : sections)
        section.setIdentity();
}

void EqeModuleProcessor::BellOrderFilter::process(juce::AudioBuffer<float>& buffer, const int numChannels) noexcept
{
    if (sectionCount <= 0)
        return;

    for (int sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
        sections[static_cast<size_t>(sectionIndex)].process(buffer, numChannels);
}

void EqeModuleProcessor::BiquadCascade::reset() noexcept
{
    for (auto& section : sections)
        section.reset();
}

void EqeModuleProcessor::BiquadCascade::setIdentity() noexcept
{
    stageCount = 0;

    for (auto& section : sections)
        section.setIdentity();
}

void EqeModuleProcessor::BiquadCascade::process(juce::AudioBuffer<float>& buffer, const int numChannels) noexcept
{
    if (stageCount <= 0)
        return;

    for (int sectionIndex = 0; sectionIndex < stageCount; ++sectionIndex)
        sections[static_cast<size_t>(sectionIndex)].process(buffer, numChannels);
}
