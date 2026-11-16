/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Inventory.h"

#include "openkal/client/ItemTooltip.h"
#include "openkal/client/RidingEquipment.h"
#include "openkal/client/ShinsuDb.h"
#include "openkal/client/UiView.h"
#include "openkal/platform/Json.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/Transform.h"
#include "openkal/server/CollectionEquipment.h"
#include "openkal/server/DragonFusing.h"
#include "openkal/server/PrefixEnchant.h"
#include "openkal/server/Qigong.h"
#include "openkal/server/WeaponEnchantment.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <string>
#include <utility>

namespace openkal::client {
    namespace {

        constexpr std::array<const char*, 6> kFilterIds{
            "consumables", "weapons", "armors", "enchantment", "skins_pets", "other"};
        constexpr std::array<const char*, 6> kFilterLabels{
            "Consumables", "Weapons", "Armors", "Enchantment", "Skins & Pets", "Other items"};

        int filter_button_index(std::string_view id) {
            for (int i = 0; i < static_cast<int>(kFilterIds.size()); ++i) {
                if (id == std::string("filter.") + kFilterIds[i]) {
                    return i;
                }
            }
            return -1;
        }

        int item_filter_category(const ItemDef* def) {
            if (!def) {
                return 5;
            }
            if (def->is_skin() || def->is_pet() || def->is_transform() || def->is_ride() ||
                def->class_small() == "face" || def->class_small() == "mask") {
                return 4;
            }
            if (def->is_weapon()) {
                return 1;
            }
            if (def->is_defense() || def->is_ornament()) {
                return 2;
            }
            if (def->has_changeprefix() || def->has_charming() || def->has_bof() ||
                server::Qigong::is_perforation_shot(def->index()) ||
                server::Qigong::is_talisman(def->index()) ||
                server::DragonFusing::is_scroll(def->index()) ||
                server::enchantment_talisman_grade(def->index()) != 0 ||
                def->index() == server::kRevisionHammerIndex ||
                def->index() == server::kPolishingStoneIndex ||
                server::collection_def(def->index()).material ||
                server::collection_def(def->index()).kind == server::CollectionKind::Essence ||
                def->index() == server::kYeoijuFortification) {
                return 3;
            }
            return def->is_consumable() ? 0 : 5;
        }

        /** Steady clock ms for equip cooldown checks. */
        [[nodiscard]] std::uint64_t steady_ms_now() {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }

        /** Em size used when layout font_size is a bitmap-style scale (1..8). */
        constexpr int kBodyPixelSize = 12;
        constexpr std::uint64_t kDoubleClickWindowMs = 450;

        [[nodiscard]] std::string
        transform_egg_display_name(const ItemDef& def, int egg_level, const ShinsuDb* shinsu) {
            const int stage = protocol::max_transform_stage(egg_level);
            if (stage >= 1 && shinsu) {
                const std::string grown = shinsu->stage_name(def.transform_race(), stage);
                if (!grown.empty()) {
                    return grown;
                }
            }
            return def.name();
        }

        [[nodiscard]] std::string format_duration(std::uint64_t seconds) {
            const std::uint64_t days = seconds / 86400ull;
            seconds %= 86400ull;
            const std::uint64_t hours = seconds / 3600ull;
            seconds %= 3600ull;
            const std::uint64_t minutes = seconds / 60ull;
            seconds %= 60ull;
            std::string text;
            if (days > 0) {
                text += std::to_string(days) + "d ";
            }
            if (hours > 0 || !text.empty()) {
                text += std::to_string(hours) + "h ";
            }
            if (minutes > 0 || !text.empty()) {
                text += std::to_string(minutes) + "m ";
            }
            return text + std::to_string(seconds) + "s";
        }

        int Li(const UiLayout* lay, const char* key, int fallback) {
            return UiLayout::get_i(lay, key, fallback);
        }

        [[nodiscard]] bool wear_id_matches(const std::string& id, const ItemDef& def) {
            if (id.compare(0, 5, "skin.") == 0) {
                return def.is_skin() &&
                       def.skin_slot() == protocol::skin_slot_for_part(id.substr(5));
            }
            if (def.is_skin()) {
                return false;
            }
            if (id == "weapon") {
                return def.is_weapon();
            }
            if (id == "shield") {
                return def.class_small() == "shield";
            }
            if (id == "pants") {
                return def.class_small() == "lowerarmor" || def.class_small() == "pants";
            }
            if (id == "upper") {
                return def.class_small() == "upperarmor" || def.class_small() == "upper";
            }
            if (id == "accessory") {
                return def.is_ornament() && def.class_small() != "ring" &&
                       def.class_small() != "necklace" && def.class_small() != "resistnecklace";
            }
            if (id == "helmet") {
                return def.class_small() == "helmet" || def.class_small() == "hat";
            }
            if (id == "gauntlet") {
                return def.class_small() == "gauntlet" || def.class_small() == "gloves";
            }
            if (id == "necklace") {
                return def.class_small() == "necklace" || def.class_small() == "resistnecklace";
            }
            if (id == "sinsu") {
                return def.is_transform();
            }
            if (id == "pet") {
                return def.is_pet();
            }
            if (id == "ride") {
                return def.is_ride();
            }
            if (id == "face") {
                return def.class_small() == "face" || def.class_small() == "mask";
            }
            return def.class_small() == id;
        }

        /** Layout slot id for one paperdoll cell, falling back to the default order. */
        [[nodiscard]] std::string wear_layout_slot_id(const UiLayout* layout, int left, int index) {
            static constexpr const char* kLeft[] = {
                "helmet", "upper", "gauntlet", "pants", "boots", "weapon", "shield"};
            static constexpr const char* kRight[] = {
                "ring", "necklace", "accessory", "face", "sinsu", "pet", "ride"};
            if (left == 3) {
                return index == 0 ? "skin.face" : "";
            }
            if (left == 2) {
                return index >= 0 && index < 7 ? std::string("skin.") + kLeft[index] : "";
            }
            std::string id;
            if (index >= 0 && index < 7) {
                id = left ? kLeft[index] : kRight[index];
            }
            if (!layout) {
                return id;
            }
            const auto* col = layout->root().get(left ? "wear_left" : "wear_right");
            const auto* slots = col ? col->get("slots") : nullptr;
            if (!slots || !slots->is_array() || index < 0 ||
                index >= static_cast<int>(slots->array().size())) {
                return id;
            }
            const auto* idj = slots->array()[static_cast<std::size_t>(index)].get("id");
            if (idj && idj->is_string() && !idj->as_string().empty()) {
                return idj->as_string();
            }
            return id;
        }

        /**
         * item definition index currently shown in a paperdoll slot.
         * Each cell resolves one ordinary equipment category, including mask and mount.
         */
        [[nodiscard]] std::int32_t wear_equipped_index(const Wear& wear, const std::string& id) {
            if (id.compare(0, 5, "skin.") == 0) {
                return wear.skin_index(protocol::skin_slot_for_part(id.substr(5)));
            }
            if (id == "weapon") {
                return wear.weapon_index();
            }
            if (id == "shield") {
                return wear.shield_index();
            }
            if (id == "ring") {
                return wear.ring_index();
            }
            if (id == "necklace") {
                return wear.necklace_index();
            }
            if (id == "accessory") {
                return wear.trinket_index();
            }
            if (id == "face") {
                return wear.cloth_index(ClothPart::Face);
            }
            if (id == "pet") {
                return wear.pet_index();
            }
            if (id == "sinsu") {
                return wear.transform_index();
            }
            if (id == "ride") {
                return wear.ride_index();
            }
            std::string small = id;
            if (id == "pants") {
                small = "lowerarmor";
            } else if (id == "upper") {
                small = "upperarmor";
            }
            return wear.cloth_index(EquipmentPaths::cloth_part_from_small(small));
        }

        /** Resolves the dedicated ordinary equipment cell for a client definition. */
        [[nodiscard]] int equipment_cell(const ItemDef& def) {
            if (def.is_skin()) {
                return 15 + static_cast<int>(def.skin_slot());
            }
            if (def.is_weapon()) {
                return 0;
            }
            if (def.is_transform()) {
                return 11;
            }
            if (def.is_pet()) {
                return 12;
            }
            if (def.is_ride()) {
                return 13;
            }
            const auto& small = def.class_small();
            if (small == "shield") {
                return 1;
            }
            if (small == "helmet" || small == "hat") {
                return 2;
            }
            if (small == "upperarmor" || small == "upper") {
                return 3;
            }
            if (small == "lowerarmor" || small == "pants") {
                return 4;
            }
            if (small == "boots") {
                return 5;
            }
            if (small == "gauntlet" || small == "gloves") {
                return 6;
            }
            if (small == "face" || small == "mask") {
                return 7;
            }
            if (small == "ring") {
                return 8;
            }
            if (small == "necklace" || small == "resistnecklace") {
                return 9;
            }
            if (def.is_defense()) {
                return 3;
            }
            if (def.is_ornament()) {
                return 10;
            }
            return -1;
        }

        /** Combined UI slot holding the worn instance of `index`, or -1. */
        [[nodiscard]] int bag_slot_for_equipped(const Bag& bag, std::int32_t index) {
            if (index <= 0) {
                return -1;
            }
            for (int i = 0; i < static_cast<int>(bag.equipment.size()); ++i) {
                const auto& slot = bag.equipment[static_cast<std::size_t>(i)];
                if (slot.index == index && slot.equipped && slot.count > 0) {
                    return kBagSlots + i;
                }
            }
            return -1;
        }

        /** True when a prefix or charming talisman may enchant this destination. */
        [[nodiscard]] bool charm_matches_gear(const ItemDef& talisman, const ItemDef& dest) {
            const std::string& target = talisman.changeprefix_target();
            if (target == "weapon") {
                return dest.is_weapon();
            }
            if (target == "defense") {
                return dest.is_defense();
            }
            if (target == "ornament") {
                return dest.is_ornament();
            }
            if (talisman.has_charming()) {
                if (dest.grade() < 40) {
                    return false;
                }
                const std::string& kind = talisman.charming();
                const bool wand = dest.class_small() == "wand";
                if (kind == "xattack") {
                    return dest.is_weapon() && !wand;
                }
                if (kind == "xmagic") {
                    return dest.is_weapon() && wand;
                }
                if (kind == "xhit") {
                    return dest.is_weapon();
                }
                if (kind == "xdefense" || kind == "xdodge") {
                    return dest.is_defense();
                }
            }
            if (talisman.has_bof()) {
                return dest.is_defense() && dest.grade() >= 46 && dest.class_small() != "shield" &&
                       dest.class_small() != "face" && dest.class_small() != "mask";
            }
            return false;
        }

        /** Copies packet or catalog endurance onto a newly filled stack. */
        void apply_stack_endurance(ItemStack& slot, const ItemDef* def, int cur_end, int max_end) {
            if (def && def->is_ornament()) {
                slot.cur_end = 0;
                slot.max_end = 0;
                return;
            }
            if (max_end > 0) {
                slot.max_end = max_end;
            } else if (def && def->endurance() > 0) {
                slot.max_end = def->endurance();
            }
            if (cur_end > 0) {
                slot.cur_end = cur_end;
            } else if (slot.cur_end <= 0 && slot.max_end > 0) {
                slot.cur_end = slot.max_end;
            }
        }

        std::vector<server::CollectionItem> collection_items(const Bag& bag) {
            std::vector<server::CollectionItem> result;
            for (const auto* slots : {&bag.normal, &bag.collection}) {
                for (const auto& item : *slots) {
                    if (item.count > 0) {
                        result.push_back({item.iid,
                                          item.index,
                                          item.upgrade_level,
                                          item.random_stat_type,
                                          item.random_stat_value,
                                          item.mix_type,
                                          item.equipped});
                    }
                }
            }
            return result;
        }

        int collection_item_slot(const Bag& bag, int slot) {
            if (slot < 0 || slot >= server::kCollectionSlots) {
                return -1;
            }
            const auto iid = server::collection_slots(collection_items(bag))[slot];
            if (!iid) {
                return -1;
            }
            for (int i = 0; i < static_cast<int>(bag.collection.size()); ++i) {
                if (bag.collection[i].iid == iid && bag.collection[i].count > 0) {
                    return i;
                }
            }
            return -1;
        }

        const ItemDef* inventory_unlock_writ(const Bag& bag, const ItemDb* catalog) {
            const ItemDef* selected = nullptr;
            if (!catalog) {
                return nullptr;
            }
            for (const auto* slots : {&bag.normal, &bag.quest}) {
                for (const auto& item : *slots) {
                    const auto* def = catalog->find(item.index);
                    if (item.count > 0 && def &&
                        def->buff_type() == static_cast<int>(protocol::BuffType::InventoryUnlock) &&
                        (!selected || def->buff_remain() < selected->buff_remain())) {
                        selected = def;
                    }
                }
            }
            return selected;
        }

        class InventoryHost final : public UiViewHost {
          public:
            InventoryHost(const Window& window,
                          const Bag& bag,
                          const std::array<std::uint32_t, 5>* riding,
                          std::string_view hovered,
                          bool pressed)
                : m_window(window), m_bag(bag), m_riding(riding), m_hovered(hovered),
                  m_pressed(pressed) {}

