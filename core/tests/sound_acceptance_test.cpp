// Acceptance tests of plan.md iteration 7, measured instead of only listened to:
// - a full-load sweep is finite, bounded, click-free and gets louder with speed,
// - retarded ignition sounds duller (less high-frequency energy),
// - load changes (pedal steps, overrun fuel cut) do not click,
// - volume and mute fade instead of jumping.

#include "app/engine_acoustics.hpp"
#include "app/sound_renderer.hpp"
#include "app/sound_sweep.hpp"
#include "domain/ecu/ecu.hpp"
#include "domain/engine/engine_simulation.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

using namespace simcore;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kStereo = 2;
/// Faster than a real sweep to keep the test short; settling (3 s) is part of every sweep.
constexpr double kTestSweepRate_rpm_s = 1500.0;
/// Largest allowed sample-to-sample change: real clicks are jumps of 0.3 and more.
constexpr double kMaxClickStep = 0.1;

std::vector<float> left(std::span<const float> interleaved) {
    std::vector<float> mono;
    for (std::size_t i = 0; i < interleaved.size(); i += kStereo) {
        mono.push_back(interleaved[i]);
    }
    return mono;
}

std::vector<float> section(const std::vector<float>& mono, double from_s, double to_s) {
    const auto begin = static_cast<std::ptrdiff_t>(from_s * kSampleRate);
    const auto end = static_cast<std::ptrdiff_t>(to_s * kSampleRate);
    return {mono.begin() + begin, mono.begin() + std::min<std::ptrdiff_t>(end, std::ssize(mono))};
}

double rms(std::span<const float> signal) {
    double sum = 0.0;
    for (const float sample : signal) {
        sum += static_cast<double>(sample) * sample;
    }
    return std::sqrt(sum / static_cast<double>(signal.size()));
}

/// Brightness measure: RMS of the first difference over RMS of the signal. The difference
/// boosts high frequencies (gain 2 sin(pi f / fs)), so a duller sound gives a smaller ratio.
double brightness(std::span<const float> signal) {
    std::vector<float> difference;
    for (std::size_t i = 1; i < signal.size(); ++i) {
        difference.push_back(signal[i] - signal[i - 1]);
    }
    return rms(difference) / rms(signal);
}

double maxStep(std::span<const float> mono) {
    double largest = 0.0;
    for (std::size_t i = 1; i < mono.size(); ++i) {
        largest = std::max(largest, static_cast<double>(std::abs(mono[i] - mono[i - 1])));
    }
    return largest;
}

double decibels(double ratio) {
    return 20.0 * std::log10(ratio);
}

std::vector<float> sweepSound(double ignitionOffset_deg) {
    SoundSweepOptions options;
    options.sweep = {1000.0, 7000.0};
    options.sweep.rate_rpm_s = kTestSweepRate_rpm_s;
    options.ignitionOffset_deg = ignitionOffset_deg;
    return left(renderSweepSound(test::referenceEngine().definition, options));
}

} // namespace

TEST_CASE("full-load sweep is bounded, click-free and gets louder with speed") {
    const std::vector<float> sound = sweepSound(0.0);
    const double duration_s = static_cast<double>(sound.size()) / kSampleRate;
    CHECK(duration_s == doctest::Approx(3.0 + 6000.0 / kTestSweepRate_rpm_s).epsilon(0.01));
    CHECK(std::all_of(sound.begin(), sound.end(), [](float s) { return std::isfinite(s); }));
    const auto [low, high] = std::minmax_element(sound.begin(), sound.end());
    CHECK(*low >= -1.0F);
    CHECK(*high <= 1.0F);
    CHECK(maxStep(sound) < kMaxClickStep);
    const double atStart = rms(section(sound, 1.0, 3.0)); // settling at 1000 1/min
    const double atEnd = rms(section(sound, duration_s - 0.5, duration_s));
    CHECK(decibels(atEnd / atStart) > 6.0);
}

TEST_CASE("10 deg later ignition sounds clearly duller") {
    const std::vector<float> base = sweepSound(0.0);
    const std::vector<float> retarded = sweepSound(-10.0);
    // The ramp from about 2000 to 6000 1/min, where the difference matters most.
    const double from_s = 3.0 + 1000.0 / kTestSweepRate_rpm_s;
    const double to_s = 3.0 + 5000.0 / kTestSweepRate_rpm_s;
    const double baseBrightness = brightness(section(base, from_s, to_s));
    const double retardedBrightness = brightness(section(retarded, from_s, to_s));
    CAPTURE(baseBrightness);
    CAPTURE(retardedBrightness);
    CHECK(retardedBrightness < 0.9 * baseBrightness);
    // Duller, but not simply switched off: the level stays within a few dB.
    const double levelChange =
        decibels(rms(section(retarded, from_s, to_s)) / rms(section(base, from_s, to_s)));
    CHECK(std::abs(levelChange) < 4.0);
}

