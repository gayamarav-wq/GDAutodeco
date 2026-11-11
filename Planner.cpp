#include "Planner.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace adeco {
namespace {

// ------------------------------------------------------------------ helpers
constexpr int kMaxFoot = 12;  // largest decoration footprint, in cells

bool rectFree(Analysis const& an, Region const& reg, int x0, int y0, int w, int h) {
    int x1 = x0 + w - 1, y1 = y0 + h - 1;
    if (x0 < reg.minX || x1 > reg.maxX || y0 < reg.minY || y1 > reg.maxY) return false;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (an.occ.occupied(x, y)) return false;
    return true;
}

void markRect(Analysis& an, int x0, int y0, int w, int h, ObjClass c, bool asMaterial) {
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) {
            an.occ.mark(x, y, c);
            if (asMaterial) an.material.push_back({x, y, c});
        }
}

struct Pick {
    DecoObject const* o;
    float w, h;  // natural sprite size, level units
};

struct RulePicks {
    std::vector<Pick> picks;
    std::vector<float> cum;
    float total = 0;
    bool randomRot = false;
};

RulePicks buildPicks(Rule const& r, ObjectCatalog const& cat, std::vector<std::string>& notes) {
    RulePicks rp;
    std::vector<int> rejected;
    for (auto const& o : r.objects) {
        float w = 30, h = 30;
        if (!cat.usable(o.id, w, h)) { rejected.push_back(o.id); continue; }
        if (w < 1) w = kCell;
        if (h < 1) h = kCell;
        rp.picks.push_back({&o, w, h});
        rp.total += o.weight;
        rp.cum.push_back(rp.total);
        if (o.rotMin != o.rotMax) rp.randomRot = true;
    }
    if (!rejected.empty()) {
        std::string m = "rule '" + r.name + "': skipped unusable object id(s):";
        for (int id : rejected) m += " " + std::to_string(id);
        notes.push_back(m);
    }
    return rp;
}

Pick const& choose(RulePicks const& rp, Rng& rng) {
    float t = rng.f() * rp.total;
    size_t i = size_t(std::lower_bound(rp.cum.begin(), rp.cum.end(), t) - rp.cum.begin());
    if (i >= rp.picks.size()) i = rp.picks.size() - 1;
    return rp.picks[i];
}

// Choose a random scale for an object and make sure its sprite fits the footprint we reserve for it:
//  * a sprite that overshoots a whole number of cells by a little (e.g. 1.04 cells) is snapped down to
//    that whole number, so it never leaks into the neighbouring (possibly occupied) cell;
//  * a sprite larger than the biggest footprint we reserve (kMaxFoot cells) is shrunk to fit.
float pickScale(Pick const& pk, Rng& rng) {
    DecoObject const& o = *pk.o;
    float s = rng.rangef(o.scaleMin, o.scaleMax);
    if (o.fit) return s;  // fit objects are stretched to their span exactly
    float rotFactor = (o.rotMin != o.rotMax) ? 1.42f : 1.f;
    float big = std::max(pk.w, pk.h) * rotFactor;
    s = std::min(s, float(kMaxFoot) * kCell / big);
    if (rotFactor == 1.f) {
        for (float dim : {pk.w, pk.h}) {
            float cells = dim * s / kCell;
            float k = std::floor(cells + 0.001f);
            if (k >= 1.f && cells - k > 0.001f && cells - k <= 0.2f * k) s = std::min(s, k * kCell / dim);
        }
    }
    return s;
}

struct Foot {
    int w, h;
};

// Footprint in cells. Conservative: rounds up, and for randomly rotated sprites uses the bounding square
// of the rotated sprite, so the object can never poke into a neighbouring occupied cell.
Foot footOf(Pick const& pk, float scale, bool quarterTurn) {
    DecoObject const& o = *pk.o;
    int w, h;
    if (o.fit) {
        w = o.spanW;
        h = o.spanH;
    } else {
        w = std::max(o.spanW, int(std::ceil(pk.w * scale / kCell - 0.001f)));
        h = std::max(o.spanH, int(std::ceil(pk.h * scale / kCell - 0.001f)));
        if (o.rotMin != o.rotMax) {
            // Random rotation produces arbitrary angles, so reserve the bounding square of the rotated sprite.
            int s = int(std::ceil(std::max(pk.w, pk.h) * scale * 1.42f / kCell - 0.001f));
            w = h = std::max(s, 1);
        } else if (quarterTurn) {
            std::swap(w, h);
        }
    }
    w = std::clamp(w, 1, kMaxFoot);
    h = std::clamp(h, 1, kMaxFoot);
    return {w, h};
}

