// Engine physics: units, magnitudes and trends (docs/research.md §2, §10).

#include "domain/common/constants.hpp"
#include "domain/common/units.hpp"
#include "domain/dyno/dyno.hpp"
#include "domain/engine/engine_model.hpp"
#include "domain/engine/engine_simulation.hpp"
#include "domain/engine/intake.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace simcore;

namespace {

const EngineDefinition& engine() {
    return test::referenceEngine().definition;
}

OperatingPoint fullLoad(double lambda, double ignitionOffset_deg) {
    const double speed = units::rpmToRadS(4000.0);
    constexpr double kPressure_Pa = 100'000.0;
    const EngineModel model(engine());
    const double air =
        model.airMassPerCycle_kg(speed, kPressure_Pa, constants::kAmbientTemperature_K);
    const double mbt = engine().tables.ignitionMbt.lookup(speed, kPressure_Pa);
    return {speed, kPressure_Pa, constants::kAmbientTemperature_K,
            air / (lambda * constants::kAfrStoichGasoline), mbt + ignitionOffset_deg};
}

} // namespace

TEST_CASE("displacement follows from bore and stroke") {
    CHECK(engine().displacement_m3() == doctest::Approx(2.9926e-3).epsilon(1.0e-4));
}

TEST_CASE("critical pressure ratio of air and continuous flow function") {
    CHECK(intake::criticalPressureRatio() == doctest::Approx(0.528).epsilon(0.002));
    CHECK(intake::flowFunction(1.0) == 0.0);
    const double left = intake::flowFunction(intake::kPressureRatioLinear - 1.0e-9);
    const double right = intake::flowFunction(intake::kPressureRatioLinear + 1.0e-9);
    CHECK(left == doctest::Approx(right));
    CHECK(intake::flowFunction(0.1) == doctest::Approx(intake::flowFunction(0.5)));
}

TEST_CASE("full-load operating point is plausible (research.md §10)") {
    const EngineOutputs out = EngineModel(engine()).evaluate(fullLoad(0.88, 0.0));
    const double bmep_bar =
        units::paToBar(torqueToBmep_Pa(out.brakeTorque_Nm, engine().displacement_m3()));
    CHECK(bmep_bar > 11.0);
    CHECK(bmep_bar < 13.5);
    CHECK(units::kelvinToCelsius(out.exhaustTemperature_K) > 700.0);
    CHECK(units::kelvinToCelsius(out.exhaustTemperature_K) < 950.0);
    CHECK(out.lambdaActual == doctest::Approx(0.88));
}

TEST_CASE("late ignition costs torque and heats the exhaust") {
    const EngineModel model(engine());
    const EngineOutputs atMbt = model.evaluate(fullLoad(0.88, 0.0));
    const EngineOutputs late = model.evaluate(fullLoad(0.88, -10.0));
    CHECK(late.brakeTorque_Nm < atMbt.brakeTorque_Nm);
    CHECK(late.exhaustTemperature_K > atMbt.exhaustTemperature_K);
}

TEST_CASE("fuel cut gives infinite lambda and engine braking") {
    const OperatingPoint point{units::rpmToRadS(4000.0), 30'000.0, constants::kAmbientTemperature_K,
                               0.0, 30.0};
    const EngineOutputs out = EngineModel(engine()).evaluate(point);
    CHECK(std::isinf(out.lambdaActual));
    CHECK(out.brakeTorque_Nm < 0.0);
}

