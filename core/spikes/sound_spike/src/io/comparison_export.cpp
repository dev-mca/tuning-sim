#include "io/comparison_export.hpp"

#include "acoustic/offline_render.hpp"
#include "adapters/wav_writer.hpp"
#include "app/acoustic_names.hpp"

#include <string>

namespace sound_spike {

std::vector<std::filesystem::path> exportComparisonWavs(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    std::vector<std::filesystem::path> written;
    for (const LayoutId layout : kAllLayouts) {
        const std::vector<float> samples =
            renderOffline(layout, kComparisonDuration_s, comparisonScenario);
        const std::filesystem::path path = directory / (std::string(layoutKey(layout)) + ".wav");
        if (writeWav16(path, samples, kStereo, kSampleRate)) {
            written.push_back(path);
        }
    }
    return written;
}

} // namespace sound_spike
