// Small helpers for the dark, rounded look used by every Auto Decorator panel.
// Design units are GD points (the screen is ~320 units tall); each popup computes a scale factor so it
// still fits on small windows / phones.
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <string>

#include "core/Types.hpp"

namespace adeco::ui {

using namespace geode::prelude;

// ---- palette of the UI itself ----
inline constexpr Rgb kPanel{0x1B, 0x1D, 0x3A};     // dark navy panel
inline constexpr Rgb kPanelLight{0x2A, 0x2D, 0x55};  // secondary buttons, inputs
inline constexpr Rgb kPrimary{0x6C, 0x63, 0xFF};   // purple primary button / highlights
inline constexpr Rgb kText{0xE8, 0xEA, 0xFF};
inline constexpr Rgb kMuted{0x9A, 0x9F, 0xC8};
inline constexpr Rgb kDanger{0xFF, 0x5A, 0x5A};

inline ccColor3B c3(Rgb c) { return {c.r, c.g, c.b}; }
inline ccColor4B c4(Rgb c, GLubyte a = 255) { return {c.r, c.g, c.b, a}; }

// Largest uniform scale (<= 1) that lets a W x H design fit the window with a margin.
inline float fitScale(float W, float H, float margin = 16.f) {
    CCSize win = CCDirector::get()->getWinSize();
    return std::clamp(std::min((win.width - margin * 2.f) / W, (win.height - margin * 2.f) / H), 0.35f, 1.f);
}

inline CCScale9Sprite* rounded(float w, float h, Rgb col, GLubyte opacity = 255) {
    auto s = CCScale9Sprite::create("square02b_001.png");
    s->setContentSize({w, h});
    s->setColor(c3(col));
    s->setOpacity(opacity);
    return s;
}

inline CCLabelBMFont* label(std::string const& text, char const* font, float scale, Rgb col = kText,
                            CCPoint anchor = {0.5f, 0.5f}) {
    auto l = CCLabelBMFont::create(text.c_str(), font);
    l->setScale(scale);
    l->setColor(c3(col));
    l->setAnchorPoint(anchor);
    return l;
}

// Rounded text button. `sub` adds a small second line (like "pillars and walls" in the reference UI).
inline CCMenuItemSpriteExtra* pill(std::string const& text, float w, float h, Rgb bg, CCObject* target,
                                   SEL_MenuHandler sel, float fontScale = 0.45f, char const* sub = nullptr,
                                   Rgb textCol = kText) {
    auto node = CCNode::create();
    node->setContentSize({w, h});
    auto bgSpr = rounded(w, h, bg);
    bgSpr->setPosition({w / 2, h / 2});
    node->addChild(bgSpr);
    auto lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    lbl->setColor(c3(textCol));
    lbl->limitLabelWidth(w - 14.f, fontScale, 0.1f);
    lbl->setPosition({w / 2, sub ? h * 0.64f : h / 2});
    node->addChild(lbl);
    if (sub) {
        auto s = CCLabelBMFont::create(sub, "chatFont.fnt");
        s->setColor(c3(kMuted));
        s->limitLabelWidth(w - 14.f, 0.32f, 0.1f);
        s->setPosition({w / 2, h * 0.27f});
        node->addChild(s);
    }
    return CCMenuItemSpriteExtra::create(node, nullptr, target, sel);
}

// Dark translucent look for a Popup's background (call after Popup::init).
inline void styleBackground(CCScale9Sprite* bg) {
    if (!bg) return;
    bg->setColor(c3(kPanel));
    bg->setOpacity(238);
}

}  // namespace adeco::ui
