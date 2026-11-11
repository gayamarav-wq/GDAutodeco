#include "PresetPreview.hpp"

#include "UiKit.hpp"

using namespace geode::prelude;

namespace adeco::ui {

namespace {
CCSprite* spriteForObject(int id) {
    auto tb = ObjectToolbox::sharedState();  // API-RISK: see Catalog.cpp
    char const* frame = tb ? tb->intKeyToFrame(id) : nullptr;
    if (!frame || !frame[0]) return nullptr;
    if (!CCSpriteFrameCache::get()->spriteFrameByName(frame)) return nullptr;  // guards against missing frames
    return CCSprite::createWithSpriteFrameName(frame);
}
}  // namespace

CCNode* makePresetPreview(Preset const& p, Palette const& pal, CCSize size) {
    auto root = CCNode::create();
    root->setContentSize(size);
    float W = size.width, H = size.height;

    auto rect = [&](float x, float y, float w, float h, Rgb c, GLubyte a = 255) {
        auto l = CCLayerColor::create(c4(c, a), w, h);
        l->setPosition({x, y});
        root->addChild(l);
        return l;
    };

    // sky + soft glow near the horizon
    rect(0, 0, W, H, pal[Role::Background]);
    auto grad = CCLayerGradient::create(c4(pal[Role::Glow], 0), c4(pal[Role::Glow], 90), {0, -1});
    grad->setContentSize({W, H * 0.7f});
    grad->setPosition({0, 0});
    root->addChild(grad);

    // deterministic skyline from the preset seed
    Rng rng(p.seed);
    float x = -W * 0.02f;
    while (x < W) {
        float w = W * rng.rangef(0.07f, 0.14f), h = H * rng.rangef(0.22f, 0.62f);
        rect(x, H * 0.16f, w, h, pal[Role::Blocks], 235);
        rect(x, H * 0.16f + h - 1.5f, w, 1.5f, pal[Role::Outline]);  // outline trim
        x += w + W * rng.rangef(0.01f, 0.05f);
    }
    // ground
    rect(0, 0, W, H * 0.16f, pal[Role::Blocks]);
    rect(0, H * 0.16f - 1.5f, W, 1.5f, pal[Role::Outline]);

    // the preset's own sprites, tinted with the palette
    Role cycle[3] = {Role::Outline, Role::Accent, Role::Glow};
    float px = W * 0.12f;
    int i = 0;
    for (int id : p.preview) {
        if (auto s = spriteForObject(id)) {
            float target = H * (i % 2 ? 0.26f : 0.34f);
            float base = std::max(1.f, std::max(s->getContentSize().width, s->getContentSize().height));
            s->setScale(std::min(1.2f, target / base));
            s->setColor(c3(pal[cycle[i % 3]]));
            s->setPosition({px, H * (i % 2 ? 0.40f : 0.55f)});
            root->addChild(s, 2);
        }
        px += W * 0.22f;
        ++i;
    }
    return root;
}

}  // namespace adeco::ui
