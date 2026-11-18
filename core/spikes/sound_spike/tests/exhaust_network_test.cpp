#include "acoustic/exhaust_network.hpp"
#include "acoustic/layout.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <vector>

using namespace sound_spike;

namespace {

constexpr float kSampleRate = 48000.0F;

/// Sends a single impulse into cylinder 1 and records the radiated energy per 0.1 s window.
std::vector<double> impulseEnergyWindows(LayoutId id, std::size_t windows) {
    const LayoutSpec layout = makeLayout(id);
    ExhaustNetwork network(layout, kSampleRate);
    std::vector<float> sources(layout.cylinders.size(), 0.0F);
    constexpr std::size_t kWindow = 4800;
    std::vector<double> energy(windows, 0.0);
    for (std::size_t n = 0; n < windows * kWindow; ++n) {
        sources[0] = (n == 0) ? 1.0F : 0.0F;
        const StereoSample out = network.process(sources);
        energy[n / kWindow] += static_cast<double>(out.left * out.left + out.right * out.right);
    }
    return energy;
}

} // namespace

TEST_CASE("an impulse reaches the outlet and then dies away (stable network)") {
    for (const LayoutId id : kAllLayouts) {
        CAPTURE(layoutKey(id));
        const std::vector<double> energy = impulseEnergyWindows(id, 10);
        CHECK(energy[0] > 0.0);                // sound arrives
        CHECK(energy[9] < 1.0e-6 * energy[0]); // and decays: no instability
        for (const double value : energy) {
            CHECK(std::isfinite(value));
        }
    }
}

TEST_CASE("the first sound arrives after the travel time through the pipes") {
    // I4: primary 0.58 m at 590 m/s + tailpipe 2.2 m at 520 m/s = about 5.2 ms = 250 samples.
    const LayoutSpec layout = makeLayout(LayoutId::Inline4);
    ExhaustNetwork network(layout, kSampleRate);
    std::vector<float> sources(layout.cylinders.size(), 0.0F);
    std::size_t firstOutput = 0;
    for (std::size_t n = 0; n < 1000; ++n) {
        sources[0] = (n == 0) ? 1.0F : 0.0F;
        const StereoSample out = network.process(sources);
        if (std::abs(out.left) > 1.0e-6F) {
            firstOutput = n;
            break;
        }
    }
    CHECK(static_cast<double>(firstOutput) == doctest::Approx(250.0).epsilon(0.05));
}

TEST_CASE("dual exhaust V8 sends each bank to its own side") {
    const LayoutSpec layout = makeLayout(LayoutId::V8Crossplane);
    ExhaustNetwork network(layout, kSampleRate);
    std::vector<float> sources(layout.cylinders.size(), 0.0F);
    double left = 0.0;
    double right = 0.0;
    for (std::size_t n = 0; n < 9600; ++n) {
        sources[0] = (n == 0) ? 1.0F : 0.0F; // cylinder 1 = left bank
        const StereoSample out = network.process(sources);
        left += static_cast<double>(out.left * out.left);
        right += static_cast<double>(out.right * out.right);
    }
    CHECK(left > 3.0 * right);
}
