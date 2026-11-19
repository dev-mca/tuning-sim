#pragma once

#include <cstdint>

namespace simcore::acoustic {

/// Small deterministic PRNG (xorshift32). Same seed -> same sound (docs/plan.md §2.3).
/// No allocation, no locks: usable in the audio callback (std::random is not guaranteed to be).
class Random {
public:
    explicit Random(std::uint32_t seed) noexcept : state_(seed != 0 ? seed : kFallbackSeed) {}

    std::uint32_t nextUint() noexcept {
        state_ ^= state_ << 13U;
        state_ ^= state_ >> 17U;
        state_ ^= state_ << 5U;
        return state_;
    }

    /// Uniform in [-1, 1].
    float uniform() noexcept {
        constexpr float kScale = 1.0F / 8388608.0F; // 2^23
        const auto mantissa = static_cast<float>(nextUint() >> 8U);
        return mantissa * kScale - 1.0F;
    }

    /// Approximately normal with standard deviation 1 (sum of 3 uniforms, scaled).
    float gaussian() noexcept {
        // Each U[-1,1] has variance 1/3, so the sum of three has variance 1.
        return uniform() + uniform() + uniform();
    }

private:
    static constexpr std::uint32_t kFallbackSeed = 0x9E3779B9U;
    std::uint32_t state_;
};

} // namespace simcore::acoustic
