#include "dsp/delay_line.hpp"

#include <algorithm>
#include <cmath>

namespace simcore::acoustic {

namespace {

constexpr std::size_t kInterpolationGuard = 4;

std::size_t nextPowerOfTwo(std::size_t value) {
    std::size_t result = 1;
    while (result < value) {
        result <<= 1U;
    }
    return result;
}

} // namespace

DelayLine::DelayLine(double maxDelaySamples)
    : buffer_(nextPowerOfTwo(static_cast<std::size_t>(std::ceil(maxDelaySamples)) +
                             kInterpolationGuard),
              0.0F),
      mask_(buffer_.size() - 1) {}

void DelayLine::reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0F);
    writeIndex_ = 0;
}

void DelayLine::write(float sample) noexcept {
    buffer_[writeIndex_] = sample;
    writeIndex_ = (writeIndex_ + 1) & mask_;
}

float DelayLine::read(double delaySamples) const noexcept {
    const double whole = std::floor(delaySamples);
    const auto fraction = static_cast<float>(delaySamples - whole);
    const auto offset = static_cast<std::size_t>(whole);
    // Power-of-two size: "& mask_" wraps negative positions without a branch.
    const std::size_t newer = (writeIndex_ + buffer_.size() - offset) & mask_;
    const std::size_t older = (newer + buffer_.size() - 1) & mask_;
    return buffer_[newer] + fraction * (buffer_[older] - buffer_[newer]);
}

} // namespace simcore::acoustic
