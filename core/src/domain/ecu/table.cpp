#include "domain/ecu/table.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace simcore {

namespace {

constexpr std::size_t kMinBins = 2;

void validateBins(const std::vector<double>& bins, const char* axis) {
    if (bins.size() < kMinBins) {
        throw std::invalid_argument(std::string(axis) + " axis needs at least 2 bins");
    }
    for (std::size_t i = 1; i < bins.size(); ++i) {
        if (!(bins[i] > bins[i - 1])) {
            throw std::invalid_argument(std::string(axis) + " bins must be strictly increasing");
        }
    }
}

void validateValues(const std::vector<double>& values, std::size_t expected) {
    if (values.size() != expected) {
        throw std::invalid_argument("expected " + std::to_string(expected) + " values, got " +
                                    std::to_string(values.size()));
    }
    if (!std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); })) {
        throw std::invalid_argument("table values must be finite");
    }
}

} // namespace

TableBin findBin(const std::vector<double>& bins, double value) noexcept {
    const std::size_t last = bins.size() - 1;
    if (value <= bins.front()) {
        return {0, 0.0};
    }
    if (value >= bins[last]) {
        return {last - 1, 1.0};
    }
    // upper_bound = first bin > value, like Python's bisect_right.
    const auto upper = std::upper_bound(bins.begin(), bins.end(), value);
    const auto index = static_cast<std::size_t>(upper - bins.begin()) - 1;
    const double lower = bins[index];
    return {index, (value - lower) / (bins[index + 1] - lower)};
}

Table1D::Table1D(std::vector<double> bins, std::vector<double> values)
    : bins_(std::move(bins)), values_(std::move(values)) {
    validateBins(bins_, "x");
    validateValues(values_, bins_.size());
}

double Table1D::lookup(double x) const noexcept {
    const TableBin bin = findBin(bins_, x);
    const double lower = values_[bin.index];
    return lower + bin.fraction * (values_[bin.index + 1] - lower);
}

Table2D::Table2D(std::vector<double> xBins, std::vector<double> yBins, std::vector<double> values)
    : xBins_(std::move(xBins)), yBins_(std::move(yBins)), values_(std::move(values)) {
    validateBins(xBins_, "x");
    validateBins(yBins_, "y");
    validateValues(values_, xBins_.size() * yBins_.size());
}

double Table2D::lookup(double x, double y) const noexcept {
    // research.md §2.3; same term order as lab/src/tuning_lab/table.py.
    const TableBin bx = findBin(xBins_, x);
    const TableBin by = findBin(yBins_, y);
    const double t = bx.fraction;
    const double u = by.fraction;
    const std::size_t columns = xBins_.size();
    const std::size_t i = bx.index;
    const std::size_t j = by.index;
    const double v00 = values_[j * columns + i];
    const double v10 = values_[j * columns + i + 1];
    const double v01 = values_[(j + 1) * columns + i];
    const double v11 = values_[(j + 1) * columns + i + 1];
    return (1.0 - t) * (1.0 - u) * v00 + t * (1.0 - u) * v10 + (1.0 - t) * u * v01 + t * u * v11;
}

std::size_t Table2D::index(std::size_t ix, std::size_t iy) const {
    if (ix >= xBins_.size() || iy >= yBins_.size()) {
        throw std::out_of_range("table cell (" + std::to_string(ix) + ", " + std::to_string(iy) +
                                ") outside " + std::to_string(xBins_.size()) + " x " +
                                std::to_string(yBins_.size()));
    }
    return iy * xBins_.size() + ix;
}

double Table2D::cell(std::size_t ix, std::size_t iy) const {
    return values_[index(ix, iy)];
}

void Table2D::setCell(std::size_t ix, std::size_t iy, double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("table values must be finite");
    }
    values_[index(ix, iy)] = value;
}

Table2D Table2D::withOffset(double delta) const {
    Table2D copy = *this;
    for (double& value : copy.values_) {
        value += delta;
    }
    return copy;
}

Table2D Table2D::scaled(double factor) const {
    Table2D copy = *this;
    for (double& value : copy.values_) {
        value *= factor;
    }
    return copy;
}

Table2D Table2D::filled(double value) const {
    Table2D copy = *this;
    std::fill(copy.values_.begin(), copy.values_.end(), value);
    return copy;
}

} // namespace simcore
