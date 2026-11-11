# Auto Decorator

A [Geode](https://geode-sdk.org) mod for **Geometry Dash 2.2081** that decorates your level layout with one click.
Open the level editor, press **Auto Deco** next to Build / Edit / Delete, pick a style, and the empty space around
your layout is filled with matching decoration. Your own objects are never moved, changed or covered.

| | |
|---|---|
| Game version | Geometry Dash **2.2081** |
| Geode | loader/SDK **v5.x** (manifest requires `5.10.1`; `.github/workflows/build.yml` pins `v5.10.1`) |
| Platforms | Windows, macOS, Android (32 + 64 bit). iOS is not packaged. |
| Language | C++23 (Geode v5 requirement). `src/core` is plain C++20 with no Geode dependency. |
| Network / data | None. Offline, collects nothing. Custom palettes live in the mod's save data, never in the level. |

> **Do not use Geode v6.** v6 targets a newer Geometry Dash than 2.2081. That is why the CI workflow pins the SDK
> version instead of using `latest`.

## Honest status (read this first)

This project was written without access to the Geode SDK, a compiler toolchain for it, or Geometry Dash. What has and
has not been verified:

| Part | Verified how |
|---|---|
| Planning engine (`src/core`: grid analysis, structure grouping, layout rules, tempo maths, preset parser, object strings) | **Unit-tested** (`bash tests/run_tests.sh`, 2354 checks, clean under ASan + UBSan). |
| `resources/presets.json`, `mod.json` | Parsed by the real loader in the tests / `tools/validate_presets.py`. |
| Geode glue (`src/*.cpp`, `src/ui/*.cpp`: hooks, popups, object creation, undo) | **Type-checked only against a hand-written stub** of the Geode API (`bash tests/syntax_check.sh`). That proves the files are consistent with each other. It does **not** prove the real SDK matches my assumptions. |
| The compiled `.geode` for Windows / macOS / Android | **Not built.** No SDK/toolchain was available. Build it with the steps below. |
| Behaviour inside GD 2.2081 (button placement, colours, undo, mobile layout) | **Not run.** Use the test checklist at the end. |

Expect a first `geode build` to surface a few compile errors where my memory of an API differs from your SDK headers.
Every such spot is tagged `API-RISK` in the source and listed in [API risks](#api-risks); several have a one-line
`#define` fallback.

## Using it

1. Open a level in the editor. Press **Auto Deco** (right of the Build / Edit / Delete column; nudge it in the mod
   settings if it overlaps something).
2. **Change style** opens a bar at the top of the editor: `<` `>` flip through styles, a static preview is placed on the
   section in view (scroll and zoom freely), **Use this style** keeps it, **Cancel** discards.
3. **Colours** edits Background / Blocks / Outline / Accent / Glow. Each has a swatch (click for the colour picker), a
   validated `#RRGGBB` field, and a live thumbnail. `< >` switches between **Original colours** and your **Custom colours**
   slot; **Reset to preset** restores the preset's own colours; **Done** saves, **Cancel** discards.
4. **Structure layout** (optional) adds grey pillars and walls - supports under floating platforms and a background
   skyline - and says *"N structure pieces added. Edit them if you like, then decorate."* They are ordinary objects:
   move or delete them.
5. **Decorate level** fills the rest. If you skipped step 4 it builds the structure first, so it is genuinely one click.
   It reports *"N structure pieces and M decoration objects added."*
6. **Tools** has **Undo** (removes the last structure/decoration run in one step), tempo sync, remove structure pieces,
   verify / reload presets and the mod settings.

Undo then Decorate again gives a new variation (results are otherwise repeatable for the same level and preset).

### Guarantees

* Added objects only occupy grid cells that no existing object covers (solids, hazards, portals, triggers and your own
  decoration all count as occupied) - verified by an independent geometric overlap test on 60k-object levels.
* Added objects stay inside the decorated region and the editor bounds, and never push the level past its object limit
  (default 80,000, minus a safety margin; both configurable).
* Only objects that exist in your game **and** are non-colliding decorations are ever placed, so a wrong ID in a preset
  can never change how a level plays (it is skipped instead).
* Empty levels, levels without a song, and 50k+ object levels are handled (see test checklist).

## Install (players)

Put `yourname.auto_decorator.geode` into `<Geometry Dash>/geode/mods/` and restart, or use `geode package install`.

## Build and package (developers)

Prerequisites: [Geode CLI](https://github.com/geode-sdk/cli), the Geode SDK at **v5.x**, CMake 3.21+, and the toolchain
for each target (MSVC or clang-cl on Windows, Xcode on macOS, Android NDK for Android).

```bash
geode sdk install            # once; make sure GEODE_SDK points at an SDK checkout at tag v5.10.1
geode sdk install-binaries   # loader/bindings binaries for your platform

# 1) run the off-game checks
bash tests/run_tests.sh      # core logic
bash tests/syntax_check.sh   # glue vs. stub (internal consistency only)
python3 tools/validate_presets.py resources/presets.json

# 2) build + package for the machine you are on
geode build                  # CMake build + creates yourname.auto_decorator.geode in build/
```

`geode build --help` lists the platform flag for cross targets (Android needs `ANDROID_NDK_ROOT`).

**One `.geode` containing all platforms.** A `.geode` holds one binary per platform, and each is built on its own
toolchain, so the supported route is CI: `.github/workflows/build.yml` builds Windows, macOS, Android32 and Android64
with `geode-sdk/build-geode-mod` and merges them with its `combine` action into a single `.geode`. Doing it by hand:
build on each platform, then merge with the CLI (`geode package merge a.geode b.geode ...` - confirm with
`geode package --help` on your CLI version).

## Preset format

Presets are data. They are loaded from:

1. the bundled `resources/presets.json`, then
2. every `*.json` in `<Geode save dir>/yourname.auto_decorator/presets/` (created on first launch; **Tools > Verify
   presets** prints the exact path). A user preset with the same `id` replaces a bundled one.

A file is either `{ "presets": [ ... ] }` or a bare array. A broken preset is reported (Tools > Verify presets, and the
game log) and skipped; it never stops other presets or the game from loading.

```jsonc
{
  "id": "nightmare",                 // required: a-z 0-9 _ -  (max 48)
  "name": "Nightmare",               // required
  "description": "Blood-red thorns...", // required, shown in the picker
  "seed": 1101,                      // optional: base for repeatable randomness
  "palette": {                       // required: the default colours, all #RRGGBB
    "background": "#14060B", "blocks": "#2A0A12", "outline": "#FF2B3A", "accent": "#8C1020", "glow": "#FF5A4D"
  },
  "preview": [126, 498, 1886],       // optional: object IDs whose sprites are drawn in the thumbnail
  "structure": {                     // optional: the grey pillars/walls phase
    "id": 3092, "color": "blocks", "z": -3, "zOrder": -6,
    "supportDensity": 0.8, "skylineDensity": 0.55,
    "width": [2, 4], "height": [3, 10], "gap": [1, 4]
  },
  "rules": [ /* see below, 1..24 rules */ ],
  "rhythm": {                        // optional: used only when tempo sync is on
    "pulseEveryBeats": 2, "fadeIn": 0, "hold": 0.02, "fadeOut": 0.25, "brighten": 0.4,
    "marker": { "id": 1764, "color": "glow", "everyBeats": 2, "row": 0, "scale": 0.6, "z": 3 }
  }
}
```

### Rules

Each rule has a `kind`, a `density` (0-1, multiplied by the *Decoration density* setting), a GD z layer `z`
(B4 = -3, B3 = -1, B2 = 1, B1 = 3, T1 = 5 ...), a `zOrder`, and a weighted list of `objects`.

| `kind` | What it does |
|---|---|
| `edge` | Free cells touching a structure. `sides`: any of `top right bottom left`. `orient: true` rotates objects outward (0/90/180/270). `near`: `solid` (default), `hazard`, `any`. |
| `cap` | `edge` on the top side only, oriented upward (grass, thorns, flames). |
| `glow` | `edge` hugging hazards (spikes, saws). |
| `hang` | Vertical strings from the underside of structures; `length: [min, max]` cells (chains, vines, icicles). |
| `scatter` | Loose ornaments within `radius` cells (1-8) of structures; across the whole level if it has none. |
| `backdrop` | Big pieces on a coarse grid, `stride: [x, y]` cells, in empty space (clouds, moons, gears). |

An object entry: `{ "id": 1887, "color": "glow", "weight": 3, "scale": [0.6, 1.0], "rot": [0, 90], "flip": "xy",
"detail": "accent", "span": [2, 2], "fit": false }`. `color`/`detail` are palette roles (`background blocks outline
accent glow`), so recolouring a palette recolours every object. `span` is a minimum footprint in cells (it is otherwise
derived from the real sprite size); `fit: true` stretches the sprite to fill `span`. Optional rule keys: `radius`,
`stride`, `length`, `maxCount`, `sides`, `orient`, `near`, `name`.

### Add a preset (no recompiling)

1. Copy a preset from `resources/presets.json` into a new file, e.g. `<save dir>/presets/my_styles.json`.
2. Give it a new `id`, `name`, `description` and `palette`, then edit its `rules`.
3. `python3 tools/validate_presets.py my_styles.json` (optional), then in game: **Tools > Reload presets**.

**Object IDs.** The bundled presets use IDs from the community GD 2.2 object-ID table. IDs I could not check against
your game are validated **at runtime**: anything that is missing or is not a non-colliding decoration is skipped, and
*Tools > Verify presets* lists exactly which IDs were rejected. If a preset looks sparse, that is where to look. To find
IDs for your own presets, use any object-ID list, or select an object in the editor and read its ID from the object
settings.

## Tempo sync

*Tools > Tempo sync...* sets on/off, **BPM (20-400), first-beat offset, and starting speed**; the speed portals in your
level (0.5x-4x) are followed automatically when mapping beats to X positions. With it on, each preset's `rhythm` adds
**pulse triggers** (the Glow colour flashes on the beat) and optional beat markers. The BPM in use is shown on the main
panel and in the result message. **Tap** sets the BPM by tapping along.

## Settings

Decoration density, max objects per run, editor object limit and safety margin, highest decorated row, editor layer
for added objects, whether the Background colour recolours the level background, live colour preview, fixed random
seed, and toolbar button offset.

## Deviations from the brief (things the API cannot do, and what is there instead)

* **BPM auto-detection from the level's song: not implemented.** Geometry Dash exposes no tempo metadata and Geode has
  no beat analysis. Alternative: manual BPM + tap tempo + offset. The brief's "detected *or* entered manually" is met by
  the manual path.
* **Colour picker:** uses Geode's built-in `ColorPickPopup` (colour wheel, R/G/B, hex) rather than custom RGB sliders.
* **Live preview:** the thumbnail in the Colours panel updates on every edit. The in-editor preview is shown while
  choosing a style (Change style); to see edited colours on it, reopen Change style after pressing Done.
* **Undo is this mod's own one-step undo** (Tools > Undo), not the editor's Ctrl+Z history: objects are created with
  the editor's undo recording off so a 10,000-object run is one undo, not ten thousand.
* **Strictly "empty space only"** means decoration never overlaps your objects, so it is sparser than overlay-style
  decorators that draw over and behind blocks. This is a deliberate reading of "never overwrite or move".
* **Structure pieces are tracked per editor session.** After reopening the editor they are ordinary decoration
  (still never overlapped), so Decorate will not trim around them as structures.
* **Look and layout** follow the reference video's panel, button order and status text, using GD's bitmap fonts and
  stock UI sprites. No assets, names or branding from the reference were copied; preset names/palettes are original.
* **Object limit:** the default of 80,000 is my best knowledge of the 2.2 editor limit; it is a setting.

## API risks

Everything Geode-facing is in `src/*.cpp` and `src/ui/*.cpp`. If a build fails, it will be one of these:

| Area | Where | Notes / fallback |
|---|---|---|
| `LevelEditorLayer::createObjectsFromString / removeObject / getAllObjects / runColorEffect`, `EditorUI::m_build/edit/deleteModeBtn`, `Popup::init(w,h)` | DecoManager, main, ui | Listed in the Geode docs / v5 migration guide when this was written. Member visibility and `runColorEffect` semantics unverified. Delete the `runColorEffect` loop in `applyPalette` if it misbehaves; colour triggers still colour the level. |
| `ObjectToolbox::sharedState()->intKeyToFrame`, `GameObject::createWithKey`, `m_objectType` | Catalog.cpp, PresetPreview.cpp | From memory. Core of the "never place a non-decoration" safety check - fix names rather than removing. |
| `GameObject::m_baseColor / m_detailColor` | DecoManager.cpp | `#define ADECO_SCAN_COLOUR_CHANNELS 0` falls back to channels 999..995. |
| `EditorUI::getLimitedPosition` | DecoManager.cpp | `#define ADECO_USE_LIMITED_POSITION 0` uses the region bounds only. |
| `ColorPickPopup` + `ColorPickPopupDelegate` | ColoursPopup | `#define ADECO_USE_COLOR_PICKER 0` keeps hex entry only. |
| `GJBaseGameLayer::m_objectLayer` position/scale | DecoManager.cpp (`visibleRegion`) | Used only to find the section in view for previews. |
| `Mod::getResourcesDir()` layout for `resources.files` | PresetStore.cpp | Two layouts are probed; failure shows "No presets available" with the path tried. |
| `GameObjectType::Slope`, `CCMenu::setTouchPriority` | DecoManager.cpp, StylePickerBar.cpp | Drop `Slope` (treated as Solid otherwise) / adjust priority. |
| Toolbar button position | main.cpp | Heuristic: right of the Build/Edit/Delete column, level with Delete. Use the X/Y offset settings. |

## Test checklist (run in game, GD 2.2081, on each platform you ship)

1. **Launch:** with the mod installed the game reaches the main menu; Geode shows no load error for *Auto Decorator*.
2. **Editor opens:** open any level in the editor; an *Auto Deco* button appears beside Build/Edit/Delete and opens the
   panel. Leave and re-enter the editor twice (no crash, single button).
3. **Empty level:** new empty level > Auto Deco > Decorate level. Expect a skyline/backdrop, a success message with
   counts, no crash, nothing placed below the ground or beyond the editor bounds.
4. **Dense level:** open a 50k+ object level > Decorate level. Expect it to finish, the object count to stay under the
   limit minus the margin, your objects untouched (spot-check a few positions/colours), and no visible overlap.
5. **No song:** a level with no song > enable Tempo sync, BPM 120 > Decorate. Expect pulses/markers and "120.0 BPM" shown.
6. **Style picker:** Change style > `<` `>` through all styles; the preview changes; **Cancel** leaves the level
   unchanged (object count back to what it was); **Use this style** returns to the panel with that style selected.
7. **Colours - Cancel:** change two colours (one via hex, one via the picker), press **Cancel**, reopen: unchanged.
8. **Colours - Done:** change colours, press **Done**; the panel thumbnail and the next preview/decoration use them;
   restart the game: they persist; **Reset to preset** restores the originals. Type `12345` / `#GGGGGG` in a field:
   it is flagged and the colour does not change.
9. **Structure then decorate:** *Structure layout* shows "N structure pieces added. Edit them if you like, then
   decorate."; delete or move a few pieces; *Decorate level*: your edits are kept.
10. **Undo:** after a Decorate, *Tools > Undo* removes exactly that run in one step; your own objects are unaffected;
    decorate again gives a different variation.
11. **Object limit:** lower *Editor object limit* in settings to just above the level's count and Decorate: it adds
    fewer objects or reports there is no room, never exceeding the limit.
12. **Presets:** *Tools > Verify presets* reports the loaded count and any rejected IDs; drop a user preset in
    `presets/`, *Reload presets*, and it appears; a deliberately broken JSON is reported, not fatal.
13. **Mobile / small window:** panels fit and every button is reachable at the smallest window size and in landscape on
    a phone; text fields open the keyboard.

## Project layout

```
mod.json  CMakeLists.txt  about.md  changelog.md  logo.png  LICENSE  README.md
resources/presets.json                 data-driven styles
src/core/        pure C++: Types, Json, Preset, Analysis, Planner, Tempo, ObjectString   (unit tested)
src/             DecoManager, Catalog, PresetStore, Settings, main (hook + toolbar button)
src/ui/          AutoDecoPopup, StylePickerBar, ColoursPopup, TempoPopup, ToolsPopup, PresetPreview, UiKit
tests/           test_core.cpp + run_tests.sh, syntax_check.sh + stub/ (glue type-check)
tools/validate_presets.py              preset checker for authors
.github/workflows/build.yml            multi-platform CI that produces the single .geode
```

## License

MIT (see `LICENSE`). Replace `YourName` / `yourname.auto_decorator` in `mod.json` and `LICENSE` with your own.
