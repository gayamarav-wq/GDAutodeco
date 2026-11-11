// Colours panel: edit the five palette colours of one preset (swatch + validated hex field + colour picker),
// switch between the preset's original colours and the player's custom slot, reset, Cancel / Done.
#pragma once

#include <Geode/ui/ColorPickPopup.hpp>
#include <functional>

#include "UiKit.hpp"
#include "core/Preset.hpp"

// Set to 0 if your SDK does not provide geode::ColorPickPopup / ColorPickPopupDelegate (see README "API risks").
// Hex entry keeps working; only the click-the-swatch picker is removed.
#ifndef ADECO_USE_COLOR_PICKER
#define ADECO_USE_COLOR_PICKER 1
#endif

namespace adeco::ui {

class ColoursPopup : public geode::Popup
#if ADECO_USE_COLOR_PICKER
    , public geode::ColorPickPopupDelegate
#endif
{
public:
    static ColoursPopup* create(Preset const& preset, std::function<void()> onDone);

protected:
    bool init(Preset const& preset, std::function<void()> onDone);

    void onSwatch(CCObject* sender);
    void onSlotPrev(CCObject*) { switchSlot(); }
    void onSlotNext(CCObject*) { switchSlot(); }
    void onReset(CCObject*);
    void onDone(CCObject*);
    void onCancel(CCObject*) { this->onClose(nullptr); }
    void onX(CCObject*) { this->onClose(nullptr); }
#if ADECO_USE_COLOR_PICKER
    void updateColor(cocos2d::ccColor4B const& color) override;
#endif

    void switchSlot();
    void setColour(size_t role, Rgb c, int skipInputRole = -1);
    void onHexEdited(size_t role, std::string const& text);
    Palette const& current() const { return m_slot == 0 ? m_original : m_custom; }
    void syncUi(int skipInputRole = -1);

    float U(float v) const { return v * m_u; }
    void place(CCNode* n, float x, float y) { m_buttonMenu->addChildAtPosition(n, Anchor::BottomLeft, ccp(U(x), U(y))); }

    Preset m_preset;  // copy: the store may reload while this panel is open
    std::function<void()> m_onDone;
    Palette m_original, m_custom;
    int m_slot = 0;  // 0 = original colours, 1 = custom colours
    float m_u = 1.f;
    bool m_syncing = false;
    size_t m_pickRole = 0;
    CCNode* m_previewHolder = nullptr;
    CCLabelBMFont* m_slotLabel = nullptr;
    CCScale9Sprite* m_swatch[kRoleCount] = {};
    TextInput* m_input[kRoleCount] = {};
    CCLabelBMFont* m_bad[kRoleCount] = {};
};

}  // namespace adeco::ui
