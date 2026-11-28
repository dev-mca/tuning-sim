#pragma once

// Conversion between the units written in table files / protocol messages and SI.
// Same table as _TO_SI in lab/src/tuning_lab/table.py. Crank angles stay in degrees.

#include <string_view>

namespace simcore::table_units {

/// Decimals kept when converting an axis unit (1/min, kPa) back for display. rad/s -> 1/min
/// is not exact in binary floating point (1400 becomes 1400.0000000000002); 6 decimals remove
/// that noise and are far finer than any meaningful operating point.
inline constexpr int kAxisDecimals = 6;

/// Throws std::invalid_argument for unknown units.
[[nodiscard]] double toSi(std::string_view unit, double value);

/// Results in the axis units 1/min and kPa are rounded to kAxisDecimals. No cell table uses
/// these units, and commands address cells by index, never by axis value, so only the display
/// of axis bins changes. Cell units (%, lambda, deg BTDC) are returned unrounded.
[[nodiscard]] double fromSi(std::string_view unit, double value);

} // namespace simcore::table_units
