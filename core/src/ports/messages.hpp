#pragma once

// Messages between the outside world (network adapter) and the simulation service.
// Plain C++ types in SI units: the JSON protocol lives in adapters/protocol (docs/interface.md).

#include "domain/dyno/dyno.hpp"
#include "domain/ecu/ecu.hpp"
#include "domain/ecu/table.hpp"

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace simcore {

// --- commands (client -> simulation) ---

struct SetPedal {
    double value; // 0..1
};
struct StartSweep {
    SweepParameters parameters;
};
struct StopSweep {};
struct GetTable {
    std::string tableId;
};
struct CellEdit {
    std::size_t x;
    std::size_t y;
    double value; // SI, already converted from the table's display unit
};
struct SetTableCells {
    std::string tableId;
    std::vector<CellEdit> cells;
};

struct SetAudio {
    double volume; // 0..1
    bool muted;
};

struct GetOverrunSettings {};
struct SetOverrunSettings {
    OverrunSettings settings; // SI: enrichment as fraction
};

using Command = std::variant<SetPedal, StartSweep, StopSweep, GetTable, SetTableCells, SetAudio,
                             GetOverrunSettings, SetOverrunSettings>;

struct IncomingCommand {
    std::string clientId;
    std::string requestId; // empty if the client sent none
    Command command;
};

// --- replies and events (simulation -> client) ---

struct Ack {};
struct ErrorReply {
    std::string message;
};
struct TableData {
    std::string tableId;
    Table2D table; // SI
};
struct DynoProgress {
    double progress; // 0..1
    double setpoint_rpm;
};
struct DynoResult {
    std::vector<SweepSample> samples;
};

struct OverrunSettingsData {
    OverrunSettings settings;
};

using OutgoingPayload =
    std::variant<Ack, ErrorReply, TableData, DynoProgress, DynoResult, OverrunSettingsData>;

struct OutgoingMessage {
    std::string clientId; // empty = broadcast to all clients
    std::string requestId;
    OutgoingPayload payload;
};

// --- state (simulation -> clients, sent at 60 Hz) ---

enum class DynoMode { Free, Sweep };

struct StateSnapshot {
    double time_s = 0.0;
    double speed_rad_s = 0.0;
    double pedal = 0.0;
    double manifoldPressure_Pa = 0.0;
    double lambdaActual = 0.0; // +infinity during fuel cut
    double lambdaTarget = 0.0;
    double ignition_deg = 0.0;
    double brakeTorque_Nm = 0.0;
    double power_W = 0.0;
    double airMassFlow_kg_s = 0.0;
    double fuelMassFlow_kg_s = 0.0;
    double exhaustTemperature_K = 0.0;
    DynoMode dynoMode = DynoMode::Free;
    double sweepProgress = 0.0;
    bool fuelCut = false;
    bool overrunFueling = false; // fuelling in overrun with the cut switched off (pops possible)
    double catalystTemperature_K = 0.0;
};

} // namespace simcore
