#pragma once

#include <cstddef>
#include <vector>

namespace simcore {

/// Position between two bins: lower index and fraction t in [0, 1].
struct TableBin {
    std::size_t index;
    double fraction;
};

/// Finds i with bins[i] <= value <= bins[i+1] by binary search; clamps outside the axis
/// (docs/research.md §2.3). Precondition: bins strictly increasing, at least two.
[[nodiscard]] TableBin findBin(const std::vector<double>& bins, double value) noexcept;

/// 1D table y = f(x), linear interpolation, clamped (e.g. IAT correction, later iterations).
class Table1D {
public:
    /// Throws std::invalid_argument if bins are not strictly increasing or sizes differ.
    Table1D(std::vector<double> bins, std::vector<double> values);

    [[nodiscard]] double lookup(double x) const noexcept;
    [[nodiscard]] const std::vector<double>& bins() const noexcept { return bins_; }
    [[nodiscard]] const std::vector<double>& values() const noexcept { return values_; }

private:
    std::vector<double> bins_;
    std::vector<double> values_;
};

/// 2D table z = f(x, y), bilinear interpolation, clamped (docs/research.md §2.3).
/// Values are stored row by row: value(ix, iy) = values[iy * xCount + ix], like the JSON
/// format values[y][x] (docs/plan.md §5.1). Units: whatever the caller uses (SI in the domain).
class Table2D {
public:
    /// Throws std::invalid_argument on invalid bins, wrong value count or non-finite values.
    Table2D(std::vector<double> xBins, std::vector<double> yBins, std::vector<double> values);

    [[nodiscard]] double lookup(double x, double y) const noexcept;

    [[nodiscard]] double cell(std::size_t ix, std::size_t iy) const;
    /// Changes one cell (table editing). Throws std::out_of_range / std::invalid_argument.
    void setCell(std::size_t ix, std::size_t iy, double value);

    [[nodiscard]] const std::vector<double>& xBins() const noexcept { return xBins_; }
    [[nodiscard]] const std::vector<double>& yBins() const noexcept { return yBins_; }
    [[nodiscard]] const std::vector<double>& values() const noexcept { return values_; }

    [[nodiscard]] Table2D withOffset(double delta) const;
    [[nodiscard]] Table2D scaled(double factor) const;
    [[nodiscard]] Table2D filled(double value) const;

private:
    [[nodiscard]] std::size_t index(std::size_t ix, std::size_t iy) const;

    std::vector<double> xBins_;
    std::vector<double> yBins_;
    std::vector<double> values_;
};

} // namespace simcore
