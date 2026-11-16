/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BootShell.h"

#include "openkal/client/AnimatedActor.h"
#include "openkal/client/CharacterCreationDraft.h"
#include "openkal/client/Equipment.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/LoadingScreen.h"
#include "openkal/client/SoftCursor.h"
#include "openkal/client/SpriteFx.h"
#include "openkal/client/UiView.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/server/Combat.h"
#include "openkal/server/LobbyClient.h"
#include "openkal/server/world/StarterKit.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    namespace {
        /** Set true when multiplayer mode select should be shown again. */
        constexpr bool kOfferMultiplayerModeSelect = false;

        /** Base / selected scale for chargen previews. */
        constexpr float kChargenScale = 1.062f;

        /** Maximum number of characters available to one local account. */
        constexpr int kMaxCharactersPerAccount = static_cast<int>(server::kMaxCharactersPerAccount);

        enum class CharacterDeleteModal { Closed, Confirm, DeletedList };

        [[nodiscard]] std::int64_t unix_seconds_now() {
            using clock = std::chrono::system_clock;
            return std::chrono::duration_cast<std::chrono::seconds>(clock::now().time_since_epoch())
                .count();
        }

        [[nodiscard]] int remaining_deletion_days(std::int64_t delete_after,
                                                  std::int64_t now_seconds) {
            const std::int64_t seconds = std::max<std::int64_t>(0, delete_after - now_seconds);
            constexpr std::int64_t kSecondsPerDay = 24LL * 60LL * 60LL;
            return static_cast<int>((seconds + kSecondsPerDay - 1) / kSecondsPerDay);
        }

        class CharacterSelectionHost final : public UiViewHost {
          public:
            CharacterSelectionHost(bool active, bool previous, bool next)
                : m_active(active), m_previous(previous), m_next(next) {}

            bool button_enabled(std::string_view action) const override {
                if (action == "selection.previous") {
                    return m_active && m_previous;
                }
                if (action == "selection.next") {
                    return m_active && m_next;
                }
                return m_active;
            }

          private:
            bool m_active;
            bool m_previous;
            bool m_next;
        };

        class DeletedCharacterListHost final : public UiViewHost {
          public:
            void set_rows(const std::vector<std::string>* rows, int selected) {
                m_rows = rows;
                m_selected = selected;
            }

            [[nodiscard]] std::vector<UiListRow>
            list_rows(std::string_view bind, const UiWidget&, const UiVarMap&) const override {
                std::vector<UiListRow> result;
                if (bind != "character_delete.rows" || !m_rows) {
                    return result;
                }
                result.reserve(m_rows->size());
                for (std::size_t i = 0; i < m_rows->size(); ++i) {
                    UiListRow row;
                    row.set_text((static_cast<int>(i) == m_selected ? "> " : "  ") + (*m_rows)[i]);
                    row.set_action("character_delete.select");
                    row.set_id(std::to_string(i));
                    result.push_back(std::move(row));
                }
                return result;
            }

            [[nodiscard]] bool button_enabled(std::string_view action) const override {
                return action != "character_delete.restore" ||
                       (m_rows && m_selected >= 0 && m_selected < static_cast<int>(m_rows->size()));
            }

          private:
            const std::vector<std::string>* m_rows = nullptr;
            int m_selected = -1;
        };

        /** Screen-left is slot 0 (view X flip); same column pick as chargen. */
        [[nodiscard]] int lineup_slot_at(float mouse_x, float width, int count) {
            if (count < 1 || width <= 0.0f) {
                return -1;
            }
            const int slot = static_cast<int>(mouse_x / (width / static_cast<float>(count)));
            if (slot < 0 || slot >= count) {
                return -1;
            }
            return slot;
        }

        /** Charsel_Info runtime state order for the selector buttons. */
        [[nodiscard]] int charsel_selector_position(int character_index, int character_count) {
            static constexpr int kSelectorToCharacter[kMaxCharactersPerAccount] = {
                6, 4, 2, 0, 1, 3, 5, 7};
            if (character_index < 0 || character_index >= character_count) {
                return -1;
            }
            for (int selector = 0; selector < kMaxCharactersPerAccount; ++selector) {
                if (kSelectorToCharacter[selector] == character_index) {
                    return selector;
                }
            }
            return -1;
        }

        /** Adjacent occupied selector slot, wrapping across either end. */
        [[nodiscard]] int
        charsel_adjacent_index(int character_index, int character_count, int direction) {
            static constexpr int kSelectorToCharacter[kMaxCharactersPerAccount] = {
                6, 4, 2, 0, 1, 3, 5, 7};
            const int selector = charsel_selector_position(character_index, character_count);
            if (selector < 0 || (direction != -1 && direction != 1)) {
                return -1;
            }
            for (int offset = 1; offset < kMaxCharactersPerAccount; ++offset) {
                const int adjacent = (selector + direction * offset + kMaxCharactersPerAccount) %
                                     kMaxCharactersPerAccount;
                const int result = kSelectorToCharacter[adjacent];
                if (result >= 0 && result < character_count) {
                    return result;
                }
            }
            return character_index;
        }

        /** Builds G42 armor + G40 weapon wear for one chargen preview class. */
        [[nodiscard]] Wear chargen_preview_wear(int job) {
            Wear wear;
            switch (job) {
            case 0:  // Knight — GangChulGap + White HwanDoo Big Sword
                wear.set_weapon_index(101);
                wear.set_cloth_index(ClothPart::Upper, 117);
                wear.set_cloth_index(ClothPart::Helmet, 118);
                wear.set_cloth_index(ClothPart::Gauntlet, 119);
                wear.set_cloth_index(ClothPart::Boots, 120);
                wear.set_cloth_index(ClothPart::Pants, 121);
                break;
            case 1:  // Magician — MyungJoo DaeChangeau + Sae Dan stick
                wear.set_weapon_index(112);
                wear.set_cloth_index(ClothPart::Upper, 127);
                wear.set_cloth_index(ClothPart::Helmet, 128);
                wear.set_cloth_index(ClothPart::Gauntlet, 129);
                wear.set_cloth_index(ClothPart::Boots, 130);
                wear.set_cloth_index(ClothPart::Pants, 131);
                break;
            case 2:  // Archer — DooSukLinGap + ChulTae Bow
                wear.set_weapon_index(108);
                wear.set_cloth_index(ClothPart::Upper, 122);
                wear.set_cloth_index(ClothPart::Helmet, 123);
                wear.set_cloth_index(ClothPart::Gauntlet, 124);
                wear.set_cloth_index(ClothPart::Boots, 125);
                wear.set_cloth_index(ClothPart::Pants, 126);
                break;
            case 3:  // Thief — MyungJoo high-class suit + HwanDoo SaeHyung dagger
                wear.set_weapon_index(1415);
                wear.set_cloth_index(ClothPart::Upper, 1570);
                wear.set_cloth_index(ClothPart::Helmet, 1571);
                wear.set_cloth_index(ClothPart::Gauntlet, 1572);
                wear.set_cloth_index(ClothPart::Boots, 1573);
                wear.set_cloth_index(ClothPart::Pants, 1574);
                break;
            default:
                break;
            }
            return wear;
        }
    }  // namespace

    Class BootShell::clazz_from_job(std::uint8_t job) {
        switch (job) {
        case 1:
            return Class::Magician;
        case 2:
            return Class::Archer;
        case 3:
            return Class::Thief;
        default:
            return Class::Knight;
        }
    }

    bool BootShell::charsel_slot_pos(int index, int count, float& x, float& y, float& z) {
        // Retain the slot bearings, but put every character on the same ring.
        constexpr float kSelectionRadius = 60.0f;
        static constexpr float kSlots[kMaxCharactersPerAccount][3] = {
            {182.140f, 3030.270f, 191.303f},
            {150.000f, 3022.270f, 203.000f},
            {199.241f, 3022.270f, 161.683f},
            {111.433f, 3022.270f, 198.963f},
            {201.965f, 3022.270f, 123.000f},
            {170.000f, 3022.270f, 116.000f},
            {132.000f, 3022.270f, 132.000f},
            {92.000f, 3022.270f, 165.000f},
        };
        if (count < 1 || count > kMaxCharactersPerAccount || index < 0 || index >= count) {
            return false;
        }
        const float dx = kSlots[index][0] - LoginMapStage::kCenterX;
        const float dz = kSlots[index][2] - LoginMapStage::kCenterZ;
        const float radius_scale = kSelectionRadius / std::hypot(dx, dz);
        x = LoginMapStage::kCenterX + dx * radius_scale;
        y = kSlots[index][1];
        z = LoginMapStage::kCenterZ + dz * radius_scale;
        return true;
    }

    bool BootShell::chargen_job_pos(int job, float& x, float& y, float& z) {
        if (job < 0 || job >= 4) {
            return false;
        }
        // +X is screen-left. Keep the four previews symmetric around the room center.
        x = LoginMapStage::kCenterX + (1.5f - static_cast<float>(job)) * 24.0f;
        y = LoginMapStage::kCenterY;
        z = LoginMapStage::kCenterZ + 60.0f;
        return true;
    }

    bool BootShell::load_playerinfo_from_store(server::ICharacterRoster& store,
                                               std::int32_t account_id,
                                               protocol::PlayerInfoMsg& out) {
        protocol::PlayerInfoMsg msg;
        msg.auth_type = 0;
        msg.exp_time = 0;
        for (const auto& rec : store.list_characters(account_id)) {
            msg.characters.push_back(char_from_record(rec, store.load_items(rec.id)));
        }
        const auto frame = protocol::PlayerInfo::frame(msg);
        if (frame.empty()) {
            return false;
        }
        return protocol::PlayerInfo::parse(frame, out);
    }

    ShellResult BootShell::run(engine::IEngine& engine,
                               UiAssets& ui,
                               server::ICharacterRoster& store,
                               std::int32_t account_id,
                               std::int32_t preferred_character_id,
                               const std::string& data_root,
                               const std::string& configuration_root,
                               const std::string& starters_dir,
                               const LoginMapStagePaths& stage_paths,
                               const FreetypeFont& ui_font,
                               const UiSettings& ui_settings,
                               bool multiplayer) {
        ShellResult result;
        result.multiplayer = multiplayer;
        auto* local_store = dynamic_cast<server::IDataStore*>(&store);
        auto* remote_lobby = dynamic_cast<server::LobbyClient*>(&store);
        ShellPhase phase =
            kOfferMultiplayerModeSelect ? ShellPhase::ModeSelect : ShellPhase::CharSelect;
        int selected_idx = 0;
        CharacterCreationDraft creation;
        bool name_focused = true;
        UiLayout creation_layout;
        UiDocument creation_document;
        if (!creation_layout.load_file(
                platform::join_path(configuration_root, "ui/character_create.json"))) {
            log::error("BootShell: character creation layout could not be loaded");
            result.phase = ShellPhase::Quit;
            return result;
        }
        UiView::reload(creation_document, &creation_layout);
        UiLayout character_delete_layout;
        UiDocument character_delete_document;
        if (!character_delete_layout.load_file(
                platform::join_path(configuration_root, "ui/character_delete.json"))) {
            log::error("BootShell: character deletion layout could not be loaded");
            result.phase = ShellPhase::Quit;
            return result;
        }
        UiView::reload(character_delete_document, &character_delete_layout);
        UiLayout selection_layout;
        UiDocument selection_document;
        if (!selection_layout.load_file(
                platform::join_path(configuration_root, "ui/character_select.json"))) {
            log::error("BootShell: character selection layout could not be loaded");
            result.phase = ShellPhase::Quit;
            return result;
        }
        UiView::reload(selection_document, &selection_layout);
        UiLayout navigation_layout;
        UiDocument navigation_document;
        if (!navigation_layout.load_file(
                platform::join_path(configuration_root, "ui/character_navigation.json"))) {
            log::error("BootShell: character navigation layout could not be loaded");
            result.phase = ShellPhase::Quit;
            return result;
        }
        UiView::reload(navigation_document, &navigation_layout);
        std::array<std::vector<int>, 4> face_choices;
        std::array<std::vector<int>, 4> hair_choices;
        for (int job = 0; job < 4; ++job) {
            const auto clazz = clazz_from_job(static_cast<std::uint8_t>(job));
            for (int index = 1; index <= 255; ++index) {
                if (!EquipmentPaths::clothes_gb_path(data_root, clazz, ClothPart::Face, 0, index)
                         .empty()) {
                    face_choices[job].push_back(index);
                }
                if (!EquipmentPaths::clothes_gb_path(data_root, clazz, ClothPart::Helmet, 0, index)
                         .empty()) {
                    hair_choices[job].push_back(index);
                }
            }
        }
        std::string status_msg;
        std::vector<protocol::PlayerInfoChar> chars;
        CharacterDeleteModal delete_modal = CharacterDeleteModal::Closed;
        std::int32_t pending_delete_id = 0;
        std::string pending_delete_name;
        std::vector<server::CharacterRecord> deleted_characters;
        std::vector<std::string> deleted_character_rows;
        int deleted_selected = -1;
        std::string deleted_status;
        DeletedCharacterListHost deleted_list_host;
        server::StarterKit starters;
        if (!starters.load_dir(starters_dir)) {
            log::warn("BootShell: starter kits missing at " + starters_dir);
        }

        ItemDb item_catalog;
        const std::string items_dir = platform::join_path(configuration_root, "items");
        const bool have_item_catalog = item_catalog.load(configuration_root, data_root, items_dir);
        if (!have_item_catalog) {
            log::warn("BootShell: item catalog missing; charsel/chargen use default look");
        }

        std::unordered_map<std::string, engine::TextureHandle> tex_cache;
        std::vector<AnimatedActor> actors;          // charsel slots
        std::vector<AnimatedActor> chargen_actors;  // 4 class previews (incl. Thief)
        bool chargen_loaded = false;
        constexpr int kClassCount = 4;
        float stage_anim = 0.0f;

        LoginMapStage stage;
        SpriteFx stage_fx;
        if (!stage_fx.init(engine, data_root)) {
            log::warn("BootShell: login-map SpriteFx initialization failed");
        }
        engine::Mat4 last_view{};
        engine::Mat4 last_proj{};
        engine::Vec3 last_eye{};
        engine::Vec3 last_target{};
        engine::Vec3 charsel_camera_start_eye{};
        engine::Vec3 charsel_camera_start_target{};
        bool charsel_camera_ready = false;
        bool charsel_camera_animating = false;
        float charsel_camera_anim_elapsed = 0.0f;
        float charsel_camera_blend = 1.0f;
        int charsel_previous_selected_idx = -1;
        SoftCursor soft_cursor;
        if (!soft_cursor.load(engine, configuration_root)) {
            log::warn("boot shell soft cursors missing under config/ui/cursors");
        }

        auto destroy_actors = [&](std::vector<AnimatedActor>& list) {
            for (auto& a : list) {
                a.destroy(engine);
            }
            list.clear();
        };

        auto cleanup_gpu = [&] {
            destroy_actors(actors);
            destroy_actors(chargen_actors);
            chargen_loaded = false;
            soft_cursor.destroy(engine);
            stage_fx.destroy(engine);
            stage.destroy(engine);
            for (auto& [k, t] : tex_cache) {
                (void) k;
                if (t) {
                    engine.destroy_texture(t);
                }
            }
            tex_cache.clear();
        };

        const int load_bg_index = std::rand() % 4;
        const std::string load_bg_file = "Loadbg" + std::to_string(load_bg_index) + ".gtx";
        LoadingScreen shell_loading(engine, ui, ui_font, load_bg_file);
        if (!shell_loading.present("Loading character selection...")) {
            cleanup_gpu();
            result.phase = ShellPhase::Quit;
            return result;
        }
        if (!stage.load(engine, stage_paths)) {
            log::warn(
                "BootShell: login_map unavailable; shell actors use configured room coordinates");
        }

        auto map_position = [&](float x, float y, float z, float& wx, float& wy, float& wz) {
            wx = x;
            wz = z;
            wy = stage.sample_floor(wx, wz, y);
        };

        auto draw_shell_backdrop = [&](float screen_w, float screen_h, float dt) {
            if (stage.loaded()) {
                stage.draw(engine, last_eye, last_target, last_view, last_proj, stage_anim);
                stage.draw_effects(engine, stage_fx, dt, last_eye);
            }
            // The login room carries its authored illumination in GB lightmaps.  Keep only
            // a light grade over it so the dragon-head glow remains visible.
            engine.draw_ui_rect(0.0f, 0.0f, screen_w, screen_h, 0.0f, 0.0f, 0.0f, 0.08f);
        };

        auto reload_chars = [&] {
            destroy_actors(actors);
            protocol::PlayerInfoMsg info;
            if (!load_playerinfo_from_store(store, account_id, info)) {
                log::warn("charsel: PLAYERINFO bridge failed; empty list");
                chars.clear();
                preferred_character_id = 0;
                return;
            }
            chars = std::move(info.characters);
            if (chars.empty()) {
                selected_idx = 0;
            } else {
                bool selected_preferred = false;
                if (preferred_character_id > 0) {
                    const auto preferred = std::find_if(
                        chars.begin(), chars.end(), [&](const protocol::PlayerInfoChar& character) {
                            return character.pid == preferred_character_id;
                        });
                    if (preferred != chars.end()) {
                        selected_idx = static_cast<int>(preferred - chars.begin());
                        selected_preferred = true;
                    }
                }
                if (!selected_preferred) {
                    const auto records = store.list_characters(account_id);
                    const auto least_advanced =
                        std::min_element(records.begin(),
                                         records.end(),
                                         [](const server::CharacterRecord& left,
                                            const server::CharacterRecord& right) {
                                             return left.level != right.level
                                                        ? left.level < right.level
                                                        : left.exp < right.exp;
                                         });
                    if (least_advanced != records.end()) {
                        const auto selected =
                            std::find_if(chars.begin(),
                                         chars.end(),
                                         [&](const protocol::PlayerInfoChar& character) {
                                             return character.pid == least_advanced->id;
                                         });
                        if (selected != chars.end()) {
                            selected_idx = static_cast<int>(selected - chars.begin());
                        }
                    }
                }
            }
            preferred_character_id = 0;
            actors.resize(chars.size());
            for (int i = 0; i < static_cast<int>(chars.size()); ++i) {
                const auto& slot = chars[static_cast<std::size_t>(i)];
                const auto clazz = BootShell::clazz_from_job(slot.job);
                Wear preview;
                const Wear* wear_ptr = nullptr;
                const ItemDb* catalog_ptr = nullptr;
                if (have_item_catalog) {
                    preview = Wear::from_item_indices(item_catalog, slot.wear);
                    wear_ptr = &preview;
                    catalog_ptr = &item_catalog;
                }
                actors[static_cast<std::size_t>(i)].set_appearance(slot.face, slot.hair);
                if (!actors[static_cast<std::size_t>(i)].load(
                        engine, data_root, clazz, tex_cache, wear_ptr, catalog_ptr)) {
                    log::warn("charsel: failed to load model for " + slot.name);
                }
            }
        };

        auto reload_deleted_characters = [&] {
            const std::int64_t now_seconds = unix_seconds_now();
            deleted_characters = store.list_deleted_characters(account_id, now_seconds);
            deleted_character_rows.clear();
            deleted_character_rows.reserve(deleted_characters.size());
            for (const auto& character : deleted_characters) {
                deleted_character_rows.push_back(
                    "Lv " + std::to_string(character.level) + "  /  " + character.name + "  /  " +
                    LocalPlayer::class_name(
                        BootShell::clazz_from_job(static_cast<std::uint8_t>(character.clazz))) +
                    "  /  " +
                    std::to_string(remaining_deletion_days(character.delete_after, now_seconds)) +
                    " days");
            }
            if (deleted_selected >= static_cast<int>(deleted_characters.size())) {
                deleted_selected = deleted_characters.empty() ? -1 : 0;
            }
            deleted_list_host.set_rows(&deleted_character_rows, deleted_selected);
        };

        auto ensure_chargen = [&] {
            if (chargen_loaded) {
                return;
            }
            destroy_actors(chargen_actors);
            chargen_actors.resize(kClassCount);
            for (int i = 0; i < kClassCount; ++i) {
                const auto clazz = BootShell::clazz_from_job(static_cast<std::uint8_t>(i));
                Wear preview = chargen_preview_wear(i);
                preview.set_cloth_index(ClothPart::Helmet, 0);
                chargen_actors[static_cast<std::size_t>(i)].set_appearance(1, 1);
                const Wear* wear_ptr = have_item_catalog ? &preview : nullptr;
                const ItemDb* catalog_ptr = have_item_catalog ? &item_catalog : nullptr;
                if (!chargen_actors[static_cast<std::size_t>(i)].load(
                        engine, data_root, clazz, tex_cache, wear_ptr, catalog_ptr)) {
                    log::warn(std::string("chargen: failed to load ") +
                              LocalPlayer::class_name(clazz));
                }
            }
            chargen_loaded = true;
        };

        auto reload_creation_preview = [&] {
            const int job = creation.selected_class();
            if (job < 0) {
                return;
            }
            auto& actor = chargen_actors[static_cast<std::size_t>(job)];
            actor.set_appearance(creation.face(), creation.hair());
            Wear wear = chargen_preview_wear(job);
            wear.set_cloth_index(ClothPart::Helmet, 0);
            if (!actor.load(engine,
                            data_root,
                            clazz_from_job(static_cast<std::uint8_t>(job)),
                            tex_cache,
                            have_item_catalog ? &wear : nullptr,
                            have_item_catalog ? &item_catalog : nullptr)) {
                status_msg = "Appearance preview could not be loaded.";
            }
        };

        auto draw_cursor = [&](const engine::FrameInput& in) {
            soft_cursor.set(CursorKind::Default);
            soft_cursor.draw(ui, in.mouse_x, in.mouse_y, in.dt, in.key_ctrl);
        };

        auto draw_shell_label = [&](float x,
                                    float y,
                                    std::string_view text,
                                    float r = 0.95f,
                                    float g = 0.91f,
                                    float b = 0.72f,
                                    float scale = 1.0f) {
            if (ui_font.ready()) {
                ui_font.draw(engine, x, y, text, r, g, b, scale);
            }
        };

        auto draw_shell_label_centered = [&](float x,
                                             float y,
                                             std::string_view text,
                                             float r = 0.95f,
                                             float g = 0.91f,
                                             float b = 0.72f,
                                             float scale = 1.0f) {
            if (ui_font.ready()) {
                ui_font.draw_centered(engine, x, y, text, r, g, b, scale);
            }
        };

        auto draw_shell_header = [&](float width,
                                     float scale,
                                     std::string_view title,
                                     std::string_view subtitle) {
            ui.draw_tex("dragon_dialog/panel_background.bmp", 0, 0, width, 90.0f * scale);
            ui.draw_tex("dragon_dialog/frame_bottom.bmp", 0, 78.0f * scale, width, 16.0f * scale);
            draw_shell_label(
                28.0f * scale, 12.0f * scale, title, 0.898f, 0.812f, 0.620f, 1.5f * scale);
            draw_shell_label(
                28.0f * scale, 48.0f * scale, subtitle, 0.741f, 0.682f, 0.573f, 0.85f * scale);
        };
        auto draw_shell_button = [&](float x,
                                     float y,
                                     float w,
                                     float h,
                                     std::string_view label,
                                     float scale,
                                     bool hovered,
                                     bool pressed,
                                     bool enabled = true) {
            const char* file = !enabled  ? "dragon_dialog/button_disabled.bmp"
                               : pressed ? "dragon_dialog/button_pressed.bmp"
                               : hovered ? "dragon_dialog/button_hover.bmp"
                                         : "dragon_dialog/button_idle.bmp";
            ui.draw_tex(file, x, y, w, h);
            const float text_scale = 14.0f / std::max(1, ui_font.pixel_size()) * scale;
            draw_shell_label_centered(x + w * 0.5f,
                                      y + (h - ui_font.line_height() * text_scale) * 0.5f,
                                      label,
                                      enabled ? 0.918f : 0.55f,
                                      enabled ? 0.890f : 0.55f,
                                      enabled ? 0.831f : 0.55f,
                                      text_scale);
        };

        /** KCamera character-room rig. */
        auto set_login_camera = [&](float target_x,
                                    float target_y,
                                    float target_z,
                                    bool close_view,
                                    bool selected_presentation,
                                    float aspect,
                                    float transition_dt) {
            constexpr float kOriginalCenterY = 2990.0f;
            const float dx = target_x - LoginMapStage::kCenterX;
            const float dy = target_y - kOriginalCenterY;
            const float dz = target_z - LoginMapStage::kCenterZ;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float inv_length = length > 0.001f ? 1.0f / length : 0.0f;
            // The select view presents the active character with the camera,
            // never by offsetting its feet below the sampled floor.
            const float distance = close_view ? 33.0f : (selected_presentation ? 42.0f : 55.0f);
            const float eye_lift = close_view ? 15.0f : (selected_presentation ? 14.0f : 12.0f);
            const engine::Vec3 desired_eye{target_x + dx * inv_length * distance,
                                           target_y + dy * inv_length * distance + eye_lift,
                                           target_z + dz * inv_length * distance};
            // KCamera derives View-All from yaw/pitch. It points level at the
            // room centre instead of looking down at the centre's fixed Y.
            engine::Vec3 desired_target{
                LoginMapStage::kCenterX, desired_eye.y, LoginMapStage::kCenterZ};
            if (close_view) {
                const float horizontal = std::sqrt((desired_eye.x - LoginMapStage::kCenterX) *
                                                       (desired_eye.x - LoginMapStage::kCenterX) +
                                                   (desired_eye.z - LoginMapStage::kCenterZ) *
                                                       (desired_eye.z - LoginMapStage::kCenterZ));
                desired_target.y -= horizontal * std::tan(10.0f * 3.14159265f / 180.0f);
            }

            engine::Vec3 eye = desired_eye;
            engine::Vec3 camera_target = desired_target;
            if (selected_presentation) {
                if (!charsel_camera_ready) {
                    charsel_camera_ready = true;
                } else if (charsel_camera_animating) {
                    constexpr float kCameraTransitionSec = 0.55f;
                    charsel_camera_anim_elapsed += std::max(0.0f, transition_dt);
                    const float linear =
                        std::clamp(charsel_camera_anim_elapsed / kCameraTransitionSec, 0.0f, 1.0f);
                    const float blend = linear * linear * (3.0f - 2.0f * linear);
                    charsel_camera_blend = blend;
                    eye = {charsel_camera_start_eye.x +
                               (desired_eye.x - charsel_camera_start_eye.x) * blend,
                           charsel_camera_start_eye.y +
                               (desired_eye.y - charsel_camera_start_eye.y) * blend,
                           charsel_camera_start_eye.z +
                               (desired_eye.z - charsel_camera_start_eye.z) * blend};
                    camera_target = {charsel_camera_start_target.x +
                                         (desired_target.x - charsel_camera_start_target.x) * blend,
                                     charsel_camera_start_target.y +
                                         (desired_target.y - charsel_camera_start_target.y) * blend,
                                     charsel_camera_start_target.z +
                                         (desired_target.z - charsel_camera_start_target.z) *
                                             blend};
                    if (linear >= 1.0f) {
                        charsel_camera_animating = false;
                        charsel_previous_selected_idx = -1;
                    }
                }
            }
            last_view =
                engine::Mat4::multiply(engine::Mat4::scaling(-1, 1, 1),
                                       engine::Mat4::look_at(eye, camera_target, {0, 1, 0}));
            last_proj =
                engine::Mat4::perspective(50.0f * 3.14159265f / 180.0f, aspect, 1.0f, 2000.0f);
            last_eye = eye;
            last_target = camera_target;
            engine.set_camera(last_view, last_proj);
        };

        auto draw_actor_at =
            [&](AnimatedActor& actor, float px, float py, float pz, float scale, float dt) {
                actor.update(engine, dt, false, false);
                const float yaw =
                    std::atan2(LoginMapStage::kCenterX - px, LoginMapStage::kCenterZ - pz);
                const float hy = yaw * 0.5f;
                const auto R = engine::Mat4::from_quaternion(0, std::sin(hy), 0, std::cos(hy));
                const auto S = engine::Mat4::scaling(scale, scale, scale);
                const auto T = engine::Mat4::translation(px, py, pz);
                const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                if (!actor.empty()) {
                    actor.draw(engine, model, last_view, false);
                    actor.draw(engine, model, last_view, true);
                }
            };

        auto draw_chargen_actor =
            [&](AnimatedActor& actor, float px, float py, float pz, float scale, float dt) {
                actor.update(engine, dt, false, false);
                constexpr float yaw = 3.14159265f;
                const float hy = yaw * 0.5f;
                const auto R = engine::Mat4::from_quaternion(0, std::sin(hy), 0, std::cos(hy));
                const auto S = engine::Mat4::scaling(scale, scale, scale);
                const auto T = engine::Mat4::translation(px, py, pz);
                const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                if (!actor.empty()) {
                    actor.draw(engine, model, last_view, false);
                    actor.draw(engine, model, last_view, true);
                }
            };

        if (!kOfferMultiplayerModeSelect) {
            reload_chars();
            reload_deleted_characters();
            if (chars.empty() && deleted_characters.empty()) {
                creation = CharacterCreationDraft{};
                name_focused = true;
                phase = ShellPhase::CharCreate;
            }
        }

        engine.set_cursor_mode(engine::CursorMode::Free);
        engine::FrameInput in;
        bool previous_left = false;
        bool previous_right = false;
        std::int64_t next_deletion_maintenance = 0;
        while (engine.poll(in)) {
            if (remote_lobby && !remote_lobby->connected()) {
                cleanup_gpu();
                result.phase = ShellPhase::Quit;
                return result;
            }
            const bool left_pressed = in.key_left && !previous_left;
            const bool right_pressed = in.key_right && !previous_right;
            previous_left = in.key_left;
            previous_right = in.key_right;
            if (in.quit) {
                cleanup_gpu();
                result.phase = ShellPhase::Quit;
                return result;
            }

            const std::int64_t now_seconds = unix_seconds_now();
            if (local_store && now_seconds >= next_deletion_maintenance) {
                constexpr std::size_t kCharacterCleanupRowsPerSecond = 64;
                const server::CharacterPurgeResult purge = local_store->process_character_deletions(
                    now_seconds, kCharacterCleanupRowsPerSecond);
                if (purge == server::CharacterPurgeResult::Error) {
                    log::warn("BootShell: deferred character cleanup step failed");
                }
                if (delete_modal == CharacterDeleteModal::DeletedList) {
                    reload_deleted_characters();
                }
                next_deletion_maintenance = now_seconds + 1;
            }

            const float W = static_cast<float>(in.width);
            const float H = static_cast<float>(in.height);
            const float cx = W * 0.5f;
            const float aspect = H > 0 ? W / H : 1.777f;

            if (kOfferMultiplayerModeSelect && phase == ShellPhase::ModeSelect) {
                engine.begin_frame(0.05f, 0.06f, 0.09f);
                ui.draw_tex("dragon_dialog/panel_background.bmp", 0, 0, W, H);
                draw_shell_header(W, 1.0f, "OpenKal", "Choose play mode");
                const float bw = 280, bh = 48;
                const float bx = cx - bw * 0.5f;
                const float by1 = H * 0.42f;
                const float by2 = by1 + bh + 24;
                const bool single_hover =
                    UiAssets::point_in_rect(in.mouse_x, in.mouse_y, bx, by1, bw, bh);
                draw_shell_button(bx,
                                  by1,
                                  bw,
                                  bh,
                                  "Single Player",
                                  1.0f,
                                  single_hover,
                                  single_hover && in.mouse_left);
                draw_shell_button(bx, by2, bw, bh, "Multiplayer", 1.0f, false, false, false);
                draw_shell_label_centered(
                    cx, by2 + bh + 24, "Multiplayer coming later", 0.741f, 0.682f, 0.573f);

                if (in.mouse_left_clicked) {
                    if (UiAssets::point_in_rect(in.mouse_x, in.mouse_y, bx, by1, bw, bh)) {
                        phase = ShellPhase::CharSelect;
                        status_msg.clear();
                        reload_chars();
                        reload_deleted_characters();
                        if (chars.empty() && deleted_characters.empty()) {
                            creation = CharacterCreationDraft{};
                            name_focused = true;
                            phase = ShellPhase::CharCreate;
                        }
                    } else if (UiAssets::point_in_rect(in.mouse_x, in.mouse_y, bx, by2, bw, bh)) {
                        status_msg = "Multiplayer coming later";
                    }
                }
                if (!status_msg.empty()) {
                    draw_shell_label_centered(cx, H - 60, status_msg, 1, 0.5f, 0.4f);
                }
                draw_cursor(in);
                engine.end_frame();
                continue;
            }

            // ----- Class picker, then a single-character creation preview -----
            if (phase == ShellPhase::CharCreate) {
                stage_anim += in.dt;
                ensure_chargen();
                const bool editing = creation.selected_class() >= 0;
                const float scale =
                    std::min({ui_settings.resolve_scale(H), W / 1024.0f, H / 900.0f});
                const float panel_x = 56.0f * scale;
                const float panel_y =
                    std::max(180.0f * scale, (H - creation_document.window_h() * scale) * 0.5f);
                const float panel_w = creation_document.window_w() * scale;
                const float panel_h = creation_document.window_h() * scale;
                constexpr float tan_half_fov = 0.46630766f;
                const float distance =
                    editing ? 60.0f : std::max(68.0f, 100.0f / (2.0f * tan_half_fov * aspect));
                const float stage_z = LoginMapStage::kCenterZ + 60.0f;
                const float floor_y =
                    stage.sample_floor(LoginMapStage::kCenterX, stage_z, LoginMapStage::kCenterY);
                const float camera_x = LoginMapStage::kCenterX +
                                       (editing ? distance * tan_half_fov * aspect * 0.45f : 0.0f);
                last_eye = {camera_x, floor_y + 22.0f, stage_z + distance};
                last_target = {camera_x, floor_y + 22.0f, stage_z};
                last_view =
                    engine::Mat4::multiply(engine::Mat4::scaling(-1, 1, 1),
                                           engine::Mat4::look_at(last_eye, last_target, {0, 1, 0}));
                last_proj =
                    engine::Mat4::perspective(50.0f * 3.14159265f / 180.0f, aspect, 1.0f, 2000.0f);
                engine.begin_frame(0.025f, 0.075f, 0.08f);
                engine.set_camera(last_view, last_proj);
                draw_shell_backdrop(W, H, in.dt);
                for (int job = 0; job < kClassCount; ++job) {
                    if (editing && job != creation.selected_class()) {
                        continue;
                    }
                    float px = 0, py = 0, pz = 0;
                    (void) chargen_job_pos(job, px, py, pz);
                    if (editing) {
                        px = LoginMapStage::kCenterX;
                    }
                    py = stage.sample_floor(px, pz, py);
                    draw_chargen_actor(chargen_actors[static_cast<std::size_t>(job)],
                                       px,
                                       py,
                                       pz,
                                       editing ? 1.8f : 1.65f,
                                       in.dt);
                }

                draw_shell_header(W,
                                  scale,
                                  "Create Character",
                                  editing ? "Choose your name, appearance and starting attributes"
                                          : "Choose your class");
                bool back = in.key_escape_pressed;
                bool submit = editing && in.key_enter_pressed;
                if (!editing) {
                    const float button_w = 165.0f * scale;
                    const float button_h = 36.0f * scale;
                    const float button_y = H - 100.0f * scale;
                    for (int job = 0; job < kClassCount; ++job) {
                        float px = 0, py = 0, pz = 0;
                        (void) chargen_job_pos(job, px, py, pz);
                        const float screen_x =
                            cx - (px - camera_x) * H / (2.0f * tan_half_fov * distance);
                        const float pick_w = 24.0f * H / (2.0f * tan_half_fov * distance);
                        const bool hovered =
                            UiAssets::point_in_rect(in.mouse_x,
                                                    in.mouse_y,
                                                    screen_x - pick_w * 0.5f,
                                                    H * 0.16f,
                                                    pick_w,
                                                    button_y + button_h - H * 0.16f);
                        draw_shell_button(
                            screen_x - button_w * 0.5f,
                            button_y,
                            button_w,
                            button_h,
                            LocalPlayer::class_name(clazz_from_job(static_cast<std::uint8_t>(job))),
                            scale,
                            hovered,
                            hovered && in.mouse_left);
                        if (in.mouse_left_clicked && hovered) {
                            if (!chargen_actors[static_cast<std::size_t>(job)].empty() &&
                                creation.select_class(job, face_choices[job], hair_choices[job])) {
                                status_msg.clear();
                                name_focused = true;
                                reload_creation_preview();
                            } else {
                                status_msg = "Class preview assets are missing.";
                            }
                        }
                    }
                    const float back_y = H - 50.0f * scale;
                    const bool hovered = UiAssets::point_in_rect(in.mouse_x,
                                                                 in.mouse_y,
                                                                 W - 179.0f * scale,
                                                                 back_y,
                                                                 165.0f * scale,
                                                                 36.0f * scale);
                    draw_shell_button(W - 179.0f * scale,
                                      back_y,
                                      165.0f * scale,
                                      36.0f * scale,
                                      "Back",
                                      scale,
                                      hovered,
                                      hovered && in.mouse_left);
                    back = back || (hovered && in.mouse_left_clicked);
                    if (!status_msg.empty()) {
                        draw_shell_label_centered(
                            cx, 108.0f * scale, status_msg, 1, 0.7f, 0.4f, scale);
                    }
                } else {
                    if (name_focused) {
                        const std::string previous_name = creation.name();
                        creation.append_name(std::string_view(
                            in.text_chars, static_cast<std::size_t>(in.text_char_count)));
                        if (in.key_backspace_pressed) {
                            creation.erase_name();
                        }
                        if (creation.name() != previous_name) {
                            status_msg.clear();
                        }
                    }
                    auto variables = [&] {
                        UiVarMap vars{
                            {"creation.class",
                             LocalPlayer::class_name(clazz_from_job(
                                 static_cast<std::uint8_t>(creation.selected_class())))},
                            {"creation.name_label", "Name"},
                            {"creation.name", creation.name() + (name_focused ? "_" : "")},
                            {"creation.appearance_label", "Appearance"},
                            {"creation.face_label", "Face"},
                            {"creation.hair_label", "Hair"},
                            {"creation.face", std::to_string(creation.face())},
                            {"creation.hair", std::to_string(creation.hair())},
                            {"creation.stats_label", "Starting attributes"},
                            {"creation.points",
                             std::to_string(creation.remaining_points()) + " points left"},
                            {"creation.previous", "<"},
                            {"creation.next", ">"},
                            {"creation.minus", "-"},
                            {"creation.plus", "+"},
                            {"creation.back", "Back"},
                            {"creation.create", "Create"},
                            {"creation.status",
                             status_msg.empty() ? creation.validation_error() : status_msg}};
                        constexpr const char* keys[] = {"str", "hth", "intl", "wis", "dex"};
                        constexpr const char* labels[] = {
                            "Strength", "Health", "Intelligence", "Wisdom", "Dexterity"};
                        for (int i = 0; i < 5; ++i) {
                            vars[std::string("creation.") + keys[i]] =
                                std::to_string(creation.stat(i));
                            vars[std::string("creation.") + keys[i] + "_label"] = labels[i];
                        }
                        ui_fill_theme_vars(creation_document, vars);
                        return vars;
                    };
                    auto vars = variables();
                    const UiHit hit = UiView::hit(creation_document,
                                                  &ui,
                                                  in.mouse_x,
                                                  in.mouse_y,
                                                  panel_x,
                                                  panel_y,
                                                  panel_w,
                                                  panel_h,
                                                  scale,
                                                  vars);
                    if (in.mouse_left_clicked) {
                        name_focused = hit.kind() == UiHitKind::Input;
                        const std::string& action = hit.action();
                        if (action == "creation.back") {
                            back = true;
                        } else if (action == "creation.submit") {
                            submit = true;
                        } else if (action == "creation.face_previous" ||
                                   action == "creation.face_next" ||
                                   action == "creation.hair_previous" ||
                                   action == "creation.hair_next") {
                            const bool face = action.find("face") != std::string::npos;
                            const int direction =
                                action.find("previous") != std::string::npos ? -1 : 1;
                            if (creation.cycle_appearance(face, direction)) {
                                reload_creation_preview();
                            }
                        } else if (action.rfind("creation.stat_plus_", 0) == 0 ||
                                   action.rfind("creation.stat_minus_", 0) == 0) {
                            const int index = action.back() - '0';
                            (void) creation.adjust_stat(
                                index, action.find("plus") != std::string::npos ? 1 : -1);
                            status_msg.clear();
                        }
                    }
                    vars = variables();
                    UiViewHost creation_host;
                    creation_host.set_pointer_hit(hit.widget_id(), in.mouse_left);
                    UiView::draw(engine,
                                 ui,
                                 creation_document,
                                 panel_x,
                                 panel_y,
                                 panel_w,
                                 panel_h,
                                 scale,
                                 vars,
                                 ui_font,
                                 ui_font,
                                 &creation_host);
                }

                if (back) {
                    status_msg.clear();
                    if (editing) {
                        creation.back();
                        destroy_actors(chargen_actors);
                        chargen_loaded = false;
                    } else {
                        phase = ShellPhase::CharSelect;
                        charsel_camera_ready = false;
                        charsel_camera_animating = false;
                    }
                } else if (submit) {
                    status_msg = creation.validation_error();
                    if (status_msg.empty() && chars.size() >= kMaxCharactersPerAccount) {
                        status_msg = "Character limit reached.";
                    }
                    if (status_msg.empty()) {
                        const auto existing = store.list_characters(account_id);
                        const auto deleted =
                            store.list_deleted_characters(account_id, unix_seconds_now());
                        const auto name_matches = [&](const auto& record) {
                            return record.name == creation.name();
                        };
                        if (std::any_of(existing.begin(), existing.end(), name_matches) ||
                            std::any_of(deleted.begin(), deleted.end(), name_matches)) {
                            status_msg = "This name is already in use.";
                        }
                    }
                    if (status_msg.empty()) {
                        server::CharacterRecord c;
                        c.account_id = account_id;
                        c.name = creation.name();
                        c.clazz = creation.selected_class();
                        c.face = creation.face();
                        c.hair = creation.hair();
                        c.str = creation.stat(0);
                        c.hth = creation.stat(1);
                        c.intl = creation.stat(2);
                        c.wis = creation.stat(3);
                        c.dex = creation.stat(4);
                        c.max_hp = c.hp = server::combat::max_hp_from_stats(c.hth, 1, c.clazz);
                        c.max_mp = c.mp = server::combat::max_mp_from_stats(c.wis, 1, c.clazz);
                        starters.apply_spawn(c);
                        if (store.save_character(c)) {
                            const auto saved = store.list_characters(account_id);
                            std::int32_t created_id = 0;
                            for (auto record : saved) {
                                if (record.name != c.name) {
                                    continue;
                                }
                                created_id = record.id;
                                std::unordered_map<int, server::ItemCombatDef> defs;
                                if (local_store) {
                                    (void) starters.grant_if_empty(*local_store, record, defs);
                                }
                                break;
                            }
                            reload_chars();
                            for (int i = 0; i < static_cast<int>(chars.size()); ++i) {
                                if (chars[i].pid == created_id) {
                                    selected_idx = i;
                                }
                            }
                            charsel_camera_ready = false;
                            charsel_camera_animating = false;
                            phase = ShellPhase::CharSelect;
                            status_msg = "Created " + c.name;
                        } else {
                            status_msg = "Could not save the character.";
                        }
                    }
                }
                draw_cursor(in);
                engine.end_frame();
                continue;
            }

            // ----- 3D character select (KalOnline charsel stage) -----
            stage_anim += in.dt;

            // Keep the header, stats panel and bottom controls clear at smaller sizes.
            const float panel_scale =
                std::min({ui_settings.resolve_scale(H), W / 1024.0f, H / 640.0f});
            const float stats_w = selection_document.window_w() * panel_scale;
            const float stats_h = selection_document.window_h() * panel_scale;
            const float stats_x = 56.0f * panel_scale;
            const float stats_y = H - stats_h - 72.0f * panel_scale;
            const float navigation_w = navigation_document.window_w() * panel_scale;
            const float navigation_h = navigation_document.window_h() * panel_scale;
            const float command_h = 36.0f * panel_scale;
            const float command_gap = 8.0f * panel_scale;
            const float available_actions_w = (W - navigation_w) * 0.5f - 40.0f * panel_scale;
            const float command_w =
                std::min(112.0f * panel_scale, (available_actions_w - command_gap * 3.0f) / 4.0f);
            const float actions_w = command_w * 4.0f + command_gap * 3.0f;
            const float actions_h = command_h;
            const float actions_x = W - actions_w - 24.0f * panel_scale;
            const float actions_y = H - actions_h - 24.0f * panel_scale;
            const float navigation_x = (W - navigation_w) * 0.5f;
            const float navigation_y = actions_y + command_h - navigation_h;
            UiVarMap selection_vars;
            ui_fill_theme_vars(selection_document, selection_vars);
            const int previous_idx =
                charsel_adjacent_index(selected_idx, static_cast<int>(chars.size()), -1);
            const int next_idx =
                charsel_adjacent_index(selected_idx, static_cast<int>(chars.size()), 1);
            CharacterSelectionHost selection_host(!chars.empty() && !charsel_camera_animating &&
                                                      delete_modal == CharacterDeleteModal::Closed,
                                                  previous_idx >= 0,
                                                  next_idx >= 0);
            const UiHit stats_hit = UiView::hit(selection_document,
                                                &ui,
                                                in.mouse_x,
                                                in.mouse_y,
                                                stats_x,
                                                stats_y,
                                                stats_w,
                                                stats_h,
                                                panel_scale,
                                                selection_vars,
                                                &selection_host);
            const UiHit selection_hit = UiView::hit(navigation_document,
                                                    &ui,
                                                    in.mouse_x,
                                                    in.mouse_y,
                                                    navigation_x,
                                                    navigation_y,
                                                    navigation_w,
                                                    navigation_h,
                                                    panel_scale,
                                                    selection_vars,
                                                    &selection_host);
            selection_host.set_pointer_hit(selection_hit.widget_id(), in.mouse_left);
            auto select_character = [&](int next_selection) {
                if (next_selection < 0 || next_selection == selected_idx) {
                    return;
                }
                const int previous_selection = selected_idx;
                selected_idx = next_selection;
                if (charsel_camera_ready) {
                    charsel_previous_selected_idx = previous_selection;
                    charsel_camera_start_eye = last_eye;
                    charsel_camera_start_target = last_target;
                    charsel_camera_anim_elapsed = 0.0f;
                    charsel_camera_blend = 0.0f;
                    charsel_camera_animating = true;
                }
            };
            if (left_pressed != right_pressed) {
                if (left_pressed && selection_host.button_enabled("selection.previous")) {
                    select_character(previous_idx);
                } else if (right_pressed && selection_host.button_enabled("selection.next")) {
                    select_character(next_idx);
                }
            }
            if (delete_modal == CharacterDeleteModal::Closed && in.mouse_left_clicked &&
                !chars.empty() && !charsel_camera_animating) {
                if (selection_hit.action() == "selection.previous") {
                    select_character(previous_idx);
                } else if (selection_hit.action() == "selection.next") {
                    select_character(next_idx);
                } else if (in.mouse_y > H * 0.12f && in.mouse_y < H * 0.78f) {
                    const bool on_stats = stats_hit.kind() != UiHitKind::None;
                    const bool on_navigation = selection_hit.kind() != UiHitKind::None;
                    const bool on_actions = UiAssets::point_in_rect(
                        in.mouse_x, in.mouse_y, actions_x, actions_y, actions_w, actions_h);
                    if (!on_stats && !on_navigation && !on_actions) {
                        const int slot =
                            lineup_slot_at(in.mouse_x, W, static_cast<int>(chars.size()));
                        if (slot >= 0) {
                            select_character(slot);
                        }
                    }
                }
            }

            const int n = static_cast<int>(chars.size());
            float focus_x = LoginMapStage::kCenterX;
            float focus_y = LoginMapStage::kCenterY;
            float focus_z = LoginMapStage::kCenterZ;
            if (n > 0) {
                (void) charsel_slot_pos(selected_idx, n, focus_x, focus_y, focus_z);
                map_position(focus_x, focus_y, focus_z, focus_x, focus_y, focus_z);
            }
            engine.begin_frame(0.025f, 0.075f, 0.08f);
            set_login_camera(focus_x, focus_y, focus_z, false, n > 0, aspect, in.dt);
            draw_shell_backdrop(W, H, in.dt);

            for (int i = 0; i < n; ++i) {
                float lx = 0, ly = 0, lz = 0;
                if (!charsel_slot_pos(i, n, lx, ly, lz)) {
                    log::warn("charsel_slot_pos failed for slot " + std::to_string(i));
                }
                float px = lx, py = ly, pz = lz;
                map_position(lx, ly, lz, px, py, pz);
                float selected_weight = i == selected_idx ? 1.0f : 0.0f;
                if (charsel_camera_animating) {
                    if (i == selected_idx) {
                        selected_weight = charsel_camera_blend;
                    } else if (i == charsel_previous_selected_idx) {
                        selected_weight = 1.0f - charsel_camera_blend;
                    }
                }
                const float scale = kChargenScale + (1.28f - kChargenScale) * selected_weight;
                draw_actor_at(actors[static_cast<std::size_t>(i)], px, py, pz, scale, in.dt);
            }

            draw_shell_header(W, panel_scale, "Select Character", "Choose your adventurer");
            selection_vars["selection.title"] = "Character";
            selection_vars["selection.name"] = "No character";
            selection_vars["selection.class"] = "Create a character to begin";
            selection_vars["selection.level"] = "";
            selection_vars["selection.count"] = "0 / 0";
            selection_vars["selection.attributes"] = "Attributes";
            selection_vars["selection.previous"] = "<";
            selection_vars["selection.next"] = ">";
            selection_vars["selection.enter"] = "Enter World";
            constexpr const char* stat_keys[] = {"str", "hth", "intl", "wis", "dex"};
            constexpr const char* stat_labels[] = {
                "Strength", "Health", "Intelligence", "Wisdom", "Dexterity"};
            for (int i = 0; i < 5; ++i) {
                selection_vars[std::string("selection.") + stat_keys[i] + "_label"] =
                    stat_labels[i];
                selection_vars[std::string("selection.") + stat_keys[i]] = "-";
            }
            if (!chars.empty()) {
                const auto& c = chars[static_cast<std::size_t>(selected_idx)];
                selection_vars["selection.name"] = c.name;
                selection_vars["selection.class"] = LocalPlayer::class_name(clazz_from_job(c.job));
                selection_vars["selection.level"] = "Level " + std::to_string(c.level);
                selection_vars["selection.count"] =
                    std::to_string(selected_idx + 1) + " / " + std::to_string(chars.size());
                const int stats[] = {c.str, c.hth, c.intel, c.wis, c.dex};
                for (int i = 0; i < 5; ++i) {
                    selection_vars[std::string("selection.") + stat_keys[i]] =
                        std::to_string(stats[i]);
                }
            }
            UiView::draw(engine,
                         ui,
                         selection_document,
                         stats_x,
                         stats_y,
                         stats_w,
                         stats_h,
                         panel_scale,
                         selection_vars,
                         ui_font,
                         ui_font,
                         &selection_host);

            UiView::draw(engine,
                         ui,
                         navigation_document,
                         navigation_x,
                         navigation_y,
                         navigation_w,
                         navigation_h,
                         panel_scale,
                         selection_vars,
                         ui_font,
                         ui_font,
                         &selection_host);

            auto command_button = [&](int index, std::string_view label) {
                const float x = actions_x + static_cast<float>(index) * (command_w + command_gap);
                const bool hover = UiAssets::point_in_rect(
                    in.mouse_x, in.mouse_y, x, actions_y, command_w, command_h);
                const bool enabled =
                    delete_modal == CharacterDeleteModal::Closed &&
                    (index != 1 || !chars.empty()) &&
                    (index != 0 || chars.size() < server::kMaxCharactersPerAccount);
                draw_shell_button(x,
                                  actions_y,
                                  command_w,
                                  command_h,
                                  label,
                                  panel_scale,
                                  enabled && hover,
                                  enabled && hover && in.mouse_left,
                                  enabled);
                return enabled && in.mouse_left_clicked && hover;
            };
            const bool select_clicked = delete_modal == CharacterDeleteModal::Closed &&
                                        !charsel_camera_animating && in.mouse_left_clicked &&
                                        selection_hit.action() == "selection.select" &&
                                        selection_host.button_enabled("selection.select");
            const bool create_clicked = command_button(0, "Create");
            const bool delete_clicked = command_button(1, "Delete");
            const bool deleted_clicked = command_button(2, "Deleted");
            const bool exit_clicked = command_button(3, "Back");

            if (delete_modal == CharacterDeleteModal::Closed && !charsel_camera_animating &&
                (select_clicked || in.key_enter_pressed)) {
                if (!chars.empty()) {
                    const auto& slot = chars[static_cast<std::size_t>(selected_idx)];
                    auto loaded = store.load_character(slot.pid);
                    if (loaded) {
                        result.selected = *loaded;
                    } else {
                        result.selected.account_id = account_id;
                        result.selected.id = slot.pid;
                        result.selected.name = slot.name;
                        result.selected.clazz = slot.job;
                        result.selected.level = slot.level;
                        result.selected.hp = 100;
                        result.selected.max_hp = 100;
                    }
                    if (have_item_catalog) {
                        const Wear wear = Wear::from_item_indices(item_catalog, slot.wear);
                        if (wear.weapon_index() > 0) {
                            result.selected.weapon_index = wear.weapon_index();
                        }
                        const std::int32_t upper = wear.cloth_index(ClothPart::Upper);
                        if (upper > 0) {
                            result.selected.armor_index = upper;
                        }
                    }
                    result.phase = ShellPhase::EnterWorld;
                    cleanup_gpu();
                    return result;
                }
                status_msg = "Create a character first";
            }
            if (create_clicked) {
                if (static_cast<int>(chars.size()) >= kMaxCharactersPerAccount) {
                    status_msg = "Character limit (8)";
                } else {
                    creation = CharacterCreationDraft{};
                    name_focused = true;
                    phase = ShellPhase::CharCreate;
                    status_msg.clear();
                }
            }
            if (delete_clicked) {
                if (!chars.empty()) {
                    reload_deleted_characters();
                    if (deleted_characters.size() >= server::kMaxDeletedCharactersPerAccount) {
                        status_msg = "Deleted character list is full.";
                    } else {
                        const auto& selected = chars[static_cast<std::size_t>(selected_idx)];
                        pending_delete_id = selected.pid;
                        pending_delete_name = selected.name;
                        delete_modal = CharacterDeleteModal::Confirm;
                        deleted_status.clear();
                    }
                }
            }
            if (deleted_clicked) {
                delete_modal = CharacterDeleteModal::DeletedList;
                deleted_selected = -1;
                deleted_status.clear();
                reload_deleted_characters();
            }
            if (delete_modal == CharacterDeleteModal::Closed && kOfferMultiplayerModeSelect &&
                (exit_clicked || in.key_escape_pressed)) {
                destroy_actors(actors);
                destroy_actors(chargen_actors);
                chargen_loaded = false;
                phase = ShellPhase::ModeSelect;
                status_msg.clear();
            } else if (delete_modal == CharacterDeleteModal::Closed &&
                       !kOfferMultiplayerModeSelect && (exit_clicked || in.key_escape_pressed)) {
                cleanup_gpu();
                result.phase = ShellPhase::Startup;
                return result;
            }
            if (!status_msg.empty()) {
                draw_shell_label_centered((stats_x + stats_w + W) * 0.5f,
                                          navigation_y - 30.0f * panel_scale,
                                          status_msg,
                                          1.0f,
                                          0.7f,
                                          0.4f,
                                          panel_scale);
            }

            if (delete_modal != CharacterDeleteModal::Closed) {
                if (in.key_escape_pressed) {
                    delete_modal = CharacterDeleteModal::Closed;
                    pending_delete_id = 0;
                    pending_delete_name.clear();
                } else {
                    const float modal_scale =
                        std::min({ui_settings.resolve_scale(H), W / 640.0f, H / 530.0f});
                    const float modal_w = character_delete_document.window_w() * modal_scale;
                    const float modal_h = character_delete_document.window_h() * modal_scale;
                    const float modal_x = (W - modal_w) * 0.5f;
                    const float modal_y = (H - modal_h + 28.0f * modal_scale) * 0.5f;
                    auto delete_variables = [&] {
                        UiVarMap vars;
                        ui_fill_theme_vars(character_delete_document, vars);
                        const bool confirming = delete_modal == CharacterDeleteModal::Confirm;
                        const bool listing = delete_modal == CharacterDeleteModal::DeletedList;
                        vars["character_delete.title"] =
                            confirming ? "Delete Character" : "Deleted Characters";
                        vars["character_delete.question"] = "Delete " + pending_delete_name + "?";
                        vars["character_delete.recovery_note"] =
                            deleted_status.empty() ? "The character can be restored for 30 days."
                                                   : deleted_status;
                        vars["character_delete.yes"] = "Delete";
                        vars["character_delete.no"] = "Cancel";
                        vars["character_delete.columns"] = "Level / Name / Class / Days left";
                        vars["character_delete.empty"] = "No recoverable characters.";
                        vars["character_delete.status"] = deleted_status;
                        vars["character_delete.restore"] = "Restore";
                        vars["character_delete.close"] = "Close";
                        vars["character_delete.confirming"] = confirming ? "true" : "false";
                        vars["character_delete.listing"] = listing ? "true" : "false";
                        vars["character_delete.empty_visible"] =
                            listing && deleted_characters.empty() ? "true" : "false";
                        return vars;
                    };
                    deleted_list_host.set_rows(&deleted_character_rows, deleted_selected);
                    UiVarMap modal_vars = delete_variables();
                    const UiHit hit = UiView::hit(character_delete_document,
                                                  &ui,
                                                  in.mouse_x,
                                                  in.mouse_y,
                                                  modal_x,
                                                  modal_y,
                                                  modal_w,
                                                  modal_h,
                                                  modal_scale,
                                                  modal_vars,
                                                  &deleted_list_host,
                                                  true);
                    if (in.mouse_left_clicked) {
                        const std::string& action = hit.action();
                        if (action == "character_delete.confirm") {
                            if (store.schedule_character_deletion(
                                    pending_delete_id, account_id, unix_seconds_now())) {
                                const std::string deleted_name = pending_delete_name;
                                delete_modal = CharacterDeleteModal::Closed;
                                pending_delete_id = 0;
                                pending_delete_name.clear();
                                reload_chars();
                                reload_deleted_characters();
                                status_msg = deleted_name + " can be restored for 30 days.";
                            } else {
                                deleted_status = "Could not schedule character deletion.";
                            }
                        } else if (action == "character_delete.cancel") {
                            delete_modal = CharacterDeleteModal::Closed;
                            pending_delete_id = 0;
                            pending_delete_name.clear();
                        } else if (action == "character_delete.select" && hit.index() >= 0 &&
                                   hit.index() < static_cast<int>(deleted_characters.size())) {
                            deleted_selected = hit.index();
                            deleted_status.clear();
                            deleted_list_host.set_rows(&deleted_character_rows, deleted_selected);
                        } else if (action == "character_delete.restore" && deleted_selected >= 0 &&
                                   deleted_selected < static_cast<int>(deleted_characters.size())) {
                            if (chars.size() >= server::kMaxCharactersPerAccount) {
                                deleted_status = "Character limit reached.";
                            } else {
                                const auto character =
                                    deleted_characters[static_cast<std::size_t>(deleted_selected)];
                                if (store.restore_character(
                                        character.id, account_id, unix_seconds_now())) {
                                    reload_chars();
                                    deleted_selected = -1;
                                    reload_deleted_characters();
                                    deleted_status = character.name + " restored.";
                                } else {
                                    reload_deleted_characters();
                                    deleted_status = "Character can no longer be restored.";
                                }
                            }
                        } else if (action == "character_delete.close") {
                            delete_modal = CharacterDeleteModal::Closed;
                        }
                        modal_vars = delete_variables();
                    }
                    deleted_list_host.set_pointer_hit(hit.widget_id(), in.mouse_left);
                    if (delete_modal != CharacterDeleteModal::Closed) {
                        engine.draw_ui_rect(0.0f, 0.0f, W, H, 0.0f, 0.0f, 0.0f, 0.58f);
                        UiView::draw(engine,
                                     ui,
                                     character_delete_document,
                                     modal_x,
                                     modal_y,
                                     modal_w,
                                     modal_h,
                                     modal_scale,
                                     modal_vars,
                                     ui_font,
                                     ui_font,
                                     &deleted_list_host,
                                     true);
                    }
                }
            }

            draw_cursor(in);
            engine.end_frame();
        }

        cleanup_gpu();
        result.phase = ShellPhase::Quit;
        return result;
    }

}  // namespace openkal::client
