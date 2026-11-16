/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/UiSettings.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace openkal::client {
    namespace {

        platform::Json jnum(double n) {
            platform::Json j;
            j.set_type(platform::Json::Type::Number);
            j.set_number(n);
            return j;
        }

        platform::Json jstr(std::string s) {
            platform::Json j;
            j.set_type(platform::Json::Type::String);
            j.string() = std::move(s);
            return j;
        }

        platform::Json jobj() {
            platform::Json j;
            j.set_type(platform::Json::Type::Object);
            return j;
        }

        platform::Json jarr() {
            platform::Json j;
            j.set_type(platform::Json::Type::Array);
            return j;
        }

        void load_chat_filters(const platform::Json& chat, ChatFilterModel& filters) {
            if (!chat.is_object()) {
                return;
            }
            if (const platform::Json* system = chat.get("system_msgs")) {
                filters.set_system_enabled(system->as_int(1) != 0);
            }
            const bool current_channels = chat.at("channel_version").as_int(0) == 2;
            if (const platform::Json* tab = chat.get("active_tab")) {
                const int t = tab->as_int(0);
                if (t >= 0 &&
                    t < (current_channels ? static_cast<int>(ChatFilterModel::kTabCount) : 4)) {
                    filters.set_active_tab(static_cast<ChatTab>(t));
                }
            }
            if (const platform::Json* ch = chat.get("channels"); ch && ch->is_array()) {
                auto flags = filters.channel_flags();
                const auto& arr = ch->array();
                if (current_channels) {
                    for (std::size_t i = 0; i < flags.size() && i < arr.size(); ++i) {
                        flags[i] = arr[i].as_int(0) != 0;
                    }
                } else {
                    // Migrate supported customized rows; old defaults become focused tab defaults.
                    constexpr std::array<std::size_t, 6> old_channels{0, 1, 2, 3, 4, 7};
                    for (std::size_t t = 0; t < 4; ++t) {
                        if (arr.size() < (t + 1) * 8) {
                            continue;
                        }
                        bool old_default = true;
                        const auto own = t == 3 ? 4 : t;
                        for (std::size_t c = 0; c < 8; ++c) {
                            const bool expected = t == 0 || c == own || c == 3 || c == 7;
                            old_default &= (arr[t * 8 + c].as_int(0) != 0) == expected;
                        }
                        if (old_default) {
                            continue;
                        }
                        for (std::size_t c = 0; c < old_channels.size(); ++c) {
                            flags[t * ChatFilterModel::kChannelCount + c] =
                                arr[t * 8 + old_channels[c]].as_int(0) != 0;
                        }
                    }
                }
                filters.set_channel_flags(flags);
            }
            if (const platform::Json* bt = chat.get("battle_msgs"); bt && bt->is_array()) {
                std::array<bool, ChatFilterModel::kBattleCount> flags{};
                flags.fill(true);
                const auto& arr = bt->array();
                for (std::size_t i = 0; i < flags.size() && i < arr.size(); ++i) {
                    flags[i] = arr[i].as_int(1) != 0;
                }
                filters.set_battle_flags(flags);
            }
        }

        [[nodiscard]] platform::Json save_chat_filters(const ChatFilterModel& filters) {
            platform::Json chat = jobj();
            chat.object()["channel_version"] = jnum(2);
            chat.object()["active_tab"] = jnum(static_cast<int>(filters.active_tab()));
            platform::Json channels = jarr();
            for (bool v : filters.channel_flags()) {
                channels.array().push_back(jnum(v ? 1 : 0));
            }
            chat.object()["channels"] = std::move(channels);
            platform::Json battle = jarr();
            for (bool v : filters.battle_flags()) {
                battle.array().push_back(jnum(v ? 1 : 0));
            }
            chat.object()["battle_msgs"] = std::move(battle);
            chat.object()["system_msgs"] = jnum(filters.system_enabled() ? 1 : 0);
            return chat;
        }

    }  // namespace

    const char* UiSettings::panel_key(HudPanelId id) {
        switch (id) {
        case HudPanelId::Status:
            return "status";
        case HudPanelId::Target:
            return "destinfo";
        case HudPanelId::Minimap:
            return "minimap";
        case HudPanelId::Dragon:
            return "dragon_hud";
        case HudPanelId::Chat:
            return "chattingctrl";
        case HudPanelId::MenuBar:
            return "mainbar";
        case HudPanelId::Clock:
            return "clock";
        default:
            return "unknown";
        }
    }

    bool UiSettings::panel_id_from_key(const std::string& key, HudPanelId& out) {
        if (key == "status") {
            out = HudPanelId::Status;
            return true;
        }
        if (key == "destinfo" || key == "target") {
            out = HudPanelId::Target;
            return true;
        }
        if (key == "minimap") {
            out = HudPanelId::Minimap;
            return true;
        }
        if (key == "dragon_hud") {
            out = HudPanelId::Dragon;
            return true;
        }
        if (key == "chattingctrl" || key == "chat") {
            out = HudPanelId::Chat;
            return true;
        }
        if (key == "clock") {
            out = HudPanelId::Clock;
            return true;
        }
        if (key == "mainbar" || key == "menu") {
            out = HudPanelId::MenuBar;
            return true;
        }
        return false;
    }

    bool UiSettings::load(const std::string& path) {
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            return false;
        }
        platform::Json root;
        std::string err;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
            !root.is_object()) {
            log::warn("UiSettings: bad json " + path + ": " + err);
            return false;
        }

        const platform::Json& scale = root.at("ui_scale");
        if (scale.is_string()) {
            const std::string s = scale.as_string();
            if (s == "auto" || s.empty()) {
                m_scale_auto = true;
            } else {
                try {
                    m_manual_scale = std::stof(s);
                    m_scale_auto = false;
                } catch (...) {
                    m_scale_auto = true;
                }
            }
        } else if (scale.is_number()) {
            m_scale_auto = false;
            m_manual_scale = scale.as_float(m_manual_scale);
        }

        if (const platform::Json* p = root.get("reference_height")) {
            m_reference_height = std::max(1.0f, p->as_float(m_reference_height));
        }
        if (const platform::Json* p = root.get("min_scale")) {
            m_min_scale = std::max(0.25f, p->as_float(m_min_scale));
        }
        if (const platform::Json* p = root.get("max_scale")) {
            m_max_scale = std::max(m_min_scale, p->as_float(m_max_scale));
        }

        m_characters.clear();
        const platform::Json& chars = root.at("characters");
        if (chars.is_object()) {
            for (const auto& [id_str, entry] : chars.object()) {
                if (!entry.is_object()) {
                    continue;
                }
                std::int32_t id = 0;
                try {
                    id = static_cast<std::int32_t>(std::stol(id_str));
                } catch (...) {
                    continue;
                }
                HudCharacterLayout layout;
                layout.set_name(entry.at("name").as_string());
                const auto& auto_hp = entry.at("auto_hp");
                AutoHpSettings medicine;
                const auto& index = auto_hp.at("item_index");
                if (index.is_number() && index.number() > 0 && index.number() < 32768 &&
                    std::floor(index.number()) == index.number()) {
                    medicine.item_index = static_cast<int>(index.number());
                }
                const auto& percent = auto_hp.at("percent");
                if (percent.is_number() && (percent.number() == 25 || percent.number() == 50 ||
                                            percent.number() == 75 || percent.number() == 100)) {
                    medicine.percent = static_cast<int>(percent.number());
                }
                layout.set_auto_hp(medicine);
                const platform::Json& panels = entry.at("panels");
                if (panels.is_object()) {
                    for (const auto& [pkey, pos] : panels.object()) {
                        if (pkey == "party") {
                            const auto& x = pos.at("x");
                            const auto& y = pos.at("y");
                            if (x.is_number() && y.is_number() && std::isfinite(x.as_float()) &&
                                std::isfinite(y.as_float())) {
                                layout.set_party_position(x.as_float(), y.as_float());
                            }
                            continue;
                        }
                        HudPanelId pid{};
                        if (!panel_id_from_key(pkey, pid) || !pos.is_object()) {
                            continue;
                        }
                        layout.set_panel_pos(pid, pos.at("x").as_float(0), pos.at("y").as_float(0));
                    }
                }
                if (const platform::Json* chat = entry.get("chat")) {
                    load_chat_filters(*chat, layout.chat_filters());
                }
                if (const auto* read = entry.get("read_mail_ids"); read && read->is_array()) {
                    std::set<std::uint32_t> ids;
                    for (const auto& value : read->array()) {
                        if (value.is_number() && value.number() > 0 &&
                            value.number() <= std::numeric_limits<std::uint32_t>::max() &&
                            std::floor(value.number()) == value.number()) {
                            ids.insert(static_cast<std::uint32_t>(value.number()));
                        }
                    }
                    layout.set_read_mail_ids(std::move(ids));
                }
                m_characters[id] = std::move(layout);
            }
        }

        log::info("UiSettings loaded " + path + " scale=" +
                  (m_scale_auto ? std::string("auto") : std::to_string(m_manual_scale)) +
                  " characters=" + std::to_string(m_characters.size()));
        return true;
    }

    bool UiSettings::save(const std::string& path) const {
        platform::Json root = jobj();
        root.object()["version"] = jnum(1);
        if (m_scale_auto) {
            root.object()["ui_scale"] = jstr("auto");
        } else {
            root.object()["ui_scale"] = jnum(m_manual_scale);
        }
        root.object()["reference_height"] = jnum(m_reference_height);
        root.object()["min_scale"] = jnum(m_min_scale);
        root.object()["max_scale"] = jnum(m_max_scale);

        platform::Json chars = jobj();
        for (const auto& [id, layout] : m_characters) {
            platform::Json entry = jobj();
            entry.object()["name"] = jstr(layout.name());
            platform::Json auto_hp = jobj();
            auto_hp.object()["item_index"] = jnum(layout.auto_hp().item_index);
            auto_hp.object()["percent"] = jnum(layout.auto_hp().percent);
            entry.object()["auto_hp"] = std::move(auto_hp);
            platform::Json panels = jobj();
            for (int i = 0; i < static_cast<int>(HudPanelId::Count); ++i) {
                const auto panel_id = static_cast<HudPanelId>(i);
                const auto& pp = layout.panel(panel_id);
                if (!pp.has_pos()) {
                    continue;
                }
                platform::Json pos = jobj();
                pos.object()["x"] = jnum(pp.x());
                pos.object()["y"] = jnum(pp.y());
                panels.object()[panel_key(panel_id)] = std::move(pos);
            }
            if (const auto& party = layout.party_position(); party.has_pos()) {
                platform::Json pos = jobj();
                pos.object()["x"] = jnum(party.x());
                pos.object()["y"] = jnum(party.y());
                panels.object()["party"] = std::move(pos);
            }
            entry.object()["panels"] = std::move(panels);
            entry.object()["chat"] = save_chat_filters(layout.chat_filters());
            platform::Json read = jarr();
            for (const auto mail_id : layout.read_mail_ids()) {
                read.array().push_back(jnum(mail_id));
            }
            entry.object()["read_mail_ids"] = std::move(read);
            chars.object()[std::to_string(id)] = std::move(entry);
        }
        root.object()["characters"] = std::move(chars);

        const std::string text = platform::json_stringify(root, 2);
        if (!platform::write_file(path, text)) {
            log::warn("UiSettings: failed to write " + path);
            return false;
        }
        return true;
    }

    const HudPanelPos& UiSettings::party_position(std::int32_t character_id) const {
        static const HudPanelPos empty;
        const auto it = m_characters.find(character_id);
        return it == m_characters.end() ? empty : it->second.party_position();
    }

    void UiSettings::set_party_position(std::int32_t character_id, float x, float y) {
        if (std::isfinite(x) && std::isfinite(y)) {
            m_characters[character_id].set_party_position(x, y);
        }
    }

    float UiSettings::resolve_scale(float framebuffer_height) const {
        float s = m_manual_scale;
        if (m_scale_auto) {
            const float ref = std::max(1.0f, m_reference_height);
            s = framebuffer_height / ref;
        }
        return std::clamp(s, m_min_scale, m_max_scale);
    }

    const std::set<std::uint32_t>& UiSettings::read_mail_ids(std::int32_t character_id) const {
        static const std::set<std::uint32_t> empty;
        const auto it = m_characters.find(character_id);
        return it == m_characters.end() ? empty : it->second.read_mail_ids();
    }

    void UiSettings::set_read_mail_ids(std::int32_t character_id, std::set<std::uint32_t> ids) {
        m_characters[character_id].set_read_mail_ids(std::move(ids));
    }

    const AutoHpSettings& UiSettings::auto_hp(std::int32_t character_id) const {
        static const AutoHpSettings empty;
        const auto it = m_characters.find(character_id);
        return it == m_characters.end() ? empty : it->second.auto_hp();
    }

    void UiSettings::set_auto_hp(std::int32_t character_id, AutoHpSettings settings) {
        m_characters[character_id].set_auto_hp(settings);
    }

    void UiSettings::apply_character(GameHud& hud, std::int32_t character_id, float scale) const {
        const auto it = m_characters.find(character_id);
        if (it == m_characters.end()) {
            return;
        }
        for (int i = 0; i < static_cast<int>(HudPanelId::Count); ++i) {
            const auto panel_id = static_cast<HudPanelId>(i);
            const auto& pp = it->second.panel(panel_id);
            if (!pp.has_pos()) {
                continue;
            }
            hud.set_panel_pos(panel_id, pp.x() * scale, pp.y() * scale);
        }
        hud.chat().filters() = it->second.chat_filters();
    }

    void UiSettings::capture_character(const GameHud& hud,
                                       std::int32_t character_id,
                                       const std::string& name,
                                       float scale) {
        const float inv = (scale > 0.0001f) ? (1.0f / scale) : 1.0f;
        HudCharacterLayout& layout = m_characters[character_id];
        layout.set_name(name);
        for (int i = 0; i < static_cast<int>(HudPanelId::Count); ++i) {
            const auto panel_id = static_cast<HudPanelId>(i);
            if (panel_id == HudPanelId::Dragon ||
                (panel_id == HudPanelId::Chat && hud.chat().battle_feed())) {
                continue;
            }
            const auto& panel = hud.panel(panel_id);
            layout.set_panel_pos(panel_id, panel.x() * inv, panel.y() * inv);
        }
        layout.chat_filters() = hud.chat().filters();
    }

}  // namespace openkal::client
