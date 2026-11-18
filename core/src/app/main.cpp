#include "simcore/version.hpp"

#include <iostream>

int main() {
    std::cout << simcore::versionBanner() << '\n';
    return 0;
}
