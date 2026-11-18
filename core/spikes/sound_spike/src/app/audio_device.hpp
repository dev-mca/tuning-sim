#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace sound_spike {

class SoundBank;

/// Plays a SoundBank on the default output device via miniaudio (adapter, plan.md §2.1).
/// The miniaudio types stay in the .cpp file, so only this adapter depends on miniaudio.
class AudioDevice {
public:
    AudioDevice(SoundBank& bank, std::uint32_t sampleRate);
    ~AudioDevice();
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    [[nodiscard]] bool ok() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::string description() const;

    // research.md §6.8: 256-512 frames per period at 48 kHz (5-10 ms latency).
    static constexpr std::uint32_t kPeriodFrames = 512;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string error_;
};

} // namespace sound_spike
