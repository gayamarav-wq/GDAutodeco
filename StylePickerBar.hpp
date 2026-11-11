// The style picker from the reference flow: a slim bar across the top of the editor with  < >, the style
// name, "n / N - Static preview", "Use this style" and "Cancel". It is NOT modal: the player can scroll and
// zoom the editor to inspect the live preview that is placed in the visible section.
#pragma once

#include "UiKit.hpp"

namespace adeco::ui {

class StylePickerBar : public CCNode {
public:
    static StylePickerBar* open(LevelEditorLayer* lel, size_t startIndex);

protected:
    bool init(LevelEditorLayer* lel, size_t startIndex);
    void onExit() override;

    void onPrev(CCObject*) { step(-1); }
    void onNext(CCObject*) { step(+1); }
    void onUse(CCObject*);
    void onCancel(CCObject*);
    void step(int delta);
    void showCurrent();
    void runPreview(float dt);
    void cleanup(bool restoreMainPanel);

    geode::Ref<LevelEditorLayer> m_lel;
    size_t m_index = 0;
    bool m_done = false;
    CCLabelBMFont* m_name = nullptr;
    CCLabelBMFont* m_sub = nullptr;
};

}  // namespace adeco::ui
