#include "ColoursPopup.hpp"

#include "../PresetStore.hpp"
#include "PresetPreview.hpp"

using namespace geode::prelude;

namespace adeco::ui {

namespace {
constexpr float W = 440.f, H = 286.f;
}

ColoursPopup* ColoursPopup::create(Preset const& preset, std::function<void()> onDone) {
    auto ret = new ColoursPopup();
    if (ret->init(preset, std::move(onDone))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ColoursPopup::init(Preset const& preset, std::function<void()> onDone) {
    m_u = fitScale(W, H);
    if (!Popup::init(W * m_u, H * m_u)) return false;
    styleBackground(m_bgSprite);
    if (m_closeBtn) { m_closeBtn->setVisible(false); m_closeBtn->setEnabled(false); }

    m_preset = preset;
    m_onDone = std::move(onDone);
    auto& store = PresetStore::get();
    m_original = preset.palette;
    m_custom = store.customOrDefault(preset);
    m_slot = store.usesCustom(preset) ? 1 : 0;

    place(label("Colours", "bigFont.fnt", 0.55f * m_u, kText, {0.f, 0.5f}), 20, H - 24);
    place(label(preset.name, "chatFont.fnt", 0.42f * m_u, kMuted, {0.f, 0.5f}), 20, H - 42);
    place(pill("x", U(22), U(22), kPanelLight, this, menu_selector(ColoursPopup::onX), 0.45f), W - 24, H - 24);

    // left column: live thumbnail, slot switcher, reset, hint
    m_previewHolder = CCNode::create();
    place(m_previewHolder, 20, H - 142);
    place(pill("<", U(22), U(22), kPanelLight, this, menu_selector(ColoursPopup::onSlotPrev), 0.45f), 31, H - 168);
    place(pill(">", U(22), U(22), kPanelLight, this, menu_selector(ColoursPopup::onSlotNext), 0.45f), 143, H - 168);
    m_slotLabel = label("", "chatFont.fnt", 0.42f * m_u, kText);
    place(m_slotLabel, 87, H - 168);
    place(pill("Reset to preset", U(134), U(24), kPanelLight, this, menu_selector(ColoursPopup::onReset), 0.36f), 87, H - 202);
    auto hint = CCLabelBMFont::create("Click a swatch for RGB and Hex controls.", "chatFont.fnt", U(134) / (0.36f * m_u), kCCTextAlignmentLeft);
    hint->setScale(0.36f * m_u);
    hint->setColor(c3(kMuted));
    hint->setAnchorPoint({0.f, 1.f});
    place(hint, 20, H - 224);

    // right column: five rows
    for (size_t i = 0; i < kRoleCount; ++i) {
        float y = H - 72.f - float(i) * 34.f;
        place(label(kRoleNames[i], "bigFont.fnt", 0.3f * m_u, kText, {0.f, 0.5f}), 172, y);

        m_swatch[i] = rounded(U(60), U(22), m_original.c[i]);
        auto swatchItem = CCMenuItemSpriteExtra::create(m_swatch[i], nullptr, this, menu_selector(ColoursPopup::onSwatch));
        swatchItem->setTag(int(i));
        place(swatchItem, 270, y);

        m_input[i] = TextInput::create(U(104) / 0.75f, "#RRGGBB", "chatFont.fnt");
        m_input[i]->setScale(0.75f);
        m_input[i]->setFilter("0123456789abcdefABCDEF#");
        m_input[i]->setMaxCharCount(7);
        m_input[i]->setCallback([this, i](std::string const& text) { this->onHexEdited(i, text); });
        place(m_input[i], 352, y);

        m_bad[i] = label("!", "bigFont.fnt", 0.45f * m_u, kDanger);
        m_bad[i]->setVisible(false);
        place(m_bad[i], 412, y);
    }

    place(pill("Cancel", U(116), U(30), kPanelLight, this, menu_selector(ColoursPopup::onCancel), 0.42f), 172 + 58, 26);
    place(pill("Done", U(116), U(30), kPrimary, this, menu_selector(ColoursPopup::onDone), 0.42f), 172 + 58 + 124, 26);

    syncUi();
    return true;
}

void ColoursPopup::syncUi(int skipInputRole) {
    m_syncing = true;
    Palette const& pal = current();
    for (size_t i = 0; i < kRoleCount; ++i) {
        m_swatch[i]->setColor(c3(pal.c[i]));
        if (int(i) != skipInputRole) {
            m_input[i]->setString(toHex(pal.c[i]));
            m_bad[i]->setVisible(false);
        }
    }
    m_slotLabel->setString(m_slot == 0 ? "Original colours" : "Custom colours");
    m_previewHolder->removeAllChildren();
    float pw = 134.f, ph = 84.f;
    auto frame = rounded(U(pw + 4), U(ph + 4), kPrimary);
    frame->setAnchorPoint({0.f, 0.f});
    m_previewHolder->addChild(frame);
    auto thumb = makePresetPreview(m_preset, pal, {U(pw), U(ph)});  // updates live on every edit
    thumb->setPosition({U(2), U(2)});
    m_previewHolder->addChild(thumb);
    m_syncing = false;
}

void ColoursPopup::switchSlot() {
    m_slot = 1 - m_slot;
    syncUi();
}

void ColoursPopup::setColour(size_t role, Rgb c, int skipInputRole) {
    // The first edit made while viewing the original colours starts the custom slot from those colours.
    if (m_slot == 0) { m_custom = m_original; m_slot = 1; }
    m_custom.c[role] = c;
    syncUi(skipInputRole);
}

void ColoursPopup::onHexEdited(size_t role, std::string const& text) {
    if (m_syncing) return;
    Rgb c;
    bool ok = parseHex(text, c);  // exactly 6 hex digits, optional leading '#'
    m_bad[role]->setVisible(!ok && !text.empty());
    if (ok && c != current().c[role]) setColour(role, c, int(role));  // keep the field the player is typing in
}

void ColoursPopup::onSwatch(CCObject* sender) {
#if ADECO_USE_COLOR_PICKER
    m_pickRole = size_t(static_cast<CCNode*>(sender)->getTag());
    Rgb c = current().c[m_pickRole];
    auto picker = ColorPickPopup::create(ccColor4B{c.r, c.g, c.b, 255});  // API-RISK: ColorPickPopup::create signature
    picker->setDelegate(this);                                           // API-RISK: delegate hookup
    picker->show();
#else
    (void)sender;
#endif
}

#if ADECO_USE_COLOR_PICKER
void ColoursPopup::updateColor(ccColor4B const& color) {
    setColour(m_pickRole, {color.r, color.g, color.b});
}
#endif

void ColoursPopup::onReset(CCObject*) {
    m_custom = m_original;  // "Reset to preset": custom colours become the preset's own again
    m_slot = 0;
    syncUi();
}

void ColoursPopup::onDone(CCObject*) {
    // Saved in the mod's save data (never in the level file).
    PresetStore::get().saveCustom(m_preset, m_custom, m_slot == 1);
    if (m_onDone) m_onDone();
    this->onClose(nullptr);
}

}  // namespace adeco::ui
