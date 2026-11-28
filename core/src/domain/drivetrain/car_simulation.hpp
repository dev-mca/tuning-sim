#pragma once

// Engine + drivetrain on the roller dyno, one fixed time step at a time (iteration 10); port of
// lab car.py. Order of a step (research.md §8.3, §9):
// 1. gearbox and clutch controls, driver aids for the ECU (launch armed by the clutch, shift
//    cut for flat shift),
// 2. ECU and engine torque at the current speed (EngineSimulation::prepare),
// 3. clutch torque from the drivetrain = load on the crank,
// 4. crank integration (EngineSimulation::finish), then the car; a locking clutch sets the
//    common speed,
// 5. starter: a stalled engine restarts once the drivetrain is decoupled.

#include "domain/drivetrain/drivetrain.hpp"
#include "domain/engine/engine_simulation.hpp"

#include <optional>

namespace simcore {

struct CarState {
    SimulationState engine;
    DrivetrainState drivetrain;
};

class CarSimulation {
public:
    explicit CarSimulation(const EngineDefinition& definition,
                           std::optional<Tune> tune = std::nullopt);

    /// The launch button of iteration 7c; the clutch pedal arms the 2-step too.
    void setLaunchButton(bool held) noexcept { launchButton_ = held; }
    [[nodiscard]] bool launchButton() const noexcept { return launchButton_; }

    CarState step(double throttle) noexcept;

    [[nodiscard]] EngineSimulation& engine() noexcept { return engine_; }
    [[nodiscard]] const EngineSimulation& engine() const noexcept { return engine_; }
    [[nodiscard]] Drivetrain& drivetrain() noexcept { return drivetrain_; }
    [[nodiscard]] const Drivetrain& drivetrain() const noexcept { return drivetrain_; }

private:
    void restartIfStalled() noexcept;

    EngineSimulation engine_;
    Drivetrain drivetrain_;
    bool launchButton_ = false;
};

} // namespace simcore
