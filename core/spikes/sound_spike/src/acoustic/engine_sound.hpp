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

namespace sound_spike {

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

    /// Renders interleaved stereo frames (L, R, L, R, ...) towards the target parameters.
    void render(std::span<float> interleavedStereo, double targetSpeed_rpm,
                float targetLoad) noexcept;

    [[nodiscard]] double currentSpeedRpm() const noexcept { return speed_.value(); }
    [[nodiscard]] float currentLoad() const noexcept { return static_cast<float>(load_.value()); }
    [[nodiscard]] std::size_t cylinderCount() const noexcept { return sources_.size(); }

    static constexpr double kMaxSpeedChange_rpm_s = 6000.0; // crank inertia (free revving)
    static constexpr double kSpeedSmoothing_s = 0.02;
    static constexpr double kLoadSmoothing_s = 0.03;
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
    std::array<Channel, 2> channels_;
    std::size_t samplesUntilToneUpdate_ = 0;
};

/// Short synthetic muffler impulse response: direct sound plus a decaying, filtered noise
/// tail (research.md §6.4: "zuerst synthetisch"). Deterministic for a given seed.
[[nodiscard]] std::vector<float> makeMufflerResponse(float sampleRate, std::uint32_t seed);

} // namespace sound_spike
