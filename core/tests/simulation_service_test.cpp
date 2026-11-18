// Application service with fake ports: no network, no threads.

#include "app/simulation_service.hpp"
#include "domain/common/units.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <deque>
#include <variant>
#include <vector>

using namespace simcore;

namespace {

/// Test double for both ports: commands are queued by the test, output is recorded.
class FakeBus final : public ICommandSource, public IStatePublisher {
public:
    void push(Command command, std::string requestId = "r") {
        commands.push_back({"client", std::move(requestId), std::move(command)});
    }
    std::optional<IncomingCommand> poll() override {
        if (commands.empty()) {
            return std::nullopt;
        }
        IncomingCommand next = commands.front();
        commands.pop_front();
        return next;
    }
    void publishState(const StateSnapshot& state) override { lastState = state; }
    void send(OutgoingMessage message) override { sent.push_back(std::move(message)); }

    template <typename T>
    [[nodiscard]] std::size_t count() const {
        std::size_t n = 0;
        for (const OutgoingMessage& message : sent) {
            n += std::holds_alternative<T>(message.payload) ? 1U : 0U;
        }
        return n;
    }

    std::deque<IncomingCommand> commands;
    std::vector<OutgoingMessage> sent;
    StateSnapshot lastState;
};

void runSteps(SimulationService& service, int steps) {
    for (int i = 0; i < steps; ++i) {
        service.step();
    }
}

} // namespace

TEST_CASE("setPedal opens the throttle and the engine revs up") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    runSteps(service, 500);
    const double idle_rpm = units::radSToRpm(bus.lastState.speed_rad_s);
    bus.push(SetPedal{0.3});
    runSteps(service, 1000);
    CHECK(bus.count<Ack>() == 1);
    CHECK(bus.sent.front().requestId == "r");
    CHECK(bus.lastState.pedal == doctest::Approx(0.3));
    CHECK(units::radSToRpm(bus.lastState.speed_rad_s) > idle_rpm + 500.0);
}

TEST_CASE("getTable returns the current table, unknown tables give an error") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(GetTable{"ignition"});
    bus.push(GetTable{"turbo"});
    service.step();
    REQUIRE(bus.sent.size() == 2);
    REQUIRE(std::holds_alternative<TableData>(bus.sent[0].payload));
    CHECK(std::get<TableData>(bus.sent[0].payload).table.values() ==
          test::referenceEngine().definition.tables.ignition.values());
    CHECK(std::holds_alternative<ErrorReply>(bus.sent[1].payload));
}

TEST_CASE("setTableCells changes the ECU tune; read-only and invalid edits are rejected") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(SetTableCells{"ignition", {{0, 0, 12.5}}});
    bus.push(SetTableCells{"ignitionMbt", {{0, 0, 1.0}}});            // model property
    bus.push(SetTableCells{"ignition", {{1, 1, 1.0}, {99, 0, 1.0}}}); // second cell invalid
    service.step();
    REQUIRE(bus.sent.size() == 3);
    CHECK(std::holds_alternative<Ack>(bus.sent[0].payload));
    CHECK(std::holds_alternative<ErrorReply>(bus.sent[1].payload));
    CHECK(std::holds_alternative<ErrorReply>(bus.sent[2].payload));
    const Tune& tune = service.simulation().ecu().tune();
    CHECK(tune.ignition.cell(0, 0) == 12.5);
    // All-or-nothing: the valid first cell of the rejected edit was not applied.
    CHECK(tune.ignition.cell(1, 1) ==
          test::referenceEngine().definition.tables.ignition.cell(1, 1));
}

TEST_CASE("editing the VE table makes the measured lambda deviate (research.md §2.4)") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    SetTableCells leaner{"ve", {}};
    const Table2D& ve = test::referenceEngine().definition.tables.ve;
    for (std::size_t y = 0; y < ve.yBins().size(); ++y) {
        for (std::size_t x = 0; x < ve.xBins().size(); ++x) {
            leaner.cells.push_back({x, y, ve.cell(x, y) * 0.9});
        }
    }
    bus.push(leaner);
    runSteps(service, 2000);
    CHECK(bus.lastState.lambdaActual / bus.lastState.lambdaTarget ==
          doctest::Approx(1.0 / 0.9).epsilon(0.01));
}

TEST_CASE("a sweep reports progress and a result, then returns to free mode") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    SweepParameters parameters{2000.0, 3000.0, 1000.0, 1.0, 250.0, 0.5};
    bus.push(StartSweep{parameters});
    runSteps(service, 200);
    CHECK(bus.lastState.dynoMode == DynoMode::Sweep);
    runSteps(service, 1400); // 0.5 s settle + 1 s ramp
    CHECK(bus.count<DynoProgress>() >= 10);
    REQUIRE(bus.count<DynoResult>() == 1);
    for (const OutgoingMessage& message : bus.sent) {
        if (const auto* result = std::get_if<DynoResult>(&message.payload)) {
            CHECK(result->samples.size() == 5); // 2000, 2250, ..., 3000
            CHECK(message.clientId.empty());    // broadcast
        }
    }
    service.step();
    CHECK(bus.lastState.dynoMode == DynoMode::Free);
}

TEST_CASE("an invalid sweep is answered with an error") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(StartSweep{{5000.0, 1000.0}});
    service.step();
    CHECK(bus.count<ErrorReply>() == 1);
    CHECK(bus.lastState.dynoMode == DynoMode::Free);
}
