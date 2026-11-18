#pragma once

// Throttle and intake manifold (filling and emptying model), docs/research.md §2.5.
// Port of lab/src/tuning_lab/intake.py.

#include "domain/common/constants.hpp"
#include "domain/engine/engine_definition.hpp"

namespace simcore::intake {

/// Above this pressure ratio the flow function becomes a straight line to zero at Pi = 1.
/// Psi has an infinite slope at Pi = 1, which makes the manifold equation extremely stiff
/// near wide-open throttle; the linear section keeps the slope finite (see lab intake.py).
inline constexpr double kPressureRatioLinear = 0.99;
inline constexpr double kFullOpenAngle_deg = 90.0;

/// Pi_crit = (2/(gamma+1))^(gamma/(gamma-1)), about 0.528 for air.
[[nodiscard]] double criticalPressureRatio(double gamma = constants::kGammaAir) noexcept;

/// Dimensionless flow function Psi(Pi), Pi = p_down / p_up; choked below Pi_crit.
[[nodiscard]] double flowFunction(double pressureRatio,
                                  double gamma = constants::kGammaAir) noexcept;

/// Open throttle area for position 0..1: pi D^2/4 (1 - cos a / cos a0) + leak area.
[[nodiscard]] double throttleArea_m2(double throttle, const IntakeParameters& intake) noexcept;

/// Cd A p_up / sqrt(R T_up) Psi(p_down/p_up); negative if the flow reverses.
[[nodiscard]] double orificeMassFlow_kg_s(double area_m2, double dischargeCoefficient,
                                          double upstreamPressure_Pa, double upstreamTemperature_K,
                                          double downstreamPressure_Pa) noexcept;

/// dp_m/dt = R T_m / V_m (mdot_throttle - mdot_cylinders).
[[nodiscard]] double manifoldPressureDerivative_Pa_s(double throttleFlow_kg_s,
                                                     double cylinderFlow_kg_s,
                                                     double manifoldTemperature_K,
                                                     double manifoldVolume_m3) noexcept;

} // namespace simcore::intake