enum Side { Top, Right, Bottom, Left };
constexpr int kDx[4] = {0, 1, 0, -1};
constexpr int kDy[4] = {1, 0, -1, 0};
constexpr int kSideBit[4] = {SideTop, SideRight, SideBottom, SideLeft};
constexpr float kSideRot[4] = {0.f, 90.f, 180.f, 270.f};

// Where a footprint sits relative to the empty cell (cx,cy) that touches the structure on `side`.
void anchorFor(int side, int cx, int cy, Foot f, int& x0, int& y0) {
    switch (side) {
        case Top: x0 = cx - (f.w - 1) / 2; y0 = cy; break;
        case Bottom: x0 = cx - (f.w - 1) / 2; y0 = cy - f.h + 1; break;
        case Right: x0 = cx; y0 = cy - (f.h - 1) / 2; break;
        default: x0 = cx - f.w + 1; y0 = cy - (f.h - 1) / 2; break;
    }
}

bool matchesNear(Near n, ObjClass c) {
    switch (n) {
        case Near::Solid: return c == ObjClass::Solid || c == ObjClass::Structure;
        case Near::Hazard: return c == ObjClass::Hazard;
        case Near::Any: return c == ObjClass::Solid || c == ObjClass::Structure || c == ObjClass::Hazard;
    }
    return false;
}

Placement makePlacement(Rule const& r, int ruleIdx, Pick const& pk, float scale, int x0, int y0, Foot f, float rot, Rng& rng) {
    DecoObject const& o = *pk.o;
    Placement p;
    p.id = o.id;
    p.x = (float(x0) + float(f.w) * 0.5f) * kCell;
    p.y = (float(y0) + float(f.h) * 0.5f) * kCell;
    if (o.fit) {
        p.sx = float(f.w) * kCell / pk.w;
        p.sy = float(f.h) * kCell / pk.h;
    } else {
        p.sx = p.sy = scale;
    }
    p.rot = rot;
    p.flipX = o.flipX && rng.f() < 0.5f;
    p.flipY = o.flipY && rng.f() < 0.5f;
    p.z = r.z;
    p.zOrder = r.zOrder;
    p.color = o.color;
    p.detail = o.detail;
    p.rule = ruleIdx;
    return p;
}

float randomRot(DecoObject const& o, Rng& rng) {
    return o.rotMin == o.rotMax ? float(o.rotMin) : rng.rangef(float(o.rotMin), float(o.rotMax));
}

template <class T> void shuffle(std::vector<T>& v, Rng& rng) {
    for (size_t i = v.size(); i > 1; --i) std::swap(v[i - 1], v[size_t(rng.range(0, int(i) - 1))]);
}

struct Cand {
    int x, y, side;
};

// ------------------------------------------------------------------ rule runners
// Each returns the number of placements it appended. `budget` is the most it may add.
struct RuleEnv {
    Preset const& preset;
    Analysis& an;
    PlanLimits const& lim;
    Rng& rng;
    float densityScale;
    Plan& plan;
};

int runEdge(RuleEnv& e, Rule const& r, int ruleIdx, RulePicks const& rp, int budget) {
    std::vector<Cand> cands;
    std::unordered_set<uint64_t> seen;
    for (auto const& m : e.an.material) {
        if (!matchesNear(r.near, m.cls)) continue;
        for (int s = 0; s < 4; ++s) {
            if (!(r.sides & kSideBit[s])) continue;
            int nx = m.x + kDx[s], ny = m.y + kDy[s];
            if (!e.lim.region.contains(nx, ny) || e.an.occ.occupied(nx, ny)) continue;
            if (seen.insert(cellKey(nx, ny)).second) cands.push_back({nx, ny, s});
        }
    }
    shuffle(cands, e.rng);
    float p = std::min(1.f, r.density * e.densityScale);
    int placed = 0;
    for (auto const& c : cands) {
        if (placed >= budget) break;
        if (e.rng.f() >= p) continue;
        Pick const& pk = choose(rp, e.rng);
        float scale = pickScale(pk, e.rng);
        bool orient = r.orient && !pk.o->fit;
        Foot f = footOf(pk, scale, orient && (c.side == Left || c.side == Right));
        int x0, y0;
        anchorFor(c.side, c.x, c.y, f, x0, y0);
        if (!rectFree(e.an, e.lim.region, x0, y0, f.w, f.h)) continue;
        float rot = randomRot(*pk.o, e.rng) + (orient ? kSideRot[c.side] : 0.f);
        e.plan.items.push_back(makePlacement(r, ruleIdx, pk, scale, x0, y0, f, rot, e.rng));
        markRect(e.an, x0, y0, f.w, f.h, ObjClass::Deco, false);
        ++placed;
    }
    return placed;
}

