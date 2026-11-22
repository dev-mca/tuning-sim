#include "app/sound_sweep.hpp"

#include "app/engine_acoustics.hpp"
#include "app/sound_renderer.hpp"
#include "domain/ecu/ecu.hpp"
#include "domain/engine/engine_simulation.hpp"

#include <cmath>
#include <cstddef>
#include <span>

namespace simcore {

namespace {

constexpr std::size_t kStereo = 2;
constexpr double kFullVolume = 1.0;

} // namespace

std::vector<float> renderSweepSound(const EngineDefinition& definition,
                                    const SoundSweepOptions& options) {
    Tune tune = Tune::fromTables(definition.tables);
    tune.ignition = tune.ignition.withOffset(options.ignitionOffset_deg);
    EngineSimulation simulation(definition, tune);
    SweepRun sweep(options.sweep, definition.inertia_kgm2, simulation.timeStep_s());

    const auto sampleRate = static_cast<double>(options.sampleRate);
    SoundRenderer renderer(exhaustLayout(definition), static_cast<float>(sampleRate), options.seed);
    renderer.setOutput(kFullVolume, false);

    const auto framesPerStep =
        static_cast<std::size_t>(std::lround(sampleRate * simulation.timeStep_s()));
    std::vector<float> output;
    std::vector<float> block(framesPerStep * kStereo);
    while (!sweep.finished()) {
        renderer.publish(acousticParams(sweep.step(simulation), definition));
        renderer.render(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    return output;
}

double blipScenarioThrottle(double time_s) noexcept {
    const bool firstBlip = time_s >= 1.0 && time_s < 2.5;
    const bool secondBlip = time_s >= 6.5 && time_s < 8.0;
    return firstBlip || secondBlip ? 1.0 : 0.0;
}

std::vector<float> renderBlipSound(const EngineDefinition& definition,
                                   const OverrunSettings& overrun, std::uint32_t sampleRate,
                                   std::uint32_t seed) {
    Tune tune = Tune::fromTables(definition.tables);
    tune.overrun = overrun;
    EngineSimulation simulation(definition, tune);
    SoundRenderer renderer(exhaustLayout(definition), static_cast<float>(sampleRate), seed);
    renderer.setOutput(kFullVolume, false);

    const double dt = simulation.timeStep_s();
    const auto framesPerStep =
        static_cast<std::size_t>(std::lround(static_cast<double>(sampleRate) * dt));
    const auto steps = static_cast<long>(std::lround(kBlipScenarioDuration_s / dt));
    std::vector<float> output;
    std::vector<float> block(framesPerStep * kStereo);
    for (long step = 0; step < steps; ++step) {
        const double throttle = blipScenarioThrottle(static_cast<double>(step) * dt);
        renderer.publish(acousticParams(simulation.step(throttle, 0.0), definition));
        renderer.render(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    return output;
}

} // namespace simcore
