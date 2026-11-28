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
    : commands_(commands), publisher_(publisher), sound_(sound), car_(engine.definition),
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
                       if (roller_) {
                           reply(incoming, ErrorReply{"a roller run is in progress"});
                           return;
                       }
                       try {
                           sweep_.emplace(command.parameters,
                                          car_.engine().definition().inertia_kgm2,
                                          car_.engine().timeStep_s());
                           stepsSinceProgress_ = 0;
                           car_.drivetrain().setNeutral(); // the brake sits on the crank
                           reply(incoming, Ack{});
                       } catch (const std::invalid_argument& error) {
                           reply(incoming, ErrorReply{error.what()});
                       }
                   },
                   [&](const StopSweep&) {
                       sweep_.reset();
                       roller_.reset();
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
                   [&](const GetOverrunSettings&) {
                       reply(incoming, OverrunSettingsData{car_.engine().ecu().tune().overrun});
                   },
                   [&](const SetOverrunSettings& command) {
                       if (const std::string error = validateOverrunSettings(command.settings);
                           !error.empty()) {
                           reply(incoming, ErrorReply{error});
                           return;
                       }
                       car_.engine().ecu().tune().overrun = command.settings;
                       reply(incoming, Ack{});
                       // Everyone sees the new settings (several UIs may be connected).
                       publisher_.send({"", "", OverrunSettingsData{command.settings}});
                   },
                   [&](const GetLaunchSettings&) {
                       reply(incoming, LaunchSettingsData{car_.engine().ecu().tune().launch});
                   },
                   [&](const SetLaunchSettings& command) {
                       if (const std::string error = validateLaunchSettings(command.settings);
                           !error.empty()) {
                           reply(incoming, ErrorReply{error});
                           return;
                       }
                       car_.engine().ecu().tune().launch = command.settings;
                       reply(incoming, Ack{});
                       publisher_.send({"", "", LaunchSettingsData{command.settings}});
                   },
                   [&](const SetLaunchSwitch& command) {
                       launchSwitch_ = command.held;
                       reply(incoming, Ack{});
                   },
                   [&](const Shift& command) {
                       if (sweep_ || roller_) {
                           reply(incoming, ErrorReply{"cannot shift during a dyno run"});
                       } else if (car_.drivetrain().shift(command.gear)) {
                           reply(incoming, Ack{});
                       } else {
                           reply(incoming, ErrorReply{"cannot shift to gear " +
                                                      std::to_string(command.gear) + " now"});
                       }
                   },
                   [&](const SetClutch& command) {
                       car_.drivetrain().setClutchPedal(command.pressed);
                       reply(incoming, Ack{});
                   },
                   [&](const StartRollerRun& command) { startRollerRun(incoming, command); },
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
    const Tune& tune = car_.engine().ecu().tune();
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
        return &car_.engine().definition().tables.ignitionMbt;
    }
    return nullptr;
}

Table2D* SimulationService::findEditableTable(const std::string& tableId) {
    const auto descriptor = std::find_if(tables_.begin(), tables_.end(),
                                         [&](const TableDescriptor& d) { return d.id == tableId; });
    if (descriptor == tables_.end() || !descriptor->editable) {
        return nullptr;
    }
    Tune& tune = car_.engine().ecu().tune(); // only the ECU tune is editable
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

void SimulationService::startRollerRun(const IncomingCommand& incoming,
                                       const StartRollerRun& command) {
    if (sweep_ || roller_) {
        reply(incoming, ErrorReply{"a dyno run is in progress"});
        return;
    }
    try {
        roller_.emplace(command.parameters, car_);
        stepsSinceProgress_ = 0;
        reply(incoming, Ack{});
    } catch (const std::invalid_argument& error) {
        reply(incoming, ErrorReply{error.what()});
    }
}

void SimulationService::advance() {
    if (sweep_) {
        // Engine dyno: the brake holds the speed, so no 2-step and no shift cut there.
        EngineSimulation& engine = car_.engine();
        engine.ecu().setLaunchSwitch(false);
        engine.ecu().setShiftCutRequest(false);
        publish(sweep_->step(engine), car_.drivetrain().state());
        sendProgress(sweep_->progress(), sweep_->setpoint_rpm());
        if (sweep_->finished()) {
            publisher_.send({"", "", DynoResult{sweep_->samples()}});
            sweep_.reset();
        }
        return;
    }
    if (roller_) {
        const CarState state = roller_->step(car_);
        publish(state.engine, state.drivetrain);
        sendProgress(roller_->progress(), roller_->currentRpm());
        if (roller_->finished()) {
            const int gear = roller_->gear();
            publisher_.send(
                {"", "",
                 RollerResult{gear, car_.engine().definition().drivetrain.overallRatio(gear),
                              roller_->samples()}});
            roller_.reset();
        }
        return;
    }
    car_.setLaunchButton(launchSwitch_);
    const CarState state = car_.step(pedal_); // driving on the roller dyno (or in neutral)
    publish(state.engine, state.drivetrain);
}

void SimulationService::sendProgress(double progress, double rpm) {
    const auto progressSteps = std::lround(kProgressInterval_s / car_.engine().timeStep_s());
    if (++stepsSinceProgress_ >= progressSteps) {
        stepsSinceProgress_ = 0;
        publisher_.send({"", "", DynoProgress{progress, rpm}});
    }
}

void SimulationService::publish(const SimulationState& state, const DrivetrainState& drivetrain) {
    if (sound_ != nullptr) {
        sound_->publish(acousticParams(state, car_.engine().definition()));
    }
    StateSnapshot snapshot;
    snapshot.time_s = state.time_s;
    snapshot.speed_rad_s = state.speed_rad_s;
    snapshot.pedal = sweep_ ? sweep_->throttle() : (roller_ ? 1.0 : pedal_);
    snapshot.manifoldPressure_Pa = state.manifoldPressure_Pa;
    snapshot.lambdaActual = state.engine.lambdaActual;
    snapshot.lambdaTarget = state.command.lambdaTarget;
    snapshot.ignition_deg = state.command.ignition_deg;
    snapshot.brakeTorque_Nm = state.engine.brakeTorque_Nm;
    snapshot.power_W = state.engine.brakeTorque_Nm * state.speed_rad_s;
    snapshot.airMassFlow_kg_s = state.engine.airMassFlow_kg_s;
    snapshot.fuelMassFlow_kg_s = state.engine.fuelMassFlow_kg_s;
    snapshot.exhaustTemperature_K = state.engine.exhaustTemperature_K;
    snapshot.dynoMode = sweep_ ? DynoMode::Sweep : (roller_ ? DynoMode::Roller : DynoMode::Free);
    snapshot.sweepProgress = sweep_ ? sweep_->progress() : 0.0;
    snapshot.fuelCut = state.command.fuelCut;
    snapshot.overrunFueling = state.command.overrunFueling;
    snapshot.catalystTemperature_K = state.catalystTemperature_K;
    snapshot.launchSwitch = car_.engine().ecu().launchSwitch();
    snapshot.launchActive = state.command.launchActive;
    snapshot.sparkCut = state.command.sparkCut;
    snapshot.gear = drivetrain.gear;
    snapshot.clutchEngagement = drivetrain.clutchEngagement;
    snapshot.vehicleSpeed_m_s = drivetrain.vehicleSpeed_m_s;
    snapshot.wheelTorque_Nm = drivetrain.wheelTorque_Nm;
    snapshot.clutchLocked = drivetrain.clutchState == ClutchState::Locked;
    snapshot.shifting = drivetrain.shifting;
    publisher_.publishState(snapshot);
}

} // namespace simcore
