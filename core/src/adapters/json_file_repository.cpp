#include "adapters/json_file_repository.hpp"

#include "adapters/table_units.hpp"
#include "domain/common/units.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

ExhaustGeometry parseExhaustGeometry(const json& exhaust) {
    return {exhaust.at("primaryLength_m").get<std::vector<double>>(),
            exhaust.at("primaryDiameter_m").get<double>(),
            exhaust.at("collectorGroups").get<std::vector<std::vector<int>>>(),
            exhaust.at("collectorLength_m").get<double>(),
            exhaust.at("collectorDiameter_m").get<double>(),
            exhaust.at("mergeGroups").get<bool>(),
            exhaust.at("tailpipeLength_m").get<double>(),
            exhaust.at("tailpipeDiameter_m").get<double>()};
}

AfterburnParameters parseAfterburn(const json& afterburn) {
    return {afterburn.at("lateCombustionStart_deg").get<double>(),
            afterburn.at("lateCombustionSpan_deg").get<double>(),
            afterburn.at("lateCombustionMax").get<double>(),
            afterburn.at("afterburnEfficiency").get<double>(),
            afterburn.at("catalystTimeConstant_s").get<double>(),
            units::celsiusToKelvin(afterburn.at("catalystInitial_C").get<double>())};
}

/// Every cylinder needs a primary pipe and must flow into exactly one collector group.
void validateExhaustGeometry(const ExhaustGeometry& exhaust, int cylinders) {
    if (exhaust.primaryLength_m.size() != static_cast<std::size_t>(cylinders)) {
        throw std::runtime_error("exhaust.primaryLength_m needs one entry per cylinder");
    }
    std::vector<int> seen(static_cast<std::size_t>(cylinders), 0);
    for (const std::vector<int>& group : exhaust.collectorGroups) {
        for (const int cylinder : group) {
            if (cylinder < 1 || cylinder > cylinders) {
                throw std::runtime_error("exhaust.collectorGroups names cylinder " +
                                         std::to_string(cylinder) + ", which does not exist");
            }
            ++seen[static_cast<std::size_t>(cylinder - 1)];
        }
    }
    if (std::any_of(seen.begin(), seen.end(), [](int count) { return count != 1; })) {
        throw std::runtime_error("exhaust.collectorGroups must contain every cylinder once");
    }
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
            {loaded[0].table, loaded[1].table, loaded[2].table, loaded[3].table},
            parseExhaustGeometry(data.at("exhaust")),
            {rpm(data.at("overrunFuelCut"), "enter_rpm"),
             rpm(data.at("overrunFuelCut"), "exit_rpm"),
             data.at("overrunFuelCut").at("closedThrottle").get<double>()},
            parseAfterburn(data.at("afterburn"))};

        const auto cylinders = static_cast<std::size_t>(definition.cylinders);
        if (definition.firingOrder.size() != cylinders ||
            definition.firingAngles_deg.size() != cylinders) {
            throw std::runtime_error(
                "firingOrder and firingAngles_deg need one entry per cylinder");
        }
        validateExhaustGeometry(definition.exhaustGeometry, definition.cylinders);
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