TEST_CASE("closed throttle without load idles near 765 1/min at about 30 kPa") {
    EngineSimulation simulation(engine(), std::nullopt, nullptr, units::rpmToRadS(1200.0));
    SimulationState state = simulation.step(0.0, 0.0);
    for (int i = 0; i < 8000; ++i) {
        state = simulation.step(0.0, 0.0);
    }
    CHECK(units::radSToRpm(state.speed_rad_s) > 600.0);
    CHECK(units::radSToRpm(state.speed_rad_s) < 1000.0);
    CHECK(state.manifoldPressure_Pa > 27'000.0);
    CHECK(state.manifoldPressure_Pa < 45'000.0);
}

TEST_CASE("rev limiter keeps a free-revving engine below 7100 1/min") {
    EngineSimulation simulation(engine(), std::nullopt, nullptr, units::rpmToRadS(3000.0));
    double maxSpeed_rpm = 0.0;
    for (int i = 0; i < 5000; ++i) {
        maxSpeed_rpm =
            std::max(maxSpeed_rpm, units::radSToRpm(simulation.step(1.0, 0.0).speed_rad_s));
    }
    CHECK(maxSpeed_rpm > 6900.0);
    CHECK(maxSpeed_rpm < 7100.0);
}

TEST_CASE("lifting off at high speed cuts fuel until shortly above idle (research.md §3.3)") {
    EngineSimulation simulation(engine(), std::nullopt, nullptr, units::rpmToRadS(4000.0));
    double lowestCutSpeed_rpm = 1.0e9;
    SimulationState state{};
    for (int i = 0; i < 6000; ++i) {
        state = simulation.step(0.0, 0.0);
        if (state.command.fuelCut) {
            lowestCutSpeed_rpm = std::min(lowestCutSpeed_rpm, units::radSToRpm(state.speed_rad_s));
        }
    }
    CHECK(lowestCutSpeed_rpm > 1050.0); // fuel returns at exit_rpm = 1100
    CHECK(lowestCutSpeed_rpm < 1200.0);
    CHECK_FALSE(state.command.fuelCut);
    CHECK(units::radSToRpm(state.speed_rad_s) > 600.0); // catches itself, no stall
    CHECK(units::radSToRpm(state.speed_rad_s) < 1000.0);
}

TEST_CASE("opening the throttle ends the overrun cut; below enter_rpm there is none") {
    EngineSimulation coasting(engine(), std::nullopt, nullptr, units::rpmToRadS(4000.0));
    CHECK(coasting.step(0.0, 0.0).command.fuelCut);
    CHECK_FALSE(coasting.step(0.3, 0.0).command.fuelCut);
    EngineSimulation slow(engine(), std::nullopt, nullptr, units::rpmToRadS(1300.0));
    CHECK_FALSE(slow.step(0.0, 0.0).command.fuelCut);
}

namespace {

std::vector<SimulationState> coasting(const OverrunSettings& settings, int steps) {
    Tune tune = Tune::fromTables(engine().tables);
    tune.overrun = settings;
    EngineSimulation simulation(engine(), tune, nullptr, units::rpmToRadS(5000.0));
    std::vector<SimulationState> states;
    for (int i = 0; i < steps; ++i) {
        states.push_back(simulation.step(0.0, 0.0));
    }
    return states;
}

const OverrunSettings kPops{false, 35.0, 0.1};

} // namespace

TEST_CASE("without fuel cut the ECU keeps fuelling in overrun; a pops tune burns late") {
    const SimulationState stock = coasting({}, 1).back();
    CHECK(stock.command.fuelCut);
    CHECK_FALSE(stock.command.overrunFueling);

    const SimulationState plain = coasting({false, 0.0, 0.0}, 1).back();
    CHECK_FALSE(plain.command.fuelCut);
    CHECK(plain.command.overrunFueling);
    CHECK(plain.engine.lateCombustionFraction == 0.0);

    const SimulationState pops = coasting(kPops, 1).back();
    CHECK(pops.command.ignition_deg == doctest::Approx(plain.command.ignition_deg - 35.0));
    CHECK(pops.command.fuelMassPerCycle_kg ==
          doctest::Approx(plain.command.fuelMassPerCycle_kg * 1.1).epsilon(1e-3));
    CHECK(pops.engine.lateCombustionFraction > 0.3);
    CHECK(pops.engine.afterburnTemperatureRise_K > 100.0);
}

TEST_CASE("the catalyst cools in fuel cut and heats up with a pops tune") {
    const double start = engine().afterburn.catalystInitial_K;
    CHECK(coasting({}, 1500).back().catalystTemperature_K < start);
    CHECK(coasting(kPops, 1500).back().catalystTemperature_K > start + 50.0);
}

TEST_CASE("a wrong ECU VE table shows up as wrong lambda (research.md §2.4)") {
    Tune lean = Tune::fromTables(engine().tables);
    lean.ve = lean.ve.scaled(0.9);
    const auto samples = runSweep(engine(), {3000.0, 4000.0, 300.0, 1.0, 500.0, 3.0}, lean);
    REQUIRE_FALSE(samples.empty());
    for (const SweepSample& sample : samples) {
        CHECK(sample.lambdaActual == doctest::Approx(0.88 / 0.9).epsilon(0.01));
    }
}

TEST_CASE("sweep parameters are validated") {
    CHECK_THROWS_AS(SweepRun({7000.0, 1000.0}, 0.2, 1.0e-3), std::invalid_argument);
    CHECK_THROWS_AS(SweepRun({1000.0, 7000.0, 0.0}, 0.2, 1.0e-3), std::invalid_argument);
}
