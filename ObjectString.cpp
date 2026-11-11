#include "ObjectString.hpp"

#include <cmath>
#include <cstdio>

namespace adeco {

std::string fmtNum(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.3f", v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0" || s.empty()) s = "0";
    return s;
}

std::string objectString(Placement const& p, ChannelMap const& map, int editorLayer) {
    std::string s = "1," + std::to_string(p.id) + ",2," + fmtNum(p.x) + ",3," + fmtNum(p.y);
    if (std::fabs(p.rot) > 0.0005f) s += ",6," + fmtNum(p.rot);
    if (p.flipX) s += ",4,1";
    if (p.flipY) s += ",5,1";
    if (std::fabs(p.sx - 1.f) > 0.0005f || std::fabs(p.sy - 1.f) > 0.0005f)
        s += ",128," + fmtNum(p.sx) + ",129," + fmtNum(p.sy);
    s += ",21," + std::to_string(map.ch[size_t(p.color)]);
    if (p.detail) s += ",22," + std::to_string(map.ch[size_t(*p.detail)]);
    s += ",24," + std::to_string(p.z) + ",25," + std::to_string(p.zOrder);
    if (editorLayer > 0) s += ",20," + std::to_string(editorLayer);
    s += ';';
    return s;
}

std::string colorTriggerString(float x, float y, Rgb c, int ch) {
    return "1,899,2," + fmtNum(x) + ",3," + fmtNum(y) + ",7," + std::to_string(c.r) + ",8," + std::to_string(c.g) +
           ",9," + std::to_string(c.b) + ",10,0,23," + std::to_string(ch) + ",35,1;";
}

std::string pulseTriggerString(float x, float y, Rgb c, int ch, float fadeIn, float hold, float fadeOut) {
    return "1,1006,2," + fmtNum(x) + ",3," + fmtNum(y) + ",7," + std::to_string(c.r) + ",8," + std::to_string(c.g) +
           ",9," + std::to_string(c.b) + ",45," + fmtNum(fadeIn) + ",46," + fmtNum(hold) + ",47," + fmtNum(fadeOut) +
           ",48,0,51," + std::to_string(ch) + ",52,0;";
}

}  // namespace adeco
