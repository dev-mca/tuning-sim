#pragma once

// Template implementation of renderOffline (templates must be visible in the header).

#include "acoustic/engine_sound.hpp"

#include <algorithm>
#include <cstddef>
#include <span>

namespace sound_spike {

template <typename Scenario>
std::vector<float> renderOffline(LayoutId layout, double duration_s, Scenario scenario,
                                 std::uint32_t seed) {
    constexpr std::size_t kBlockFrames = 256; // parameters update per block, like a device
    const auto sampleRate = static_cast<float>(kSampleRate);
    const auto totalFrames = static_cast<std::size_t>(duration_s * kSampleRate);
    std::vector<float> output(totalFrames * kStereo, 0.0F);

    EngineSound sound(makeLayout(layout), sampleRate, seed);
    const ScenarioPoint start = scenario(0.0);
    sound.reset(start.speed_rpm, start.load);
    for (std::size_t frame = 0; frame < totalFrames; frame += kBlockFrames) {
        const std::size_t frames = std::min(kBlockFrames, totalFrames - frame);
        const ScenarioPoint target = scenario(static_cast<double>(frame) / kSampleRate);
        sound.render(std::span<float>(output).subspan(frame * kStereo, frames * kStereo),
                     target.speed_rpm, target.load);
    }
    return output;
}

} // namespace sound_spike
