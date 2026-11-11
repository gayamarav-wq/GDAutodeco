#include "ToolsPopup.hpp"

#include <Geode/ui/MDPopup.hpp>
#include <set>

#include "../Catalog.hpp"
#include "../DecoManager.hpp"
#include "../PresetStore.hpp"
#include "TempoPopup.hpp"

using namespace geode::prelude;

namespace adeco::ui {

namespace { constexpr float W = 300.f, H = 262.f; }

ToolsPopup* ToolsPopup::create(LevelEditorLayer* lel, std::function<void()> onChanged) {
    auto ret = new ToolsPopup();
    if (ret->init(lel, std::move(onChanged))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ToolsPopup::init(LevelEditorLayer* lel, std::function<void()> onChanged) {
    m_u = fitScale(W, H);
    if (!Popup::init(W * m_u, H * m_u)) return false;
    styleBackground(m_bgSprite);
    if (m_closeBtn) { m_closeBtn->setVisible(false); m_closeBtn->setEnabled(false); }
    m_lel = lel;
    m_onChanged = std::move(onChanged);

    place(label("Tools", "bigFont.fnt", 0.55f * m_u, kText, {0.f, 0.5f}), 20, H - 24);
    place(pill("x", U(22), U(22), kPanelLight, this, menu_selector(ToolsPopup::onX), 0.45f), W - 24, H - 24);

    auto& dm = DecoManager::get();
    std::string undoText = dm.undoDepth() ? "Undo: " + dm.lastRunLabel() : std::string("Undo last decoration");
    float bw = W - 40.f, y = H - 62.f;
    auto row = [&](std::string const& text, SEL_MenuHandler sel, Rgb bg = kPanelLight) {
        place(pill(text, U(bw), U(30), bg, this, sel, 0.38f), W / 2, y);
        y -= 38.f;
    };
    row(undoText, menu_selector(ToolsPopup::onUndo), dm.undoDepth() ? kPrimary : kPanelLight);
    row("Remove structure pieces", menu_selector(ToolsPopup::onRemoveStructure));
    row("Tempo sync...", menu_selector(ToolsPopup::onTempo));
    row("Verify presets", menu_selector(ToolsPopup::onVerify));
    row("Reload presets", menu_selector(ToolsPopup::onReload));
    row("Mod settings", menu_selector(ToolsPopup::onSettings));
    return true;
}

void ToolsPopup::onUndo(CCObject*) {
    std::string msg;
    bool ok = DecoManager::get().undoLast(m_lel, msg);
    Notification::create(msg, ok ? NotificationIcon::Success : NotificationIcon::Warning, 4.f)->show();
    if (ok && m_onChanged) m_onChanged();
    if (ok) this->onClose(nullptr);
}

void ToolsPopup::onRemoveStructure(CCObject*) {
    int n = 0;
    bool ok = DecoManager::get().removeStructure(m_lel, n);
    Notification::create(ok ? fmt::format("Removed {} structure pieces.", n) : std::string("No structure pieces to remove."),
                         ok ? NotificationIcon::Success : NotificationIcon::Warning, 3.f)->show();
}

void ToolsPopup::onTempo(CCObject*) {
    Ref<ToolsPopup> self = this;
    TempoPopup::create([self]() { if (self->m_onChanged) self->m_onChanged(); })->show();
}

void ToolsPopup::onVerify(CCObject*) {
    auto& store = PresetStore::get();
    GeodeCatalog cat;
    std::string md = fmt::format("**{} presets loaded.**\n\n", store.presets().size());
    if (!store.errors().empty()) {
        md += "### Problems while loading\n";
        for (auto const& e : store.errors()) md += "- " + e + "\n";
        md += "\n";
    }
    int problems = 0;
    for (auto const& p : store.presets()) {
        std::set<int> bad;
        float w, h;
        for (auto const& r : p.rules)
            for (auto const& o : r.objects)
                if (!cat.usable(o.id, w, h)) bad.insert(o.id);
        if (p.structure.enabled && !cat.usable(p.structure.id, w, h)) bad.insert(p.structure.id);
        if (p.rhythm.marker.id > 0 && !cat.usable(p.rhythm.marker.id, w, h)) bad.insert(p.rhythm.marker.id);
        if (bad.empty()) continue;
        ++problems;
        md += "- **" + p.name + "**: unusable object IDs (skipped when decorating):";
        for (int id : bad) md += " " + std::to_string(id);
        md += "\n";
    }
    if (!problems) md += "All object IDs used by the presets exist in this game version and are non-solid decorations.";
    md += "\n\nYour own presets go in:\n`" + store.userPresetDir() + "`";
    MDPopup::create("Preset check", md, "OK")->show();
}

void ToolsPopup::onReload(CCObject*) {
    PresetStore::get().reload();
    Notification::create(fmt::format("Reloaded: {} presets, {} problem(s).", PresetStore::get().presets().size(),
                                     PresetStore::get().errors().size()),
                         NotificationIcon::Success, 3.f)->show();
    if (m_onChanged) m_onChanged();
}

void ToolsPopup::onSettings(CCObject*) {
    openSettingsPopup(Mod::get(), false);
}

}  // namespace adeco::ui
