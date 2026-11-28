#pragma once

// Which WebSocket clients may control simcore, judged by the HTTP `Origin` header of the
// handshake (docs/adr/0011-websocket-origin-check.md). simcore only listens on 127.0.0.1, but
// any web page open in the user's browser can still open ws://127.0.0.1:47800; the browser then
// sends that page's origin, which a page cannot forge. Non-browser clients (CLI tools, the Node
// e2e test) send no Origin at all.

#include <optional>
#include <string_view>

namespace simcore::origin_policy {

/// True if a client with this Origin header may connect. `std::nullopt` means the header is
/// absent (non-browser client) and is allowed. Allowed origins:
/// - http://localhost, http://127.0.0.1, http://[::1] with any port (Vite dev server, which
///   moves to 5174, 5175, ... when 5173 is taken, and `vite preview` on 4173)
/// - ws:// and wss:// with one of these loopback hosts and any port: IXWebSocket clients
///   (our C++ tests) send their own URL as Origin; browsers never send ws:// origins
/// - the Tauri v2 app: tauri://localhost, http://tauri.localhost, https://tauri.localhost
/// Scheme and host compare case-insensitively (RFC 6454 §6.1); anything else, including the
/// opaque origin "null" and origins with a path or trailing slash, is rejected.
[[nodiscard]] bool isAllowed(std::optional<std::string_view> origin);

} // namespace simcore::origin_policy
