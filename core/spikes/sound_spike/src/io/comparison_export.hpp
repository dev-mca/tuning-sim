#pragma once

#include <filesystem>
#include <vector>

namespace sound_spike {

/// Renders the comparison scenario for every layout to `<directory>/<layout>.wav`.
/// Returns the written files.
[[nodiscard]] std::vector<std::filesystem::path>
exportComparisonWavs(const std::filesystem::path& directory);

} // namespace sound_spike
