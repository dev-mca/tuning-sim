// JSON protocol v1 (docs/interface.md): decoding, encoding, unit conversion at the edge.

#include "adapters/protocol.hpp"
#include "domain/common/units.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <limits>
#include <variant>

using namespace simcore;
using nlohmann::json;

namespace {

const ProtocolCodec& codec() {
    static const ProtocolCodec instance(test::referenceEngine());
    return instance;
}

template <typename T>
T decodeAs(const std::string& text) {
    const DecodeResult result = codec().decode(text, "client-1");
    REQUIRE_MESSAGE(result.command.has_value(), result.error);
    REQUIRE(std::holds_alternative<T>(result.command->command));
    return std::get<T>(result.command->command);
}

} // namespace

TEST_CASE("setPedal is decoded with client and request id") {
    const DecodeResult result =
        codec().decode(R"({"v":1,"type":"setPedal","id":"42","payload":{"value":0.5}})", "c7");
    REQUIRE(result.command.has_value());
    CHECK(result.command->clientId == "c7");
    CHECK(result.command->requestId == "42");
    CHECK(std::get<SetPedal>(result.command->command).value == doctest::Approx(0.5));
}

TEST_CASE("startSweep fills defaults for rate and pedal") {
    const auto sweep = decodeAs<StartSweep>(
        R"({"v":1,"type":"startSweep","payload":{"rpmFrom":1000,"rpmTo":7000}})");
    CHECK(sweep.parameters.speedFrom_rpm == 1000.0);
    CHECK(sweep.parameters.speedTo_rpm == 7000.0);
    CHECK(sweep.parameters.rate_rpm_s == doctest::Approx(300.0));
    CHECK(sweep.parameters.throttle == doctest::Approx(1.0));
}

TEST_CASE("setTableCells converts display units to SI") {
    const auto edit = decodeAs<SetTableCells>(
        R"({"v":1,"type":"setTableCells","payload":{"id":"ve","cells":[{"x":3,"y":4,"value":85.0}]}})");
    REQUIRE(edit.cells.size() == 1);
    CHECK(edit.cells[0].x == 3);
    CHECK(edit.cells[0].y == 4);
    CHECK(edit.cells[0].value == doctest::Approx(0.85)); // % -> fraction
}

TEST_CASE("invalid messages produce an error instead of a command") {
    for (const char* text :
         {"not json", R"({"v":2,"type":"stopSweep"})", R"({"v":1,"type":"teleport"})",
          R"({"v":1,"type":"setPedal","payload":{}})",
          R"({"v":1,"type":"setTableCells","payload":{"id":"nope","cells":[]}})"}) {
        CAPTURE(text);
        const DecodeResult result = codec().decode(text, "c");
        CHECK_FALSE(result.command.has_value());
        CHECK_FALSE(result.error.empty());
    }
}

TEST_CASE("state is encoded in protocol units, infinite lambda as null") {
    StateSnapshot state;
    state.speed_rad_s = 314.159265358979; // 3000 1/min
    state.manifoldPressure_Pa = 95'000.0;
    state.power_W = 90'000.0;
    state.exhaustTemperature_K = 1073.15;
    state.lambdaActual = std::numeric_limits<double>::infinity();
    const json message = json::parse(codec().encodeState(state));
    CHECK(message["v"] == 1);
    CHECK(message["type"] == "state");
    const json& payload = message["payload"];
    CHECK(payload["rpm"].get<double>() == doctest::Approx(3000.0));
    CHECK(payload["map_kPa"].get<double>() == doctest::Approx(95.0));
    CHECK(payload["power_kW"].get<double>() == doctest::Approx(90.0));
    CHECK(payload["egt_C"].get<double>() == doctest::Approx(800.0));
    CHECK(payload["lambda"].is_null());
    CHECK(payload["dynoMode"] == "free");
}

TEST_CASE("a table round-trips in display units") {
    const Table2D& ve = test::referenceEngine().definition.tables.ve;
    const json message = json::parse(codec().encode({"c", "7", TableData{"ve", ve}}));
    CHECK(message["type"] == "table");
    CHECK(message["id"] == "7");
    const json& payload = message["payload"];
    CHECK(payload["unit"] == "%");
    CHECK(payload["editable"] == true);
    CHECK(payload["xAxis"]["bins"][0].get<double>() == doctest::Approx(800.0));
    CHECK(payload["yAxis"]["bins"][15].get<double>() == doctest::Approx(105.0));
    CHECK(payload["values"].size() == 16);
    CHECK(payload["values"][0].size() == 16);
    CHECK(payload["values"][0][0].get<double>() == doctest::Approx(ve.cell(0, 0) * 100.0));
}

