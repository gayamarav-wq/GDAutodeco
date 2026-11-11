// Off-game tests for the Auto Decorator planning core. Build + run with tests/run_tests.sh
// (needs only a C++20 compiler). These do NOT test the Geode glue (UI, hooks, object creation) -
// see the manual checklist in README.md for that.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_set>

#include "core/Analysis.hpp"
#include "core/ObjectString.hpp"
#include "core/Planner.hpp"
#include "core/Preset.hpp"
#include "core/Tempo.hpp"
#include "core/Types.hpp"

using namespace adeco;

static int g_checks = 0, g_fail = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)
#define SECTION(name) std::printf("[%s]\n", name)

// ------------------------------------------------------------------ stub catalog
struct StubCatalog : ObjectCatalog {
    std::set<int> bad;     // ids that "do not exist / are solid"
    bool rejectAll = false;
    bool usable(int id, float& w, float& h) const override {
        if (rejectAll || bad.count(id)) return false;
        w = h = 30;
        if (id == 130 || id == 936 || id == 937) { w = 90; h = 60; }   // clouds
        if (id == 3637 || id == 3636 || id == 3622) { w = h = 120; }   // big circles
        if (id == 3639) { w = h = 90; }
        if (id == 85 || id == 86 || id == 678) { w = h = 90; }
        if (id == 1058 || id == 1059 || id == 1060) { w = h = 60; }
        return true;
    }
};

// ------------------------------------------------------------------ level generators
static std::vector<InObject> makeLevel(int lengthCells, int heightCells, double fill, uint64_t seed, bool withHazards = true) {
    Rng rng(seed);
    std::vector<InObject> v;
    // ground with a few gaps
    for (int x = 0; x < lengthCells; ++x) {
        if (x % 97 > 90) continue;
        v.push_back({1, x * kCell + 15, 15, 30, 30, ObjClass::Solid});
    }
    for (int y = 1; y < heightCells; ++y)
        for (int x = 0; x < lengthCells; ++x)
            if (rng.f() < fill) {
                bool hazard = withHazards && rng.f() < 0.08;
                v.push_back({hazard ? 8 : 1, x * kCell + 15, y * kCell + 15, 30, 30,
                             hazard ? ObjClass::Hazard : ObjClass::Solid});
            }
    // a few big saws (3x3 cells) and some portals/triggers that must also stay untouched
    for (int i = 0; i < lengthCells / 40; ++i)
        v.push_back({88, float(rng.range(5, lengthCells - 5)) * kCell + 15, float(rng.range(3, heightCells)) * kCell + 15, 90, 90, ObjClass::Hazard});
    for (int i = 0; i < lengthCells / 60; ++i)
        v.push_back({10, float(rng.range(0, lengthCells)) * kCell + 15, float(rng.range(1, heightCells)) * kCell + 15, 30, 60, ObjClass::Other});
    return v;
}

// Independent geometric overlap check (does not reuse the planner's cell logic).
struct RectF { double x0, y0, x1, y1; };
static bool overlaps(RectF a, RectF b) {
    const double eps = 0.5;
    return a.x0 < b.x1 - eps && a.x1 > b.x0 + eps && a.y0 < b.y1 - eps && a.y1 > b.y0 + eps;
}

static RectF placementRect(Placement const& p, ObjectCatalog const& cat, int structureId) {
    float w = 30, h = 30;
    cat.usable(p.id, w, h);
    (void)structureId;
    double pw = w * std::fabs(p.sx), ph = h * std::fabs(p.sy);
    double r = std::fmod(std::fabs(p.rot), 180.0);
    if (r == 0.0) {}
    else if (r == 90.0) std::swap(pw, ph);
    else { double s = std::max(pw, ph) * 1.4142; pw = ph = s; }
    return {p.x - pw / 2, p.y - ph / 2, p.x + pw / 2, p.y + ph / 2};
}

