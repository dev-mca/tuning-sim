#pragma once

// Unit conversions at the edges of the system (docs/research.md §0). Inside: SI only.
// Mirrors lab/src/tuning_lab/units.py expression by expression, so that C++ and Python
// produce the same floating-point results (needed for the golden file test).

#include <numbers>

namespace simcore::units {

inline constexpr double kSecondsPerMinute = 60.0;
inline constexpr double kPascalPerKilopascal = 1.0e3;
inline constexpr double kPascalPerBar = 1.0e5;
inline constexpr double kWattPerKilowatt = 1.0e3;
inline constexpr double kWattPerPs = 735.5; // metric horsepower
inline constexpr double kKelvinOffset = 273.15;
inline constexpr double kPercent = 100.0;

constexpr double rpmToRadS(double speed_rpm) {
    return speed_rpm * (2.0 * std::numbers::pi / kSecondsPerMinute);
}
constexpr double radSToRpm(double speed_rad_s) {
    return speed_rad_s * (kSecondsPerMinute / (2.0 * std::numbers::pi));
}
constexpr double kpaToPa(double pressure_kPa) {
    return pressure_kPa * kPascalPerKilopascal;
}
constexpr double paToKpa(double pressure_Pa) {
    return pressure_Pa / kPascalPerKilopascal;
}
constexpr double paToBar(double pressure_Pa) {
    return pressure_Pa / kPascalPerBar;
}
constexpr double wToKw(double power_W) {
    return power_W / kWattPerKilowatt;
}
constexpr double wToPs(double power_W) {
    return power_W / kWattPerPs;
}
constexpr double kelvinToCelsius(double temperature_K) {
    return temperature_K - kKelvinOffset;
}
constexpr double celsiusToKelvin(double temperature_C) {
    return temperature_C + kKelvinOffset;
}

} // namespace simcore::units
