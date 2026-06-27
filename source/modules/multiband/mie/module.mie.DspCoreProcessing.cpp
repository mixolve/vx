#include "module.mie.DspCore.h"

namespace mie::dsp
{
DspCore::StereoSample DspCore::processSample(const double leftInput, const double rightInput)
{
    const auto gainedL = leftInput * derived.linkedGain * derived.leftGain;
    const auto gainedR = rightInput * derived.linkedGain * derived.rightGain;
    const auto mid = 0.5 * (gainedL + gainedR);
    const auto side = 0.5 * (gainedL - gainedR) * derived.wideAmount;
    return { mid + side, mid - side };
}
} // namespace mie::dsp
