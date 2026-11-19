#include "acoustic/layout.hpp"

#include <cstddef>
#include <initializer_list>

namespace simcore::acoustic {

namespace {

constexpr double kCycleDeg = 720.0;

// research.md §6.3: hot exhaust near the valves ~590 m/s, cooler downstream.
constexpr double kHotSoundSpeed_m_s = 590.0;
constexpr double kWarmSoundSpeed_m_s = 520.0;
constexpr double kCoolSoundSpeed_m_s = 480.0;

PipeSpec primary(double length_m, double diameter_m = 0.040) {
    return {length_m, diameter_m, kHotSoundSpeed_m_s};
}

/// Firing angle of every cylinder (index = cylinder number - 1) from the firing order,
/// with even spacing 720 / z (research.md §1.2).
std::vector<double> firingAngles(std::initializer_list<int> firingOrder) {
    const double spacing = kCycleDeg / static_cast<double>(firingOrder.size());
    std::vector<double> angles(firingOrder.size(), 0.0);
    double angle = 0.0;
    for (const int cylinder : firingOrder) {
        angles[static_cast<std::size_t>(cylinder - 1)] = angle;
        angle += spacing;
    }
    return angles;
}

std::vector<CylinderSpec> cylinders(const std::vector<double>& angles,
                                    std::initializer_list<double> primaryLengths_m,
                                    std::initializer_list<int> groups) {
    std::vector<CylinderSpec> result;
    auto length = primaryLengths_m.begin();
    auto group = groups.begin();
    for (const double angle : angles) {
        result.push_back({angle, primary(*length++), *group++});
    }
    return result;
}

LayoutSpec inline4() {
    // Firing order 1-3-4-2, 4-into-1 header, single exhaust.
    return {"I4 2.0 (4-into-1)",
            cylinders(firingAngles({1, 3, 4, 2}), {0.58, 0.60, 0.60, 0.58}, {0, 0, 0, 0}),
            {{{2.2, 0.055, kWarmSoundSpeed_m_s}, 0.0F}},
            false,
            {}};
}

LayoutSpec inline6() {
    // Firing order 1-5-3-6-2-4, 3-into-1 x2 (typical BMW), Y-pipe into one exhaust.
    return {"I6 3.0 (3-into-1 x2, Y-pipe)",
            cylinders(firingAngles({1, 5, 3, 6, 2, 4}), {0.60, 0.55, 0.50, 0.60, 0.55, 0.50},
                      {0, 0, 0, 1, 1, 1}),
            {{{0.8, 0.050, kWarmSoundSpeed_m_s}, 0.0F}, {{0.8, 0.050, kWarmSoundSpeed_m_s}, 0.0F}},
            true,
            {1.8, 0.065, kCoolSoundSpeed_m_s}};
}

constexpr float kBankPan = 0.6F;

LayoutSpec v8(std::string_view name, std::initializer_list<int> firingOrder,
              std::initializer_list<int> banks) {
    // Dual exhaust: each bank 4-into-1 with its own tailpipe. Keeping the banks separate is
    // what makes the per-bank firing pattern audible (research.md §1.3).
    return {name,
            cylinders(firingAngles(firingOrder), {0.50, 0.52, 0.54, 0.56, 0.50, 0.52, 0.54, 0.56},
                      banks),
            {{{2.4, 0.060, kWarmSoundSpeed_m_s}, -kBankPan},
             {{2.4, 0.060, kWarmSoundSpeed_m_s}, kBankPan}},
            false,
            {}};
}

} // namespace

LayoutSpec makeLayout(LayoutId id) {
    switch (id) {
    case LayoutId::Inline4:
        return inline4();
    case LayoutId::Inline6:
        return inline6();
    case LayoutId::V8Crossplane:
        // GM LS: 1-8-7-2-6-5-4-3, odd cylinders left bank, even right. Per bank uneven.
        return v8("V8 crossplane (1-8-7-2-6-5-4-3)", {1, 8, 7, 2, 6, 5, 4, 3},
                  {0, 1, 0, 1, 0, 1, 0, 1});
    case LayoutId::V8Flatplane:
        // 1-5-4-8-3-7-2-6, cylinders 1-4 one bank, 5-8 the other. Each bank fires every 180 deg.
        return v8("V8 flatplane (1-5-4-8-3-7-2-6)", {1, 5, 4, 8, 3, 7, 2, 6},
                  {0, 0, 0, 0, 1, 1, 1, 1});
    }
    return inline4();
}

std::string_view layoutKey(LayoutId id) {
    switch (id) {
    case LayoutId::Inline4:
        return "i4";
    case LayoutId::Inline6:
        return "i6";
    case LayoutId::V8Crossplane:
        return "v8_crossplane";
    case LayoutId::V8Flatplane:
        return "v8_flatplane";
    }
    return "unknown";
}

} // namespace simcore::acoustic
