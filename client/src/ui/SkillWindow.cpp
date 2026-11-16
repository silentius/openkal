/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SkillWindow.h"

#include "openkal/client/ItemTooltip.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/TalentTree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::client {
    namespace {

        void draw_ttf(engine::IEngine& engine,
                      const FreetypeFont& font,
                      float x,
                      float y,
                      const std::string& text,
                      float r,
                      float g,
                      float b,
                      float scale) {
            if (!font.ready() || text.empty()) {
                return;
            }
            font.draw(engine, x, y, text, r, g, b, scale);
        }

        [[nodiscard]] float
        ttf_width(const FreetypeFont& font, const std::string& text, float scale) {
            return font.ready() ? font.text_width(text, scale) : 0.0f;
        }

        std::vector<const SkillDef*> sorted_class_skills(const Catalog& cat, int clazz) {
            auto list = cat.for_class(clazz);
            std::stable_sort(list.begin(), list.end(), [](const SkillDef* a, const SkillDef* b) {
                if (a->passive() != b->passive()) {
                    return !a->passive() && b->passive();
                }
                if (a->level_limit() != b->level_limit()) {
                    return a->level_limit() < b->level_limit();
                }
                return a->index() < b->index();
            });
            return list;
        }

        int grid_cols(const UiDocument& doc) {
            if (const UiWidget* g = doc.find_widget("active_grid")) {
                return std::max(1, g->cols());
            }
            return 6;
        }
        int active_slots(const UiDocument& doc) {
            if (const UiWidget* g = doc.find_widget("active_grid")) {
                return std::max(1, g->cols()) * std::max(1, g->rows());
            }
            return 18;
        }
        int passive_slots(const UiDocument& doc) {
            if (const UiWidget* g = doc.find_widget("passive_grid")) {
                return std::max(1, g->cols()) * std::max(1, g->rows());
            }
            return 12;
        }
        int total_slots(const UiDocument& doc, SkillTab tab) {
            if (tab == SkillTab::Action) {
                if (const UiWidget* grid = doc.find_widget("action_grid")) {
                    return std::max(1, grid->cols()) * std::max(1, grid->rows());
                }
            }
            return active_slots(doc) + passive_slots(doc);
        }

        bool skill_slot_rect(const UiDocument& doc,
                             float s,
                             SkillTab tab,
                             int slot,
                             float win_x,
                             float win_y,
                             float& ix,
                             float& iy,
                             float& iw,
                             float& ih) {
            const int n_total = total_slots(doc, tab);
            if (slot < 0 || slot >= n_total) {
                return false;
            }
            const float scale = std::max(0.25f, s);
            const int n_active = active_slots(doc);
            const UiWidget* active = doc.find_widget("active_grid");
            const UiWidget* passive = doc.find_widget("passive_grid");
            const bool use_passive = (tab != SkillTab::Action && slot >= n_active);
            const UiWidget* grid = tab == SkillTab::Action ? doc.find_widget("action_grid")
                                                           : (use_passive ? passive : active);
            const int local = use_passive ? slot - n_active : slot;
            const int cols = grid ? std::max(1, grid->cols()) : std::max(1, grid_cols(doc));
            const float icon = (grid && grid->cell_w() > 0.0f ? grid->cell_w() : 32.0f) * scale;
            const float pitch_x =
                (grid && grid->pitch_x() > 0.0f ? grid->pitch_x() : 54.0f) * scale;
            const float pitch_y =
                (grid && grid->pitch_y() > 0.0f ? grid->pitch_y() : 60.0f) * scale;
            const float origin_x = (grid ? grid->x() : 29.0f) * scale;
            const float base_y = (grid ? grid->y() : (use_passive ? 314.0f : 96.0f)) * scale;
            const int col = local % cols;
            const int row = local / cols;
            ix = win_x + origin_x + static_cast<float>(col) * pitch_x;
            iy = win_y + base_y + static_cast<float>(row) * pitch_y;
            iw = icon;
            ih = (grid && grid->cell_h() > 0.0f ? grid->cell_h() : 32.0f) * scale;
            return true;
        }

        bool is_action_skill(const SkillDef& s) {
            // Only the shared Action-tab macros (trade/pick/emotes). Class skills like
            // Rest (action002) stay on Common via grade — never here.
            return Catalog::is_shared_action(s);
        }

        bool tab_unlocked_for_specialty(SkillTab tab, std::uint32_t specialty) {
            using protocol::kSpecialtyFirstJob;
            using protocol::kSpecialtySecondJobA;
            using protocol::kSpecialtySecondJobB;
            using protocol::kSpecialtyThirdJobA;
            using protocol::kSpecialtyThirdJobB;
            using protocol::specialty_meets_grade;
            switch (tab) {
            case SkillTab::Common:
            case SkillTab::Action:
            case SkillTab::Talents:
                return true;
            case SkillTab::Job1:
                return specialty_meets_grade(specialty, kSpecialtyFirstJob);
            case SkillTab::Job2:
                return specialty_meets_grade(specialty, kSpecialtySecondJobA) ||
                       specialty_meets_grade(specialty, kSpecialtySecondJobB);
            case SkillTab::Job3:
                return specialty_meets_grade(specialty, kSpecialtyThirdJobA) ||
                       specialty_meets_grade(specialty, kSpecialtyThirdJobB);
            case SkillTab::Awaken:
                return specialty_meets_grade(specialty, kSpecialtyThirdJobA) ||
                       specialty_meets_grade(specialty, kSpecialtyThirdJobB);
            default:
                return false;
            }
        }

        bool matches_skill_tab(const SkillDef& s, SkillTab tab, std::uint32_t specialty) {
            const bool talent = (s.index() >= protocol::kTalentSkillFirst &&
                                 s.index() <= protocol::kTalentSkillLast) ||
                                s.index() == protocol::kBeaconSkill;
            if (talent || tab == SkillTab::Talents) {
                return talent && tab == SkillTab::Talents;
            }
            if (tab == SkillTab::Action) {
                return is_action_skill(s);
            }
            if (is_action_skill(s)) {
                return false;
            }
            if (!tab_unlocked_for_specialty(tab, specialty)) {
                return false;
            }
            const auto g = static_cast<std::uint32_t>(std::max(0, s.grade_mask()));
            if (!protocol::specialty_meets_grade(specialty, g)) {
                return false;
            }
            switch (tab) {
            case SkillTab::Common:
                return g == protocol::kStartingSpecialty;
            case SkillTab::Job1:
                return g == protocol::kSpecialtyFirstJob;
            case SkillTab::Job2:
                return g == protocol::kSpecialtySecondJobA || g == protocol::kSpecialtySecondJobB;
            case SkillTab::Job3:
                return !s.awaken() &&
                       (g == protocol::kSpecialtyThirdJobA || g == protocol::kSpecialtyThirdJobB);
            case SkillTab::Awaken:
                return s.awaken();
            default:
                return false;
            }
        }

        void skills_for_tab(const Catalog& cat,
                            int clazz,
                            SkillTab tab,
                            int specialty,
                            bool multiplayer,
                            std::vector<const SkillDef*>& active,
                            std::vector<const SkillDef*>& passive) {
            active.clear();
            passive.clear();
            const auto spec = static_cast<std::uint32_t>(std::max(0, specialty));
            for (const SkillDef* s : sorted_class_skills(cat, clazz)) {
                if (!s || s->image().empty() || !matches_skill_tab(*s, tab, spec)) {
                    continue;
                }
                const auto social = social_action(*s);
                if (social == SocialAction::Removed ||
                    (!multiplayer && social != SocialAction::None)) {
                    continue;
                }
                // Class Run (0) / Rest (11) are shown via shared Action-tab entries only.
                if (!Catalog::is_shared_action(*s) && (s->index() == 0 || s->index() == 11)) {
                    continue;
                }
                if (s->passive()) {
                    passive.push_back(s);
                } else {
                    active.push_back(s);
                }
            }
        }

        const SkillDef* skill_at_slot(const UiDocument& doc,
                                      SkillTab tab,
                                      const std::vector<const SkillDef*>& active,
                                      const std::vector<const SkillDef*>& passive,
                                      int slot) {
            if (slot < 0 || slot >= total_slots(doc, tab)) {
                return nullptr;
            }
            if (tab == SkillTab::Action) {
                return (slot < static_cast<int>(active.size()))
                           ? active[static_cast<std::size_t>(slot)]
                           : nullptr;
            }
            const int n_active = active_slots(doc);
            if (slot < n_active) {
                return (slot < static_cast<int>(active.size()))
                           ? active[static_cast<std::size_t>(slot)]
                           : nullptr;
            }
            const int i = slot - n_active;
            return (i < static_cast<int>(passive.size())) ? passive[static_cast<std::size_t>(i)]
                                                          : nullptr;
        }

        constexpr const char* kTabIds[] = {
            "common", "job1", "job2", "job3", "awaken", "action", "talents"};
        constexpr const char* kTabLabels[] = {
            "Common", "1st Job", "2nd Job", "3rd Job", "Awaken", "Action", "Talents"};

    }  // namespace

    class SkillWindow::ViewHost final : public UiViewHost {
      public:
        explicit ViewHost(const SkillWindow& window) : m_window(window) {}

        bool button_enabled(std::string_view action) const override {
            for (int i = 0; i < static_cast<int>(SkillTab::Count); ++i) {
                if (action == std::string("skill.tab.") + kTabIds[i]) {
                    return m_window.tab_unlocked(static_cast<SkillTab>(i));
                }
            }
            return true;
        }

        bool button_pressed(std::string_view id) const override {
            return id == std::string("tab_") + kTabIds[static_cast<int>(m_window.m_tab)] ||
                   UiViewHost::button_pressed(id);
        }

      private:
        const SkillWindow& m_window;
    };

    UiVarMap SkillWindow::view_vars() const {
        UiVarMap vars;
        vars["skill.title"] = "Skills";
        vars["skill.talent_tree_label"] = "Talent Tree";
        vars["skill.action_page"] = m_tab == SkillTab::Action ? "true" : "false";
        vars["skill.class_page"] = m_tab == SkillTab::Action ? "false" : "true";
        vars["skill.tab"] = kTabIds[static_cast<int>(m_tab)];
        vars["ui.active"] = "Active";
        vars["ui.passive"] = "Passive";
        for (int i = 0; i < static_cast<int>(SkillTab::Count); ++i) {
            vars[std::string("ui.tab_") + kTabIds[i]] = kTabLabels[i];
        }
        ui_fill_theme_vars(m_v1, vars);
        return vars;
    }

    void SkillWindow::set_tab(SkillTab tab) {
        m_tab = tab_unlocked(tab) ? tab : SkillTab::Common;
        m_selected_slot = -1;
    }

    void SkillWindow::set_player_specialty(int specialty) {
        m_player_specialty = std::max(0, specialty);
        clamp_tab_to_unlocked();
    }

    bool SkillWindow::tab_unlocked(SkillTab tab) const {
        return tab_unlocked_for_specialty(
            tab, static_cast<std::uint32_t>(std::max(0, m_player_specialty)));
    }

    void SkillWindow::clamp_tab_to_unlocked() {
        if (!tab_unlocked(m_tab)) {
            m_tab = SkillTab::Common;
            m_selected_slot = -1;
        }
    }

    void SkillWindow::ensure_tab_skills_cache() const {
        if (m_cache_catalog == m_catalog && m_cache_clazz == m_player_clazz &&
            m_cache_tab == m_tab && m_cache_specialty == m_player_specialty) {
            return;
        }
        m_cache_catalog = m_catalog;
        m_cache_clazz = m_player_clazz;
        m_cache_tab = m_tab;
        m_cache_specialty = m_player_specialty;
        m_cache_active.clear();
        m_cache_passive.clear();
        if (m_catalog) {
            skills_for_tab(*m_catalog,
                           m_player_clazz,
                           m_tab,
                           m_player_specialty,
                           m_multiplayer,
                           m_cache_active,
                           m_cache_passive);
        }
    }

    void SkillWindow::sync_from_layout() {
        UiView::reload(m_v1, m_layout);
        const float s = std::max(0.25f, m_scale);
        m_w = m_v1.window_w() * s;
        m_h = m_v1.window_h() * s;
    }

    void SkillWindow::sync_fonts(engine::IEngine& engine) {
        const int body_size = std::clamp(m_v1.font_pixel_size(), 8, 48);
        if (!m_body_font.ready() || m_body_font_pixel_size != body_size) {
            const std::string path = FreetypeFont::find_system_sans();
            m_body_font.destroy(engine);
            m_body_font_pixel_size = 0;
            if (!path.empty() && m_body_font.load(engine, path, body_size)) {
                m_body_font_pixel_size = body_size;
            }
        }
        const UiWidget* title = m_v1.find_widget("title");
        const int title_size = title && title->pixel_size() > 0 ? title->pixel_size() : body_size;
        if (!m_title_font.ready() || m_title_font.pixel_size() != title_size) {
            m_title_font.destroy(engine);
            (void) m_title_font.load(engine, FreetypeFont::find_system_sans_bold(), title_size);
        }
        const int tip_px = FreetypeFont::kTipPixelSize;
        if (!m_tip_font.ready() || m_tip_font_pixel_size != tip_px) {
            const std::string path = FreetypeFont::find_system_sans();
            m_tip_font.destroy(engine);
            m_tip_font_pixel_size = 0;
            if (!path.empty() && m_tip_font.load(engine, path, tip_px)) {
                m_tip_font_pixel_size = tip_px;
            }
        }
    }

    void SkillWindow::destroy_fonts(engine::IEngine& engine) {
        m_title_font.destroy(engine);
        m_body_font.destroy(engine);
        m_body_font_pixel_size = 0;
        m_tip_font.destroy(engine);
        m_tip_font_pixel_size = 0;
    }

    void SkillWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        WidgetTools::center_rect(m_x, m_y, m_w, m_h, screen_w, screen_h);
    }

    void SkillWindow::tick(float dt) {
        for (auto& [idx, left] : m_cooldown_left) {
            (void) idx;
            left = std::max(0.0f, left - dt);
        }
    }

    void SkillWindow::begin_cooldown(int skill_index, int delay_pre_ms, int delay_ms) {
        const float sec =
            static_cast<float>(std::max(0, delay_pre_ms) + std::max(0, delay_ms)) / 1000.0f;
        if (sec > 0.0f) {
            m_cooldown_left[skill_index] = sec;
            m_cooldown_total[skill_index] = sec;
        }
    }

    void SkillWindow::sync_cooldown(int skill_index, int remaining_ms, int total_ms) {
        if (remaining_ms <= 0 || total_ms <= 0) {
            m_cooldown_left.erase(skill_index);
            m_cooldown_total.erase(skill_index);
            return;
        }
        m_cooldown_left[skill_index] =
            static_cast<float>(std::min(remaining_ms, total_ms)) / 1000.0f;
        m_cooldown_total[skill_index] = static_cast<float>(total_ms) / 1000.0f;
    }

    void SkillWindow::clear_cooldowns() {
        m_cooldown_left.clear();
        m_cooldown_total.clear();
    }

    bool SkillWindow::on_cooldown(int skill_index) const {
        auto it = m_cooldown_left.find(skill_index);
        return it != m_cooldown_left.end() && it->second > 0.05f;
    }

    float SkillWindow::cooldown_fraction(int skill_index) const {
        auto it = m_cooldown_left.find(skill_index);
        auto tot = m_cooldown_total.find(skill_index);
        if (it == m_cooldown_left.end() || tot == m_cooldown_total.end() || tot->second <= 0.0f) {
            return 0.0f;
        }
        if (it->second <= 0.05f) {
            return 0.0f;
        }
        return std::min(1.0f, it->second / tot->second);
    }

    bool SkillWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(
            mx, my, m_x, m_y, m_w, m_h, s, UiView::frame_style(m_v1));
    }

    void SkillWindow::update_pointer(const engine::FrameInput& in) {
        if (m_drag_skill_index >= 0) {
            const float dx = in.mouse_x - m_skill_press_x;
            const float dy = in.mouse_y - m_skill_press_y;
            m_skill_drag_moved = m_skill_drag_moved || dx * dx + dy * dy >= 16.0f;
        }
        m_hover_slot = -1;
        m_hover_mx = in.mouse_x;
        m_hover_my = in.mouse_y;
        m_mouse_down = in.mouse_left;
        if (!m_open) {
            m_dragging = false;
            return;
        }
        if (m_dragging) {
            if (in.mouse_left) {
                m_x = in.mouse_x - m_drag_dx;
                m_y = in.mouse_y - m_drag_dy;
            } else {
                m_dragging = false;
            }
            return;
        }

        const float s = std::max(0.25f, m_scale);
        ensure_tab_skills_cache();
        const float label_extra = 24.0f * s;
        for (int slot = 0; slot < total_slots(m_v1, m_tab); ++slot) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            if (!skill_slot_rect(m_v1, s, m_tab, slot, m_x, m_y, ix, iy, iw, ih)) {
                continue;
            }
            if (!WidgetTools::point_in_rect(in.mouse_x, in.mouse_y, ix, iy, iw, ih + label_extra)) {
                continue;
            }
            if (skill_at_slot(m_v1, m_tab, m_cache_active, m_cache_passive, slot)) {
                m_hover_slot = slot;
            }
            break;
        }
    }

    int SkillWindow::click_cast_on_release(float mx, float my) const {
        const float dx = mx - m_skill_press_x;
        const float dy = my - m_skill_press_y;
        return m_open && !m_skill_drag_moved && dx * dx + dy * dy < 16.0f &&
                       (Catalog::skill_bar_casts_on_click(m_player_clazz, m_drag_skill_index) ||
                        (m_catalog && m_catalog->find(m_player_clazz, m_drag_skill_index) &&
                         social_action(*m_catalog->find(m_player_clazz, m_drag_skill_index)) !=
                             SocialAction::None))
                   ? m_drag_skill_index
                   : -1;
    }

    SkillClickResult SkillWindow::handle_click(float mx, float my) {
        m_last_cast_index = -1;
        m_last_cast_kind = CastKind::None;
        if (!m_open) {
            return SkillClickResult::Ignored;
        }

        const float s = std::max(0.25f, m_scale);
        const WindowFrameStyle chrome = UiView::frame_style(m_v1);

        const UiHit hit = UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w, m_h, s, view_vars());
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            m_open = false;
            m_dragging = false;
            return SkillClickResult::Consumed;
        }
        if (hit.action() == "skill.talent_tree") {
            return SkillClickResult::OpenTalents;
        }
        if (hit.kind() == UiHitKind::Button) {
            for (int i = 0; i < static_cast<int>(SkillTab::Count); ++i) {
                if (hit.action() == std::string("skill.tab.") + kTabIds[i]) {
                    const auto tab = static_cast<SkillTab>(i);
                    if (tab_unlocked(tab)) {
                        set_tab(tab);
                    }
                    return SkillClickResult::Consumed;
                }
            }
        }

        if (!WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w, m_h, s, chrome)) {
            return SkillClickResult::Ignored;
        }

        // Title / frame chrome drag (outside or along top of client).
        {
            float tx = 0, ty = 0, tw = 0, th = 0;
            WidgetTools::window_title_drag_rect(m_x, m_y, m_w, m_h, s, chrome, tx, ty, tw, th);
            if (WidgetTools::point_in_rect(mx, my, tx, ty, tw, th)) {
                m_dragging = true;
                m_drag_dx = mx - m_x;
                m_drag_dy = my - m_y;
                return SkillClickResult::Consumed;
            }
        }

        // Content interactions require the client box.
        const bool in_client = WidgetTools::point_in_rect(mx, my, m_x, m_y, m_w, m_h);
        if (!in_client) {
            // Click on side/bottom frame padding — drag the window.
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return SkillClickResult::Consumed;
        }

        if (!m_catalog) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return SkillClickResult::Consumed;
        }
        ensure_tab_skills_cache();

        const float label_extra = 24.0f * s;
        for (int slot = 0; slot < total_slots(m_v1, m_tab); ++slot) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            if (!skill_slot_rect(m_v1, s, m_tab, slot, m_x, m_y, ix, iy, iw, ih)) {
                continue;
            }
            if (!WidgetTools::point_in_rect(mx, my, ix, iy, iw, ih + label_extra)) {
                continue;
            }
            m_selected_slot = slot;
            const SkillDef* skill =
                skill_at_slot(m_v1, m_tab, m_cache_active, m_cache_passive, slot);
            if (!skill) {
                return SkillClickResult::Consumed;
            }
            const bool learned = m_player && m_player->is_learned(skill->index());
            const int have = m_player ? m_player->level_of(skill->index()) : 0;
            const int max_lv = std::max(1, skill->max_level());
            if (skill->manual_learn() && !learned && have < max_lv) {
                m_last_cast_index = skill->index();
                return SkillClickResult::Learn;
            }
            if (learned && !skill->passive() &&
                (Catalog::is_castable(*skill) || Catalog::is_shared_action(*skill))) {
                m_drag_skill_index = skill->index();
                m_skill_press_x = mx;
                m_skill_press_y = my;
                m_skill_drag_moved = false;
                return SkillClickResult::DragBegin;
            }
            return SkillClickResult::Consumed;
        }

        m_dragging = true;
        m_drag_dx = mx - m_x;
        m_drag_dy = my - m_y;
        return SkillClickResult::Consumed;
    }

    void SkillWindow::draw(engine::IEngine& engine, WidgetTools& ui) const {
        if (!m_open) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const FreetypeFont& body = m_body_font;
        const RgbColor lv_col{0.92f, 0.89f, 0.83f};
        const UiVarMap vars = view_vars();
        ViewHost host(*this);
        const UiHit hit =
            UiView::hit(m_v1, &ui, m_hover_mx, m_hover_my, m_x, m_y, m_w, m_h, s, vars);
        host.set_pointer_hit(m_dragging ? std::string{} : hit.widget_id(), m_mouse_down);
        UiView::draw(
            engine, ui, m_v1, m_x, m_y, m_w, m_h, s, vars, m_body_font, m_title_font, &host);

        ensure_tab_skills_cache();

        const float level_fs = s;
        for (int slot = 0; slot < total_slots(m_v1, m_tab); ++slot) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            if (!skill_slot_rect(m_v1, s, m_tab, slot, m_x, m_y, ix, iy, iw, ih)) {
                continue;
            }
            const SkillDef* skill =
                skill_at_slot(m_v1, m_tab, m_cache_active, m_cache_passive, slot);
            if (m_selected_slot == slot) {
                const UiWidget* grid = m_v1.find_widget(
                    m_tab == SkillTab::Action
                        ? "action_grid"
                        : (slot >= active_slots(m_v1) ? "passive_grid" : "active_grid"));
                const float fw = (grid && grid->frame_w() > 0 ? grid->frame_w() : 48.0f) * s;
                const float fh = (grid && grid->frame_h() > 0 ? grid->frame_h() : 48.0f) * s;
                ui.draw_tex("dragon_hud/slot_selected.bmp",
                            ix + (iw - fw) * 0.5f,
                            iy + (ih - fh) * 0.5f,
                            fw,
                            fh);
            }
            if (!skill) {
                continue;
            }
            const int lv = m_player ? m_player->level_of(skill->index()) : 0;
            const bool is_action = Catalog::is_shared_action(*skill);
            const bool locked = !is_action && m_player_level < skill->level_limit();
            const bool learned = is_action || lv > 0;
            const std::string icon = ui.resolve_bmp(skill->icon_stem());
            const float cd = cooldown_fraction(skill->index());
            float a = locked ? 0.35f : (learned ? 1.0f : 0.6f);
            if (cd > 0.05f) {
                a = std::min(a, 0.7f);
            }
            if (!icon.empty() && ui.tex(icon)) {
                ui.draw_tex(icon, ix, iy, iw, ih, a);
            } else {
                engine.draw_ui_rect(ix, iy, iw, ih, 0.3f, 0.25f, 0.2f, a);
            }
            if (cd > 0.05f) {
                ui.draw_cooldown_sweep(ix, iy, std::min(iw, ih), cd);
            }
            if (learned && !is_action) {
                const std::string lv_txt = "Lv " + std::to_string(lv);
                if (body.ready()) {
                    body.draw_centered(engine,
                                       ix + iw * 0.5f,
                                       iy + ih + 9.0f * s,
                                       lv_txt,
                                       lv_col.r(),
                                       lv_col.g(),
                                       lv_col.b(),
                                       level_fs);
                }
            }
        }
    }

    void SkillWindow::draw_tooltip(engine::IEngine& engine,
                                   WidgetTools& ui,
                                   float screen_w,
                                   float screen_h) const {
        if (!m_open) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const FreetypeFont& tip_face = m_tip_font.ready() ? m_tip_font : m_body_font;
        ensure_tab_skills_cache();
        if (m_hover_slot >= 0 && m_catalog && tip_face.ready()) {
            const SkillDef* tip =
                skill_at_slot(m_v1, m_tab, m_cache_active, m_cache_passive, m_hover_slot);
            if (tip && !Catalog::is_shared_action(*tip)) {
                // Match ItemTooltip: tip_font at native 1.0 scale (not UI s).
                const float tip_fs = 1.0f;
                const float tip_line_h = tip_face.line_height() * tip_fs + 2.0f;
                const float border = WidgetTools::overlay_border_thickness(s);
                const float pad = 6.0f + border;
                const float max_text_w = 280.0f;

                auto wrap_lines = [&](const std::string& text) {
                    std::vector<std::string> lines;
                    std::string cur;
                    auto flush = [&]() {
                        lines.push_back(cur);
                        cur.clear();
                    };
                    for (std::size_t i = 0; i <= text.size(); ++i) {
                        const char ch = (i < text.size()) ? text[i] : '\n';
                        if (ch == '\n') {
                            flush();
                            continue;
                        }
                        cur.push_back(ch);
                        if (ttf_width(tip_face, cur, tip_fs) > max_text_w) {
                            cur.pop_back();
                            if (cur.empty()) {
                                cur.push_back(ch);
                                flush();
                            } else {
                                flush();
                                cur.push_back(ch);
                            }
                        }
                    }
                    if (!cur.empty()) {
                        flush();
                    }
                    if (lines.empty()) {
                        lines.emplace_back();
                    }
                    return lines;
                };

                const std::string tip_title = tip->display_name();
                const auto body_lines = wrap_lines(tip->tooltip_body());

                const int learned = m_player ? m_player->level_of(tip->index()) : 0;
                const int preview_lv = std::max(1, learned);
                const std::string ability_text = tip->format_ability(preview_lv, m_ability_vars);
                const auto ability_lines =
                    ability_text.empty() ? std::vector<std::string>{} : wrap_lines(ability_text);

                std::vector<std::string> meta_lines;
                if (tip->max_level() > 0) {
                    meta_lines.push_back("Lv " + std::to_string(learned) + " / " +
                                         std::to_string(tip->max_level()));
                }
                std::vector<std::string> next_lines;
                if (tip->passive() && learned > 0 && learned < tip->max_level()) {
                    const std::string next_ab = tip->format_ability(learned + 1, m_ability_vars);
                    if (!next_ab.empty() && next_ab != ability_text) {
                        next_lines = wrap_lines(std::string("Next: ") + next_ab);
                    }
                }

                struct CondLine {
                    std::string text;
                    bool met = true;
                };
                std::vector<CondLine> conds;
                int required_level = tip->level_limit();
                if (learned < tip->max_level()) {
                    const auto& auto_lv = tip->auto_learn_levels();
                    if (learned >= 0 && learned < static_cast<int>(auto_lv.size()) &&
                        auto_lv[static_cast<std::size_t>(learned)] > 0) {
                        required_level = auto_lv[static_cast<std::size_t>(learned)];
                    }
                }
                if (required_level > 0) {
                    CondLine c;
                    c.text = "Required Level: " + std::to_string(required_level);
                    c.met = m_player_level >= required_level;
                    conds.push_back(std::move(c));
                }
                if (tip->require_skill_index() > 0 && tip->require_skill_level() > 0) {
                    const SkillDef* need =
                        m_catalog->find(m_player_clazz, tip->require_skill_index());
                    CondLine c;
                    c.text = "Required Skill: ";
                    c.text += need ? need->display_name()
                                   : ("#" + std::to_string(tip->require_skill_index()));
                    c.text += " Lv " + std::to_string(tip->require_skill_level());
                    const int have = m_player ? m_player->level_of(tip->require_skill_index()) : 0;
                    c.met = have >= tip->require_skill_level();
                    conds.push_back(std::move(c));
                }

                float content_w = ttf_width(tip_face, tip_title, tip_fs);
                for (const auto& ln : body_lines) {
                    content_w = std::max(content_w, ttf_width(tip_face, ln, tip_fs));
                }
                for (const auto& ln : ability_lines) {
                    content_w = std::max(content_w, ttf_width(tip_face, ln, tip_fs));
                }
                for (const auto& ln : meta_lines) {
                    content_w = std::max(content_w, ttf_width(tip_face, ln, tip_fs));
                }
                for (const auto& ln : next_lines) {
                    content_w = std::max(content_w, ttf_width(tip_face, ln, tip_fs));
                }
                for (const auto& c : conds) {
                    content_w = std::max(content_w, ttf_width(tip_face, c.text, tip_fs));
                }

                const int body_count = static_cast<int>(body_lines.size());
                const int ability_count = static_cast<int>(ability_lines.size());
                const int meta_count = static_cast<int>(meta_lines.size());
                const int next_count = static_cast<int>(next_lines.size());
                const int cond_count = static_cast<int>(conds.size());
                const float gap_ability = ability_count > 0 ? tip_line_h * 0.35f : 0.0f;
                const float gap_before_cond = cond_count > 0 ? tip_line_h * 0.5f : 0.0f;
                const float box_w = content_w + pad * 2.0f;
                const float box_h =
                    pad * 2.0f +
                    tip_line_h * static_cast<float>(1 + body_count + ability_count + meta_count +
                                                    next_count + cond_count) +
                    gap_ability + gap_before_cond;

                float tip_x = 0.0f;
                float tip_y = 0.0f;
                ItemTooltip::place_near_cursor(
                    m_hover_mx, m_hover_my, box_w, box_h, screen_w, screen_h, tip_x, tip_y);

                ui.draw_overlay_panel(tip_x, tip_y, box_w, box_h, s);

                float ty = tip_y + pad;
                const float tx = tip_x + pad;
                draw_ttf(engine, tip_face, tx, ty, tip_title, 1.0f, 0.95f, 0.75f, tip_fs);
                ty += tip_line_h;
                for (const auto& ln : body_lines) {
                    draw_ttf(engine, tip_face, tx, ty, ln, 0.92f, 0.92f, 0.88f, tip_fs);
                    ty += tip_line_h;
                }
                ty += gap_ability;
                for (const auto& ln : ability_lines) {
                    draw_ttf(engine, tip_face, tx, ty, ln, 0.75f, 0.92f, 1.0f, tip_fs);
                    ty += tip_line_h;
                }
                for (const auto& ln : meta_lines) {
                    draw_ttf(engine, tip_face, tx, ty, ln, 0.85f, 0.85f, 0.7f, tip_fs);
                    ty += tip_line_h;
                }
                for (const auto& ln : next_lines) {
                    draw_ttf(engine, tip_face, tx, ty, ln, 0.7f, 0.85f, 0.95f, tip_fs);
                    ty += tip_line_h;
                }
                ty += gap_before_cond;
                for (const auto& c : conds) {
                    if (c.met) {
                        draw_ttf(engine, tip_face, tx, ty, c.text, 1.0f, 1.0f, 1.0f, tip_fs);
                    } else {
                        draw_ttf(engine, tip_face, tx, ty, c.text, 1.0f, 0.25f, 0.25f, tip_fs);
                    }
                    ty += tip_line_h;
                }
            } else if (tip && Catalog::is_shared_action(*tip)) {
                const float tip_fs = 1.0f;
                const float tip_line_h = tip_face.line_height() * tip_fs + 2.0f;
                const float border = WidgetTools::overlay_border_thickness(s);
                const float pad = 6.0f + border;
                const std::string tip_title = tip->display_name();
                const std::string tip_body = tip->tooltip_body();
                float content_w = std::max(ttf_width(tip_face, tip_title, tip_fs),
                                           ttf_width(tip_face, tip_body, tip_fs));
                const float box_w = content_w + pad * 2.0f;
                const float box_h = pad * 2.0f + tip_line_h * (tip_body.empty() ? 1.0f : 2.0f);
                float tip_x = 0.0f;
                float tip_y = 0.0f;
                ItemTooltip::place_near_cursor(
                    m_hover_mx, m_hover_my, box_w, box_h, screen_w, screen_h, tip_x, tip_y);
                ui.draw_overlay_panel(tip_x, tip_y, box_w, box_h, s);
                draw_ttf(engine,
                         tip_face,
                         tip_x + pad,
                         tip_y + pad,
                         tip_title,
                         1.0f,
                         0.95f,
                         0.75f,
                         tip_fs);
                if (!tip_body.empty()) {
                    draw_ttf(engine,
                             tip_face,
                             tip_x + pad,
                             tip_y + pad + tip_line_h,
                             tip_body,
                             0.92f,
                             0.92f,
                             0.88f,
                             tip_fs);
                }
            }
        }
    }

    void SkillWindow::draw_drag_ghost(WidgetTools& ui, float mx, float my) const {
        if (m_drag_skill_index < 0 || !m_catalog) {
            return;
        }
        const SkillDef* d = m_catalog->find(m_player_clazz, m_drag_skill_index);
        if (!d) {
            return;
        }
        const std::string icon = ui.resolve_bmp(d->icon_stem());
        if (icon.empty() || !ui.tex(icon)) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const float half = UiLayout::get_f(m_layout, "drag_ghost.half", 16.0f) * s;
        const float size = UiLayout::get_f(m_layout, "drag_ghost.size", 32.0f) * s;
        const float alpha = UiLayout::get_f(m_layout, "drag_ghost.alpha", 0.85f);
        ui.draw_tex(icon, mx - half, my - half, size, size, alpha);
    }

}  // namespace openkal::client
