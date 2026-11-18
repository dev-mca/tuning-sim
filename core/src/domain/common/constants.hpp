#pragma once

// Physical constants and fuel properties in SI units (docs/research.md §0).
// Same values as lab/src/tuning_lab/constants.py.

#include <numbers>

namespace simcore::constants {

inline constexpr double kGasConstantAir_J_kgK = 287.05;
inline constexpr double kGammaAir = 1.40;
inline constexpr double kCpExhaust_J_kgK = 1150.0;
inline constexpr double kAfrStoichGasoline = 14.7;
inline constexpr double kLowerHeatingValueGasoline_J_kg = 43.0e6;
inline constexpr double kAmbientPressure_Pa = 101'325.0;
inline constexpr double kAmbientTemperature_K = 298.15; // 25 degC, also intake manifold
/// One four-stroke cycle = two revolutions = 4 pi rad (research.md §1.1).
inline constexpr double kRadiansPerCycle = 4.0 * std::numbers::pi;

} // namespace simcore::constants
