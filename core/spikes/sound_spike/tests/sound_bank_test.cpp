// Tests of the spike's real-time front end (SoundBank): layout crossfade and recording.

#include "acoustic/offline_render.hpp"
#include "app/sound_bank.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

using namespace sound_spike;

namespace {

constexpr double kRate = 48000.0;

double maxStep(std::span<const float> interleaved) {
    double largest = 0.0;
    for (std::size_t i = 2; i < interleaved.size(); ++i) {
        largest =
            std::max(largest, static_cast<double>(std::abs(interleaved[i] - interleaved[i - 2])));
    }
    return largest;
}

} // namespace

TEST_CASE("switching the layout while playing crossfades without a click") {
    constexpr std::size_t kBlockFrames = 512;
    SoundBank bank(static_cast<float>(kRate), 0.1);
    bank.setTargetSpeed(4000.0);
    bank.setTargetLoad(1.0F);
    std::vector<float> output;
    std::vector<float> block(kBlockFrames * kStereo);
    for (std::size_t i = 0; i < 200; ++i) { // about 2.1 s
        if (i == 100) {
            bank.selectLayout(LayoutId::V8Crossplane);
        }
        bank.render(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    CHECK(maxStep(output) < 0.2);
    CHECK(bank.currentSpeedRpm() == doctest::Approx(4000.0).epsilon(0.01));
}

TEST_CASE("recording captures exactly the requested duration") {
    SoundBank bank(static_cast<float>(kRate), 0.05); // 2400 frames
    std::vector<float> block(1000 * kStereo);
    bank.startRecording();
    CHECK_FALSE(bank.recordingFinished());
    for (int i = 0; i < 3; ++i) {
        bank.render(block);
    }
    CHECK(bank.recordingFinished());
    CHECK(bank.recording().size() == 2400 * kStereo);
}
