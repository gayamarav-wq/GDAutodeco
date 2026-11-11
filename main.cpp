// Auto Decorator - entry point. Adds an "Auto Deco" button to the level editor toolbar.
#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include <cfloat>

#include "DecoManager.hpp"
#include "Settings.hpp"
#include "ui/AutoDecoPopup.hpp"

using namespace geode::prelude;

class $modify(AutoDecoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;
        adeco::DecoManager::get().attach(lel);
        this->addAutoDecoButton();
        return true;
    }

    void addAutoDecoButton() {
        // The Build / Edit / Delete mode buttons live in one menu; we add ours to the same menu so it shares
        // their touch priority and sits "next to" them.
        if (!m_buildModeBtn) return;
        CCNode* parent = m_buildModeBtn->getParent();
        if (!parent) return;
        if (parent->getChildByID("auto-deco-button"_spr)) return;

        auto spr = ButtonSprite::create("Auto Deco", 90, true, "bigFont.fnt", "GJ_button_02.png", 30.f, 0.6f);
        spr->setScale(0.62f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AutoDecoEditorUI::onAutoDeco));
        btn->setID("auto-deco-button"_spr);

        // Bounding box of the existing mode buttons, in the shared parent's coordinates.
        float minX = FLT_MAX, maxX = -FLT_MAX, minY = FLT_MAX, maxY = -FLT_MAX;
        for (CCNode* b : {static_cast<CCNode*>(m_buildModeBtn), static_cast<CCNode*>(m_editModeBtn),
                          static_cast<CCNode*>(m_deleteModeBtn)}) {
            if (!b || b->getParent() != parent) continue;
            CCRect r = b->boundingBox();
            minX = std::min(minX, r.getMinX()); maxX = std::max(maxX, r.getMaxX());
            minY = std::min(minY, r.getMinY()); maxY = std::max(maxY, r.getMaxY());
        }
        CCSize bs = btn->getContentSize();
        float x = maxX + bs.width * 0.5f + 4.f;  // just right of the Build / Edit / Delete column...
        float y = minY + bs.height * 0.5f;       // ...level with the bottom (Delete) row
        x += adeco::cfg::buttonOffsetX();
        y += adeco::cfg::buttonOffsetY();
        btn->setPosition({x, y});
        parent->addChild(btn);
    }

    void onAutoDeco(CCObject*) {
        adeco::ui::AutoDecoPopup::create(m_editorLayer)->show();
    }
};
