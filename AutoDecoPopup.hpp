// Main "Auto Deco" panel: style preview, Change style, Colours, Structure layout, Decorate level, Tools.
#pragma once

#include "UiKit.hpp"

namespace adeco::ui {

class AutoDecoPopup : public geode::Popup {
public:
    static AutoDecoPopup* create(LevelEditorLayer* lel, int presetIndex = -1);
    void refresh();  // re-reads the selected preset + palette and redraws the dynamic parts

protected:
    bool init(LevelEditorLayer* lel, int presetIndex);

    void onChangeStyle(CCObject*);
    void onColours(CCObject*);
    void onStructure(CCObject*);
    void onDecorate(CCObject*);
    void onTools(CCObject*);
    void onX(CCObject*) { this->onClose(nullptr); }

    float U(float v) const { return v * m_u; }
    void place(CCNode* n, float x, float y) { m_buttonMenu->addChildAtPosition(n, Anchor::BottomLeft, ccp(U(x), U(y))); }

    geode::Ref<LevelEditorLayer> m_lel;
    size_t m_index = 0;
    float m_u = 1.f;
    CCNode* m_previewHolder = nullptr;
    CCLabelBMFont* m_name = nullptr;
    CCLabelBMFont* m_tag = nullptr;
    CCLabelBMFont* m_desc = nullptr;
    CCLabelBMFont* m_tempo = nullptr;
};

}  // namespace adeco::ui
