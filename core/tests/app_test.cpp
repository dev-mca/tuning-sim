// Fixed-step accumulator and command line parsing.

#include "app/cli.hpp"
#include "app/fixed_step_loop.hpp"

#include <doctest/doctest.h>

using namespace simcore;

TEST_CASE("accumulator runs one step per elapsed time step, keeping the remainder") {
    FixedStepAccumulator accumulator(1.0e-3, 0.1);
    CHECK(accumulator.addElapsed(0.0005) == 0);
    CHECK(accumulator.addElapsed(0.0007) == 1); // 1.2 ms total
    CHECK(accumulator.addElapsed(0.0029) == 3); // 0.2 + 2.9 = 3.1 ms
    CHECK(accumulator.addElapsed(0.0) == 0);
}

TEST_CASE("accumulator caps the backlog after a stall") {
    FixedStepAccumulator accumulator(1.0e-3, 0.1);
    CHECK(accumulator.addElapsed(5.0) == 100); // not 5000
    CHECK(accumulator.addElapsed(-1.0) == 0);  // clock going backwards is ignored
}

TEST_CASE("no arguments serve the default engine on port 47800") {
    const CliParseResult result = parseCommandLine({});
    REQUIRE(result.options.has_value());
    CHECK(result.options->mode == CliMode::Serve);
    CHECK(result.options->engineId == "i6_30_na");
    CHECK(result.options->port == 47800);
}

TEST_CASE("sweep options are parsed") {
    const CliParseResult result =
        parseCommandLine({"--engine", "i6_30_na", "--sweep", "1000", "7000", "--rate", "500"});
    REQUIRE(result.options.has_value());
    CHECK(result.options->mode == CliMode::Sweep);
    CHECK(result.options->sweepFrom_rpm == 1000.0);
    CHECK(result.options->sweepTo_rpm == 7000.0);
    CHECK(result.options->sweepRate_rpm_s == 500.0);
}

TEST_CASE("invalid command lines are rejected with a message") {
    for (const auto& arguments : std::vector<std::vector<std::string>>{
             {"--sweep", "1000"}, {"--port", "abc"}, {"--port", "70000"}, {"--fly"}}) {
        const CliParseResult result = parseCommandLine(arguments);
        CHECK_FALSE(result.options.has_value());
        CHECK_FALSE(result.error.empty());
    }
}
