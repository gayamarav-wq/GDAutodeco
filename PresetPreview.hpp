// Small thumbnail of a preset: its palette plus the actual sprites its rules use.
#pragma once

#include <Geode/Geode.hpp>

#include "core/Preset.hpp"

namespace adeco::ui {

// Builds a node of the given size (origin bottom-left). Pure drawing - no level access, so it is cheap
// enough to rebuild on every colour edit ("live" preview in the Colours panel).
cocos2d::CCNode* makePresetPreview(Preset const& p, Palette const& pal, cocos2d::CCSize size);

}  // namespace adeco::ui
