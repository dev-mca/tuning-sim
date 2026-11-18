#include "app/sound_bank.hpp"

#include "io/offline_render.hpp"

#include <algorithm>

namespace sound_spike {

SoundBank::SoundBank(float sampleRate, double recordDuration_s)
    : fadeBuffer_(kMaxBlockFrames * kStereo, 0.0F),
      crossfadeFrames_(static_cast<std::size_t>(kCrossfade_s * static_cast<double>(sampleRate))),
      recordBuffer_(static_cast<std::size_t>(recordDuration_s * static_cast<double>(sampleRate)) *
                        kStereo,
                    0.0F),
      recordPosition_(recordBuffer_.size()) {
    for (std::size_t i = 0; i < kAllLayouts.size(); ++i) {
        sounds_[i] =
            std::make_unique<EngineSound>(makeLayout(kAllLayouts[i]), sampleRate, kDefaultSeed);
    }
    sound(activeLayout_).reset(targetSpeed_.load(), targetLoad_.load());
}

EngineSound& SoundBank::sound(LayoutId layout) noexcept {
    return *sounds_[static_cast<std::size_t>(layout)];
}

void SoundBank::startRecording() noexcept {
    recordRequested_.store(true);
}

bool SoundBank::recordingFinished() const noexcept {
    return !recordRequested_.load() && recordPosition_.load() == recordBuffer_.size();
}

std::span<const float> SoundBank::recording() const noexcept {
    return recordBuffer_;
}

void SoundBank::render(std::span<float> interleavedStereo) noexcept {
    const double speed = targetSpeed_.load();
    const float load = targetLoad_.load();

    const LayoutId requested = requestedLayout_.load();
    if (requested != activeLayout_) {
        // Continue the new layout from the current speed and load: no jump in pitch.
        EngineSound& previous = sound(activeLayout_);
        sound(requested).reset(previous.currentSpeedRpm(), previous.currentLoad());
        fadingOutLayout_ = activeLayout_;
        activeLayout_ = requested;
        crossfadeRemaining_ = crossfadeFrames_;
    }

    // Process in chunks so the crossfade buffer never needs to grow.
    for (std::size_t offset = 0; offset < interleavedStereo.size();
         offset += kMaxBlockFrames * kStereo) {
        const std::size_t length =
            std::min(kMaxBlockFrames * kStereo, interleavedStereo.size() - offset);
        std::span<float> block = interleavedStereo.subspan(offset, length);
        sound(activeLayout_).render(block, speed, load);
        if (crossfadeRemaining_ > 0) {
            std::span<float> fading(fadeBuffer_.data(), length);
            sound(fadingOutLayout_).render(fading, speed, load);
            for (std::size_t i = 0; i + 1 < length && crossfadeRemaining_ > 0; i += kStereo) {
                const float oldWeight =
                    static_cast<float>(crossfadeRemaining_) / static_cast<float>(crossfadeFrames_);
                block[i] = block[i] * (1.0F - oldWeight) + fading[i] * oldWeight;
                block[i + 1] = block[i + 1] * (1.0F - oldWeight) + fading[i + 1] * oldWeight;
                --crossfadeRemaining_;
            }
        }
    }
    reportedSpeed_.store(sound(activeLayout_).currentSpeedRpm());

    // Recording: copy into the preallocated buffer.
    if (recordRequested_.exchange(false)) {
        recordPosition_.store(0);
    }
    std::size_t position = recordPosition_.load();
    if (position < recordBuffer_.size()) {
        const std::size_t count =
            std::min(interleavedStereo.size(), recordBuffer_.size() - position);
        std::copy_n(interleavedStereo.begin(), count,
                    recordBuffer_.begin() + static_cast<std::ptrdiff_t>(position));
        recordPosition_.store(position + count);
    }
}

} // namespace sound_spike
