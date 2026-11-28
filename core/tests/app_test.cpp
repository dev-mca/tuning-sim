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

TEST_CASE("sound sweep and audio options are parsed") {
    const CliParseResult result =
        parseCommandLine({"--sound-sweep", "out.wav", "--ignition-offset", "-10", "--no-audio"});
    REQUIRE(result.options.has_value());
    CHECK(result.options->mode == CliMode::SoundSweep);
    CHECK(result.options->soundSweepFile == "out.wav");
    CHECK(result.options->ignitionOffset_deg == -10.0);
    CHECK(result.options->sweepFrom_rpm == kSoundSweepFrom_rpm);
    CHECK(result.options->sweepTo_rpm == kSoundSweepTo_rpm);
    CHECK_FALSE(result.options->audio);
    CHECK(parseCommandLine({}).options->audio);
}

TEST_CASE("sound blips with overrun settings are parsed") {
    const CliParseResult result =
        parseCommandLine({"--sound-blips", "pops.wav", "--no-overrun-cut", "--overrun-retard", "35",
                          "--overrun-enrichment", "0.1"});
    REQUIRE(result.options.has_value());
    CHECK(result.options->mode == CliMode::SoundBlips);
    CHECK(result.options->soundBlipsFile == "pops.wav");
    CHECK_FALSE(result.options->overrunFuelCut);
    CHECK(result.options->overrunRetard_deg == 35.0);
    CHECK(result.options->overrunEnrichment == 0.1);
}

TEST_CASE("--sound-launch renders a 2-step with optional launch speed and retard") {
    const CliParseResult defaults = parseCommandLine({"--sound-launch", "launch.wav"});
    REQUIRE(defaults.options.has_value());
    CHECK(defaults.options->mode == CliMode::SoundLaunch);
    CHECK(defaults.options->soundLaunchFile == "launch.wav");
    CHECK(defaults.options->launch_rpm == kDefaultLaunch_rpm);
    CHECK(defaults.options->launchRetard_deg == kDefaultSoundLaunchRetard_deg);

    const CliParseResult custom = parseCommandLine(
        {"--sound-launch", "launch.wav", "--launch-rpm", "4500", "--launch-retard", "15"});
    REQUIRE(custom.options.has_value());
    CHECK(custom.options->launch_rpm == 4500.0);
    CHECK(custom.options->launchRetard_deg == 15.0);
}

TEST_CASE("invalid command lines are rejected with a message") {
    for (const auto& arguments :
         std::vector<std::vector<std::string>>{{"--sweep", "1000"},
                                               {"--port", "abc"},
                                               {"--port", "70000"},
                                               {"--fly"},
                                               {"--sound-sweep"},
                                               {"--ignition-offset", "x"}}) {
        const CliParseResult result = parseCommandLine(arguments);
        CHECK_FALSE(result.options.has_value());
        CHECK_FALSE(result.error.empty());
    }
}
