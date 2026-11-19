// Engine sound acceptance tests from plan.md iteration 3 (moved from the sound spike):
// - I4, I6 and V8 crossplane are clearly distinguishable (objective: engine order analysis),
// - speed changes are glitch-free,
// - output is finite, bounded and deterministic.

#include "acoustic/offline_render.hpp"
#include "order_analysis.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

using namespace simcore::acoustic;
using simcore::acoustic::testing::channel;
using simcore::acoustic::testing::orderPower;
using simcore::acoustic::testing::shareOnMultiplesOf;

namespace {

constexpr double kRate = 48000.0;
constexpr double kAnalysisSpeed_rpm = 3000.0;
constexpr double kSettle_s = 1.0;
// 3000 1/min: one cycle (720 deg) = 40 ms. 2 s = exactly 50 cycles: no leakage between orders.
constexpr double kAnalysis_s = 2.0;
constexpr std::size_t kLeft = 0;

std::vector<float> steadyChannel(LayoutId id, std::size_t channelIndex) {
    const std::vector<float> stereo =
        renderConstant(id, kSettle_s + kAnalysis_s, kAnalysisSpeed_rpm, 1.0F);
    const std::vector<float> mono = channel(stereo, channelIndex);
    const auto skip = static_cast<std::ptrdiff_t>(kSettle_s * kRate);
    return {mono.begin() + skip, mono.end()};
}

double share(LayoutId id, double baseOrder) {
    return shareOnMultiplesOf(steadyChannel(id, kLeft), baseOrder, kAnalysisSpeed_rpm, kRate);
}

double maxStep(std::span<const float> interleaved) {
    double largest = 0.0;
    for (std::size_t i = 2; i < interleaved.size(); ++i) {
        largest =
            std::max(largest, static_cast<double>(std::abs(interleaved[i] - interleaved[i - 2])));
    }
    return largest;
}

} // namespace

TEST_CASE("I4 sound is dominated by the 2nd engine order") {
    CHECK(share(LayoutId::Inline4, 2.0) > 0.9);
}

TEST_CASE("I6 sound is dominated by the 3rd engine order (150 Hz at 3000 1/min)") {
    CHECK(share(LayoutId::Inline6, 3.0) > 0.6);
    const std::vector<float> left = steadyChannel(LayoutId::Inline6, kLeft);
    const double third = orderPower(left, 3.0, kAnalysisSpeed_rpm, kRate);
    for (const double other : {0.5, 1.0, 1.5, 2.0, 2.5}) {
        CAPTURE(other);
        CHECK(third > orderPower(left, other, kAnalysisSpeed_rpm, kRate));
    }
}

TEST_CASE("crossplane V8 has strong half orders per bank, flatplane does not") {
    // research.md §1.3: per bank, crossplane fires unevenly (burble), flatplane like an I4.
    const double flat = share(LayoutId::V8Flatplane, 2.0);
    const double cross = share(LayoutId::V8Crossplane, 2.0);
    CHECK(flat > 0.9);
    CHECK(cross < 0.75);
    CHECK(flat - cross > 0.2);
}

TEST_CASE("comparison scenario is finite, bounded and free of jumps") {
    for (const LayoutId id : kAllLayouts) {
        CAPTURE(layoutKey(id));
        const std::vector<float> samples =
            renderOffline(id, kComparisonDuration_s, comparisonScenario);
        const bool allFinite =
            std::all_of(samples.begin(), samples.end(), [](float s) { return std::isfinite(s); });
        CHECK(allFinite);
        const auto [low, high] = std::minmax_element(samples.begin(), samples.end());
        CHECK(*low >= -1.0F);
        CHECK(*high <= 1.0F);
        CHECK(*high > 0.2F);           // clearly audible at full load
        CHECK(maxStep(samples) < 0.2); // no clicks during run-up and lift-off
    }
}

TEST_CASE("rendering is deterministic for a given seed") {
    const auto a = renderConstant(LayoutId::Inline4, 0.2, 2000.0, 0.5F, 1);
    const auto b = renderConstant(LayoutId::Inline4, 0.2, 2000.0, 0.5F, 1);
    const auto c = renderConstant(LayoutId::Inline4, 0.2, 2000.0, 0.5F, 2);
    CHECK(a == b);
    CHECK(a != c);
}
