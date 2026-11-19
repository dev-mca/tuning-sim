// Mapping from the simulation's operating point to the sound target (research.md §6.2).

#include "acoustic/acoustic_params.hpp"

#include <doctest/doctest.h>

#include <limits>
#include <numbers>

using namespace simcore::acoustic;

namespace {

AcousticParams fullLoadAt3000() {
    AcousticParams params;
    params.speed_rad_s = 3000.0 * 2.0 * std::numbers::pi / 60.0;
    params.chargeRatio = 0.95;
    params.ignitionRetard_deg = 0.0;
    params.lambda = 0.88;
    return params;
}

} // namespace

TEST_CASE("speed is converted to rpm and the charge becomes the load") {
    const SoundTarget target = soundTargetFor(fullLoadAt3000());
    CHECK(target.speed_rpm == doctest::Approx(3000.0));
    CHECK(target.load == doctest::Approx(0.95F));
    CHECK(target.brightness == doctest::Approx(1.0F));
    CHECK(target.cycleVariation == doctest::Approx(PulseSource::kDefaultAmplitudeVariation));
}

TEST_CASE("retarded ignition is slightly louder but clearly duller") {
    AcousticParams retarded = fullLoadAt3000();
    retarded.ignitionRetard_deg = 10.0;
    const SoundTarget base = soundTargetFor(fullLoadAt3000());
    const SoundTarget late = soundTargetFor(retarded);
    CHECK(late.load > base.load);
    CHECK(late.load < base.load * 1.2F);
    CHECK(late.brightness < 0.6F);
}

TEST_CASE("a lean mixture sounds rougher, with an upper limit") {
    AcousticParams lean = fullLoadAt3000();
    lean.lambda = 1.15;
    CHECK(soundTargetFor(lean).cycleVariation == doctest::Approx(0.15F));
    lean.lambda = 2.0;
    CHECK(soundTargetFor(lean).cycleVariation == doctest::Approx(kMaxCycleVariation));
}

TEST_CASE("fuel cut leaves only dull pumping pulses") {
    AcousticParams cut = fullLoadAt3000();
    cut.fuelCut = true;
    cut.lambda = std::numeric_limits<double>::infinity();
    const SoundTarget target = soundTargetFor(cut);
    CHECK(target.load == 0.0F);
    CHECK(target.brightness == doctest::Approx(kFuelCutBrightness));
    CHECK(target.speed_rpm == doctest::Approx(3000.0));
}

TEST_CASE("out-of-range inputs are clamped") {
    AcousticParams odd = fullLoadAt3000();
    odd.speed_rad_s = -5.0;
    odd.chargeRatio = 1.6;
    odd.ignitionRetard_deg = -8.0; // early ignition: no retard effect
    const SoundTarget target = soundTargetFor(odd);
    CHECK(target.speed_rpm == 0.0);
    CHECK(target.load == doctest::Approx(1.0F));
    CHECK(target.brightness == doctest::Approx(1.0F));
}
