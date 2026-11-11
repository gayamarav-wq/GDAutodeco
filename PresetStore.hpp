// Loads presets (bundled + user files) and stores the player's custom palettes.
#pragma once

#include <string>
#include <vector>

#include "core/Preset.hpp"

namespace adeco {

class PresetStore {
public:
    static PresetStore& get();

    // Re-reads resources/presets.json and every *.json in <save dir>/presets/.
    void reload();

    std::vector<Preset> const& presets();             // lazily loads on first use
    std::vector<std::string> const& errors();         // problems found while loading
    size_t indexOf(std::string const& id);            // 0 if unknown
    size_t lastUsedIndex();                           // from save data, defaults to 0

    // Palette handling. Custom palettes live in the mod's save data, never in the level.
    bool hasCustom(Preset const& p);
    bool usesCustom(Preset const& p);
    Palette customOrDefault(Preset const& p);          // saved custom palette, or the preset's own colours
    Palette effective(Preset const& p);                // what Decorate / previews will use
    void saveCustom(Preset const& p, Palette const& pal, bool useCustom);

    std::string userPresetDir() const;

private:
    void load();
    bool m_loaded = false;
    std::vector<Preset> m_presets;
    std::vector<std::string> m_errors;
};

}  // namespace adeco
