// Application service with fake ports: no network, no threads.

#include "app/simulation_service.hpp"
#include "domain/common/units.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
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

/// Records what the service hands to the sound.
class FakeSound final : public IAcousticSink {
public:
    void publish(const acoustic::AcousticParams& params) override {
        last = params;
        ++published;
    }
    void setOutput(double newVolume, bool newMuted) override {
        volume = newVolume;
        muted = newMuted;
    }
    acoustic::AcousticParams last;
    int published = 0;
    double volume = -1.0;
    bool muted = false;
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

TEST_CASE("every step hands the acoustic parameters to the sound") {
    FakeBus bus;
    FakeSound sound;
    SimulationService service(test::referenceEngine(), bus, bus, &sound);
    runSteps(service, 50);
    CHECK(sound.published == 50);
    CHECK(sound.last.speed_rad_s == doctest::Approx(bus.lastState.speed_rad_s));
    CHECK(sound.last.chargeRatio > 0.0);
}

TEST_CASE("setAudio reaches the sound and is acknowledged, also without sound") {
    FakeBus bus;
    FakeSound sound;
    SimulationService withSound(test::referenceEngine(), bus, bus, &sound);
    bus.push(SetAudio{0.4, true});
    runSteps(withSound, 1);
    CHECK(sound.volume == doctest::Approx(0.4));
    CHECK(sound.muted);
    CHECK(bus.count<Ack>() == 1);

    FakeBus silentBus;
    SimulationService withoutSound(test::referenceEngine(), silentBus, silentBus);
    silentBus.push(SetAudio{0.4, false});
    runSteps(withoutSound, 1);
    CHECK(silentBus.count<Ack>() == 1);
}

TEST_CASE("overrun settings are read, validated, applied and broadcast") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(GetOverrunSettings{}, "get");
    runSteps(service, 1);
    REQUIRE(bus.count<OverrunSettingsData>() == 1);
    CHECK(std::get<OverrunSettingsData>(bus.sent.back().payload).settings.fuelCut);

    bus.push(SetOverrunSettings{{false, 70.0, 0.1}}, "bad");
    runSteps(service, 1);
    CHECK(bus.count<ErrorReply>() == 1);
    CHECK(service.simulation().ecu().tune().overrun.fuelCut); // unchanged

    bus.push(SetOverrunSettings{{false, 35.0, 0.1}}, "good");
    runSteps(service, 1);
    CHECK_FALSE(service.simulation().ecu().tune().overrun.fuelCut);
    CHECK(service.simulation().ecu().tune().overrun.retard_deg == 35.0);
    CHECK(bus.count<Ack>() == 1);
    REQUIRE(bus.count<OverrunSettingsData>() == 2);
    CHECK(bus.sent.back().clientId.empty()); // broadcast to every client
}

TEST_CASE("state reports catalyst temperature and overrun fuelling") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    runSteps(service, 10);
    CHECK(bus.lastState.catalystTemperature_K > 600.0);
    CHECK_FALSE(bus.lastState.overrunFueling);
}

TEST_CASE("launch settings are read, validated, applied and broadcast") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(GetLaunchSettings{}, "get");
    runSteps(service, 1);
    REQUIRE(bus.count<LaunchSettingsData>() == 1);
    CHECK_FALSE(std::get<LaunchSettingsData>(bus.sent.back().payload).settings.enabled);

    bus.push(SetLaunchSettings{{true, units::rpmToRadS(9000.0), 10.0}}, "bad");
    runSteps(service, 1);
    CHECK(bus.count<ErrorReply>() == 1);
    CHECK_FALSE(service.simulation().ecu().tune().launch.enabled); // unchanged

    bus.push(SetLaunchSettings{{true, units::rpmToRadS(4000.0), 10.0}}, "good");
    runSteps(service, 1);
    CHECK(service.simulation().ecu().tune().launch.enabled);
    CHECK(bus.count<Ack>() == 1);
    REQUIRE(bus.count<LaunchSettingsData>() == 2);
    CHECK(bus.sent.back().clientId.empty()); // broadcast to every client
}

