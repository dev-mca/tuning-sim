#pragma once

// What the simulation hands to the sound (plan.md §2.2: AcousticParams). Written by the
// simulation thread at 1 kHz, read by the audio thread. Plain values in SI units, no
// dependency on the simulation code, so the acoustic library stays independent.

#include "acoustic/engine_sound.hpp"

namespace simcore::acoustic {

struct AcousticParams {
    double speed_rad_s = 0.0;
    /// Air mass per cycle relative to a full cylinder at ambient conditions (VE = 1).
    /// About 0.3 at idle and 0.9-1.0 at wide-open throttle for the NA reference engine.
    double chargeRatio = 0.0;
    /// How much later than MBT the ECU ignites, deg crank angle (>= 0; early ignition is 0).
    double ignitionRetard_deg = 0.0;
    /// Measured lambda; +infinity without fuel.
    double lambda = 1.0;
    /// True while the ECU cuts fuel (rev limiter or overrun): no combustion, only pumping.
    bool fuelCut = false;
    /// Probability per exhaust event that unburnt mixture bangs in the exhaust (0..1).
    double afterburnRate = 0.0;
};

/// Maps the engine's operating point to the sound (research.md §6.2 step 3):
/// - amplitude follows the cylinder charge (pressure at exhaust valve opening),
/// - retarded ignition leaves more pressure at EVO (slightly louder) but burns slower and
///   later, which removes the sharp onset: the tone gets duller,
/// - lean mixtures burn less evenly: more cycle-to-cycle variation (rougher),
/// - fuel cut: no combustion, only weak pumping pulses with a dull tone.
[[nodiscard]] SoundTarget soundTargetFor(const AcousticParams& params) noexcept;

/// Extra pulse amplitude per degree of retard (hotter, higher-pressure exhaust at EVO).
inline constexpr double kRetardAmplitudeGain_per_deg = 0.01;
/// Retard at which the brightness has fallen to 1/e (about 37 %).
inline constexpr double kRetardDullness_deg = 15.0;
/// Above this lambda the combustion becomes noticeably uneven.
inline constexpr double kLeanRoughnessThreshold = 1.05;
/// Extra cycle variation per unit lambda above the threshold, and its upper limit.
inline constexpr double kLeanRoughnessGain = 1.0;
inline constexpr float kMaxCycleVariation = 0.3F;
/// Tone of the bare pumping pulses during fuel cut.
inline constexpr float kFuelCutBrightness = 0.6F;

} // namespace simcore::acoustic
