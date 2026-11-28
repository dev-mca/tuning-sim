// Origin allowlist of the WebSocket server (docs/adr/0011-websocket-origin-check.md).

#include "adapters/origin_policy.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string_view>

using simcore::origin_policy::isAllowed;

TEST_CASE("clients without an Origin header are allowed") {
    // CLI tools and the Node e2e test send no Origin.
    CHECK(isAllowed(std::nullopt));
}

TEST_CASE("the Vite dev server on loopback is allowed on any port") {
    CHECK(isAllowed("http://localhost:5173"));
    CHECK(isAllowed("http://127.0.0.1:5173"));
    CHECK(isAllowed("http://localhost:5174")); // Vite moves on when 5173 is taken
    CHECK(isAllowed("http://localhost:4173")); // vite preview
    CHECK(isAllowed("http://[::1]:5173"));
    CHECK(isAllowed("http://localhost")); // default port
}

TEST_CASE("the Tauri app origins are allowed") {
    CHECK(isAllowed("tauri://localhost"));
    CHECK(isAllowed("http://tauri.localhost"));
    CHECK(isAllowed("https://tauri.localhost"));
}

TEST_CASE("IXWebSocket clients, which send their own ws:// URL, are allowed") {
    CHECK(isAllowed("ws://127.0.0.1:47800"));
    CHECK(isAllowed("ws://localhost:47800"));
    CHECK(isAllowed("wss://127.0.0.1:47800"));
}

TEST_CASE("foreign web pages are rejected") {
    CHECK_FALSE(isAllowed("https://evil.example"));
    CHECK_FALSE(isAllowed("http://example.com:5173"));
    CHECK_FALSE(isAllowed("http://localhost.evil.example:5173")); // prefix trick
    CHECK_FALSE(isAllowed("http://127.0.0.1.nip.io:5173"));
    CHECK_FALSE(isAllowed("http://tauri.localhost.evil.example"));
    CHECK_FALSE(isAllowed("http://192.168.1.10:5173")); // LAN, not loopback
    CHECK_FALSE(isAllowed("https://localhost:5173"));   // not used by Vite or Tauri
    CHECK_FALSE(isAllowed("file://"));
}

TEST_CASE("the opaque origin null and malformed values are rejected") {
    // "null" comes from sandboxed iframes and local files, which anyone can craft.
    CHECK_FALSE(isAllowed("null"));
    CHECK_FALSE(isAllowed(""));
    CHECK_FALSE(isAllowed("localhost:5173"));
    CHECK_FALSE(isAllowed("http://"));
    CHECK_FALSE(isAllowed("http://localhost:"));
    CHECK_FALSE(isAllowed("http://localhost:0"));
    CHECK_FALSE(isAllowed("http://localhost:65536"));
    CHECK_FALSE(isAllowed("http://localhost:51a3"));
    CHECK_FALSE(isAllowed("http://localhost:-1"));
    CHECK_FALSE(isAllowed("http://[::1"));
    CHECK_FALSE(isAllowed("http://user@localhost:5173"));
}

TEST_CASE("scheme and host compare case-insensitively") {
    CHECK(isAllowed("HTTP://LocalHost:5173"));
    CHECK(isAllowed("Tauri://LOCALHOST"));
}

TEST_CASE("a trailing slash or path is rejected because a browser never sends one") {
    CHECK_FALSE(isAllowed("http://localhost:5173/"));
    CHECK_FALSE(isAllowed("http://localhost/"));
    CHECK_FALSE(isAllowed("tauri://localhost/"));
    CHECK_FALSE(isAllowed("http://localhost:5173/app"));
}
