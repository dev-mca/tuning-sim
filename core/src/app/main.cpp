// simcore: headless simulation process (docs/adr/0001-architecture.md).
// Wiring only: parse the command line, create adapters, connect them to the application core.

#include "adapters/json_file_repository.hpp"
#include "adapters/message_bus.hpp"
#include "adapters/protocol.hpp"
#include "adapters/steady_clock.hpp"
#include "adapters/sweep_csv.hpp"
#include "adapters/websocket_server.hpp"
#include "app/cli.hpp"
#include "app/realtime_runner.hpp"
#include "app/simulation_service.hpp"
#include "domain/dyno/dyno.hpp"
#include "simcore/version.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace simcore;

std::atomic<bool> gStopRequested{false};

extern "C" void onSignal(int /*signal*/) {
    gStopRequested = true;
}

std::string sharedDirectory(const CliOptions& options) {
    return options.sharedDir.empty() ? std::string(SIMCORE_DEFAULT_SHARED_DIR) : options.sharedDir;
}

int runSweepToStdout(const CliOptions& options, const LoadedEngine& engine) {
    SweepParameters parameters{options.sweepFrom_rpm, options.sweepTo_rpm};
    parameters.rate_rpm_s = options.sweepRate_rpm_s.value_or(kDefaultSweepRate_rpm_s);
    const std::vector<SweepSample> samples = runSweep(engine.definition, parameters);
    writeSweepCsv(std::cout, samples, engine.definition.displacement_m3(),
                  "engine: " + engine.definition.id + ", full-load sweep, simcore " +
                      std::string(version()));
    return 0;
}

int serve(const CliOptions& options, const LoadedEngine& engine) {
    constexpr auto kMainLoopInterval = std::chrono::milliseconds(100);
    auto bus = std::make_unique<MessageBus>(); // large queues: keep them off the stack
    const ProtocolCodec codec(engine);
    SimulationService service(engine, *bus, *bus);
    SteadyClock clock;
    RealtimeRunner runner(service, clock);
    WebSocketServer server(*bus, codec, options.port);

    if (const std::string error = server.start(); !error.empty()) {
        std::cerr << "error: cannot listen on " << WebSocketServer::kHost << ':' << options.port
                  << ": " << error << '\n';
        return 1;
    }
    runner.start();
    std::cout << versionBanner() << " - engine " << engine.definition.id << '\n'
              << "listening on ws://" << WebSocketServer::kHost << ':' << options.port
              << " (Ctrl+C to stop)" << std::endl;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    while (!gStopRequested) {
        std::this_thread::sleep_for(kMainLoopInterval);
    }
    std::cout << "\nstopping" << std::endl;
    server.stop(); // stop the network first: no new commands
    runner.stop();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    const CliParseResult parsed = parseCommandLine(arguments);
    if (!parsed.options) {
        std::cerr << "error: " << parsed.error << "\n\n" << usageText();
        return 2;
    }
    const CliOptions& options = *parsed.options;
    if (options.mode == CliMode::Help) {
        std::cout << usageText();
        return 0;
    }
    if (options.mode == CliMode::Version) {
        std::cout << versionBanner() << '\n';
        return 0;
    }
    try {
        const JsonFileRepository repository(sharedDirectory(options));
        const LoadedEngine engine = repository.load(options.engineId);
        return options.mode == CliMode::Sweep ? runSweepToStdout(options, engine)
                                              : serve(options, engine);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
