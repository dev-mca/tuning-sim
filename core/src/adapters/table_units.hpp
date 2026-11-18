#pragma once

// Conversion between the units written in table files / protocol messages and SI.
// Same table as _TO_SI in lab/src/tuning_lab/table.py. Crank angles stay in degrees.

#include <string_view>

namespace simcore::table_units {

/// Throws std::invalid_argument for unknown units.
[[nodiscard]] double toSi(std::string_view unit, double value);
[[nodiscard]] double fromSi(std::string_view unit, double value);

} // namespace simcore::table_units
