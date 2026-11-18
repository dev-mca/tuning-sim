#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace sound_spike {

enum class LayoutId { Inline4, Inline6, V8Crossplane, V8Flatplane };

inline constexpr std::array kAllLayouts = {LayoutId::Inline4, LayoutId::Inline6,
                                           LayoutId::V8Crossplane, LayoutId::V8Flatplane};

/// A straight exhaust pipe section (research.md §6.3).
struct PipeSpec {
    double length_m;
    double diameter_m;
    double soundSpeed_m_s; // hot gas near the engine, cooler further downstream
};

struct CylinderSpec {
    double firingAngle_deg; // crank angle of this cylinder's firing TDC within 0..720
    PipeSpec primary;       // header pipe from the exhaust valve to the collector
    int group;              // collector group (e.g. bank) this primary flows into
};

struct GroupSpec {
    PipeSpec collector; // pipe after the group's collector junction
    float pan;          // -1 = left, +1 = right; used only if groups are not merged
};

/// Exhaust topology:
/// - every cylinder has a primary pipe into the collector of its group,
/// - every group continues in its collector pipe,
/// - either all groups merge into one pipe with one outlet (Y-pipe), or each group has its
///   own outlet (dual exhaust, panned in stereo).
struct LayoutSpec {
    std::string_view name;
    std::vector<CylinderSpec> cylinders;
    std::vector<GroupSpec> groups;
    bool mergeGroups;
    PipeSpec mergedPipe; // only used if mergeGroups
};

[[nodiscard]] LayoutSpec makeLayout(LayoutId id);
[[nodiscard]] std::string_view layoutKey(LayoutId id); // file-name friendly, e.g. "v8_crossplane"

} // namespace sound_spike
