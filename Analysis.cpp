#include "Analysis.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace adeco {

CellRect cellsFor(float x, float y, float w, float h) {
    float hw = std::max(w, 1.f) * 0.5f, hh = std::max(h, 1.f) * 0.5f;
    CellRect r;
    r.x0 = int(std::floor((x - hw + 0.001f) / kCell));
    r.x1 = int(std::floor((x + hw - 0.001f) / kCell));
    r.y0 = int(std::floor((y - hh + 0.001f) / kCell));
    r.y1 = int(std::floor((y + hh - 0.001f) / kCell));
    if (r.x1 < r.x0) r.x1 = r.x0;
    if (r.y1 < r.y0) r.y1 = r.y0;
    constexpr int kMaxSpan = 16;  // cells
    auto clip = [&](int& a, int& b, float centre) {
        if (b - a + 1 > kMaxSpan) {
            int c = int(std::floor(centre / kCell));
            a = c - kMaxSpan / 2;
            b = a + kMaxSpan - 1;
        }
    };
    clip(r.x0, r.x1, x);
    clip(r.y0, r.y1, y);
    return r;
}

void Occupancy::mark(int x, int y, ObjClass c) {
    auto& v = cells[cellKey(x, y)];
    // Stored as class+1 so that a zero-initialised map entry reads as "newly inserted".
    uint8_t want = uint8_t(uint8_t(c) + 1);
    if (want > v) v = want;
}

ObjClass Occupancy::at(int x, int y) const {
    auto it = cells.find(cellKey(x, y));
    if (it == cells.end() || it->second == 0) return ObjClass::Other;
    return ObjClass(it->second - 1);
}

char const* kindName(StructKind k) {
    switch (k) {
        case StructKind::Platform: return "platform";
        case StructKind::Pillar: return "pillar";
        case StructKind::Mass: return "mass";
        case StructKind::Cluster: return "cluster";
        case StructKind::Hazards: return "hazards";
    }
    return "?";
}

namespace {

struct DSU {
    std::vector<int> p;
    explicit DSU(size_t n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    int find(int a) {
        while (p[size_t(a)] != a) {
            p[size_t(a)] = p[size_t(p[size_t(a)])];
            a = p[size_t(a)];
        }
        return a;
    }
    void unite(int a, int b) {
        a = find(a); b = find(b);
        if (a != b) p[size_t(b)] = a;
    }
};

StructKind classify(Component const& c) {
    int w = c.maxX - c.minX + 1, h = c.maxY - c.minY + 1;
    if (c.solid == 0) return StructKind::Hazards;
    if (h <= 2 && w >= 3) return StructKind::Platform;
    if (w <= 2 && h >= 3) return StructKind::Pillar;
    if (w >= 3 && h >= 3 && c.cells * 2 >= w * h) return StructKind::Mass;
    return StructKind::Cluster;
}

}  // namespace

Analysis analyse(std::vector<InObject> const& objects) {
    Analysis a;
    a.objectCount = objects.size();
    a.empty = objects.empty();

    bool any = false;
    Region b{0, 0, 0, 0};
    std::vector<MaterialCell> structural;  // player's solid + hazard cells only

    for (auto const& o : objects) {
        CellRect r = cellsFor(o.x, o.y, o.w, o.h);
        for (int y = r.y0; y <= r.y1; ++y)
            for (int x = r.x0; x <= r.x1; ++x) {
                a.occ.mark(x, y, o.cls);
                if (o.cls == ObjClass::Solid || o.cls == ObjClass::Hazard) structural.push_back({x, y, o.cls});
            }
        if (!any) { b = {r.x0, r.x1, r.y0, r.y1}; any = true; }
        else {
            b.minX = std::min(b.minX, r.x0); b.maxX = std::max(b.maxX, r.x1);
            b.minY = std::min(b.minY, r.y0); b.maxY = std::max(b.maxY, r.y1);
        }
    }
    a.bounds = any ? b : Region{0, 59, 0, 11};  // empty level: ~6 seconds of room at normal speed

    // De-duplicate structural cells (several objects can share a cell). Class priority is already
    // resolved inside the occupancy map, so look it up there.
    std::vector<uint64_t> keys;
    keys.reserve(structural.size());
    std::unordered_map<uint64_t, int> index;
    index.reserve(structural.size() * 2);
    std::vector<MaterialCell> cells;
    cells.reserve(structural.size());
    for (auto const& s : structural) {
        uint64_t k = cellKey(s.x, s.y);
        if (index.emplace(k, int(cells.size())).second) cells.push_back({s.x, s.y, a.occ.at(s.x, s.y)});
    }
    a.material = cells;

    // Connected components, 8-neighbourhood so slopes and diagonal spikes stay attached.
    DSU dsu(cells.size());
    for (size_t i = 0; i < cells.size(); ++i)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dy) continue;
                auto it = index.find(cellKey(cells[i].x + dx, cells[i].y + dy));
                if (it != index.end()) dsu.unite(int(i), it->second);
            }

    std::unordered_map<int, size_t> compOf;
    for (size_t i = 0; i < cells.size(); ++i) {
        int root = dsu.find(int(i));
        auto [it, fresh] = compOf.emplace(root, a.comps.size());
        if (fresh) {
            Component c;
            c.minX = c.maxX = cells[i].x;
            c.minY = c.maxY = cells[i].y;
            a.comps.push_back(c);
        }
        Component& c = a.comps[it->second];
        c.minX = std::min(c.minX, cells[i].x); c.maxX = std::max(c.maxX, cells[i].x);
        c.minY = std::min(c.minY, cells[i].y); c.maxY = std::max(c.maxY, cells[i].y);
        ++c.cells;
        if (cells[i].cls == ObjClass::Hazard) ++c.hazard; else ++c.solid;
    }

    // Lowest solid cell per column - used to build supports under floating platforms.
    std::vector<std::unordered_map<int, int>> low(a.comps.size());
    for (size_t i = 0; i < cells.size(); ++i) {
        if (cells[i].cls != ObjClass::Solid) continue;
        size_t ci = compOf[dsu.find(int(i))];
        auto [it, fresh] = low[ci].emplace(cells[i].x, cells[i].y);
        if (!fresh && cells[i].y < it->second) it->second = cells[i].y;
    }
    for (size_t ci = 0; ci < a.comps.size(); ++ci) {
        Component& c = a.comps[ci];
        c.kind = classify(c);
        c.grounded = c.minY <= 1;
        c.lowestPerColumn.assign(low[ci].begin(), low[ci].end());
        std::sort(c.lowestPerColumn.begin(), c.lowestPerColumn.end());
        switch (c.kind) {
            case StructKind::Platform: ++a.summary.platforms; break;
            case StructKind::Pillar: ++a.summary.pillars; break;
            case StructKind::Mass: ++a.summary.masses; break;
            case StructKind::Cluster: ++a.summary.clusters; break;
            case StructKind::Hazards: ++a.summary.hazardGroups; break;
        }
    }
    return a;
}

Region decorRegion(Analysis const& a, int maxCellY) {
    Region r;
    r.minX = std::min(a.bounds.minX, 0);
    r.maxX = a.bounds.maxX + 8;
    r.minY = 0;
    r.maxY = std::clamp(a.bounds.maxY + 8, 14, std::max(14, maxCellY));
    return r;
}

}  // namespace adeco
