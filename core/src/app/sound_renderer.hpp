#pragma once

// Real-time front end of the engine sound (docs/plan.md §2.2, research.md §6.8).
//
// simulation thread --publish()--> triple buffer --render()--> audio thread
// The audio thread reads the newest parameters once per block; EngineSound smooths them per
// sample, so 1 kHz updates never cause zipper noise. Volume and mute are atomics with a
// smoothed gain ramp: changing them never clicks.

#include "acoustic/acoustic_params.hpp"
#include "acoustic/engine_sound.hpp"
#include "acoustic/layout.hpp"
#include "concurrency/triple_buffer.hpp"
#include "ports/ports.hpp"

#include <atomic>
#include <cstdint>
#include <span>

namespace simcore {

class SoundRenderer final : public IAcousticSink, public IAudioRenderer {
public:
    SoundRenderer(const acoustic::LayoutSpec& layout, float sampleRate, std::uint32_t seed);

    // --- simulation thread (single writer) ---
    void publish(const acoustic::AcousticParams& params) override { params_.write(params); }

    // --- any thread ---
    void setOutput(double volume, bool muted) override;
    [[nodiscard]] float volume() const noexcept { return volume_.load(); }
    [[nodiscard]] bool muted() const noexcept { return muted_.load(); }

    // --- audio thread: no allocation, no locks, no I/O ---
    void render(std::span<float> interleavedStereo) noexcept override;

    static constexpr float kDefaultVolume = 0.7F;
    /// Volume and mute changes fade over about this time.
    static constexpr double kGainSmoothing_s = 0.03;

private:
    TripleBuffer<acoustic::AcousticParams> params_;
    acoustic::AcousticParams current_;
    acoustic::EngineSound sound_;
    std::atomic<float> volume_{kDefaultVolume};
    std::atomic<bool> muted_{false};
    float gain_ = 0.0F; // starts silent and fades in: no click when the device starts
    float gainCoefficient_;
    bool receivedParams_ = false;
};

} // namespace simcore
