// Auto Decorator - core types.
//
// Everything under src/core is plain C++20 with NO Geode / cocos2d-x dependency. That is
// deliberate: the planning logic (analysis, layout, tempo maths, preset parsing) can be unit
// tested off-game with tests/run_tests.sh, and the Geode-facing code in src/ stays thin.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adeco {

// One editor grid cell, in level units.
inline constexpr float kCell = 30.f;

// ---------------------------------------------------------------- colours
struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(Rgb const& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(Rgb const& o) const { return !(*this == o); }
};

// Accepts "#RRGGBB" or "RRGGBB" - exactly six hex digits, nothing else.
bool parseHex(std::string_view text, Rgb& out);
// Always "#RRGGBB", upper-case.
std::string toHex(Rgb c);
// Linear mix, t in [0,1] (0 = a, 1 = b).
Rgb mix(Rgb a, Rgb b, float t);

enum class Role : uint8_t { Background = 0, Blocks, Outline, Accent, Glow };
inline constexpr size_t kRoleCount = 5;
inline constexpr std::array<const char*, kRoleCount> kRoleKeys = {"background", "blocks", "outline", "accent", "glow"};
inline constexpr std::array<const char*, kRoleCount> kRoleNames = {"Background", "Blocks", "Outline", "Accent", "Glow"};
std::optional<Role> roleFromKey(std::string_view key);

struct Palette {
    std::array<Rgb, kRoleCount> c{};
    Rgb& operator[](Role r) { return c[size_t(r)]; }
    Rgb const& operator[](Role r) const { return c[size_t(r)]; }
    bool operator==(Palette const& o) const { return c == o.c; }
    bool operator!=(Palette const& o) const { return !(*this == o); }
};

// "#A,#B,#C,#D,#E" - the format stored in the mod's save data.
std::string paletteToString(Palette const& p);
bool paletteFromString(std::string_view text, Palette& out);

// ---------------------------------------------------------------- RNG
// Tiny deterministic PRNG (splitmix64). std::mt19937 + std::uniform_*_distribution give
// different results on different standard libraries; we want the same preset + level to
// produce the same decoration on Windows, macOS and Android.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed = 1) : s(seed * 0x9E3779B97F4A7C15ull + 0x2545F4914F6CDD1Dull) {}
    uint64_t next() {
        s += 0x9E3779B97F4A7C15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // [0,1)
    float f() { return float(next() >> 40) * (1.0f / 16777216.0f); }
    // inclusive integer range
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + int(next() % uint64_t(hi - lo + 1));
    }
    float rangef(float lo, float hi) { return lo + (hi - lo) * f(); }
};

// ---------------------------------------------------------------- grid
struct Region {  // inclusive cell rectangle
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    bool contains(int x, int y) const { return x >= minX && x <= maxX && y >= minY && y <= maxY; }
};

inline uint64_t cellKey(int x, int y) { return (uint64_t(uint32_t(x)) << 32) | uint64_t(uint32_t(y)); }

// How the scanner classifies objects that already exist in the level.
enum class ObjClass : uint8_t { Other = 0, Deco, Structure, Solid, Hazard };

struct InObject {
    int id = 0;
    float x = 0, y = 0;    // centre, level units
    float w = 30, h = 30;  // size, level units
    ObjClass cls = ObjClass::Other;
};

}  // namespace adeco
