// C++ port vs. Python reference: shared/golden/i6_30_na_sweep.csv (docs/plan.md iteration 4).
// plan.md asks for +-1 %; the port reproduces the lab bit for bit, so the test uses a much
// tighter tolerance and would reveal any accidental change in the physics.

#include "adapters/sweep_csv.hpp"
#include "domain/common/units.hpp"
#include "domain/dyno/dyno.hpp"
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
