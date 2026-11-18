#include "simcore/version.hpp"

namespace simcore {

namespace {

constexpr std::string_view kProgramName = "simcore";
constexpr std::string_view kVersion = SIMCORE_VERSION; // injected by CMake from project VERSION

} // namespace

std::string_view programName() noexcept {
    return kProgramName;
}

std::string_view version() noexcept {
    return kVersion;
}

std::string versionBanner() {
    std::string banner{kProgramName};
    banner += ' ';
    banner += kVersion;
    return banner;
}

} // namespace simcore
