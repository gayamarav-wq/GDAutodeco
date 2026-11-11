// Typed access to the mod's settings (mod.json "settings") and saved values (save data).
#pragma once

#include <Geode/Geode.hpp>
#include <string>

namespace adeco::cfg {

inline geode::Mod* mod() { return geode::Mod::get(); }

// ---- settings (editable in Geode's mod settings screen) ----
inline float densityScale() { return float(mod()->getSettingValue<double>("density-scale")); }
inline int maxNewObjects() { return int(mod()->getSettingValue<int64_t>("max-new-objects")); }
inline int previewMaxObjects() { return int(mod()->getSettingValue<int64_t>("preview-max-objects")); }
inline int objectLimit() { return int(mod()->getSettingValue<int64_t>("object-limit")); }
inline int safetyMargin() { return int(mod()->getSettingValue<int64_t>("object-limit-margin")); }
inline int maxCellY() { return int(mod()->getSettingValue<int64_t>("max-cell-y")); }
inline int decoEditorLayer() { return int(mod()->getSettingValue<int64_t>("deco-editor-layer")); }
inline bool applyBackground() { return mod()->getSettingValue<bool>("apply-background-colour"); }
inline bool liveColourPreview() { return mod()->getSettingValue<bool>("live-colour-preview"); }
inline int fixedSeed() { return int(mod()->getSettingValue<int64_t>("random-seed")); }
inline float buttonOffsetX() { return float(mod()->getSettingValue<double>("button-offset-x")); }
inline float buttonOffsetY() { return float(mod()->getSettingValue<double>("button-offset-y")); }

// ---- saved values (mod save data; never written into the level) ----
inline std::string lastPreset() { return mod()->getSavedValue<std::string>("last-preset", ""); }
inline void setLastPreset(std::string const& id) { mod()->setSavedValue<std::string>("last-preset", id); }

inline bool tempoEnabled() { return mod()->getSavedValue<bool>("tempo-enabled", false); }
inline void setTempoEnabled(bool v) { mod()->setSavedValue<bool>("tempo-enabled", v); }
inline double tempoBpm() { return mod()->getSavedValue<double>("tempo-bpm", 120.0); }
inline void setTempoBpm(double v) { mod()->setSavedValue<double>("tempo-bpm", v); }
inline int tempoOffsetMs() { return int(mod()->getSavedValue<int64_t>("tempo-offset-ms", 0)); }
inline void setTempoOffsetMs(int v) { mod()->setSavedValue<int64_t>("tempo-offset-ms", v); }
inline int tempoSpeed() { return int(mod()->getSavedValue<int64_t>("tempo-speed", 1)); }
inline void setTempoSpeed(int v) { mod()->setSavedValue<int64_t>("tempo-speed", v); }

}  // namespace adeco::cfg
