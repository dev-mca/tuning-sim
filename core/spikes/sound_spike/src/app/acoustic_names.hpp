#pragma once

// The spike's DSP and acoustic code moved to simcore (core/src/acoustic, core/src/dsp) in
// iteration 7. This using-directive lets the remaining spike code keep its short names.

namespace simcore::acoustic {}
namespace simcore {}

namespace sound_spike {
using namespace simcore::acoustic;
using namespace simcore;
} // namespace sound_spike
