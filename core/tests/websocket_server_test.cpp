// End-to-end: real WebSocket client <-> simcore server, simulation running in real time.
// Acceptance of plan.md iteration 4: state arrives at about 60 Hz, commands have an effect.

#include "adapters/message_bus.hpp"
#include "adapters/protocol.hpp"
#include "adapters/steady_clock.hpp"
#include "adapters/websocket_server.hpp"
#include "app/realtime_runner.hpp"
#include "app/simulation_service.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace simcore;
using nlohmann::json;

namespace {

constexpr int kTestPort = 47891; // not the default, so a running simcore does not interfere
constexpr auto kTimeout = std::chrono::seconds(5);

class TestClient {
public:
    explicit TestClient(int port) {
        socket_.setUrl("ws://127.0.0.1:" + std::to_string(port));
        socket_.disableAutomaticReconnection();
        socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (message->type == ix::WebSocketMessageType::Message) {
                const std::lock_guard<std::mutex> lock(mutex_);
                messages_.push_back(json::parse(message->str));
            }
        });
        socket_.start();
    }
    ~TestClient() { socket_.stop(); }

    void send(const json& message) { socket_.sendText(message.dump()); }

    /// Waits until a message matches; returns it (or null JSON on timeout).
    json waitFor(const std::function<bool(const json&)>& predicate) {
        const auto deadline = std::chrono::steady_clock::now() + kTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                for (const json& message : messages_) {
                    if (predicate(message)) {
                        return message;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return nullptr;
    }

    std::size_t count(const std::string& type) {
        const std::lock_guard<std::mutex> lock(mutex_);
        std::size_t n = 0;
        for (const json& message : messages_) {
            n += message.value("type", "") == type ? 1U : 0U;
        }
        return n;
    }

private:
    ix::WebSocket socket_;
    std::mutex mutex_;
    std::vector<json> messages_;
};

bool hasType(const json& message, const char* type) {
    return message.value("type", "") == type;
}

} // namespace

TEST_CASE("client receives hello and state, commands change the simulation") {
    const LoadedEngine& engine = test::referenceEngine();
    auto bus = std::make_unique<MessageBus>();
    const ProtocolCodec codec(engine);
    SimulationService service(engine, *bus, *bus);
    SteadyClock clock;
    RealtimeRunner runner(service, clock);
    WebSocketServer server(*bus, codec, kTestPort);
    REQUIRE(server.start().empty());
    runner.start();

    {
        TestClient client(kTestPort);
        const json hello = client.waitFor([](const json& m) { return hasType(m, "hello"); });
        REQUIRE_FALSE(hello.is_null());
        CHECK(hello["payload"]["engine"]["id"] == "i6_30_na");

        // State arrives at about 60 Hz.
        const std::size_t before = client.count("state");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const std::size_t received = client.count("state") - before;
        CHECK(received >= 20);
        CHECK(received <= 40);

        // setPedal is acknowledged with the request id and shows up in the state.
        client.send({{"v", 1}, {"type", "setPedal"}, {"id", "p1"}, {"payload", {{"value", 0.4}}}});
        CHECK_FALSE(client
                        .waitFor([](const json& m) {
                            return hasType(m, "ack") && m.value("id", "") == "p1";
                        })
                        .is_null());
        CHECK_FALSE(client
                        .waitFor([](const json& m) {
                            return hasType(m, "state") &&
                                   m["payload"]["pedal"].get<double>() == 0.4 &&
                                   m["payload"]["rpm"].get<double>() > 2000.0;
                        })
                        .is_null());

        // getTable answers only this client with the table in display units.
        client.send({{"v", 1}, {"type", "getTable"}, {"id", "t1"}, {"payload", {{"id", "ve"}}}});
        const json table = client.waitFor([](const json& m) { return hasType(m, "table"); });
        REQUIRE_FALSE(table.is_null());
        CHECK(table["id"] == "t1");
        CHECK(table["payload"]["values"].size() == 16);

        // setTableCells is applied.
        client.send(
            {{"v", 1},
             {"type", "setTableCells"},
             {"id", "s1"},
             {"payload", {{"id", "ignition"}, {"cells", {{{"x", 0}, {"y", 0}, {"value", 9.5}}}}}}});
        CHECK_FALSE(client
                        .waitFor([](const json& m) {
                            return hasType(m, "ack") && m.value("id", "") == "s1";
                        })
                        .is_null());

        // Garbage gets an error, the server keeps running.
        client.send("{not json");
        CHECK_FALSE(client.waitFor([](const json& m) { return hasType(m, "error"); }).is_null());
    }

    server.stop();
    runner.stop();
    CHECK(service.simulation().ecu().tune().ignition.cell(0, 0) == 9.5);
}
