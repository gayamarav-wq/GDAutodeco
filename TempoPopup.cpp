#include "TempoPopup.hpp"

#include <chrono>
#include <cstdlib>

#include "../Settings.hpp"
#include "../core/Tempo.hpp"

using namespace geode::prelude;

namespace adeco::ui {

namespace {
constexpr float W = 380.f, H = 262.f;

double nowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

TempoPopup* TempoPopup::create(std::function<void()> onDone) {
    auto ret = new TempoPopup();
    if (ret->init(std::move(onDone))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool TempoPopup::init(std::function<void()> onDone) {
    m_u = fitScale(W, H);
    if (!Popup::init(W * m_u, H * m_u)) return false;
    styleBackground(m_bgSprite);
    if (m_closeBtn) { m_closeBtn->setVisible(false); m_closeBtn->setEnabled(false); }
    m_onDone = std::move(onDone);
    m_enabled = cfg::tempoEnabled();
    m_speed = std::clamp(cfg::tempoSpeed(), 0, 4);

    place(label("Tempo sync", "bigFont.fnt", 0.55f * m_u, kText, {0.f, 0.5f}), 20, H - 24);
    place(label("Pulses and accents on the beat", "chatFont.fnt", 0.42f * m_u, kMuted, {0.f, 0.5f}), 20, H - 42);
    place(pill("x", U(22), U(22), kPanelLight, this, menu_selector(TempoPopup::onX), 0.45f), W - 24, H - 24);

    // row 1: enable
    place(label("Sync decorations to the beat", "bigFont.fnt", 0.3f * m_u, kText, {0.f, 0.5f}), 20, H - 76);
    m_toggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(TempoPopup::onToggle), 0.7f * m_u);
    m_toggle->toggle(m_enabled);
    place(m_toggle, W - 40, H - 76);

    // row 2: BPM + tap
    place(label("BPM", "bigFont.fnt", 0.3f * m_u, kText, {0.f, 0.5f}), 20, H - 112);
    m_bpmInput = TextInput::create(U(96) / 0.8f, "120", "chatFont.fnt");
    m_bpmInput->setScale(0.8f);
    m_bpmInput->setFilter("0123456789.");
    m_bpmInput->setMaxCharCount(6);
    m_bpmInput->setString(fmt::format("{:g}", cfg::tempoBpm()));
    place(m_bpmInput, 190, H - 112);
    place(pill("Tap", U(64), U(26), kPanelLight, this, menu_selector(TempoPopup::onTap), 0.42f), W - 56, H - 112);

    // row 3: offset
    place(label("First beat offset (ms)", "bigFont.fnt", 0.3f * m_u, kText, {0.f, 0.5f}), 20, H - 148);
    m_offsetInput = TextInput::create(U(96) / 0.8f, "0", "chatFont.fnt");
    m_offsetInput->setScale(0.8f);
    m_offsetInput->setFilter("-0123456789");
    m_offsetInput->setMaxCharCount(6);
    m_offsetInput->setString(std::to_string(cfg::tempoOffsetMs()));
    place(m_offsetInput, 190 + 70, H - 148);

    // row 4: starting speed (speed portals in the level are followed automatically)
    place(label("Starting speed", "bigFont.fnt", 0.3f * m_u, kText, {0.f, 0.5f}), 20, H - 184);
    place(pill("<", U(22), U(22), kPanelLight, this, menu_selector(TempoPopup::onSpeedPrev), 0.45f), 220, H - 184);
    m_speedLabel = label("", "bigFont.fnt", 0.4f * m_u, kText);
    place(m_speedLabel, 262, H - 184);
    place(pill(">", U(22), U(22), kPanelLight, this, menu_selector(TempoPopup::onSpeedNext), 0.45f), 304, H - 184);

    m_info = label("", "chatFont.fnt", 0.42f * m_u, kPrimary, {0.f, 0.5f});
    place(m_info, 20, 66);
    auto note = CCLabelBMFont::create(
        "Tempo cannot be detected automatically: Geometry Dash does not expose a song's BPM. Enter it, or tap along.",
        "chatFont.fnt", U(W - 40) / (0.34f * m_u), kCCTextAlignmentLeft);
    note->setScale(0.34f * m_u);
    note->setColor(c3(kMuted));
    note->setAnchorPoint({0.f, 1.f});
    place(note, 20, 58);

    place(pill("Cancel", U(110), U(28), kPanelLight, this, menu_selector(TempoPopup::onCancel), 0.42f), W / 2 - 60, 20);
    place(pill("Done", U(110), U(28), kPrimary, this, menu_selector(TempoPopup::onDone), 0.42f), W / 2 + 60, 20);

    this->cycleSpeed(0);
    this->updateInfo();
    return true;
}

void TempoPopup::onToggle(CCObject*) {
    m_enabled = !m_toggle->isToggled();  // CCMenuItemToggler reports the state before the flip inside the callback
    this->updateInfo();
}

void TempoPopup::onTap(CCObject*) {
    double t = nowSeconds();
    if (!m_taps.empty() && t - m_taps.back() > 2.5) m_taps.clear();  // a long pause starts a new measurement
    m_taps.push_back(t);
    if (auto bpm = tapTempo(m_taps)) {
        m_bpmInput->setString(fmt::format("{:.1f}", *bpm));
        this->updateInfo();
    }
}

void TempoPopup::cycleSpeed(int d) {
    m_speed = (m_speed + d + 5) % 5;
    m_speedLabel->setString(speedName(speedFromIndex(m_speed)));
}

void TempoPopup::updateInfo() {
    double bpm = 0;
    if (!m_enabled) m_info->setString("Tempo sync is off");
    else if (parseBpm(m_bpmInput->getString(), bpm)) m_info->setString(fmt::format("BPM used: {:.1f} (manual)", bpm).c_str());
    else m_info->setString("BPM must be a number from 20 to 400");
}

void TempoPopup::onDone(CCObject*) {
    double bpm = cfg::tempoBpm();
    if (!parseBpm(m_bpmInput->getString(), bpm)) {
        Notification::create("BPM must be a number from 20 to 400", NotificationIcon::Error, 3.f)->show();
        return;  // stay open so the player can fix it
    }
    long off = std::strtol(m_offsetInput->getString().c_str(), nullptr, 10);
    off = std::clamp(off, -5000L, 20000L);
    cfg::setTempoEnabled(m_enabled);
    cfg::setTempoBpm(bpm);
    cfg::setTempoOffsetMs(int(off));
    cfg::setTempoSpeed(m_speed);
    if (m_onDone) m_onDone();
    this->onClose(nullptr);
}

}  // namespace adeco::ui
