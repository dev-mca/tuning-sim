#pragma once

// Application core: owns the engine simulation, executes commands, runs dyno sweeps and
// publishes the state. Talks to the outside only through ports (ICommandSource,
// IStatePublisher), so it is tested without network or threads.

#include "domain/dyno/dyno.hpp"
#include "domain/engine/engine_simulation.hpp"
#include "ports/ports.hpp"

#include <optional>
#include <vector>

namespace simcore {

class SimulationService {
public:
    SimulationService(const LoadedEngine& engine, ICommandSource& commands,
                      IStatePublisher& publisher);

    /// One fixed time step: handle pending commands, advance the simulation, publish state.
    void step();

    [[nodiscard]] double timeStep_s() const noexcept { return simulation_.timeStep_s(); }
    [[nodiscard]] const EngineSimulation& simulation() const noexcept { return simulation_; }

    /// Dyno progress events are sent at this interval while a sweep runs.
    static constexpr double kProgressInterval_s = 0.1;

private:
    void handle(const IncomingCommand& incoming);
    void reply(const IncomingCommand& incoming, OutgoingPayload payload);
    void applyCells(const IncomingCommand& incoming, const SetTableCells& edit);
    [[nodiscard]] const Table2D* findTable(const std::string& tableId) const;
    [[nodiscard]] Table2D* findEditableTable(const std::string& tableId);
    void advance();
    void publish(const SimulationState& state);

    ICommandSource& commands_;
    IStatePublisher& publisher_;
    EngineSimulation simulation_;
    std::vector<TableDescriptor> tables_;
    double pedal_ = 0.0;
    std::optional<SweepRun> sweep_;
    long stepsSinceProgress_ = 0;
};

} // namespace simcore