            bool resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override {
                out.set_empty(true);
                constexpr std::string_view general = "inventory.general.";
                const bool general_grid = bind.substr(0, general.size()) == general;
                if (general_grid || bind == "inventory.quest" || bind == "inventory.task") {
                    if (general_grid) {
                        index += std::atoi(std::string(bind.substr(general.size())).c_str()) *
                                 kSlotsPerPage;
                    } else if (bind == "inventory.task") {
                        index += kOtherSectionSlots;
                    }
                    if (general_grid) {
                        index = m_window.general_source_slot(index);
                    }
                    const auto& slots = general_grid ? m_bag.normal : m_bag.quest;
                    if (index < 0 || index >= static_cast<int>(slots.size()) ||
                        (index == m_window.drag_slot() &&
                         general_grid == (m_window.drag_source_tab() != Tab::Quest))) {
                        return true;
                    }
                    const auto& item = slots[static_cast<std::size_t>(index)];
                    if (item.index == 0 || item.count <= 0) {
                        return true;
                    }
                    fill_icon(out, item.index, item.transform_level);
                    out.set_count(item.count);
                    out.set_high_contrast_count(true);
                    out.set_account_bound(item.account_bound);
                    out.set_character_bound(item.character_bound);
                    out.set_enchant_protected(item.enchant_protected);
                    out.set_equipped(item.equipped || (m_riding && riding_equipment_contains_iid(
                                                                       *m_riding, item.iid)));
                    return true;
                }
                constexpr std::string_view collection = "inventory.collection.";
                if (bind.substr(0, collection.size()) == collection) {
                    const int cell = std::atoi(std::string(bind.substr(collection.size())).c_str());
                    const int slot = collection_item_slot(m_bag, cell);
                    if (slot >= 0) {
                        const auto& item = m_bag.collection[slot];
                        fill_icon(out, item.index, 1);
                        out.set_equipped(true);
                        out.set_account_bound(item.account_bound);
                        out.set_character_bound(item.character_bound);
                        if (item.upgrade_level > 0) {
                            out.set_count(item.upgrade_level);
                        }
                    }
                    return true;
                }
                constexpr std::string_view prefix = "inventory.wear.";
                if (bind.substr(0, prefix.size()) == prefix && m_window.wear()) {
                    const auto item = wear_equipped_index(*m_window.wear(),
                                                          std::string(bind.substr(prefix.size())));
                    if (item != 0) {
                        fill_icon(out, item, m_bag.transform_level_for_index(item));
                        out.set_account_bound(m_bag.account_bound_for_index(item));
                        const int bag_slot = bag_slot_for_equipped(m_bag, item);
                        if (bag_slot >= 0) {
                            out.set_enchant_protected(m_bag.item_at(bag_slot)->enchant_protected);
                            out.set_character_bound(m_bag.item_at(bag_slot)->character_bound);
                        }
                    }
                    return true;
                }
                return false;
            }

            bool button_enabled(std::string_view action) const override {
                if (action == "inventory.expand") {
                    return m_window.slots_capacity() < kBagSlots && !m_window.capturing_pointer() &&
                           inventory_unlock_writ(m_bag, m_window.catalog());
                }
                if (action.substr(0, 17) == "inventory.filter.") {
                    return !m_window.capturing_pointer();
                }
                return action != "inventory.sort" || !m_window.capturing_pointer();
            }

            bool button_hovered(std::string_view id) const override {
                return id == m_hovered;
            }

            bool button_pressed(std::string_view id) const override {
                const int category = filter_button_index(id);
                return (category >= 0 && m_window.filter_active(category)) ||
                       (id == m_hovered && m_pressed);
            }

          private:
            void fill_icon(UiSlotIcon& out, std::int32_t index, int level) const {
                out.set_empty(false);
                if (m_window.catalog()) {
                    out.set_file(m_window.catalog()->icon_filename(index, level));
                }
            }

            const Window& m_window;
            const Bag& m_bag;
            const std::array<std::uint32_t, 5>* m_riding;
            std::string_view m_hovered;
            bool m_pressed;
        };

    }  // namespace

    std::array<int, 5> collection_primary_bonuses(const Bag& bag) {
        const auto bonuses = server::collection_bonuses(collection_items(bag), false);
        return {static_cast<int>(bonuses.ability[0]),
                static_cast<int>(bonuses.ability[3]),
                static_cast<int>(bonuses.ability[2]),
                static_cast<int>(bonuses.ability[1]),
                static_cast<int>(bonuses.ability[4])};
    }

    int Bag::starter_weapon_index(int clazz) {
        switch (clazz) {
        case 1:
            return kStarterWand;
        case 2:
            return kStarterBow;
        case 3:
            return kStarterDagger;
        default:
            return kStarterSword;
        }
    }

    void Bag::clear() {
        normal.assign(kBagSlots, ItemStack{});
        quest.assign(kOtherSlots, ItemStack{});
        collection.clear();
        equipment.clear();
        money = 0;
    }

    void Bag::seed_demo(const ItemDb& catalog, int clazz, int level) {
        clear();
        money = 38;
        auto put = [&](int, std::int32_t index, int count) {
            if (catalog.find(index)) {
                (void) try_insert(index, count, catalog);
            }
        };
        put(0, 47, 5);  // Small Medicine
        put(1, 48, 3);  // Medium Medicine
        put(2, starter_weapon_index(clazz), 1);
        if (clazz == 0) {
            put(3, kScrollNarootuh, 5);
            put(4, kScrollJookSuh, 5);
            put(5, kScrollGeumOhMine, 5);
            put(6, kScrollGiantBird, 5);
            put(7, kScrollGeumOheeFort, 5);
            put(8, kScrollCityOfPriest, 5);
        } else {
            put(3, 6, 1);  // Basic upper armor
        }
        (void) level;
        const bool all_on[kMaxRegisters] = {true, true, true, true, true, true};
        sort_compact(all_on);
    }

    bool Bag::try_insert(std::int32_t index,
                         int count,
                         const ItemDb& catalog,
                         int prefix,
                         std::uint32_t iid,
                         int cur_end,
                         int max_end,
                         bool equipped) {
        if (index <= 0 || count <= 0) {
            return false;
        }
        if (prefix < 0) {
            prefix = 0;
        }
        if (index == 31) {
            money += count;
            return true;
        }
        const ItemDef* def = catalog.find(index);
        if (equipped) {
            const bool is_collection =
                server::collection_def(index).kind != server::CollectionKind::None;
            auto& worn = is_collection ? collection : equipment;
            worn.resize(is_collection ? server::kCollectionSlots : kEquipmentSlots);
            if (iid == 0 || count != 1) {
                return false;
            }
            for (auto& item : worn) {
                if (item.iid == iid) {
                    return true;
                }
            }
            if (!is_collection && (!def || equipment_cell(*def) < 0)) {
                return false;
            }
            for (auto& item : worn) {
                if (item.count <= 0 && (is_collection || &item == &worn[equipment_cell(*def)])) {
                    item = ItemStack{index, 1, true, prefix, iid};
                    apply_stack_endurance(item, def, cur_end, max_end);
                    return true;
                }
            }
            return false;
        }
        const bool other = def && def->is_quest();
        auto* destination = other ? &quest : &normal;
        int base = other && def->is_task() ? kOtherSectionSlots : 0;
        int end = other ? base + kOtherSectionSlots : kBagSlots;
        destination->resize(std::max(destination->size(),
                                     static_cast<std::size_t>(other ? kOtherSlots : kBagSlots)));
        const bool stackable = def && def->is_stackable();
        // Login and authoritative inserts must restore old stacks even on expired pages.
        auto writable = [&](int cell) {
            return other || iid != 0 || compact_register_enabled[cell / kSlotsPerPage];
        };
        if (stackable) {
            // Server instance IDs must stay distinct for consumption and binding updates.
            for (int i = base; i < end; ++i) {
                auto& slot = (*destination)[i];
                if (writable(i) && slot.index == index && slot.prefix == prefix && slot.count > 0 &&
                    ((iid == 0 && !slot.character_bound && !slot.account_bound) ||
                     slot.iid == iid)) {
                    slot.count += count;
                    if (slot.iid == 0) {
                        slot.iid = iid;
                    }
                    compact_slots();
                    return true;
                }
            }
        }
        const int required = stackable ? 1 : count;
        int free = 0;
        for (int i = base; i < end; ++i) {
            if (writable(i) && (*destination)[i].count <= 0) {
                ++free;
            }
        }
        // Preserve legacy overflow sent by the server in visible General cells.
        if (free < required && other && iid != 0) {
            destination = &normal;
            destination->resize(std::max(destination->size(), static_cast<std::size_t>(kBagSlots)));
            base = 0;
            end = kBagSlots;
            free = static_cast<int>(
                std::count_if(destination->begin(), destination->end(), [](const ItemStack& item) {
                    return item.count <= 0;
                }));
        }
        if (free < required) {
            return false;
        }
        int remaining = count;
        bool used_iid = false;
        for (int i = base; i < end && remaining > 0; ++i) {
            auto& slot = (*destination)[i];
            if (slot.count > 0 || !writable(i)) {
                continue;
            }
            slot = ItemStack{index, stackable ? remaining : 1, false, prefix, used_iid ? 0u : iid};
            used_iid = true;
            apply_stack_endurance(slot, def, cur_end, max_end);
            remaining -= slot.count;
        }
        compact_slots();
        return remaining == 0;
    }

    const ItemStack* Bag::find_instance(std::uint32_t iid) const {
        if (iid == 0) {
            return nullptr;
        }
        for (const auto* slots : {&normal, &quest, &equipment, &collection}) {
            for (const auto& item : *slots) {
                if (item.iid == iid && item.count > 0) {
                    return &item;
                }
            }
        }
        return nullptr;
    }

    ItemStack* Bag::item_at(int slot) {
        if (slot < 0) {
            return nullptr;
        }
        auto& slots = slot < kBagSlots ? normal : equipment;
        const int cell = slot < kBagSlots ? slot : slot - kBagSlots;
        return cell < static_cast<int>(slots.size()) ? &slots[cell] : nullptr;
    }

    const ItemStack* Bag::item_at(int slot) const {
        if (slot < 0) {
            return nullptr;
        }
        const auto& slots = slot < kBagSlots ? normal : equipment;
        const int cell = slot < kBagSlots ? slot : slot - kBagSlots;
        return cell < static_cast<int>(slots.size()) ? &slots[cell] : nullptr;
    }

    bool Bag::set_equipment_equipped(std::uint32_t iid, bool equipped, const ItemDb& catalog) {
        if (iid == 0) {
            return false;
        }
        equipment.resize(kEquipmentSlots);
        normal.resize(kBagSlots);
        auto& source = equipped ? normal : equipment;
        auto& destination = equipped ? equipment : normal;
        for (const auto& item : destination) {
            if (item.iid == iid && item.count > 0) {
                return item.equipped == equipped;
            }
        }
        for (auto& item : source) {
            if (item.iid != iid || item.count <= 0) {
                continue;
            }
            const auto* def = catalog.find(item.index);
            if (!def || server::collection_def(item.index).kind != server::CollectionKind::None) {
                return false;
            }
            if (equipped) {
                const int cell = equipment_cell(*def);
                if (cell < 0) {
                    return false;
                }
                auto previous = destination[cell];
                ItemStack* returned = &item;
                const auto source_cell = static_cast<std::size_t>(&item - normal.data());
                if (previous.count > 0 && !compact_register_enabled[source_cell / kSlotsPerPage]) {
                    returned = nullptr;
                    for (int i = 0; i < kBagSlots; ++i) {
                        if (compact_register_enabled[i / kSlotsPerPage] && normal[i].count <= 0) {
                            returned = &normal[i];
                            break;
                        }
                    }
                    if (!returned) {
                        return false;
                    }
                }
                destination[cell] = item;
                destination[cell].equipped = true;
                previous.equipped = false;
                item = {};
                *returned = previous;
                compact_slots();
                return true;
            }
            for (int i = 0; i < kBagSlots; ++i) {
                if (!compact_register_enabled[i / kSlotsPerPage] || destination[i].count > 0) {
                    continue;
                }
                destination[i] = item;
                destination[i].equipped = false;
                item = {};
                compact_slots();
                return true;
            }
            return false;
        }
        return false;
    }

    void Bag::sort_other(const ItemDb* catalog) {
        quest.resize(std::max(quest.size(), static_cast<std::size_t>(kOtherSlots)));
        std::stable_sort(quest.begin(),
                         quest.begin() + kOtherSectionSlots,
                         [catalog](const ItemStack& a, const ItemStack& b) {
                             if ((a.count > 0) != (b.count > 0)) {
                                 return a.count > 0;
                             }
                             const auto* ad = catalog ? catalog->find(a.index) : nullptr;
                             const auto* bd = catalog ? catalog->find(b.index) : nullptr;
                             const std::string an = ad ? ad->name() : std::string{};
                             const std::string bn = bd ? bd->name() : std::string{};
                             if (an != bn) {
                                 return an < bn;
                             }
                             if (a.index != b.index) {
                                 return a.index < b.index;
                             }
                             return a.count > b.count;
                         });
    }

