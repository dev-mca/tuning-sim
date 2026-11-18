// Table1D / Table2D: properties from docs/research.md §2.3.

#include "domain/ecu/table.hpp"

#include <doctest/doctest.h>

#include <stdexcept>
#include <vector>

using namespace simcore;

namespace {

const std::vector<double> kX = {0.0, 10.0, 30.0};
const std::vector<double> kY = {100.0, 200.0};

/// f(x, y) = x + 0.01 y is bilinear: interpolation must reproduce it exactly.
Table2D bilinearTable() {
    std::vector<double> values;
    for (const double y : kY) {
        for (const double x : kX) {
            values.push_back(x + 0.01 * y);
        }
    }
    return {kX, kY, values};
}

} // namespace

TEST_CASE("Table2D is exact at every node") {
    const Table2D table = bilinearTable();
    for (const double y : kY) {
        for (const double x : kX) {
            CHECK(table.lookup(x, y) == doctest::Approx(x + 0.01 * y));
        }
    }
}

TEST_CASE("Table2D reproduces a bilinear function between nodes") {
    const Table2D table = bilinearTable();
    CHECK(table.lookup(5.0, 150.0) == doctest::Approx(6.5));
    CHECK(table.lookup(20.0, 120.0) == doctest::Approx(21.2));
}

TEST_CASE("Table2D clamps outside the axes") {
    const Table2D table = bilinearTable();
    CHECK(table.lookup(-100.0, 150.0) == doctest::Approx(table.lookup(0.0, 150.0)));
    CHECK(table.lookup(1.0e6, 150.0) == doctest::Approx(table.lookup(30.0, 150.0)));
    CHECK(table.lookup(5.0, -1.0) == doctest::Approx(table.lookup(5.0, 100.0)));
    CHECK(table.lookup(5.0, 1.0e6) == doctest::Approx(table.lookup(5.0, 200.0)));
}

TEST_CASE("Table2D is continuous across bin borders") {
    const Table2D table = bilinearTable();
    CHECK(table.lookup(10.0 - 1.0e-9, 150.0) ==
          doctest::Approx(table.lookup(10.0 + 1.0e-9, 150.0)));
}

TEST_CASE("Table2D rejects invalid input") {
    CHECK_THROWS_AS(Table2D({0.0, 0.0}, {0.0, 1.0}, {1, 2, 3, 4}), std::invalid_argument);
    CHECK_THROWS_AS(Table2D({1.0, 0.0}, {0.0, 1.0}, {1, 2, 3, 4}), std::invalid_argument);
    CHECK_THROWS_AS(Table2D({0.0}, {0.0, 1.0}, {1, 2}), std::invalid_argument);
    CHECK_THROWS_AS(Table2D({0.0, 1.0}, {0.0, 1.0}, {1, 2, 3}), std::invalid_argument);
}

TEST_CASE("Table2D cells can be edited and are range checked") {
    Table2D table = bilinearTable();
    table.setCell(2, 1, 99.0);
    CHECK(table.cell(2, 1) == 99.0);
    CHECK(table.lookup(30.0, 200.0) == doctest::Approx(99.0));
    CHECK_THROWS_AS(table.setCell(3, 0, 1.0), std::out_of_range);
    CHECK_THROWS_AS(table.setCell(0, 2, 1.0), std::out_of_range);
}

TEST_CASE("Table2D variants leave the original unchanged") {
    const Table2D table = bilinearTable();
    CHECK(table.withOffset(1.0).lookup(5.0, 150.0) == doctest::Approx(7.5));
    CHECK(table.scaled(2.0).lookup(5.0, 150.0) == doctest::Approx(13.0));
    CHECK(table.filled(0.85).lookup(17.0, 133.0) == doctest::Approx(0.85));
    CHECK(table.lookup(5.0, 150.0) == doctest::Approx(6.5));
}

TEST_CASE("Table1D interpolates linearly and clamps") {
    const Table1D table({0.0, 10.0, 20.0}, {0.0, 100.0, 50.0});
    CHECK(table.lookup(5.0) == doctest::Approx(50.0));
    CHECK(table.lookup(15.0) == doctest::Approx(75.0));
    CHECK(table.lookup(-5.0) == doctest::Approx(0.0));
    CHECK(table.lookup(99.0) == doctest::Approx(50.0));
    CHECK_THROWS_AS(Table1D({0.0, 1.0}, {1.0}), std::invalid_argument);
}
