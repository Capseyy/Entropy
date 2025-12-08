#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include "Renderer/Graphics/ImGui/imgui.h"

struct MapDef {
    std::string display_name;
    uint32_t    map_hash = 0;
};

// NEW: phase info (from TigerActivity.phases)
struct PhaseDef {
    std::string display_name;
    uint32_t    phase_tag = 0;
};

struct ActivityDef {
    std::string display_name;
    uint32_t    activity_id = 0;
    std::vector<MapDef>   maps;    // loadzones
    std::vector<PhaseDef> phases;  // NEW: activity phases (TigerActivity.phases)
};

using ActivityProvider = std::function<const std::vector<ActivityDef>& ()>;

struct ActivityBrowserCallbacks {
    // existing callbacks
    std::function<void(const ActivityDef&, const MapDef&)> on_map_chosen = nullptr;
    std::function<void(const ActivityDef&)>                on_activity_selected = nullptr;

    // NEW: called when both a map and phase are selected
    // (if set, it will be preferred over on_map_chosen)
    std::function<void(const ActivityDef&, const MapDef&, const PhaseDef&)> on_map_phase_chosen = nullptr;
};

// ---------- updated drop-in replacement ----------
inline void DrawActivityBrowser(ActivityProvider provider, const ActivityBrowserCallbacks& cb = {})
{
    if (!provider) { ImGui::TextDisabled("No ActivityProvider set."); return; }
    const auto& activities = provider();

    static int   selectedActivity = -1;
    static int   selectedMap = -1;
    static int   selectedPhase = -1;   // NEW: selected phase index

    static ImGuiTextFilter activityFilter;
    static ImGuiTextFilter mapFilter;

    ImGui::TextUnformatted("Activity & Map Browser");
    ImGui::Separator();

    // Toolbar
    if (ImGui::Button("Refresh")) { /* hook optional refresh */ }
    ImGui::SameLine();
    ImGui::TextDisabled("%d activities", (int)activities.size());

    activityFilter.Draw("Filter activities");

    // ---------- LEFT: Activities ----------
    ImGui::BeginGroup();
    if (ImGui::BeginChild("##activities", ImVec2(ImGui::GetContentRegionAvail().x * 0.35f, 340), true)) {
        if (activities.empty()) {
            ImGui::TextDisabled("No activities available.");
        }
        else {
            std::vector<int> visible;
            visible.reserve(activities.size());
            for (int i = 0; i < (int)activities.size(); ++i) {
                if (activityFilter.PassFilter(activities[i].display_name.c_str()))
                    visible.push_back(i);
            }

            // If current selection is filtered out, clear it
            if (selectedActivity >= 0) {
                if (selectedActivity >= (int)activities.size() ||
                    !activityFilter.PassFilter(activities[selectedActivity].display_name.c_str()))
                {
                    selectedActivity = -1;
                    selectedMap = -1;
                    selectedPhase = -1;
                }
            }

            int visibleCount = 0;
            for (int idx : visible) {
                const auto& a = activities[idx];
                ImGui::PushID(a.activity_id ? (int)a.activity_id : idx);
                const bool isSel = (idx == selectedActivity);
                if (ImGui::Selectable(a.display_name.c_str(), isSel)) {
                    selectedActivity = idx;
                    selectedMap = -1;
                    selectedPhase = -1;     // reset phase on new activity

                    if (cb.on_activity_selected)
                        cb.on_activity_selected(a);
                }
                if (isSel) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
                ++visibleCount;
            }

            if (visibleCount == 0) {
                ImGui::TextDisabled("No activities match the filter.");
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    // ---------- RIGHT: Phases + Maps ----------
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::BeginChild("##maps_panel", ImVec2(0, 340), true)) {
        if (selectedActivity < 0 || selectedActivity >= (int)activities.size()) {
            ImGui::TextDisabled("Select an activity to see its maps.");
        }
        else {
            const auto& act = activities[selectedActivity];
            const auto& maps = act.maps;

            ImGui::TextDisabled("Maps in: %s", act.display_name.c_str());

            // ---------- NEW: Phase selection menu ----------
            if (!act.phases.empty()) {
                // Clamp selection
                if (selectedPhase < 0 || selectedPhase >= (int)act.phases.size())
                    selectedPhase = 0;

                const char* currentPhaseName = act.phases[selectedPhase].display_name.c_str();
                ImGui::TextUnformatted("Phase");
                ImGui::SameLine();
                if (ImGui::BeginCombo("##phase_combo", currentPhaseName)) {
                    for (int i = 0; i < (int)act.phases.size(); ++i) {
                        const bool isSel = (i == selectedPhase);
                        if (ImGui::Selectable(act.phases[i].display_name.c_str(), isSel))
                            selectedPhase = i;
                        if (isSel)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else {
                // No phases; reset selection
                selectedPhase = -1;
                ImGui::TextDisabled("No phases available for this activity.");
            }

            // ---------- Map filter + table ----------
            mapFilter.Draw("Filter maps");
            ImGui::Separator();

            if (ImGui::BeginTable("##maps_table", 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableHeadersRow();

                int visibleIndex = 0;
                for (int i = 0; i < (int)maps.size(); ++i) {
                    const auto& m = maps[i];
                    if (!mapFilter.PassFilter(m.display_name.c_str())) continue;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::PushID(m.map_hash ? (int)m.map_hash : i);
                    bool selected = (i == selectedMap);
                    if (ImGui::Selectable(m.display_name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        selectedMap = i;
                    }

                    // Double-click: map + (optional) phase
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        if (cb.on_map_phase_chosen && selectedPhase >= 0 && selectedPhase < (int)act.phases.size()) {
                            cb.on_map_phase_chosen(act, m, act.phases[selectedPhase]);
                        }
                        else if (cb.on_map_chosen) {
                            cb.on_map_chosen(act, m);
                        }
                    }

                    ImGui::PopID();

                    ImGui::TableNextColumn();
                    ImGui::Text("0x%08X", m.map_hash);
                    ++visibleIndex;
                }

                if (visibleIndex == 0) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("No maps match the filter.");
                    ImGui::TableNextColumn();
                }
                ImGui::EndTable();
            }

            ImGui::Separator();

            const bool mapIndexValid =
                (selectedMap >= 0 && selectedMap < (int)maps.size() &&
                    mapFilter.PassFilter(maps[selectedMap].display_name.c_str()));

            if (!mapIndexValid) ImGui::BeginDisabled();
            if (ImGui::Button("Load Selected Map")) {
                const auto& m = maps[selectedMap];

                // Prefer phase-aware callback if set and we have a valid phase
                if (cb.on_map_phase_chosen && selectedPhase >= 0 &&
                    selectedPhase < (int)act.phases.size())
                {
                    cb.on_map_phase_chosen(act, m, act.phases[selectedPhase]);
                }
                else if (cb.on_map_chosen) {
                    cb.on_map_chosen(act, m);
                }
            }
            if (!mapIndexValid) ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::TextDisabled("%d maps", (int)maps.size());
        }
    }
    ImGui::EndChild();
    ImGui::EndGroup();
}
