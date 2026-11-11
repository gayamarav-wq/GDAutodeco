#include "StylePickerBar.hpp"

#include "../DecoManager.hpp"
#include "../PresetStore.hpp"
#include "../Settings.hpp"
#include "AutoDecoPopup.hpp"

using namespace geode::prelude;

namespace adeco::ui {

namespace { StylePickerBar* s_open = nullptr; }

StylePickerBar* StylePickerBar::open(LevelEditorLayer* lel, size_t startIndex) {
    if (s_open) s_open->cleanup(false);
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return nullptr;
    auto ret = new StylePickerBar();
    if (ret->init(lel, startIndex)) {
        ret->autorelease();
        scene->addChild(ret, 500);
        s_open = ret;
        ret->showCurrent();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool StylePickerBar::init(LevelEditorLayer* lel, size_t startIndex) {
    if (!CCNode::init()) return false;
    m_lel = lel;
    m_index = startIndex;

    CCSize win = CCDirector::get()->getWinSize();
    float W = 470.f, H = 40.f;
    float u = fitScale(W, H, 8.f);
    float w = W * u, h = H * u;
    this->setContentSize({w, h});
    this->setAnchorPoint({0.5f, 0.5f});
    this->ignoreAnchorPointForPosition(false);
    this->setPosition({win.width / 2, win.height - 68.f * std::max(0.6f, u)});

    auto bg = rounded(w, h, kPanel, 238);
    bg->setPosition({w / 2, h / 2});
    this->addChild(bg);

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    menu->setContentSize({w, h});
    menu->setTouchPriority(-130);  // above the editor's own buttons that sit under the bar
    this->addChild(menu, 1);

    auto add = [&](CCMenuItemSpriteExtra* item, float x, float y) { item->setPosition({x * u, y * u}); menu->addChild(item); };
    add(pill("<", 24 * u, 24 * u, kPanelLight, this, menu_selector(StylePickerBar::onPrev), 0.5f), 22, H / 2);
    add(pill(">", 24 * u, 24 * u, kPanelLight, this, menu_selector(StylePickerBar::onNext), 0.5f), 52, H / 2);
    add(pill("Use this style", 112 * u, 28 * u, kPrimary, this, menu_selector(StylePickerBar::onUse), 0.4f), W - 10 - 72 - 6 - 56, H / 2);
    add(pill("Cancel", 72 * u, 28 * u, kPanelLight, this, menu_selector(StylePickerBar::onCancel), 0.4f), W - 10 - 36, H / 2);

    m_name = label("", "bigFont.fnt", 0.42f * u, kText, {0.f, 0.5f});
    m_name->setPosition({72 * u, h * 0.66f});
    this->addChild(m_name, 2);
    m_sub = label("", "chatFont.fnt", 0.36f * u, kMuted, {0.f, 0.5f});
    m_sub->setPosition({72 * u, h * 0.28f});
    this->addChild(m_sub, 2);
    return true;
}

void StylePickerBar::step(int delta) {
    long n = long(PresetStore::get().presets().size());
    if (n == 0) return;
    long i = (long(m_index) + delta) % n;
    if (i < 0) i += n;
    m_index = size_t(i);
    this->showCurrent();
}

void StylePickerBar::showCurrent() {
    auto& store = PresetStore::get();
    auto const& p = store.presets()[m_index];
    m_name->setString(p.name.c_str());
    m_name->limitLabelWidth(150.f, 0.42f, 0.15f);
    m_sub->setString(fmt::format("{} / {}  -  Preparing preview...", m_index + 1, store.presets().size()).c_str());
    DecoManager::get().clearPreview(m_lel);
    this->unschedule(schedule_selector(StylePickerBar::runPreview));
    this->scheduleOnce(schedule_selector(StylePickerBar::runPreview), 0.03f);  // let the label repaint first
}

void StylePickerBar::runPreview(float) {
    auto& store = PresetStore::get();
    auto const& p = store.presets()[m_index];
    auto rep = DecoManager::get().preview(m_lel, p, store.effective(p));
    m_sub->setString(rep.ok ? fmt::format("{} / {}  -  Static preview", m_index + 1, store.presets().size()).c_str()
                            : rep.message.c_str());
}

void StylePickerBar::onUse(CCObject*) {
    cfg::setLastPreset(PresetStore::get().presets()[m_index].id);
    this->cleanup(true);
}

void StylePickerBar::onCancel(CCObject*) { this->cleanup(true); }

void StylePickerBar::onExit() {
    CCNode::onExit();
    if (!m_done) this->cleanup(false);  // editor closed underneath us
}

void StylePickerBar::cleanup(bool restoreMainPanel) {
    if (m_done) return;
    m_done = true;
    if (s_open == this) s_open = nullptr;
    this->unschedule(schedule_selector(StylePickerBar::runPreview));
    auto lel = m_lel;
    DecoManager::get().clearPreview(lel);
    size_t idx = m_index;
    this->removeFromParentAndCleanup(true);
    if (restoreMainPanel && lel) AutoDecoPopup::create(lel, int(idx))->show();
}

}  // namespace adeco::ui
