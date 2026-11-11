#include "PresetStore.hpp"

#include <Geode/Geode.hpp>
#include <algorithm>
#include <filesystem>

#include "Settings.hpp"

using namespace geode::prelude;

namespace adeco {

PresetStore& PresetStore::get() {
    static PresetStore s;
    return s;
}

std::string PresetStore::userPresetDir() const {
    return (Mod::get()->getSaveDir() / "presets").string();
}

static void mergeInto(std::vector<Preset>& dst, std::vector<Preset>&& src) {
    for (auto& p : src) {
        auto it = std::find_if(dst.begin(), dst.end(), [&](Preset const& q) { return q.id == p.id; });
        if (it != dst.end()) *it = std::move(p);  // user presets override bundled ones with the same id
        else dst.push_back(std::move(p));
    }
}

void PresetStore::load() {
    m_presets.clear();
    m_errors.clear();
    namespace fs = std::filesystem;

    // Bundled presets. API-RISK: where Geode places files listed under mod.json "resources.files".
    // We probe the two layouts seen in practice and log which one was used.
    fs::path res = Mod::get()->getResourcesDir();
    std::vector<fs::path> candidates = {res / "presets.json", res / "resources" / "presets.json"};
    bool found = false;
    for (auto const& c : candidates) {
        std::error_code ec;
        if (!fs::exists(c, ec)) continue;
        auto text = utils::file::readString(c);
        if (text.isErr()) { m_errors.push_back(c.string() + ": cannot read file"); continue; }
        auto r = loadPresetsFromJson(text.unwrap(), "presets.json");
        for (auto& e : r.errors) m_errors.push_back(std::move(e));
        mergeInto(m_presets, std::move(r.presets));
        log::info("Loaded bundled presets from {}", c.string());
        found = true;
        break;
    }
    if (!found) m_errors.push_back("bundled presets.json not found under " + res.string());

    // User presets: drop *.json files into <save dir>/presets/ - no recompiling or repackaging needed.
    std::error_code ec;
    fs::path userDir = Mod::get()->getSaveDir() / "presets";
    fs::create_directories(userDir, ec);
    std::vector<fs::path> files;
    for (auto it = fs::directory_iterator(userDir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
        if (it->is_regular_file() && it->path().extension() == ".json") files.push_back(it->path());
    std::sort(files.begin(), files.end());
    for (auto const& f : files) {
        auto text = utils::file::readString(f);
        if (text.isErr()) { m_errors.push_back(f.filename().string() + ": cannot read file"); continue; }
        auto r = loadPresetsFromJson(text.unwrap(), f.filename().string());
        for (auto& e : r.errors) m_errors.push_back(std::move(e));
        mergeInto(m_presets, std::move(r.presets));
    }

    for (auto const& e : m_errors) log::warn("preset problem: {}", e);
    log::info("{} presets available", m_presets.size());
    m_loaded = true;
}

void PresetStore::reload() { load(); }

std::vector<Preset> const& PresetStore::presets() {
    if (!m_loaded) load();
    return m_presets;
}
std::vector<std::string> const& PresetStore::errors() {
    if (!m_loaded) load();
    return m_errors;
}

size_t PresetStore::indexOf(std::string const& id) {
    auto const& v = presets();
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].id == id) return i;
    return 0;
}
size_t PresetStore::lastUsedIndex() { return indexOf(cfg::lastPreset()); }

bool PresetStore::hasCustom(Preset const& p) {
    Palette tmp;
    return paletteFromString(Mod::get()->getSavedValue<std::string>("custom:" + p.id, ""), tmp);
}
bool PresetStore::usesCustom(Preset const& p) {
    return hasCustom(p) && Mod::get()->getSavedValue<bool>("use-custom:" + p.id, false);
}
Palette PresetStore::customOrDefault(Preset const& p) {
    Palette pal;
    if (paletteFromString(Mod::get()->getSavedValue<std::string>("custom:" + p.id, ""), pal)) return pal;
    return p.palette;
}
Palette PresetStore::effective(Preset const& p) { return usesCustom(p) ? customOrDefault(p) : p.palette; }

void PresetStore::saveCustom(Preset const& p, Palette const& pal, bool useCustom) {
    Mod::get()->setSavedValue<std::string>("custom:" + p.id, paletteToString(pal));
    Mod::get()->setSavedValue<bool>("use-custom:" + p.id, useCustom);
}

}  // namespace adeco
