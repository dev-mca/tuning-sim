#include "adapters/table_units.hpp"

#include "domain/common/units.hpp"

#include <stdexcept>
#include <string>

namespace simcore::table_units {

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
        return units::radSToRpm(value);
    }
    if (unit == "kPa") {
        return units::paToKpa(value);
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
