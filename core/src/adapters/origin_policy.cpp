#include "adapters/origin_policy.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace simcore::origin_policy {

namespace {

constexpr std::string_view kSchemeSeparator = "://";
constexpr int kMaxPort = 65535;

// Origins of the packaged Tauri v2 app (tauri:// on Linux/macOS, http(s)://tauri.localhost on
// Windows). They carry no port.
constexpr std::array<std::string_view, 3> kTauriOrigins = {
    "tauri://localhost", "http://tauri.localhost", "https://tauri.localhost"};

// Schemes accepted together with a loopback host and any port, see header.
constexpr std::array<std::string_view, 3> kLoopbackSchemes = {"http", "ws", "wss"};
constexpr std::array<std::string_view, 3> kLoopbackHosts = {"localhost", "127.0.0.1", "[::1]"};

std::string toLower(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

template <std::size_t N>
bool contains(const std::array<std::string_view, N>& list, std::string_view value) {
    return std::ranges::find(list, value) != list.end();
}

/// Empty (default port) or a decimal port number 1..65535.
bool isValidPort(std::string_view port) {
    if (port.empty()) {
        return true;
    }
    if (port.size() > std::to_string(kMaxPort).size() ||
        !std::ranges::all_of(port, [](unsigned char c) { return std::isdigit(c) != 0; })) {
        return false;
    }
    const int number = std::stoi(std::string(port));
    return number >= 1 && number <= kMaxPort;
}

/// Splits "host:port" (host may be a bracketed IPv6 literal) and checks both parts.
bool isLoopbackAuthority(std::string_view authority) {
    const std::size_t hostEnd =
        authority.starts_with('[') ? authority.find(']') + 1 : authority.find(':');
    const std::string_view host = authority.substr(0, hostEnd);
    std::string_view rest =
        hostEnd >= authority.size() ? std::string_view{} : authority.substr(hostEnd);
    if (!rest.empty()) {
        if (rest.front() != ':' || rest.size() == 1) {
            return false; // garbage after the host, or "host:" without a port
        }
        rest.remove_prefix(1);
    }
    return contains(kLoopbackHosts, host) && isValidPort(rest);
}

} // namespace

bool isAllowed(std::optional<std::string_view> origin) {
    if (!origin) {
        return true;
    }
    const std::string lower = toLower(*origin);
    if (contains(kTauriOrigins, lower)) {
        return true;
    }
    const std::size_t separator = lower.find(kSchemeSeparator);
    if (separator == std::string::npos) {
        return false; // includes the opaque origin "null"
    }
    const std::string_view view = lower;
    const std::string_view scheme = view.substr(0, separator);
    const std::string_view authority = view.substr(separator + kSchemeSeparator.size());
    return contains(kLoopbackSchemes, scheme) && isLoopbackAuthority(authority);
}

} // namespace simcore::origin_policy
