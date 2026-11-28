#include "adapters/protocol.hpp"

#include "adapters/table_units.hpp"
#include "domain/common/units.hpp"
#include "simcore/version.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace simcore {

namespace {

constexpr double kPercent = 100.0; // enrichment: fraction inside, percent in the protocol

using nlohmann::json;

constexpr double kLitresPerCubicMetre = 1.0e3;

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

/// JSON has no infinity: a non-finite value (lambda during fuel cut) becomes null.
json number(double value) {
    return std::isfinite(value) ? json(value) : json(nullptr);
}

/// Not available (e.g. no turbo, no gearbox) becomes null; otherwise converted for display.
template <typename Convert>
json optionalNumber(const std::optional<double>& value, Convert convert) {
    return value.has_value() ? number(convert(*value)) : json(nullptr);
}

double unchanged(double value) {
    return value;
}

double fractionToPercent(double fraction) {
    return fraction * kPercent;
}

std::string envelope(const char* type, const std::string& requestId, json payload) {
    json message = {{"v", kProtocolVersion}, {"type", type}, {"payload", std::move(payload)}};
    if (!requestId.empty()) {
        message["id"] = requestId;
    }
    return message.dump();
}

const char* dynoModeName(DynoMode mode) {
    switch (mode) {
    case DynoMode::Sweep:
        return "sweep";
    case DynoMode::Roller:
        return "roller";
    case DynoMode::Free:
        break;
    }
    return "free";
}

json sweepResultPayload(const DynoResult& result) {
    json payload = {{"kind", "engine"},          {"gear", nullptr},
                    {"rpm", json::array()},      {"torque_Nm", json::array()},
                    {"power_kW", json::array()}, {"lambda", json::array()},
                    {"map_kPa", json::array()},  {"ignition_deg", json::array()},
                    {"egt_C", json::array()},    {"airflow_kg_s", json::array()}};
    for (const SweepSample& sample : result.samples) {
        payload["rpm"].push_back(units::radSToRpm(sample.speed_rad_s));
        payload["torque_Nm"].push_back(sample.measuredTorque_Nm);
        payload["power_kW"].push_back(units::wToKw(sample.power_W()));
        payload["lambda"].push_back(number(sample.lambdaActual));
        payload["map_kPa"].push_back(units::paToKpa(sample.manifoldPressure_Pa));
        payload["ignition_deg"].push_back(sample.ignition_deg);
        payload["egt_C"].push_back(units::kelvinToCelsius(sample.exhaustTemperature_K));
        payload["airflow_kg_s"].push_back(sample.airMassFlow_kg_s);
    }
    return payload;
}

/// Roller run: power at the wheels; torque as engine-equivalent torque = wheel torque / i.
json rollerResultPayload(const RollerResult& result) {
    json payload = {{"kind", "roller"},          {"gear", result.gear},
                    {"rpm", json::array()},      {"torque_Nm", json::array()},
                    {"power_kW", json::array()}, {"lambda", json::array()},
                    {"map_kPa", json::array()},  {"ignition_deg", json::array()},
                    {"egt_C", json::array()},    {"airflow_kg_s", json::array()}};
    for (const RollerSample& sample : result.samples) {
        payload["rpm"].push_back(units::radSToRpm(sample.speed_rad_s));
        payload["torque_Nm"].push_back(sample.wheelTorque_Nm / result.overallRatio);
        payload["power_kW"].push_back(units::wToKw(sample.wheelPower_W()));
        payload["lambda"].push_back(number(sample.lambdaActual));
        payload["map_kPa"].push_back(units::paToKpa(sample.manifoldPressure_Pa));
        payload["ignition_deg"].push_back(sample.ignition_deg);
        payload["egt_C"].push_back(units::kelvinToCelsius(sample.exhaustTemperature_K));
        payload["airflow_kg_s"].push_back(sample.airMassFlow_kg_s);
    }
    return payload;
}

Command decodeCommand(const std::string& type, const json& payload,
                      const std::function<const TableDescriptor*(const std::string&)>& find) {
    if (type == "setPedal") {
        return SetPedal{payload.at("value").get<double>()};
    }
    if (type == "startSweep") {
        SweepParameters parameters{payload.at("rpmFrom").get<double>(),
                                   payload.at("rpmTo").get<double>()};
        parameters.rate_rpm_s = payload.value("rate_rpm_s", kDefaultSweepRate_rpm_s);
        parameters.throttle = std::clamp(payload.value("pedal", 1.0), 0.0, 1.0);
        return StartSweep{parameters};
    }
    if (type == "setAudio") {
        return SetAudio{std::clamp(payload.at("volume").get<double>(), 0.0, 1.0),
                        payload.value("mute", false)};
    }
    if (type == "getOverrunSettings") {
        return GetOverrunSettings{};
    }
    if (type == "setOverrunSettings") {
        return SetOverrunSettings{{payload.at("fuelCut").get<bool>(),
                                   payload.at("retard_deg").get<double>(),
                                   payload.at("enrichment_pct").get<double>() / kPercent}};
    }
    if (type == "getLaunchSettings") {
        return GetLaunchSettings{};
    }
    if (type == "setLaunchSettings") {
        return SetLaunchSettings{{payload.at("enabled").get<bool>(),
                                  units::rpmToRadS(payload.at("launch_rpm").get<double>()),
                                  payload.at("retard_deg").get<double>(),
                                  payload.value("flatShift", false)}};
    }
    if (type == "setLaunchSwitch") {
        return SetLaunchSwitch{payload.at("held").get<bool>()};
    }
    if (type == "shift") {
        return Shift{payload.at("gear").get<int>()};
    }
    if (type == "setBrake") {
        return SetBrake{std::clamp(payload.at("value").get<double>(), 0.0, 1.0)};
    }
    if (type == "setClutch") {
        return SetClutch{payload.at("pressed").get<bool>()};
    }
    if (type == "startRollerRun") {
        RollerParameters parameters;
        parameters.gear = payload.at("gear").get<int>();
        parameters.speedFrom_rpm = payload.value("rpmFrom", parameters.speedFrom_rpm);
        parameters.speedTo_rpm = payload.value("rpmTo", parameters.speedTo_rpm);
        return StartRollerRun{parameters};
    }
    if (type == "stopSweep") {
        return StopSweep{};
    }
    if (type == "getTable") {
        return GetTable{payload.at("id").get<std::string>()};
    }
    if (type == "setTableCells") {
        const std::string tableId = payload.at("id").get<std::string>();
        const TableDescriptor* table = find(tableId);
        if (table == nullptr) {
            throw std::invalid_argument("unknown table '" + tableId + "'");
        }
        SetTableCells edit{tableId, {}};
        for (const json& cell : payload.at("cells")) {
            edit.cells.push_back({cell.at("x").get<std::size_t>(), cell.at("y").get<std::size_t>(),
                                  table_units::toSi(table->unit, cell.at("value").get<double>())});
        }
        return edit;
    }
    throw std::invalid_argument("unknown message type '" + type + "'");
}

} // namespace

