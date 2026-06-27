#pragma once

#include "module.mie.DspCore.h"
#include "../module.multiband.Processor.h"

namespace mie::dsp
{
using MultibandProcessor = vx::multiband::Processor<DspCore>;
} // namespace mie::dsp
