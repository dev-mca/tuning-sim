#pragma once

// Plays an IAudioRenderer on the default output device via miniaudio (adapter, plan.md §2.1).
// Only this file pair knows miniaudio; the rest of simcore sees IAudioRenderer and AudioInfo.

#include "ports/ports.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace simcore {

class AudioOutput {
public:
    AudioOutput(IAudioRenderer& renderer, std::uint32_t sampleRate);
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    /// available = false and the reason in `device` if the device could not be opened.
    [[nodiscard]] const AudioInfo& info() const noexcept { return info_; }

    // research.md §6.8: 256-512 frames per period at 48 kHz (5-10 ms latency).
    static constexpr std::uint32_t kPeriodFrames = 512;
    static constexpr std::uint32_t kChannels = 2;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    AudioInfo info_;
};

} // namespace simcore