int runHang(RuleEnv& e, Rule const& r, int ruleIdx, RulePicks const& rp, int budget) {
    std::vector<Cand> cands;
    std::unordered_set<uint64_t> seen;
    for (auto const& m : e.an.material) {
        if (!matchesNear(r.near, m.cls)) continue;
        int nx = m.x, ny = m.y - 1;
        if (!e.lim.region.contains(nx, ny) || e.an.occ.occupied(nx, ny)) continue;
        if (seen.insert(cellKey(nx, ny)).second) cands.push_back({nx, ny, Bottom});
    }
    shuffle(cands, e.rng);
    float p = std::min(1.f, r.density * e.densityScale);
    int placed = 0;
    for (auto const& c : cands) {
        if (placed >= budget) break;
        if (e.rng.f() >= p) continue;
        Pick const& pk = choose(rp, e.rng);  // one object type per string
        int len = e.rng.range(r.lengthMin, r.lengthMax);
        int yTop = c.y;
        for (int i = 0; i < len && placed < budget; ++i) {
            float scale = pickScale(pk, e.rng);
            Foot f = footOf(pk, scale, false);
            int x0 = c.x - (f.w - 1) / 2, y0 = yTop - f.h + 1;
            if (!rectFree(e.an, e.lim.region, x0, y0, f.w, f.h)) break;
            e.plan.items.push_back(makePlacement(r, ruleIdx, pk, scale, x0, y0, f, randomRot(*pk.o, e.rng), e.rng));
            markRect(e.an, x0, y0, f.w, f.h, ObjClass::Deco, false);
            ++placed;
            yTop = y0 - 1;
        }
    }
    return placed;
}

int runScatter(RuleEnv& e, Rule const& r, int ruleIdx, RulePicks const& rp, int budget) {
    std::vector<uint32_t> hug;
    for (size_t i = 0; i < e.an.material.size(); ++i)
        if (matchesNear(r.near, e.an.material[i].cls)) hug.push_back(uint32_t(i));

    float p = std::min(1.f, r.density * e.densityScale);
    int64_t attempts;
    int64_t area = int64_t(e.lim.region.maxX - e.lim.region.minX + 1) * (e.lim.region.maxY - e.lim.region.minY + 1);
    if (!hug.empty()) attempts = int64_t(std::ceil(double(hug.size()) * p * 2.0)) + 1;
    else attempts = int64_t(std::ceil(double(area) * p * 0.25)) + 1;
    attempts = std::min<int64_t>(attempts, int64_t(budget) * 3 + 8);

    int placed = 0;
    for (int64_t a = 0; a < attempts && placed < budget; ++a) {
        int cx, cy;
        if (!hug.empty()) {
            auto const& m = e.an.material[hug[size_t(e.rng.range(0, int(hug.size()) - 1))]];
            cx = m.x + e.rng.range(-r.radius, r.radius);
            cy = m.y + e.rng.range(-r.radius, r.radius);
        } else {
            cx = e.rng.range(e.lim.region.minX, e.lim.region.maxX);
            cy = e.rng.range(e.lim.region.minY, e.lim.region.maxY);
        }
        Pick const& pk = choose(rp, e.rng);
        float scale = pickScale(pk, e.rng);
        Foot f = footOf(pk, scale, false);
        int x0 = cx - (f.w - 1) / 2, y0 = cy - (f.h - 1) / 2;
        if (!rectFree(e.an, e.lim.region, x0, y0, f.w, f.h)) continue;
        e.plan.items.push_back(makePlacement(r, ruleIdx, pk, scale, x0, y0, f, randomRot(*pk.o, e.rng), e.rng));
        markRect(e.an, x0, y0, f.w, f.h, ObjClass::Deco, false);
        ++placed;
    }
    return placed;
}

