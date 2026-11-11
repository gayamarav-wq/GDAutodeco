// Placement planning. Pure functions: (preset, analysed level, catalog, limits, seed) -> list of
// placements. Nothing here touches the game; the Geode layer turns Plans into editor objects.
//
// Guarantees (verified by tests/test_core.cpp):
//   * every placement's footprint lies entirely in cells that were EMPTY in the analysed level
//     (the player's objects are never overlapped or moved),
//   * every placement lies inside the supplied Region,
//   * the number of placements never exceeds PlanLimits::maxObjects,
//   * objects the catalog rejects are never emitted,
//   * identical inputs + seed give identical output on every platform.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Analysis.hpp"
#include "Preset.hpp"
#include "Types.hpp"

namespace adeco {

// The game-facing side tells the planner which object IDs can be used and how big their sprite is.
struct ObjectCatalog {
    virtual ~ObjectCatalog() = default;
    // True if `id` exists and is a harmless decoration. Fills the sprite's natural size (level units).
    virtual bool usable(int id, float& w, float& h) const = 0;
};

struct Placement {
    int id = 0;
    float x = 0, y = 0;  // centre, level units
    float rot = 0;       // degrees clockwise
    bool flipX = false, flipY = false;
    float sx = 1, sy = 1;
    int z = 1, zOrder = 0;
    Role color = Role::Accent;
    std::optional<Role> detail;
    int rule = -1;  // -1 structure piece, -2 beat marker, >=0 index into Preset::rules
};

struct PlanLimits {
    int maxObjects = 1000;
    Region region;
};

struct PlanStats {
    int structurePieces = 0;
    std::vector<std::pair<std::string, int>> perRule;
    std::vector<std::string> notes;  // e.g. "rule 'chains' skipped: no usable objects"
};

struct Plan {
    std::vector<Placement> items;
    PlanStats stats;
};

// Phase 1: grey "pillars and walls" - supports under floating structures plus a background skyline.
// Planned cells are written back into `an` (as ObjClass::Structure) so phase 2 sees them.
Plan planStructure(Preset const& p, Analysis& an, ObjectCatalog const& cat, PlanLimits const& lim, uint64_t seed);

// Phase 2: preset rules (trims, halos, chains, scatter, backdrops).
Plan planDeco(Preset const& p, Analysis& an, ObjectCatalog const& cat, PlanLimits const& lim, uint64_t seed,
              float densityScale);

struct BeatTrigger {
    float x = 0, y = 0;
};
struct RhythmPlan {
    std::vector<Placement> markers;
    std::vector<BeatTrigger> pulses;
    std::vector<std::string> notes;
};
// Phase 3 (optional): beat markers + pulse trigger positions for the supplied beat X positions.
RhythmPlan planRhythm(Preset const& p, Analysis& an, ObjectCatalog const& cat, Region const& region,
                      std::vector<float> const& beatXs, int maxObjects);

}  // namespace adeco