TEST_CASE("pedal steps and overrun fuel cut do not click") {
    const EngineDefinition& definition = test::referenceEngine().definition;
    EngineSimulation simulation(definition);
    SoundRenderer renderer(exhaustLayout(definition), static_cast<float>(kSampleRate), 1);
    renderer.setOutput(1.0, false);
    std::vector<float> output;
    std::vector<float> block(48 * kStereo); // 1 ms per simulation step
    bool sawLimiterCut = false;
    bool sawOverrunCut = false;
    for (int step = 0; step < 4000; ++step) {
        const double pedal = step < 1000 ? 0.1 : step < 2500 ? 1.0 : 0.0; // tip-in, lift-off
        const SimulationState state = simulation.step(pedal, 0.0);
        sawLimiterCut = sawLimiterCut || (state.command.fuelCut && pedal > 0.5);
        sawOverrunCut = sawOverrunCut || (state.command.fuelCut && pedal == 0.0);
        renderer.publish(acousticParams(state, definition));
        renderer.render(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    // Without a brake the free-revving engine reaches the limiter, then coasts in overrun cut.
    CHECK(sawLimiterCut);
    CHECK(sawOverrunCut);
    const std::vector<float> sound = left(output);
    CHECK(maxStep(sound) < kMaxClickStep);
    // Full load right after the tip-in is clearly louder than coasting without fuel.
    CHECK(rms(section(sound, 1.05, 1.2)) > 2.0 * rms(section(sound, 2.8, 3.5)));
    // The few firings between limiter cuts reach full strength: the limiter stays within
    // about 7 dB of the run-up instead of fading to a hiss (it fires only ~16 % of the time).
    CHECK(rms(section(sound, 1.5, 2.5)) > 0.42 * rms(section(sound, 1.05, 1.2)));
}

TEST_CASE("the renderer is silent until the simulation sends parameters") {
    SoundRenderer renderer(exhaustLayout(test::referenceEngine().definition),
                           static_cast<float>(kSampleRate), 1);
    std::vector<float> block(256 * kStereo, 1.0F);
    renderer.render(block);
    CHECK(std::all_of(block.begin(), block.end(), [](float s) { return s == 0.0F; }));
}

TEST_CASE("mute fades out without a click and volume is clamped") {
    const EngineDefinition& definition = test::referenceEngine().definition;
    EngineSimulation simulation(definition);
    SoundRenderer renderer(exhaustLayout(definition), static_cast<float>(kSampleRate), 1);
    renderer.setOutput(1.0, false);
    std::vector<float> output;
    std::vector<float> block(48 * kStereo);
    for (int step = 0; step < 1500; ++step) {
        if (step == 1000) {
            renderer.setOutput(1.0, true);
        }
        renderer.publish(acousticParams(simulation.step(1.0, 0.0), definition));
        renderer.render(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    const std::vector<float> sound = left(output);
    CHECK(maxStep(sound) < kMaxClickStep);
    CHECK(rms(section(sound, 1.3, 1.5)) < 0.01 * rms(section(sound, 0.7, 0.95)));

    renderer.setOutput(3.0, false);
    CHECK(renderer.volume() == 1.0F);
    renderer.setOutput(-1.0, false);
    CHECK(renderer.volume() == 0.0F);
}

namespace {

/// Number of 10 ms windows whose peak exceeds `threshold`: transients such as bangs.
int loudWindows(std::span<const float> mono, float threshold) {
    constexpr std::size_t kWindow = 480;
    int count = 0;
    for (std::size_t start = 0; start + kWindow <= mono.size(); start += kWindow) {
        float windowPeak = 0.0F;
        for (std::size_t i = start; i < start + kWindow; ++i) {
            windowPeak = std::max(windowPeak, std::abs(mono[i]));
        }
        count += windowPeak > threshold ? 1 : 0;
    }
    return count;
}

} // namespace

TEST_CASE("a pops tune bangs in overrun, the stock tune stays quiet") {
    const EngineDefinition& definition = test::referenceEngine().definition;
    const std::vector<float> stock = left(renderBlipSound(definition, OverrunSettings{}));
    const std::vector<float> pops =
        left(renderBlipSound(definition, OverrunSettings{false, 35.0, 0.1}));
    constexpr float kBangThreshold = 0.15F;
    // Second lift-off (8.0 s ...): the catalyst is hot from the pops of the first one.
    const std::vector<float> stockOverrun = section(stock, 8.1, 9.5);
    const std::vector<float> popsOverrun = section(pops, 8.1, 9.5);
    CHECK(loudWindows(stockOverrun, kBangThreshold) == 0);
    CHECK(loudWindows(popsOverrun, kBangThreshold) >= 5);
    CHECK(rms(popsOverrun) > 3.0 * rms(stockOverrun));
    const auto [low, high] = std::minmax_element(pops.begin(), pops.end());
    CHECK(*low >= -1.0F);
    CHECK(*high <= 1.0F);
}

TEST_CASE("switching the fuel cut off alone gives a soft burble without bangs") {
    const EngineDefinition& definition = test::referenceEngine().definition;
    const std::vector<float> stock = left(renderBlipSound(definition, OverrunSettings{}));
    const std::vector<float> noCut =
        left(renderBlipSound(definition, OverrunSettings{false, 0.0, 0.0}));
    const std::vector<float> overrun = section(noCut, 8.1, 9.5);
    CHECK(loudWindows(overrun, 0.15F) == 0);
    CHECK(rms(overrun) > 1.5 * rms(section(stock, 8.1, 9.5)));
}

TEST_CASE("a 2-step bangs at the launch speed, the stock limiter does not") {
    const EngineDefinition& definition = test::referenceEngine().definition;
    const std::vector<float> stock = left(renderLaunchSound(definition, LaunchSettings{}));
    const std::vector<float> twoStep =
        left(renderLaunchSound(definition, {true, units::rpmToRadS(4000.0), 10.0}));
    constexpr float kBangThreshold = 0.25F; // well above the stock rev limiter (about 0.17)
    // Late in the hold phase the catalyst is hot from the 2-step's own afterburn.
    const std::vector<float> stockHold = section(stock, 3.5, 6.0);
    const std::vector<float> twoStepHold = section(twoStep, 3.5, 6.0);
    CHECK(loudWindows(stockHold, kBangThreshold) == 0);
    CHECK(loudWindows(twoStepHold, kBangThreshold) >= 5);
    const auto [low, high] = std::minmax_element(twoStep.begin(), twoStep.end());
    CHECK(*low >= -1.0F);
    CHECK(*high <= 1.0F);
}
