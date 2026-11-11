// Level layout analysis: occupancy grid + connected-structure detection.
#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "Types.hpp"

namespace adeco {

struct CellRect {  // inclusive
    int x0, y0, x1, y1;
};

// Cells covered by an axis-aligned box centred on (x, y). Huge objects are clipped to 16x16 cells
// around their centre so one giant sprite can never make analysis explode.
CellRect cellsFor(float x, float y, float w, float h);

struct Occupancy {
    std::unordered_map<uint64_t, uint8_t> cells;  // value = ObjClass (highest priority wins)
    bool occupied(int x, int y) const { return cells.find(cellKey(x, y)) != cells.end(); }
    void mark(int x, int y, ObjClass c);
    ObjClass at(int x, int y) const;
};

struct MaterialCell {  // a solid / hazard / structure cell that decoration can hug
    int x, y;
    ObjClass cls;
};

enum class StructKind : uint8_t { Platform, Pillar, Mass, Cluster, Hazards };

struct Component {
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    int cells = 0, solid = 0, hazard = 0;
    StructKind kind = StructKind::Cluster;
    bool grounded = false;                        // touches row 0 or 1
    std::vector<std::pair<int, int>> lowestPerColumn;  // (x, lowest solid y), sorted by x
};

struct Summary {
    int platforms = 0, pillars = 0, masses = 0, clusters = 0, hazardGroups = 0;
    int total() const { return platforms + pillars + masses + clusters + hazardGroups; }
};

struct Analysis {
    Occupancy occ;
    std::vector<MaterialCell> material;
    std::vector<Component> comps;
    Summary summary;
    Region bounds;          // cell extent of all objects (a default box for empty levels)
    size_t objectCount = 0;
    bool empty = true;
};

Analysis analyse(std::vector<InObject> const& objects);

// Region the decorator is allowed to fill: the level's extent plus margins, clamped to [0, maxCellY].
Region decorRegion(Analysis const& a, int maxCellY);

char const* kindName(StructKind k);

}  // namespace adeco
