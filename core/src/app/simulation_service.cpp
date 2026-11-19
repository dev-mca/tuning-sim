#include "app/simulation_service.hpp"

#include "app/engine_acoustics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace simcore {

namespace {

// Helper for std::visit with lambdas (the "overloaded" idiom).
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

} // namespace

SimulationService::SimulationService(const LoadedEngine& engine, ICommandSource& commands,
                                     IStatePublisher& publisher, IAcousticSink* sound)
    : commands_(commands), publisher_(publisher), sound_(sound), simulation_(engine.definition),
      tables_(engine.tables) {}

void SimulationService::step() {
    while (const std::optional<IncomingCommand> incoming = commands_.poll()) {
        handle(*incoming);
    }
    advance();
}

void SimulationService::reply(const IncomingCommand& incoming, OutgoingPayload payload) {
    publisher_.send({incoming.clientId, incoming.requestId, std::move(payload)});
}

void SimulationService::handle(const IncomingCommand& incoming) {
    std::visit(Overloaded{
                   [&](const SetPedal& command) {
                       pedal_ = std::clamp(command.value, 0.0, 1.0);
                       reply(incoming, Ack{});
                   },
                   [&](const StartSweep& command) {
                       try {
                           sweep_.emplace(command.parameters, simulation_.definition().inertia_kgm2,
                                          simulation_.timeStep_s());
                           stepsSinceProgress_ = 0;
                           reply(incoming, Ack{});
                       } catch (const std::invalid_argument& error) {
                           reply(incoming, ErrorReply{error.what()});
                       }
                   },
                   [&](const StopSweep&) {
                       sweep_.reset();
                       reply(incoming, Ack{});
                   },
                   [&](const GetTable& command) {
                       if (const Table2D* table = findTable(command.tableId)) {
                           reply(incoming, TableData{command.tableId, *table});
                       } else {
                           reply(incoming, ErrorReply{"unknown table '" + command.tableId + "'"});
                       }
                   },
                   [&](const SetTableCells& command) { applyCells(incoming, command); },
                   [&](const SetAudio& command) {
                       if (sound_ != nullptr) {
                           sound_->setOutput(command.volume, command.muted);
                       }
                       reply(incoming, Ack{}); // without audio output the setting is moot
                   },
               },
               incoming.command);
}

void SimulationService::applyCells(const IncomingCommand& incoming, const SetTableCells& edit) {
    Table2D* table = findEditableTable(edit.tableId);
    if (table == nullptr) {
        reply(incoming, ErrorReply{"table '" + edit.tableId + "' does not exist or is read-only"});
        return;
    }
    // Validate everything first, then apply: an invalid cell must not leave a half edit.
    Table2D edited = *table;
    try {
        for (const CellEdit& cell : edit.cells) {
            edited.setCell(cell.x, cell.y, cell.value);
        }
    } catch (const std::exception& error) {
        reply(incoming, ErrorReply{error.what()});
        return;
    }
    *table = std::move(edited);
    reply(incoming, Ack{});
}

const Table2D* SimulationService::findTable(const std::string& tableId) const {
    const Tune& tune = simulation_.ecu().tune();
    if (tableId == "ve") {
        return &tune.ve;
    }
    if (tableId == "lambdaTarget") {
        return &tune.lambdaTarget;
    }
    if (tableId == "ignition") {
        return &tune.ignition;
    }
    if (tableId == "ignitionMbt") {
        return &simulation_.definition().tables.ignitionMbt;
    }
    return nullptr;
}

Table2D* SimulationService::findEditableTable(const std::string& tableId) {
    const auto descriptor = std::find_if(tables_.begin(), tables_.end(),
                                         [&](const TableDescriptor& d) { return d.id == tableId; });
    if (descriptor == tables_.end() || !descriptor->editable) {
        return nullptr;
    }
    Tune& tune = simulation_.ecu().tune(); // only the ECU tune is editable
    if (tableId == "ve") {
        return &tune.ve;
    }
    if (tableId == "lambdaTarget") {
        return &tune.lambdaTarget;
    }
    if (tableId == "ignition") {
        return &tune.ignition;
    }
    return nullptr;
}

void SimulationService::advance() {
    if (!sweep_) {
        publish(simulation_.step(pedal_, 0.0)); // free running, no brake
        return;
    }
    publish(sweep_->step(simulation_));
    const auto progressSteps = std::lround(kProgressInterval_s / simulation_.timeStep_s());
    if (++stepsSinceProgress_ >= progressSteps) {
        stepsSinceProgress_ = 0;
        publisher_.send({"", "", DynoProgress{sweep_->progress(), sweep_->setpoint_rpm()}});
    }
    if (sweep_->finished()) {
        publisher_.send({"", "", DynoResult{sweep_->samples()}});
        sweep_.reset();
    }
}

void SimulationService::publish(const SimulationState& state) {
    if (sound_ != nullptr) {
        sound_->publish(acousticParams(state, simulation_.definition()));
    }
    StateSnapshot snapshot;
    snapshot.time_s = state.time_s;
    snapshot.speed_rad_s = state.speed_rad_s;
    snapshot.pedal = sweep_ ? sweep_->throttle() : pedal_;
    snapshot.manifoldPressure_Pa = state.manifoldPressure_Pa;
    snapshot.lambdaActual = state.engine.lambdaActual;
    snapshot.lambdaTarget = state.command.lambdaTarget;
    snapshot.ignition_deg = state.command.ignition_deg;
    snapshot.brakeTorque_Nm = state.engine.brakeTorque_Nm;
    snapshot.power_W = state.engine.brakeTorque_Nm * state.speed_rad_s;
    snapshot.airMassFlow_kg_s = state.engine.airMassFlow_kg_s;
    snapshot.fuelMassFlow_kg_s = state.engine.fuelMassFlow_kg_s;
    snapshot.exhaustTemperature_K = state.engine.exhaustTemperature_K;
    snapshot.dynoMode = sweep_ ? DynoMode::Sweep : DynoMode::Free;
    snapshot.sweepProgress = sweep_ ? sweep_->progress() : 0.0;
    snapshot.fuelCut = state.command.fuelCut;
    publisher_.publishState(snapshot);
}

} // namespace simcore
