#pragma once

// Renders the engine sound of a full-load dyno sweep offline (no audio device): simulation and
// sound advance in lockstep, 1 ms of simulation per 48 audio frames. Deterministic, so it
// serves both as a listening aid (simcore --sound-sweep out.wav) and for acceptance tests.

#include "domain/dyno/dyno.hpp"
#include "domain/ecu/ecu.hpp"
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

/// Free-revving "blips" without a brake: 1 s idle, then twice 1.5 s full throttle (into the
/// limiter) followed by 4 s lift-off. Lets you hear limiter and overrun, e.g. pops.
inline constexpr double kBlipScenarioDuration_s = 12.0;
[[nodiscard]] double blipScenarioThrottle(double time_s) noexcept;

[[nodiscard]] std::vector<float> renderBlipSound(const EngineDefinition& definition,
                                                 const OverrunSettings& overrun,
                                                 std::uint32_t sampleRate = 48000,
                                                 std::uint32_t seed = 1);

/// 2-step while standing: 1 s idle, launch switch held with full throttle until 6 s, released
/// with full throttle until 6.5 s (the "launch"), then lift-off. Lets you hear the 2-step.
inline constexpr double kLaunchHoldStart_s = 1.0;
inline constexpr double kLaunchRelease_s = 6.0;
inline constexpr double kLaunchLiftOff_s = 6.5;
inline constexpr double kLaunchScenarioDuration_s = 9.0;

struct LaunchScenarioInputs {
    double throttle;
    bool launchSwitch;
};
[[nodiscard]] LaunchScenarioInputs launchScenarioInputs(double time_s) noexcept;

[[nodiscard]] std::vector<float> renderLaunchSound(const EngineDefinition& definition,
                                                   const LaunchSettings& launch,
                                                   std::uint32_t sampleRate = 48000,
                                                   std::uint32_t seed = 1);

} // namespace simcore
