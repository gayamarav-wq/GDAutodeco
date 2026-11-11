// Tools: undo, remove structure pieces, tempo sync, verify/reload presets, mod settings.
#pragma once

#include <functional>

#include "UiKit.hpp"

namespace adeco::ui {

class ToolsPopup : public geode::Popup {
public:
    static ToolsPopup* create(LevelEditorLayer* lel, std::function<void()> onChanged);

protected:
    bool init(LevelEditorLayer* lel, std::function<void()> onChanged);
    void onUndo(CCObject*);
    void onRemoveStructure(CCObject*);
    void onTempo(CCObject*);
    void onVerify(CCObject*);
    void onReload(CCObject*);
    void onSettings(CCObject*);
    void onX(CCObject*) { this->onClose(nullptr); }

    float U(float v) const { return v * m_u; }
    void place(CCNode* n, float x, float y) { m_buttonMenu->addChildAtPosition(n, Anchor::BottomLeft, ccp(U(x), U(y))); }

    geode::Ref<LevelEditorLayer> m_lel;
    std::function<void()> m_onChanged;
    float m_u = 1.f;
};

}  // namespace adeco::ui