struct Verdict { size_t placed = 0; bool ok = true; };

static Verdict verify(std::vector<InObject> const& level, std::vector<Placement> const& items, ObjectCatalog const& cat,
                      Region const& reg, size_t maxObjects, char const* label) {
    Verdict v;
    v.placed = items.size();
    if (items.size() > maxObjects) { std::printf("  FAIL %s: %zu items > max %zu\n", label, items.size(), maxObjects); v.ok = false; ++g_fail; }
    ++g_checks;
    // Index original objects by coarse cell for a fast independent overlap test.
    std::unordered_map<uint64_t, std::vector<RectF>> grid;
    auto bucket = [](double c) { return int(std::floor(c / 120.0)); };
    for (auto const& o : level) {
        RectF r{o.x - o.w / 2.0, o.y - o.h / 2.0, o.x + o.w / 2.0, o.y + o.h / 2.0};
        for (int by = bucket(r.y0); by <= bucket(r.y1); ++by)
            for (int bx = bucket(r.x0); bx <= bucket(r.x1); ++bx) grid[cellKey(bx, by)].push_back(r);
    }
    size_t overlapCount = 0, outOfRegion = 0;
    for (auto const& p : items) {
        RectF r = placementRect(p, cat, 3092);
        for (int by = bucket(r.y0); by <= bucket(r.y1); ++by)
            for (int bx = bucket(r.x0); bx <= bucket(r.x1); ++bx) {
                auto it = grid.find(cellKey(bx, by));
                if (it == grid.end()) continue;
                for (auto const& o : it->second)
                    if (overlaps(r, o)) {
                        if (++overlapCount <= 2 && std::getenv("ADECO_DEBUG"))
                            std::printf("    overlap: rule=%d id=%d pos=(%.1f,%.1f) rot=%.1f scale=(%.2f,%.2f) rect=[%.1f,%.1f,%.1f,%.1f] vs obj=[%.1f,%.1f,%.1f,%.1f]\n",
                                        p.rule, p.id, p.x, p.y, p.rot, p.sx, p.sy, r.x0, r.y0, r.x1, r.y1, o.x0, o.y0, o.x1, o.y1);
                        goto next;
                    }
            }
    next:
        if (r.x0 < reg.minX * kCell - 0.5 || r.x1 > (reg.maxX + 1) * kCell + 0.5 || r.y0 < reg.minY * kCell - 0.5 || r.y1 > (reg.maxY + 1) * kCell + 0.5)
            ++outOfRegion;
    }
    ++g_checks;
    if (overlapCount) { std::printf("  FAIL %s: %zu placements overlap existing objects\n", label, overlapCount); v.ok = false; ++g_fail; }
    ++g_checks;
    if (outOfRegion) { std::printf("  FAIL %s: %zu placements outside the region\n", label, outOfRegion); v.ok = false; ++g_fail; }
    return v;
}

static uint64_t hashPlan(std::vector<Placement> const& items) {
    uint64_t h = 1469598103934665603ull;
    auto mixIn = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    for (auto const& p : items) {
        mixIn(uint64_t(p.id)); mixIn(uint64_t(std::llround(p.x * 100))); mixIn(uint64_t(std::llround(p.y * 100)));
        mixIn(uint64_t(std::llround(p.rot * 100))); mixIn(uint64_t(std::llround(p.sx * 1000))); mixIn(uint64_t(p.z));
    }
    return h;
}

