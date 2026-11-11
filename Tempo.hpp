// Tempo sync maths: BPM -> beat times -> level X positions (taking speed portals into account).
#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace adeco {

enum class Speed : int { Slow = 0, Normal = 1, Fast = 2, Faster = 3, Fastest = 4 };

// Player speed in level units per second (0.5x, 1x, 2x, 3x, 4x).
float unitsPerSecond(Speed s);
char const* speedName(Speed s);
Speed speedFromIndex(int i);  // clamps to [0,4]

// GD speed portal object IDs: 200 = 0.5x, 201 = 1x, 202 = 2x, 203 = 3x, 1334 = 4x.
std::optional<Speed> speedFromPortalId(int objectId);

struct SpeedChange {
    float x;
    Speed speed;
};  // keep sorted by x

struct TempoParams {
    double bpm = 120.0;
    double offsetSec = 0.0;  // time of the first beat (can be negative)
    Speed startSpeed = Speed::Normal;
};

constexpr double kMinBpm = 20.0, kMaxBpm = 400.0;

// Accepts "128", "128.5", " 128 " in [20, 400].
bool parseBpm(std::string_view text, double& out);

// Level X (units) the player reaches after `t` seconds, starting at x = 0.
float xAtTime(double t, Speed start, std::vector<SpeedChange> const& changes);

// X position of every beat that falls in [xFrom, xTo], at most maxCount of them.
std::vector<float> beatPositions(TempoParams const& tp, float xFrom, float xTo,
                                 std::vector<SpeedChange> const& changes, size_t maxCount);

// Tap tempo: BPM from tap timestamps (seconds). Uses the last 8 taps; returns nullopt for < 2 taps.
std::optional<double> tapTempo(std::vector<double> const& taps);

}  // namespace adeco
