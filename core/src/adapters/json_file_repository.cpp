#include "adapters/json_file_repository.hpp"

#include "adapters/table_units.hpp"
#include "domain/common/units.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace simcore {

namespace {

using nlohmann::json;

json readJson(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open " + path.string());
    }
    try {
        return json::parse(file);
    } catch (const json::exception& error) {
        throw std::runtime_error("invalid JSON in " + path.string() + ": " + error.what());
    }
}

std::vector<double> convertAll(const json& values, const std::string& unit) {
    std::vector<double> result;
    result.reserve(values.size());
    for (const json& value : values) {
        result.push_back(table_units::toSi(unit, value.get<double>()));
    }
    return result;
}

struct LoadedTable {
    Table2D table;
    TableDescriptor descriptor;
};

LoadedTable loadTable(const std::filesystem::path& path, bool editable) {
    const json data = readJson(path);
    const std::string unit = data.at("unit").get<std::string>();
    const json& xAxis = data.at("xAxis");
    const json& yAxis = data.at("yAxis");
    std::vector<double> values;
    for (const json& row : data.at("values")) { // values[y][x], row by row
        const std::vector<double> converted = convertAll(row, unit);
        values.insert(values.end(), converted.begin(), converted.end());
    }
    Table2D table(convertAll(xAxis.at("bins"), xAxis.at("unit").get<std::string>()),
                  convertAll(yAxis.at("bins"), yAxis.at("unit").get<std::string>()),
                  std::move(values));
    TableDescriptor descriptor{data.at("id").get<std::string>(),
                               data.value("name", ""),
                               unit,
                               {xAxis.value("name", "x"), xAxis.at("unit").get<std::string>()},
                               {yAxis.value("name", "y"), yAxis.at("unit").get<std::string>()},
                               data.value("min", 0.0),
                               data.value("max", 0.0),
                               editable};
    return {std::move(table), std::move(descriptor)};
}

double rpm(const json& data, const char* key) {
    return units::rpmToRadS(data.at(key).get<double>());
}

} // namespace

JsonFileRepository::JsonFileRepository(std::filesystem::path sharedDir)
    : sharedDir_(std::move(sharedDir)) {}

LoadedEngine JsonFileRepository::load(const std::string& engineId) const {
    const std::filesystem::path path = sharedDir_ / "engines" / (engineId + ".json");
    const json data = readJson(path);
    try {
        const json& intake = data.at("intake");
        const json& combustion = data.at("combustion");
        const json& friction = data.at("friction");
        const json& tables = data.at("tables");

        // ECU tune tables are editable; ignitionMbt is a property of the engine model.
        constexpr std::array kTableKeys = {"ve", "lambdaTarget", "ignition", "ignitionMbt"};
        std::vector<LoadedTable> loaded;
        for (const char* key : kTableKeys) {
            const bool editable = std::string(key) != "ignitionMbt";
            loaded.push_back(loadTable(sharedDir_ / tables.at(key).get<std::string>(), editable));
        }

        EngineDefinition definition{
            data.at("id").get<std::string>(),
            data.at("name").get<std::string>(),
            data.at("cylinders").get<int>(),
            data.at("bore_m").get<double>(),
            data.at("stroke_m").get<double>(),
            data.at("compressionRatio").get<double>(),
            data.at("firingOrder").get<std::vector<int>>(),
            data.at("firingAngles_deg").get<std::vector<double>>(),
            rpm(data, "idle_rpm"),
            rpm(data, "revLimit_rpm"),
            rpm(data, "revLimitHysteresis_rpm"),
            data.at("inertia_kg_m2").get<double>(),
            {intake.at("manifoldVolume_m3").get<double>(),
             intake.at("throttleDiameter_m").get<double>(),
             intake.at("throttleClosedAngle_deg").get<double>(),
             intake.at("throttleDischargeCoefficient").get<double>(),
             intake.at("throttleLeakArea_m2").get<double>()},
            {combustion.at("indicatedEfficiencyBase").get<double>(),
             combustion.at("lambdaOptimum").get<double>(),
             combustion.at("lambdaCurvature").get<double>(),
             combustion.at("ignitionCurvature_per_deg2").get<double>(),
             combustion.at("coolingFraction").get<double>()},
            {units::kpaToPa(friction.at("fmep_c0_kPa").get<double>()),
             units::kpaToPa(friction.at("fmep_c1_kPa_per_krpm").get<double>()),
             units::kpaToPa(friction.at("fmep_c2_kPa_per_krpm2").get<double>())},
            data.at("accessoryTorque_Nm").get<double>(),
            data.at("exhaust").at("backpressureCoefficient_Pa_s2_per_kg2").get<double>(),
            {loaded[0].table, loaded[1].table, loaded[2].table, loaded[3].table}};

        const auto cylinders = static_cast<std::size_t>(definition.cylinders);
        if (definition.firingOrder.size() != cylinders ||
            definition.firingAngles_deg.size() != cylinders) {
            throw std::runtime_error(
                "firingOrder and firingAngles_deg need one entry per cylinder");
        }
        std::vector<TableDescriptor> descriptors;
        for (const LoadedTable& table : loaded) {
            descriptors.push_back(table.descriptor);
        }
        return {std::move(definition), std::move(descriptors)};
    } catch (const json::exception& error) {
        throw std::runtime_error("invalid engine definition " + path.string() + ": " +
                                 error.what());
    }
}

} // namespace simcore
