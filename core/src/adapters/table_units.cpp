#include "adapters/table_units.hpp"

#include "domain/common/units.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace simcore::table_units {

namespace {

constexpr double powerOfTen(int exponent) {
    double result = 1.0;
    for (int i = 0; i < exponent; ++i) {
        result *= 10.0;
    }
    return result;
}

constexpr double kAxisScale = powerOfTen(kAxisDecimals);

double roundToAxisDecimals(double value) {
    return std::round(value * kAxisScale) / kAxisScale;
}

} // namespace

double toSi(std::string_view unit, double value) {
    if (unit == "1/min") {
        return units::rpmToRadS(value);
    }
    if (unit == "kPa") {
        return units::kpaToPa(value);
    }
    if (unit == "%") {
        return value / units::kPercent;
    }
    if (unit == "lambda" || unit == "deg BTDC") {
        return value; // dimensionless / crank angle in degrees by convention
    }
    throw std::invalid_argument("unknown unit '" + std::string(unit) + "'");
}

double fromSi(std::string_view unit, double value) {
    if (unit == "1/min") {
        return roundToAxisDecimals(units::radSToRpm(value));
    }
    if (unit == "kPa") {
        return roundToAxisDecimals(units::paToKpa(value));
    }
    if (unit == "%") {
        return value * units::kPercent;
    }
    if (unit == "lambda" || unit == "deg BTDC") {
        return value;
    }
    throw std::invalid_argument("unknown unit '" + std::string(unit) + "'");
}

} // namespace simcore::table_units