static std::string readFile(char const* path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ================================================================== tests
static void testColours() {
    SECTION("colours / hex validation");
    Rgb c;
    CHECK(parseHex("#12AbEF", c) && c.r == 0x12 && c.g == 0xAB && c.b == 0xEF);
    CHECK(parseHex("12abef", c));
    CHECK(!parseHex("#12AbE", c));
    CHECK(!parseHex("#12AbEFF", c));
    CHECK(!parseHex("#12AbEG", c));
    CHECK(!parseHex("", c));
    CHECK(!parseHex("#", c));
    CHECK(!parseHex("# 12345", c));
    CHECK(toHex({0, 0xA, 0xFF}) == "#000AFF");
    Palette p;
    p[Role::Background] = {1, 2, 3}; p[Role::Blocks] = {4, 5, 6}; p[Role::Outline] = {7, 8, 9};
    p[Role::Accent] = {10, 11, 12}; p[Role::Glow] = {250, 251, 252};
    Palette q;
    CHECK(paletteFromString(paletteToString(p), q) && q == p);
    CHECK(!paletteFromString("#000000,#000000", q));
    CHECK(!paletteFromString("#000000,#000000,#000000,#000000,#00000Z", q));
    CHECK(mix({0, 0, 0}, {255, 255, 255}, 0.5f) == Rgb({128, 128, 128}));
}

static void testTempo() {
    SECTION("tempo");
    double bpm = 0;
    CHECK(parseBpm("128", bpm) && bpm == 128.0);
    CHECK(parseBpm(" 128.5 ", bpm));
    CHECK(!parseBpm("19.9", bpm));
    CHECK(!parseBpm("401", bpm));
    CHECK(!parseBpm("abc", bpm));
    CHECK(!parseBpm("", bpm));
    CHECK(!parseBpm("1e2", bpm));
    CHECK(!parseBpm("-120", bpm));

    TempoParams tp; tp.bpm = 120;
    auto xs = beatPositions(tp, 0, 5000, {}, 100000);
    CHECK(xs.size() > 30);
    CHECK(std::fabs(xs[0]) < 0.01f);
    CHECK(std::fabs((xs[1] - xs[0]) - 0.5f * 311.58f) < 0.05f);

    // Speed portal at x=1000 switches to 2x.
    std::vector<SpeedChange> ch = {{1000.f, Speed::Fast}};
    double tPortal = 1000.0 / 311.58;
    float expect = float(1000.0 + (4.0 - tPortal) * 387.42);
    CHECK(std::fabs(xAtTime(4.0, Speed::Normal, ch) - expect) < 0.1f);
    CHECK(xAtTime(-3.0, Speed::Normal, ch) == 0.f);

    // offset
    tp.offsetSec = 0.25;
    xs = beatPositions(tp, 0, 2000, {}, 100000);
    CHECK(std::fabs(xs[0] - 0.25f * 311.58f) < 0.05f);
    // negative offset skips beats before the level starts
    tp.offsetSec = -0.1;
    xs = beatPositions(tp, 0, 2000, {}, 100000);
    CHECK(xs[0] >= 0.f);
    // cap + invalid bpm
    tp.offsetSec = 0; tp.bpm = 400;
    CHECK(beatPositions(tp, 0, 1e9f, {}, 123).size() == 123);
    tp.bpm = 5;
    CHECK(beatPositions(tp, 0, 5000, {}, 100).empty());

    CHECK(speedFromPortalId(200) == Speed::Slow);
    CHECK(speedFromPortalId(1334) == Speed::Fastest);
    CHECK(!speedFromPortalId(8).has_value());

    std::vector<double> taps = {10.0, 10.5, 11.0, 11.5};
    auto t = tapTempo(taps);
    CHECK(t && std::fabs(*t - 120.0) < 0.01);
    CHECK(!tapTempo({1.0}).has_value());
}

static void testObjectString() {
    SECTION("object strings");
    ChannelMap map;
    Placement p;
    p.id = 1764; p.x = 45; p.y = 15; p.z = 3; p.zOrder = 4; p.color = Role::Accent;
    CHECK(objectString(p, map, 0) == "1,1764,2,45,3,15,21,903,24,3,25,4;");
    p.rot = 90; p.flipX = true; p.sx = p.sy = 0.5f; p.detail = Role::Glow;
    CHECK(objectString(p, map, 2) == "1,1764,2,45,3,15,6,90,4,1,128,0.5,129,0.5,21,903,22,904,24,3,25,4,20,2;");
    CHECK(colorTriggerString(15, 495, {255, 0, 16}, 901) == "1,899,2,15,3,495,7,255,8,0,9,16,10,0,23,901,35,1;");
    CHECK(pulseTriggerString(100, 200, {1, 2, 3}, 904, 0, 0.03f, 0.2f) ==
          "1,1006,2,100,3,200,7,1,8,2,9,3,45,0,46,0.03,47,0.2,48,0,51,904,52,0;");
    CHECK(fmtNum(-0.0001) == "0");
    CHECK(fmtNum(12.3456) == "12.346");
}

static void testJsonAndPresets() {
    SECTION("preset loading (real resources/presets.json)");
    std::string text = readFile("resources/presets.json");
    CHECK(!text.empty());
    auto r = loadPresetsFromJson(text, "presets.json");
    for (auto const& e : r.errors) std::printf("  load error: %s\n", e.c_str());
    CHECK(r.errors.empty());
    CHECK(r.presets.size() >= 10);
    std::set<std::string> ids, names;
    for (auto const& p : r.presets) {
        CHECK(!p.id.empty() && !p.name.empty() && !p.description.empty());
        CHECK(ids.insert(p.id).second);
        CHECK(names.insert(p.name).second);
        CHECK(!p.rules.empty());
        CHECK(p.preview.size() >= 1);
        for (auto const& rule : p.rules) CHECK(!rule.objects.empty());
    }
    for (char const* want : {"Nightmare", "Candy", "Frost", "Stone", "Lagoon", "Neon Circuit", "Retro", "Sunset", "Forest", "Cyber"})
        CHECK(names.count(want) == 1);

    SECTION("preset loading (malformed input is reported, never fatal)");
    auto bad = loadPresetsFromJson("{ not json", "x");
    CHECK(bad.presets.empty() && bad.errors.size() == 1);
    bad = loadPresetsFromJson("[1,2,]", "x");
    CHECK(bad.presets.empty() && !bad.errors.empty());
    bad = loadPresetsFromJson("{\"presets\": 5}", "x");
    CHECK(bad.presets.empty() && !bad.errors.empty());
    // one good + one with a bad hex colour: the good one still loads
    std::string good = R"({"presets":[
      {"id":"ok","name":"Ok","description":"d","palette":{"background":"#000000","blocks":"#111111","outline":"#222222","accent":"#333333","glow":"#444444"},
       "rules":[{"kind":"edge","objects":[{"id":1764}]}]},
      {"id":"broken","name":"B","description":"d","palette":{"background":"#00000G","blocks":"#111111","outline":"#222222","accent":"#333333","glow":"#444444"},
       "rules":[{"kind":"edge","objects":[{"id":1764}]}]},
      {"id":"Bad Id","name":"B","description":"d"}
    ]})";
    auto mixed = loadPresetsFromJson(good, "x");
    CHECK(mixed.presets.size() == 1 && mixed.presets[0].id == "ok");
    CHECK(mixed.errors.size() == 2);
    // later duplicates override earlier ones
    std::string dup = R"({"presets":[
      {"id":"a","name":"First","description":"d","palette":{"background":"#000000","blocks":"#111111","outline":"#222222","accent":"#333333","glow":"#444444"},"rules":[{"kind":"cap","objects":[{"id":1}]}]},
      {"id":"a","name":"Second","description":"d","palette":{"background":"#000000","blocks":"#111111","outline":"#222222","accent":"#333333","glow":"#444444"},"rules":[{"kind":"cap","objects":[{"id":1}]}]}]})";
    auto d = loadPresetsFromJson(dup, "x");
    CHECK(d.presets.size() == 1 && d.presets[0].name == "Second");
    // numeric clamping
    std::string clampDoc = R"({"presets":[{"id":"c","name":"C","description":"d","palette":{"background":"#000000","blocks":"#111111","outline":"#222222","accent":"#333333","glow":"#444444"},
      "rules":[{"kind":"edge","density":9,"z":99,"objects":[{"id":1764,"scale":[100,0.0001]}]}]}]})";
    auto cl = loadPresetsFromJson(clampDoc, "x");
    CHECK(cl.presets.size() == 1 && cl.presets[0].rules[0].density == 1.0f && cl.presets[0].rules[0].z == 11);
    CHECK(cl.presets[0].rules[0].objects[0].scaleMax <= 8.f && cl.presets[0].rules[0].objects[0].scaleMin >= 0.05f);
}

