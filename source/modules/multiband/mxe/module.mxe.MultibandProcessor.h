#pragma once

#include "module.mxe.DspCore.h"
#include "../module.multiband.Processor.h"

namespace mxe::dsp
{
using MultibandProcessor = vx::multiband::Processor<DspCore>;
} // namespace mxe::dsp
