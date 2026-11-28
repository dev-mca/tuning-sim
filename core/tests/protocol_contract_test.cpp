// Contract test (plan.md §11, option A): the examples in shared/protocol/v1/ are the shared
// contract of docs/interface.md. Every client example must decode, and every message simcore
// encodes must have the same shape (field names, nesting, JSON types) as its example. The UI
// checks the same files in ui/src/protocol/contract.test.ts.

#include "adapters/protocol.hpp"
#include "domain/common/units.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <variant>

using namespace simcore;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {

fs::path examplesDir() {
    return fs::path(test::sharedDir()) / "protocol" / "v1";
}

std::string readFile(const fs::path& path) {
    std::ifstream file(path);
    REQUIRE(file.good());
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

/// Message type -> example text, from the file names of one direction.
std::map<std::string, std::string> examples(const std::string& direction) {
    std::map<std::string, std::string> result;
    for (const fs::directory_entry& entry : fs::directory_iterator(examplesDir() / direction)) {
        if (entry.path().extension() == ".json") {
            result[entry.path().stem().string()] = readFile(entry.path());
        }
    }
    return result;
}

std::string jsonTypeName(const json& value) {
    return value.is_number() ? "number" : value.type_name();
}

/// Empty if `actual` has the shape of `expected`, otherwise where and how it differs.
/// null in the example means "not available" and matches a number or null.
std::optional<std::string> shapeMismatch(const json& expected, const json& actual,
                                         const std::string& path) {
    if (expected.is_null()) {
        return actual.is_null() || actual.is_number()
                   ? std::nullopt
                   : std::optional<std::string>(path + ": expected number or null");
    }
    if (jsonTypeName(expected) != jsonTypeName(actual)) {
        return path + ": expected " + jsonTypeName(expected) + ", got " + jsonTypeName(actual);
    }
    if (expected.is_object()) {
        std::set<std::string> expectedKeys;
        std::set<std::string> actualKeys;
        for (const auto& [key, value] : expected.items()) {
            expectedKeys.insert(key);
        }
        for (const auto& [key, value] : actual.items()) {
            actualKeys.insert(key);
        }
        for (const std::string& key : expectedKeys) {
            if (!actualKeys.contains(key)) {
                return path + "." + key + ": missing";
            }
            if (auto mismatch = shapeMismatch(expected[key], actual[key], path + "." + key)) {
                return mismatch;
            }
        }
        for (const std::string& key : actualKeys) {
            if (!expectedKeys.contains(key)) {
                return path + "." + key + ": not in the example";
            }
        }
    }
    if (expected.is_array() && !expected.empty() && !actual.empty()) {
        return shapeMismatch(expected.front(), actual.front(), path + "[0]");
    }
    return std::nullopt;
}

const ProtocolCodec& codec() {
    static const ProtocolCodec instance(test::referenceEngine());
    return instance;
}

/// One encoded sample per server message type.
std::map<std::string, std::string> encodedServerMessages() {
    const EngineDefinition& engine = test::referenceEngine().definition;
    StateSnapshot state;
    state.lambdaActual = 1.0;
    SweepSample sample{};
    sample.lambdaActual = 0.88;
    return {
        {"hello", codec().encodeHello()},
        {"state", codec().encodeState(state)},
        {"table", codec().encode({"c", "7", TableData{"ve", engine.tables.ve}})},
        {"ack", codec().encode({"c", "7", Ack{}})},
        {"error", codec().encode({"c", "7", ErrorReply{"unknown table 'turbo'"}})},
        {"dynoProgress", codec().encode({"", "", DynoProgress{0.42, 3520.0}})},
        {"dynoResult", codec().encode({"", "", DynoResult{{sample}}})},
        {"overrunSettings", codec().encode({"", "", OverrunSettingsData{{}}})},
        {"launchSettings", codec().encode({"", "", LaunchSettingsData{{}}})},
    };
}

} // namespace

TEST_CASE("shape comparison finds renamed, missing and retyped fields") {
    const json example = json::parse(R"({"a":1,"b":{"c":true},"d":[{"e":"x"}],"f":null})");
    CHECK_FALSE(shapeMismatch(
        example, json::parse(R"({"a":2,"b":{"c":false},"d":[{"e":"y"}],"f":3})"), "$"));
    CHECK(shapeMismatch(example, json::parse(R"({"a":2,"b":{"c":false},"d":[],"g":1})"), "$"));
    CHECK(shapeMismatch(example, json::parse(R"({"a":"2","b":{"c":false},"d":[],"f":null})"), "$"));
    CHECK(shapeMismatch(example, json::parse(R"({"a":2,"b":{"c":1},"d":[],"f":null})"), "$"));
    CHECK(shapeMismatch(example, json::parse(R"({"a":2,"b":{"c":true},"d":[{"z":"y"}],"f":null})"),
                        "$"));
}

TEST_CASE("every client example of the contract decodes into a command") {
    const std::map<std::string, std::string> clientExamples = examples("client");
    CHECK(clientExamples.size() == std::variant_size_v<Command>); // one example per command
    for (const auto& [type, text] : clientExamples) {
        CAPTURE(type);
        const DecodeResult result = codec().decode(text, "client-1");
        CHECK_MESSAGE(result.command.has_value(), result.error);
    }
}

TEST_CASE("every message simcore sends has the shape of its contract example") {
    const std::map<std::string, std::string> serverExamples = examples("server");
    const std::map<std::string, std::string> encoded = encodedServerMessages();
    std::set<std::string> exampleTypes;
    std::set<std::string> encodedTypes;
    for (const auto& [type, text] : serverExamples) {
        exampleTypes.insert(type);
    }
    for (const auto& [type, text] : encoded) {
        encodedTypes.insert(type);
    }
    CHECK(exampleTypes == encodedTypes);
    for (const auto& [type, text] : encoded) {
        CAPTURE(type);
        const auto example = serverExamples.find(type);
        REQUIRE(example != serverExamples.end());
        const json expected = json::parse(example->second);
        const json actual = json::parse(text);
        CHECK(actual["type"] == type);
        const std::optional<std::string> mismatch = shapeMismatch(expected, actual, type);
        CHECK_MESSAGE(!mismatch.has_value(), mismatch.value_or(""));
    }
}