static void testAnalysis() {
    SECTION("analysis / structure grouping");
    std::vector<InObject> lv;
    auto block = [&](int cx, int cy, ObjClass c = ObjClass::Solid) { lv.push_back({1, cx * kCell + 15, cy * kCell + 15, 30, 30, c}); };
    for (int x = 0; x < 6; ++x) block(x, 0);                 // floor strip: platform (6x1)
    for (int y = 0; y < 5; ++y) block(10, y);                // pillar (1x5)
    for (int x = 20; x < 24; ++x) for (int y = 0; y < 4; ++y) block(x, y);  // 4x4 mass
    lv.push_back({8, 30 * kCell + 15, 15, 30, 30, ObjClass::Hazard});       // lone spike
    lv.push_back({1, 40 * kCell + 15, 6 * kCell + 15, 30, 30, ObjClass::Solid});  // floating block
    lv.push_back({10, 12 * kCell + 15, 15, 30, 60, ObjClass::Other});       // portal: occupied but not structure
    Analysis a = analyse(lv);
    CHECK(a.summary.platforms == 1);
    CHECK(a.summary.pillars == 1);
    CHECK(a.summary.masses == 1);
    CHECK(a.summary.hazardGroups == 1);
    CHECK(a.summary.clusters == 1);
    CHECK(a.summary.total() == 5);
    CHECK(a.occ.occupied(12, 0) && a.occ.occupied(12, 1));     // portal is 2 cells tall
    CHECK(!a.occ.occupied(15, 0));
    CHECK(a.bounds.minX == 0 && a.bounds.maxX == 40);
    // diagonal neighbours join (slopes)
    std::vector<InObject> diag = {{1, 15, 15, 30, 30, ObjClass::Solid}, {1, 45, 45, 30, 30, ObjClass::Solid}};
    CHECK(analyse(diag).comps.size() == 1);

    SECTION("empty level");
    Analysis e = analyse({});
    CHECK(e.empty && e.comps.empty());
    Region r = decorRegion(e, 90);
    CHECK(r.maxX > r.minX && r.maxY >= 14);
}

