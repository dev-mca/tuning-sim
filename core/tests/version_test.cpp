#include "simcore/version.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t kSemverPartCount = 3;

bool isAsciiDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

// Checks the "MAJOR.MINOR.PATCH" shape: three non-empty numeric parts separated by dots.
bool isSemanticVersion(std::string_view text) {
    std::size_t partCount = 0;
    std::size_t partStart = 0;
    while (partStart <= text.size()) {
        const std::size_t dot = text.find('.', partStart);
        const std::size_t partEnd = (dot == std::string_view::npos) ? text.size() : dot;
        const std::string_view part = text.substr(partStart, partEnd - partStart);
        if (part.empty() || !std::all_of(part.begin(), part.end(), isAsciiDigit)) {
            return false;
        }
        ++partCount;
        if (dot == std::string_view::npos) {
            break;
        }
        partStart = dot + 1;
    }
    return partCount == kSemverPartCount;
}

} // namespace

TEST_CASE("version matches the CMake project version") {
    CHECK(simcore::version() == SIMCORE_EXPECTED_VERSION);
}

TEST_CASE("version is a semantic version") {
    CHECK(isSemanticVersion(simcore::version()));
}

TEST_CASE("banner is program name, a space and the version") {
    const std::string expected =
        std::string{simcore::programName()} + " " + std::string{simcore::version()};
    CHECK(simcore::versionBanner() == expected);
    CHECK(simcore::programName() == "simcore");
}

TEST_CASE("semantic version check rejects malformed input") {
    CHECK(isSemanticVersion("1.2.3"));
    CHECK_FALSE(isSemanticVersion("1.2"));
    CHECK_FALSE(isSemanticVersion("1.2.3.4"));
    CHECK_FALSE(isSemanticVersion("1..3"));
    CHECK_FALSE(isSemanticVersion("1.2.x"));
    CHECK_FALSE(isSemanticVersion(""));
}
