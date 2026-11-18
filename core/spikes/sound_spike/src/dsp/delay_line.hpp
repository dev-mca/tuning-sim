#pragma once

#include <cstddef>
#include <vector>

namespace sound_spike {

/// Ring buffer with fractional read position (linear interpolation).
/// A pipe of length L delays sound by L / c seconds = L * fs / c samples (research.md §6.3),
/// which is rarely an integer, hence the fractional read.
class DelayLine {
public:
    /// Allocates once; never allocates afterwards (safe for the audio thread).
    explicit DelayLine(double maxDelaySamples);

    void reset() noexcept;

    /// Appends the newest sample.
    void write(float sample) noexcept;

    /// Sample written `delaySamples` writes ago; read(1) returns the last written sample.
    /// Valid range: 1 <= delaySamples <= maxDelaySamples.
    [[nodiscard]] float read(double delaySamples) const noexcept;

private:
    std::vector<float> buffer_;
    std::size_t mask_;
    std::size_t writeIndex_ = 0;
};

} // namespace sound_spike
