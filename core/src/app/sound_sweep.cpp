#include "app/sound_sweep.hpp"

#include "app/engine_acoustics.hpp"
#include "app/sound_renderer.hpp"
#include "domain/common/units.hpp"
#include "domain/drivetrain/car_simulation.hpp"
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

namespace {

/// Free-running engine (no brake) driven by `inputs(time_s, ecu)`, which returns the throttle
/// and may set ECU switches; sound rendered in lockstep with the simulation.
template <typename Inputs>
std::vector<float> renderFreeRunning(const EngineDefinition& definition, const Tune& tune,
                                     double duration_s, Inputs inputs, std::uint32_t sampleRate,
                                     std::uint32_t seed) {
    EngineSimulation simulation(definition, tune);
    SoundRenderer renderer(exhaustLayout(definition), static_cast<float>(sampleRate), seed);
    renderer.setOutput(kFullVolume, false);

    const double dt = simulation.timeStep_s();
    const auto framesPerStep =
        static_cast<std::size_t>(std::lround(static_cast<double>(sampleRate) * dt));
    const auto steps = static_cast<long>(std::lround(duration_s / dt));
    std::vector<float> output;
    std::vector<float> block(framesPerStep * kStereo);
    for (long step = 0; step < steps; ++step) {
        const double throttle = inputs(static_cast<double>(step) * dt, simulation.ecu());
        renderer.publish(acousticParams(simulation.step(throttle, 0.0), definition));
        renderer.render(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    return output;
}

} // namespace

std::vector<float> renderBlipSound(const EngineDefinition& definition,
                                   const OverrunSettings& overrun, std::uint32_t sampleRate,
                                   std::uint32_t seed) {
    Tune tune = Tune::fromTables(definition.tables);
    tune.overrun = overrun;
    return renderFreeRunning(
        definition, tune, kBlipScenarioDuration_s,
        [](double time_s, Ecu& /*ecu*/) { return blipScenarioThrottle(time_s); }, sampleRate, seed);
}

std::vector<float> renderDriveSound(const EngineDefinition& definition,
                                    const LaunchSettings& launch, std::uint32_t sampleRate,
                                    std::uint32_t seed) {
    Tune tune = Tune::fromTables(definition.tables);
    tune.launch = launch;
    CarSimulation car(definition, tune);
    SoundRenderer renderer(exhaustLayout(definition), static_cast<float>(sampleRate), seed);
    renderer.setOutput(kFullVolume, false);
    car.drivetrain().setClutchPedal(true);
    static_cast<void>(car.drivetrain().shift(1));

    const double dt = car.engine().timeStep_s();
    const auto framesPerStep =
        static_cast<std::size_t>(std::lround(static_cast<double>(sampleRate) * dt));
    const auto steps = std::lround(kDriveScenarioDuration_s / dt);
    const auto clutchUpStep = std::lround(kDriveClutchUp_s / dt);
    const auto throttleStep = std::lround(kDriveThrottleFrom_s / dt);
    std::vector<float> output;
    std::vector<float> block(framesPerStep * kStereo);
    for (long step = 0; step < steps; ++step) {
        if (step == clutchUpStep) {
            car.drivetrain().setClutchPedal(false);
        }
        const CarState state = car.step(step >= throttleStep ? 1.0 : 0.0);
        const DrivetrainState& drivetrain = state.drivetrain;
        if (units::radSToRpm(state.engine.speed_rad_s) >= kDriveShift_rpm &&
            drivetrain.clutchState == ClutchState::Locked && drivetrain.gear >= 1 &&
            drivetrain.gear < kDriveTopGear) {
            static_cast<void>(car.drivetrain().shift(drivetrain.gear + 1));
        }
        renderer.publish(acousticParams(state.engine, definition));
        renderer.render(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    return output;
}

LaunchScenarioInputs launchScenarioInputs(double time_s) noexcept {
    const bool held = time_s >= kLaunchHoldStart_s && time_s < kLaunchRelease_s;
    const bool launching = time_s >= kLaunchRelease_s && time_s < kLaunchLiftOff_s;
    return {held || launching ? 1.0 : 0.0, held};
}

std::vector<float> renderLaunchSound(const EngineDefinition& definition,
                                     const LaunchSettings& launch, std::uint32_t sampleRate,
                                     std::uint32_t seed) {
    Tune tune = Tune::fromTables(definition.tables);
    tune.launch = launch;
    return renderFreeRunning(
        definition, tune, kLaunchScenarioDuration_s,
        [](double time_s, Ecu& ecu) {
            const LaunchScenarioInputs inputs = launchScenarioInputs(time_s);
            ecu.setLaunchSwitch(inputs.launchSwitch);
            return inputs.throttle;
        },
        sampleRate, seed);
}

} // namespace simcore
