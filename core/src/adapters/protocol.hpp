#pragma once

// JSON protocol v1 between simcore and the UI (docs/interface.md).
// Envelope: { "v": 1, "type": "<type>", "id": "<optional request id>", "payload": { ... } }.
// Converts between protocol units (1/min, kPa, %, kW, degC) and SI at this edge.

#include "ports/ports.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace simcore {

inline constexpr int kProtocolVersion = 1;
inline constexpr double kStateRate_Hz = 60.0;

struct DecodeResult {
    std::optional<IncomingCommand> command; // set on success
    std::string requestId;                  // echoed in an error reply if present
    std::string error;                      // set on failure
};

class ProtocolCodec {
public:
    /// `audio` is announced in hello, so the UI knows whether sound can play.
    explicit ProtocolCodec(const LoadedEngine& engine, AudioInfo audio = {});

    [[nodiscard]] DecodeResult decode(std::string_view text, const std::string& clientId) const;

    [[nodiscard]] std::string encodeHello() const;
    [[nodiscard]] std::string encodeState(const StateSnapshot& state) const;
    [[nodiscard]] std::string encode(const OutgoingMessage& message) const;
    [[nodiscard]] std::string encodeError(const std::string& requestId,
                                          const std::string& message) const;

private:
    [[nodiscard]] const TableDescriptor* descriptor(const std::string& tableId) const;

    std::string engineId_;
    std::string engineName_;
    int cylinders_;
    double displacement_m3_;
    double idleSpeed_rad_s_;
    double revLimit_rad_s_;
    std::vector<TableDescriptor> tables_;
    AudioInfo audio_;
};

} // namespace simcore
