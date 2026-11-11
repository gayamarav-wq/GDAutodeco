// Tells the planner which object IDs exist in THIS game build and are safe to place.
#pragma once

#include <unordered_map>

#include "core/Planner.hpp"

namespace adeco {

class GeodeCatalog final : public ObjectCatalog {
public:
    // True only if the object has a sprite in the game AND is a non-colliding decoration. Anything that
    // could interact with the player (solids, hazards, orbs, portals, triggers) is rejected, so a typo in a
    // preset can never change how the level plays.
    bool usable(int id, float& w, float& h) const override;

private:
    struct Entry { bool ok = false; float w = 30, h = 30; };
    mutable std::unordered_map<int, Entry> m_cache;
    static Entry probe(int id);
};

}  // namespace adeco
