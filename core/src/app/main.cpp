// simcore: headless simulation process (docs/adr/0001-architecture.md).
// Wiring only: parse the command line, create adapters, connect them to the application core.

#include "adapters/audio_output.hpp"
#include "adapters/json_file_repository.hpp"
#include "adapters/message_bus.hpp"
#include "adapters/protocol.hpp"
#include "adapters/steady_clock.hpp"
#include "adapters/sweep_csv.hpp"
#include "adapters/wav_writer.hpp"
#include "adapters/websocket_server.hpp"
#include "app/cli.hpp"
#include "app/engine_acoustics.hpp"
#include "app/realtime_runner.hpp"
#include "app/simulation_service.hpp"
#include "app/sound_renderer.hpp"
#include "app/sound_sweep.hpp"
#include "domain/common/units.hpp"
#include "domain/dyno/dyno.hpp"
#include "simcore/version.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
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

constexpr std::uint32_t kSampleRate = 48000; // plan.md §2.2
constexpr std::uint32_t kSoundSeed = 1;
constexpr std::uint16_t kStereo = 2;

int runSoundSweep(const CliOptions& options, const LoadedEngine& engine) {
    SoundSweepOptions sound;
    sound.sweep = {options.sweepFrom_rpm, options.sweepTo_rpm};
    sound.sweep.rate_rpm_s = options.sweepRate_rpm_s.value_or(kDefaultSweepRate_rpm_s);
    sound.ignitionOffset_deg = options.ignitionOffset_deg;
    sound.sampleRate = kSampleRate;
    const std::vector<float> samples = renderSweepSound(engine.definition, sound);
    if (!writeWav16(options.soundSweepFile, samples, kStereo, kSampleRate)) {
        std::cerr << "error: cannot write " << options.soundSweepFile << '\n';
        return 1;
    }
    std::cout << "wrote " << options.soundSweepFile << " ("
              << static_cast<double>(samples.size() / kStereo) / kSampleRate << " s)\n";
    return 0;
}

int runSoundBlips(const CliOptions& options, const LoadedEngine& engine) {
    const OverrunSettings overrun{options.overrunFuelCut, options.overrunRetard_deg,
                                  options.overrunEnrichment};
    const std::vector<float> samples = renderBlipSound(engine.definition, overrun, kSampleRate);
    if (!writeWav16(options.soundBlipsFile, samples, kStereo, kSampleRate)) {
        std::cerr << "error: cannot write " << options.soundBlipsFile << '\n';
        return 1;
    }
    std::cout << "wrote " << options.soundBlipsFile << " ("
              << static_cast<double>(samples.size() / kStereo) / kSampleRate << " s)\n";
    return 0;
}

int runSoundLaunch(const CliOptions& options, const LoadedEngine& engine) {
    const LaunchSettings launch{true, units::rpmToRadS(options.launch_rpm),
                                options.launchRetard_deg};
    if (const std::string error = validateLaunchSettings(launch); !error.empty()) {
        std::cerr << "error: " << error << '\n';
        return 2;
    }
    const std::vector<float> samples = renderLaunchSound(engine.definition, launch, kSampleRate);
    if (!writeWav16(options.soundLaunchFile, samples, kStereo, kSampleRate)) {
        std::cerr << "error: cannot write " << options.soundLaunchFile << '\n';
        return 1;
    }
    std::cout << "wrote " << options.soundLaunchFile << " ("
              << static_cast<double>(samples.size() / kStereo) / kSampleRate << " s)\n";
    return 0;
}

int runSoundDrive(const CliOptions& options, const LoadedEngine& engine) {
    const LaunchSettings launch{true, units::rpmToRadS(options.launch_rpm),
                                options.launchRetard_deg, options.flatShift};
    const std::vector<float> samples = renderDriveSound(engine.definition, launch, kSampleRate);
    if (!writeWav16(options.soundDriveFile, samples, kStereo, kSampleRate)) {
        std::cerr << "error: cannot write " << options.soundDriveFile << '\n';
        return 1;
    }
    std::cout << "wrote " << options.soundDriveFile << " ("
              << static_cast<double>(samples.size() / kStereo) / kSampleRate << " s)\n";
    return 0;
}

int serve(const CliOptions& options, const LoadedEngine& engine) {
    constexpr auto kMainLoopInterval = std::chrono::milliseconds(100);
    auto bus = std::make_unique<MessageBus>(); // large queues: keep them off the stack
    SoundRenderer sound(exhaustLayout(engine.definition), static_cast<float>(kSampleRate),
                        kSoundSeed);
    std::unique_ptr<AudioOutput> audio;
    AudioInfo audioInfo{false, "disabled with --no-audio"};
    if (options.audio) {
        audio = std::make_unique<AudioOutput>(sound, kSampleRate);
        audioInfo = audio->info();
    }
    const ProtocolCodec codec(engine, audioInfo);
    SimulationService service(engine, *bus, *bus, &sound);
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
              << " (Ctrl+C to stop)\n"
              << "audio: " << (audioInfo.available ? "" : "off, ") << audioInfo.device << std::endl;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    while (!gStopRequested) {
        std::this_thread::sleep_for(kMainLoopInterval);
    }
    std::cout << "\nstopping" << std::endl;
    server.stop(); // stop the network first: no new commands
    runner.stop();
    audio.reset(); // stop the audio callback before the renderer goes away
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
        switch (options.mode) {
        case CliMode::Sweep:
            return runSweepToStdout(options, engine);
        case CliMode::SoundSweep:
            return runSoundSweep(options, engine);
        case CliMode::SoundBlips:
            return runSoundBlips(options, engine);
        case CliMode::SoundLaunch:
            return runSoundLaunch(options, engine);
        case CliMode::SoundDrive:
            return runSoundDrive(options, engine);
        default:
            return serve(options, engine);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
