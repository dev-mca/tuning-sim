#include "adapters/wav_writer.hpp"
#include "dsp/delay_line.hpp"
#include "dsp/filters.hpp"
#include "dsp/fir_filter.hpp"
#include "dsp/random.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <vector>

using namespace simcore::acoustic;
using simcore::encodeWav16;

TEST_CASE("delay line returns samples after an integer delay") {
    DelayLine line(16.0);
    for (int i = 1; i <= 10; ++i) {
        line.write(static_cast<float>(i));
    }
    CHECK(line.read(1.0) == doctest::Approx(10.0));
    CHECK(line.read(4.0) == doctest::Approx(7.0));
}

TEST_CASE("delay line interpolates fractional delays linearly") {
    DelayLine line(16.0);
    for (int i = 1; i <= 10; ++i) {
        line.write(static_cast<float>(i));
    }
    CHECK(line.read(2.5) == doctest::Approx(8.5));
    line.reset();
    CHECK(line.read(3.0) == doctest::Approx(0.0));
}

TEST_CASE("FIR filter reproduces its impulse response") {
    FirFilter filter({0.5F, 0.25F, 0.125F});
    CHECK(filter.process(1.0F) == doctest::Approx(0.5));
    CHECK(filter.process(0.0F) == doctest::Approx(0.25));
    CHECK(filter.process(0.0F) == doctest::Approx(0.125));
    CHECK(filter.process(0.0F) == doctest::Approx(0.0));
}

TEST_CASE("DC blocker removes a constant offset") {
    DcBlocker blocker(20.0F, 48000.0F);
    float output = 1.0F;
    for (int i = 0; i < 48000; ++i) {
        output = blocker.process(1.0F);
    }
    CHECK(std::abs(output) < 1.0e-3F);
}

TEST_CASE("slew smoother limits the rate of change") {
    constexpr double kSampleRate = 1000.0;
    SlewSmoother smoother(1000.0, 1.0e-6, kSampleRate); // 1 unit per sample, no smoothing
    smoother.jumpTo(0.0);
    CHECK(smoother.process(100.0) == doctest::Approx(1.0));
    CHECK(smoother.process(100.0) == doctest::Approx(2.0));
}

TEST_CASE("random generator is deterministic and stays in range") {
    Random a(42);
    Random b(42);
    for (int i = 0; i < 1000; ++i) {
        const float value = a.uniform();
        CHECK(value == b.uniform());
        CHECK(value >= -1.0F);
        CHECK(value < 1.0F);
    }
}

TEST_CASE("WAV header describes 16-bit stereo PCM") {
    const std::vector<float> samples = {0.0F, 1.0F, -1.0F, 0.5F};
    const std::vector<std::uint8_t> bytes = encodeWav16(samples, 2, 48000);
    REQUIRE(bytes.size() == 44 + samples.size() * 2);
    const auto le32 = [&](std::size_t at) {
        return static_cast<std::uint32_t>(bytes[at]) |
               static_cast<std::uint32_t>(bytes[at + 1]) << 8U |
               static_cast<std::uint32_t>(bytes[at + 2]) << 16U |
               static_cast<std::uint32_t>(bytes[at + 3]) << 24U;
    };
    CHECK(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF");
    CHECK(le32(4) == 36 + 8);
    CHECK(std::string(bytes.begin() + 8, bytes.begin() + 16) == "WAVEfmt ");
    CHECK(le32(24) == 48000);         // sample rate
    CHECK(le32(28) == 48000 * 2 * 2); // byte rate
    CHECK(le32(40) == samples.size() * 2);
    CHECK(bytes[46] == 0xFF); // +1.0 -> 32767 = 0x7FFF, little-endian
    CHECK(bytes[47] == 0x7F);
}
