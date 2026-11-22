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

} // namespace simcore
