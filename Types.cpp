#include "Types.hpp"

#include <cmath>
#include <cstdio>

namespace adeco {

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool parseHex(std::string_view t, Rgb& out) {
    if (!t.empty() && t.front() == '#') t.remove_prefix(1);
    if (t.size() != 6) return false;
    int v[6];
    for (int i = 0; i < 6; ++i) {
        v[i] = hexVal(t[size_t(i)]);
        if (v[i] < 0) return false;
    }
    out.r = uint8_t(v[0] * 16 + v[1]);
    out.g = uint8_t(v[2] * 16 + v[3]);
    out.b = uint8_t(v[4] * 16 + v[5]);
    return true;
}

std::string toHex(Rgb c) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

Rgb mix(Rgb a, Rgb b, float t) {
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    auto m = [&](uint8_t x, uint8_t y) { return uint8_t(std::lround(float(x) + (float(y) - float(x)) * t)); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

std::optional<Role> roleFromKey(std::string_view key) {
    for (size_t i = 0; i < kRoleCount; ++i)
        if (key == kRoleKeys[i]) return Role(i);
    return std::nullopt;
}

std::string paletteToString(Palette const& p) {
    std::string out;
    for (size_t i = 0; i < kRoleCount; ++i) {
        if (i) out += ',';
        out += toHex(p.c[i]);
    }
    return out;
}

bool paletteFromString(std::string_view text, Palette& out) {
    Palette tmp;
    size_t idx = 0, start = 0;
    while (idx < kRoleCount) {
        size_t comma = text.find(',', start);
        std::string_view part =
            text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        if (!parseHex(part, tmp.c[idx])) return false;
        ++idx;
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    if (idx != kRoleCount) return false;
    out = tmp;
    return true;
}

}  // namespace adeco
