#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kProgramName = "simcore";
constexpr std::string_view kVersion = SIMCORE_VERSION; // injected by CMake from project VERSION

} // namespace

int main() {
    std::cout << kProgramName << ' ' << kVersion << '\n';
    return 0;
}
