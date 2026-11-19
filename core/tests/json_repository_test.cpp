// Engine files: exhaust geometry and overrun cut are loaded and validated.

#include "adapters/json_file_repository.hpp"
#include "domain/common/units.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>

using namespace simcore;
using nlohmann::json;

namespace {

/// Copy of shared/ in a temporary folder with a modified engine file. `name` must be unique per
/// test, because ctest runs test cases as parallel processes.
class TemporaryShared {
public:
    TemporaryShared(const std::string& name, const std::function<void(json&)>& modify)
        : root_(std::filesystem::temp_directory_path() / ("simcore-test-" + name)) {
        std::filesystem::remove_all(root_);
        std::filesystem::copy(test::sharedDir(), root_, std::filesystem::copy_options::recursive);
        const std::filesystem::path engineFile = root_ / "engines" / "i6_30_na.json";
        json engine = json::parse(std::ifstream(engineFile));
        modify(engine);
        std::ofstream(engineFile) << engine.dump(2);
    }
    ~TemporaryShared() { std::filesystem::remove_all(root_); }
    TemporaryShared(const TemporaryShared&) = delete;
    TemporaryShared& operator=(const TemporaryShared&) = delete;

    [[nodiscard]] LoadedEngine load() const { return JsonFileRepository(root_).load("i6_30_na"); }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("exhaust geometry and overrun cut of the reference engine are loaded") {
    const EngineDefinition& definition = test::referenceEngine().definition;
    CHECK(definition.exhaustGeometry.primaryLength_m.size() == 6);
    CHECK(definition.exhaustGeometry.collectorGroups.size() == 2);
    CHECK(definition.exhaustGeometry.mergeGroups);
    CHECK(definition.exhaustGeometry.tailpipeDiameter_m == doctest::Approx(0.065));
    CHECK(units::radSToRpm(definition.overrunCut.enter_rad_s) == doctest::Approx(1500.0));
    CHECK(units::radSToRpm(definition.overrunCut.exit_rad_s) == doctest::Approx(1100.0));
}

TEST_CASE("an exhaust without a primary pipe per cylinder is rejected") {
    const TemporaryShared shared("primaries", [](json& engine) {
        engine["exhaust"]["primaryLength_m"] = json::array({0.6, 0.6});
    });
    CHECK_THROWS_WITH_AS(static_cast<void>(shared.load()),
                         doctest::Contains("one entry per cylinder"), std::runtime_error);
}

TEST_CASE("collector groups must name every cylinder exactly once") {
    const TemporaryShared missing("missing-cylinder", [](json& engine) {
        engine["exhaust"]["collectorGroups"] = json::array({json::array({1, 2, 3})});
    });
    CHECK_THROWS_AS(static_cast<void>(missing.load()), std::runtime_error);
    const TemporaryShared unknown("unknown-cylinder", [](json& engine) {
        engine["exhaust"]["collectorGroups"] =
            json::array({json::array({1, 2, 3}), json::array({4, 5, 9})});
    });
    CHECK_THROWS_WITH_AS(static_cast<void>(unknown.load()), doctest::Contains("cylinder 9"),
                         std::runtime_error);
}
