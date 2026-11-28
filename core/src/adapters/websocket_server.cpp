#include "adapters/websocket_server.hpp"

#include "adapters/origin_policy.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <chrono>
#include <cstdint>
#include <optional>

namespace simcore {

namespace {

constexpr auto kPublishInterval = std::chrono::microseconds(16'667); // 60 Hz
constexpr std::uint16_t kPolicyViolationCloseCode = 1008;            // RFC 6455 §7.4.1

std::optional<std::string> originOf(const ix::WebSocketOpenInfo& openInfo) {
    // The header map compares names case-insensitively.
    if (const auto origin = openInfo.headers.find("Origin"); origin != openInfo.headers.end()) {
        return origin->second;
    }
    return std::nullopt;
}

} // namespace

WebSocketServer::WebSocketServer(MessageBus& bus, const ProtocolCodec& codec, int port)
    : bus_(bus), codec_(codec), port_(port) {
    ix::initNetSystem(); // required on Windows, no-op elsewhere
}

WebSocketServer::~WebSocketServer() {
    stop();
}

std::string WebSocketServer::start() {
    server_ = std::make_unique<ix::WebSocketServer>(port_, kHost);
    server_->setOnClientMessageCallback(
        [this](const std::shared_ptr<ix::ConnectionState>& connection, ix::WebSocket& socket,
               const ix::WebSocketMessagePtr& message) {
            const std::string& clientId = connection->getId();
            switch (message->type) {
            case ix::WebSocketMessageType::Open:
                onOpen(clientId, socket, originOf(message->openInfo));
                break;
            case ix::WebSocketMessageType::Close:
                onClose(clientId);
                break;
            case ix::WebSocketMessageType::Message:
                onText(clientId, socket, message->str);
                break;
            default:
                break; // ping/pong/fragments/errors are handled by the library
            }
        });
    const auto [ok, error] = server_->listen();
    if (!ok) {
        server_.reset();
        return error;
    }
    server_->start();
    running_ = true;
    publisher_ = std::thread([this] { publishLoop(); });
    return {};
}

void WebSocketServer::stop() {
    if (running_.exchange(false) && publisher_.joinable()) {
        publisher_.join();
    }
    if (server_) {
        server_->stop();
        server_.reset();
    }
    const std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.clear();
}

std::size_t WebSocketServer::clientCount() {
    const std::lock_guard<std::mutex> lock(clientsMutex_);
    return clients_.size();
}

void WebSocketServer::onOpen(const std::string& clientId, ix::WebSocket& socket,
                             const std::optional<std::string>& origin) {
    // IXWebSocket has no hook before the 101 response, so a foreign origin is closed right
    // after the handshake. It never gets registered: no hello, no state, commands ignored.
    if (!origin_policy::isAllowed(origin)) {
        socket.close(kPolicyViolationCloseCode, "origin not allowed");
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_[clientId] = &socket;
    }
    socket.sendText(codec_.encodeHello());
}

bool WebSocketServer::isRegistered(const std::string& clientId) {
    const std::lock_guard<std::mutex> lock(clientsMutex_);
    return clients_.contains(clientId);
}

void WebSocketServer::onClose(const std::string& clientId) {
    const std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.erase(clientId);
}

void WebSocketServer::onText(const std::string& clientId, ix::WebSocket& socket,
                             const std::string& text) {
    if (!isRegistered(clientId)) {
        return; // rejected origin; frames may still arrive until the close completes
    }
    DecodeResult decoded = codec_.decode(text, clientId);
    if (!decoded.command) {
        socket.sendText(codec_.encodeError(decoded.requestId, decoded.error));
        return;
    }
    if (!bus_.submit(std::move(*decoded.command))) {
        socket.sendText(codec_.encodeError(decoded.requestId, "simulation busy, command dropped"));
    }
}

void WebSocketServer::publishLoop() {
    auto nextTick = std::chrono::steady_clock::now();
    StateSnapshot state;
    while (running_) {
        while (const std::optional<OutgoingMessage> message = bus_.nextOutgoing()) {
            const std::string text = codec_.encode(*message);
            if (message->clientId.empty()) {
                broadcast(text);
            } else {
                sendTo(message->clientId, text);
            }
        }
        if (bus_.latestState(state)) {
            broadcast(codec_.encodeState(state));
        }
        nextTick += kPublishInterval;
        std::this_thread::sleep_until(nextTick);
    }
}

void WebSocketServer::sendTo(const std::string& clientId, const std::string& text) {
    const std::lock_guard<std::mutex> lock(clientsMutex_);
    if (const auto client = clients_.find(clientId); client != clients_.end()) {
        client->second->sendText(text);
    }
}

void WebSocketServer::broadcast(const std::string& text) {
    const std::lock_guard<std::mutex> lock(clientsMutex_);
    for (const auto& [clientId, socket] : clients_) {
        socket->sendText(text);
    }
}

} // namespace simcore
