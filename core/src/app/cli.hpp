#pragma once

// Command line of simcore (docs/plan.md iteration 4):
//   simcore                                  serve the WebSocket API (default)
//   simcore --engine i6_30_na --sweep 1000 7000 [--rate 300]   dyno sweep as CSV on stdout
//   simcore --sound-sweep out.wav [--ignition-offset -10]       engine sound of a sweep as WAV
//   simcore --sound-blips out.wav [--no-overrun-cut] [--overrun-retard 35]
//                                 [--overrun-enrichment 0.1]     throttle blips with overrun
// Options: --engine <id>, --shared <dir>, --port <n>, --no-audio, --version, --help.

#include <optional>
#include <string>
#include <vector>

namespace simcore {

enum class CliMode { Serve, Sweep, SoundSweep, SoundBlips, Version, Help };

/// Range of --sound-sweep: the full speed range of the reference engine.
inline constexpr double kSoundSweepFrom_rpm = 1000.0;
inline constexpr double kSoundSweepTo_rpm = 7000.0;

struct CliOptions {
    CliMode mode = CliMode::Serve;
    std::string engineId = "i6_30_na";
    std::string sharedDir; // empty = built-in default (source tree)
    int port = 47800;
    double sweepFrom_rpm = 0.0;
    double sweepTo_rpm = 0.0;
    std::optional<double> sweepRate_rpm_s;
    bool audio = true;          // false with --no-audio (CI, tests, machines without sound)
    std::string soundSweepFile; // target of --sound-sweep
    double ignitionOffset_deg = 0.0;
    std::string soundBlipsFile; // target of --sound-blips
    bool overrunFuelCut = true;
    double overrunRetard_deg = 0.0;
    double overrunEnrichment = 0.0;
};

struct CliParseResult {
    std::optional<CliOptions> options;
    std::string error;
};

[[nodiscard]] CliParseResult parseCommandLine(const std::vector<std::string>& arguments);
[[nodiscard]] std::string usageText();

} // namespace simcore
