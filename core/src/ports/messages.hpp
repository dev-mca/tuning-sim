#pragma once

// Messages between the outside world (network adapter) and the simulation service.
// Plain C++ types in SI units: the JSON protocol lives in adapters/protocol (docs/interface.md).

#include "domain/dyno/dyno.hpp"
#include "domain/ecu/ecu.hpp"
#include "domain/ecu/table.hpp"

#include <cstddef>
#include <optional>
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

struct GetLaunchSettings {};
struct SetLaunchSettings {
    LaunchSettings settings; // SI: launch speed in rad/s
};
/// The driver's launch button (2-step, research.md §3.4); like the pedal a live input.
struct SetLaunchSwitch {
    bool held;
};

using Command = std::variant<SetPedal, StartSweep, StopSweep, GetTable, SetTableCells, SetAudio,
                             GetOverrunSettings, SetOverrunSettings, GetLaunchSettings,
                             SetLaunchSettings, SetLaunchSwitch>;

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

struct LaunchSettingsData {
    LaunchSettings settings;
};

using OutgoingPayload = std::variant<Ack, ErrorReply, TableData, DynoProgress, DynoResult,
                                     OverrunSettingsData, LaunchSettingsData>;

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
    bool launchSwitch = false; // launch button held (ignored during a dyno sweep)
    bool launchActive = false; // launch control works (enabled, switch held, throttle open)
    bool sparkCut = false;     // launch control cuts the spark right now
    // Agreed fields for later iterations (plan.md §11); empty = not available (sent as null).
    // Charging (iteration 8):
    std::optional<double> intakeTemperature_K; // after the intercooler; null = ambient model
    std::optional<double> boostPressure_Pa;    // gauge pressure above ambient
    std::optional<double> turboSpeed_rad_s;    // turbocharger shaft speed
    std::optional<double> wastegateDuty;       // 0..1
    double knockRetard_deg = 0.0;              // ignition taken back by the knock control
    // Drivetrain (iteration 10):
    std::optional<int> gear;                // 0 = neutral, 1.. = forward gears
    std::optional<double> clutchEngagement; // 0 = open (pedal pressed) .. 1 = closed
    std::optional<double> vehicleSpeed_m_s;
    std::optional<double> wheelTorque_Nm; // sum of both driven wheels
};

} // namespace simcore