ProtocolCodec::ProtocolCodec(const LoadedEngine& engine, AudioInfo audio)
    : engineId_(engine.definition.id), engineName_(engine.definition.name),
      cylinders_(engine.definition.cylinders),
      displacement_m3_(engine.definition.displacement_m3()),
      idleSpeed_rad_s_(engine.definition.idleSpeed_rad_s),
      revLimit_rad_s_(engine.definition.revLimit_rad_s), tables_(engine.tables),
      audio_(std::move(audio)) {}

const TableDescriptor* ProtocolCodec::descriptor(const std::string& tableId) const {
    const auto found = std::find_if(tables_.begin(), tables_.end(),
                                    [&](const TableDescriptor& d) { return d.id == tableId; });
    return found == tables_.end() ? nullptr : &*found;
}

DecodeResult ProtocolCodec::decode(std::string_view text, const std::string& clientId) const {
    DecodeResult result;
    try {
        const json message = json::parse(text);
        result.requestId = message.value("id", "");
        if (message.value("v", 0) != kProtocolVersion) {
            result.error = "unsupported protocol version, expected v=1";
            return result;
        }
        const json payload = message.value("payload", json::object());
        const auto find = [this](const std::string& id) { return descriptor(id); };
        result.command =
            IncomingCommand{clientId, result.requestId,
                            decodeCommand(message.at("type").get<std::string>(), payload, find)};
    } catch (const std::exception& error) { // invalid JSON, missing field, wrong type
        result.command.reset();
        result.error = error.what();
    }
    return result;
}

std::string ProtocolCodec::encodeHello() const {
    json tables = json::array();
    for (const TableDescriptor& table : tables_) {
        tables.push_back({{"id", table.id},
                          {"name", table.name},
                          {"unit", table.unit},
                          {"editable", table.editable}});
    }
    return envelope("hello", "",
                    {{"protocolVersion", kProtocolVersion},
                     {"simcore", std::string(version())},
                     {"stateRate_Hz", kStateRate_Hz},
                     {"engine",
                      {{"id", engineId_},
                       {"name", engineName_},
                       {"cylinders", cylinders_},
                       {"displacement_l", displacement_m3_ * kLitresPerCubicMetre},
                       {"idle_rpm", units::radSToRpm(idleSpeed_rad_s_)},
                       {"revLimit_rpm", units::radSToRpm(revLimit_rad_s_)}}},
                     {"tables", tables},
                     {"audio", {{"available", audio_.available}, {"device", audio_.device}}}});
}

