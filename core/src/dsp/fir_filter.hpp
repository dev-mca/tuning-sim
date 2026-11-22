#pragma once

#include <cstddef>
#include <vector>

namespace simcore::acoustic {

/// Direct-form FIR convolution y[n] = sum_k h[k] x[n-k] (research.md §6.4).
/// Fine for short impulse responses (a few hundred taps); longer ones would need
/// partitioned FFT convolution.
class FirFilter {
public:
    explicit FirFilter(std::vector<float> taps);

    void reset() noexcept;
    float process(float input) noexcept;

private:
    std::vector<float> taps_;
    std::vector<float> history_; // stored twice in a row: no wrap-around inside the dot product
    std::size_t position_ = 0;
};

} // namespace simcore::acoustic
