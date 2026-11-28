// Unit conversion at the edge between table files / protocol and SI (adapters/table_units).

#include "adapters/table_units.hpp"
#include "domain/common/units.hpp"

#include <doctest/doctest.h>

#include <array>
#include <stdexcept>

using namespace simcore;

namespace {

// Axis bins of the reference tables (shared/tables/i6_30_na/*.json).
constexpr std::array<double, 16> kRpmBins = {800,  1100, 1400, 1800, 2200, 2600, 3000, 3500,
                                             4000, 4500, 5000, 5500, 6000, 6500, 7000, 7500};
constexpr std::array<double, 16> kMapBins_kPa = {20, 25, 30, 35, 40, 45, 50,  55,
                                                 60, 65, 70, 75, 80, 90, 100, 105};

} // namespace

TEST_CASE("rpm axis bins survive the round trip through rad/s exactly") {
    // Without rounding 1400 1/min comes back as 1400.0000000000002.
    CHECK(units::radSToRpm(units::rpmToRadS(1400.0)) != 1400.0);
    for (const double rpm : kRpmBins) {
        CHECK(table_units::fromSi("1/min", table_units::toSi("1/min", rpm)) == rpm);
    }
}

TEST_CASE("kPa axis bins survive the round trip through Pa exactly") {
    for (const double map_kPa : kMapBins_kPa) {
        CHECK(table_units::fromSi("kPa", table_units::toSi("kPa", map_kPa)) == map_kPa);
    }
}

TEST_CASE("axis units are rounded to kAxisDecimals, finer values are kept") {
    CHECK(table_units::fromSi("1/min", units::rpmToRadS(812.345678)) == 812.345678);
    CHECK(table_units::fromSi("1/min", units::rpmToRadS(812.3456784)) == 812.345678);
    CHECK(table_units::fromSi("kPa", 101'325.0) == 101.325);
    CHECK(table_units::fromSi("kPa", 1.0e-4) == 0.0); // below 1e-6 kPa
}

TEST_CASE("cell units are not rounded") {
    // 6 decimals would turn this into 12.345679 %.
    CHECK(table_units::fromSi("%", 0.123456789123) == 0.123456789123 * units::kPercent);
    CHECK(table_units::fromSi("lambda", 0.8712345678912) == 0.8712345678912);
    CHECK(table_units::fromSi("deg BTDC", 12.3456789123) == 12.3456789123);
}

TEST_CASE("toSi is unchanged and keeps full precision") {
    CHECK(table_units::toSi("1/min", 1100.0) == units::rpmToRadS(1100.0));
    CHECK(table_units::toSi("kPa", 30.0) == 30'000.0);
    CHECK(table_units::toSi("%", 85.0) == 85.0 / units::kPercent);
}

TEST_CASE("unknown units throw") {
    CHECK_THROWS_AS((void)table_units::toSi("furlong", 1.0), std::invalid_argument);
    CHECK_THROWS_AS((void)table_units::fromSi("furlong", 1.0), std::invalid_argument);
}
