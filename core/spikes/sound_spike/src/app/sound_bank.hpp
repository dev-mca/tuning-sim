#pragma once

#include "acoustic/engine_sound.hpp"
#include "acoustic/layout.hpp"
#include "app/acoustic_names.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace sound_spike {

/// Real-time front end for the audio callback (research.md §6.8, plan.md §2.2).
///
/// - All four layouts are built up front; switching never allocates.
/// - The control thread writes targets into atomics; the audio thread only reads them.
/// - A layout switch crossfades over a few milliseconds instead of clicking.
/// - Recording copies the output into a preallocated buffer (no allocation, no I/O);
///   the control thread writes the WAV file once the buffer is full.
class SoundBank {
public:
    SoundBank(float sampleRate, double recordDuration_s);

    // --- control thread ---
    void setTargetSpeed(double speed_rpm) noexcept { targetSpeed_.store(speed_rpm); }
    void setTargetLoad(float load) noexcept { targetLoad_.store(load); }
    void selectLayout(LayoutId layout) noexcept { requestedLayout_.store(layout); }
    void startRecording() noexcept;
    [[nodiscard]] bool recordingFinished() const noexcept;
    [[nodiscard]] std::span<const float> recording() const noexcept; // valid once finished
    [[nodiscard]] double currentSpeedRpm() const noexcept { return reportedSpeed_.load(); }

    // --- audio thread ---
    /// Fills interleaved stereo; must not allocate, lock or block.
    void render(std::span<float> interleavedStereo) noexcept;

    static constexpr std::size_t kMaxBlockFrames = 4096;
    static constexpr double kCrossfade_s = 0.08;

private:
    EngineSound& sound(LayoutId layout) noexcept;

    std::array<std::unique_ptr<EngineSound>, kAllLayouts.size()> sounds_;
    std::vector<float> fadeBuffer_; // second layout during a crossfade
    std::size_t crossfadeFrames_;
    std::size_t crossfadeRemaining_ = 0;
    LayoutId activeLayout_ = LayoutId::Inline6;
    LayoutId fadingOutLayout_ = LayoutId::Inline6;

    std::atomic<double> targetSpeed_{900.0};
    std::atomic<float> targetLoad_{0.15F};
    std::atomic<LayoutId> requestedLayout_{LayoutId::Inline6};
    std::atomic<double> reportedSpeed_{0.0};

    std::vector<float> recordBuffer_;
    std::atomic<std::size_t> recordPosition_; // == size when idle or finished
    std::atomic<bool> recordRequested_{false};
};

} // namespace sound_spike
