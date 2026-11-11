#include "Catalog.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace adeco {

GeodeCatalog::Entry GeodeCatalog::probe(int id) {
    Entry e;
    if (id < 1 || id > 99999) return e;

    // 1) Does this game version know the object, and is its sprite loaded?
    auto toolbox = ObjectToolbox::sharedState();  // API-RISK: name/signature of ObjectToolbox accessors
    char const* frame = toolbox ? toolbox->intKeyToFrame(id) : nullptr;
    if (!frame || !frame[0]) return e;
    auto sf = CCSpriteFrameCache::get()->spriteFrameByName(frame);
    if (!sf) return e;

    // 2) Is it a pure decoration? (GameObjectType::Decoration never collides with the player.)
    auto obj = GameObject::createWithKey(id);  // API-RISK: static factory name; autoreleased
    if (!obj || obj->m_objectType != GameObjectType::Decoration) return e;

    CCSize sz = sf->getOriginalSize();
    e.w = sz.width > 1.f ? sz.width : kCell;
    e.h = sz.height > 1.f ? sz.height : kCell;
    e.ok = true;
    return e;
}

bool GeodeCatalog::usable(int id, float& w, float& h) const {
    auto it = m_cache.find(id);
    if (it == m_cache.end()) it = m_cache.emplace(id, probe(id)).first;
    w = it->second.w;
    h = it->second.h;
    return it->second.ok;
}

}  // namespace adeco
