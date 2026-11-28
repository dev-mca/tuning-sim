// Launch control / 2-step while standing (research.md §3.4, iteration 7c); mirrors
// lab/tests/test_launch.py.

#include "domain/common/units.hpp"
#include "domain/ecu/ecu.hpp"
#include "domain/engine/engine_simulation.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace simcore;

namespace {

constexpr int kStepsPerSecond = 1000;
const LaunchSettings kTwoStep{true, units::rpmToRadS(4000.0), 10.0};

std::vector<SimulationState> fullThrottle(const LaunchSettings& settings, bool held, int steps) {
    const EngineDefinition& engine = test::referenceEngine().definition;
    Tune tune = Tune::fromTables(engine.tables);
    tune.launch = settings;
    EngineSimulation simulation(engine, tune);
    simulation.ecu().setLaunchSwitch(held);
    std::vector<SimulationState> states;
    for (int i = 0; i < steps; ++i) {
        states.push_back(simulation.step(1.0, 0.0));
    }
    return states;
}

double peakRpm(const std::vector<SimulationState>& states) {
    double peak = 0.0;
    for (const SimulationState& state : states) {
        peak = std::max(peak, units::radSToRpm(state.speed_rad_s));
    }
    return peak;
}

} // namespace

TEST_CASE("without launch control the engine revs to the normal limiter") {
    CHECK_FALSE(LaunchSettings{}.enabled);
    CHECK(peakRpm(fullThrottle(LaunchSettings{}, true, 3 * kStepsPerSecond)) > 6500.0);
    CHECK(peakRpm(fullThrottle(kTwoStep, false, 3 * kStepsPerSecond)) > 6500.0);
}

TEST_CASE("2-step holds the launch speed by cutting the spark and keeps injecting") {
    const std::vector<SimulationState> states = fullThrottle(kTwoStep, true, 3 * kStepsPerSecond);
    int cutSteps = 0;
    for (std::size_t i = kStepsPerSecond; i < states.size(); ++i) {
        const SimulationState& state = states[i];
        const double rpm = units::radSToRpm(state.speed_rad_s);
        CHECK(rpm > 3700.0);
        CHECK(rpm < 4150.0);
        CHECK(state.command.launchActive);
        if (state.command.sparkCut) {
            ++cutSteps;
            CHECK_FALSE(state.command.fuelCut);
            CHECK(state.command.fuelMassPerCycle_kg > 0.0);
            CHECK(state.engine.indicatedTorque_Nm == 0.0);
            CHECK(state.engine.lateCombustionFraction == 1.0);
        }
    }
    CHECK(cutSteps > 0);
    CHECK(cutSteps < 2 * kStepsPerSecond); // it bounces like a limiter
}

TEST_CASE("2-step retards the ignition and needs an open throttle") {
    const SimulationState plain = fullThrottle(LaunchSettings{}, true, 1).back();
    const SimulationState launch = fullThrottle(kTwoStep, true, 1).back();
    CHECK(launch.command.launchActive);
    CHECK(launch.command.ignition_deg == doctest::Approx(plain.command.ignition_deg - 10.0));

    const EngineDefinition& engine = test::referenceEngine().definition;
    Tune tune = Tune::fromTables(engine.tables);
    tune.launch = kTwoStep;
    EngineSimulation simulation(engine, tune);
    simulation.ecu().setLaunchSwitch(true);
    CHECK_FALSE(simulation.step(0.0, 0.0).command.launchActive);
}

TEST_CASE("launch settings are validated") {
    CHECK(validateLaunchSettings(kTwoStep).empty());
    CHECK_FALSE(validateLaunchSettings({true, units::rpmToRadS(1500.0), 10.0}).empty());
    CHECK_FALSE(validateLaunchSettings({true, units::rpmToRadS(7000.0), 10.0}).empty());
    CHECK_FALSE(validateLaunchSettings({true, units::rpmToRadS(4000.0), -1.0}).empty());
    CHECK_FALSE(validateLaunchSettings({true, units::rpmToRadS(4000.0), 31.0}).empty());
}
