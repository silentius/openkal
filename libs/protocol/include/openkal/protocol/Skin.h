/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace openkal::protocol {
    /** Combat weapon categories accepted by cosmetic appearances. */
    enum class WeaponSkinType { None, Sword, Sword2h, Wand, Bow, Dagger };

    /** Resolves an ordinary weapon class token.
     * @param name Weapon subclass from the item definition.
     * @return Supported weapon type, or None.
     */
    [[nodiscard]] inline WeaponSkinType weapon_skin_type_for_name(std::string_view name) {
        if (name == "sword") {
            return WeaponSkinType::Sword;
        }
        if (name == "sword2h") {
            return WeaponSkinType::Sword2h;
        }
        if (name == "wand") {
            return WeaponSkinType::Wand;
        }
        if (name == "bow") {
            return WeaponSkinType::Bow;
        }
        if (name == "dagger") {
            return WeaponSkinType::Dagger;
        }
        return WeaponSkinType::None;
    }

    /** Resolves a skin's explicit type or its unambiguous class default.
     * @param configured Optional skin.weapon_type token; invalid values fail closed.
     * @param clazz Class limit: Knight 0, Magician 1, Archer 2, Thief 3.
     * @return Required weapon type; Knight skins require explicit classification.
     */
    [[nodiscard]] inline WeaponSkinType resolve_weapon_skin_type(std::string_view configured,
                                                                 int clazz) {
        if (!configured.empty()) {
            return weapon_skin_type_for_name(configured);
        }
        if (clazz == 1) {
            return WeaponSkinType::Wand;
        }
        if (clazz == 2) {
            return WeaponSkinType::Bow;
        }
        if (clazz == 3) {
            return WeaponSkinType::Dagger;
        }
        return WeaponSkinType::None;
    }

    /** Stable cosmetic slots, independent of combat equipment. */
    enum class SkinSlot : int {
        Weapon = 0,
        Shield,
        Helmet,
        Upper,
        Pants,
        Boots,
        Gauntlet,
        Face,
        Count,
        None = -1
    };

    /** Number of cosmetic slots in an appearance snapshot. */
    inline constexpr std::size_t kSkinSlots = static_cast<std::size_t>(SkinSlot::Count);

    /** Cosmetic definition indices; zero denotes an empty slot. */
    using SkinIndices = std::array<std::uint16_t, kSkinSlots>;

    /** Resolves a skin definition's part name.
     * @param part Second class token; body occupies the upper slot.
     * @return Cosmetic slot, or None for an unsupported part.
     */
    [[nodiscard]] inline SkinSlot skin_slot_for_part(std::string_view part) {
        if (part == "weapon") {
            return SkinSlot::Weapon;
        }
        if (part == "shield") {
            return SkinSlot::Shield;
        }
        if (part == "helmet") {
            return SkinSlot::Helmet;
        }
        if (part == "upper" || part == "body") {
            return SkinSlot::Upper;
        }
        if (part == "pants") {
            return SkinSlot::Pants;
        }
        if (part == "boots") {
            return SkinSlot::Boots;
        }
        if (part == "gauntlet") {
            return SkinSlot::Gauntlet;
        }
        if (part == "face") {
            return SkinSlot::Face;
        }
        return SkinSlot::None;
    }

    /** Tests whether a slot belongs to the character's body.
     * @param slot Cosmetic slot.
     * @return True for clothing, head and face slots.
     */
    [[nodiscard]] inline bool is_body_skin_slot(SkinSlot slot) {
        return slot >= SkinSlot::Helmet && slot < SkinSlot::Count;
    }

    /** Determines which existing skins an incoming skin must return to the bag.
     * @param incoming Incoming cosmetic slot.
     * @param full_body Whether the incoming skin replaces the whole body.
     * @param existing Existing cosmetic slot.
     * @param existing_full_body Whether the existing skin replaces the whole body.
     * @return True for a replacement or mutually exclusive body skins.
     */
    [[nodiscard]] inline bool skin_slots_conflict(SkinSlot incoming,
                                                  bool full_body,
                                                  SkinSlot existing,
                                                  bool existing_full_body) {
        return incoming != SkinSlot::None && existing != SkinSlot::None &&
               (incoming == existing ||
                (is_body_skin_slot(incoming) && is_body_skin_slot(existing) &&
                 (full_body || existing_full_body)));
    }
}  // namespace openkal::protocol
