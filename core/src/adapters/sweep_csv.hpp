#pragma once

// Dyno sweep as CSV in the format of shared/golden/*.csv (lab/src/tuning_lab/golden.py):
// comment lines start with '#', the header names every column with its unit.

#include "domain/dyno/dyno.hpp"

#include <array>
#include <ostream>
#include <string>
#include <vector>

namespace simcore {

inline constexpr std::array kSweepCsvColumns = {
    "speedSetpoint_rpm", "speed_rpm", "torque_Nm",        "engineTorque_Nm", "power_kW", "bmep_bar",
    "map_kPa",           "lambda",    "ignition_degBTDC", "airflow_kg_s",    "egt_degC"};

/// One CSV row in the columns above, converted to user-facing units.
[[nodiscard]] std::array<double, kSweepCsvColumns.size()> sweepCsvRow(const SweepSample& sample,
                                                                      double displacement_m3);

void writeSweepCsv(std::ostream& out, const std::vector<SweepSample>& samples,
                   double displacement_m3, const std::string& comment);

} // namespace simcore
