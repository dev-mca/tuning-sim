#pragma once

// Renders the engine sound of a full-load dyno sweep offline (no audio device): simulation and
// sound advance in lockstep, 1 ms of simulation per 48 audio frames. Deterministic, so it
// serves both as a listening aid (simcore --sound-sweep out.wav) and for acceptance tests.

#include "domain/dyno/dyno.hpp"
#include "domain/engine/engine_definition.hpp"

#include <cstdint>
#include <vector>

namespace simcore {

struct SoundSweepOptions {
    SweepParameters sweep;
    /// Added to every cell of the ignition table (negative = retarded), deg.
    double ignitionOffset_deg = 0.0;
    std::uint32_t sampleRate = 48000;
    std::uint32_t seed = 1;
};

/// Interleaved stereo samples in [-1, 1] at full volume.
[[nodiscard]] std::vector<float> renderSweepSound(const EngineDefinition& definition,
                                                  const SoundSweepOptions& options);

} // namespace simcore
