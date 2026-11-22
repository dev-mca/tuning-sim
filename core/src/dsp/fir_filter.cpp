#include "dsp/fir_filter.hpp"

#include <algorithm>
#include <utility>

namespace simcore::acoustic {

FirFilter::FirFilter(std::vector<float> taps)
    : taps_(std::move(taps)), history_(2 * taps_.size(), 0.0F) {}

void FirFilter::reset() noexcept {
    std::fill(history_.begin(), history_.end(), 0.0F);
    position_ = 0;
}

float FirFilter::process(float input) noexcept {
    const std::size_t length = taps_.size();
    if (length == 0) {
        return 0.0F;
    }
    // Newest sample at history_[position_], older samples follow; the copy at +length
    // makes history_[position_ .. position_+length) contiguous.
    position_ = (position_ == 0) ? length - 1 : position_ - 1;
    history_[position_] = input;
    history_[position_ + length] = input;
    float sum = 0.0F;
    for (std::size_t k = 0; k < length; ++k) {
        sum += taps_[k] * history_[position_ + k];
    }
    return sum;
}

} // namespace simcore::acoustic
