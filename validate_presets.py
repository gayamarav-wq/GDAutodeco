#!/usr/bin/env python3
"""Validate an Auto Decorator presets file without launching the game.

Usage:  python3 tools/validate_presets.py resources/presets.json [more.json ...]

Mirrors the rules in src/core/Preset.cpp (the in-game loader). Errors = the in-game loader would skip that preset.
Warnings = values the loader would silently clamp. Exit status 1 if any error was found.
"""
import json, re, sys

ROLES = {"background", "blocks", "outline", "accent", "glow"}
KINDS = {"edge", "cap", "glow", "hang", "scatter", "backdrop"}
SIDES = {"top", "right", "bottom", "left"}
NEAR = {"solid", "hazard", "any"}
HEX = re.compile(r"^#?[0-9A-Fa-f]{6}$")
ID = re.compile(r"^[a-z0-9_-]{1,48}$")

def validate(path):
    errors, warnings = [], []
    try:
        with open(path, encoding="utf-8") as f:
            doc = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        return [f"{path}: cannot parse: {e}"], []
    presets = doc if isinstance(doc, list) else doc.get("presets") if isinstance(doc, dict) else None
    if not isinstance(presets, list):
        return [f"{path}: expected an array, or an object with a 'presets' array"], []

    seen = set()
    for i, p in enumerate(presets):
        who = p.get("id", f"#{i}") if isinstance(p, dict) else f"#{i}"
        def err(m): errors.append(f"{path}: preset {who}: {m}")
        def warn(m): warnings.append(f"{path}: preset {who}: {m}")
        if not isinstance(p, dict): err("not an object"); continue
        if not ID.match(str(p.get("id", ""))): err("'id' must be 1-48 chars of a-z 0-9 _ -"); continue
        if p["id"] in seen: warn("duplicate id in this file (the later one wins)")
        seen.add(p["id"])
        if not p.get("name"): err("'name' is required"); continue
        if not p.get("description"): err("'description' is required"); continue
        pal = p.get("palette")
        if not isinstance(pal, dict): err("missing 'palette'"); continue
        bad = False
        for r in ("background", "blocks", "outline", "accent", "glow"):
            if not isinstance(pal.get(r), str) or not HEX.match(pal[r]):
                err(f"palette.{r} must be #RRGGBB"); bad = True
        if bad: continue
        rules = p.get("rules")
        if not isinstance(rules, list) or not rules: err("'rules' must be a non-empty array"); continue
        if len(rules) > 24: err("too many rules (max 24)"); continue
        for ri, r in enumerate(rules):
            def rerr(m): err(f"rules[{ri}]: {m}")
            if not isinstance(r, dict): rerr("not an object"); continue
            if r.get("kind") not in KINDS: rerr(f"kind must be one of {sorted(KINDS)}"); continue
            if "near" in r and r["near"] not in NEAR: rerr("near must be solid, hazard or any")
            if "sides" in r and (not isinstance(r["sides"], list) or not r["sides"] or not set(r["sides"]) <= SIDES):
                rerr("sides must be a non-empty list of top/right/bottom/left")
            d = r.get("density", 0.3)
            if not isinstance(d, (int, float)) or not 0 <= d <= 1: warn(f"rules[{ri}].density {d!r} will be clamped to 0..1")
            z = r.get("z", 1)
            if not isinstance(z, int) or not -5 <= z <= 11: warn(f"rules[{ri}].z {z!r} will be clamped to -5..11")
            objs = r.get("objects")
            if not isinstance(objs, list) or not objs: rerr("'objects' must be a non-empty array"); continue
            for oi, o in enumerate(objs):
                if not isinstance(o, dict) or not isinstance(o.get("id"), int) or not 1 <= o["id"] <= 99999:
                    rerr(f"objects[{oi}] needs an integer 'id' (1-99999)"); continue
                for k in ("color", "detail"):
                    if k in o and o[k] not in ROLES: rerr(f"objects[{oi}].{k} must be one of {sorted(ROLES)}")
                if o.get("fit") and not (isinstance(o.get("span"), list) and len(o["span"]) >= 2 and min(o["span"][:2]) >= 1):
                    rerr(f"objects[{oi}]: 'fit' needs a 'span' of at least [1,1]")
        st = p.get("structure", {})
        if "color" in st and st["color"] not in ROLES: err("structure.color must be a palette role")
        mk = p.get("rhythm", {}).get("marker", {})
        if "color" in mk and mk["color"] not in ROLES: err("rhythm.marker.color must be a palette role")
    return errors, warnings

def main(argv):
    if len(argv) < 2:
        print(__doc__); return 2
    total_err = 0
    for path in argv[1:]:
        errors, warnings = validate(path)
        for w in warnings: print("warning:", w)
        for e in errors: print("ERROR:", e)
        total_err += len(errors)
        if not errors: print(f"{path}: OK")
    return 1 if total_err else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
