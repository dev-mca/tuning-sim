#include "acoustic/offline_render.hpp"

namespace simcore::acoustic {

namespace {

constexpr double kIdleSpeed_rpm = 900.0;
constexpr float kIdleLoad = 0.15F;
constexpr double kRunUpStart_s = 2.0;
constexpr double kRunUpEnd_s = 7.0;
constexpr double kHoldEnd_s = 8.0;
constexpr double kTopSpeed_rpm = 6500.0;
constexpr double kLiftOffSpeed_rpm = 1200.0;

} // namespace

ScenarioPoint comparisonScenario(double time_s) {
    if (time_s < kRunUpStart_s) {
        return {kIdleSpeed_rpm, kIdleLoad};
    }
    if (time_s < kRunUpEnd_s) {
        const double progress = (time_s - kRunUpStart_s) / (kRunUpEnd_s - kRunUpStart_s);
        return {kIdleSpeed_rpm + progress * (kTopSpeed_rpm - kIdleSpeed_rpm), 1.0F};
    }
    if (time_s < kHoldEnd_s) {
        return {kTopSpeed_rpm, 1.0F};
    }
    return {kLiftOffSpeed_rpm, 0.0F}; // throttle closed: speed falls, pulses get weak
}

std::vector<float> renderConstant(LayoutId layout, double duration_s, double speed_rpm, float load,
                                  std::uint32_t seed) {
    return renderOffline(
        layout, duration_s, [=](double) { return ScenarioPoint{speed_rpm, load}; }, seed);
}

} // namespace simcore::acoustic
