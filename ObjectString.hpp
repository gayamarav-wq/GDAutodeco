// Turns plans into GD object strings (the "key,value,key,value;" format used by the level format and
// by LevelEditorLayer::createObjectsFromString).
//
// Keys used (GD object-string keys): 1 id, 2 x, 3 y, 4 flipX, 5 flipY, 6 rotation, 20 editor layer,
// 21 main colour channel, 22 detail colour channel, 24 z layer, 25 z order, 128 scaleX, 129 scaleY.
// Colour trigger (899): 7/8/9 r,g,b, 10 duration, 23 target channel, 35 opacity.
// Pulse trigger (1006): 7/8/9 r,g,b, 45 fade in, 46 hold, 47 fade out, 48 mode (0 = colour),
//                       51 target id, 52 target type (0 = colour channel).
#pragma once

#include <array>
#include <string>

#include "Planner.hpp"

namespace adeco {

struct ChannelMap {
    // GD colour channel for each palette role. Background maps to the special BG channel (1000) when
    // the user asks us to recolour the background, otherwise to a free custom channel.
    std::array<int, kRoleCount> ch{1000, 901, 902, 903, 904};
};

inline constexpr int kColorTriggerId = 899;
inline constexpr int kPulseTriggerId = 1006;

std::string fmtNum(double v);  // up to 3 decimals, no trailing zeros, "-0" -> "0"
std::string objectString(Placement const& p, ChannelMap const& map, int editorLayer);
std::string colorTriggerString(float x, float y, Rgb col, int targetChannel);
std::string pulseTriggerString(float x, float y, Rgb col, int targetChannel, float fadeIn, float hold, float fadeOut);

}  // namespace adeco