    bool Bag::set_collection_equipped(std::uint32_t iid, bool equipped) {
        if (iid == 0) {
            return false;
        }
        auto& source = equipped ? normal : collection;
        auto& destination = equipped ? collection : normal;
        for (const auto& item : destination) {
            if (item.iid == iid && item.count > 0) {
                return item.equipped == equipped;
            }
        }
        for (auto& item : source) {
            if (item.iid != iid || item.count <= 0 ||
                server::collection_def(item.index).kind == server::CollectionKind::None) {
                continue;
            }
            destination.resize(equipped ? server::kCollectionSlots : kBagSlots);
            for (std::size_t i = 0; i < destination.size(); ++i) {
                if ((!equipped && !compact_register_enabled[i / kSlotsPerPage]) ||
                    destination[i].count > 0) {
                    continue;
                }
                destination[i] = item;
                destination[i].equipped = equipped;
                item = {};
                compact_slots();
                return true;
            }
            return false;
        }
        return false;
    }

    bool Bag::try_update_instance(std::uint32_t iid,
                                  std::int32_t index,
                                  int prefix,
                                  int count,
                                  bool set_count,
                                  bool& prefix_changed,
                                  int cur_end,
                                  int max_end) {
        prefix_changed = false;
        if (iid == 0) {
            return false;
        }
        auto apply = [&](std::vector<ItemStack>& slots) -> bool {
            for (auto& slot : slots) {
                if (slot.iid != iid) {
                    continue;
                }
                prefix_changed = slot.prefix != prefix;
                slot.index = index;
                slot.prefix = prefix;
                if (set_count) {
                    slot.count = count;
                } else {
                    slot.count += count;
                }
                if (cur_end >= 0) {
                    slot.cur_end = cur_end;
                }
                if (max_end >= 0) {
                    slot.max_end = max_end;
                }
                if (slot.count <= 0) {
                    slot = {};
                    compact_slots();
                }
                return true;
            }
            return false;
        };
        return apply(normal) || apply(quest) || apply(equipment) || apply(collection);
    }

    bool Bag::try_set_transform_progress(std::uint32_t iid, int level, int exp) {
        if (iid == 0) {
            return false;
        }
        const int lv = level < 1 ? 1 : level;
        const int xp = exp < 0 ? 0 : exp;
        auto apply = [&](std::vector<ItemStack>& slots) -> bool {
            for (auto& slot : slots) {
                if (slot.iid != iid) {
                    continue;
                }
                slot.transform_level = lv;
                slot.transform_exp = xp;
                return true;
            }
            return false;
        };
        return apply(normal) || apply(quest) || apply(equipment);
    }

    bool Bag::try_remove_iid(std::uint32_t iid) {
        if (iid == 0) {
            return false;
        }
        auto strip = [this, iid](std::vector<ItemStack>& slots) {
            for (auto& slot : slots) {
                if (slot.iid != iid) {
                    continue;
                }
                slot = {};
                compact_slots();
                return true;
            }
            return false;
        };
        return strip(normal) || strip(quest) || strip(equipment) || strip(collection);
    }

    bool Bag::try_remove(std::int32_t index, int count) {
        if (index <= 0 || count <= 0) {
            return true;
        }
        auto remove_from = [&](std::vector<ItemStack>& slots) -> int {
            int remaining = count;
            for (auto& slot : slots) {
                if (remaining <= 0) {
                    break;
                }
                if (slot.index != index || slot.count <= 0) {
                    continue;
                }
                const int take = std::min(slot.count, remaining);
                slot.count -= take;
                remaining -= take;
                if (slot.count <= 0) {
                    slot = {};
                }
            }
            return remaining;
        };
        int left = remove_from(normal);
        if (left > 0) {
            left = remove_from(quest);
        }
        if (left == 0) {
            compact_slots();
        }
        return left == 0;
    }

    ConsumeResult Bag::try_consume_one(std::int32_t index, const ItemDb& catalog) {
        ConsumeResult out;
        if (index <= 0) {
            return out;
        }
        const ItemDef* def = catalog.find(index);
        if (!def || def->is_task() || !def->is_consumable() || def->auto_hp()) {
            return out;
        }
        auto take_one = [index](std::vector<ItemStack>& slots) -> bool {
            for (auto& slot : slots) {
                if (slot.index != index || slot.count <= 0 || slot.equipped) {
                    continue;
                }
                --slot.count;
                if (slot.count <= 0) {
                    slot = {};
                }
                return true;
            }
            return false;
        };
        if (!take_one(normal) && !take_one(quest)) {
            return out;
        }
        compact_slots();
        out.ok = true;
        out.teleport_loc = def->teleport_loc();
        out.refresh_hp = def->refresh_hp();
        out.refresh_mp = def->refresh_mp();
        return out;
    }

    void Window::complete_sort(Bag& bag, bool quest) {
        if (quest) {
            bag.sort_other(m_catalog);
        } else {
            bag.sort_items(m_catalog, m_register_enabled.data());
        }
        m_last_click_slot = -1;
        m_last_wear_click_slot = -1;
        m_hover_slot = -1;
        m_register_page = 0;
        set_scroll_row(0);
        refresh_general_projection(bag);
    }

    void Bag::sort_items(const ItemDb* catalog, const bool register_enabled[kMaxRegisters]) {
        normal.resize(std::max(normal.size(), static_cast<std::size_t>(kBagSlots)));
        struct NamedStack {
            ItemStack stack;
            std::string name;
        };
        std::vector<NamedStack> items;
        for (int i = 0; i < kBagSlots; ++i) {
            const auto& item = normal[static_cast<std::size_t>(i)];
            if (register_enabled[i / kSlotsPerPage] && item.index != 0 && item.count > 0) {
                const auto* def = catalog ? catalog->find(item.index) : nullptr;
                if (def && def->is_task()) {
                    continue;
                }
                items.push_back({item, def ? def->name() : std::string{}});
            }
        }
        std::stable_sort(items.begin(), items.end(), [](const NamedStack& a, const NamedStack& b) {
            if (a.name != b.name) {
                return a.name < b.name;
            }
            if (a.stack.index != b.stack.index) {
                return a.stack.index < b.stack.index;
            }
            return a.stack.count > b.stack.count;
        });
        std::size_t next = 0;
        for (int i = 0; i < kBagSlots; ++i) {
            if (register_enabled[i / kSlotsPerPage]) {
                const auto& existing = normal[static_cast<std::size_t>(i)];
                const auto* def = catalog ? catalog->find(existing.index) : nullptr;
                if (existing.count > 0 && def && def->is_task()) {
                    continue;
                }
                normal[static_cast<std::size_t>(i)] =
                    next < items.size() ? items[next++].stack : ItemStack{};
            }
        }
    }

    void Bag::sort_compact(const bool register_enabled[kMaxRegisters]) {
        auto pack_range = [](std::vector<ItemStack>& slots, int base, int count) {
            if (base < 0 || count <= 0 || base >= static_cast<int>(slots.size())) {
                return;
            }
            const int end = std::min(base + count, static_cast<int>(slots.size()));
            std::vector<ItemStack> kept;
            kept.reserve(static_cast<std::size_t>(end - base));
            for (int i = base; i < end; ++i) {
                const auto& s = slots[static_cast<std::size_t>(i)];
                if (s.index != 0 && s.count > 0) {
                    kept.push_back(s);
                }
            }
            for (int i = base; i < end; ++i) {
                slots[static_cast<std::size_t>(i)] = {};
            }
            for (std::size_t i = 0; i < kept.size(); ++i) {
                slots[static_cast<std::size_t>(base) + i] = kept[i];
            }
        };

        auto compact_bag = [&](std::vector<ItemStack>& slots) {
            if (static_cast<int>(slots.size()) < kBagSlots) {
                slots.resize(static_cast<std::size_t>(kBagSlots));
            }
            // Enabled pages: one contiguous pack across writable registers (page
            // order).
            std::vector<ItemStack> enabled_kept;
            enabled_kept.reserve(static_cast<std::size_t>(kBagSlots));
            for (int page = 0; page < kMaxRegisters; ++page) {
                if (!register_enabled[page]) {
                    continue;
                }
                const int base = page * kSlotsPerPage;
                for (int i = 0; i < kSlotsPerPage; ++i) {
                    const auto& s = slots[static_cast<std::size_t>(base + i)];
                    if (s.index != 0 && s.count > 0) {
                        enabled_kept.push_back(s);
                    }
                }
            }
            for (int page = 0; page < kMaxRegisters; ++page) {
                if (!register_enabled[page]) {
                    continue;
                }
                const int base = page * kSlotsPerPage;
                for (int i = 0; i < kSlotsPerPage; ++i) {
                    slots[static_cast<std::size_t>(base + i)] = {};
                }
            }
            int write_page = 0;
            int write_i = 0;
            for (const auto& s : enabled_kept) {
                while (write_page < kMaxRegisters && !register_enabled[write_page]) {
                    ++write_page;
                    write_i = 0;
                }
                if (write_page >= kMaxRegisters) {
                    break;
                }
                slots[static_cast<std::size_t>(write_page * kSlotsPerPage + write_i)] = s;
                ++write_i;
                if (write_i >= kSlotsPerPage) {
                    write_i = 0;
                    ++write_page;
                }
            }
            // Disabled pages: pack in place so overflow stays on that register.
            for (int page = 0; page < kMaxRegisters; ++page) {
                if (register_enabled[page]) {
                    continue;
                }
                pack_range(slots, page * kSlotsPerPage, kSlotsPerPage);
            }
        };

        compact_bag(normal);
        pack_range(quest, 0, kOtherSectionSlots);
    }

    EquipToggleResult Bag::toggle_equip_slot(
        Bag& bag, int slot, Wear& wear, const ItemDb& catalog, int clazz, int level) {
        EquipToggleResult r;
        const auto* item = bag.item_at(slot);
        if (!item || item->index <= 0 || item->count <= 0) {
            return r;
        }
        const auto& stack = *item;
        const auto* def = catalog.find(stack.index);
        if (!def || !def->can_equip()) {
            return r;
        }
        if (server::collection_def(stack.index).kind != server::CollectionKind::None) {
            if (!def->can_equip_by(clazz, level)) {
                return r;
            }
            const auto item = server::CollectionItem{stack.iid,
                                                     stack.index,
                                                     stack.upgrade_level,
                                                     stack.random_stat_type,
                                                     stack.random_stat_value,
                                                     stack.mix_type,
                                                     stack.equipped};
            if (!server::collection_wear_error(collection_items(bag), item, !stack.equipped)
                     .empty()) {
                return r;
            }
            // The server acknowledges the change; do not optimistically alter instance state.
            return {true,
                    stack.equipped ? EquipNetOp::PutOff : EquipNetOp::PutOn,
                    stack.index,
                    stack.iid};
        }
        if (!stack.equipped) {
            if (!def->can_equip_by(clazz, level)) {
                return r;
            }
            const auto* actual_weapon = catalog.find(wear.weapon_index());
            if (def->skin_slot() == protocol::SkinSlot::Weapon &&
                (!actual_weapon || !def->skin_matches_weapon(*actual_weapon))) {
                return r;
            }
            const auto* weapon_skin = catalog.find(wear.skin_index(protocol::SkinSlot::Weapon));
            if (def->is_weapon() && weapon_skin && !weapon_skin->skin_matches_weapon(*def)) {
                return r;
            }
            if (def->is_weapon() && def->class_small() == "sword2h" && wear.shield_index() != 0) {
                return r;
            }
            if (def->class_small() == "shield" && wear.weapon_index() != 0) {
                const auto* weapon = catalog.find(wear.weapon_index());
                if (weapon && weapon->class_small() == "sword2h") {
                    return r;
                }
            }
        }
        return {
            true, stack.equipped ? EquipNetOp::PutOff : EquipNetOp::PutOn, stack.index, stack.iid};
    }

    void Window::apply_register_enabled_from_layout() {
        m_register_enabled = {true, true, false, false, false, false};
        if (m_layout) {
            const auto* arr = m_layout->root().get("registers");
            const auto* en = arr ? arr->get("enabled") : nullptr;
            if (en && en->is_array()) {
                for (int i = 0; i < kMaxRegisters && i < static_cast<int>(en->array().size());
                     ++i) {
                    const auto& node = en->array()[static_cast<std::size_t>(i)];
                    if (node.type() == platform::Json::Type::Bool) {
                        m_register_enabled[static_cast<std::size_t>(i)] = node.boolean();
                    } else if (node.is_number()) {
                        m_register_enabled[static_cast<std::size_t>(i)] = node.as_int(0) != 0;
                    }
                }
            }
        }
        // First two registers stay available so base capacity remains 72.
        m_register_enabled[0] = true;
        m_register_enabled[1] = true;
        const std::uint64_t now_ms = steady_ms_now();
        for (int i = 2; i < kMaxRegisters; ++i) {
            m_register_enabled[static_cast<std::size_t>(i)] =
                m_register_unlock_expire_ms[static_cast<std::size_t>(i)] > now_ms;
        }
    }