static void runPresetOn(Preset const& p, std::vector<InObject> const& level, StubCatalog const& cat, int maxObjects,
                        char const* label, bool withStructure, size_t* outPlaced = nullptr) {
    Analysis an = analyse(level);
    Region reg = decorRegion(an, 90);
    PlanLimits lim; lim.maxObjects = maxObjects; lim.region = reg;
    std::vector<Placement> all;
    if (withStructure) {
        Plan s = planStructure(p, an, cat, lim, 77);
        all.insert(all.end(), s.items.begin(), s.items.end());
        lim.maxObjects = std::max(0, maxObjects - int(all.size()));
    }
    Plan d = planDeco(p, an, cat, lim, 77, 1.0f);
    all.insert(all.end(), d.items.begin(), d.items.end());
    std::string tag = std::string(label) + "/" + p.id;
    Verdict v = verify(level, all, cat, reg, size_t(maxObjects), tag.c_str());
    if (outPlaced) *outPlaced += v.placed;
}

static void testPlanner(std::vector<Preset> const& presets) {
    StubCatalog cat;

    SECTION("planner: empty level (every preset)");
    {
        std::vector<InObject> empty;
        size_t total = 0;
        for (auto const& p : presets) runPresetOn(p, empty, cat, 5000, "empty", true, &total);
        CHECK(total > 0);  // an empty level still gets a skyline / backdrop
        std::printf("  placed %zu objects across %zu presets on an empty level\n", total, presets.size());
    }

    SECTION("planner: small hand-built level (every preset)");
    {
        auto lv = makeLevel(120, 8, 0.02, 5);
        size_t total = 0;
        for (auto const& p : presets) runPresetOn(p, lv, cat, 8000, "small", true, &total);
        CHECK(total > 0);
        std::printf("  placed %zu objects (%zu level objects)\n", total, lv.size());
    }

    SECTION("planner: dense 60k-object level (every preset)");
    {
        auto lv = makeLevel(3000, 40, 0.5, 9);
        std::printf("  level objects: %zu\n", lv.size());
        CHECK(lv.size() >= 50000);
        size_t total = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (auto const& p : presets) runPresetOn(p, lv, cat, 20000, "dense", true, &total);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("  analyse+plan+verify x%zu presets: %.0f ms total (%.0f ms/preset)\n", presets.size(), ms, ms / double(presets.size()));
        CHECK(total > 0);
    }

    SECTION("planner: very long sparse level (budget thins out evenly)");
    {
        auto lv = makeLevel(30000, 6, 0.01, 3);
        Preset const& p = presets[0];
        Analysis an = analyse(lv);
        PlanLimits lim; lim.maxObjects = 3000; lim.region = decorRegion(an, 90);
        Plan d = planDeco(p, an, cat, lim, 5, 1.0f);
        CHECK(d.items.size() <= 3000);
        // must reach the far end of the level, not exhaust the budget in the first few screens
        float maxX = 0;
        for (auto const& it : d.items) maxX = std::max(maxX, it.x);
        std::printf("  3000-object budget reached x=%.0f of %d\n", maxX, 30000 * 30);
        CHECK(maxX > 30000 * 30 * 0.5f);
    }

    SECTION("planner: budget edge cases");
    {
        auto lv = makeLevel(200, 10, 0.05, 11);
        Preset const& p = presets[1];
        for (int budget : {0, 1, 7, 50}) {
            Analysis an = analyse(lv);
            PlanLimits lim; lim.maxObjects = budget; lim.region = decorRegion(an, 90);
            Plan s = planStructure(p, an, cat, lim, 1);
            Plan d = planDeco(p, an, cat, lim, 1, 1.0f);
            CHECK(int(s.items.size()) <= budget);
            CHECK(int(d.items.size()) <= budget);
        }
        // density scale
        Analysis a1 = analyse(lv), a2 = analyse(lv);
        PlanLimits lim; lim.maxObjects = 100000; lim.region = decorRegion(a1, 90);
        size_t lo = planDeco(p, a1, cat, lim, 3, 0.25f).items.size();
        size_t hi = planDeco(p, a2, cat, lim, 3, 2.0f).items.size();
        std::printf("  density 0.25x -> %zu objects, 2.0x -> %zu objects\n", lo, hi);
        CHECK(hi > lo);
    }

    SECTION("planner: determinism and variation");
    {
        auto lv = makeLevel(400, 12, 0.08, 21);
        Preset const& p = presets[2];
        auto run = [&](uint64_t seed) {
            Analysis an = analyse(lv);
            PlanLimits lim; lim.maxObjects = 20000; lim.region = decorRegion(an, 90);
            Plan s = planStructure(p, an, cat, lim, seed);
            Plan d = planDeco(p, an, cat, lim, seed, 1.0f);
            s.items.insert(s.items.end(), d.items.begin(), d.items.end());
            return hashPlan(s.items);
        };
        CHECK(run(42) == run(42));
        CHECK(run(42) != run(43));
    }

    SECTION("planner: unusable objects are never emitted");
    {
        auto lv = makeLevel(200, 10, 0.05, 31);
        StubCatalog picky;
        picky.bad = {1764, 1887, 126, 498, 3092};
        for (auto const& p : presets) {
            Analysis an = analyse(lv);
            PlanLimits lim; lim.maxObjects = 20000; lim.region = decorRegion(an, 90);
            Plan s = planStructure(p, an, picky, lim, 9);
            Plan d = planDeco(p, an, picky, lim, 9, 1.0f);
            CHECK(s.items.empty());  // structure object 3092 rejected -> no pieces, but no crash
            for (auto const& it : d.items) CHECK(!picky.bad.count(it.id));
        }
        StubCatalog none; none.rejectAll = true;
        Analysis an = analyse(lv);
        PlanLimits lim; lim.maxObjects = 1000; lim.region = decorRegion(an, 90);
        Plan d = planDeco(presets[0], an, none, lim, 9, 1.0f);
        CHECK(d.items.empty() && !d.stats.notes.empty());
    }

    SECTION("planner: structure supports sit under floating platforms and only in free cells");
    {
        std::vector<InObject> lv;
        for (int x = 5; x < 15; ++x) lv.push_back({1, x * kCell + 15, 8 * kCell + 15, 30, 30, ObjClass::Solid});  // floating platform at y=8
        for (int x = 0; x < 30; ++x) lv.push_back({1, x * kCell + 15, 15, 30, 30, ObjClass::Solid});            // floor
        Preset p = presets[3];
        p.structure.supportDensity = 1.0f; p.structure.skylineDensity = 0.0f;
        Analysis an = analyse(lv);
        PlanLimits lim; lim.maxObjects = 1000; lim.region = decorRegion(an, 90);
        Plan s = planStructure(p, an, cat, lim, 4);
        CHECK(!s.items.empty());
        CHECK(s.stats.structurePieces == int(s.items.size()));
        int coveredCols = 0;
        for (auto const& it : s.items) {
            double top = it.y + 30 * it.sy / 2, bottom = it.y - 30 * it.sy / 2;
            CHECK(top <= 8 * kCell + 0.5);        // never reaches into the platform
            CHECK(bottom >= 1 * kCell - 0.5);     // never reaches into the floor
            coveredCols += int(std::lround(30 * it.sx / kCell));
        }
        CHECK(coveredCols == 10);                 // all 10 platform columns got support
        verify(lv, s.items, cat, lim.region, 1000, "supports");
    }

    SECTION("planner: rhythm");
    {
        auto lv = makeLevel(200, 8, 0.03, 41);
        Preset const& p = presets[0];
        Analysis an = analyse(lv);
        Region reg = decorRegion(an, 90);
        TempoParams tp; tp.bpm = 128;
        auto beats = beatPositions(tp, float(reg.minX) * kCell, float(reg.maxX + 1) * kCell, {}, 5000);
        RhythmPlan rp = planRhythm(p, an, cat, reg, beats, 2000);
        CHECK(!rp.pulses.empty());
        for (auto const& b : rp.pulses) CHECK(b.y > (reg.maxY) * kCell);
        std::vector<Placement> markers = rp.markers;
        verify(lv, markers, cat, reg, 2000, "markers");
        RhythmPlan limited = planRhythm(p, an, cat, reg, beats, 3);
        CHECK(limited.pulses.size() + limited.markers.size() <= 3);
    }
}

int main() {
    std::printf("Auto Decorator core tests\n");
    testColours();
    testTempo();
    testObjectString();
    testJsonAndPresets();
    testAnalysis();

    std::string text = readFile("resources/presets.json");
    auto r = loadPresetsFromJson(text, "presets.json");
    if (r.presets.empty()) { std::printf("no presets loaded; cannot run planner tests\n"); return 1; }
    testPlanner(r.presets);

    std::printf("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
