// Tempo sync settings: on/off, BPM (manual or tap), first-beat offset, starting speed.
#pragma once

#include <functional>
#include <vector>

#include "UiKit.hpp"

namespace adeco::ui {

class TempoPopup : public geode::Popup {
public:
    static TempoPopup* create(std::function<void()> onDone);

protected:
    bool init(std::function<void()> onDone);
    void onToggle(CCObject*);
    void onTap(CCObject*);
    void onSpeedPrev(CCObject*) { cycleSpeed(-1); }
    void onSpeedNext(CCObject*) { cycleSpeed(+1); }
    void onDone(CCObject*);
    void onCancel(CCObject*) { this->onClose(nullptr); }
    void onX(CCObject*) { this->onClose(nullptr); }

    void cycleSpeed(int d);
    void updateInfo();
    float U(float v) const { return v * m_u; }
    void place(CCNode* n, float x, float y) { m_buttonMenu->addChildAtPosition(n, Anchor::BottomLeft, ccp(U(x), U(y))); }

    std::function<void()> m_onDone;
    float m_u = 1.f;
    bool m_enabled = false;
    int m_speed = 1;
    std::vector<double> m_taps;
    CCMenuItemToggler* m_toggle = nullptr;
    TextInput* m_bpmInput = nullptr;
    TextInput* m_offsetInput = nullptr;
    CCLabelBMFont* m_speedLabel = nullptr;
    CCLabelBMFont* m_info = nullptr;
};

}  // namespace adeco::ui
