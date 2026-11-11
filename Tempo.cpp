#include "Tempo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace adeco {

float unitsPerSecond(Speed s) {
    switch (s) {
        case Speed::Slow: return 251.16f;
        case Speed::Normal: return 311.58f;
        case Speed::Fast: return 387.42f;
        case Speed::Faster: return 468.0f;
        case Speed::Fastest: return 576.0f;
    }
    return 311.58f;
}

char const* speedName(Speed s) {
    switch (s) {
        case Speed::Slow: return "0.5x";
        case Speed::Normal: return "1x";
        case Speed::Fast: return "2x";
        case Speed::Faster: return "3x";
        case Speed::Fastest: return "4x";
    }
    return "1x";
}

Speed speedFromIndex(int i) { return Speed(std::clamp(i, 0, 4)); }

std::optional<Speed> speedFromPortalId(int id) {
    switch (id) {
        case 200: return Speed::Slow;
        case 201: return Speed::Normal;
        case 202: return Speed::Fast;
        case 203: return Speed::Faster;
        case 1334: return Speed::Fastest;
        default: return std::nullopt;
    }
}

bool parseBpm(std::string_view text, double& out) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    if (text.empty() || text.size() > 12) return false;
    std::string s(text);
    for (char c : s)
        if (!((c >= '0' && c <= '9') || c == '.')) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return false;
    if (!(v >= kMinBpm && v <= kMaxBpm)) return false;
    out = v;
    return true;
}

float xAtTime(double t, Speed start, std::vector<SpeedChange> const& changes) {
    if (t <= 0) return 0.f;
    double x = 0, speed = unitsPerSecond(start);
    for (auto const& c : changes) {
        if (c.x <= x) { speed = unitsPerSecond(c.speed); continue; }  // portal at/behind us: just switch speed
        double dist = double(c.x) - x, need = dist / speed;
        if (t <= need) return float(x + t * speed);
        t -= need;
        x = c.x;
        speed = unitsPerSecond(c.speed);
    }
    return float(x + t * speed);
}

std::vector<float> beatPositions(TempoParams const& tp, float xFrom, float xTo,
                                 std::vector<SpeedChange> const& changes, size_t maxCount) {
    std::vector<float> out;
    if (!(tp.bpm >= kMinBpm && tp.bpm <= kMaxBpm) || xTo < xFrom) return out;
    double beat = 60.0 / tp.bpm;
    // First beat index whose time is >= 0 (beats before the start of the level are irrelevant).
    long first = tp.offsetSec >= 0 ? 0 : long(std::ceil(-tp.offsetSec / beat));
    for (long k = first; out.size() < maxCount; ++k) {
        double t = tp.offsetSec + double(k) * beat;
        float x = xAtTime(t, tp.startSpeed, changes);
        if (x > xTo) break;
        if (x >= xFrom) out.push_back(x);
        if (k - first > 2000000) break;  // absolute safety net
    }
    return out;
}

std::optional<double> tapTempo(std::vector<double> const& taps) {
    if (taps.size() < 2) return std::nullopt;
    size_t n = std::min<size_t>(taps.size(), 8);
    size_t from = taps.size() - n;
    double span = taps.back() - taps[from];
    if (span <= 0) return std::nullopt;
    double bpm = 60.0 * double(n - 1) / span;
    if (!(bpm >= kMinBpm && bpm <= kMaxBpm)) return std::nullopt;
    return bpm;
}

}  // namespace adeco
