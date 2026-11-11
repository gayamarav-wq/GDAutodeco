// Bridges the pure planning core and the live editor: scans the level, runs the planner, creates
// objects, applies the palette, and keeps the undo / preview bookkeeping.
#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/ObjectString.hpp"
#include "core/Tempo.hpp"

namespace adeco {

struct TempoSettings {
    bool enabled = false;
    TempoParams params;
};

struct RunReport {
    bool ok = false;
    std::string message;        // one line the player sees, e.g. "1744 structure pieces added. Edit them ..."
    int structurePieces = 0;
    int decoObjects = 0;
    int triggers = 0;           // colour + pulse triggers
    double bpm = 0;             // 0 = tempo sync not used
    std::vector<std::string> notes;
};

class DecoManager {
public:
    static DecoManager& get();

    // Call whenever an editor opens: forgets bookkeeping that belonged to a previous editor session.
    void attach(LevelEditorLayer* lel);

    // Phase 1 only: grey "pillars and walls". Replaces any pieces from a previous layout run.
    RunReport buildStructure(LevelEditorLayer* lel, Preset const& p, Palette const& pal);
    // Full decoration. Builds the structure layout first if none exists yet.
    RunReport decorate(LevelEditorLayer* lel, Preset const& p, Palette const& pal, TempoSettings const& tempo);
    // Temporary preview of the section currently in view; removed by clearPreview().
    RunReport preview(LevelEditorLayer* lel, Preset const& p, Palette const& pal);
    void clearPreview(LevelEditorLayer* lel);

    // One-step undo of the most recent structure/decoration run.
    bool undoLast(LevelEditorLayer* lel, std::string& message);
    bool removeStructure(LevelEditorLayer* lel, int& removed);

    size_t undoDepth() const { return m_runs.size(); }
    std::string lastRunLabel() const { return m_runs.empty() ? std::string() : m_runs.back().label; }
    bool hasStructure(LevelEditorLayer* lel);

private:
    struct Run {
        std::vector<geode::Ref<GameObject>> objects;
        std::string label;
    };
    struct Scan {
        std::vector<InObject> objects;
        std::vector<SpeedChange> speeds;
        std::unordered_set<int> usedChannels;
        size_t total = 0;
    };

    Scan scan(LevelEditorLayer* lel);
    ChannelMap pickChannels(Scan const& sc);
    uint64_t makeSeed(Preset const& p, Scan const& sc) const;
    int roomForObjects(LevelEditorLayer* lel) const;
    std::vector<geode::Ref<GameObject>> create(LevelEditorLayer* lel, std::vector<std::string> const& strings);
    // Creates the colour triggers for the palette (and applies them to the editor view when enabled).
    std::vector<geode::Ref<GameObject>> applyPalette(LevelEditorLayer* lel, Palette const& pal, ChannelMap const& map,
                                                     Region const& reg);
    void removeAll(LevelEditorLayer* lel, std::vector<geode::Ref<GameObject>>& objs, int* removed = nullptr);
    std::unordered_set<GameObject*> liveSet(LevelEditorLayer* lel);

    RunReport run(LevelEditorLayer* lel, Preset const& p, Palette const& pal, TempoSettings const& tempo,
                  bool doStructure, bool doDeco, bool isPreview);

    LevelEditorLayer* m_editor = nullptr;  // identity only; never dereferenced unless the caller passes it again
    std::vector<Run> m_runs;
    std::vector<geode::Ref<GameObject>> m_structure;  // current (user-editable) structure pieces
    std::vector<geode::Ref<GameObject>> m_preview;
    std::array<int, kRoleCount> m_picked{};  // custom colour channels reserved for this editor session
    bool m_channelsChosen = false;
    int m_variation = 0;  // bumped by undo so "undo, then decorate again" gives a new variation
};

}  // namespace adeco