    void Window::set_register_unlock_expire_ms(
        const std::array<std::uint64_t, kMaxRegisters>& expire_ms) {
        m_register_unlock_expire_ms = expire_ms;
        apply_register_enabled_from_layout();
    }

    void Window::sync_from_layout() {
        UiView::reload(m_v1, m_layout);
        if (m_layout && !m_layout->path().empty()) {
            const auto slash = m_layout->path().find_last_of("/\\");
            const std::string path =
                m_layout->path().substr(0, slash + 1) + "inventory_collection.json";
            if (m_collection_layout.path() != path) {
                (void) m_collection_layout.load_file(path);
            } else {
                (void) m_collection_layout.reload_if_stale();
            }
            UiView::reload(m_collection_document, &m_collection_layout);
        }
        m_scale = std::max(0.25f, m_requested_scale);
        if (m_screen_w > 0) {
            m_scale = std::min(m_scale,
                               m_screen_w / (m_v1.window_w() + m_v1.pad_left() + m_v1.pad_right()));
        }
        if (m_screen_h > 0) {
            m_scale = std::min(m_scale,
                               m_screen_h / (m_v1.window_h() + m_v1.pad_top() + m_v1.pad_bottom()));
        }
        m_w = m_v1.window_w() * m_scale;
        m_h = m_v1.window_h() * m_scale;
        apply_register_enabled_from_layout();
        set_scroll_row(scroll_row());
        if (m_register_page < 0 || m_register_page >= kMaxRegisters) {
            m_register_page = 0;
        }
    }

    void Window::sync_fonts(engine::IEngine& engine) {
        UiView::reload(m_v1, m_layout);
        int body_px = m_v1.font_pixel_size();
        if (body_px < 8) {
            body_px = kBodyPixelSize;
        }
        body_px = std::clamp(body_px, 8, 48);
        if (!m_body_font.ready() || m_body_font_pixel_size != body_px) {
            const std::string path = FreetypeFont::find_system_sans();
            m_body_font.destroy(engine);
            m_body_font_pixel_size = 0;
            if (!path.empty() && m_body_font.load(engine, path, body_px)) {
                m_body_font_pixel_size = body_px;
            }
        }
        int title_px = 18;
        if (const UiWidget* title = m_v1.find_widget("title")) {
            if (title->pixel_size() > 0) {
                title_px = title->pixel_size();
            }
        }
        title_px = std::clamp(title_px, 8, 64);
        if (!m_title_font.ready() || m_title_font_pixel_size != title_px) {
            m_title_font.destroy(engine);
            m_title_font_pixel_size = 0;
            const std::string path = FreetypeFont::find_system_sans_bold();
            if (!path.empty() && m_title_font.load(engine, path, title_px)) {
                m_title_font_pixel_size = title_px;
            }
        }
    }

    void Window::destroy_fonts(engine::IEngine& engine) {
        m_body_font.destroy(engine);
        m_body_font_pixel_size = 0;
        m_title_font.destroy(engine);
        m_title_font_pixel_size = 0;
    }

    void Window::layout_size(float screen_w, float screen_h) {
        m_screen_w = screen_w;
        m_screen_h = screen_h;
        sync_from_layout();
        if (m_collection_open && m_screen_w > 0) {
            const float left =
                (m_collection_document.window_w() + 80) * collection_scale() + 48 * m_scale;
            m_x = std::clamp(m_x, left, std::max(left, m_screen_w - m_w - 40 * m_scale));
        }
        if (m_screen_w > 0 && m_screen_h > 0) {
            const float left = m_v1.pad_left() * m_scale;
            const float top = m_v1.pad_top() * m_scale;
            m_x = std::clamp(
                m_x, left, std::max(left, m_screen_w - m_w - m_v1.pad_right() * m_scale));
            m_y =
                std::clamp(m_y, top, std::max(top, m_screen_h - m_h - m_v1.pad_bottom() * m_scale));
        }
    }

    void Window::center_on_screen(float screen_w, float screen_h) {
        m_screen_w = screen_w;
        m_screen_h = screen_h;
        sync_from_layout();
        const float s = std::max(0.25f, m_scale);
        WidgetTools::center_rect(m_x,
                                 m_y,
                                 m_w + (m_v1.pad_left() + m_v1.pad_right()) * s,
                                 m_h + (m_v1.pad_top() + m_v1.pad_bottom()) * s,
                                 screen_w,
                                 screen_h);
        m_x += m_v1.pad_left() * s;
        m_y += m_v1.pad_top() * s;
    }

    WindowFrameStyle Window::frame_style() const {
        return UiView::frame_style(m_v1);
    }

    void Window::draw_chrome(UiAssets& ui) const {
        const float s = std::max(0.25f, m_scale);
        UiView::draw_chrome(ui, m_v1, m_x, m_y, m_w, m_h, s, true);
    }

    bool Window::wear_column_rect(
        int left, int index, float& sx, float& sy, float& sw, float& sh) const {
        static constexpr const char* kLeft[] = {"wear.helmet",
                                                "wear.upper",
                                                "wear.gauntlet",
                                                "wear.pants",
                                                "wear.boots",
                                                "wear.weapon",
                                                "wear.shield"};
        static constexpr const char* kRight[] = {"wear.ring",
                                                 "wear.necklace",
                                                 "wear.accessory",
                                                 "wear.face",
                                                 "wear.sinsu",
                                                 "wear.pet",
                                                 "wear.ride"};
        if (index < 0 || index >= 7) {
            return false;
        }
        if (left == 3 && index != 0) {
            return false;
        }
        const std::string id = left >= 2 ? "wear." + wear_layout_slot_id(nullptr, left, index)
                                         : (left ? kLeft[index] : kRight[index]);
        const UiWidget* w = m_v1.find_widget(id);
        if (!w) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        const float slot = (w->frame_w() > 0.0f ? w->frame_w() : 42.0f) * s;
        sx = m_x + w->x() * s + (w->w() * s - slot) * 0.5f;
        sy = m_y + w->y() * s + (w->h() * s - slot) * 0.5f;
        sw = slot;
        sh = slot;
        return true;
    }

    bool Window::wear_columns_hit(float mx, float my) const {
        for (const int left : {1, 0, 2, 3}) {
            for (int i = 0; i < 8; ++i) {
                float sx, sy, sw, sh;
                if (!wear_column_rect(left, i, sx, sy, sw, sh)) {
                    break;
                }
                if (UiAssets::point_in_rect(mx, my, sx, sy, sw, sh)) {
                    return true;
                }
            }
        }
        return false;
    }

    int Window::slots_capacity() const {
        int pages = 0;
        for (bool on : m_register_enabled) {
            if (on) {
                ++pages;
            }
        }
        return pages * kSlotsPerPage;
    }

    int Window::slots_used(const Bag& bag) const {
        const auto& slots = active_slots(bag);
        int used = 0;
        for (const auto& it : slots) {
            if (it.index != 0 && it.count > 0) {
                ++used;
            }
        }
        return used;
    }

    bool Window::register_writable(int page) const {
        return page >= 0 && page < kMaxRegisters &&
               m_register_enabled[static_cast<std::size_t>(page)];
    }

    bool Window::register_page_has_items(const Bag& bag, int page) const {
        if (page < 0 || page >= kMaxRegisters) {
            return false;
        }
        const auto& slots = active_slots(bag);
        const int base = page * kSlotsPerPage;
        for (int i = 0; i < kSlotsPerPage; ++i) {
            const int abs_i = base + i;
            if (abs_i >= static_cast<int>(slots.size())) {
                break;
            }
            const auto& it = slots[static_cast<std::size_t>(abs_i)];
            if (it.index != 0 && it.count > 0) {
                return true;
            }
        }
        return false;
    }

    bool Window::register_viewable(const Bag& bag, int page) const {
        return register_writable(page) || register_page_has_items(bag, page);
    }

    bool Window::filter_active(int category) const {
        return category >= 0 && category < 6 && (m_filter_mask & (1u << category)) != 0;
    }

    int Window::general_source_slot(int display_slot) const {
        if (display_slot < 0 || display_slot >= kBagSlots) {
            return -1;
        }
        return m_filter_mask == 0 ? display_slot : m_general_projection[display_slot];
    }

    void Window::refresh_general_projection(const Bag& bag) const {
        if (m_filter_mask == 0) {
            return;
        }
        m_general_projection.fill(-1);
        // Enabled pages form one domain. Each expired page stays isolated so its
        // take-out-only cells never acquire the appearance of writable storage.
        auto pack = [&](const int* cells, std::size_t count) {
            std::size_t display = 0;
            for (std::size_t i = 0; i < count; ++i) {
                const int source = cells[i];
                if (source >= static_cast<int>(bag.normal.size())) {
                    continue;
                }
                const auto& item = bag.normal[source];
                if (item.index != 0 && item.count > 0 &&
                    filter_active(
                        item_filter_category(m_catalog ? m_catalog->find(item.index) : nullptr))) {
                    m_general_projection[cells[display++]] = source;
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                const int source = cells[i];
                if (source < static_cast<int>(bag.normal.size()) &&
                    (bag.normal[source].index == 0 || bag.normal[source].count <= 0)) {
                    m_general_projection[cells[display++]] = source;
                }
            }
        };
        std::array<int, kBagSlots> enabled;
        std::size_t enabled_count = 0;
        std::array<int, kSlotsPerPage> cells;
        for (int page = 0; page < kMaxRegisters; ++page) {
            const bool writable = register_writable(page);
            for (int cell = 0; cell < kSlotsPerPage; ++cell) {
                const int source = page * kSlotsPerPage + cell;
                if (writable) {
                    enabled[enabled_count++] = source;
                } else {
                    cells[cell] = source;
                }
            }
            if (!writable) {
                pack(cells.data(), cells.size());
            }
        }
        pack(enabled.data(), enabled_count);
    }

    const UiWidget* Window::active_viewport() const {
        return m_v1.find_widget(m_tab == Tab::Normal ? "general_viewport" : "bag_viewport");
    }

    int Window::scroll_row() const {
        return m_scroll_rows[m_tab == Tab::Quest ? 1 : 0];
    }

    int Window::max_scroll_row() const {
        const auto* viewport = active_viewport();
        const auto* last = m_v1.find_widget(m_tab == Tab::Quest ? "task" : "bag.5");
        if (!viewport || !last) {
            return 0;
        }
        const float bottom = last->y() + (last->rows() - 1) * last->pitch_y() +
                             std::max(last->cell_h(), last->frame_h()) - 5;
        return std::max(0,
                        static_cast<int>(std::ceil((bottom - viewport->y() - viewport->h()) / 46)));
    }

    void Window::set_scroll_row(int row) {
        auto& current = m_scroll_rows[m_tab == Tab::Quest ? 1 : 0];
        const int target = std::clamp(row, 0, max_scroll_row());
        if (current == target) {
            return;
        }
        current = target;
        m_hover_slot = -1;
        m_last_click_slot = -1;
        m_arm_equip = false;
    }

    bool Window::scroll_area_hit(float mx, float my) const {
        const auto* viewport = active_viewport();
        return viewport && UiAssets::point_in_rect(mx,
                                                   my,
                                                   m_x + viewport->x() * m_scale,
                                                   m_y + viewport->y() * m_scale,
                                                   (viewport->w() + 24) * m_scale,
                                                   viewport->h() * m_scale);
    }

    void Window::scroll_to_pointer(float my) {
        const auto* track = m_v1.find_widget("scroll_track");
        if (!track) {
            return;
        }
        const float ratio = (my - m_y - track->y() * m_scale) / (track->h() * m_scale);
        set_scroll_row(
            static_cast<int>(std::round(std::clamp(ratio, 0.0f, 1.0f) * max_scroll_row())));
    }

    bool Window::grid_slot_rect(int index, float& sx, float& sy, float& sw, float& sh) const {
        if (index < 0 || index >= (m_tab == Tab::Normal ? kBagSlots : kOtherSlots)) {
            return false;
        }
        if (m_tab == Tab::Normal && m_filter_mask != 0) {
            const auto cell =
                std::find(m_general_projection.begin(), m_general_projection.end(), index);
            if (cell == m_general_projection.end()) {
                return false;
            }
            index = static_cast<int>(cell - m_general_projection.begin());
        }
        const int block = index / kSlotsPerPage;
        const std::string id = m_tab == Tab::Normal ? "bag." + std::to_string(block)
                               : block == 0         ? "quest"
                                                    : "task";
        const auto* grid = m_v1.find_widget(id);
        if (!grid) {
            return false;
        }
        const auto display = ui_scrolled_widget(
            *grid, m_v1, {{"inventory.scroll", std::to_string(scroll_row() * 46)}});
        const int cell = index % kSlotsPerPage - display.first_slot();
        const int cols = std::max(1, display.cols());
        if (!display.visible() || cell < 0 || cell >= display.rows() * cols) {
            return false;
        }
        sx = m_x + (display.x() + (cell % cols) * display.pitch_x()) * m_scale;
        sy = m_y + (display.y() + (cell / cols) * display.pitch_y()) * m_scale;
        sw = display.cell_w() * m_scale;
        sh = display.cell_h() * m_scale;
        return true;
    }

    int Window::grid_slot_at(float mx, float my) const {
        const UiVarMap vars{{"inventory.scroll", std::to_string(scroll_row() * 46)},
                            {"inventory.normal_active", m_tab == Tab::Normal ? "true" : "false"},
                            {"inventory.quest_active", m_tab == Tab::Quest ? "true" : "false"}};
        const auto hit = UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w, m_h, m_scale, vars);
        if (hit.kind() != UiHitKind::IconGridCell) {
            return -1;
        }
        if (hit.bind() == "inventory.quest") {
            return hit.index();
        }
        if (hit.bind() == "inventory.task") {
            return kOtherSectionSlots + hit.index();
        }
        constexpr std::string_view prefix = "inventory.general.";
        if (hit.bind().substr(0, prefix.size()) == prefix) {
            return general_source_slot(
                std::atoi(hit.bind().substr(prefix.size()).c_str()) * kSlotsPerPage + hit.index());
        }
        return -1;
    }

