#pragma once

#include <string>
#include <string_view>

namespace simcore {

/// Program name as shown to users and in the protocol handshake.
[[nodiscard]] std::string_view programName() noexcept;

/// Semantic version "MAJOR.MINOR.PATCH", taken from the CMake project version.
[[nodiscard]] std::string_view version() noexcept;

/// Human-readable banner, e.g. "simcore 0.1.0".
[[nodiscard]] std::string versionBanner();

} // namespace simcore
