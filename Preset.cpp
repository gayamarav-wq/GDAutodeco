#include "Preset.hpp"

#include <algorithm>
#include <cmath>

#include "Json.hpp"

namespace adeco {
namespace {

using json::Value;

// ------------------------------------------------------------ small accessors
double num(Value const* v, double def) { return (v && v->isNumber()) ? v->n : def; }
int integer(Value const* v, int def) { return (v && v->isNumber()) ? int(std::lround(v->n)) : def; }
bool boolean(Value const* v, bool def) { return (v && v->isBool()) ? v->b : def; }
std::string text(Value const* v, std::string def = {}) { return (v && v->isString()) ? v->s : def; }

// A number "x" or an array [lo, hi]. Leaves lo/hi untouched when absent or malformed.
void range(Value const* v, double& lo, double& hi) {
    if (!v) return;
    if (v->isNumber()) { lo = hi = v->n; return; }
    if (v->isArray() && v->arr.size() >= 1 && v->arr[0].isNumber()) {
        lo = v->arr[0].n;
        hi = (v->arr.size() >= 2 && v->arr[1].isNumber()) ? v->arr[1].n : lo;
        if (hi < lo) std::swap(lo, hi);
    }
}
void irange(Value const* v, int& lo, int& hi) {
    double a = lo, b = hi;
    range(v, a, b);
    lo = int(std::lround(a));
    hi = int(std::lround(b));
}

template <class T> T clampT(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool validId(std::string const& id) {
    if (id.empty() || id.size() > 48) return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

std::optional<Role> roleOf(Value const* v) {
    if (!v || !v->isString()) return std::nullopt;
    return roleFromKey(v->s);
}

// ------------------------------------------------------------ sections
bool parseObject(Value const& v, DecoObject& o, std::string& err) {
    if (!v.isObject()) { err = "object entry is not an object"; return false; }
    Value const* id = v.find("id");
    if (!id || !id->isNumber() || id->n < 1 || id->n > 99999) { err = "object entry needs a numeric \"id\" (1-99999)"; return false; }
    o.id = int(id->n);
    o.weight = float(clampT(num(v.find("weight"), 1.0), 0.01, 1000.0));
    double lo = 1, hi = 1;
    range(v.find("scale"), lo, hi);
    o.scaleMin = float(clampT(lo, 0.05, 8.0));
    o.scaleMax = float(clampT(hi, 0.05, 8.0));
    int rlo = 0, rhi = 0;
    irange(v.find("rot"), rlo, rhi);
    o.rotMin = clampT(rlo, -360, 360);
    o.rotMax = clampT(rhi, -360, 360);
    std::string flip = text(v.find("flip"), "none");
    o.flipX = flip.find('x') != std::string::npos;
    o.flipY = flip.find('y') != std::string::npos;
    if (Value const* c = v.find("color")) {
        auto r = roleOf(c);
        if (!r) { err = "unknown colour role \"" + text(c) + "\""; return false; }
        o.color = *r;
    }
    if (Value const* d = v.find("detail")) {
        auto r = roleOf(d);
        if (!r) { err = "unknown detail colour role \"" + text(d) + "\""; return false; }
        o.detail = *r;
    }
    if (Value const* s = v.find("span"); s && s->isArray() && s->arr.size() >= 2) {
        o.spanW = clampT(integer(&s->arr[0], 0), 0, 12);
        o.spanH = clampT(integer(&s->arr[1], 0), 0, 12);
    }
    o.fit = boolean(v.find("fit"), false);
    if (o.fit && (o.spanW < 1 || o.spanH < 1)) { err = "\"fit\" needs a \"span\" of at least [1,1]"; return false; }
    return true;
}

bool parseRule(Value const& v, Rule& r, std::string& err) {
    if (!v.isObject()) { err = "rule is not an object"; return false; }
    std::string kind = text(v.find("kind"));
    r.near = Near::Solid;
    if (kind == "edge") r.kind = RuleKind::Edge;
    else if (kind == "cap") { r.kind = RuleKind::Edge; r.sides = SideTop; r.orient = true; }
    else if (kind == "glow") { r.kind = RuleKind::Edge; r.near = Near::Hazard; }
    else if (kind == "hang") { r.kind = RuleKind::Hang; r.sides = SideBottom; }
    else if (kind == "scatter") r.kind = RuleKind::Scatter;
    else if (kind == "backdrop") r.kind = RuleKind::Backdrop;
    else { err = "unknown rule kind \"" + kind + "\" (edge, cap, glow, hang, scatter, backdrop)"; return false; }

    r.name = text(v.find("name"), kind);
    r.density = float(clampT(num(v.find("density"), 0.3), 0.0, 1.0));
    r.z = clampT(integer(v.find("z"), 1), -5, 11);
    r.zOrder = clampT(integer(v.find("zOrder"), 0), -100, 100);
    r.orient = boolean(v.find("orient"), r.orient);
    r.maxCount = clampT(integer(v.find("maxCount"), 0), 0, 1000000);
    r.radius = clampT(integer(v.find("radius"), 3), 1, 8);
    r.strideX = r.strideY = 4;
    if (Value const* st = v.find("stride")) {
        if (st->isNumber()) r.strideX = r.strideY = clampT(integer(st, 4), 1, 60);
        else if (st->isArray() && st->arr.size() >= 2) {
            r.strideX = clampT(integer(&st->arr[0], 4), 1, 60);
            r.strideY = clampT(integer(&st->arr[1], 4), 1, 60);
        }
    }
    irange(v.find("length"), r.lengthMin, r.lengthMax);
    r.lengthMin = clampT(r.lengthMin, 1, 30);
    r.lengthMax = clampT(r.lengthMax, r.lengthMin, 30);

    if (Value const* n = v.find("near")) {
        std::string s = text(n);
        if (s == "solid") r.near = Near::Solid;
        else if (s == "hazard") r.near = Near::Hazard;
        else if (s == "any") r.near = Near::Any;
        else { err = "\"near\" must be solid, hazard or any"; return false; }
    }
    if (Value const* sd = v.find("sides")) {
        if (!sd->isArray()) { err = "\"sides\" must be an array"; return false; }
        int mask = 0;
        for (auto const& e : sd->arr) {
            std::string s = text(&e);
            if (s == "top") mask |= SideTop;
            else if (s == "right") mask |= SideRight;
            else if (s == "bottom") mask |= SideBottom;
            else if (s == "left") mask |= SideLeft;
            else { err = "unknown side \"" + s + "\""; return false; }
        }
        if (mask == 0) { err = "\"sides\" is empty"; return false; }
        r.sides = mask;
    }

    Value const* objs = v.find("objects");
    if (!objs || !objs->isArray() || objs->arr.empty()) { err = "rule \"" + r.name + "\" has no \"objects\""; return false; }
    for (size_t i = 0; i < objs->arr.size(); ++i) {
        DecoObject o;
        std::string e;
        if (!parseObject(objs->arr[i], o, e)) { err = "rule \"" + r.name + "\" objects[" + std::to_string(i) + "]: " + e; return false; }
        r.objects.push_back(o);
    }
    return true;
}

bool parseStructure(Value const& v, StructureSpec& s, std::string& err) {
    if (!v.isObject()) { err = "\"structure\" must be an object"; return false; }
    s.enabled = boolean(v.find("enabled"), true);
    s.id = clampT(integer(v.find("id"), s.id), 1, 99999);
    if (Value const* c = v.find("color")) {
        auto r = roleOf(c);
        if (!r) { err = "structure: unknown colour role"; return false; }
        s.color = *r;
    }
    s.z = clampT(integer(v.find("z"), s.z), -5, 11);
    s.zOrder = clampT(integer(v.find("zOrder"), s.zOrder), -100, 100);
    s.supportDensity = float(clampT(num(v.find("supportDensity"), s.supportDensity), 0.0, 1.0));
    s.skylineDensity = float(clampT(num(v.find("skylineDensity"), s.skylineDensity), 0.0, 1.0));
    irange(v.find("width"), s.widthMin, s.widthMax);
    irange(v.find("height"), s.heightMin, s.heightMax);
    irange(v.find("gap"), s.gapMin, s.gapMax);
    s.widthMin = clampT(s.widthMin, 1, 12);   s.widthMax = clampT(s.widthMax, s.widthMin, 12);
    s.heightMin = clampT(s.heightMin, 1, 40); s.heightMax = clampT(s.heightMax, s.heightMin, 40);
    s.gapMin = clampT(s.gapMin, 0, 20);       s.gapMax = clampT(s.gapMax, s.gapMin, 20);
    return true;
}

bool parseRhythm(Value const& v, RhythmSpec& r, std::string& err) {
    if (!v.isObject()) { err = "\"rhythm\" must be an object"; return false; }
    r.enabled = boolean(v.find("enabled"), true);
    r.pulseEveryBeats = clampT(integer(v.find("pulseEveryBeats"), 1), 1, 64);
    r.fadeIn = float(clampT(num(v.find("fadeIn"), 0.0), 0.0, 4.0));
    r.hold = float(clampT(num(v.find("hold"), 0.03), 0.0, 4.0));
    r.fadeOut = float(clampT(num(v.find("fadeOut"), 0.2), 0.0, 4.0));
    r.brighten = float(clampT(num(v.find("brighten"), 0.5), 0.0, 1.0));
    if (Value const* m = v.find("marker")) {
        if (!m->isObject()) { err = "rhythm.marker must be an object"; return false; }
        r.marker.id = clampT(integer(m->find("id"), 0), 0, 99999);
        if (Value const* c = m->find("color")) {
            auto ro = roleOf(c);
            if (!ro) { err = "rhythm.marker: unknown colour role"; return false; }
            r.marker.color = *ro;
        }
        r.marker.everyBeats = clampT(integer(m->find("everyBeats"), 1), 1, 64);
        r.marker.row = clampT(integer(m->find("row"), 0), 0, 200);
        r.marker.z = clampT(integer(m->find("z"), 3), -5, 11);
        r.marker.scale = float(clampT(num(m->find("scale"), 0.6), 0.05, 4.0));
    }
    return true;
}

bool parsePalette(Value const* v, Palette& p, std::string& err) {
    if (!v || !v->isObject()) { err = "missing \"palette\" object"; return false; }
    for (size_t i = 0; i < kRoleCount; ++i) {
        Value const* c = v->find(kRoleKeys[i]);
        if (!c || !c->isString()) { err = std::string("palette.") + kRoleKeys[i] + " missing"; return false; }
        if (!parseHex(c->s, p.c[i])) { err = std::string("palette.") + kRoleKeys[i] + " is not #RRGGBB: \"" + c->s + "\""; return false; }
    }
    return true;
}

}  // namespace

LoadResult loadPresetsFromJson(std::string_view textIn, std::string_view sourceName) {
    LoadResult out;
    auto prefix = [&](std::string const& m) { return std::string(sourceName) + ": " + m; };

    auto parsed = json::parse(textIn);
    if (!parsed.ok) { out.errors.push_back(prefix("JSON error at " + parsed.error)); return out; }

    Value const& root = parsed.value;
    Value const* list = root.isArray() ? &root : root.find("presets");
    if (!list || !list->isArray()) { out.errors.push_back(prefix("expected an array or an object with a \"presets\" array")); return out; }

    for (size_t idx = 0; idx < list->arr.size(); ++idx) {
        Value const& pv = list->arr[idx];
        Preset p;
        std::string err;
        auto bad = [&](std::string const& why) {
            std::string who = p.id.empty() ? "#" + std::to_string(idx) : p.id;
            out.errors.push_back(prefix("preset " + who + ": " + why));
        };
        if (!pv.isObject()) { bad("not an object"); continue; }

        p.id = text(pv.find("id"));
        if (!validId(p.id)) { bad("\"id\" must be 1-48 chars of a-z 0-9 _ -"); continue; }
        p.name = text(pv.find("name"));
        if (p.name.empty()) { bad("\"name\" is required"); continue; }
        p.description = text(pv.find("description"));
        if (p.description.empty()) { bad("\"description\" is required"); continue; }
        if (!parsePalette(pv.find("palette"), p.palette, err)) { bad(err); continue; }
        p.seed = uint32_t(clampT(integer(pv.find("seed"), 1), 0, 2000000000));

        if (Value const* pr = pv.find("preview"); pr && pr->isArray())
            for (auto const& e : pr->arr)
                if (e.isNumber() && e.n >= 1 && e.n <= 99999 && p.preview.size() < 6) p.preview.push_back(int(e.n));

        if (Value const* st = pv.find("structure"); st && !parseStructure(*st, p.structure, err)) { bad(err); continue; }
        if (Value const* rh = pv.find("rhythm"); rh && !parseRhythm(*rh, p.rhythm, err)) { bad(err); continue; }

        Value const* rules = pv.find("rules");
        if (!rules || !rules->isArray() || rules->arr.empty()) { bad("\"rules\" must be a non-empty array"); continue; }
        bool ruleFail = false;
        for (size_t r = 0; r < rules->arr.size() && !ruleFail; ++r) {
            Rule rule;
            if (!parseRule(rules->arr[r], rule, err)) { bad("rules[" + std::to_string(r) + "]: " + err); ruleFail = true; break; }
            p.rules.push_back(std::move(rule));
        }
        if (ruleFail) continue;
        if (p.rules.size() > 24) { bad("too many rules (max 24)"); continue; }

        // Later presets with the same id replace earlier ones (user files override built-ins).
        auto dup = std::find_if(out.presets.begin(), out.presets.end(), [&](Preset const& q) { return q.id == p.id; });
        if (dup != out.presets.end()) *dup = std::move(p);
        else out.presets.push_back(std::move(p));
    }
    return out;
}

}  // namespace adeco
