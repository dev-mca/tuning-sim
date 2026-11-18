// C++ port vs. Python reference: shared/golden/i6_30_na_sweep.csv (docs/plan.md iteration 4).
// plan.md asks for +-1 %; the port reproduces the lab bit for bit, so the test uses a much
// tighter tolerance and would reveal any accidental change in the physics.

#include "adapters/sweep_csv.hpp"
#include "domain/dyno/dyno.hpp"
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
