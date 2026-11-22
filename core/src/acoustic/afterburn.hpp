#pragma once

// Afterburn bangs ("pops & bangs", research.md §6.7): unburnt mixture that ignites in the hot
// exhaust. Every exhaust valve opening may schedule a bang with a probability that comes from
// the simulation (late combustion x hot catalyst). A bang is a sharp noise crack plus a short,
// low "thump"; most are small crackles, a few are big bangs.
//
// Real-time safe: a fixed number of simultaneous bangs (no allocation), deterministic random.

#include "dsp/filters.hpp"
#include "dsp/random.hpp"

#include <array>
#include <cstdint>

namespace simcore::acoustic {

class AfterburnSource {
public:
    AfterburnSource(float sampleRate, std::uint32_t seed);

    void reset() noexcept;

    /// An exhaust valve just opened; schedules a bang with probability `rate` (0..1).
    void onExhaustEvent(float rate) noexcept;

    /// Next mono sample of all running bangs.
    float process() noexcept;

    /// Delay between EVO and the bang: the mixture needs time to reach hot spots and ignite.
    static constexpr double kMinDelay_s = 0.002;
    static constexpr double kMaxDelay_s = 0.025;
    static constexpr double kMinCrackDecay_s = 0.002;
    static constexpr double kMaxCrackDecay_s = 0.010;
    static constexpr double kThumpDecay_s = 0.025;
    static constexpr double kMinThumpHz = 70.0;
    static constexpr double kMaxThumpHz = 140.0;
    static constexpr float kThumpShare = 0.6F;
    /// Amplitude = small + (big - small) * u^3: mostly crackles, sometimes a big bang.
    static constexpr float kSmallAmplitude = 0.25F;
    static constexpr float kBigAmplitude = 2.0F;
    static constexpr std::size_t kMaxBangs = 8;

private:
    struct Bang {
        bool active = false;
        double delaySamples = 0.0;
        double age = 0.0; // samples since the bang started
        float amplitude = 0.0F;
        float crackDecay = 0.0F; // per-sample envelope factor
        float thumpDecay = 0.0F;
        float thumpEnvelope = 0.0F;
        float crackEnvelope = 0.0F;
        double thumpPhase = 0.0;
        double thumpIncrement = 0.0;
    };

    [[nodiscard]] float uniform01() noexcept { return 0.5F * (random_.uniform() + 1.0F); }
    [[nodiscard]] float decayFactor(double time_s) const noexcept;

    float sampleRate_;
    std::uint32_t seed_;
    Random random_;
    std::array<Bang, kMaxBangs> bangs_{};
    OnePoleLowpass crackColour_; // takes the hiss out of the noise, keeps the crack
};

} // namespace simcore::acoustic
