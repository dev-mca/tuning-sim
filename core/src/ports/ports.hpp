#pragma once

// Ports of the hexagon (docs/adr/0001-architecture.md): interfaces the application core needs
// from the outside. Adapters implement them; tests use fakes. Like Java interfaces.
// Not yet needed (later iterations): IAudioSink (7), ITableRepository for saving tunes (6).

#include "domain/engine/engine_definition.hpp"
#include "ports/messages.hpp"

#include <optional>
#include <string>
#include <vector>

namespace simcore {

/// Display metadata of a table, as stored in shared/tables/*.json (docs/plan.md §5.1).
struct AxisDescriptor {
    std::string name;
    std::string unit;
};
struct TableDescriptor {
    std::string id;
    std::string name;
    std::string unit;
    AxisDescriptor xAxis;
    AxisDescriptor yAxis;
    double min;
    double max;
    bool editable; // ECU tune tables yes; model properties (ignitionMbt) no
};

struct LoadedEngine {
    EngineDefinition definition; // SI
    std::vector<TableDescriptor> tables;
};

/// Loads engine definitions with their tables.
class IEngineRepository {
public:
    virtual ~IEngineRepository() = default;
    /// Throws std::runtime_error if the engine cannot be loaded.
    [[nodiscard]] virtual LoadedEngine load(const std::string& engineId) const = 0;
};

/// Commands from clients; polled by the simulation thread.
class ICommandSource {
public:
    virtual ~ICommandSource() = default;
    [[nodiscard]] virtual std::optional<IncomingCommand> poll() = 0;
};

/// State and replies towards clients; called from the simulation thread.
class IStatePublisher {
public:
    virtual ~IStatePublisher() = default;
    virtual void publishState(const StateSnapshot& state) = 0;
    virtual void send(OutgoingMessage message) = 0;
};

/// Monotonic time source (seconds); faked in tests of the real-time loop.
class IClock {
public:
    virtual ~IClock() = default;
    [[nodiscard]] virtual double now_s() const = 0;
};

} // namespace simcore
