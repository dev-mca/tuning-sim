#include "domain/engine/intake.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simcore::intake {

namespace {

constexpr double kRadiansPerDegree = std::numbers::pi / 180.0;

double psiSubsonic(double pressureRatio, double gamma) noexcept {
    return std::sqrt(
        2.0 * gamma / (gamma - 1.0) *
        (std::pow(pressureRatio, 2.0 / gamma) - std::pow(pressureRatio, (gamma + 1.0) / gamma)));
}

} // namespace

double criticalPressureRatio(double gamma) noexcept {
    return std::pow(2.0 / (gamma + 1.0), gamma / (gamma - 1.0));
}

double flowFunction(double pressureRatio, double gamma) noexcept {
    if (pressureRatio >= 1.0) {
        return 0.0;
    }
    if (pressureRatio > kPressureRatioLinear) {
        const double psiAtLinear = psiSubsonic(kPressureRatioLinear, gamma);
        return psiAtLinear * (1.0 - pressureRatio) / (1.0 - kPressureRatioLinear);
    }
    return psiSubsonic(std::max(pressureRatio, criticalPressureRatio(gamma)), gamma);
}

double throttleArea_m2(double throttle, const IntakeParameters& intake) noexcept {
    const double position = std::min(std::max(throttle, 0.0), 1.0);
    const double closed_deg = intake.throttleClosedAngle_deg;
    const double angle =
        (closed_deg + position * (kFullOpenAngle_deg - closed_deg)) * kRadiansPerDegree;
    const double boreArea =
        std::numbers::pi / 4.0 * (intake.throttleDiameter_m * intake.throttleDiameter_m);
    const double plateArea =
        boreArea * (1.0 - std::cos(angle) / std::cos(closed_deg * kRadiansPerDegree));
    return plateArea + intake.throttleLeakArea_m2;
}

double orificeMassFlow_kg_s(double area_m2, double dischargeCoefficient, double upstreamPressure_Pa,
                            double upstreamTemperature_K, double downstreamPressure_Pa) noexcept {
    if (downstreamPressure_Pa > upstreamPressure_Pa) {
        return -orificeMassFlow_kg_s(area_m2, dischargeCoefficient, downstreamPressure_Pa,
                                     upstreamTemperature_K, upstreamPressure_Pa);
    }
    const double densityTerm =
        upstreamPressure_Pa / std::sqrt(constants::kGasConstantAir_J_kgK * upstreamTemperature_K);
    const double psi = flowFunction(downstreamPressure_Pa / upstreamPressure_Pa);
    return dischargeCoefficient * area_m2 * densityTerm * psi;
}

double manifoldPressureDerivative_Pa_s(double throttleFlow_kg_s, double cylinderFlow_kg_s,
                                       double manifoldTemperature_K,
                                       double manifoldVolume_m3) noexcept {
    return constants::kGasConstantAir_J_kgK * manifoldTemperature_K / manifoldVolume_m3 *
           (throttleFlow_kg_s - cylinderFlow_kg_s);
}

} // namespace simcore::intake