TEST_CASE("hello describes the engine and its tables") {
    const json message = json::parse(codec().encodeHello());
    CHECK(message["type"] == "hello");
    const json& payload = message["payload"];
    CHECK(payload["protocolVersion"] == 1);
    CHECK(payload["engine"]["id"] == "i6_30_na");
    CHECK(payload["engine"]["revLimit_rpm"].get<double>() == doctest::Approx(7000.0));
    CHECK(payload["tables"].size() == 4);
}

TEST_CASE("setAudio is decoded with clamped volume; mute defaults to false") {
    const SetAudio audio = decodeAs<SetAudio>(
        R"({"v":1,"type":"setAudio","id":"5","payload":{"volume":1.7,"mute":true}})");
    CHECK(audio.volume == 1.0);
    CHECK(audio.muted);
    CHECK_FALSE(decodeAs<SetAudio>(R"({"v":1,"type":"setAudio","payload":{"volume":0.3}})").muted);
}

TEST_CASE("hello announces whether audio is available") {
    const json withoutAudio = json::parse(codec().encodeHello());
    CHECK(withoutAudio["payload"]["audio"]["available"] == false);
    const ProtocolCodec withAudio(test::referenceEngine(), AudioInfo{true, "Speakers (PipeWire)"});
    const json hello = json::parse(withAudio.encodeHello());
    CHECK(hello["payload"]["audio"]["available"] == true);
    CHECK(hello["payload"]["audio"]["device"] == "Speakers (PipeWire)");
}

TEST_CASE("overrun settings are decoded with enrichment in percent and encoded back") {
    const SetOverrunSettings set = decodeAs<SetOverrunSettings>(
        R"({"v":1,"type":"setOverrunSettings","payload":{"fuelCut":false,"retard_deg":35,"enrichment_pct":10}})");
    CHECK_FALSE(set.settings.fuelCut);
    CHECK(set.settings.retard_deg == 35.0);
    CHECK(set.settings.enrichment == doctest::Approx(0.1));
    static_cast<void>(decodeAs<GetOverrunSettings>(R"({"v":1,"type":"getOverrunSettings"})"));

    const json encoded =
        json::parse(codec().encode({"c", "9", OverrunSettingsData{{false, 35.0, 0.1}}}));
    CHECK(encoded["type"] == "overrunSettings");
    CHECK(encoded["id"] == "9");
    CHECK(encoded["payload"]["fuelCut"] == false);
    CHECK(encoded["payload"]["retard_deg"].get<double>() == doctest::Approx(35.0));
    CHECK(encoded["payload"]["enrichment_pct"].get<double>() == doctest::Approx(10.0));
}

TEST_CASE("state carries catalyst temperature and overrun fuelling") {
    StateSnapshot state;
    state.catalystTemperature_K = 873.15;
    state.overrunFueling = true;
    const json payload = json::parse(codec().encodeState(state))["payload"];
    CHECK(payload["catalyst_C"].get<double>() == doctest::Approx(600.0));
    CHECK(payload["overrunFueling"] == true);
}

TEST_CASE("launch settings use 1/min on the wire and are encoded back") {
    const SetLaunchSettings set = decodeAs<SetLaunchSettings>(
        R"({"v":1,"type":"setLaunchSettings","payload":{"enabled":true,"launch_rpm":4000,"retard_deg":10}})");
    CHECK(set.settings.enabled);
    CHECK(set.settings.launchSpeed_rad_s == doctest::Approx(units::rpmToRadS(4000.0)));
    CHECK(set.settings.retard_deg == 10.0);
    static_cast<void>(decodeAs<GetLaunchSettings>(R"({"v":1,"type":"getLaunchSettings"})"));

    const json encoded = json::parse(
        codec().encode({"c", "4", LaunchSettingsData{{true, units::rpmToRadS(4500.0), 12.0}}}));
    CHECK(encoded["type"] == "launchSettings");
    CHECK(encoded["id"] == "4");
    CHECK(encoded["payload"]["enabled"] == true);
    CHECK(encoded["payload"]["launch_rpm"].get<double>() == doctest::Approx(4500.0));
    CHECK(encoded["payload"]["retard_deg"].get<double>() == doctest::Approx(12.0));
}

TEST_CASE("the launch switch is decoded and the state reports launch control") {
    CHECK(decodeAs<SetLaunchSwitch>(R"({"v":1,"type":"setLaunchSwitch","payload":{"held":true}})")
              .held);
    StateSnapshot state;
    state.launchSwitch = true;
    state.launchActive = true;
    state.sparkCut = true;
    const json payload = json::parse(codec().encodeState(state))["payload"];
    CHECK(payload["launchSwitch"] == true);
    CHECK(payload["launchActive"] == true);
    CHECK(payload["sparkCut"] == true);
}