TEST_CASE("holding the launch switch at full pedal holds the launch speed") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(SetLaunchSettings{{true, units::rpmToRadS(4000.0), 10.0}}, "launch");
    bus.push(SetLaunchSwitch{true}, "held");
    bus.push(SetPedal{1.0}, "pedal");
    runSteps(service, 3000);
    CHECK(bus.lastState.launchSwitch);
    CHECK(bus.lastState.launchActive);
    CHECK(units::radSToRpm(bus.lastState.speed_rad_s) == doctest::Approx(4000.0).epsilon(0.05));

    bus.push(SetLaunchSwitch{false}, "released");
    runSteps(service, 2000);
    CHECK_FALSE(bus.lastState.launchSwitch);
    CHECK_FALSE(bus.lastState.launchActive);
    CHECK(units::radSToRpm(bus.lastState.speed_rad_s) > 6000.0);
}

TEST_CASE("shift and clutch commands drive the car on the roller dyno") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(Shift{9}, "bad");
    runSteps(service, 1);
    CHECK(bus.count<ErrorReply>() == 1);

    bus.push(SetClutch{true}, "clutch");
    bus.push(Shift{1}, "first");
    runSteps(service, 300);
    CHECK(bus.lastState.gear == 1);
    CHECK(bus.lastState.clutchEngagement == 0.0);
    bus.push(SetPedal{0.6}, "pedal");
    bus.push(SetClutch{false}, "go");
    runSteps(service, 3000);
    CHECK(bus.lastState.clutchLocked);
    REQUIRE(bus.lastState.vehicleSpeed_m_s.has_value());
    CHECK(units::msToKmh(*bus.lastState.vehicleSpeed_m_s) > 20.0);
    REQUIRE(bus.lastState.wheelTorque_Nm.has_value());
    CHECK(*bus.lastState.wheelTorque_Nm > 0.0);
}

TEST_CASE("a roller run reports progress and a roller result, and refuses shifting") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(StartRollerRun{{3, 3000.0, 3600.0}}, "roll");
    runSteps(service, 1);
    CHECK(bus.count<Ack>() == 1);
    CHECK(bus.lastState.dynoMode == DynoMode::Roller);
    bus.push(Shift{4}, "shift");
    runSteps(service, 1);
    CHECK(bus.count<ErrorReply>() == 1);
    runSteps(service, 5000);
    REQUIRE(bus.count<RollerResult>() == 1);
    CHECK(bus.count<DynoProgress>() >= 1);
    CHECK(bus.lastState.dynoMode == DynoMode::Free);
    const auto& result =
        std::get<RollerResult>(std::find_if(bus.sent.begin(), bus.sent.end(), [](const auto& m) {
                                   return std::holds_alternative<RollerResult>(m.payload);
                               })->payload);
    CHECK(result.gear == 3);
    CHECK(result.samples.size() == 7); // 3000..3600 per 100 1/min
}

TEST_CASE("an engine dyno sweep takes the car out of gear") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(Shift{1}, "first");
    runSteps(service, 500);
    CHECK(service.car().drivetrain().gear() == 1);
    bus.push(StartSweep{{2000.0, 2500.0, 1000.0, 1.0, 100.0}}, "sweep");
    runSteps(service, 1);
    CHECK(service.car().drivetrain().gear() == 0);
}

TEST_CASE("the brake pedal is clamped, applied and reported") {
    FakeBus bus;
    SimulationService service(test::referenceEngine(), bus, bus);
    bus.push(SetBrake{0.5}, "brake");
    runSteps(service, 1);
    CHECK(bus.lastState.brake == 0.5);
    CHECK(service.car().drivetrain().state().brake == 0.5);
}
