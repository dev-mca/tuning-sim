// C++ port vs. Python reference: shared/golden/i6_30_na_sweep.csv (docs/plan.md iteration 4).
// plan.md asks for +-1 %; the port reproduces the lab bit for bit, so the test uses a much
// tighter tolerance and would reveal any accidental change in the physics.

#include "adapters/sweep_csv.hpp"
#include "domain/common/units.hpp"
#include "domain/drivetrain/car_simulation.hpp"
#include "domain/dyno/dyno.hpp"
#include "domain/dyno/roller.hpp"
#include "domain/ecu/ecu.hpp"
#include "domain/engine/engine_simulation.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace simcore;

namespace {

constexpr double kRelativeTolerance = 1.0e-6;
constexpr double kAbsoluteTolerance = 2.0e-6; // the CSV has 6 decimals

struct Csv {
    std::vector<std::string> header;
    std::vector<std::vector<double>> rows;
};

std::vector<std::string> splitLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

Csv readCsv(const std::string& path) {
    std::ifstream file(path);
    REQUIRE(file.good());
    Csv csv;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (csv.header.empty()) {
            csv.header = splitLine(line);
            continue;
        }
        std::vector<double> row;
        for (const std::string& field : splitLine(line)) {
            row.push_back(std::stod(field));
        }
        csv.rows.push_back(row);
    }
    return csv;
}

void checkRow(const Csv& golden, std::size_t row, const std::vector<std::string>& columns,
              const std::vector<double>& actual) {
    REQUIRE(row < golden.rows.size());
    for (std::size_t c = 0; c < actual.size(); ++c) {
        const double expected = golden.rows[row][c];
        const double tolerance = kAbsoluteTolerance + kRelativeTolerance * std::abs(expected);
        CAPTURE(row);
        CAPTURE(columns[c]);
        CHECK(std::abs(actual[c] - expected) <= tolerance);
    }
}

} // namespace

TEST_CASE("full-load sweep matches the Python golden file") {
    const Csv golden = readCsv(test::sharedDir() + "/golden/i6_30_na_sweep.csv");
    REQUIRE(golden.header.size() == kSweepCsvColumns.size());
    for (std::size_t i = 0; i < kSweepCsvColumns.size(); ++i) {
        CHECK(golden.header[i] == kSweepCsvColumns[i]);
    }

    const EngineDefinition& engine = test::referenceEngine().definition;
    // Same parameters as I6_30_NA_SWEEP in lab/src/tuning_lab/golden.py.
    const std::vector<SweepSample> samples = runSweep(engine, {1000.0, 7000.0, 300.0, 1.0, 100.0});
    REQUIRE(samples.size() == golden.rows.size());

    for (std::size_t r = 0; r < samples.size(); ++r) {
        const auto row = sweepCsvRow(samples[r], engine.displacement_m3());
        for (std::size_t c = 0; c < row.size(); ++c) {
            const double expected = golden.rows[r][c];
            const double tolerance = kAbsoluteTolerance + kRelativeTolerance * std::abs(expected);
            CAPTURE(r);
            CAPTURE(kSweepCsvColumns[c]);
            CHECK(std::abs(row[c] - expected) <= tolerance);
        }
    }
}

