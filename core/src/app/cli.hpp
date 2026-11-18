#pragma once

// Command line of simcore (docs/plan.md iteration 4):
//   simcore                                  serve the WebSocket API (default)
//   simcore --engine i6_30_na --sweep 1000 7000 [--rate 300]   dyno sweep as CSV on stdout
// Options: --engine <id>, --shared <dir>, --port <n>, --version, --help.

#include <optional>
#include <string>
#include <vector>

namespace simcore {

enum class CliMode { Serve, Sweep, Version, Help };

struct CliOptions {
    CliMode mode = CliMode::Serve;
    std::string engineId = "i6_30_na";
    std::string sharedDir; // empty = built-in default (source tree)
    int port = 47800;
    double sweepFrom_rpm = 0.0;
    double sweepTo_rpm = 0.0;
    std::optional<double> sweepRate_rpm_s;
};

struct CliParseResult {
    std::optional<CliOptions> options;
    std::string error;
};

[[nodiscard]] CliParseResult parseCommandLine(const std::vector<std::string>& arguments);
[[nodiscard]] std::string usageText();

} // namespace simcore
