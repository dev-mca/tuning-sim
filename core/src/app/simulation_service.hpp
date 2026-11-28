#pragma once

// Application core: owns the engine simulation, executes commands, runs dyno sweeps and
// publishes the state. Talks to the outside only through ports (ICommandSource,
// IStatePublisher), so it is tested without network or threads.

#include "domain/drivetrain/car_simulation.hpp"
#include "domain/dyno/dyno.hpp"
#include "domain/dyno/roller.hpp"
#include "ports/ports.hpp"

#include <optional>
#include <vector>

namespace simcore {

class SimulationService {
public:
    /// `sound` is optional: without it (tests, CLI sweeps) no acoustic parameters are produced.
    SimulationService(const LoadedEngine& engine, ICommandSource& commands,
                      IStatePublisher& publisher, IAcousticSink* sound = nullptr);

    /// One fixed time step: handle pending commands, advance the simulation, publish state.
    void step();

    [[nodiscard]] double timeStep_s() const noexcept { return car_.engine().timeStep_s(); }
    [[nodiscard]] const EngineSimulation& simulation() const noexcept { return car_.engine(); }
    [[nodiscard]] const CarSimulation& car() const noexcept { return car_; }

    /// Dyno progress events are sent at this interval while a sweep runs.
    static constexpr double kProgressInterval_s = 0.1;

private:
    void handle(const IncomingCommand& incoming);
    void reply(const IncomingCommand& incoming, OutgoingPayload payload);
    void applyCells(const IncomingCommand& incoming, const SetTableCells& edit);
    [[nodiscard]] const Table2D* findTable(const std::string& tableId) const;
    [[nodiscard]] Table2D* findEditableTable(const std::string& tableId);
    void startRollerRun(const IncomingCommand& incoming, const StartRollerRun& command);
    void advance();
    void sendProgress(double progress, double rpm);
    void publish(const SimulationState& state, const DrivetrainState& drivetrain);

    ICommandSource& commands_;
    IStatePublisher& publisher_;
    IAcousticSink* sound_;
    CarSimulation car_; // the car on the roller dyno; in neutral the engine revs freely
    std::vector<TableDescriptor> tables_;
    double pedal_ = 0.0;
    bool launchSwitch_ = false; // launch button as sent by the UI (ignored during dyno runs)
    std::optional<SweepRun> sweep_;
    std::optional<RollerRun> roller_;
    long stepsSinceProgress_ = 0;
};

} // namespace simcore
