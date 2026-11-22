#include "app/sound_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace simcore {

SoundRenderer::SoundRenderer(const acoustic::LayoutSpec& layout, float sampleRate,
                             std::uint32_t seed)
    : sound_(layout, sampleRate, seed),
      gainCoefficient_(static_cast<float>(
          1.0 - std::exp(-1.0 / (kGainSmoothing_s * static_cast<double>(sampleRate))))) {}

void SoundRenderer::setOutput(double volume, bool muted) {
    volume_.store(static_cast<float>(std::clamp(volume, 0.0, 1.0)));
    muted_.store(muted);
}

void SoundRenderer::render(std::span<float> interleavedStereo) noexcept {
    if (params_.read(current_) && !receivedParams_) {
        // First parameters: start at the engine's current speed instead of running up from 0.
        receivedParams_ = true;
        const acoustic::SoundTarget start = acoustic::soundTargetFor(current_);
        sound_.reset(start.speed_rpm, start.load);
    }
    if (!receivedParams_) {
        std::fill(interleavedStereo.begin(), interleavedStereo.end(), 0.0F);
        return;
    }
    sound_.render(interleavedStereo, acoustic::soundTargetFor(current_));
    const float targetGain = muted_.load() ? 0.0F : volume_.load();
    for (std::size_t frame = 0; frame + 1 < interleavedStereo.size(); frame += 2) {
        gain_ += gainCoefficient_ * (targetGain - gain_);
        interleavedStereo[frame] *= gain_;
        interleavedStereo[frame + 1] *= gain_;
    }
}

} // namespace simcore
