#pragma once

#include "acoustic/exhaust_network.hpp"
#include "acoustic/layout.hpp"
#include "acoustic/pulse_source.hpp"
#include "dsp/filters.hpp"
#include "dsp/fir_filter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace simcore::acoustic {

/// What the sound should do right now. EngineSound moves towards it smoothly.
struct SoundTarget {
    double speed_rpm = 0.0;
    /// Pulse amplitude: 1 = full load at best ignition, 0 = only pumping (no combustion).
    float load = 0.0F;
    /// Factor on the tone cutoff: 1 = normal, smaller = duller (e.g. retarded ignition).
    float brightness = 1.0F;
    /// Cycle-to-cycle amplitude variation (standard deviation), see PulseSource.
    float cycleVariation = PulseSource::kDefaultAmplitudeVariation;
};

/// Complete exhaust sound of one engine layout (research.md §6.2-6.4, §6.8).
///
/// pulses -> waveguide exhaust -> muffler (FIR convolution with a short synthetic impulse
/// response) -> load-dependent low-pass -> DC blocker -> gain -> soft clipper.
/// Engine speed and load are smoothed per sample, so parameter changes never click.
class EngineSound {
public:
    EngineSound(const LayoutSpec& layout, float sampleRate, std::uint32_t seed);

    /// Clears all filter and pipe state and sets the smoothed parameters directly.
    void reset(double speed_rpm, float load) noexcept;

    /// Renders interleaved stereo frames (L, R, L, R, ...) towards the target.
    void render(std::span<float> interleavedStereo, const SoundTarget& target) noexcept;

    /// Shortcut with normal brightness and variation (used by the sound spike).
    void render(std::span<float> interleavedStereo, double targetSpeed_rpm,
                float targetLoad) noexcept {
        render(interleavedStereo, SoundTarget{targetSpeed_rpm, targetLoad});
    }

    [[nodiscard]] double currentSpeedRpm() const noexcept { return speed_.value(); }
    [[nodiscard]] float currentLoad() const noexcept { return static_cast<float>(load_.value()); }
    [[nodiscard]] std::size_t cylinderCount() const noexcept { return sources_.size(); }

    static constexpr double kMaxSpeedChange_rpm_s = 6000.0; // crank inertia (free revving)
    static constexpr double kSpeedSmoothing_s = 0.02;
    // Short: each pulse latches its amplitude at EVO, so load jumps cannot click, and the
    // few firings between limiter cuts must reach full strength (the "brap").
    static constexpr double kLoadSmoothing_s = 0.002;
    static constexpr double kBrightnessSmoothing_s = 0.05;
    /// Upper load limit: retarded ignition can raise the exhaust pulse above full load.
    static constexpr float kMaxLoad = 1.3F;
    /// The tone filter never goes below this, even when very dull.
    static constexpr float kMinToneCutoffHz = 400.0F;
    // Calibrated: full load at high speed gives about -16 dBFS RMS, peaks below 0.8.
    static constexpr float kOutputGain = 12.0F;

private:
    struct Channel {
        Channel(const std::vector<float>& mufflerResponse, float sampleRate);
        FirFilter muffler;
        BiquadLowpass tone;
        DcBlocker dcBlocker;
    };

    void updateTone() noexcept;

    float sampleRate_;
    PulseSource pulses_;
    ExhaustNetwork exhaust_;
    std::vector<float> sources_; // one value per cylinder, reused every sample
    SlewSmoother speed_;
    SlewSmoother load_;
    SlewSmoother brightness_;
    std::array<Channel, 2> channels_;
    std::size_t samplesUntilToneUpdate_ = 0;
};

/// Short synthetic muffler impulse response: direct sound plus a decaying, filtered noise
/// tail (research.md §6.4: "zuerst synthetisch"). Deterministic for a given seed.
[[nodiscard]] std::vector<float> makeMufflerResponse(float sampleRate, std::uint32_t seed);

} // namespace simcore::acoustic
