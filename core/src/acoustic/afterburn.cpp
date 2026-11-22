#include "acoustic/afterburn.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simcore::acoustic {

namespace {

constexpr float kCrackCutoffHz = 3500.0F;
constexpr std::uint32_t kSeedOffset = 0xBA9E5F01U;

} // namespace

AfterburnSource::AfterburnSource(float sampleRate, std::uint32_t seed)
    : sampleRate_(sampleRate), seed_(seed ^ kSeedOffset), random_(seed_),
      crackColour_(kCrackCutoffHz, sampleRate) {}

void AfterburnSource::reset() noexcept {
    random_ = Random(seed_);
    bangs_.fill(Bang{});
    crackColour_.reset();
}

float AfterburnSource::decayFactor(double time_s) const noexcept {
    // Envelope e^(-t / time) sampled per sample.
    return static_cast<float>(std::exp(-1.0 / (time_s * static_cast<double>(sampleRate_))));
}

void AfterburnSource::onExhaustEvent(float rate) noexcept {
    if (rate <= 0.0F || uniform01() >= rate) {
        return;
    }
    const auto free =
        std::find_if(bangs_.begin(), bangs_.end(), [](const Bang& b) { return !b.active; });
    if (free == bangs_.end()) {
        return; // all slots busy: a bang more or less is inaudible in a burst
    }
    const auto lerp = [this](double low, double high) {
        return low + (high - low) * static_cast<double>(uniform01());
    };
    const float size = uniform01();
    Bang& bang = *free;
    bang.active = true;
    bang.age = 0.0;
    bang.delaySamples = lerp(kMinDelay_s, kMaxDelay_s) * static_cast<double>(sampleRate_);
    bang.amplitude = kSmallAmplitude + (kBigAmplitude - kSmallAmplitude) * size * size * size;
    bang.crackDecay = decayFactor(lerp(kMinCrackDecay_s, kMaxCrackDecay_s));
    bang.thumpDecay = decayFactor(kThumpDecay_s);
    bang.crackEnvelope = 1.0F;
    bang.thumpEnvelope = 1.0F;
    bang.thumpPhase = 0.0;
    bang.thumpIncrement =
        2.0 * std::numbers::pi * lerp(kMinThumpHz, kMaxThumpHz) / static_cast<double>(sampleRate_);
}

float AfterburnSource::process() noexcept {
    float noise = 0.0F;
    float thump = 0.0F;
    bool anyActive = false;
    for (Bang& bang : bangs_) {
        if (!bang.active) {
            continue;
        }
        anyActive = true;
        if (bang.delaySamples > 0.0) {
            bang.delaySamples -= 1.0;
            continue;
        }
        noise += bang.amplitude * bang.crackEnvelope;
        thump +=
            bang.amplitude * bang.thumpEnvelope * static_cast<float>(std::sin(bang.thumpPhase));
        bang.crackEnvelope *= bang.crackDecay;
        bang.thumpEnvelope *= bang.thumpDecay;
        bang.thumpPhase += bang.thumpIncrement;
        bang.age += 1.0;
        bang.active = bang.thumpEnvelope > 1.0e-3F;
    }
    if (!anyActive) {
        return crackColour_.process(0.0F);
    }
    // One noise generator for all bangs; its level follows the summed crack envelopes.
    const float crack = crackColour_.process(noise * random_.uniform());
    return (1.0F - kThumpShare) * crack + kThumpShare * thump;
}

} // namespace simcore::acoustic
