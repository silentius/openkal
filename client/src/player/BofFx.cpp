/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BofFx.h"

#include "openkal/client/AnimatedActor.h"
#include "openkal/client/Equipment.h"
#include "openkal/client/SpriteFx.h"

#include <cctype>
#include <cstdio>

namespace openkal::client {
    namespace {

        constexpr float kBofLoopDelaySeconds = 10.0f;

        [[nodiscard]] std::string ascii_lowered(std::string_view text) {
            std::string out(text);
            for (char& c : out) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return out;
        }

    }  // namespace

    bool BofFx::parse_part(std::string_view token, Part& out) {
        const std::string t = ascii_lowered(token);
        if (t == "helmet" || t == "helm" || t == "hat" || t == "head") {
            out = Part::Helmet;
            return true;
        }
        if (t == "chest" || t == "upper" || t == "armor" || t == "coat") {
            out = Part::Chest;
            return true;
        }
        if (t == "gloves" || t == "glove" || t == "gauntlet" || t == "gauntlets") {
            out = Part::Gloves;
            return true;
        }
        if (t == "pants" || t == "lower" || t == "legs") {
            out = Part::Pants;
            return true;
        }
        if (t == "boots" || t == "boot" || t == "shoes") {
            out = Part::Boots;
            return true;
        }
        return false;
    }

    std::string BofFx::fx_name(int full_set, int level, Class clazz) {
        char buf[32];
        std::snprintf(
            buf, sizeof(buf), "ef_setitem_%02d%02d%02d", full_set, level, static_cast<int>(clazz));
        return std::string(buf);
    }

    bool BofFx::is_supported_level(int level) {
        return level == 46 || level == 50 || level == 55 || level == 60 || level == 65;
    }

    bool BofFx::set_level(int level) {
        if (!is_supported_level(level)) {
            return false;
        }
        m_level = level;
        return true;
    }

    bool BofFx::toggle_part(Part part) {
        const auto i = static_cast<std::size_t>(part);
        if (i >= m_parts.size()) {
            return false;
        }
        m_parts[i] = !m_parts[i];
        return m_parts[i];
    }

    void BofFx::set_all(bool enabled) {
        m_parts.fill(enabled);
    }

    void BofFx::clear() {
        m_parts.fill(false);
    }

    int BofFx::enabled_count() const {
        int n = 0;
        for (bool on : m_parts) {
            if (on) {
                ++n;
            }
        }
        return n;
    }

    int BofFx::set_completeness() const {
        if (!m_debug_override && m_wear_completeness >= 0) {
            return m_wear_completeness;
        }
        const int n = enabled_count();
        if (n >= kFullCount) {
            return 2;
        }
        if (n >= kHalfCount) {
            return 1;
        }
        return 0;
    }

    const char* BofFx::part_name(Part part) {
        switch (part) {
        case Part::Helmet:
            return "helmet";
        case Part::Chest:
            return "chest";
        case Part::Gloves:
            return "gloves";
        case Part::Pants:
            return "pants";
        case Part::Boots:
            return "boots";
        default:
            return "?";
        }
    }

    std::string BofFx::status_line() const {
        std::string line = "BOF L" + std::to_string(m_level) + " [";
        bool first = true;
        for (int i = 0; i < static_cast<int>(Part::Count); ++i) {
            if (!m_parts[static_cast<std::size_t>(i)]) {
                continue;
            }
            if (!first) {
                line += ' ';
            }
            first = false;
            line += part_name(static_cast<Part>(i));
        }
        if (first) {
            line += "none";
        }
        line += "] ";
        switch (set_completeness()) {
        case 2:
            line += "full";
            break;
        case 1:
            line += "half";
            break;
        default:
            line += "no set FX";
            break;
        }
        if (!m_attached.empty()) {
            line += " -> ";
            line += m_attached;
        }
        return line;
    }

    void BofFx::detach(SpriteFx& sprite_fx) {
        sprite_fx.detach(kEntity);
        m_attached.clear();
    }

    void BofFx::sync(engine::IEngine& engine, SpriteFx& sprite_fx, Class clazz) {
        detach(sprite_fx);
        const int completeness = set_completeness();
        if (completeness <= 0 || !sprite_fx.ready()) {
            return;
        }
        const std::string name = fx_name(completeness, m_level, clazz);
        if (!sprite_fx.has_fx(name)) {
            return;
        }
        sprite_fx.attach_fx(engine, kEntity, name, -1, kBofLoopDelaySeconds);
        if (sprite_fx.has(kEntity)) {
            m_attached = name;
        }
    }

    void BofFx::apply_wear(
        int completeness, int fx_level, engine::IEngine& engine, SpriteFx& sprite_fx, Class clazz) {
        m_debug_override = false;
        const int level = fx_level > 0 ? fx_level : m_level;
        const std::string want =
            completeness > 0 ? fx_name(completeness, level, clazz) : std::string{};
        if (m_wear_completeness == completeness && m_level == level && m_attached == want) {
            return;
        }
        m_wear_completeness = completeness;
        m_level = level;
        sync(engine, sprite_fx, clazz);
    }

    void BofFx::set_debug_override(bool enabled) {
        m_debug_override = enabled;
        if (enabled) {
            m_wear_completeness = -1;
        }
    }

    void BofFx::follow(SpriteFx& sprite_fx,
                       const AnimatedActor& actor,
                       const engine::Mat4& model,
                       Class clazz,
                       float feet_x,
                       float feet_y,
                       float feet_z) const {
        if (m_attached.empty() || !sprite_fx.has(kEntity)) {
            return;
        }
        sprite_fx.follow_bone(kEntity, -1, feet_x, feet_y, feet_z);
        const int* slots = nullptr;
        int slot_count = 0;
        static constexpr int kKnightSlots[] = {1, 5, 6};
        static constexpr int kThiefSlots[] = {1, 6, 7};
        if (clazz == Class::Thief) {
            slots = kThiefSlots;
            slot_count = 3;
        } else {
            slots = kKnightSlots;
            slot_count = 3;
        }
        for (int i = 0; i < slot_count; ++i) {
            const int slot = slots[i];
            int skel = sprite_fx.real_effect_bone(kEntity, slot);
            if (skel < 0) {
                skel = EquipmentPaths::effect_slot_skeleton(clazz, slot);
            }
            float x = feet_x;
            float y = feet_y;
            float z = feet_z;
            if (skel >= 0) {
                (void) actor.bone_world(model, skel, x, y, z);
            }
            sprite_fx.follow_bone(kEntity, slot, x, y, z);
        }
    }

}  // namespace openkal::client
