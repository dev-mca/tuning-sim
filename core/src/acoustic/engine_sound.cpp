#include "acoustic/engine_sound.hpp"

#include "dsp/random.hpp"

#include <algorithm>
#include <cmath>

namespace simcore::acoustic {

namespace {

constexpr std::size_t kMufflerTaps = 256; // about 5 ms at 48 kHz
constexpr float kMufflerTailLevel = 0.35F;
constexpr float kMufflerDecay_s = 0.0015F;
constexpr float kMufflerTailCutoffHz = 2500.0F;

// Tone: closed throttle sounds dull, full load bright (more high-frequency energy at EVO).
constexpr float kToneCutoffAtZeroLoadHz = 1200.0F;
constexpr float kToneCutoffAtFullLoadHz = 5500.0F;
constexpr std::size_t kToneUpdateInterval = 32; // samples; filter coefficients cost cos/sin

constexpr float kDcCutoffHz = 20.0F; // research.md §6.8
constexpr std::uint32_t kMufflerSeedOffset = 0x51ED270BU;

} // namespace

std::vector<float> makeMufflerResponse(float sampleRate, std::uint32_t seed) {
    std::vector<float> response(kMufflerTaps, 0.0F);
    response[0] = 1.0F; // direct path
    Random random(seed ^ kMufflerSeedOffset);
    OnePoleLowpass smoothing(kMufflerTailCutoffHz, sampleRate);
    for (std::size_t n = 1; n < kMufflerTaps; ++n) {
        const float time_s = static_cast<float>(n) / sampleRate;
        const float envelope = std::exp(-time_s / kMufflerDecay_s);
        response[n] = kMufflerTailLevel * envelope * smoothing.process(random.uniform());
    }
    return response;
}

EngineSound::Channel::Channel(const std::vector<float>& mufflerResponse, float sampleRate)
    : muffler(mufflerResponse), dcBlocker(kDcCutoffHz, sampleRate) {}

EngineSound::EngineSound(const LayoutSpec& layout, float sampleRate, std::uint32_t seed)
    : sampleRate_(sampleRate), pulses_(layout.cylinders, sampleRate, seed),
      exhaust_(layout, sampleRate), sources_(layout.cylinders.size(), 0.0F),
      speed_(kMaxSpeedChange_rpm_s, kSpeedSmoothing_s, static_cast<double>(sampleRate)),
      load_(1.0e9, kLoadSmoothing_s, static_cast<double>(sampleRate)),
      brightness_(1.0e9, kBrightnessSmoothing_s, static_cast<double>(sampleRate)),
      channels_{Channel(makeMufflerResponse(sampleRate, seed), sampleRate),
                Channel(makeMufflerResponse(sampleRate, seed), sampleRate)} {
    reset(0.0, 0.0F);
}

void EngineSound::reset(double speed_rpm, float load) noexcept {
    pulses_.reset();
    exhaust_.reset();
    speed_.jumpTo(speed_rpm);
    load_.jumpTo(static_cast<double>(load));
    brightness_.jumpTo(1.0);
    for (Channel& channel : channels_) {
        channel.muffler.reset();
        channel.tone.reset();
        channel.dcBlocker.reset();
    }
    samplesUntilToneUpdate_ = 0;
}

void EngineSound::updateTone() noexcept {
    const float load = std::min(currentLoad(), 1.0F);
    const auto brightness = static_cast<float>(brightness_.value());
    const float cutoff = std::max(
        (kToneCutoffAtZeroLoadHz + (kToneCutoffAtFullLoadHz - kToneCutoffAtZeroLoadHz) * load) *
            brightness,
        kMinToneCutoffHz);
    for (Channel& channel : channels_) {
        channel.tone.setCutoff(cutoff, sampleRate_);
    }
}

void EngineSound::render(std::span<float> interleavedStereo, const SoundTarget& target) noexcept {
    const double clampedLoad =
        std::clamp(static_cast<double>(target.load), 0.0, static_cast<double>(kMaxLoad));
    const double clampedBrightness = std::clamp(static_cast<double>(target.brightness), 0.0, 1.0);
    pulses_.setCycleVariation(std::max(target.cycleVariation, 0.0F));
    for (std::size_t frame = 0; frame + 1 < interleavedStereo.size(); frame += 2) {
        const double speed = speed_.process(std::max(0.0, target.speed_rpm));
        const auto load = static_cast<float>(load_.process(clampedLoad));
        pulses_.setSoftness(static_cast<float>(1.0 - brightness_.process(clampedBrightness)));
        if (samplesUntilToneUpdate_ == 0) {
            updateTone();
            samplesUntilToneUpdate_ = kToneUpdateInterval;
        }
        --samplesUntilToneUpdate_;

        pulses_.process(speed, load, sources_);
        const StereoSample radiated = exhaust_.process(sources_);
        const std::array<float, 2> raw = {radiated.left, radiated.right};
        for (std::size_t channel = 0; channel < 2; ++channel) {
            Channel& c = channels_[channel];
            float sample = c.muffler.process(raw[channel]);
            sample = c.tone.process(sample);
            sample = c.dcBlocker.process(sample);
            interleavedStereo[frame + channel] = softClip(kOutputGain * sample);
        }
    }
}

} // namespace simcore::acoustic
