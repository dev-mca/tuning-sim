#include "adapters/sweep_csv.hpp"

#include "domain/common/units.hpp"
#include "domain/engine/engine_model.hpp"

#include <cstdio>

namespace simcore {

namespace {

constexpr int kDecimals = 6; // same as lab golden.py

std::string formatNumber(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", kDecimals, value);
    return buffer;
}

} // namespace

std::array<double, kSweepCsvColumns.size()> sweepCsvRow(const SweepSample& sample,
                                                        double displacement_m3) {
    // Same expressions as lab golden.py sweep_rows (bmep = 4 pi / V_d * T).
    const double bmep_Pa = torqueToBmep_Pa(1.0, displacement_m3) * sample.measuredTorque_Nm;
    return {units::radSToRpm(sample.setpoint_rad_s),
            units::radSToRpm(sample.speed_rad_s),
            sample.measuredTorque_Nm,
            sample.engineTorque_Nm,
            units::wToKw(sample.power_W()),
            units::paToBar(bmep_Pa),
            units::paToKpa(sample.manifoldPressure_Pa),
            sample.lambdaActual,
            sample.ignition_deg,
            sample.airMassFlow_kg_s,
            units::kelvinToCelsius(sample.exhaustTemperature_K)};
}

void writeSweepCsv(std::ostream& out, const std::vector<SweepSample>& samples,
                   double displacement_m3, const std::string& comment) {
    if (!comment.empty()) {
        out << "# " << comment << '\n';
    }
    for (std::size_t i = 0; i < kSweepCsvColumns.size(); ++i) {
        out << (i == 0 ? "" : ",") << kSweepCsvColumns[i];
    }
    out << '\n';
    for (const SweepSample& sample : samples) {
        const auto row = sweepCsvRow(sample, displacement_m3);
        for (std::size_t i = 0; i < row.size(); ++i) {
            out << (i == 0 ? "" : ",") << formatNumber(row[i]);
        }
        out << '\n';
    }
}

} // namespace simcore
