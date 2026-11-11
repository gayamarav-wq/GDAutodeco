#include "DecoManager.hpp"

#include <algorithm>
#include <cmath>

#include "Catalog.hpp"
#include "Settings.hpp"
#include "core/Analysis.hpp"

using namespace geode::prelude;

// Set to 0 if your SDK headers do not expose GameObject::m_baseColor / m_detailColor (see README "API risks").
// Without the scan the mod falls back to channels 999..995 and warns in the log.
#ifndef ADECO_SCAN_COLOUR_CHANNELS
#define ADECO_SCAN_COLOUR_CHANNELS 1
#endif
// Set to 0 to skip the extra EditorUI::getLimitedPosition() bounds check (see README "API risks").
#ifndef ADECO_USE_LIMITED_POSITION
#define ADECO_USE_LIMITED_POSITION 1
#endif

namespace adeco {

namespace {

constexpr size_t kBatch = 1500;  // objects per createObjectsFromString call

ObjClass classify(GameObject* o) {
    switch (o->m_objectType) {
        case GameObjectType::Solid:
        case GameObjectType::Slope: return ObjClass::Solid;
        case GameObjectType::Hazard: return ObjClass::Hazard;
        case GameObjectType::Decoration: return ObjClass::Deco;
        default: return ObjClass::Other;
    }
}

// Visible part of the level in grid cells. API-RISK: uses GJBaseGameLayer::m_objectLayer's position/scale,
// which is how the editor scrolls and zooms.
Region visibleRegion(LevelEditorLayer* lel) {
    auto layer = lel->m_objectLayer;
    CCPoint pos = layer->getPosition();
    float sc = std::max(0.05f, layer->getScale());
    CCSize win = CCDirector::get()->getWinSize();
    float x0 = -pos.x / sc, y0 = -pos.y / sc;
    float x1 = x0 + win.width / sc, y1 = y0 + win.height / sc;
    Region r;
    r.minX = int(std::floor(x0 / kCell)); r.maxX = int(std::ceil(x1 / kCell));
    r.minY = int(std::floor(y0 / kCell)); r.maxY = int(std::ceil(y1 / kCell));
    return r;
}

}  // namespace

DecoManager& DecoManager::get() {
    static DecoManager m;
    return m;
}

void DecoManager::attach(LevelEditorLayer* lel) {
    if (m_editor == lel) return;
    m_editor = lel;
    m_runs.clear();
    m_structure.clear();
    m_preview.clear();
    m_channelsChosen = false;
    m_variation = 0;
}

std::unordered_set<GameObject*> DecoManager::liveSet(LevelEditorLayer* lel) {
    std::unordered_set<GameObject*> live;
    auto all = lel->getAllObjects();
    if (!all) return live;
    live.reserve(size_t(all->count()) * 2);
    for (auto obj : CCArrayExt<GameObject*>(all)) live.insert(obj);
    return live;
}

bool DecoManager::hasStructure(LevelEditorLayer* lel) {
    if (!lel || m_structure.empty()) return false;
    auto live = liveSet(lel);
    for (auto& s : m_structure)
        if (live.count(s.data())) return true;
    return false;
}

DecoManager::Scan DecoManager::scan(LevelEditorLayer* lel) {
    Scan sc;
    auto all = lel->getAllObjects();
    if (!all) return sc;
    std::unordered_set<GameObject*> structure;
    for (auto& s : m_structure) structure.insert(s.data());

    sc.total = size_t(all->count());
    sc.objects.reserve(sc.total);
    for (auto obj : CCArrayExt<GameObject*>(all)) {
        if (!obj) continue;
        CCPoint p = obj->getPosition();
        CCSize cs = obj->getContentSize();
        InObject o;
        o.id = obj->m_objectID;
        o.x = p.x;
        o.y = p.y;
        o.w = std::fabs(cs.width * obj->getScaleX());
        o.h = std::fabs(cs.height * obj->getScaleY());
        if (o.w < 1.f) o.w = kCell;
        if (o.h < 1.f) o.h = kCell;
        o.cls = structure.count(obj) ? ObjClass::Structure : classify(obj);
        sc.objects.push_back(o);

        if (auto sp = speedFromPortalId(o.id)) sc.speeds.push_back({p.x, *sp});

#if ADECO_SCAN_COLOUR_CHANNELS
        // API-RISK: GameObject::m_baseColor / m_detailColor (GJSpriteColor::m_colorID).
        if (obj->m_baseColor) sc.usedChannels.insert(obj->m_baseColor->m_colorID);
        if (obj->m_detailColor) sc.usedChannels.insert(obj->m_detailColor->m_colorID);
#endif
    }
    std::sort(sc.speeds.begin(), sc.speeds.end(), [](SpeedChange const& a, SpeedChange const& b) { return a.x < b.x; });
    return sc;
}

ChannelMap DecoManager::pickChannels(Scan const& sc) {
    if (!m_channelsChosen) {
        // Take the highest unused custom channels (1..999). Levels almost always use the low numbers, and the
        // scan above guarantees we never reuse a channel that existing objects are coloured with.
        size_t n = 0;
        for (int c = 999; c >= 1 && n < kRoleCount; --c)
            if (!sc.usedChannels.count(c)) m_picked[n++] = c;
        for (; n < kRoleCount; ++n) m_picked[n] = 999 - int(n);  // level uses everything: share (very unlikely)
        m_channelsChosen = true;
    }
    ChannelMap m;
    m.ch = m_picked;
    if (cfg::applyBackground()) m.ch[size_t(Role::Background)] = 1000;  // GD's special BG channel
    return m;
}

uint64_t DecoManager::makeSeed(Preset const& p, Scan const& sc) const {
    uint64_t base = cfg::fixedSeed() != 0 ? uint64_t(uint32_t(cfg::fixedSeed())) : uint64_t(p.seed) + uint64_t(m_variation) * 7919ull;
    return base ^ (uint64_t(sc.total) * 0x9E3779B97F4A7C15ull);
}

int DecoManager::roomForObjects(LevelEditorLayer* lel) const {
    auto all = lel->getAllObjects();
    int existing = all ? int(all->count()) : 0;
    return std::max(0, cfg::objectLimit() - cfg::safetyMargin() - existing);
}

std::vector<Ref<GameObject>> DecoManager::create(LevelEditorLayer* lel, std::vector<std::string> const& strings) {
    std::vector<Ref<GameObject>> out;
    out.reserve(strings.size());
    for (size_t i = 0; i < strings.size(); i += kBatch) {
        std::string chunk;
        size_t end = std::min(strings.size(), i + kBatch);
        for (size_t k = i; k < end; ++k) chunk += strings[k];
        // noUndo = true: we keep our own one-step undo; noLimit = false: the game still enforces its limit.
        CCArray* made = lel->createObjectsFromString(chunk, true, false);
        if (!made) continue;
        for (auto obj : CCArrayExt<GameObject*>(made)) out.push_back(obj);
    }
    return out;
}

std::vector<Ref<GameObject>> DecoManager::applyPalette(LevelEditorLayer* lel, Palette const& pal, ChannelMap const& map,
                                                       Region const& reg) {
    // One colour trigger per palette role, parked in a free-ish spot above the decorated area.
    float x = std::max(15.f, float(reg.minX) * kCell + 15.f);
    float y = (float(reg.maxY) + 2.5f) * kCell;
    std::vector<std::string> strs;
    std::vector<int> channels;
    for (size_t r = 0; r < kRoleCount; ++r) {
        strs.push_back(colorTriggerString(x, y + float(r) * kCell, pal.c[r], map.ch[r]));
        channels.push_back(map.ch[r]);
    }
    auto objs = create(lel, strs);

    // Show the colours in the editor right away. API-RISK: LevelEditorLayer::runColorEffect signature.
    // If it differs, delete this loop: the colour triggers above still colour the level when played or scrolled to.
    if (cfg::liveColourPreview() && objs.size() == channels.size()) {
        for (size_t i = 0; i < objs.size(); ++i)
            if (auto eff = typeinfo_cast<EffectGameObject*>(objs[i].data()))
                lel->runColorEffect(eff, channels[i], 0.f, 0.f, true);
    }
    return objs;
}

void DecoManager::removeAll(LevelEditorLayer* lel, std::vector<Ref<GameObject>>& objs, int* removed) {
    if (objs.empty()) return;
    auto live = liveSet(lel);  // objects the player already deleted by hand must not be removed twice
    int n = 0;
    for (auto it = objs.rbegin(); it != objs.rend(); ++it) {
        GameObject* o = it->data();
        if (o && live.count(o)) {
            lel->removeObject(o, true);
            ++n;
        }
    }
    if (removed) *removed += n;
    objs.clear();
}

// ======================================================================= main entry
RunReport DecoManager::run(LevelEditorLayer* lel, Preset const& p, Palette const& pal, TempoSettings const& tempo,
                           bool doStructure, bool doDeco, bool isPreview) {
    RunReport rep;
    if (!lel || !lel->m_editorUI) { rep.message = "The editor is not ready."; return rep; }
    attach(lel);
    clearPreview(lel);

    Scan sc = scan(lel);
    Analysis an = analyse(sc.objects);
    Region full = decorRegion(an, cfg::maxCellY());
    Region reg = full;
    if (isPreview) {
        Region v = visibleRegion(lel);
        reg = {std::max(full.minX, v.minX), std::min(full.maxX, v.maxX), std::max(full.minY, v.minY), std::min(full.maxY, v.maxY)};
        if (reg.minX > reg.maxX || reg.minY > reg.maxY) { rep.message = "Nothing in view to preview."; return rep; }
    }

    int room = roomForObjects(lel);
    int cap = std::min(room, isPreview ? cfg::previewMaxObjects() : cfg::maxNewObjects());
    const int triggerReserve = 8;
    if (cap <= triggerReserve + 4) {
        rep.message = "Not enough room under the object limit to add decorations.";
        return rep;
    }
    int beatBudget = (!isPreview && tempo.enabled && doDeco) ? std::min(2500, cap / 5) : 0;
    int mainBudget = cap - triggerReserve - beatBudget;

    GeodeCatalog cat;
    uint64_t seed = makeSeed(p, sc);
    ChannelMap map = pickChannels(sc);

    std::vector<Placement> placements;
    Plan structurePlan;
    // Pieces the player already has (and may have edited) are kept: never regenerate them under a decorate/preview.
    bool wantStructure = doStructure && !(doDeco && hasStructure(lel));
    if (wantStructure) {
        PlanLimits lim;
        lim.region = reg;
        lim.maxObjects = doDeco ? int(mainBudget * 0.35) : mainBudget;
        structurePlan = planStructure(p, an, cat, lim, seed);
        for (auto& n : structurePlan.stats.notes) rep.notes.push_back(n);
        placements.insert(placements.end(), structurePlan.items.begin(), structurePlan.items.end());
    }
    Plan decoPlan;
    if (doDeco) {
        PlanLimits lim;
        lim.region = reg;
        lim.maxObjects = std::max(0, mainBudget - int(placements.size()));
        decoPlan = planDeco(p, an, cat, lim, seed, cfg::densityScale());
        for (auto& n : decoPlan.stats.notes) rep.notes.push_back(n);
        placements.insert(placements.end(), decoPlan.items.begin(), decoPlan.items.end());
    }

    // Optional tempo sync (full runs only).
    RhythmPlan rhythm;
    if (beatBudget > 0) {
        TempoParams tp = tempo.params;
        auto beats = beatPositions(tp, float(reg.minX) * kCell, float(reg.maxX + 1) * kCell, sc.speeds, size_t(beatBudget) * 2);
        rhythm = planRhythm(p, an, cat, reg, beats, beatBudget);
        for (auto& n : rhythm.notes) rep.notes.push_back(n);
        rep.bpm = tp.bpm;
    }

    // ---- placements -> object strings (with an out-of-bounds guard) ----
    int layer = cfg::decoEditorLayer();
    std::vector<std::string> structStrs, decoStrs, pulseStrs;
    int skippedBounds = 0;
    auto inBounds = [&](float x, float y) {
#if ADECO_USE_LIMITED_POSITION
        CCPoint lp = lel->m_editorUI->getLimitedPosition({x, y});  // API-RISK: EditorUI::getLimitedPosition
        return std::fabs(lp.x - x) < 0.5f && std::fabs(lp.y - y) < 0.5f;
#else
        return x >= 0.f && y >= 0.f;
#endif
    };
    for (auto const& pl : placements) {
        if (!inBounds(pl.x, pl.y)) { ++skippedBounds; continue; }
        (pl.rule == -1 ? structStrs : decoStrs).push_back(objectString(pl, map, layer));
    }
    for (auto const& m : rhythm.markers) {
        if (!inBounds(m.x, m.y)) { ++skippedBounds; continue; }
        decoStrs.push_back(objectString(m, map, layer));
    }
    Rgb pulseCol = mix(pal[Role::Glow], {255, 255, 255}, p.rhythm.brighten);
    for (auto const& b : rhythm.pulses) {
        if (!inBounds(b.x, b.y)) { ++skippedBounds; continue; }
        pulseStrs.push_back(pulseTriggerString(b.x, b.y, pulseCol, map.ch[size_t(Role::Glow)], p.rhythm.fadeIn,
                                               p.rhythm.hold, p.rhythm.fadeOut));
    }
    if (skippedBounds) rep.notes.push_back(std::to_string(skippedBounds) + " object(s) skipped: outside the editor bounds");

    // ---- create (separate calls per kind, so the counts we report are exactly what the game created) ----
    std::vector<Ref<GameObject>> newStructure = create(lel, structStrs);
    std::vector<Ref<GameObject>> decoObjs = create(lel, decoStrs);
    std::vector<Ref<GameObject>> pulseObjs = create(lel, pulseStrs);
    std::vector<Ref<GameObject>> trig = applyPalette(lel, pal, map, reg);
    int structurePieces = int(newStructure.size()), deco = int(decoObjs.size()), pulses = int(pulseObjs.size());
    std::vector<Ref<GameObject>> created = newStructure;
    created.insert(created.end(), decoObjs.begin(), decoObjs.end());
    created.insert(created.end(), pulseObjs.begin(), pulseObjs.end());

    if (isPreview) {
        m_preview = created;
        for (auto& t : trig) m_preview.push_back(t);
        rep.ok = true;
        rep.structurePieces = structurePieces;
        rep.decoObjects = deco;
        rep.message = "Static preview";
        return rep;
    }

    if (!newStructure.empty()) m_structure = newStructure;

    Run r;
    r.objects = created;
    for (auto& t : trig) r.objects.push_back(t);
    r.label = doDeco ? p.name + " decoration" : p.name + " structure layout";
    m_runs.push_back(std::move(r));
    if (m_runs.size() > 8) m_runs.erase(m_runs.begin());

    rep.ok = true;
    rep.structurePieces = structurePieces;
    rep.decoObjects = deco;
    rep.triggers = int(trig.size()) + pulses;
    if (!doDeco) {
        rep.message = std::to_string(structurePieces) + " structure pieces added. Edit them if you like, then decorate.";
    } else {
        rep.message = std::to_string(structurePieces) + " structure pieces and " + std::to_string(deco) + " decoration objects added";
        if (rep.bpm > 0) rep.message += " (synced to " + fmtNum(std::round(rep.bpm * 10) / 10) + " BPM)";
        rep.message += ".";
    }
    if (structurePieces == 0 && deco == 0) {
        rep.message = "Nothing could be added - see Tools > Verify presets.";
        rep.ok = false;
    }
    return rep;
}

RunReport DecoManager::buildStructure(LevelEditorLayer* lel, Preset const& p, Palette const& pal) {
    // Rebuilding replaces the old pieces, so forget them before planning (they must not count as occupied).
    if (lel) { attach(lel); auto old = m_structure; removeAll(lel, old); m_structure.clear(); }
    return run(lel, p, pal, {}, true, false, false);
}

RunReport DecoManager::decorate(LevelEditorLayer* lel, Preset const& p, Palette const& pal, TempoSettings const& tempo) {
    return run(lel, p, pal, tempo, true, true, false);
}

RunReport DecoManager::preview(LevelEditorLayer* lel, Preset const& p, Palette const& pal) {
    return run(lel, p, pal, {}, true, true, true);
}

void DecoManager::clearPreview(LevelEditorLayer* lel) {
    if (m_preview.empty()) return;
    if (lel && lel == m_editor) removeAll(lel, m_preview);
    else m_preview.clear();
}

bool DecoManager::undoLast(LevelEditorLayer* lel, std::string& message) {
    if (!lel || m_runs.empty()) { message = "Nothing to undo."; return false; }
    attach(lel);
    Run r = std::move(m_runs.back());
    m_runs.pop_back();
    int removed = 0;
    removeAll(lel, r.objects, &removed);
    // Drop structure pieces that no longer exist.
    auto live = liveSet(lel);
    m_structure.erase(std::remove_if(m_structure.begin(), m_structure.end(), [&](Ref<GameObject>& g) { return !live.count(g.data()); }), m_structure.end());
    ++m_variation;
    message = "Undid \"" + r.label + "\" (" + std::to_string(removed) + " objects removed).";
    return true;
}

bool DecoManager::removeStructure(LevelEditorLayer* lel, int& removed) {
    removed = 0;
    if (!lel || m_structure.empty()) return false;
    auto old = m_structure;
    removeAll(lel, old, &removed);
    m_structure.clear();
    return removed > 0;
}

}  // namespace adeco