int runBackdrop(RuleEnv& e, Rule const& r, int ruleIdx, RulePicks const& rp, int budget) {
    Region const& g = e.lim.region;
    int64_t cols = (g.maxX - g.minX) / r.strideX + 1, rows = (g.maxY - g.minY) / r.strideY + 1;
    double expected = double(cols) * double(rows) * double(std::min(1.f, r.density * e.densityScale));
    float p = std::min(1.f, r.density * e.densityScale);
    // If the budget cannot cover the whole level, thin out uniformly instead of filling only the start.
    if (expected > double(budget)) p = float(double(p) * double(budget) / expected * 0.95);

    int placed = 0;
    for (int gx = g.minX; gx <= g.maxX && placed < budget; gx += r.strideX)
        for (int gy = g.minY; gy <= g.maxY && placed < budget; gy += r.strideY) {
            if (e.rng.f() >= p) continue;
            Pick const& pk = choose(rp, e.rng);
            float scale = pickScale(pk, e.rng);
            Foot f = footOf(pk, scale, false);
            int x0 = gx + e.rng.range(0, r.strideX - 1), y0 = gy + e.rng.range(0, r.strideY - 1);
            if (!rectFree(e.an, g, x0, y0, f.w, f.h)) continue;
            e.plan.items.push_back(makePlacement(r, ruleIdx, pk, scale, x0, y0, f, randomRot(*pk.o, e.rng), e.rng));
            markRect(e.an, x0, y0, f.w, f.h, ObjClass::Deco, false);
            ++placed;
        }
    return placed;
}

}  // namespace

// ====================================================================== structure
Plan planStructure(Preset const& p, Analysis& an, ObjectCatalog const& cat, PlanLimits const& lim, uint64_t seed) {
    Plan plan;
    StructureSpec const& sp = p.structure;
    if (!sp.enabled) { plan.stats.notes.push_back("structure layout disabled by this preset"); return plan; }
    float bw = 30, bh = 30;
    if (!cat.usable(sp.id, bw, bh)) {
        plan.stats.notes.push_back("structure object id " + std::to_string(sp.id) + " is not usable in this game version");
        return plan;
    }
    if (bw < 1) bw = kCell;
    if (bh < 1) bh = kCell;

    Rng rng(seed ^ 0x57A4C7ull);
    Region const& reg = lim.region;
    int budget = lim.maxObjects;

    auto addPiece = [&](int x0, int y0, int w, int h) {
        Placement pl;
        pl.id = sp.id;
        pl.x = (float(x0) + float(w) * 0.5f) * kCell;
        pl.y = (float(y0) + float(h) * 0.5f) * kCell;
        pl.sx = float(w) * kCell / bw;
        pl.sy = float(h) * kCell / bh;
        pl.z = sp.z;
        pl.zOrder = sp.zOrder;
        pl.color = sp.color;
        pl.rule = -1;
        plan.items.push_back(pl);
        markRect(an, x0, y0, w, h, ObjClass::Structure, true);
        ++plan.stats.structurePieces;
        --budget;
    };
    auto columnFree = [&](int x, int y0, int y1) {
        for (int y = y0; y <= y1; ++y)
            if (an.occ.occupied(x, y)) return false;
        return true;
    };

    // 1) Supports: fill the free space under floating solid structures, down to the floor.
    for (size_t ci = 0; ci < an.comps.size() && budget > 0; ++ci) {
        Component const& c = an.comps[ci];
        auto const& cols = c.lowestPerColumn;
        size_t i = 0;
        while (i < cols.size() && budget > 0) {
            if (rng.f() >= sp.supportDensity) { ++i; continue; }
            int x = cols[i].first, lowY = cols[i].second;
            if (lowY <= reg.minY || !reg.contains(x, lowY - 1)) { ++i; continue; }
            int y = lowY - 1;
            while (y >= reg.minY && !an.occ.occupied(x, y)) --y;
            int bottom = y + 1, top = lowY - 1;
            if (top < bottom) { ++i; continue; }
            int wMax = rng.range(1, sp.widthMax), w = 1;
            while (w < wMax && i + size_t(w) < cols.size() && cols[i + size_t(w)].first == x + w &&
                   cols[i + size_t(w)].second == lowY && reg.contains(x + w, top) && columnFree(x + w, bottom, top))
                ++w;
            addPiece(x, bottom, w, top - bottom + 1);
            i += size_t(w);
        }
    }

    // 2) Skyline: pillars and walls rising from the floor behind the level.
    int x = reg.minX + rng.range(0, sp.gapMax);
    while (x <= reg.maxX && budget > 0) {
        int w = rng.range(sp.widthMin, sp.widthMax);
        if (x + w - 1 > reg.maxX) w = reg.maxX - x + 1;
        if (rng.f() < sp.skylineDensity) {
            int hWant = std::min(rng.range(sp.heightMin, sp.heightMax), reg.maxY - reg.minY + 1);
            int fh = 0;
            while (fh < hWant) {
                bool free = true;
                for (int cx = x; cx < x + w; ++cx)
                    if (an.occ.occupied(cx, reg.minY + fh)) { free = false; break; }
                if (!free) break;
                ++fh;
            }
            if (fh >= std::max(2, sp.heightMin / 2)) addPiece(x, reg.minY, w, fh);
        }
        x += w + rng.range(sp.gapMin, sp.gapMax);
    }
    return plan;
}

