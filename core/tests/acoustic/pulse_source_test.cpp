#include "acoustic/layout.hpp"
#include "acoustic/pulse_source.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <vector>

using namespace simcore::acoustic;

namespace {

constexpr float kSampleRate = 48000.0F;

/// Sample indices at which each cylinder's pulse starts (value rises from zero).
std::vector<std::vector<std::size_t>> pulseOnsets(LayoutId id, double speed_rpm,
                                                  std::size_t samples) {
    const LayoutSpec layout = makeLayout(id);
    PulseSource source(layout.cylinders, kSampleRate, 7);
    std::vector<float> values(layout.cylinders.size(), 0.0F);
    std::vector<float> previous(layout.cylinders.size(), 0.0F);
    std::vector<std::vector<std::size_t>> onsets(layout.cylinders.size());
    for (std::size_t n = 0; n < samples; ++n) {
        source.process(speed_rpm, 1.0F, values);
        for (std::size_t k = 0; k < values.size(); ++k) {
            if (previous[k] == 0.0F && values[k] > 0.0F) {
                onsets[k].push_back(n);
            }
            previous[k] = values[k];
        }
    }
    return onsets;
}

} // namespace

TEST_CASE("every cylinder fires once per cycle (720 deg)") {
    constexpr double kSpeed_rpm = 3000.0;   // 50 rev/s -> 25 cycles of 720 deg per second
    constexpr std::size_t kSamples = 48000; // 1 s
    const auto onsets = pulseOnsets(LayoutId::Inline6, kSpeed_rpm, kSamples);
    for (const auto& cylinder : onsets) {
        CHECK(cylinder.size() >= 24);
        CHECK(cylinder.size() <= 26);
        for (std::size_t i = 1; i < cylinder.size(); ++i) {
            // 1920 samples per cycle at 3000 1/min; jitter is +-0.5 deg = +-1.3 samples.
            CHECK(static_cast<double>(cylinder[i] - cylinder[i - 1]) ==
                  doctest::Approx(1920.0).epsilon(0.002));
        }
    }
}

TEST_CASE("I6 cylinders fire evenly in the order 1-5-3-6-2-4") {
    const auto onsets = pulseOnsets(LayoutId::Inline6, 3000.0, 4000);
    // 120 deg spacing = 320 samples at 3000 1/min. Order: cylinder index 0,4,2,5,1,3.
    // Differences are taken modulo one cycle (1920 samples): the order wraps around.
    constexpr long kCycleSamples = 1920;
    const std::vector<std::size_t> order = {0, 4, 2, 5, 1, 3};
    for (std::size_t i = 1; i < order.size(); ++i) {
        REQUIRE_FALSE(onsets[order[i]].empty());
        REQUIRE_FALSE(onsets[order[i - 1]].empty());
        const long difference = static_cast<long>(onsets[order[i]].front()) -
                                static_cast<long>(onsets[order[i - 1]].front());
        const long spacing = ((difference % kCycleSamples) + kCycleSamples) % kCycleSamples;
        CHECK(static_cast<double>(spacing) == doctest::Approx(320.0).epsilon(0.01));
    }
}

TEST_CASE("crossplane V8 fires unevenly within one bank, flatplane evenly") {
    // research.md §1.3: crossplane per bank 180/270/180/90 deg, flatplane 180 deg.
    const auto bankIntervals = [](LayoutId id) {
        const LayoutSpec layout = makeLayout(id);
        std::vector<double> angles;
        for (const CylinderSpec& cylinder : layout.cylinders) {
            if (cylinder.group == 0) {
                angles.push_back(cylinder.firingAngle_deg);
            }
        }
        std::sort(angles.begin(), angles.end());
        std::vector<double> intervals;
        for (std::size_t i = 0; i < angles.size(); ++i) {
            const double next = i + 1 < angles.size() ? angles[i + 1] : angles[0] + 720.0;
            intervals.push_back(next - angles[i]);
        }
        return intervals;
    };
    const std::vector<double> cross = bankIntervals(LayoutId::V8Crossplane);
    const std::vector<double> flat = bankIntervals(LayoutId::V8Flatplane);
    CHECK(*std::max_element(cross.begin(), cross.end()) == doctest::Approx(270.0));
    CHECK(*std::min_element(cross.begin(), cross.end()) == doctest::Approx(90.0));
    for (const double interval : flat) {
        CHECK(interval == doctest::Approx(180.0));
    }
}

TEST_CASE("no pulses at standstill") {
    const auto onsets = pulseOnsets(LayoutId::Inline4, 0.0, 4800);
    for (const auto& cylinder : onsets) {
        CHECK(cylinder.size() <= 1); // at most a pulse whose EVO sits exactly at 0 deg
    }
}
