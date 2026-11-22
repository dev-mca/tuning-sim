// From engine definition and simulation state to the sound model's inputs.

#include "app/engine_acoustics.hpp"
#include "domain/engine/engine_simulation.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <set>

using namespace simcore;

namespace {

SimulationState settledState(double pedal, int steps = 3000) {
    EngineSimulation simulation(test::referenceEngine().definition);
    SimulationState state{};
    for (int i = 0; i < steps; ++i) {
        state = simulation.step(pedal, 0.0);
    }
    return state;
}

} // namespace

TEST_CASE("the reference engine's exhaust becomes a 3-into-1 x2 layout with a Y-pipe") {
    const acoustic::LayoutSpec layout = exhaustLayout(test::referenceEngine().definition);
    REQUIRE(layout.cylinders.size() == 6);
    CHECK(layout.groups.size() == 2);
    CHECK(layout.mergeGroups);
    CHECK(layout.mergedPipe.length_m == doctest::Approx(1.8));
    // Cylinders 1-3 into collector 0, 4-6 into collector 1 (1-based in the JSON).
    CHECK(layout.cylinders[0].group == 0);
    CHECK(layout.cylinders[2].group == 0);
    CHECK(layout.cylinders[3].group == 1);
    CHECK(layout.cylinders[0].primary.length_m == doctest::Approx(0.60));
    // Firing angles come from the engine definition: six different, 120 deg apart.
    std::set<double> angles;
    for (const acoustic::CylinderSpec& cylinder : layout.cylinders) {
        angles.insert(cylinder.firingAngle_deg);
    }
    CHECK(angles.size() == 6);
}

TEST_CASE("charge ratio is low at idle and close to 1 at wide-open throttle") {
    const EngineDefinition& definition = test::referenceEngine().definition;
    const acoustic::AcousticParams idle = acousticParams(settledState(0.0), definition);
    const acoustic::AcousticParams full = acousticParams(settledState(1.0, 300), definition);
    CHECK(idle.chargeRatio > 0.15);
    CHECK(idle.chargeRatio < 0.45);
    CHECK(full.chargeRatio > 0.8);
    CHECK(full.chargeRatio < 1.1);
    CHECK(idle.speed_rad_s > 0.0);
}

TEST_CASE("retard is measured against MBT and never negative") {
    SimulationState state = settledState(0.5, 500);
    state.engine.ignitionMbt_deg = 30.0;
    state.command.ignition_deg = 22.0;
    CHECK(acousticParams(state, test::referenceEngine().definition).ignitionRetard_deg ==
          doctest::Approx(8.0));
    state.command.ignition_deg = 34.0; // earlier than MBT
    CHECK(acousticParams(state, test::referenceEngine().definition).ignitionRetard_deg == 0.0);
}

TEST_CASE("fuel cut and lambda are passed on") {
    SimulationState state = settledState(0.5, 500);
    state.command.fuelCut = true;
    state.engine.lambdaActual = INFINITY;
    const acoustic::AcousticParams params =
        acousticParams(state, test::referenceEngine().definition);
    CHECK(params.fuelCut);
    CHECK(std::isinf(params.lambda));
}
