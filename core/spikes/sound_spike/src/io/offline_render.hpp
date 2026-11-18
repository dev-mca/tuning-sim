#pragma once

#include "acoustic/layout.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace sound_spike {

inline constexpr std::uint32_t kSampleRate = 48000; // plan.md iteration 3
inline constexpr std::uint16_t kStereo = 2;
inline constexpr std::uint32_t kDefaultSeed = 12345;

/// Target speed and load over time for an offline render.
struct ScenarioPoint {
    double speed_rpm;
    float load;
};

/// The 10 s comparison scenario: idle, full-load run-up 900 -> 6500 1/min, hold, lift-off.
[[nodiscard]] ScenarioPoint comparisonScenario(double time_s);
inline constexpr double kComparisonDuration_s = 10.0;

/// Renders `duration_s` of a layout without an audio device. Interleaved stereo.
template <typename Scenario>
[[nodiscard]] std::vector<float> renderOffline(LayoutId layout, double duration_s,
                                               Scenario scenario,
                                               std::uint32_t seed = kDefaultSeed);

/// Constant speed and load (used by the order analysis tests).
[[nodiscard]] std::vector<float> renderConstant(LayoutId layout, double duration_s,
                                                double speed_rpm, float load,
                                                std::uint32_t seed = kDefaultSeed);

/// Renders the comparison scenario for every layout to `<directory>/<layout>.wav`.
/// Returns the written files.
[[nodiscard]] std::vector<std::filesystem::path>
exportComparisonWavs(const std::filesystem::path& directory);

} // namespace sound_spike

#include "io/offline_render_impl.hpp"
