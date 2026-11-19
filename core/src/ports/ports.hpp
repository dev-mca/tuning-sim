#pragma once

// Ports of the hexagon (docs/adr/0001-architecture.md): interfaces the application core needs
// from the outside. Adapters implement them; tests use fakes. Like Java interfaces.

#include "acoustic/acoustic_params.hpp"
#include "domain/engine/engine_definition.hpp"
#include "ports/messages.hpp"

#include <optional>
#include <span>
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

/// Engine sound (docs/plan.md §2.2). The simulation thread publishes the acoustic parameters
/// every step; volume changes arrive as commands. Implementations must never block the caller.
class IAcousticSink {
public:
    virtual ~IAcousticSink() = default;
    virtual void publish(const acoustic::AcousticParams& params) = 0;
    /// volume in [0, 1]; muted silences the output without losing the volume setting.
    virtual void setOutput(double volume, bool muted) = 0;
};

/// Called by an audio output adapter on its real-time thread to fill interleaved stereo.
/// Must not allocate, lock, block or throw (research.md §6.8).
class IAudioRenderer {
public:
    virtual ~IAudioRenderer() = default;
    virtual void render(std::span<float> interleavedStereo) noexcept = 0;
};

/// Audio output as reported to clients (hello): whether sound plays and on which device.
struct AudioInfo {
    bool available = false;
    std::string device;
};

/// Monotonic time source (seconds); faked in tests of the real-time loop.
class IClock {
public:
    virtual ~IClock() = default;
    [[nodiscard]] virtual double now_s() const = 0;
};

} // namespace simcore