TEST_CASE("overrun with a pops tune matches the Python golden file (research.md §2.10a)") {
    const Csv golden = readCsv(test::sharedDir() + "/golden/i6_30_na_overrun.csv");
    const std::vector<std::string> columns = {
        "time_s",   "speed_rpm",      "map_kPa",          "throttle",
        "fuelCut",  "overrunFueling", "ignition_degBTDC", "lateCombustion",
        "egt_degC", "catalyst_degC",  "engineTorque_Nm"};
    REQUIRE(golden.header == columns);

    // Same scenario as OVERRUN_* in lab/src/tuning_lab/golden.py.
    const EngineDefinition& engine = test::referenceEngine().definition;
    Tune tune = Tune::fromTables(engine.tables);
    tune.overrun = {false, 35.0, 0.1};
    EngineSimulation simulation(engine, tune);
    constexpr int kSteps = 6000;
    constexpr int kSampleSteps = 50;
    std::size_t row = 0;
    for (int step = 0; step < kSteps; ++step) {
        const double time_s = step * simulation.timeStep_s();
        const double throttle = (time_s >= 0.5 && time_s < 1.3) ? 1.0 : 0.0;
        const SimulationState state = simulation.step(throttle, 0.0);
        if ((step + 1) % kSampleSteps != 0) {
            continue;
        }
        REQUIRE(row < golden.rows.size());
        const std::vector<double> actual = {
            state.time_s,
            units::radSToRpm(state.speed_rad_s),
            units::paToKpa(state.manifoldPressure_Pa),
            state.throttle,
            state.command.fuelCut ? 1.0 : 0.0,
            state.command.overrunFueling ? 1.0 : 0.0,
            state.command.ignition_deg,
            state.engine.lateCombustionFraction,
            units::kelvinToCelsius(state.engine.exhaustTemperature_K),
            units::kelvinToCelsius(state.catalystTemperature_K),
            state.engine.brakeTorque_Nm};
        for (std::size_t c = 0; c < actual.size(); ++c) {
            const double expected = golden.rows[row][c];
            const double tolerance = kAbsoluteTolerance + kRelativeTolerance * std::abs(expected);
            CAPTURE(row);
            CAPTURE(columns[c]);
            CHECK(std::abs(actual[c] - expected) <= tolerance);
        }
        ++row;
    }
    CHECK(row == golden.rows.size());
}

TEST_CASE("launch control (2-step) matches the Python golden file (research.md §3.4)") {
    const Csv golden = readCsv(test::sharedDir() + "/golden/i6_30_na_launch.csv");
    const std::vector<std::string> columns = {
        "time_s",  "speed_rpm",        "throttle",       "launchActive",  "sparkCut",
        "fuelCut", "ignition_degBTDC", "lateCombustion", "catalyst_degC", "engineTorque_Nm"};
    REQUIRE(golden.header == columns);

    // Same scenario as LAUNCH_* in lab/src/tuning_lab/golden.py.
    const EngineDefinition& engine = test::referenceEngine().definition;
    Tune tune = Tune::fromTables(engine.tables);
    tune.launch = {true, units::rpmToRadS(4000.0), 10.0};
    EngineSimulation simulation(engine, tune);
    constexpr int kSteps = 5000;
    constexpr int kSampleSteps = 50;
    std::size_t row = 0;
    for (int step = 0; step < kSteps; ++step) {
        const double time_s = step * simulation.timeStep_s();
        const bool held = time_s >= 0.5 && time_s < 4.0;
        const bool throttleOpen = held || (time_s >= 4.0 && time_s < 4.3);
        simulation.ecu().setLaunchSwitch(held);
        const SimulationState state = simulation.step(throttleOpen ? 1.0 : 0.0, 0.0);
        if ((step + 1) % kSampleSteps != 0) {
            continue;
        }
        REQUIRE(row < golden.rows.size());
        const std::vector<double> actual = {state.time_s,
                                            units::radSToRpm(state.speed_rad_s),
                                            state.throttle,
                                            state.command.launchActive ? 1.0 : 0.0,
                                            state.command.sparkCut ? 1.0 : 0.0,
                                            state.command.fuelCut ? 1.0 : 0.0,
                                            state.command.ignition_deg,
                                            state.engine.lateCombustionFraction,
                                            units::kelvinToCelsius(state.catalystTemperature_K),
                                            state.engine.brakeTorque_Nm};
        for (std::size_t c = 0; c < actual.size(); ++c) {
            const double expected = golden.rows[row][c];
            const double tolerance = kAbsoluteTolerance + kRelativeTolerance * std::abs(expected);
            CAPTURE(row);
            CAPTURE(columns[c]);
            CHECK(std::abs(actual[c] - expected) <= tolerance);
        }
        ++row;
    }
    CHECK(row == golden.rows.size());
}

