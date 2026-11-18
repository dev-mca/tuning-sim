#pragma once

// Mean-value engine model: air, fuel, torque, losses, exhaust (docs/research.md §2).
// Port of lab/src/tuning_lab/mvm.py; everything per engine cycle (720 deg), so torque stays
// defined at standstill. Mass flows = per cycle * cycles per second (omega / 4 pi).

#include "domain/engine/engine_definition.hpp"

namespace simcore {

struct OperatingPoint {
    double speed_rad_s;
    double manifoldPressure_Pa;
    double manifoldTemperature_K;
    double fuelMassPerCycle_kg;
    double ignition_deg; // advance, deg BTDC
};

struct EngineOutputs {
    double volumetricEfficiency;
    double airMassPerCycle_kg;
    double airMassFlow_kg_s;
    double fuelMassFlow_kg_s;
    double lambdaActual; // +infinity without fuel (fuel cut)
    double ignitionMbt_deg;
    double ignitionEfficiency;
    double lambdaEfficiency;
    double indicatedTorque_Nm;
    double frictionTorque_Nm;
    double pumpingTorque_Nm;
    double accessoryTorque_Nm;
    double brakeTorque_Nm;
    double exhaustPressure_Pa;
    double exhaustTemperature_K;
};

/// rho = p / (R T) (research.md §2.1).
[[nodiscard]] double airDensity_kg_m3(double pressure_Pa, double temperature_K) noexcept;
/// T = mep V_d / (4 pi) (research.md §2.8).
[[nodiscard]] double meanEffectivePressureToTorque_Nm(double mep_Pa,
                                                      double displacement_m3) noexcept;
/// bmep = 4 pi T / V_d (research.md §2.9).
[[nodiscard]] double torqueToBmep_Pa(double torque_Nm, double displacement_m3) noexcept;
/// eta_ign = max(0, 1 - k (theta - theta_MBT)^2) (research.md §2.7).
[[nodiscard]] double ignitionEfficiency(double ignition_deg, double ignitionMbt_deg,
                                        const CombustionParameters& combustion) noexcept;
/// Parabola with maximum 1 at lambdaOptimum, 0 for infinite lambda (research.md §2.6).
[[nodiscard]] double lambdaEfficiency(double lambda,
                                      const CombustionParameters& combustion) noexcept;

/// The "real" engine. Its true VE may differ from the VE table the ECU believes in
/// (research.md §2.4) – that difference is what VE tuning is about.
class EngineModel {
public:
    /// Uses definition.tables.ve as true VE unless `trueVe` is given.
    explicit EngineModel(const EngineDefinition& definition, const Table2D* trueVe = nullptr);

    /// m_air,cyc = eta_v rho_m V_d (research.md §2.2).
    [[nodiscard]] double airMassPerCycle_kg(double speed_rad_s, double manifoldPressure_Pa,
                                            double manifoldTemperature_K) const noexcept;
    /// mdot_air = m_air,cyc omega / (4 pi) (research.md §2.2).
    [[nodiscard]] double airMassFlow_kg_s(double speed_rad_s, double manifoldPressure_Pa,
                                          double manifoldTemperature_K) const noexcept;

    [[nodiscard]] EngineOutputs evaluate(const OperatingPoint& point) const noexcept;

private:
    EngineDefinition
        definition_; // own copy: no dangling reference, edits to the ECU tune stay separate
    Table2D trueVe_;
    double displacement_m3_;
};

} // namespace simcore
