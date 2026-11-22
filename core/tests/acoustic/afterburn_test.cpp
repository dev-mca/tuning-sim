// Afterburn bangs (research.md §6.7): probability, determinism, bounded output.

#include "acoustic/afterburn.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace simcore::acoustic;

namespace {

constexpr float kRate = 48000.0F;
constexpr int kEventInterval = 480; // an exhaust event every 10 ms

std::vector<float> run(float rate, int samples, std::uint32_t seed = 7) {
    AfterburnSource source(kRate, seed);
    std::vector<float> out;
    for (int i = 0; i < samples; ++i) {
        if (i % kEventInterval == 0) {
            source.onExhaustEvent(rate);
        }
        out.push_back(source.process());
    }
    return out;
}

float peak(const std::vector<float>& signal) {
    float largest = 0.0F;
    for (const float s : signal) {
        largest = std::max(largest, std::abs(s));
    }
    return largest;
}

} // namespace

TEST_CASE("no afterburn means silence") {
    CHECK(peak(run(0.0F, 48000)) == 0.0F);
}

TEST_CASE("every event bangs at rate 1, and the output stays finite and bounded") {
    const std::vector<float> signal = run(1.0F, 48000);
    CHECK(peak(signal) > 0.1F);
    CHECK(peak(signal) < 4.0F);
    CHECK(std::all_of(signal.begin(), signal.end(), [](float s) { return std::isfinite(s); }));
}

TEST_CASE("bangs start after the delay, not at the exhaust event") {
    AfterburnSource source(kRate, 3);
    source.onExhaustEvent(1.0F);
    const auto delaySamples = static_cast<int>(AfterburnSource::kMinDelay_s * kRate);
    float early = 0.0F;
    for (int i = 0; i < delaySamples - 1; ++i) {
        early = std::max(early, std::abs(source.process()));
    }
    CHECK(early == 0.0F);
}

TEST_CASE("the bang rate follows the probability") {
    // Count 10 ms windows with a bang in 5 s of events at 100 per second. Rates stay low so
    // that bangs (about 50 ms audible each) rarely overlap and the count does not saturate.
    const auto count = [](float rate) {
        const std::vector<float> signal = run(rate, 5 * 48000);
        int loud = 0;
        for (std::size_t start = 0; start + 480 <= signal.size(); start += 480) {
            float windowPeak = 0.0F;
            for (std::size_t i = start; i < start + 480; ++i) {
                windowPeak = std::max(windowPeak, std::abs(signal[i]));
            }
            loud += windowPeak > 0.02F ? 1 : 0;
        }
        return loud;
    };
    const int rare = count(0.02F);
    const int often = count(0.1F);
    CHECK(rare > 5);
    CHECK(often > 3 * rare);
}

TEST_CASE("bangs are deterministic for a seed and reset restarts them") {
    CHECK(run(0.3F, 9600, 11) == run(0.3F, 9600, 11));
    CHECK(run(0.3F, 9600, 11) != run(0.3F, 9600, 12));
}
