#pragma once

// WebSocket adapter (docs/adr/0004-websocket-library.md, protocol in docs/interface.md).
// Listens on 127.0.0.1 only. Incoming text is decoded and submitted to the MessageBus;
// a publisher thread sends the latest state at 60 Hz and forwards replies and events.
// Browser clients from foreign web pages are refused by their Origin (adapters/origin_policy).

#include "adapters/message_bus.hpp"
#include "adapters/protocol.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ix {
class WebSocketServer;
class WebSocket;
} // namespace ix

namespace simcore {

class WebSocketServer {
public:
    static constexpr int kDefaultPort = 47800; // docs/plan.md §6
    static constexpr const char* kHost = "127.0.0.1";

    WebSocketServer(MessageBus& bus, const ProtocolCodec& codec, int port = kDefaultPort);
    ~WebSocketServer();
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    /// Binds the port and starts serving; returns an error text on failure.
    [[nodiscard]] std::string start();
    void stop();

    [[nodiscard]] int port() const noexcept { return port_; }
    [[nodiscard]] std::size_t clientCount();

private:
    void onOpen(const std::string& clientId, ix::WebSocket& socket,
                const std::optional<std::string>& origin);
    [[nodiscard]] bool isRegistered(const std::string& clientId);
    void onClose(const std::string& clientId);
    void onText(const std::string& clientId, ix::WebSocket& socket, const std::string& text);
    void publishLoop();
    void sendTo(const std::string& clientId, const std::string& text);
    void broadcast(const std::string& text);

    MessageBus& bus_;
    const ProtocolCodec& codec_;
    int port_;
    std::unique_ptr<ix::WebSocketServer> server_;
    std::mutex clientsMutex_;
    std::map<std::string, ix::WebSocket*> clients_; // valid between Open and Close callbacks
    std::atomic<bool> running_{false};
    std::thread publisher_;
};

} // namespace simcore
