// Preset data model + JSON loader. See README.md ("Preset format") for the file schema.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Types.hpp"

namespace adeco {

enum class RuleKind : uint8_t {
    Edge,      // objects in free cells touching a structure (halos, trims, grass, thorns...)
    Hang,      // vertical strings hanging from the underside of structures (chains, vines...)
    Scatter,   // loose particles/ornaments near structures (or anywhere in an empty level)
    Backdrop,  // big pieces on a coarse grid in empty space (clouds, moons, gears...)
};

enum class Near : uint8_t { Solid, Hazard, Any };

enum SideMask : int { SideTop = 1, SideRight = 2, SideBottom = 4, SideLeft = 8, SideAll = 15 };

struct DecoObject {
    int id = 0;
    float weight = 1.f;
    float scaleMin = 1.f, scaleMax = 1.f;
    int rotMin = 0, rotMax = 0;     // random rotation in degrees
    bool flipX = false, flipY = false;  // randomly flip when true
    Role color = Role::Accent;
    std::optional<Role> detail;     // colour of the object's secondary layer, if any
    int spanW = 0, spanH = 0;       // footprint override in cells (0 = derive from the sprite)
    bool fit = false;               // stretch the sprite to fill span exactly
};

struct Rule {
    RuleKind kind = RuleKind::Edge;
    std::string name;
    std::vector<DecoObject> objects;
    float density = 0.3f;           // 0..1 probability per candidate
    int z = 1;                      // GD z layer (B4=-3, B3=-1, B2=1, B1=3, T1=5, T2=7, T3=9)
    int zOrder = 0;
    int sides = SideAll;            // Edge: which sides of a structure to decorate
    bool orient = false;            // Edge: rotate outward (0/90/180/270 for top/right/bottom/left)
    Near near = Near::Solid;        // Edge/Scatter: what to hug
    int radius = 3;                 // Scatter: max distance in cells
    int strideX = 4, strideY = 4;   // Backdrop: grid pitch in cells
    int lengthMin = 1, lengthMax = 3;  // Hang: length in cells
    int maxCount = 0;               // hard cap for this rule (0 = no cap)
};

struct StructureSpec {
    bool enabled = true;
    int id = 3092;                  // plain 1x1 pixel block, a non-solid decoration
    Role color = Role::Blocks;
    int z = -3, zOrder = -6;
    float supportDensity = 0.8f;    // fill under floating platforms
    float skylineDensity = 0.6f;    // background pillars / walls
    int widthMin = 2, widthMax = 4;
    int heightMin = 3, heightMax = 9;
    int gapMin = 1, gapMax = 4;
};

struct MarkerSpec {
    int id = 0;                     // 0 = no markers
    Role color = Role::Accent;
    int everyBeats = 1;
    int row = 0;                    // grid row used for the beat markers
    int z = 3;
    float scale = 0.6f;
};

struct RhythmSpec {
    bool enabled = true;
    int pulseEveryBeats = 1;
    float fadeIn = 0.f, hold = 0.03f, fadeOut = 0.2f;
    float brighten = 0.5f;          // pulse colour = mix(glow, white, brighten)
    MarkerSpec marker;
};

struct Preset {
    std::string id, name, description;
    Palette palette;
    uint32_t seed = 1;
    std::vector<int> preview;       // object IDs whose sprites are drawn in the thumbnail
    StructureSpec structure;
    std::vector<Rule> rules;
    RhythmSpec rhythm;
};

struct LoadResult {
    std::vector<Preset> presets;
    std::vector<std::string> errors;  // human readable; a bad preset never aborts the others
};

// Parse a presets document. `sourceName` is only used in error messages.
LoadResult loadPresetsFromJson(std::string_view text, std::string_view sourceName);

}  // namespace adeco