std::string ProtocolCodec::encodeState(const StateSnapshot& state) const {
    return envelope("state", "",
                    {{"t_s", state.time_s},
                     {"rpm", units::radSToRpm(state.speed_rad_s)},
                     {"pedal", state.pedal},
                     {"map_kPa", units::paToKpa(state.manifoldPressure_Pa)},
                     {"lambda", number(state.lambdaActual)},
                     {"lambdaTarget", state.lambdaTarget},
                     {"ignition_deg", state.ignition_deg},
                     {"torque_Nm", state.brakeTorque_Nm},
                     {"power_kW", units::wToKw(state.power_W)},
                     {"airflow_kg_s", state.airMassFlow_kg_s},
                     {"fuelflow_kg_s", state.fuelMassFlow_kg_s},
                     {"egt_C", units::kelvinToCelsius(state.exhaustTemperature_K)},
                     {"dynoMode", dynoModeName(state.dynoMode)},
                     {"sweepProgress", state.sweepProgress},
                     {"cutActive", state.fuelCut},
                     {"overrunFueling", state.overrunFueling},
                     {"catalyst_C", units::kelvinToCelsius(state.catalystTemperature_K)},
                     {"launchSwitch", state.launchSwitch},
                     {"launchActive", state.launchActive},
                     {"sparkCut", state.sparkCut},
                     {"iat_C", optionalNumber(state.intakeTemperature_K, units::kelvinToCelsius)},
                     {"boost_kPa", optionalNumber(state.boostPressure_Pa, units::paToKpa)},
                     {"turboSpeed_rpm", optionalNumber(state.turboSpeed_rad_s, units::radSToRpm)},
                     {"wastegateDuty_pct", optionalNumber(state.wastegateDuty, fractionToPercent)},
                     {"knockRetard_deg", state.knockRetard_deg},
                     {"gear", state.gear.has_value() ? json(*state.gear) : json(nullptr)},
                     {"clutch", optionalNumber(state.clutchEngagement, unchanged)},
                     {"vehicleSpeed_kmh", optionalNumber(state.vehicleSpeed_m_s, units::msToKmh)},
                     {"wheelTorque_Nm", optionalNumber(state.wheelTorque_Nm, unchanged)},
                     {"clutchLocked", state.clutchLocked},
                     {"shifting", state.shifting},
                     {"brake", state.brake}});
}

std::string ProtocolCodec::encodeError(const std::string& requestId,
                                       const std::string& message) const {
    return envelope("error", requestId, {{"message", message}});
}

std::string ProtocolCodec::encode(const OutgoingMessage& message) const {
    return std::visit(
        Overloaded{
            [&](const Ack&) { return envelope("ack", message.requestId, json::object()); },
            [&](const ErrorReply& error) { return encodeError(message.requestId, error.message); },
            [&](const DynoProgress& progress) {
                return envelope("dynoProgress", message.requestId,
                                {{"progress", progress.progress}, {"rpm", progress.setpoint_rpm}});
            },
            [&](const DynoResult& result) {
                return envelope("dynoResult", message.requestId, sweepResultPayload(result));
            },
            [&](const OverrunSettingsData& data) {
                return envelope("overrunSettings", message.requestId,
                                {{"fuelCut", data.settings.fuelCut},
                                 {"retard_deg", data.settings.retard_deg},
                                 {"enrichment_pct", data.settings.enrichment * kPercent}});
            },
            [&](const LaunchSettingsData& data) {
                return envelope("launchSettings", message.requestId,
                                {{"enabled", data.settings.enabled},
                                 {"launch_rpm", units::radSToRpm(data.settings.launchSpeed_rad_s)},
                                 {"retard_deg", data.settings.retard_deg},
                                 {"flatShift", data.settings.flatShift}});
            },
            [&](const RollerResult& result) {
                return envelope("dynoResult", message.requestId, rollerResultPayload(result));
            },
            [&](const TableData& data) {
                const TableDescriptor* table = descriptor(data.tableId);
                if (table == nullptr) {
                    return encodeError(message.requestId, "unknown table '" + data.tableId + "'");
                }
                const auto convert = [](const std::vector<double>& values,
                                        const std::string& unit) {
                    json converted = json::array();
                    for (const double value : values) {
                        converted.push_back(table_units::fromSi(unit, value));
                    }
                    return converted;
                };
                const std::size_t columns = data.table.xBins().size();
                json rows = json::array();
                for (std::size_t y = 0; y < data.table.yBins().size(); ++y) {
                    json row = json::array();
                    for (std::size_t x = 0; x < columns; ++x) {
                        row.push_back(table_units::fromSi(table->unit, data.table.cell(x, y)));
                    }
                    rows.push_back(std::move(row));
                }
                return envelope("table", message.requestId,
                                {{"id", table->id},
                                 {"name", table->name},
                                 {"unit", table->unit},
                                 {"editable", table->editable},
                                 {"xAxis",
                                  {{"name", table->xAxis.name},
                                   {"unit", table->xAxis.unit},
                                   {"bins", convert(data.table.xBins(), table->xAxis.unit)}}},
                                 {"yAxis",
                                  {{"name", table->yAxis.name},
                                   {"unit", table->yAxis.unit},
                                   {"bins", convert(data.table.yBins(), table->yAxis.unit)}}},
                                 {"min", table->min},
                                 {"max", table->max},
                                 {"values", rows}});
            },
        },
        message.payload);
}

} // namespace simcore