TEST_CASE("roller dyno run in 4th gear matches the Python golden file (research.md §8.4)") {
    const Csv golden = readCsv(test::sharedDir() + "/golden/i6_30_na_roller.csv");
    const std::vector<std::string> columns = {
        "time_s",         "speed_rpm",     "vehicleSpeed_kmh", "wheelForce_N",
        "wheelTorque_Nm", "wheelPower_kW", "engineTorque_Nm"};
    REQUIRE(golden.header == columns);
    // Same run as ROLLER_* in lab/src/tuning_lab/golden.py.
    const std::vector<RollerSample> samples =
        runRoller(test::referenceEngine().definition, {4, 1500.0, 6800.0});
    REQUIRE(samples.size() == golden.rows.size());
    for (std::size_t row = 0; row < samples.size(); ++row) {
        const RollerSample& s = samples[row];
        checkRow(golden, row, columns,
                 {s.time_s, units::radSToRpm(s.speed_rad_s), units::msToKmh(s.vehicleSpeed_m_s),
                  s.wheelForce_N, s.wheelTorque_Nm, units::wToKw(s.wheelPower_W()),
                  s.engineTorque_Nm});
    }
}

TEST_CASE("launch and flat-shifted drive match the Python golden file (research.md §8)") {
    const Csv golden = readCsv(test::sharedDir() + "/golden/i6_30_na_drive.csv");
    const std::vector<std::string> columns = {
        "time_s",           "speed_rpm",       "vehicleSpeed_kmh", "gear",
        "clutchEngagement", "clutchLocked",    "launchActive",     "sparkCut",
        "flatShift",        "clutchTorque_Nm", "catalyst_degC"};
    REQUIRE(golden.header == columns);
    // Same scenario as DRIVE_* in lab/src/tuning_lab/golden.py.
    const EngineDefinition& engine = test::referenceEngine().definition;
    Tune tune = Tune::fromTables(engine.tables);
    tune.launch = {true, units::rpmToRadS(4000.0), 10.0, true};
    CarSimulation car(engine, tune);
    car.drivetrain().setClutchPedal(true);
    static_cast<void>(car.drivetrain().shift(1));
    constexpr int kSteps = 10000;
    constexpr int kClutchUpStep = 2500;
    constexpr int kThrottleStep = 500;
    constexpr int kSampleSteps = 50;
    constexpr double kShift_rpm = 6500.0;
    constexpr int kTopGear = 3;
    std::size_t row = 0;
    for (int step = 0; step < kSteps; ++step) {
        if (step == kClutchUpStep) {
            car.drivetrain().setClutchPedal(false);
        }
        const CarState state = car.step(step >= kThrottleStep ? 1.0 : 0.0);
        const DrivetrainState& d = state.drivetrain;
        if (units::radSToRpm(state.engine.speed_rad_s) >= kShift_rpm &&
            d.clutchState == ClutchState::Locked && d.gear >= 1 && d.gear < kTopGear) {
            static_cast<void>(car.drivetrain().shift(d.gear + 1));
        }
        if ((step + 1) % kSampleSteps != 0) {
            continue;
        }
        const EcuCommand& c = state.engine.command;
        checkRow(golden, row, columns,
                 {state.engine.time_s, units::radSToRpm(state.engine.speed_rad_s),
                  units::msToKmh(d.vehicleSpeed_m_s), static_cast<double>(d.gear),
                  d.clutchEngagement, d.clutchState == ClutchState::Locked ? 1.0 : 0.0,
                  c.launchActive ? 1.0 : 0.0, c.sparkCut ? 1.0 : 0.0, c.flatShiftActive ? 1.0 : 0.0,
                  d.clutchTorque_Nm, units::kelvinToCelsius(state.engine.catalystTemperature_K)});
        ++row;
    }
    CHECK(row == golden.rows.size());
}
