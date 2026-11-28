#include "domain/engine/engine_model.hpp"

#include "domain/common/constants.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace simcore {

using constants::kRadiansPerCycle;

double airDensity_kg_m3(double pressure_Pa, double temperature_K) noexcept {
    return pressure_Pa / (constants::kGasConstantAir_J_kgK * temperature_K);
}

double meanEffectivePressureToTorque_Nm(double mep_Pa, double displacement_m3) noexcept {
    return mep_Pa * displacement_m3 / kRadiansPerCycle;
}

double torqueToBmep_Pa(double torque_Nm, double displacement_m3) noexcept {
    return kRadiansPerCycle * torque_Nm / displacement_m3;
}

double ignitionEfficiency(double ignition_deg, double ignitionMbt_deg,
                          const CombustionParameters& combustion) noexcept {
    const double deviation = ignition_deg - ignitionMbt_deg;
    return std::max(0.0, 1.0 - combustion.ignitionCurvature_per_deg2 * deviation * deviation);
}

double lambdaEfficiency(double lambda, const CombustionParameters& combustion) noexcept {
    if (std::isinf(lambda)) {
        return 0.0;
    }
    const double deviation = lambda - combustion.lambdaOptimum;
    return std::max(0.0, 1.0 - combustion.lambdaCurvature * deviation * deviation);
}

EngineModel::EngineModel(const EngineDefinition& definition, const Table2D* trueVe)
    : definition_(definition), trueVe_(trueVe != nullptr ? *trueVe : definition.tables.ve),
      displacement_m3_(definition.displacement_m3()) {}

double EngineModel::airMassPerCycle_kg(double speed_rad_s, double manifoldPressure_Pa,
                                       double manifoldTemperature_K) const noexcept {
    const double ve = trueVe_.lookup(speed_rad_s, manifoldPressure_Pa);
    const double density = airDensity_kg_m3(manifoldPressure_Pa, manifoldTemperature_K);
    return ve * density * displacement_m3_;
}

double EngineModel::airMassFlow_kg_s(double speed_rad_s, double manifoldPressure_Pa,
                                     double manifoldTemperature_K) const noexcept {
    const double perCycle =
        airMassPerCycle_kg(speed_rad_s, manifoldPressure_Pa, manifoldTemperature_K);
    return perCycle * std::max(speed_rad_s, 0.0) / kRadiansPerCycle;
}

EngineOutputs EngineModel::evaluate(const OperatingPoint& point) const noexcept {
    const CombustionParameters& combustion = definition_.combustion;
    const double speed = point.speed_rad_s;
    const double cyclesPerSecond = std::max(speed, 0.0) / kRadiansPerCycle;

    const double ve = trueVe_.lookup(speed, point.manifoldPressure_Pa);
    const double airPerCycle =
        ve * airDensity_kg_m3(point.manifoldPressure_Pa, point.manifoldTemperature_K) *
        displacement_m3_;
    const double fuelPerCycle = point.fuelMassPerCycle_kg;

    // §2.4: actual lambda from the true air mass and the injected fuel.
    const double lambdaActual = fuelPerCycle > 0.0
                                    ? airPerCycle / (fuelPerCycle * constants::kAfrStoichGasoline)
                                    : std::numeric_limits<double>::infinity();
    // §2.6: with lambda < 1 oxygen is missing; at most air / AFR_st can burn.
    const double burnableFuel = std::min(fuelPerCycle, airPerCycle / constants::kAfrStoichGasoline);
    // §2.10a: without a spark the burnable mixture goes to the exhaust instead.
    const double unburntMixtureFuel = point.sparkCut ? burnableFuel : 0.0;
    const double fuelBurned = point.sparkCut ? 0.0 : burnableFuel;

    const double mbt_deg = definition_.tables.ignitionMbt.lookup(speed, point.manifoldPressure_Pa);
    const double etaIgnition = ignitionEfficiency(point.ignition_deg, mbt_deg, combustion);
    const double etaLambda = lambdaEfficiency(lambdaActual, combustion);
    const double etaIndicated = combustion.indicatedEfficiencyBase * etaIgnition * etaLambda;

    // §2.6: W_i = m_fuel,burned H_u eta_i ; T_i = W_i / (4 pi)
    const double fuelEnergyPerCycle = fuelBurned * constants::kLowerHeatingValueGasoline_J_kg;
    const double indicatedTorque = fuelEnergyPerCycle * etaIndicated / kRadiansPerCycle;

    // §2.8: friction from the fmep correlation, pumping from p_exh - p_m.
    const double airFlow = airPerCycle * cyclesPerSecond;
    const double fuelFlow = fuelPerCycle * cyclesPerSecond;
    const double exhaustFlow = airFlow + fuelFlow;
    const double exhaustPressure =
        constants::kAmbientPressure_Pa +
        definition_.exhaustBackpressureCoefficient * exhaustFlow * exhaustFlow;
    const double frictionTorque =
        meanEffectivePressureToTorque_Nm(definition_.friction.fmep_Pa(speed), displacement_m3_);
    const double pumpingTorque = meanEffectivePressureToTorque_Nm(
        exhaustPressure - point.manifoldPressure_Pa, displacement_m3_);
    // §2.9: T_e = T_i - T_f - T_p - T_aux
    const double accessoryTorque = definition_.accessoryTorque_Nm;
    const double brakeTorque = indicatedTorque - frictionTorque - pumpingTorque - accessoryTorque;

    // §2.10: energy not turned into work or lost to cooling leaves with the exhaust.
    const double exhaustHeatPerCycle =
        fuelEnergyPerCycle * std::max(0.0, 1.0 - etaIndicated - combustion.coolingFraction);
    const double exhaustMassPerCycle = airPerCycle + fuelPerCycle;
    const double exhaustTemperature =
        point.manifoldTemperature_K +
        (exhaustMassPerCycle > 0.0
             ? exhaustHeatPerCycle / (exhaustMassPerCycle * constants::kCpExhaust_J_kgK)
             : 0.0);

    // §2.10a: far behind MBT the combustion is not finished at EVO; that mixture can afterburn
    // in the exhaust and heats the catalyst on top of the exhaust temperature.
    const AfterburnParameters& afterburn = definition_.afterburn;
    double lateFraction = 1.0; // spark cut: all burnable mixture reaches the exhaust (§3.4)
    double afterburnHeatPerCycle = afterburn.afterburnEfficiency * unburntMixtureFuel *
                                   constants::kLowerHeatingValueGasoline_J_kg;
    if (!point.sparkCut) {
        const double retard_deg = mbt_deg - point.ignition_deg;
        lateFraction = std::min(std::max((retard_deg - afterburn.lateCombustionStart_deg) /
                                             afterburn.lateCombustionSpan_deg,
                                         0.0),
                                afterburn.lateCombustionMax);
        afterburnHeatPerCycle = afterburn.afterburnEfficiency * lateFraction * fuelBurned *
                                constants::kLowerHeatingValueGasoline_J_kg;
    }
    const double afterburnRise =
        exhaustMassPerCycle > 0.0
            ? afterburnHeatPerCycle / (exhaustMassPerCycle * constants::kCpExhaust_J_kgK)
            : 0.0;

    return {ve,
            airPerCycle,
            airFlow,
            fuelFlow,
            lambdaActual,
            mbt_deg,
            etaIgnition,
            etaLambda,
            indicatedTorque,
            frictionTorque,
            pumpingTorque,
            accessoryTorque,
            brakeTorque,
            exhaustPressure,
            exhaustTemperature,
            lateFraction,
            afterburnRise};
}

} // namespace simcore
