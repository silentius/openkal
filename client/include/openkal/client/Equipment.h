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

#include "openkal/client/ItemDb.h"
#include "openkal/client/Player.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::client {

    /** Clothes part order, matching KalOnline Wear indices and the SGPAFH mesh
     * suffixes. */
    enum class ClothPart : int {
        /** Boots — C{job}_*_s*.gb. */
        Boots = 0,

        /** Gauntlet — C{job}_*_g*.gb. */
        Gauntlet = 1,

        /** Pants / lower armor — C{job}_*_p*.gb. */
        Pants = 2,

        /** Upper armor — C{job}_*_a*.gb. */
        Upper = 3,

        /** Face — C{job}_*_f*.gb. */
        Face = 4,

        /** Helmet — C{job}_*_h*.gb. */
        Helmet = 5,

        /** Number of clothes parts. */
        Count = 6,
    };

    /** What the character currently has on the body, as item definition indices (0 =
     * empty). */
    class Wear {
      public:
        /** @return item definition index of the equipped weapon, or 0. */
        [[nodiscard]] std::int32_t weapon_index() const {
            return m_weapon;
        }

        /**
         * Equips or clears the weapon slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_weapon_index(std::int32_t v) {
            m_weapon = v;
        }

        /** @return item definition index of the equipped shield, or 0. */
        [[nodiscard]] std::int32_t shield_index() const {
            return m_shield;
        }

        /**
         * Equips or clears the shield slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_shield_index(std::int32_t v) {
            m_shield = v;
        }

        /**
         * Reads one clothes slot.
         * @param part Clothes part.
         * @return item definition index, or 0 when empty or the part is out of range.
         */
        [[nodiscard]] std::int32_t cloth_index(ClothPart part) const;

        /**
         * Equips or clears one clothes slot.
         * @param part Clothes part; out-of-range values are ignored.
         * @param v item definition index, or 0 to clear.
         */
        void set_cloth_index(ClothPart part, std::int32_t v);

        /** @return True if a weapon is equipped. */
        [[nodiscard]] bool has_weapon() const {
            return m_weapon != 0;
        }

        /** @return True if a shield is equipped. */
        [[nodiscard]] bool has_shield() const {
            return m_shield != 0;
        }

        /** @return item definition index of the equipped transform item, or 0. */
        [[nodiscard]] std::int32_t transform_index() const {
            return m_transform;
        }

        /**
         * Equips or clears the transform slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_transform_index(std::int32_t v) {
            m_transform = v;
        }

        /** @return item definition index of the equipped pet, or 0. */
        [[nodiscard]] std::int32_t pet_index() const {
            return m_pet;
        }

        /**
         * Equips or clears the pet slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_pet_index(std::int32_t v) {
            m_pet = v;
        }

        /** @return item definition index of the registered common riding animal, or 0. */
        [[nodiscard]] std::int32_t ride_index() const {
            return m_ride;
        }

        /**
         * Registers or clears the common riding-animal slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_ride_index(std::int32_t v) {
            m_ride = v;
        }

        /** @return item definition index of the equipped ring, or 0. */
        [[nodiscard]] std::int32_t ring_index() const {
            return m_ring;
        }

        /**
         * Equips or clears the ring slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_ring_index(std::int32_t v) {
            m_ring = v;
        }

        /** @return item definition index of the equipped necklace, or 0. */
        [[nodiscard]] std::int32_t necklace_index() const {
            return m_necklace;
        }

        /**
         * Equips or clears the necklace slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_necklace_index(std::int32_t v) {
            m_necklace = v;
        }

        /** @return item definition index of the equipped trinket, or 0. */
        [[nodiscard]] std::int32_t trinket_index() const {
            return m_trinket;
        }

        /**
         * Equips or clears the trinket slot.
         * @param v item definition index, or 0 to clear.
         */
        void set_trinket_index(std::int32_t v) {
            m_trinket = v;
        }

        /**
         * Tests whether an item is worn in any slot.
         * @param index item definition index.
         * @return True if the item is currently equipped.
         */
        [[nodiscard]] bool is_equipped(std::int32_t index) const;

        /**
         * Clears every wear slot that holds this item definition index.
         * @param index item definition index; 0 is ignored.
         */
        void unequip_index(std::int32_t index);

        /** Unequips every slot. */
        void clear();

        /**
         * Equips one item definition into the matching wear slot.
         * @param def Item definition (weapon, defense, ornament, or transform).
         */
        void apply_item(const ItemDef& def);

        /**
         * Builds wear from S2C_PLAYERINFO equipped item definition indices.
         * Helmets are skipped after a mask, matching CharSelSystem::ProcessPacket.
         * @param catalog item definition catalog.
         * @param indices Equipped item indices from PLAYERINFO.
         * @return Wear snapshot for the character model.
         */
        [[nodiscard]] static Wear from_item_indices(const ItemDb& catalog,
                                                    const std::vector<std::uint16_t>& indices);

        /** Reads one cosmetic slot.
         * @param slot Cosmetic slot.
         * @return Skin definition index, or zero for an invalid or empty slot.
         */
        [[nodiscard]] std::int32_t skin_index(protocol::SkinSlot slot) const;

        /** @return True when a whole-body skin occupies the chest skin slot. */
        [[nodiscard]] bool has_full_body_skin() const {
            return m_full_body_skin;
        }

      private:
        /** Cosmetic indices, independent of the underlying equipment. */
        std::array<std::int32_t, protocol::kSkinSlots> m_skins{};
        /** Whether the chest skin replaces the whole body. */
        bool m_full_body_skin = false;

        /** Number of clothes slots. */
        static constexpr std::size_t kClothCount = static_cast<std::size_t>(ClothPart::Count);

        /** Equipped weapon (item definition index, 0 = empty). */
        std::int32_t m_weapon = 0;

        /** Equipped shield (item definition index, 0 = empty). */
        std::int32_t m_shield = 0;

        /** Equipped clothes in SGPAFH order (item definition index, 0 = empty). */
        std::array<std::int32_t, kClothCount> m_cloth{};

        /** Equipped transform item (item definition index, 0 = empty). */
        std::int32_t m_transform = 0;

        /** Equipped pet item (item definition index, 0 = empty). */
        std::int32_t m_pet = 0;

        /** Registered common riding item (item definition index, 0 = empty). */
        std::int32_t m_ride = 0;

        /** Equipped ring (item definition index, 0 = empty). */
        std::int32_t m_ring = 0;

        /** Equipped necklace (item definition index, 0 = empty). */
        std::int32_t m_necklace = 0;

        /** Equipped trinket (item definition index, 0 = empty). */
        std::int32_t m_trinket = 0;
    };

    /** Resolves KalOnline mesh paths and skeleton sockets for worn equipment. */
    class EquipmentPaths {
      public:
        /**
         * Maps a clothes part to its mesh filename letter.
         * @param p Clothes part.
         * @return Letter from "sgpafh"; 'a' for out-of-range values.
         */
        [[nodiscard]] static char cloth_part_letter(ClothPart p);

        /**
         * Maps an item definition small class string to a clothes part.
         * @param small Item class_small value, e.g. "upperarmor".
         * @return Matching part; ClothPart::Upper when unknown.
         */
        [[nodiscard]] static ClothPart cloth_part_from_small(const std::string& small);

        /**
         * Species letter for weapon paths P{S|G|B|D}_S{a}_{b:02d}
         * (GameObject::runtime state).
         * @param c Player class.
         * @return Species letter.
         */
        [[nodiscard]] static char species_letter(Class c);

        /**
         * Weapon attach bone, from macro-e characterbone (part weapon shield).
         * @param c Player class.
         * @return Bone index.
         */
        [[nodiscard]] static int weapon_bone_index(Class c);

        /**
         * Shield / off-hand attach bone, from macro-e characterbone part[1].
         * Knight 32, archer quiver 34, thief off-hand 28; 0 when unused.
         * @param c Player class.
         * @return Bone index, or 0 when the class has no shield socket.
         */
        [[nodiscard]] static int shield_bone_index(Class c);

        /**
         * Local +Y (mesh thickness) applied after attaching a knight shield to
         * bone 32. Shield GBs place the back plate at the bone origin, which
         * intersects skinned gauntlets and chest; other classes keep 0.
         * @param c Player class.
         * @return Offset in model units, or 0.
         */
        [[nodiscard]] static float shield_local_offset_y(Class c);

        /**
         * Cast / projectile origin socket, from characterbone
         * bone[EFFECTBONE_EFFECT].
         * @param c Player class.
         * @return Bone index.
         */
        [[nodiscard]] static int effect_bone_index(Class c);

        /**
         * Maps an EFFECTBONE slot to a skeleton bone (macro-e characterbone).
         * @param c Player class.
         * @param effect_slot EFFECTBONE index (0..7), or -1 for origin.
         * @return Skeleton bone index, or -1 when the slot is origin / invalid.
         */
        [[nodiscard]] static int effect_slot_skeleton(Class c, int effect_slot);

        /**
         * Effect scale numerator from characterbone target[3] (scale = value *
         * model_scale / 16).
         * @param c Player class.
         * @return Target height.
         */
        [[nodiscard]] static float effect_target_height(Class c);

        /** Resolves a cosmetic mesh without borrowing a different class's model.
         * @param data_root Optional client asset root.
         * @param c Character class.
         * @param def Cosmetic definition.
         * @return Existing mesh path, or empty when incompatible or unavailable.
         */
        [[nodiscard]] static std::string
        skin_gb_path(const std::string& data_root, Class c, const ItemDef& def);

        /**
         * Joins a directory and filename when the resulting file exists.
         * Falls back to case-insensitive lookup so mixed-case KalOnline
         * clothes names (e.g. compatibility behavior.gb) resolve on Linux.
         * @param dir Directory.
         * @param name Filename.
         * @return Full path, or empty when the file is missing.
         */
        [[nodiscard]] static std::string resolve_existing(const std::string& dir,
                                                          const std::string& name);

        /**
         * Resolves a clothes mesh, trying lowercase then uppercase job letters
         * and matching on-disk filename case (e.g. compatibility behavior.gb).
         * @param data_root Client data root.
         * @param c Player class.
         * @param part Clothes part.
         * @param action_class item definition (Action a b) → a.
         * @param action_index item definition (Action a b) → b.
         * @return Existing .gb path, or empty.
         */
        [[nodiscard]] static std::string clothes_gb_path(const std::string& data_root,
                                                         Class c,
                                                         ClothPart part,
                                                         int action_class,
                                                         int action_index);

        /**
         * Resolves a weapon mesh across the known species and separator spellings.
         * @param data_root Client data root.
         * @param c Player class.
         * @param action_class item definition (Action a b) → a.
         * @param action_index item definition (Action a b) → b.
         * @return Existing .gb path, or empty.
         */
        [[nodiscard]] static std::string
        weapon_gb_path(const std::string& data_root, Class c, int action_class, int action_index);

        /**
         * Resolves a shield mesh: knight `P{S}_B{a}_{b:02d}`, thief off-hand
         * `P{D}_s{a}_{b:02d}` (compatibility behavior).
         * @param data_root Client data root.
         * @param c Player class.
         * @param action_class item definition (Action a b) → a.
         * @param action_index item definition (Action a b) → b.
         * @return Existing .gb path, or empty.
         */
        [[nodiscard]] static std::string
        shield_gb_path(const std::string& data_root, Class c, int action_class, int action_index);

        /**
         * Resolves every clothes mesh for a worn set, falling back to the default
         * look per part.
         * @param data_root Client data root.
         * @param c Player class.
         * @param wear Worn equipment.
         * @param catalog Item catalog used to resolve action class/index.
         * @param face Natural face mesh index when no mask is equipped.
         * @param hair Natural hair mesh index when no helmet is equipped.
         * @return Existing .gb paths, in part order.
         */
        [[nodiscard]] static std::vector<std::string>
        clothes_paths_for_wear(const std::string& data_root,
                               Class c,
                               const Wear& wear,
                               const ItemDb& catalog,
                               int face = 1,
                               int hair = 1);

        /**
         * Shinsu clothes GBs (OnChangedPart `GB%02d_B%02d` / `GD%02d_B%02d`).
         * @param data_root Client data root.
         * @param type Metamo race (0 tiger / 4 panther / 6 bird / 8 gale).
         * @param stage 1–2 baby/young use GB, 3+ adult uses GD.
         * @return Existing mesh paths for parts 1–10.
         */
        [[nodiscard]] static std::vector<std::string>
        shinsu_clothes_paths(const std::string& data_root, int type, int stage);
    };

}  // namespace openkal::client
