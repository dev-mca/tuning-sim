#pragma once

// Test helper: energy of engine orders in a rendered signal (research.md §1.2).
// An engine order k is the frequency k * (rotational frequency N / 60).

#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

namespace simcore::acoustic::testing {

/// One channel (0 = left, 1 = right) of an interleaved stereo signal.
inline std::vector<float> channel(std::span<const float> interleaved, std::size_t index) {
    std::vector<float> result;
    result.reserve(interleaved.size() / 2);
    for (std::size_t i = index; i < interleaved.size(); i += 2) {
        result.push_back(interleaved[i]);
    }
    return result;
}

/// Goertzel algorithm: power of a single frequency, cheaper than a full FFT.
inline double goertzelPower(std::span<const float> signal, double frequencyHz, double sampleRate) {
    const double coefficient = 2.0 * std::cos(2.0 * std::numbers::pi * frequencyHz / sampleRate);
    double previous = 0.0;
    double previous2 = 0.0;
    for (const float sample : signal) {
        const double current = static_cast<double>(sample) + coefficient * previous - previous2;
        previous2 = previous;
        previous = current;
    }
    const double power =
        previous * previous + previous2 * previous2 - coefficient * previous * previous2;
    return power / static_cast<double>(signal.size() * signal.size());
}

/// Power at order `order` for a signal recorded at constant engine speed.
inline double orderPower(std::span<const float> signal, double order, double speed_rpm,
                         double sampleRate) {
    return goertzelPower(signal, order * speed_rpm / 60.0, sampleRate);
}

/// Share of the power in orders 0.5, 1.0, ..., maxOrder that falls on multiples of `base`.
/// For an evenly firing engine with z cylinders, base = z / 2 dominates (research.md §1.2).
inline double shareOnMultiplesOf(std::span<const float> signal, double base, double speed_rpm,
                                 double sampleRate, double maxOrder = 8.0) {
    constexpr double kHalfOrder = 0.5;
    double onMultiples = 0.0;
    double total = 0.0;
    for (double order = kHalfOrder; order <= maxOrder + 1.0e-9; order += kHalfOrder) {
        const double power = orderPower(signal, order, speed_rpm, sampleRate);
        total += power;
        const double ratio = order / base;
        if (std::abs(ratio - std::round(ratio)) < 1.0e-9) {
            onMultiples += power;
        }
    }
    return onMultiples / total;
}

} // namespace simcore::acoustic::testing