// ====================================================================== decoration
Plan planDeco(Preset const& p, Analysis& an, ObjectCatalog const& cat, PlanLimits const& lim, uint64_t seed,
              float densityScale) {
    Plan plan;
    Rng rng(seed ^ 0xDEC0FFEEull);
    RuleEnv env{p, an, lim, rng, densityScale, plan};

    // Resolve usable objects up-front so we know which rules can run at all.
    std::vector<RulePicks> picks;
    picks.reserve(p.rules.size());
    int runnable = 0;
    for (auto const& r : p.rules) {
        picks.push_back(buildPicks(r, cat, plan.stats.notes));
        if (!picks.back().picks.empty()) ++runnable;
        else plan.stats.notes.push_back("rule '" + r.name + "' skipped: none of its objects are usable");
    }

    int left = runnable;
    for (size_t i = 0; i < p.rules.size(); ++i) {
        Rule const& r = p.rules[i];
        if (picks[i].picks.empty()) continue;
        int remaining = lim.maxObjects - int(plan.items.size());
        if (remaining <= 0) break;
        int budget = std::max(1, remaining / std::max(1, left));  // equal share of what is left
        if (r.maxCount > 0) budget = std::min(budget, r.maxCount);
        --left;

        int n = 0;
        switch (r.kind) {
            case RuleKind::Edge: n = runEdge(env, r, int(i), picks[i], budget); break;
            case RuleKind::Hang: n = runHang(env, r, int(i), picks[i], budget); break;
            case RuleKind::Scatter: n = runScatter(env, r, int(i), picks[i], budget); break;
            case RuleKind::Backdrop: n = runBackdrop(env, r, int(i), picks[i], budget); break;
        }
        plan.stats.perRule.emplace_back(r.name, n);
    }
    return plan;
}

// ====================================================================== rhythm
RhythmPlan planRhythm(Preset const& p, Analysis& an, ObjectCatalog const& cat, Region const& region,
                      std::vector<float> const& beatXs, int maxObjects) {
    RhythmPlan rp;
    RhythmSpec const& rs = p.rhythm;
    if (!rs.enabled) { rp.notes.push_back("this preset has no rhythm section"); return rp; }

    int budget = maxObjects;
    // Pulse triggers sit in a free cell just above the decorated region.
    for (size_t b = 0; b < beatXs.size() && budget > 0; b += size_t(rs.pulseEveryBeats)) {
        int cx = int(std::floor(beatXs[b] / kCell));
        if (cx < region.minX || cx > region.maxX) continue;
        int cy = region.maxY + 1;
        int tries = 0;
        while (an.occ.occupied(cx, cy) && tries++ < 8) ++cy;
        if (an.occ.occupied(cx, cy)) continue;
        an.occ.mark(cx, cy, ObjClass::Other);
        rp.pulses.push_back({beatXs[b], (float(cy) + 0.5f) * kCell});
        --budget;
    }

    // Optional beat markers on a fixed row.
    MarkerSpec const& ms = rs.marker;
    if (ms.id > 0) {
        float w = 30, h = 30;
        if (!cat.usable(ms.id, w, h)) {
            rp.notes.push_back("beat marker object " + std::to_string(ms.id) + " is not usable");
        } else {
            if (w < 1) w = kCell;
            if (h < 1) h = kCell;
            DecoObject o;
            o.id = ms.id;
            o.scaleMin = o.scaleMax = ms.scale;
            o.color = ms.color;
            Pick pk{&o, w, h};
            for (size_t b = 0; b < beatXs.size() && budget > 0; b += size_t(ms.everyBeats)) {
                int cx = int(std::floor(beatXs[b] / kCell));
                Foot f = footOf(pk, ms.scale, false);
                int x0 = cx - (f.w - 1) / 2, y0 = ms.row;
                if (!rectFree(an, region, x0, y0, f.w, f.h)) continue;
                Placement pl;
                pl.id = ms.id;
                pl.x = (float(x0) + float(f.w) * 0.5f) * kCell;
                pl.y = (float(y0) + float(f.h) * 0.5f) * kCell;
                pl.sx = pl.sy = ms.scale;
                pl.z = ms.z;
                pl.color = ms.color;
                pl.rule = -2;
                rp.markers.push_back(pl);
                markRect(an, x0, y0, f.w, f.h, ObjClass::Deco, false);
                --budget;
            }
        }
    }
    return rp;
}

}  // namespace adeco
