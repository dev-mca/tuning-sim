#pragma once

// Engine definition in SI units (docs/plan.md §5.2). Loaded from shared/engines/<id>.json by
// an adapter (adapters/json_file_repository); the domain never touches files.

#include "domain/common/units.hpp"
#include "domain/ecu/table.hpp"

#include <numbers>
#include <string>
#include <vector>

namespace simcore {

struct IntakeParameters {
    double manifoldVolume_m3;
    double throttleDiameter_m;
    double throttleClosedAngle_deg;
    double throttleDischargeCoefficient;
    double throttleLeakArea_m2;
};

struct CombustionParameters {
    double indicatedEfficiencyBase;
    double lambdaOptimum;
    double lambdaCurvature;
    double ignitionCurvature_per_deg2;
    double coolingFraction;
};

/// fmep = c0 + c1 (N/1000) + c2 (N/1000)^2 with N in 1/min (research.md §2.8).
struct FrictionParameters {
    double c0_Pa;
    double c1_Pa_per_krpm;
    double c2_Pa_per_krpm2;

    [[nodiscard]] double fmep_Pa(double speed_rad_s) const noexcept {
        constexpr double kRpmPerKrpm = 1.0e3;
        const double krpm = units::radSToRpm(speed_rad_s) / kRpmPerKrpm;
        return c0_Pa + c1_Pa_per_krpm * krpm + c2_Pa_per_krpm2 * krpm * krpm;
    }
};

/// ECU tune (ve, lambdaTarget, ignition) plus the model property ignitionMbt.
/// Axes: x = engine speed [rad/s], y = manifold pressure [Pa]. Values: ve as fraction,
/// lambda dimensionless, ignition in deg BTDC.
struct EngineTables {
    Table2D ve;
    Table2D lambdaTarget;
    Table2D ignition;
    Table2D ignitionMbt;
};

struct EngineDefinition {
    std::string id;
    std::string name;
    int cylinders;
    double bore_m;
    double stroke_m;
    double compressionRatio;
    std::vector<int> firingOrder;
    std::vector<double> firingAngles_deg;
    double idleSpeed_rad_s;
    double revLimit_rad_s;
    double revLimitHysteresis_rad_s;
    double inertia_kgm2;
    IntakeParameters intake;
    CombustionParameters combustion;
    FrictionParameters friction;
    double accessoryTorque_Nm;
    double exhaustBackpressureCoefficient;
    EngineTables tables;

    /// V_d = z pi/4 B^2 S (research.md §1.4).
    [[nodiscard]] double displacement_m3() const noexcept {
        return cylinders * std::numbers::pi / 4.0 * (bore_m * bore_m) * stroke_m;
    }
};

} // namespace simcore
