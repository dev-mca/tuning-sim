#include "app/engine_acoustics.hpp"

#include "domain/common/constants.hpp"
#include "domain/engine/engine_model.hpp"

#include <algorithm>
#include <cstddef>

namespace simcore {

acoustic::LayoutSpec exhaustLayout(const EngineDefinition& definition) {
    const ExhaustGeometry& exhaust = definition.exhaustGeometry;
    acoustic::LayoutSpec layout{definition.name, {}, {}, exhaust.mergeGroups, {}};

    std::vector<int> groupOfCylinder(exhaust.primaryLength_m.size(), 0);
    for (std::size_t group = 0; group < exhaust.collectorGroups.size(); ++group) {
        for (const int cylinder : exhaust.collectorGroups[group]) {
            groupOfCylinder[static_cast<std::size_t>(cylinder - 1)] = static_cast<int>(group);
        }
    }
    for (std::size_t cylinder = 0; cylinder < exhaust.primaryLength_m.size(); ++cylinder) {
        layout.cylinders.push_back({definition.firingAngles_deg[cylinder],
                                    {exhaust.primaryLength_m[cylinder], exhaust.primaryDiameter_m,
                                     acoustic::kHotSoundSpeed_m_s},
                                    groupOfCylinder[cylinder]});
    }
    const std::size_t groups = exhaust.collectorGroups.size();
    for (std::size_t group = 0; group < groups; ++group) {
        // Spread separate outlets from left to right; a single outlet sits in the middle.
        const float pan =
            groups < 2
                ? 0.0F
                : kOutletPanSpread *
                      (2.0F * static_cast<float>(group) / static_cast<float>(groups - 1) - 1.0F);
        layout.groups.push_back({{exhaust.collectorLength_m, exhaust.collectorDiameter_m,
                                  acoustic::kWarmSoundSpeed_m_s},
                                 exhaust.mergeGroups ? 0.0F : pan});
    }
    layout.mergedPipe = {exhaust.tailpipeLength_m, exhaust.tailpipeDiameter_m,
                         acoustic::kCoolSoundSpeed_m_s};
    return layout;
}

double catalystHotness(double catalystTemperature_K) noexcept {
    return std::clamp((catalystTemperature_K - kPopLightOff_K) / (kPopFullyHot_K - kPopLightOff_K),
                      0.0, 1.0);
}

acoustic::AcousticParams acousticParams(const SimulationState& state,
                                        const EngineDefinition& definition) noexcept {
    // Reference: a cylinder set completely filled at ambient conditions (VE = 1, §2.2).
    const double fullCharge_kg =
        airDensity_kg_m3(constants::kAmbientPressure_Pa, constants::kAmbientTemperature_K) *
        definition.displacement_m3();
    acoustic::AcousticParams params;
    params.speed_rad_s = state.speed_rad_s;
    params.chargeRatio =
        fullCharge_kg > 0.0 ? state.engine.airMassPerCycle_kg / fullCharge_kg : 0.0;
    params.ignitionRetard_deg =
        std::max(0.0, state.engine.ignitionMbt_deg - state.command.ignition_deg);
    params.lambda = state.engine.lambdaActual;
    params.fuelCut = state.command.fuelCut;
    // Pops need fuel in overrun, late combustion and a hot exhaust (research.md §2.10a, §6.7).
    params.afterburnRate = state.command.overrunFueling
                               ? kAfterburnEventShare * state.engine.lateCombustionFraction *
                                     catalystHotness(state.catalystTemperature_K)
                               : 0.0;
    return params;
}

} // namespace simcore
