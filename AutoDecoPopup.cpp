#include "AutoDecoPopup.hpp"

#include "../DecoManager.hpp"
#include "../PresetStore.hpp"
#include "../Settings.hpp"
#include "ColoursPopup.hpp"
#include "PresetPreview.hpp"
#include "StylePickerBar.hpp"
#include "ToolsPopup.hpp"

using namespace geode::prelude;

namespace adeco::ui {

namespace {
constexpr float W = 440.f, H = 262.f;

TempoSettings currentTempo() {
    TempoSettings t;
    t.enabled = cfg::tempoEnabled();
    t.params.bpm = cfg::tempoBpm();
    t.params.offsetSec = double(cfg::tempoOffsetMs()) / 1000.0;
    t.params.startSpeed = speedFromIndex(cfg::tempoSpeed());
    return t;
}

void showResult(RunReport const& rep) {
    for (auto const& n : rep.notes) log::info("Auto Decorator: {}", n);
    Notification::create(rep.message, rep.ok ? NotificationIcon::Success : NotificationIcon::Error, rep.ok ? 5.f : 6.f)->show();
}
}  // namespace

AutoDecoPopup* AutoDecoPopup::create(LevelEditorLayer* lel, int presetIndex) {
    auto ret = new AutoDecoPopup();
    if (ret->init(lel, presetIndex)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AutoDecoPopup::init(LevelEditorLayer* lel, int presetIndex) {
    m_u = fitScale(W, H);
    if (!Popup::init(W * m_u, H * m_u)) return false;
    styleBackground(m_bgSprite);
    if (m_closeBtn) { m_closeBtn->setVisible(false); m_closeBtn->setEnabled(false); }  // we draw our own small "x"

    m_lel = lel;
    auto& store = PresetStore::get();
    if (store.presets().empty()) {
        // Nothing loaded at all: show why instead of an empty panel.
        std::string why = store.errors().empty() ? "No presets were found." : store.errors().front();
        auto l = label("No presets available\n" + why, "chatFont.fnt", 0.5f * m_u, kDanger);
        place(l, W / 2, H / 2);
        return true;
    }
    m_index = presetIndex >= 0 ? size_t(presetIndex) % store.presets().size() : store.lastUsedIndex();
    cfg::setLastPreset(store.presets()[m_index].id);

    // header
    place(label("Auto Deco", "bigFont.fnt", 0.55f * m_u, kText, {0.f, 0.5f}), 22, H - 26);
    place(label("Preview a style on the section in view", "chatFont.fnt", 0.42f * m_u, kMuted, {0.f, 0.5f}), 22, H - 45);
    auto x = pill("x", U(22), U(22), kPanelLight, this, menu_selector(AutoDecoPopup::onX), 0.45f * 1.f);
    place(x, W - 24, H - 26);

    // left: preview card; right: name / description / Change style
    m_previewHolder = CCNode::create();
    place(m_previewHolder, 22, 88);
    float rx = 230.f, rw = W - 22.f - rx;
    m_name = label("", "bigFont.fnt", 0.5f * m_u, kText, {0.f, 0.5f});
    place(m_name, rx, 188);
    m_tag = label("", "chatFont.fnt", 0.4f * m_u, kPrimary, {0.f, 0.5f});
    place(m_tag, rx, 170);
    m_desc = CCLabelBMFont::create("", "chatFont.fnt", U(rw) / (0.42f * m_u), kCCTextAlignmentLeft);
    m_desc->setScale(0.42f * m_u);
    m_desc->setColor(c3(kMuted));
    m_desc->setAnchorPoint({0.f, 1.f});
    place(m_desc, rx, 160);
    place(pill("Change style", U(rw), U(30), kPanelLight, this, menu_selector(AutoDecoPopup::onChangeStyle), 0.42f), rx + rw / 2, 100);

    // action row: Colours | Structure layout | Decorate level
    float bw = (W - 44.f - 16.f) / 3.f, y = 55.f;
    place(pill("Colours", U(bw), U(38), kPanelLight, this, menu_selector(AutoDecoPopup::onColours), 0.42f), 22 + bw / 2, y);
    place(pill("Structure layout", U(bw), U(38), kPanelLight, this, menu_selector(AutoDecoPopup::onStructure), 0.4f, "pillars and walls"),
          22 + bw * 1.5f + 8, y);
    place(pill("Decorate level", U(bw), U(38), kPrimary, this, menu_selector(AutoDecoPopup::onDecorate), 0.42f), 22 + bw * 2.5f + 16, y);

    // footer
    place(pill("Tools", U(70), U(20), kPanelLight, this, menu_selector(AutoDecoPopup::onTools), 0.38f), 22 + 35, 18);
    m_tempo = label("", "chatFont.fnt", 0.36f * m_u, kMuted, {0.5f, 0.5f});
    place(m_tempo, W / 2 + 10, 18);
    place(label("Auto Decorator " + Mod::get()->getVersion().toVString(), "chatFont.fnt", 0.32f * m_u, kMuted, {1.f, 0.5f}), W - 22, 18);

    this->refresh();
    return true;
}

void AutoDecoPopup::refresh() {
    auto& store = PresetStore::get();
    if (store.presets().empty()) return;
    Preset const& p = store.presets()[m_index];
    Palette pal = store.effective(p);

    m_name->setString(p.name.c_str());
    m_name->limitLabelWidth(U(190), 0.5f * m_u, 0.2f);
    m_tag->setString(store.usesCustom(p) ? "Custom colours" : "Original colours");
    m_desc->setString(p.description.c_str());

    m_previewHolder->removeAllChildren();
    float pw = 190.f, ph = 118.f;
    auto frame = rounded(U(pw + 4), U(ph + 4), kPrimary);
    frame->setAnchorPoint({0.f, 0.f});
    m_previewHolder->addChild(frame);
    auto thumb = makePresetPreview(p, pal, {U(pw), U(ph)});
    thumb->setPosition({U(2), U(2)});
    m_previewHolder->addChild(thumb);

    auto t = currentTempo();
    if (t.enabled) m_tempo->setString(fmt::format("Tempo sync: {:.1f} BPM", t.params.bpm).c_str());
    else m_tempo->setString("Tempo sync: off");
}

void AutoDecoPopup::onChangeStyle(CCObject*) {
    auto lel = m_lel;
    size_t idx = m_index;
    this->onClose(nullptr);
    StylePickerBar::open(lel, idx);
}

void AutoDecoPopup::onColours(CCObject*) {
    auto const& p = PresetStore::get().presets()[m_index];
    Ref<AutoDecoPopup> self = this;
    ColoursPopup::create(p, [self]() { self->refresh(); })->show();
}

void AutoDecoPopup::onStructure(CCObject*) {
    auto& store = PresetStore::get();
    Preset const& p = store.presets()[m_index];
    auto rep = DecoManager::get().buildStructure(m_lel, p, store.effective(p));
    showResult(rep);
    if (rep.ok) this->onClose(nullptr);
}

void AutoDecoPopup::onDecorate(CCObject*) {
    auto& store = PresetStore::get();
    Preset const& p = store.presets()[m_index];
    auto rep = DecoManager::get().decorate(m_lel, p, store.effective(p), currentTempo());
    showResult(rep);
    if (rep.ok) this->onClose(nullptr);
}

void AutoDecoPopup::onTools(CCObject*) {
    Ref<AutoDecoPopup> self = this;
    ToolsPopup::create(m_lel, [self]() { self->refresh(); })->show();
}

}  // namespace adeco::ui