    bool Window::destroy_rect(float& sx, float& sy, float& sw, float& sh) const {
        const UiWidget* w = m_v1.find_widget("destroy");
        if (!w) {
            return false;
        }
        UiView::widget_rect(*w, nullptr, m_x, m_y, m_w, m_h, m_scale, sx, sy, sw, sh);
        return true;
    }

    bool Window::exit_bottom_rect(float& sx, float& sy, float& sw, float& sh) const {
        const UiWidget* w = m_v1.find_widget("close");
        if (!w) {
            return false;
        }
        UiView::widget_rect(*w, nullptr, m_x, m_y, m_w, m_h, m_scale, sx, sy, sw, sh);
        return true;
    }

    float Window::collection_progress() const {
        const float elapsed = std::min(
            1.0f, static_cast<float>(steady_ms_now() - m_collection_animation_ms) / 180.0f);
        return m_collection_animation_from +
               ((m_collection_open ? 1.0f : 0.0f) - m_collection_animation_from) * elapsed;
    }

    float Window::collection_x() const {
        return m_x -
               ((m_collection_document.window_w() + 40.0f) * collection_scale() + 48.0f * m_scale) *
                   collection_progress();
    }

    float Window::collection_scale() const {
        float scale = m_scale;
        if (m_screen_w > 0) {
            scale = std::min(
                scale, (m_screen_w - m_w - 88 * m_scale) / (m_collection_document.window_w() + 80));
        }
        if (m_screen_h > 0) {
            scale = std::min(scale, m_screen_h / (m_collection_document.window_h() + 140));
        }
        return std::max(0.25f, scale);
    }

    float Window::collection_y() const {
        const float scale = collection_scale();
        const float top = 84 * scale;
        const float bottom =
            m_screen_h > 0
                ? std::max(top, m_screen_h - (m_collection_document.window_h() + 56) * scale)
                : m_y;
        return std::clamp(m_y, top, std::max(top, bottom));
    }

    int Window::collection_slot_at(float mx, float my) const {
        if (!m_open || !m_collection_open || collection_progress() < 0.99f) {
            return -1;
        }
        for (int slot = 0; slot < server::kCollectionSlots; ++slot) {
            const auto* widget =
                m_collection_document.find_widget("collection." + std::to_string(slot));
            if (!widget) {
                continue;
            }
            const float x = collection_x() + (widget->x() - 5) * collection_scale();
            const float y = collection_y() + (widget->y() - 5) * collection_scale();
            if (UiAssets::point_in_rect(
                    mx, my, x, y, 42 * collection_scale(), 42 * collection_scale())) {
                return slot;
            }
        }
        return -1;
    }

