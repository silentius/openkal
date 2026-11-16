/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Equipment.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace openkal::client {
    namespace {

        /** True when the part maps onto a valid clothes slot. */
        bool is_valid_cloth_part(ClothPart part) {
            const int i = static_cast<int>(part);
            return i >= 0 && i < static_cast<int>(ClothPart::Count);
        }

    }  // namespace

    std::int32_t Wear::cloth_index(ClothPart part) const {
        if (!is_valid_cloth_part(part)) {
            return 0;
        }
        return m_cloth[static_cast<std::size_t>(part)];
    }

    void Wear::set_cloth_index(ClothPart part, std::int32_t v) {
        if (!is_valid_cloth_part(part)) {
            return;
        }
        m_cloth[static_cast<std::size_t>(part)] = v;
    }

    std::int32_t Wear::skin_index(protocol::SkinSlot slot) const {
        const int cell = static_cast<int>(slot);
        return cell >= 0 && cell < static_cast<int>(m_skins.size()) ? m_skins[cell] : 0;
    }

    bool Wear::is_equipped(std::int32_t index) const {
        if (index > 0 && std::find(m_skins.begin(), m_skins.end(), index) != m_skins.end()) {
            return true;
        }
        if (m_weapon == index || m_shield == index || m_transform == index || m_pet == index ||
            m_ride == index || m_ring == index || m_necklace == index || m_trinket == index) {
            return true;
        }
        for (const std::int32_t slot : m_cloth) {
            if (slot == index) {
                return true;
            }
        }
        return false;
    }

    void Wear::unequip_index(std::int32_t index) {
        if (index == 0) {
            return;
        }
        for (std::size_t i = 0; i < m_skins.size(); ++i) {
            if (m_skins[i] == index) {
                m_skins[i] = 0;
                if (i == static_cast<std::size_t>(protocol::SkinSlot::Upper)) {
                    m_full_body_skin = false;
                }
            }
        }
        if (m_weapon == index) {
            m_weapon = 0;
        }
        if (m_shield == index) {
            m_shield = 0;
        }
        if (m_transform == index) {
            m_transform = 0;
        }
        if (m_pet == index) {
            m_pet = 0;
        }
        if (m_ride == index) {
            m_ride = 0;
        }
        if (m_ring == index) {
            m_ring = 0;
        }
        if (m_necklace == index) {
            m_necklace = 0;
        }
        if (m_trinket == index) {
            m_trinket = 0;
        }
        for (std::int32_t& slot : m_cloth) {
            if (slot == index) {
                slot = 0;
            }
        }
    }

    void Wear::clear() {
        m_skins.fill(0);
        m_full_body_skin = false;
        m_weapon = 0;
        m_shield = 0;
        m_cloth.fill(0);
        m_transform = 0;
        m_pet = 0;
        m_ride = 0;
        m_ring = 0;
        m_necklace = 0;
        m_trinket = 0;
    }

    void Wear::apply_item(const ItemDef& def) {
        if (def.is_skin()) {
            for (std::size_t i = 0; i < m_skins.size(); ++i) {
                if (protocol::skin_slots_conflict(
                        def.skin_slot(),
                        def.full_body_skin(),
                        static_cast<protocol::SkinSlot>(i),
                        i == static_cast<std::size_t>(protocol::SkinSlot::Upper) &&
                            m_full_body_skin)) {
                    m_skins[i] = 0;
                }
            }
            if (protocol::is_body_skin_slot(def.skin_slot())) {
                m_full_body_skin = def.full_body_skin();
            }
            m_skins[static_cast<std::size_t>(def.skin_slot())] = def.index();
            return;
        }
        if (def.is_weapon()) {
            set_weapon_index(def.index());
            return;
        }
        if (def.is_defense()) {
            if (def.class_small() == "shield") {
                set_shield_index(def.index());
                return;
            }
            set_cloth_index(EquipmentPaths::cloth_part_from_small(def.class_small()), def.index());
            return;
        }
        if (def.is_transform()) {
            set_transform_index(def.index());
            return;
        }
        if (def.is_pet()) {
            set_pet_index(def.index());
            return;
        }
        if (def.is_ride()) {
            set_ride_index(def.index());
            return;
        }
        if (!def.is_ornament()) {
            return;
        }
        const std::string& small = def.class_small();
        if (small == "ring") {
            set_ring_index(def.index());
            return;
        }
        if (small == "necklace" || small == "resistnecklace") {
            set_necklace_index(def.index());
            return;
        }
        set_trinket_index(def.index());
    }

    Wear Wear::from_item_indices(const ItemDb& catalog, const std::vector<std::uint16_t>& indices) {
        Wear wear;
        bool mask = false;
        for (const std::uint16_t index : indices) {
            if (index == 0) {
                continue;
            }
            const ItemDef* def = catalog.find(static_cast<std::int32_t>(index));
            if (!def) {
                continue;
            }
            const bool helmet = def->is_defense() &&
                                (def->class_small() == "helmet" || def->class_small() == "hat");
            if (helmet && mask) {
                continue;
            }
            if (def->is_defense() &&
                (def->class_small() == "face" || def->class_small() == "mask")) {
                mask = true;
            }
            wear.apply_item(*def);
        }
        return wear;
    }

    char EquipmentPaths::cloth_part_letter(ClothPart p) {
        static constexpr char kLetters[] = {'s', 'g', 'p', 'a', 'f', 'h'};
        if (!is_valid_cloth_part(p)) {
            return 'a';
        }
        return kLetters[static_cast<std::size_t>(p)];
    }

    ClothPart EquipmentPaths::cloth_part_from_small(const std::string& small) {
        if (small == "boots") {
            return ClothPart::Boots;
        }
        if (small == "gauntlet" || small == "gloves") {
            return ClothPart::Gauntlet;
        }
        if (small == "lowerarmor" || small == "pants") {
            return ClothPart::Pants;
        }
        if (small == "upperarmor" || small == "armor") {
            return ClothPart::Upper;
        }
        if (small == "helmet" || small == "hat") {
            return ClothPart::Helmet;
        }
        if (small == "face" || small == "mask") {
            return ClothPart::Face;
        }
        return ClothPart::Upper;
    }

    char EquipmentPaths::species_letter(Class c) {
        switch (c) {
        case Class::Magician:
            return 'G';
        case Class::Archer:
            return 'B';
        case Class::Thief:
            return 'D';  // PD_* dagger meshes (GameObject::runtime state "SGBD")
        case Class::Knight:
        default:
            return 'S';
        }
    }

    int EquipmentPaths::weapon_bone_index(Class c) {
        switch (c) {
        case Class::Magician:
            return 44;
        case Class::Archer:
            return 20;
        case Class::Thief:
            return 44;  // characterbone key 3 (part 44 28)
        case Class::Knight:
        default:
            return 45;
        }
    }

    int EquipmentPaths::shield_bone_index(Class c) {
        switch (c) {
        case Class::Archer:
            return 34;  // characterbone key 2 (part 20 34) quiver
        case Class::Thief:
            return 28;  // characterbone key 3 (part 44 28)
        case Class::Knight:
            return 32;  // characterbone key 0 (part 45 32)
        case Class::Magician:
        default:
            return 0;
        }
    }

    float EquipmentPaths::shield_local_offset_y(Class c) {
        switch (c) {
        case Class::Knight:
            // Back plate of PS_B* sits at local Y ≈ 0..0.06; 0.28 clears gauntlet
            // / chest intersection without floating the grip off the socket.
            return 0.28f;
        default:
            return 0.0f;
        }
    }

    int EquipmentPaths::effect_bone_index(Class c) {
        switch (c) {
        case Class::Magician:
            return 75;
        case Class::Archer:
            return 64;
        case Class::Thief:
            return 2;  // characterbone key 3 bone[0]
        case Class::Knight:
        default:
            return 86;
        }
    }

    int EquipmentPaths::effect_slot_skeleton(Class c, int effect_slot) {
        if (effect_slot < 0 || effect_slot >= 8) {
            return -1;
        }
        // macro-e characterbone (bone …) — eight EFFECTBONE slots per class.
        static constexpr int kKnight[8] = {86, 15, 17, 25, 38, 5, 11, 45};
        static constexpr int kMage[8] = {75, 3, 5, 19, 33, 59, 63, 44};
        static constexpr int kArcher[8] = {64, 3, 5, 13, 27, 44, 48, 20};
        static constexpr int kThief[8] = {2, 3, 5, 44, 28, 14, 30, 56};
        const int* bones = kKnight;
        switch (c) {
        case Class::Magician:
            bones = kMage;
            break;
        case Class::Archer:
            bones = kArcher;
            break;
        case Class::Thief:
            bones = kThief;
            break;
        case Class::Knight:
        default:
            bones = kKnight;
            break;
        }
        return bones[effect_slot];
    }

    float EquipmentPaths::effect_target_height(Class c) {
        switch (c) {
        case Class::Magician:
            return 16.0f;
        case Class::Archer:
            return 17.0f;
        case Class::Thief:
            return 17.0f;
        case Class::Knight:
        default:
            return 18.0f;
        }
    }

    std::string EquipmentPaths::resolve_existing(const std::string& dir, const std::string& name) {
        const std::string exact = platform::join_path(dir, name);
        if (platform::file_exists(exact)) {
            return exact;
        }
        // Windows packs mix job/part casing (compatibility behavior.gb vs compatibility
        // behavior.gb).
        const std::string resolved = platform::Path::resolve_path_case(dir, name);
        if (resolved != exact && platform::file_exists(resolved)) {
            return resolved;
        }
        return {};
    }

    std::string
    EquipmentPaths::skin_gb_path(const std::string& data_root, Class c, const ItemDef& def) {
        if (!def.is_skin() || def.action_class() < 0 || def.action_index() < 1 ||
            (def.class_limit() >= 0 && def.class_limit() != static_cast<int>(c))) {
            return {};
        }
        const auto slot = def.skin_slot();
        char name[64];
        if (slot == protocol::SkinSlot::Weapon || slot == protocol::SkinSlot::Shield) {
            std::snprintf(name,
                          sizeof(name),
                          "P%c_%c%d_%02d.gb",
                          species_letter(c),
                          slot == protocol::SkinSlot::Shield ? 'B' : 'S',
                          def.action_class(),
                          def.action_index());
            return resolve_existing(platform::join_path(data_root, "Item/Weapon"), name);
        }
        static constexpr char parts[] = {'s', 'g', 'h', 'a', 'p', 's', 'g', 'f'};
        const char part = def.uses_costume_mesh() ? 'c' : parts[static_cast<int>(slot)];
        std::snprintf(name,
                      sizeof(name),
                      "C%c_%d_%c%02d.gb",
                      LocalPlayer::job_letter(c),
                      def.action_class(),
                      part,
                      def.action_index());
        return resolve_existing(platform::join_path(data_root, "Model/Clothes"), name);
    }

    std::string EquipmentPaths::clothes_gb_path(
        const std::string& data_root, Class c, ClothPart part, int action_class, int action_index) {
        const char job = LocalPlayer::job_letter(c);
        const char pl = cloth_part_letter(part);
        const std::string dir = platform::join_path(data_root, "Model/Clothes");
        char name[64];
        std::snprintf(name, sizeof(name), "C%c_%d_%c%02d.gb", job, action_class, pl, action_index);
        std::string p = resolve_existing(dir, name);
        if (!p.empty()) {
            return p;
        }
        const char ju = static_cast<char>(std::toupper(static_cast<unsigned char>(job)));
        std::snprintf(name, sizeof(name), "C%c_%d_%c%02d.gb", ju, action_class, pl, action_index);
        return resolve_existing(dir, name);
    }

    std::string EquipmentPaths::weapon_gb_path(const std::string& data_root,
                                               Class c,
                                               int action_class,
                                               int action_index) {
        const char sp = species_letter(c);
        const std::string dir = platform::join_path(data_root, "Item/Weapon");
        char name[64];
        // Separators: classic "S"/"s"; some thief cash meshes use "C"/"c"
        // (PD_C1_01.gb).
        const char species_try[] = {sp, 'D', 'I', 'B', 'S', 'G', 0};
        for (int si = 0; species_try[si]; ++si) {
            for (const char* sep : {"S", "s", "C", "c"}) {
                std::snprintf(name,
                              sizeof(name),
                              "P%c_%s%d_%02d.gb",
                              species_try[si],
                              sep,
                              action_class,
                              action_index);
                std::string p = resolve_existing(dir, name);
                if (!p.empty()) {
                    return p;
                }
                std::snprintf(
                    name,
                    sizeof(name),
                    "P%c_%s%d_%02d.gb",
                    static_cast<char>(std::tolower(static_cast<unsigned char>(species_try[si]))),
                    sep,
                    action_class,
                    action_index);
                p = resolve_existing(dir, name);
                if (!p.empty()) {
                    return p;
                }
            }
        }
        return {};
    }

    std::string EquipmentPaths::shield_gb_path(const std::string& data_root,
                                               Class c,
                                               int action_class,
                                               int action_index) {
        const char sp = species_letter(c);
        const std::string dir = platform::join_path(data_root, "Item/Weapon");
        char name[64];
        // Knight/archer shields use B; thief off-hand daggers use s (OnChangedWeapon).
        const char* seps[] = {c == Class::Thief ? "s" : "B",
                              c == Class::Thief ? "S" : "b",
                              c == Class::Thief ? "B" : "s",
                              c == Class::Thief ? "b" : "S",
                              nullptr};
        const char species_try[] = {sp, 'S', 'D', 0};
        auto try_name = [&](char species, const char* sep, int ac, int ai) -> std::string {
            std::snprintf(name, sizeof(name), "P%c_%s%d_%02d.gb", species, sep, ac, ai);
            std::string p = resolve_existing(dir, name);
            if (!p.empty()) {
                return p;
            }
            std::snprintf(name,
                          sizeof(name),
                          "P%c_%s%d_%02d.gb",
                          static_cast<char>(std::tolower(static_cast<unsigned char>(species))),
                          sep,
                          ac,
                          ai);
            return resolve_existing(dir, name);
        };
        const int indices[] = {action_index > 0 ? action_index : 1, 1};
        for (int ii = 0; ii < 2; ++ii) {
            if (ii == 1 && indices[0] <= 1) {
                break;
            }
            for (int si = 0; species_try[si]; ++si) {
                for (int sep_i = 0; seps[sep_i]; ++sep_i) {
                    std::string p =
                        try_name(species_try[si], seps[sep_i], action_class, indices[ii]);
                    if (!p.empty()) {
                        return p;
                    }
                }
            }
        }
        return {};
    }

    std::vector<std::string> EquipmentPaths::clothes_paths_for_wear(const std::string& data_root,
                                                                    Class c,
                                                                    const Wear& wear,
                                                                    const ItemDb& catalog,
                                                                    int face,
                                                                    int hair) {
        std::vector<std::string> paths;
        const auto* body = catalog.find(wear.skin_index(protocol::SkinSlot::Upper));
        const std::string body_path =
            body && body->full_body_skin() ? skin_gb_path(data_root, c, *body) : std::string{};
        if (!body_path.empty()) {
            paths.push_back(body_path);
        }
        constexpr protocol::SkinSlot skin_parts[] = {protocol::SkinSlot::Boots,
                                                     protocol::SkinSlot::Gauntlet,
                                                     protocol::SkinSlot::Pants,
                                                     protocol::SkinSlot::Upper,
                                                     protocol::SkinSlot::Face,
                                                     protocol::SkinSlot::Helmet};
        for (int i = 0; i < static_cast<int>(ClothPart::Count); ++i) {
            if (!body_path.empty() && i < static_cast<int>(ClothPart::Face)) {
                continue;
            }
            if (body_path.empty()) {
                if (const auto* skin = catalog.find(wear.skin_index(skin_parts[i]));
                    skin && skin->is_skin()) {
                    const auto path = skin_gb_path(data_root, c, *skin);
                    if (!path.empty()) {
                        paths.push_back(path);
                        continue;
                    }
                }
            }
            const auto part = static_cast<ClothPart>(i);
            if (!body_path.empty() && ((part == ClothPart::Helmet && !body->skin_keeps_hair()) ||
                                       (part == ClothPart::Face && body->skin_replaces_face()))) {
                continue;
            }
            int ac = 0;
            int ai = part == ClothPart::Face
                         ? std::clamp(face, 1, 255)
                         : (part == ClothPart::Helmet ? std::clamp(hair, 1, 255) : 1);
            if (body_path.empty() && wear.cloth_index(part) != 0) {
                const ItemDef* def = catalog.find(wear.cloth_index(part));
                if (def && def->action_class() > 0) {
                    ac = def->action_class();
                    ai = def->action_index() > 0 ? def->action_index() : 1;
                }
            }
            std::string p = clothes_gb_path(data_root, c, part, ac, ai);
            if (p.empty() && (ac != 0 || ai != 1)) {
                p = clothes_gb_path(data_root, c, part, 0, 1);
            }
            if (!p.empty()) {
                paths.push_back(std::move(p));
            }
        }
        return paths;
    }

    std::vector<std::string>
    EquipmentPaths::shinsu_clothes_paths(const std::string& data_root, int type, int stage) {
        std::vector<std::string> paths;
        const std::string dir = platform::join_path(data_root, "Model/Clothes");
        const char* prefix = stage >= 3 ? "GD" : "GB";
        for (int i = 1; i <= 10; ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "%s%02d_B%02d.gb", prefix, std::max(0, type), i);
            std::string p = resolve_existing(dir, name);
            if (!p.empty()) {
                paths.push_back(std::move(p));
            }
        }
        return paths;
    }

}  // namespace openkal::client