    bool Window::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        if (const auto* button = m_v1.find_widget("frame_handle")) {
            if (UiAssets::point_in_rect(mx,
                                        my,
                                        m_x + button->x() * s,
                                        m_y + button->y() * s,
                                        button->w() * s,
                                        button->h() * s)) {
                return true;
            }
        }
        if (m_collection_open &&
            UiAssets::point_in_rect(mx,
                                    my,
                                    collection_x() - 40 * collection_scale(),
                                    collection_y() - 84 * collection_scale(),
                                    (m_collection_document.window_w() + 80) * collection_scale(),
                                    (m_collection_document.window_h() + 140) *
                                        collection_scale())) {
            return true;
        }
        if (WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w, m_h, s, frame_style()) ||
            UiAssets::point_in_rect(mx, my, m_x, m_y, m_w, m_h) || wear_columns_hit(mx, my)) {
            return true;
        }
        float dx, dy, dw, dh;
        if (!destroy_rect(dx, dy, dw, dh)) {
            return false;
        }
        return UiAssets::point_in_rect(mx, my, dx, dy, dw, dh);
    }

    void Window::update_pointer(const engine::FrameInput& in, Bag& bag) {
        refresh_general_projection(bag);
        m_hover_slot = -1;
        m_collection_hover = false;
        m_hover_register = -1;
        m_hover_mx = in.mouse_x;
        m_hover_my = in.mouse_y;
        m_hovered_button.clear();
        m_pointer_down = in.mouse_left;
        if (!m_open) {
            m_dragging_window = false;
            m_press_slot = -1;
            m_drag_slot = -1;
            m_arm_equip = false;
            return;
        }
        if (m_scrolling) {
            if (!in.mouse_left) {
                m_scrolling = false;
            } else {
                scroll_to_pointer(in.mouse_y);
            }
            return;
        }
        if (in.mouse_scroll != 0 && scroll_area_hit(in.mouse_x, in.mouse_y)) {
            set_scroll_row(scroll_row() -
                           static_cast<int>(std::copysign(std::max(1.0f, std::abs(in.mouse_scroll)),
                                                          in.mouse_scroll)));
        }
        if (m_dragging_window) {
            if (!in.mouse_left) {
                m_dragging_window = false;
            } else {
                m_x = in.mouse_x - m_drag_dx;
                m_y = in.mouse_y - m_drag_dy;
            }
        }
        if (m_press_slot >= 0 && in.mouse_left && m_drag_slot < 0) {
            const float dx = in.mouse_x - m_press_mx;
            const float dy = in.mouse_y - m_press_my;
            constexpr float kDragThreshold = 6.0f;
            if (dx * dx + dy * dy >= kDragThreshold * kDragThreshold) {
                m_drag_slot = m_press_slot;
                m_arm_equip = false;
            }
        }

        if (!m_dragging_window) {
            const auto hit = UiView::hit(
                m_v1, nullptr, in.mouse_x, in.mouse_y, m_x, m_y, m_w, m_h, m_scale, make_vars(bag));
            if (hit.kind() == UiHitKind::Button) {
                m_hovered_button = hit.widget_id();
            }
            if (m_collection_open) {
                const auto collection_hit =
                    UiView::hit(m_collection_document,
                                nullptr,
                                in.mouse_x,
                                in.mouse_y,
                                collection_x(),
                                collection_y(),
                                m_collection_document.window_w() * collection_scale(),
                                m_collection_document.window_h() * collection_scale(),
                                collection_scale(),
                                make_vars(bag));
                if (collection_hit.kind() == UiHitKind::Button) {
                    m_hovered_button = collection_hit.widget_id();
                }
            }
        }
        if (m_drag_slot >= 0) {
            const auto tab_hit = UiView::hit(
                m_v1, nullptr, in.mouse_x, in.mouse_y, m_x, m_y, m_w, m_h, m_scale, make_vars(bag));
            if (tab_hit.widget_id() == "frame_handle" && !m_collection_open) {
                const auto now = steady_ms_now();
                if (!m_drag_hover_ms) {
                    m_drag_hover_ms = now;
                } else if (now - m_drag_hover_ms >= 350) {
                    m_collection_animation_from = collection_progress();
                    m_collection_animation_ms = now;
                    m_collection_open = true;
                    layout_size(m_screen_w, m_screen_h);
                }
            } else if (tab_hit.widget_id() == "bag_tabs") {
                const Tab target = tab_hit.tab_id() == "quest" ? Tab::Quest : Tab::Normal;
                const auto now = steady_ms_now();
                if (target != m_drag_hover_tab || m_drag_hover_ms == 0) {
                    m_drag_hover_tab = target;
                    m_drag_hover_ms = now;
                } else if (now - m_drag_hover_ms >= 350) {
                    set_tab(target);
                }
            } else {
                m_drag_hover_ms = 0;
            }
        }
        // Hover bag grid slot under the cursor (for tooltip).
        if (m_drag_slot < 0 && !m_dragging_window) {
            if (m_tab == Tab::Normal && m_hovered_button == "expand") {
                m_hover_register = 2;
            }
            const int collection_slot = collection_slot_at(in.mouse_x, in.mouse_y);
            if (collection_slot >= 0) {
                m_hover_slot = collection_item_slot(bag, collection_slot);
                m_collection_hover = true;
                return;
            }
            const int abs_i = grid_slot_at(in.mouse_x, in.mouse_y);
            if (abs_i >= 0) {
                const auto& slots = active_slots(bag);
                if (abs_i < static_cast<int>(slots.size())) {
                    const auto& it = slots[static_cast<std::size_t>(abs_i)];
                    if (it.index != 0 && it.count > 0) {
                        m_hover_slot = abs_i;
                    }
                }
            }
            if (m_hover_slot < 0 && m_wear) {
                for (const int left : {1, 0, 2, 3}) {
                    for (int i = 0; i < 8; ++i) {
                        float sx = 0;
                        float sy = 0;
                        float sw = 0;
                        float sh = 0;
                        if (!wear_column_rect(left, i, sx, sy, sw, sh)) {
                            break;
                        }
                        if (!UiAssets::point_in_rect(in.mouse_x, in.mouse_y, sx, sy, sw, sh)) {
                            continue;
                        }
                        const std::string id = wear_layout_slot_id(m_layout, left, i);
                        const std::int32_t idx = wear_equipped_index(*m_wear, id);
                        m_hover_slot = bag_slot_for_equipped(bag, idx);
                        return;
                    }
                }
            }
        }
    }

    ClickResult Window::handle_click(float mx, float my, Bag& bag, Wear& wear_out) {
        refresh_general_projection(bag);
        (void) wear_out;
        if (!m_open || !contains(mx, my)) {
            return ClickResult::Ignored;
        }
        const float s = std::max(0.25f, m_scale);
        const auto style = frame_style();
        const auto toggle =
            UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w, m_h, m_scale, make_vars(bag));
        if (toggle.action() == "inventory.collection_toggle" && m_collection_layout.loaded()) {
            m_collection_animation_from = collection_progress();
            m_collection_animation_ms = steady_ms_now();
            m_collection_open = !m_collection_open;
            if (m_collection_open) {
                m_x = std::max(
                    m_x, (m_collection_document.window_w() + 80) * collection_scale() + 48 * s);
                if (m_screen_w > 0) {
                    m_x = std::min(m_x, std::max(m_w + 40 * s, m_screen_w) - m_w - 40 * s);
                }
            }
            return ClickResult::Handled;
        }
        if (m_collection_open && collection_progress() >= .99f) {
            const auto hit = UiView::hit(m_collection_document,
                                         nullptr,
                                         mx,
                                         my,
                                         collection_x(),
                                         collection_y(),
                                         m_collection_document.window_w() * collection_scale(),
                                         m_collection_document.window_h() * collection_scale(),
                                         collection_scale(),
                                         make_vars(bag));
            if (hit.kind() == UiHitKind::TitleDrag) {
                m_dragging_window = true;
                m_drag_dx = mx - m_x;
                m_drag_dy = my - m_y;
                return ClickResult::Handled;
            }
            if (hit.action() == "inventory.collection_absorb") {
                const auto items = collection_items(bag);
                for (const auto& item : items) {
                    if (item.iid == m_collection_selected_iid &&
                        server::collection_def(item.index).kind ==
                            server::CollectionKind::Taegeuk) {
                        m_last_enchant_from_id = item.iid;
                        m_last_enchant_to_id = item.iid;
                        return ClickResult::Enchanted;
                    }
                }
                return ClickResult::Handled;
            }
        }
        const int filter = filter_button_index(toggle.widget_id());
        if (filter >= 0) {
            if (!capturing_pointer()) {
                m_filter_mask ^= 1u << filter;
                m_hover_slot = m_last_click_slot = m_last_wear_click_slot = -1;
                m_arm_equip = false;
                set_scroll_row(0);
                refresh_general_projection(bag);
            }
            return ClickResult::Handled;
        }
        if (toggle.action() == "inventory.expand") {
            if (slots_capacity() >= kBagSlots || capturing_pointer() || !m_catalog) {
                return ClickResult::Handled;
            }
            // Prefer the shortest available writ; the server owns its duration and consumption.
            const auto* selected = inventory_unlock_writ(bag, m_catalog);
            if (!selected) {
                return ClickResult::Handled;
            }
            m_last_consumed_index = selected->index();
            m_last_consumed_iid = 0;
            m_last_teleport_loc = -1;
            m_last_refresh_hp = m_last_refresh_mp = 0;
            return ClickResult::Consumed;
        }
        const int cell = collection_slot_at(mx, my);
        if (cell >= 0 && m_catalog) {
            const int slot = collection_item_slot(bag, cell);
            const auto now = steady_ms_now();
            const bool twice = m_last_wear_click_slot == cell + 32 &&
                               now - m_last_wear_click_ms <= kDoubleClickWindowMs;
            m_last_wear_click_slot = cell + 32;
            m_last_wear_click_ms = now;
            if (twice && slot >= 0) {
                const auto& item = bag.collection[slot];
                const server::CollectionItem equipped{item.iid,
                                                      item.index,
                                                      item.upgrade_level,
                                                      item.random_stat_type,
                                                      item.random_stat_value,
                                                      item.mix_type,
                                                      true};
                if (server::collection_wear_error(collection_items(bag), equipped, false).empty()) {
                    m_last_equip_op = EquipNetOp::PutOff;
                    m_last_equip_index = item.index;
                    m_last_equip_iid = item.iid;
                    return ClickResult::EquipChanged;
                }
            }
            return ClickResult::Handled;
        }
        if (WidgetTools::window_close_hit(mx, my, m_x, m_y, m_w, m_h, s, style)) {
            m_open = false;
            m_drag_slot = -1;
            m_press_slot = -1;
            m_last_click_slot = -1;
            m_last_wear_click_slot = -1;
            m_arm_equip = false;
            return ClickResult::Handled;
        }
        {
            float bx, by, bw, bh;
            if (exit_bottom_rect(bx, by, bw, bh) &&
                UiAssets::point_in_rect(mx, my, bx, by, bw, bh)) {
                m_open = false;
                m_drag_slot = -1;
                m_press_slot = -1;
                m_last_click_slot = -1;
                m_last_wear_click_slot = -1;
                m_arm_equip = false;
                return ClickResult::Handled;
            }
        }

        // Title-bar drag.
        float ox, oy, ow, oh;
        WidgetTools::window_outer_rect(m_x, m_y, m_w, m_h, s, style, ox, oy, ow, oh);
        if (UiAssets::point_in_rect(mx, my, ox, oy, ow, style.title_drag_h() * s)) {
            m_dragging_window = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return ClickResult::Handled;
        }

        const UiWidget* bag_tabs = m_v1.find_widget("bag_tabs");
        const float tab_y = m_y + (bag_tabs ? bag_tabs->y() : 6.0f) * s;
        const float tab_h = (bag_tabs && bag_tabs->h() > 0.0f ? bag_tabs->h() : 27.0f) * s;
        if (bag_tabs) {
            for (const auto& tab : bag_tabs->tabs()) {
                const float nx = m_x + tab.x() * s;
                const float nw = tab.w() * s;
                if (!UiAssets::point_in_rect(mx, my, nx, tab_y, nw, tab_h)) {
                    continue;
                }
                set_tab(tab.id() == "quest" ? Tab::Quest : Tab::Normal);
                m_last_click_slot = -1;
                return ClickResult::Handled;
            }
        }

        const auto control =
            UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w, m_h, m_scale, make_vars(bag));
        if (control.action() == "inventory.scroll.up" ||
            control.action() == "inventory.scroll.down") {
            set_scroll_row(scroll_row() + (control.action() == "inventory.scroll.up" ? -1 : 1));
            return ClickResult::Handled;
        }
        if (control.widget_id() == "scroll_track") {
            m_scrolling = true;
            scroll_to_pointer(my);
            return ClickResult::Handled;
        }
        if (control.action() == "inventory.sort") {
            if (!capturing_pointer()) {
                return ClickResult::SortRequested;
            }
            return ClickResult::Handled;
        }
        if (control.widget_id() == "destroy") {
            return ClickResult::Handled;
        }

        const int hit = grid_slot_at(mx, my);
        if (hit >= 0) {
            auto& slots = active_slots(bag);
            if (hit < static_cast<int>(slots.size())) {
                const auto& it = slots[static_cast<std::size_t>(hit)];
                if (it.index != 0 && it.count > 0) {
                    const auto* definition = m_catalog ? m_catalog->find(it.index) : nullptr;
                    if ((m_tab == Tab::Quest && hit >= kOtherSectionSlots) ||
                        (definition && definition->is_task())) {
                        return ClickResult::Handled;
                    }
                    const std::uint64_t now_ms = steady_ms_now();
                    m_arm_equip = (m_tab == Tab::Normal && hit == m_last_click_slot &&
                                   now_ms - m_last_click_ms <= kDoubleClickWindowMs);
                    m_last_click_slot = hit;
                    if (m_tab == Tab::Normal &&
                        server::collection_def(it.index).kind == server::CollectionKind::Taegeuk) {
                        m_collection_selected_iid = it.iid;
                    }
                    m_last_click_ms = now_ms;
                    m_last_wear_click_slot = -1;
                    m_source_tab = m_tab;
                    m_drag_hover_ms = 0;
                    m_press_slot = hit;
                    m_press_mx = mx;
                    m_press_my = my;
                    m_drag_index = it.index;
                    m_drag_enchant_protected = it.enchant_protected;
                    m_drag_account_bound = it.account_bound;
                    m_drag_character_bound = it.character_bound;
                    m_drag_count = it.count;
                    m_drag_egg_level = it.transform_level < 1 ? 1 : it.transform_level;
                } else {
                    m_last_click_slot = -1;
                    m_last_wear_click_slot = -1;
                    m_arm_equip = false;
                }
            }
            return ClickResult::Handled;
        }

        if (m_catalog && m_wear) {
            for (const int left : {1, 0, 2, 3}) {
                for (int i = 0; i < 8; ++i) {
                    float sx = 0.0f;
                    float sy = 0.0f;
                    float sw = 0.0f;
                    float sh = 0.0f;
                    if (!wear_column_rect(left, i, sx, sy, sw, sh)) {
                        break;
                    }
                    if (!UiAssets::point_in_rect(mx, my, sx, sy, sw, sh)) {
                        continue;
                    }
                    const std::uint64_t now_ms = steady_ms_now();
                    const int wear_slot = left * 16 + i;
                    const bool dbl = (wear_slot == m_last_wear_click_slot &&
                                      now_ms - m_last_wear_click_ms <= kDoubleClickWindowMs);
                    m_last_wear_click_slot = wear_slot;
                    m_last_wear_click_ms = now_ms;
                    m_last_click_slot = -1;
                    if (!dbl) {
                        return ClickResult::Handled;
                    }
                    const std::string id = wear_layout_slot_id(m_layout, left, i);
                    const std::int32_t idx = wear_equipped_index(wear_out, id);
                    if (idx == 0) {
                        return ClickResult::Handled;
                    }
                    for (int si = kBagSlots;
                         si < kBagSlots + static_cast<int>(bag.equipment.size());
                         ++si) {
                        const auto& cand = *bag.item_at(si);
                        if (cand.index != idx || !cand.equipped) {
                            continue;
                        }
                        const auto eq = Bag::toggle_equip_slot(
                            bag, si, wear_out, *m_catalog, m_player_clazz, m_player_level);
                        if (eq.changed && eq.op == EquipNetOp::PutOff) {
                            const std::uint32_t cooldown_key =
                                eq.iid != 0 ? eq.iid : static_cast<std::uint32_t>(eq.index);
                            m_putoff_cooldown_ms[cooldown_key] = steady_ms_now() + kEquipCooldownMs;
                            m_last_equip_op = eq.op;
                            m_last_equip_index = eq.index;
                            m_last_equip_iid = eq.iid;
                            return ClickResult::EquipChanged;
                        }
                    }
                    return ClickResult::Handled;
                }
            }
        }

        return ClickResult::Handled;
    }

    ClickResult Window::handle_right_click(float mx, float my, Bag& bag) {
        refresh_general_projection(bag);
        if (!m_open || !contains(mx, my)) {
            return ClickResult::Ignored;
        }
        m_last_teleport_loc = -1;
        m_last_refresh_hp = 0;
        m_last_refresh_mp = 0;
        m_last_consumed_slot = -1;
        m_last_consumed_index = 0;
        m_last_consumed_iid = 0;
        if (m_drag_slot >= 0 || m_dragging_window) {
            return ClickResult::Handled;
        }
        const int hit = grid_slot_at(mx, my);
        if (hit < 0 || !m_catalog || (m_tab == Tab::Quest && hit >= kOtherSectionSlots)) {
            return ClickResult::Handled;
        }
        auto& slots = active_slots(bag);
        if (hit >= static_cast<int>(slots.size())) {
            return ClickResult::Handled;
        }
        auto& stack = slots[static_cast<std::size_t>(hit)];
        if (stack.index == 0 || stack.count <= 0) {
            return ClickResult::Handled;
        }
        const ItemDef* def = m_catalog->find(stack.index);
        if (!def || def->is_task() || !def->is_consumable() || def->auto_hp()) {
            return ClickResult::Handled;
        }
        m_last_consumed_slot = hit;
        m_last_consumed_index = stack.index;
        m_last_consumed_iid = stack.iid;
        m_last_teleport_loc = def->teleport_loc();
        m_last_refresh_hp = def->refresh_hp();
        m_last_refresh_mp = def->refresh_mp();
        return ClickResult::Consumed;
    }

    ClickResult Window::handle_release(float mx, float my, Bag& bag, Wear& wear_out) {
        refresh_general_projection(bag);
        m_scrolling = false;
        m_drag_hover_ms = 0;
        auto& source_slots = m_source_tab == Tab::Quest ? bag.quest : bag.normal;
        const int press = m_press_slot;
        const bool arm = m_arm_equip;
        m_press_slot = -1;
        m_arm_equip = false;
        if (m_drag_slot >= 0) {
            const int from = m_drag_slot;
            m_drag_slot = -1;
            if (m_source_tab == Tab::Quest && from >= kOtherSectionSlots) {
                return ClickResult::Handled;
            }
            if (m_catalog && from >= 0 && from < static_cast<int>(source_slots.size())) {
                const auto* definition = m_catalog->find(source_slots[from].index);
                if (definition && definition->is_task()) {
                    return ClickResult::Handled;
                }
            }
            const int cell = collection_slot_at(mx, my);
            if (cell >= 0 && m_catalog && m_source_tab == Tab::Normal &&
                from < static_cast<int>(bag.normal.size())) {
                const auto& item = bag.normal[from];
                if (!item.equipped && server::collection_accepts(item.index, cell)) {
                    const auto result = Bag::toggle_equip_slot(
                        bag, from, wear_out, *m_catalog, m_player_clazz, m_player_level);
                    if (result.changed) {
                        m_last_equip_op = result.op;
                        m_last_equip_index = result.index;
                        m_last_equip_iid = result.iid;
                        return ClickResult::EquipChanged;
                    }
                }
                return ClickResult::Handled;
            }
            float dx, dy, dw, dh;
            if (destroy_rect(dx, dy, dw, dh) && UiAssets::point_in_rect(mx, my, dx, dy, dw, dh)) {
                auto& slots = source_slots;
                if (from >= 0 && from < static_cast<int>(slots.size())) {
                    const auto& gone = slots[static_cast<std::size_t>(from)];
                    if (gone.index == 0 || gone.count <= 0) {
                        return ClickResult::Handled;
                    }
                    m_last_destroyed_slot = from;
                    m_last_destroyed_iid = gone.iid;
                    return ClickResult::Destroyed;
                }
            }
            if (m_source_tab == Tab::Normal && m_catalog && from >= 0 &&
                from < static_cast<int>(bag.normal.size())) {
                const ItemDef* def =
                    m_catalog->find(bag.normal[static_cast<std::size_t>(from)].index);
                for (const int left : {1, 0, 2, 3}) {
                    for (int i = 0; i < 8; ++i) {
                        float sx, sy, sw, sh;
                        if (!wear_column_rect(left, i, sx, sy, sw, sh)) {
                            break;
                        }
                        if (!UiAssets::point_in_rect(mx, my, sx, sy, sw, sh)) {
                            continue;
                        }
                        const std::string id = wear_layout_slot_id(m_layout, left, i);
                        if (id.compare(0, 5, "skin.") == 0 && wear_out.has_full_body_skin() &&
                            protocol::is_body_skin_slot(
                                protocol::skin_slot_for_part(id.substr(5))) &&
                            id != "skin.upper") {
                            return ClickResult::Handled;
                        }
                        const std::int32_t worn_idx = wear_equipped_index(wear_out, id);
                        if (def && def->has_repair() && worn_idx != 0) {
                            const int to_slot = bag_slot_for_equipped(bag, worn_idx);
                            const ItemDef* gear = m_catalog->find(worn_idx);
                            if (to_slot >= 0 && gear && gear->endurance() > 0) {
                                const auto& src = bag.normal[static_cast<std::size_t>(from)];
                                const auto& dst = *bag.item_at(to_slot);
                                m_last_enchant_from_slot = from;
                                m_last_enchant_to_slot = to_slot;
                                m_last_enchant_from_id =
                                    src.iid != 0 ? src.iid : static_cast<std::uint32_t>(src.index);
                                m_last_enchant_to_id =
                                    dst.iid != 0 ? dst.iid : static_cast<std::uint32_t>(dst.index);
                                return ClickResult::Enchanted;
                            }
                        }
                        if (def && wear_id_matches(id, *def)) {
                            const auto& item = bag.normal[static_cast<std::size_t>(from)];
                            if (item.equipped) {
                                return ClickResult::Handled;
                            }
                            const std::uint32_t key =
                                item.iid != 0 ? item.iid : static_cast<std::uint32_t>(item.index);
                            const auto cooldown = m_putoff_cooldown_ms.find(key);
                            if (cooldown != m_putoff_cooldown_ms.end() &&
                                steady_ms_now() < cooldown->second) {
                                return ClickResult::Handled;
                            }
                            const auto result = Bag::toggle_equip_slot(
                                bag, from, wear_out, *m_catalog, m_player_clazz, m_player_level);
                            if (result.changed) {
                                m_last_equip_op = result.op;
                                m_last_equip_index = result.index;
                                m_last_equip_iid = result.iid;
                                return ClickResult::EquipChanged;
                            }
                        }
                        return ClickResult::Handled;
                    }
                }
            }
            const int to = grid_slot_at(mx, my);
            if (to >= 0 && (to != from || m_tab != m_source_tab)) {
                auto& slots = source_slots;
                auto& destination = active_slots(bag);
                if (from < static_cast<int>(slots.size()) &&
                    to < static_cast<int>(destination.size())) {
                    const int to_page = to / kSlotsPerPage;
                    const bool dest_empty = destination[static_cast<std::size_t>(to)].index == 0 ||
                                            destination[static_cast<std::size_t>(to)].count <= 0;
                    if (!dest_empty && m_source_tab == Tab::Normal && m_tab == Tab::Normal &&
                        m_catalog) {
                        const ItemStack& src = slots[static_cast<std::size_t>(from)];
                        const ItemStack& dst = destination[static_cast<std::size_t>(to)];
                        const ItemDef* charm = m_catalog->find(src.index);
                        const ItemDef* gear = m_catalog->find(dst.index);
                        if (charm && gear && charm->has_repair() && gear->endurance() > 0) {
                            m_last_enchant_from_slot = from;
                            m_last_enchant_to_slot = to;
                            m_last_enchant_from_id =
                                src.iid != 0 ? src.iid : static_cast<std::uint32_t>(src.index);
                            m_last_enchant_to_id =
                                dst.iid != 0 ? dst.iid : static_cast<std::uint32_t>(dst.index);
                            return ClickResult::Enchanted;
                        }
                        const bool qigong_drop =
                            charm && gear &&
                            ((server::Qigong::is_perforation_shot(charm->index()) &&
                              server::Qigong::can_perforate(gear->is_weapon(),
                                                            gear->is_defense(),
                                                            gear->class_small(),
                                                            gear->grade(),
                                                            dst.equipped,
                                                            dst.qigong_perforated != 0) ==
                                  server::QigongApplyResult::Ok) ||
                             (server::Qigong::is_talisman(charm->index()) &&
                              server::Qigong::can_apply(gear->is_weapon(),
                                                        gear->is_defense(),
                                                        gear->class_small(),
                                                        gear->grade(),
                                                        dst.equipped,
                                                        dst.qigong_perforated != 0) ==
                                  server::QigongApplyResult::Ok));
                        const bool dragon_fusing_drop =
                            charm && gear && server::DragonFusing::is_scroll(charm->index()) &&
                            server::DragonFusing::can_apply(gear->is_weapon(),
                                                            gear->is_defense(),
                                                            gear->class_small(),
                                                            dst.equipped) ==
                                server::DragonFusingApplyResult::Ok;
                        if (charm && gear &&
                            (server::collection_def(dst.index).kind !=
                                 server::CollectionKind::None ||
                             qigong_drop || dragon_fusing_drop ||
                             (server::enchantment_talisman_grade(charm->index()) != 0 &&
                              !src.equipped &&
                              server::can_apply_weapon_enchantment(charm->index(),
                                                                   gear->is_weapon(),
                                                                   dst.equipped,
                                                                   dst.count,
                                                                   gear->grade())) ||
                             (charm->index() == server::kRevisionHammerIndex && !src.equipped &&
                              server::can_apply_revision_hammer(gear->is_weapon(),
                                                                gear->is_defense(),
                                                                gear->is_ornament(),
                                                                gear->class_small(),
                                                                dst.equipped,
                                                                dst.count,
                                                                gear->grade())) ||
                             (charm->index() == server::kPolishingStoneIndex && !src.equipped &&
                              server::can_apply_polishing_stone(gear->is_weapon(),
                                                                gear->is_defense(),
                                                                dst.equipped,
                                                                dst.count,
                                                                dst.enchant_protected)) ||
                             ((charm->has_changeprefix() || charm->has_charming() ||
                               charm->has_bof()) &&
                              !dst.equipped && charm_matches_gear(*charm, *gear)))) {
                            m_last_enchant_from_slot = from;
                            m_last_enchant_to_slot = to;
                            m_last_enchant_from_id =
                                src.iid != 0 ? src.iid : static_cast<std::uint32_t>(src.index);
                            m_last_enchant_to_id =
                                dst.iid != 0 ? dst.iid : static_cast<std::uint32_t>(dst.index);
                            return ClickResult::Enchanted;
                        }
                    }
                    const bool same_section = m_tab == m_source_tab &&
                                              (m_tab == Tab::Normal || from / kOtherSectionSlots ==
                                                                           to / kOtherSectionSlots);
                    const bool target_writable =
                        m_tab == Tab::Quest ? to < kOtherSectionSlots : register_writable(to_page);
                    const bool source_writable =
                        m_source_tab == Tab::Quest || register_writable(from / kSlotsPerPage);
                    if (same_section && target_writable && (dest_empty || source_writable)) {
                        std::swap(slots[static_cast<std::size_t>(from)],
                                  destination[static_cast<std::size_t>(to)]);
                    }
                }
            }
            if (contains(mx, my)) {
                return ClickResult::Handled;
            }

            auto& slots = source_slots;
            if (from < 0 || from >= static_cast<int>(slots.size())) {
                return ClickResult::Handled;
            }
            const ItemStack& stack = slots[static_cast<std::size_t>(from)];
            if (stack.iid == 0 || stack.count <= 0 || stack.equipped || stack.index == 0) {
                return ClickResult::Handled;
            }
            if (m_catalog) {
                const ItemDef* def = m_catalog->find(stack.index);
                if (def && def->is_quest()) {
                    return ClickResult::Handled;
                }
            }
            m_last_drop_iid = stack.iid;
            m_last_drop_count = stack.count;
            return ClickResult::DroppedOnWorld;
        }
        if (arm && press >= 0 && m_source_tab == Tab::Normal && m_catalog) {
            const auto& press_stack = bag.normal[static_cast<std::size_t>(press)];
            if (press_stack.equipped) {
                return ClickResult::Handled;
            }
            const std::uint32_t cooldown_key = press_stack.iid != 0
                                                   ? press_stack.iid
                                                   : static_cast<std::uint32_t>(press_stack.index);
            const auto cooldown_it = m_putoff_cooldown_ms.find(cooldown_key);
            const std::uint64_t now_ms = steady_ms_now();
            if (cooldown_it != m_putoff_cooldown_ms.end() && now_ms < cooldown_it->second) {
                return ClickResult::Handled;
            }
            const auto eq = Bag::toggle_equip_slot(
                bag, press, wear_out, *m_catalog, m_player_clazz, m_player_level);
            if (eq.changed) {
                if (eq.op == EquipNetOp::PutOff) {
                    const std::uint32_t cooldown_key =
                        eq.iid != 0 ? eq.iid : static_cast<std::uint32_t>(eq.index);
                    m_putoff_cooldown_ms[cooldown_key] = steady_ms_now() + kEquipCooldownMs;
                }
                m_last_equip_op = eq.op;
                m_last_equip_index = eq.index;
                m_last_equip_iid = eq.iid;
                return ClickResult::EquipChanged;
            }
        }
        return ClickResult::Ignored;
    }

    void Window::apply_last_destroy(Bag& bag, Wear* wear) {
        auto unequip_if_worn = [&](const ItemStack& stack) {
            if (wear && stack.equipped) {
                wear->unequip_index(stack.index);
            }
        };
        if (m_last_destroyed_iid != 0) {
            for (auto* slots : {&bag.normal, &bag.quest}) {
                for (auto& slot : *slots) {
                    if (slot.iid != m_last_destroyed_iid) {
                        continue;
                    }
                    const auto* definition = m_catalog ? m_catalog->find(slot.index) : nullptr;
                    if (definition && definition->is_task()) {
                        return;
                    }
                    unequip_if_worn(slot);
                    slot = {};
                    bag.compact_slots();
                    if (!register_viewable(bag, m_register_page)) {
                        m_register_page = 0;
                    }
                    return;
                }
            }
        }
        auto& slots = m_source_tab == Tab::Quest ? bag.quest : bag.normal;
        if (m_last_destroyed_slot < 0 || m_last_destroyed_slot >= static_cast<int>(slots.size())) {
            return;
        }
        const auto* definition =
            m_catalog ? m_catalog->find(slots[m_last_destroyed_slot].index) : nullptr;
        if (definition && definition->is_task()) {
            return;
        }
        unequip_if_worn(slots[static_cast<std::size_t>(m_last_destroyed_slot)]);
        slots[static_cast<std::size_t>(m_last_destroyed_slot)] = {};
        bag.compact_slots();
        if (!register_viewable(bag, m_register_page)) {
            m_register_page = 0;
        }
    }

    void Window::draw_drag_ghost(UiAssets& ui, float mx, float my) const {
        if (m_drag_slot < 0 || !m_catalog || m_drag_index == 0) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const std::string icon = m_catalog->icon_filename(m_drag_index, m_drag_egg_level);
        const UiWidget* bag_grid = m_v1.find_widget("bag");
        const float sz = (bag_grid && bag_grid->cell_w() > 0.0f ? bag_grid->cell_w() : 32.0f) * s;
        if (!icon.empty() && ui.item_tex(icon)) {
            ui.draw_item_tex(icon, mx - sz * 0.5f, my - sz * 0.5f, sz, sz);
            if (m_drag_enchant_protected && ui.renderer()) {
                UiView::draw_enchant_protection(*ui.renderer(),
                                                mx - sz * 0.5f,
                                                my - sz * 0.5f,
                                                sz,
                                                sz,
                                                steady_ms_now() / 1000.0);
            }
            UiView::draw_item_binding(ui,
                                      mx - sz * 0.5f,
                                      my - sz * 0.5f,
                                      sz,
                                      sz,
                                      m_drag_account_bound,
                                      m_drag_character_bound);
            if (m_drag_count > 1) {
                ui.draw_stack_count(
                    &m_body_font, mx - sz * 0.5f, my - sz * 0.5f, sz, sz, s, m_drag_count, true);
            }
        }
    }

    UiVarMap Window::make_vars(const Bag& bag) const {
        UiVarMap vars;
        ui_fill_theme_vars(m_v1, vars);
        vars["ui.title"] = "Inventory";
        vars["collection.title"] = "Collection";
        vars["collection.toggle"] = m_collection_open ? ">" : "<";
        vars["collection.absorb_help"] = "Select a bag Taegeuk, then Absorb.";
        const auto cells = server::collection_slots(collection_items(bag));
        const char* cell_names[] = {"Mirror", "Geon", "Tae",     "I",      "Jin", "Son", "Gam",
                                    "Gan",    "Gon",  "Taegeuk", "Mirror", "1",   "2",   "3",
                                    "4",      "5",    "1",       "2",      "3",   "4",   "5",
                                    "1",      "2",    "3",       "4",      "5"};
        for (int cell = 0; cell < server::kCollectionSlots; ++cell) {
            vars["collection.empty." + std::to_string(cell)] = cells[cell] ? "" : cell_names[cell];
        }
        vars["collection.absorb"] = "Absorb";
        vars["collection.taegeuk"] = "Taegeuk";
        vars["collection.taegeuk_help"] = "Eight trigrams, mirror and Taegeuk";
        vars["collection.hanin"] = "Hanin's Mirror";
        vars["collection.hanin_help"] = "Mirror + five essences. Drag to equip.";
        vars["collection.yeoiju"] = "Yeoiju";
        vars["collection.attack"] = "Attack";
        vars["collection.defense"] = "Defense";
        vars["collection.pvp_notice"] = "PvP: only movement speed applies.";
        vars["ui.tab_normal"] = "General";
        vars["ui.tab_task"] = "Other";
        vars["inventory.equipment_tab"] = "Equipment";
        vars["inventory.equipment_label"] = "Equipped";
        vars["inventory.expand_label"] =
            slots_capacity() >= kBagSlots ? "Fully expanded" : "Expand +36";
        vars["inventory.destroy_label"] = "Destroy";
        vars["inventory.general_capacity"] =
            "General: " + std::to_string(slots_capacity()) + " / 216 slots unlocked";
        vars["inventory.general_hint"] = "72 permanent slots + four timed expansions";
        vars["inventory.other_capacity"] = "Other: 72 permanent slots";
        for (int section = 0; section < 2; ++section) {
            int used = 0;
            for (int i = section * kOtherSectionSlots;
                 i <
                 std::min(static_cast<int>(bag.quest.size()), (section + 1) * kOtherSectionSlots);
                 ++i) {
                used += bag.quest[i].count > 0;
            }
            vars[section == 0 ? "inventory.quest_label" : "inventory.task_label"] =
                std::string(section == 0 ? "Quest Items" : "Task Items") + "  " +
                std::to_string(used) + " / 36";
        }
        constexpr const char* wear_ids[] = {"helmet",
                                            "upper",
                                            "gauntlet",
                                            "pants",
                                            "boots",
                                            "weapon",
                                            "shield",
                                            "ring",
                                            "necklace",
                                            "accessory",
                                            "face",
                                            "sinsu",
                                            "pet",
                                            "ride"};
        constexpr const char* wear_labels[] = {"Helmet",
                                               "Armor",
                                               "Gloves",
                                               "Pants",
                                               "Boots",
                                               "Weapon",
                                               "Shield",
                                               "Ring",
                                               "Necklace",
                                               "Accessory I",
                                               "Mask",
                                               "Sin Su",
                                               "Pet",
                                               "Mount"};
        for (int i = 0; i < 14; ++i) {
            vars[std::string("inventory.wear_label.") + wear_ids[i]] = wear_labels[i];
        }
        constexpr const char* skin_names[] = {
            "weapon", "shield", "helmet", "upper", "pants", "boots", "gauntlet", "face"};
        vars["inventory.skin_label"] = "Skins";
        vars["inventory.skin_locked_label"] = "Locked";
        for (int cell = 0; cell < static_cast<int>(protocol::kSkinSlots); ++cell) {
            vars[std::string("inventory.skin_locked.") + skin_names[cell]] =
                m_wear && m_wear->has_full_body_skin() &&
                        protocol::is_body_skin_slot(static_cast<protocol::SkinSlot>(cell)) &&
                        cell != static_cast<int>(protocol::SkinSlot::Upper)
                    ? "true"
                    : "false";
        }
        for (int category = 0; category < 6; ++category) {
            const std::string key = std::string("inventory.filter.") + kFilterIds[category];
            vars[key] = filter_active(category) ? "true" : "false";
            vars[key + "_visible"] =
                m_tab == Tab::Normal && filter_active(category) ? "true" : "false";
        }
        vars["inventory.sort_label"] = "Sort";
        vars["inventory.money_label"] = "Geon";
        vars["inventory.money"] = std::to_string(bag.money);
        vars["inventory.count_text"] =
            std::to_string(slots_used(bag)) + " / " +
            std::to_string(m_tab == Tab::Quest ? kOtherSlots : slots_capacity());
        vars["inventory.tab"] = m_tab == Tab::Normal ? "normal" : "quest";
        vars["inventory.bag_active"] = "true";
        vars["inventory.normal_active"] = m_tab == Tab::Normal ? "true" : "false";
        vars["inventory.quest_active"] = m_tab == Tab::Quest ? "true" : "false";
        vars["inventory.register"] = std::to_string(m_register_page);
        vars["inventory.scroll_up_label"] = "^";
        vars["inventory.scroll_down_label"] = "v";
        vars["inventory.scroll"] = std::to_string(scroll_row() * 46);
        vars["inventory.scroll_up"] = scroll_row() > 0 ? "true" : "false";
        vars["inventory.scroll_down"] = scroll_row() < max_scroll_row() ? "true" : "false";
        for (int i = 0; i < kMaxRegisters; ++i) {
            const std::string key = "inventory.page" + std::to_string(i + 1);
            vars[key + "_locked"] = register_writable(i) ? "false" : "true";
            vars[key + "_shaded"] =
                m_tab == Tab::Normal && !register_writable(i) ? "true" : "false";
            const auto now = steady_ms_now();
            const auto expiry = m_register_unlock_expire_ms[i];
            const std::string state = i < 2          ? "Permanent"
                                      : expiry > now ? format_duration((expiry - now + 999) / 1000)
                                                     : "Locked";
            vars[key + "_label"] = std::to_string(i * kSlotsPerPage + 1) + "-" +
                                   std::to_string((i + 1) * kSlotsPerPage) + "  " + state;
        }
        return vars;
    }

    void Window::draw(UiAssets& ui, engine::IEngine& engine, const Bag& bag) const {
        refresh_general_projection(bag);
        if (!m_open) {
            return;
        }
        if (Li(m_layout, "debug.border_only", 0) != 0) {
            draw_chrome(ui);
            return;
        }
        InventoryHost host(*this, bag, m_riding_equipment_iids, m_hovered_button, m_pointer_down);
        if (collection_progress() > 0.001f && m_collection_layout.loaded()) {
            UiView::draw(engine,
                         ui,
                         m_collection_document,
                         collection_x(),
                         collection_y(),
                         m_collection_document.window_w() * collection_scale(),
                         m_collection_document.window_h() * collection_scale(),
                         collection_scale(),
                         make_vars(bag),
                         m_body_font,
                         m_title_font.ready() ? m_title_font : m_body_font,
                         &host,
                         true);
        }
        UiView::draw(engine,
                     ui,
                     m_v1,
                     m_x,
                     m_y,
                     m_w,
                     m_h,
                     m_scale,
                     make_vars(bag),
                     m_body_font,
                     m_title_font.ready() ? m_title_font : m_body_font,
                     &host,
                     true);
        if (const auto* track = m_v1.find_widget("scroll_track")) {
            const int rows = m_tab == Tab::Normal ? 6 : 7;
            const float height = std::max(18.0f, track->h() * rows / (max_scroll_row() + rows));
            const float ratio =
                max_scroll_row() > 0 ? static_cast<float>(scroll_row()) / max_scroll_row() : 0;
            engine.draw_ui_rect(m_x + (track->x() + 2) * m_scale,
                                m_y + (track->y() + (track->h() - height) * ratio) * m_scale,
                                (track->w() - 4) * m_scale,
                                height * m_scale,
                                .65f,
                                .52f,
                                .32f,
                                1);
        }
    }

    void Window::draw_tooltip(UiAssets& ui,
                              engine::IEngine& engine,
                              const Bag& bag,
                              const FreetypeFont* tip_font,
                              float screen_w,
                              float screen_h) const {
        if (!m_open) {
            return;
        }
        const FreetypeFont& body = m_body_font;
        const bool equipment_hover = !m_collection_hover && m_hover_slot >= kBagSlots;
        const auto& slots = m_collection_hover ? bag.collection
                            : equipment_hover  ? bag.equipment
                                               : active_slots(bag);
        const int hover_slot = equipment_hover ? m_hover_slot - kBagSlots : m_hover_slot;

        if (m_hover_register >= 2 && m_drag_slot < 0) {
            const auto* tip = tip_font && tip_font->ready() ? tip_font
                              : body.ready()                ? &body
                                                            : nullptr;
            if (tip) {
                std::vector<TipLine> lines;
                TipLine title;
                title.add("Inventory expansions", .9f, .8f, .6f);
                lines.push_back(std::move(title));
                const auto now = steady_ms_now();
                for (int i = 2; i < kMaxRegisters; ++i) {
                    const auto expiry = m_register_unlock_expire_ms[i];
                    const std::string status =
                        expiry > now ? format_duration((expiry - now + 999) / 1000) + " remaining"
                                     : "Locked";
                    TipLine line;
                    line.add(std::to_string(i * kSlotsPerPage + 1) + "-" +
                                 std::to_string((i + 1) * kSlotsPerPage) + ": " + status,
                             expiry > now ? .6f : .75f,
                             expiry > now ? .9f : .7f,
                             .6f);
                    lines.push_back(std::move(line));
                }
                TipLine help;
                help.add(inventory_unlock_writ(bag, m_catalog)
                             ? "Uses the shortest available Inventory Unlock writ."
                             : "No Inventory Unlock writ available.",
                         .8f,
                         .75f,
                         .65f);
                lines.push_back(std::move(help));
                ItemTooltip::draw(ui,
                                  engine,
                                  *tip,
                                  lines,
                                  m_hover_mx,
                                  std::max(0.0f, m_hover_my - 170 * m_scale),
                                  screen_w,
                                  screen_h);
                return;
            }
        }

        const int category = filter_button_index(m_hovered_button);
        if (category >= 0 && m_drag_slot < 0) {
            const auto* tip = tip_font && tip_font->ready() ? tip_font
                              : body.ready()                ? &body
                                                            : nullptr;
            if (tip) {
                std::vector<TipLine> lines(3);
                lines[0].add(kFilterLabels[category], .9f, .8f, .6f);
                lines[1].add(filter_active(category) ? "Selected - click to turn off"
                                                     : "Click to select this category",
                             .9f,
                             .88f,
                             .8f);
                lines[2].add("Combine categories. None selected shows all items.", .75f, .75f, .7f);
                ItemTooltip::draw(
                    ui, engine, *tip, lines, m_hover_mx, m_hover_my, screen_w, screen_h);
                return;
            }
        }

        // Hover tooltip near the cursor — tooltip layout and #0–#9 colour markup.
        if (hover_slot >= 0 && hover_slot < static_cast<int>(slots.size()) && m_catalog &&
            m_drag_slot < 0) {
            const auto& it = slots[static_cast<std::size_t>(hover_slot)];
            if (it.index != 0) {
                const client::ItemDef* def = m_catalog->find(it.index);
                if (def) {
                    const FreetypeFont* tip = (tip_font && tip_font->ready())
                                                  ? tip_font
                                                  : (body.ready() ? &body : nullptr);
                    if (tip) {
                        const int cur_end = it.cur_end > 0 ? it.cur_end : -1;
                        const int max_end = it.max_end > 0 ? it.max_end : -1;
                        ItemTipExtras extras;
                        extras.x_attack = it.x_attack;
                        extras.x_magic = it.x_magic;
                        extras.x_hit = it.x_hit;
                        extras.x_defense = it.x_defense;
                        extras.x_dodge = it.x_dodge;
                        extras.upgrade_level = it.upgrade_level;
                        extras.random_stat_type =
                            static_cast<protocol::RandomItemStatType>(it.random_stat_type);
                        extras.random_stat_value = it.random_stat_value;
                        extras.mix_type = it.mix_type;
                        extras.bof = it.bof;
                        extras.qigong_perforated = it.qigong_perforated;
                        extras.qigong_effect = it.qigong_effect;
                        extras.dragon_effect = it.dragon_effect;
                        extras.account_bound = it.account_bound;
                        extras.character_bound = it.character_bound;
                        extras.enchant_protected = it.enchant_protected;
                        if (def->is_transform()) {
                            extras.egg_level = it.transform_level < 1 ? 1 : it.transform_level;
                            extras.egg_exp = it.transform_exp < 0 ? 0 : it.transform_exp;
                            if (it.equipped && m_transform_egg_index == it.index) {
                                extras.egg_level = m_transform_egg_level;
                                extras.egg_exp = m_transform_egg_exp;
                            }
                            extras.egg_name =
                                transform_egg_display_name(*def, extras.egg_level, m_shinsu);
                        }
                        if (def->is_pet()) {
                            const auto pet = m_pet_tooltip_info.find(def->index());
                            if (pet != m_pet_tooltip_info.end()) {
                                extras.pet_heal_per_two_seconds = pet->second.heal_per_two_seconds;
                                extras.pet_auto_pickup = pet->second.auto_pickup;
                                extras.lifetime_duration_seconds = pet->second.duration_seconds;
                            }
                        }
                        if (def->is_pet() || def->is_ride() ||
                            server::collection_def(def->index()).kind ==
                                server::CollectionKind::Mirror) {
                            if (server::collection_def(def->index()).kind ==
                                server::CollectionKind::Mirror) {
                                extras.lifetime_duration_seconds = 7 * 86400;
                            }
                            const auto expiry = m_item_expire_at.find(it.iid);
                            if (expiry != m_item_expire_at.end() && expiry->second > 0) {
                                extras.lifetime_expiration_known = true;
                                const std::int64_t remaining =
                                    static_cast<std::int64_t>(expiry->second) -
                                    static_cast<std::int64_t>(std::time(nullptr));
                                if (remaining > 0) {
                                    extras.lifetime_remaining_seconds =
                                        static_cast<std::uint64_t>(remaining);
                                }
                            }
                        }
                        const std::string markup = ItemTooltip::make_tip(*def,
                                                                         *m_catalog,
                                                                         it.count,
                                                                         m_player_clazz,
                                                                         m_player_level,
                                                                         it.prefix,
                                                                         cur_end,
                                                                         max_end,
                                                                         extras);
                        const auto lines = ItemTooltip::parse_markup(markup);
                        const float tip_sw = screen_w > 1.0f ? screen_w : m_x + m_w + 64.0f;
                        const float tip_sh = screen_h > 1.0f ? screen_h : m_y + m_h + 64.0f;
                        ItemTooltip::draw(
                            ui, engine, *tip, lines, m_hover_mx, m_hover_my, tip_sw, tip_sh);
                    }
                }
            }
        }
    }

}  // namespace openkal::client
