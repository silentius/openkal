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

#include "openkal/client/Equipment.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    class ShinsuDb;

    // General storage: six vertically stacked, scrollable 6x6 blocks. Base capacity is 72;
    // registers 2-5 are optional extras. Disabled registers that still hold items
    // remain viewable for take-out only (no deposit into empty slots there).
    inline constexpr int kSlotIcon = 32;
    inline constexpr int kCols = 6;
    inline constexpr int kRowsPerPage = 6;
    inline constexpr int kSlotsPerPage = kCols * kRowsPerPage;  // 36
    inline constexpr int kMaxRegisters = 6;
    inline constexpr int kBagSlots = kSlotsPerPage * kMaxRegisters;  // 216

    /** Ordinary equipment and eight cosmetic cells; cell 14 remains reserved for Collection. */
    inline constexpr int kEquipmentSlots = 23;

    /** Permanent capacity for each dedicated Other section. */
    inline constexpr int kOtherSectionSlots = 36;
    /** Quest cells followed by protected task cells in the Other storage vector. */
    inline constexpr int kOtherSlots = 2 * kOtherSectionSlots;

    /** Minimum wait after unequip before the same item can be worn again. */
    inline constexpr std::uint64_t kEquipCooldownMs = 1000;

    /** Shared category IDs and captions in inventory filter order. */
    inline constexpr std::array<const char*, 6> kInventoryFilterIds{
        "consumables", "weapons", "armors", "enchantment", "skins_pets", "other"};
    inline constexpr std::array<const char*, 6> kInventoryFilterLabels{
        "Consumables", "Weapons", "Armors", "Enchantment", "Skins & Pets", "Other items"};

    /** Resolves an inventory filter ID.
     * @param id Category ID.
     * @return Category index, or -1 for an unknown ID.
     */
    [[nodiscard]] int inventory_filter_index(std::string_view id);

    /** Classifies an item for the inventory filters.
     * @param def Item definition, if known.
     * @return Category index in inventory filter order.
     */
    [[nodiscard]] int inventory_filter_category(const ItemDef* def);

    /** Inventory views beside the permanently visible worn equipment columns. */
    enum class Tab : int { Normal = 0, Quest = 1 };

    /** JSON-defined behavior shown in an inventory pet tooltip. */
    struct PetTooltipInfo {
        /** HP restored every two seconds while worn. */
        int heal_per_two_seconds = 0;

        /** Whether the pet automatically picks eligible drops. */
        bool auto_pickup = false;

        /** Configured lifetime in seconds; 0 means permanent. */
        std::uint64_t duration_seconds = 0;
    };

    /** Outcome of a click offered to the inventory window. */
    enum class ClickResult : int {
        Ignored = 0,
        Handled = 1,
        EquipChanged = 2,
        Destroyed = 3,
        Consumed = 4,
        Enchanted = 5,
        DroppedOnWorld = 6,
        OpenMail = 7,
        SortRequested = 8,
    };

    /** Pure data record for one bag stack (intentional DTO). */
    struct ItemStack {
        /** item definition index (0 = empty). */
        std::int32_t index = 0;

        /** Stack count. */
        int count = 0;

        /** True when this cell is the worn instance (ITEM_PUTON). */
        bool equipped = false;

        /** Equipped prefix index, or 0. */
        int prefix = 0;

        /** Inventory instance id from the server, or 0. */
        std::uint32_t iid = 0;

        /** Current endurance, or 0 when unused. */
        int cur_end = 0;

        /** Max endurance, or 0 when unused. */
        int max_end = 0;

        /** Talisman of Attack plus. */
        int x_attack = 0;

        /** Talisman of Magic plus. */
        int x_magic = 0;

        /** Talisman of Accuracy plus. */
        int x_hit = 0;

        /** Talisman of Defense plus. */
        int x_defense = 0;

        /** Talisman of Evasion plus. */
        int x_dodge = 0;

        /** Explosive Blow level. */
        int upgrade_level = 0;

        /** Stone of Revision applications at the current EB level. */
        int upgrade_rate = 0;

        /** Mixing effect type encoded as int32 (low 8 bits transported). */
        int mix_type = 0;

        /** 1 when Bead of Fire is applied to this armor instance. */
        int bof = 0;

        /** 1 after a successful Perforation Shot. */
        int qigong_perforated = 0;

        /** Qigong random effect id in [1, 80], or zero. */
        int qigong_effect = 0;

        /** Dragon Fusing random effect id in [1, 240], or zero. */
        int dragon_effect = 0;

        /** Random equipment modifier type encoded as protocol::RandomItemStatType. */
        int random_stat_type = 0;

        /** Random equipment modifier magnitude (1 through 5), or zero. */
        int random_stat_value = 0;

        /** Animal level stored on this transform egg (compatibility behavior). */
        int transform_level = 1;

        /** Animal EXP stored on this transform egg (compatibility behavior). */
        int transform_exp = 0;

        /** True when the instance cannot be sold, dropped, traded, or converted. */
        bool account_bound = false;

        /** One remaining enchantment attempt protected by a Polishing Stone. */
        bool enchant_protected = false;

        /** True when the instance cannot leave its character or enter account storage. */
        bool character_bound = false;
    };

    /** Compares two occupied stacks in inventory sort order.
     * @param a First stack.
     * @param b Second stack.
     * @param a_name First item name.
     * @param b_name Second item name.
     * @return True if the first stack sorts before the second.
     */
    [[nodiscard]] bool inventory_stack_precedes(const ItemStack& a,
                                                const ItemStack& b,
                                                std::string_view a_name,
                                                std::string_view b_name);

    // Starter weapons: knight/mage/archer from g_nItemIndex[]; thief = training
    // dagger (item definition 1404).
    inline constexpr int kStarterSword = 1;      // Short Iron Sword (knight)
    inline constexpr int kStarterWand = 90;      // Bamboo Stick (mage)
    inline constexpr int kStarterBow = 22;       // Birch Bow (archer)
    inline constexpr int kStarterDagger = 1404;  // Training Dagger (thief)

    // Individual city teleport scrolls (specialty.teleport.loc → kTeleportLocs).
    inline constexpr int kScrollNarootuh = 192;
    inline constexpr int kScrollJookSuh = 193;
    inline constexpr int kScrollGeumOhMine = 194;
    inline constexpr int kScrollGiantBird = 195;
    inline constexpr int kScrollGeumOheeFort = 196;
    inline constexpr int kScrollCityOfPriest = 485;

    /** Network equip op reported after a successful bag/wear toggle. */
    enum class EquipNetOp : std::uint8_t { None = 0, PutOn = 1, PutOff = 2 };

    /** Pure data record for a bag-slot equip toggle (intentional DTO). */
    struct EquipToggleResult {
        bool changed = false;
        EquipNetOp op = EquipNetOp::None;
        std::int32_t index = 0;
        std::uint32_t iid = 0;
    };

    /** Result of consuming one matching stack from the bag. */
    struct ConsumeResult {
        /** True when a stack was decremented. */
        bool ok = false;

        /** City teleport loc from the consumed item, or -1. */
        int teleport_loc = -1;

        /** HP restored by the consumed item. */
        int refresh_hp = 0;

        /** MP restored by the consumed item. */
        int refresh_mp = 0;
    };

    /** Tracks active item-use cooldowns by definition, with one shared HP/MP refresh cooldown. */
    class ItemCooldowns {
      public:
        /** Binds the item definitions used to resolve durations and shared cooldowns.
         * @param catalog Item definitions; must outlive the cooldowns.
         */
        explicit ItemCooldowns(const ItemDb& catalog) : m_catalog(catalog) {}

        /** Starts the configured cooldown for a requested item use.
         * @param item_index Item definition index.
         */
        void start(int item_index);

        /** Advances active cooldowns and removes expired entries.
         * @param elapsed Seconds since the previous update.
         */
        void advance(float elapsed);

        /** Looks up the active cooldown for an item.
         * @param item_index Item definition index.
         * @return Remaining fraction from 0 (ready) to 1 (just started).
         */
        [[nodiscard]] float remaining_fraction(int item_index) const;

      private:
        struct Timer {
            float remaining;
            float duration;
        };

        const ItemDb& m_catalog;
        std::unordered_map<int, Timer> m_active;
    };

    /** Pure data record for player bag contents (intentional DTO). */
    struct Bag {
        std::vector<ItemStack> normal;
        /** Worn ordinary instances, independent of General inventory capacity. */
        std::vector<ItemStack> equipment;
        /** Other storage: quest slots 0–35, task slots 36–71. */
        std::vector<ItemStack> quest;
        /** Equipped collection instances, stored independently of inventory capacity. */
        std::vector<ItemStack> collection;
        std::int64_t money = 0;

        /** Per-register writability used by automatic compaction after insert/remove. */
        std::array<bool, kMaxRegisters> compact_register_enabled{
            true, true, false, false, false, false};

        /**
         * Replaces the register mask used when compacting after insert/remove.
         * @param enabled Per-register flags (length kMaxRegisters).
         */
        void set_compact_register_enabled(const std::array<bool, kMaxRegisters>& enabled) {
            compact_register_enabled = enabled;
        }

        /** Compacts normal and quest bags using compact_register_enabled. */
        void compact_slots() {
            sort_compact(compact_register_enabled.data());
        }

        /** Finds an owned instance across inventory and equipped storage.
         * @param iid Inventory instance id.
         * @return Matching stack, or nullptr.
         */
        [[nodiscard]] const ItemStack* find_instance(std::uint32_t iid) const;

        /** Applies an authoritative ordinary equipment transfer, including slot replacement.
         * @param iid Instance to equip or return to General.
         * @param equipped True to move into its equipment slot.
         * @param catalog Item definitions used to resolve equipment slots.
         * @return True when the location was applied or already matches.
         */
        [[nodiscard]] bool
        set_equipment_equipped(std::uint32_t iid, bool equipped, const ItemDb& catalog);

        /** Resolves a General or equipment UI slot (equipment starts at kBagSlots).
         * @param slot Combined UI slot index.
         * @return Matching instance, or nullptr for an invalid slot.
         */
        [[nodiscard]] ItemStack* item_at(int slot);

        /** Resolves a General or equipment UI slot without modifying it.
         * @param slot Combined UI slot index.
         * @return Matching instance, or nullptr for an invalid slot.
         */
        [[nodiscard]] const ItemStack* item_at(int slot) const;

        /** Sorts Quest Items while leaving protected Task Items in place.
         * @param catalog Item names, or nullptr to order by definition index.
         */
        void sort_other(const ItemDb* catalog);

        /**
         * Resolves the class starter weapon item definition index.
         * @param clazz Player class index.
         * @return item definition index for the starter weapon.
         */
        [[nodiscard]] static int starter_weapon_index(int clazz);

        /**
         * Requests an equipment transfer. Instances move on server acknowledgement.
         * @param bag Player bag.
         * @param slot General or equipment UI slot index (equipment starts at kBagSlots).
         * @param wear Current equipment used to validate hand conflicts.
         * @param catalog Item catalog for equip rules.
         * @param clazz Player class index.
         * @param level Character level.
         * @return Toggle outcome (changed, op, index).
         */
        static EquipToggleResult toggle_equip_slot(
            Bag& bag, int slot, Wear& wear, const ItemDb& catalog, int clazz = 0, int level = 1);

        /** Clears inventory, equipment, collection slots and money. */
        void clear();

        /**
         * Seeds an SP starter set: class weapon (NewPlayer) + a few consumables for
         * testing. Knight also gets one of each individual city teleport scroll.
         * @param catalog Item catalog used to validate item definition indices.
         * @param clazz Player class index.
         * @param level Character level (reserved).
         */
        void seed_demo(const ItemDb& catalog, int clazz = 0, int level = 1);

        /**
         * Compacts: packs enabled pages as a contiguous prefix (no gaps); packs each
         * disabled page in place so overflow stays on that register until taken out.
         * @param register_enabled Per-register writability flags (length
         * kMaxRegisters).
         */
        void sort_compact(const bool register_enabled[kMaxRegisters]);

        /** Sorts normal items by name and packs writable pages without moving locked items.
         * @param catalog Item names, or nullptr to order by item index.
         * @param register_enabled Per-register writability flags (length kMaxRegisters).
         */
        void sort_items(const ItemDb* catalog, const bool register_enabled[kMaxRegisters]);

        /**
         * Inserts into the matching General, Quest Items or Task Items section.
         * Authoritative legacy overflow remains visible in General.
         * Wearable items and items without (stackable 1) each take their own slot.
         * @param index item definition index (31 = money → adds to money).
         * @param count Stack count or money amount.
         * @param catalog Item catalog used for the stackable rule.
         * @param prefix Prefix index stored on the stack, or 0.
         * @param iid Inventory instance id, or 0.
         * @param cur_end Current endurance from the server, or 0 to use the catalog.
         * @param max_end Max endurance from the server, or 0 to use the catalog.
         * @param equipped Restore a worn instance directly into its equipment storage.
         * @return True when inserted.
         */
        [[nodiscard]] bool try_insert(std::int32_t index,
                                      int count,
                                      const ItemDb& catalog,
                                      int prefix = 0,
                                      std::uint32_t iid = 0,
                                      int cur_end = 0,
                                      int max_end = 0,
                                      bool equipped = false);

        /** Applies an authoritative collection location change without losing instance data.
         * @param iid Item instance to move.
         * @param equipped True to move into collection, false to return to a writable bag slot.
         * @return True if the item is already at the destination or was moved successfully.
         */
        [[nodiscard]] bool set_collection_equipped(std::uint32_t iid, bool equipped);

        /**
         * Updates an existing stack by inventory instance id.
         * @param iid Inventory instance id.
         * @param index item definition index.
         * @param prefix Prefix index.
         * @param count Absolute stack count when set_count is true.
         * @param set_count True to replace the stack count.
         * @param prefix_changed Out: true when the prefix value changed.
         * @param cur_end Current endurance, or -1 to leave unchanged.
         * @param max_end Max endurance, or -1 to leave unchanged.
         * @return True when a matching iid was found.
         */
        [[nodiscard]] bool try_update_instance(std::uint32_t iid,
                                               std::int32_t index,
                                               int prefix,
                                               int count,
                                               bool set_count,
                                               bool& prefix_changed,
                                               int cur_end = -1,
                                               int max_end = -1);

        /**
         * Writes animal level/EXP onto a bag egg (S2C_UPDATEITEMINFO).
         * @param iid Inventory instance id.
         * @param level Animal level (at least 1).
         * @param exp EXP toward the next level (at least 0).
         * @return True when a matching iid was found.
         */
        [[nodiscard]] bool try_set_transform_progress(std::uint32_t iid, int level, int exp);

        /**
         * Removes the stack with the given inventory instance id.
         * @param iid Inventory instance id.
         * @return True when a matching iid was found and cleared.
         */
        [[nodiscard]] bool try_remove_iid(std::uint32_t iid);

        /**
         * Removes items by item definition index from normal (then quest) slots.
         * @param index item definition index.
         * @param count Count to remove.
         * @return True when enough items were removed.
         */
        [[nodiscard]] bool try_remove(std::int32_t index, int count);

        /**
         * Consumes one matching non-equipped stack (normal bag, then quest).
         * @param index item definition index.
         * @param catalog Catalog used to test is_consumable and read refresh/teleport.
         * @return Consume result; ok is false when no stack was taken.
         */
        [[nodiscard]] ConsumeResult try_consume_one(std::int32_t index, const ItemDb& catalog);

        /**
         * Finds the first non-equipped stack of an item definition index (normal, then quest).
         * @param index item definition index.
         * @param iid Out inventory instance id, or 0 when the stack has none.
         * @param count Out stack count.
         * @return True when a matching stack was found.
         */
        [[nodiscard]] bool
        try_find_stack(std::int32_t index, std::uint32_t& iid, int& count) const {
            iid = 0;
            count = 0;
            if (index <= 0) {
                return false;
            }
            auto scan = [index, &iid, &count](const std::vector<ItemStack>& slots) -> bool {
                for (const auto& slot : slots) {
                    if (slot.index != index || slot.count <= 0 || slot.equipped) {
                        continue;
                    }
                    iid = slot.iid;
                    count = slot.count;
                    return true;
                }
                return false;
            };
            return scan(equipment) || scan(normal) || scan(quest);
        }

        /**
         * Counts matching inventory and ordinary equipment instances.
         * @param index item definition index.
         * @return Total count, or 0.
         */
        [[nodiscard]] int count_item(std::int32_t index) const {
            if (index <= 0) {
                return 0;
            }
            int total = 0;
            auto add = [&](const std::vector<ItemStack>& slots) {
                for (const auto& slot : slots) {
                    if (slot.index == index && slot.count > 0) {
                        total += slot.count;
                    }
                }
            };
            add(normal);
            add(quest);
            add(equipment);
            return total;
        }

        /**
         * Animal level of a bag egg. Prefers the worn instance when several match.
         * @param index item definition index.
         * @return Level, or 1 when none.
         */
        [[nodiscard]] int transform_level_for_index(std::int32_t index) const {
            if (index <= 0) {
                return 1;
            }
            int found = 0;
            auto scan = [&](const std::vector<ItemStack>& slots) {
                for (const auto& slot : slots) {
                    if (slot.index != index || slot.count <= 0) {
                        continue;
                    }
                    const int lv = slot.transform_level < 1 ? 1 : slot.transform_level;
                    if (slot.equipped) {
                        found = lv;
                        return true;
                    }
                    if (found == 0) {
                        found = lv;
                    }
                }
                return false;
            };
            if (!scan(equipment) && !scan(normal)) {
                (void) scan(quest);
            }
            return found < 1 ? 1 : found;
        }

        /**
         * Checks whether the worn instance of an item index is account-bound.
         * @param index item definition index.
         * @return True when the worn matching instance is account-bound.
         */
        [[nodiscard]] bool account_bound_for_index(std::int32_t index) const {
            if (index <= 0) {
                return false;
            }
            auto scan = [&](const std::vector<ItemStack>& slots) {
                return std::any_of(slots.begin(), slots.end(), [index](const ItemStack& slot) {
                    return slot.index == index && slot.count > 0 && slot.equipped &&
                           slot.account_bound;
                });
            };
            return scan(equipment) || scan(normal) || scan(quest);
        }
    };

    /** Computes worn Collection primary bonuses for the Character Info equipment display.
     * @param bag Inventory and separately stored collection instances.
     * @return PvE STR, HTH, INT, WIS and AGI bonuses, rounded as in server combat.
     */
    [[nodiscard]] std::array<int, 5> collection_primary_bonuses(const Bag& bag);

    /**
     * Compact inventory with worn equipment and vertically scrollable General/Other storage.
     *
     * Chrome and geometry come from config/ui/inventory.json when a layout is
     * bound.
     */
    class Window {
      public:
        /** @return True while the window is shown. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /**
         * Shows or hides the window; opening selects General.
         * @param open True to show it.
         */
        void set_open(bool open) {
            if (open && !m_open) {
                set_tab(Tab::Normal);
            }
            m_open = open;
            if (!m_open) {
                m_drag_slot = -1;
                m_press_slot = -1;
                m_last_click_slot = -1;
                m_last_click_ms = 0;
                m_last_wear_click_slot = -1;
                m_last_wear_click_ms = 0;
                m_arm_equip = false;
                m_drag_index = 0;
                m_drag_count = 0;
                m_drag_egg_level = 1;
                m_dragging_window = false;
                m_scrolling = false;
            }
        }

        /** Flips the open state. */
        void toggle() {
            set_open(!m_open);
        }

        /** @return Active content tab. */
        [[nodiscard]] Tab tab() const {
            return m_tab;
        }

        /**
         * Selects the content tab.
         * @param tab Tab to show.
         */
        void set_tab(Tab tab) {
            m_tab = tab;
            m_hover_slot = -1;
            m_collection_hover = false;
            m_last_click_slot = -1;
            m_last_wear_click_slot = -1;
        }

        /** @return Current register page index (0 .. kMaxRegisters-1). */
        [[nodiscard]] int register_page() const {
            return m_register_page;
        }

        /**
         * Selects the register page.
         * @param page Register page index.
         */
        void set_register_page(int page) {
            m_register_page = page;
        }

        /** @return Last clicked bag slot used for double-click equip, or -1. */
        [[nodiscard]] int selected() const {
            return m_last_click_slot;
        }

        /**
         * Clears or sets the last clicked bag slot (double-click equip tracking only).
         * @param slot Absolute bag slot index, or -1.
         */
        void set_selected(int slot) {
            m_last_click_slot = slot;
        }

        /**
         * Sets whether a register page accepts deposits.
         * @param page Register page index.
         * @param enabled True when the page is writable.
         */
        void set_register_enabled(int page, bool enabled) {
            if (page >= 0 && page < kMaxRegisters) {
                m_register_enabled[static_cast<std::size_t>(page)] = enabled;
            }
        }

        /**
         * Replaces the full register writability mask.
         * @param enabled Per-register flags (length kMaxRegisters).
         */
        void set_register_enabled(const std::array<bool, kMaxRegisters>& enabled) {
            m_register_enabled = enabled;
        }

        /**
         * Updates timed unlock expiries for the six inventory registers.
         * Registers 1 and 2 remain permanently available; registers 3 through 6
         * are enabled while their corresponding expiry is in the future.
         * @param expire_ms Per-register steady-clock expiry timestamps.
         */
        void
        set_register_unlock_expire_ms(const std::array<std::uint64_t, kMaxRegisters>& expire_ms);

        /**
         * Tests whether a register page accepts deposits.
         * @param page Register page index.
         * @return True when the page is writable.
         */
        [[nodiscard]] bool register_enabled(int page) const {
            return page >= 0 && page < kMaxRegisters &&
                   m_register_enabled[static_cast<std::size_t>(page)];
        }

        /** @return Per-register writability flags. */
        [[nodiscard]] const std::array<bool, kMaxRegisters>& register_enabled() const {
            return m_register_enabled;
        }

        /** @return Bag slot under the cursor for tooltips, or -1. */
        [[nodiscard]] int hover_slot() const {
            return m_hover_slot;
        }

        /** @return Client box left in pixels. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Client box top in pixels. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return Client box width in pixels. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @return Client box height in pixels. */
        [[nodiscard]] float h() const {
            return m_h;
        }

        /**
         * Moves the window.
         * @param x Client box left in pixels.
         * @param y Client box top in pixels.
         */
        void set_pos(float x, float y) {
            m_x = x;
            m_y = y;
        }

        /**
         * Sets the client box left.
         * @param x Left in pixels.
         */
        void set_x(float x) {
            m_x = x;
        }

        /**
         * Sets the client box top.
         * @param y Top in pixels.
         */
        void set_y(float y) {
            m_y = y;
        }

        /** @return Display scale (1 = native JSON units). */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the display scale. Call sync_from_layout() afterwards to resize the
         * client box.
         * @param scale Display scale (1 = native JSON units).
         */
        void set_scale(float scale) {
            m_requested_scale = std::max(0.25f, scale);
            m_scale = m_requested_scale;
        }

        /** Reads a General filter toggle.
         * @param category Zero-based filter button index (0..5).
         * @return True when the category is selected.
         */
        [[nodiscard]] bool filter_active(int category) const;

        /** Resolves a filtered General display cell to its underlying bag slot.
         * @param display_slot Absolute display cell (0..215).
         * @return Underlying bag slot, or -1 for a filtered-out cell.
         */
        [[nodiscard]] int general_source_slot(int display_slot) const;

        /**
         * Binds the item catalog read for icons and equip rules.
         * @param catalog Catalog; must outlive the window. Pass nullptr to unbind.
         */
        void set_catalog(const ItemDb* catalog) {
            m_catalog = catalog;
        }

        /**
         * Binds shinsu stage names used for grown-egg tooltips.
         * @param shinsu Catalog; must outlive the window. Pass nullptr to unbind.
         */
        void set_shinsu(const ShinsuDb* shinsu) {
            m_shinsu = shinsu;
        }

        /** @return Bound item catalog, or nullptr. */
        [[nodiscard]] const ItemDb* catalog() const {
            return m_catalog;
        }

        /**
         * Binds the equipped wear snapshot used for paperdoll and equip markers.
         * @param wear Wear; must outlive the window. Pass nullptr to unbind.
         */
        void set_wear(const Wear* wear) {
            m_wear = wear;
        }

        /** Binds Battle Riding gear used for inventory equip markers.
         * @param equipment_iids Authoritative IDs; must outlive the window, or nullptr.
         */
        void set_riding_equipment(const std::array<std::uint32_t, 5>* equipment_iids) {
            m_riding_equipment_iids = equipment_iids;
        }

        /** @return Bound wear snapshot, or nullptr. */
        [[nodiscard]] const Wear* wear() const {
            return m_wear;
        }

        /**
         * Binds the layout document holding chrome and geometry.
         * @param layout Layout; must outlive the window. Pass nullptr for baked
         * defaults.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** @return Bound layout document, or nullptr. */
        [[nodiscard]] const UiLayout* layout() const {
            return m_layout;
        }

        /** @return Player class index used for equip/tooltip gating. */
        [[nodiscard]] int player_clazz() const {
            return m_player_clazz;
        }

        /**
         * Sets the player class index.
         * @param clazz Class index.
         */
        void set_player_clazz(int clazz) {
            m_player_clazz = clazz;
        }

        /** @return Player level used for equip/tooltip gating. */
        [[nodiscard]] int player_level() const {
            return m_player_level;
        }

        /**
         * Sets the player level.
         * @param level Character level.
         */
        void set_player_level(int level) {
            m_player_level = level;
        }

        /**
         * Sets worn-egg level/EXP used by transform hover tips.
         * @param index Equipped egg item definition index, or 0.
         * @param level Animal level.
         * @param exp Current EXP toward the next level.
         */
        void set_transform_egg(int index, int level, int exp) {
            m_transform_egg_index = index;
            m_transform_egg_level = level < 1 ? 1 : level;
            m_transform_egg_exp = exp < 0 ? 0 : exp;
        }

        /** Replaces pet tooltip definitions loaded from config/pets/catalog.json.
         * @param info Definitions keyed by item definition index.
         */
        void set_pet_tooltip_info(std::unordered_map<int, PetTooltipInfo> info) {
            m_pet_tooltip_info = std::move(info);
        }

        /** Updates the authoritative expiry of one pet or riding item instance.
         * @param iid Inventory instance id.
         * @param expire_at Unix timestamp, or 0 for permanent/unset.
         */
        void set_item_expire_at(std::uint32_t iid, std::uint64_t expire_at) {
            if (iid == 0) {
                return;
            }
            if (expire_at == 0) {
                m_item_expire_at.erase(iid);
            } else {
                m_item_expire_at[iid] = expire_at;
            }
        }

        /** @return Last equip network op after EquipChanged, or None. */
        [[nodiscard]] EquipNetOp last_equip_op() const {
            return m_last_equip_op;
        }

        /** @return item definition index of the last equip toggle. */
        [[nodiscard]] std::int32_t last_equip_index() const {
            return m_last_equip_index;
        }

        /** @return Inventory instance id of the last equip toggle, or 0. */
        [[nodiscard]] std::uint32_t last_equip_iid() const {
            return m_last_equip_iid;
        }

        /** @return Talisman id of the last enchant drop, or 0. */
        [[nodiscard]] std::uint32_t last_enchant_from_id() const {
            return m_last_enchant_from_id;
        }

        /** @return Destination gear id of the last enchant drop, or 0. */
        [[nodiscard]] std::uint32_t last_enchant_to_id() const {
            return m_last_enchant_to_id;
        }

        /** @return Absolute bag slot of the last enchant talisman, or -1. */
        [[nodiscard]] int last_enchant_from_slot() const {
            return m_last_enchant_from_slot;
        }

        /** @return Absolute bag slot of the last enchant destination, or -1. */
        [[nodiscard]] int last_enchant_to_slot() const {
            return m_last_enchant_to_slot;
        }

        /** @return Instance id of the last inventory drop onto the world, or 0. */
        [[nodiscard]] std::uint32_t last_drop_iid() const {
            return m_last_drop_iid;
        }

        /** @return Stack count of the last inventory drop onto the world. */
        [[nodiscard]] int last_drop_count() const {
            return m_last_drop_count;
        }

        /** @return Instance id of the last drag-to-destroy, or 0. */
        [[nodiscard]] std::uint32_t last_destroyed_iid() const {
            return m_last_destroyed_iid;
        }

        /** @return Absolute bag slot last destroyed by drag-to-destroy, or -1. */
        [[nodiscard]] int last_destroyed_slot() const {
            return m_last_destroyed_slot;
        }

        /** @return Absolute bag slot last consumed by right-click, or -1. */
        [[nodiscard]] int last_consumed_slot() const {
            return m_last_consumed_slot;
        }

        /** @return Exact instance selected for authoritative use, or zero. */
        [[nodiscard]] std::uint32_t last_consumed_iid() const {
            return m_last_consumed_iid;
        }

        /** @return Item definition index of the last consumed stack. */
        [[nodiscard]] std::int32_t last_consumed_index() const {
            return m_last_consumed_index;
        }

        /** @return City teleport loc from the last consumed scroll, or -1. */
        [[nodiscard]] int last_teleport_loc() const {
            return m_last_teleport_loc;
        }

        /** @return HP restore amount from the last consumed refresh item. */
        [[nodiscard]] int last_refresh_hp() const {
            return m_last_refresh_hp;
        }

        /** @return MP restore amount from the last consumed refresh item. */
        [[nodiscard]] int last_refresh_mp() const {
            return m_last_refresh_mp;
        }

        /** @return Absolute bag slot currently dragged, or -1. */
        [[nodiscard]] int drag_slot() const {
            return m_drag_slot;
        }

        /** @return True while a bag press, item drag or scrollbar drag is held. */
        [[nodiscard]] bool capturing_pointer() const {
            return m_scrolling || m_press_slot >= 0 || m_drag_slot >= 0;
        }

        /**
         * Sets the absolute bag slot currently dragged.
         * @param slot Absolute bag slot index, or -1 to clear.
         */
        void set_drag_slot(int slot) {
            m_drag_slot = slot;
            if (slot >= 0) {
                m_source_tab = m_tab;
            }
        }

        /** @return Storage tab captured when the current item drag began. */
        [[nodiscard]] Tab drag_source_tab() const {
            return m_source_tab;
        }

        /** @return item definition index of the dragged stack. */
        [[nodiscard]] std::int32_t drag_index() const {
            return m_drag_index;
        }

        /** @return Count of the dragged stack. */
        [[nodiscard]] int drag_count() const {
            return m_drag_count;
        }

        /** @return True while the window frame itself is being dragged. */
        [[nodiscard]] bool dragging_window() const {
            return m_dragging_window;
        }

        /** Applies window.w / window.h and register flags from the layout. */
        void sync_from_layout();

        /**
         * Loads or reloads FreeType faces from the layout font sizes.
         * @param engine Engine used to create atlas textures.
         */
        void sync_fonts(engine::IEngine& engine);

        /**
         * Releases owned FreeType atlas textures.
         * @param engine Engine that owns the textures.
         */
        void destroy_fonts(engine::IEngine& engine);

        /**
         * Resizes from the layout, keeping the current position.
         * @param screen_w Framebuffer width (unused; kept for call-site symmetry).
         * @param screen_h Framebuffer height (unused; kept for call-site symmetry).
         */
        void layout_size(float screen_w, float screen_h);

        /** Sorts presentation after the server committed stack consolidation.
         * @param bag Updated authoritative inventory.
         * @param quest True for Quest Items, false for General.
         */
        void complete_sort(Bag& bag, bool quest);

        /**
         * Resizes from the layout and centers the window on screen.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /**
         * @param bag Player bag.
         * @return Mutable slots for the active tab.
         */
        std::vector<ItemStack>& active_slots(Bag& bag) {
            return m_tab == Tab::Quest ? bag.quest : bag.normal;
        }

        /**
         * @param bag Player bag.
         * @return Const slots for the active tab.
         */
        const std::vector<ItemStack>& active_slots(const Bag& bag) const {
            return m_tab == Tab::Quest ? bag.quest : bag.normal;
        }

        /** @return Writable slot capacity across enabled registers. */
        [[nodiscard]] int slots_capacity() const;

        /**
         * Counts occupied stacks on the active tab.
         * @param bag Player bag.
         * @return Number of non-empty stacks.
         */
        [[nodiscard]] int slots_used(const Bag& bag) const;

        /** @return Zero; General now displays every storage block at once. */
        [[nodiscard]] int page_slot_base() const {
            return 0;
        }

        /**
         * Tests whether a register page accepts deposits.
         * @param page Register page index.
         * @return True when the page is enabled.
         */
        [[nodiscard]] bool register_writable(int page) const;

        /**
         * Tests whether a register page still holds any stacks.
         * @param bag Player bag.
         * @param page Register page index.
         * @return True when any non-empty stack is present.
         */
        [[nodiscard]] bool register_page_has_items(const Bag& bag, int page) const;

        /**
         * Tests whether a register page can be selected (writable or still holding
         * items).
         * @param bag Player bag.
         * @param page Register page index.
         * @return True when the page is viewable.
         */
        [[nodiscard]] bool register_viewable(const Bag& bag, int page) const;

        /**
         * Resolves the icon rect for a visible grid cell.
         * @param visible_index Absolute cell within General (0–215) or Other (0–71).
         * @param sx Out left.
         * @param sy Out top.
         * @param sw Out width.
         * @param sh Out height.
         * @return True when visible_index is valid.
         */
        [[nodiscard]] bool
        grid_slot_rect(int visible_index, float& sx, float& sy, float& sw, float& sh) const;

        /**
         * Hits the bag grid under the pointer.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return Absolute bag slot index, or -1.
         */
        [[nodiscard]] int grid_slot_at(float mx, float my) const;

        /**
         * Resolves the destroy drop target rect.
         * @param sx Out left.
         * @param sy Out top.
         * @param sw Out width.
         * @param sh Out height.
         * @return True when the drop target exists in the layout.
         */
        [[nodiscard]] bool destroy_rect(float& sx, float& sy, float& sw, float& sh) const;

        /**
         * Resolves the bottom exit button rect.
         * @param sx Out left.
         * @param sy Out top.
         * @param sw Out width.
         * @param sh Out height.
         * @return True when the close control exists in the layout.
         */
        [[nodiscard]] bool exit_bottom_rect(float& sx, float& sy, float& sw, float& sh) const;

        /**
         * Tests whether the pointer is over the framed window or wear columns.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when the window is open and under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Applies one frame of pointer input: window dragging and slot hover for the
         * tooltip.
         * @param in Frame input.
         * @param bag Player bag used for hover content checks.
         */
        void update_pointer(const engine::FrameInput& in, Bag& bag);

        /**
         * Left-click: start a potential drag or arm double-click equip on a bag slot.
         * @param mx Cursor X in framebuffer pixels.
         * @param my Cursor Y in framebuffer pixels.
         * @param bag Player bag.
         * @param wear_out Unused; kept for API stability.
         * @return Click result.
         */
        ClickResult handle_click(float mx, float my, Bag& bag, Wear& wear_out);

        /**
         * Right-click: consume a usable stack (teleport / refresh), else no-op.
         * @param mx Cursor X in framebuffer pixels.
         * @param my Cursor Y in framebuffer pixels.
         * @param bag Player bag.
         * @return Consumed, Handled, or Ignored.
         */
        ClickResult handle_right_click(float mx, float my, Bag& bag);

        /**
         * Completes a bag press: drag swap/destroy, or toggle-equip on a second click.
         * @param mx Cursor X in framebuffer pixels.
         * @param my Cursor Y in framebuffer pixels.
         * @param bag Player bag mutated on swap/equip.
         * @param wear_out Equipped wear mutated when toggling gear.
         * @return Destroyed, EquipChanged, DroppedOnWorld, Handled, or Ignored.
         */
        ClickResult handle_release(float mx, float my, Bag& bag, Wear& wear_out);

        /**
         * Applies the last drag-to-destroy locally (offline / UI preview).
         * @param bag Player bag mutated on success.
         * @param wear Equipped wear; worn items are taken off when destroyed.
         */
        void apply_last_destroy(Bag& bag, Wear* wear = nullptr);

        /**
         * Draws the inventory window chrome and contents (no hover tip).
         * @param ui UI asset helper.
         * @param engine Engine used for quads.
         * @param bag Player bag contents.
         * @param cooldowns Active item-use cooldowns, if available.
         */
        void draw(UiAssets& ui,
                  engine::IEngine& engine,
                  const Bag& bag,
                  const ItemCooldowns* cooldowns = nullptr) const;

        /**
         * Draws the hover item tip on top of the soft cursor.
         * @param ui UI asset helper.
         * @param engine Engine used for quads.
         * @param bag Player bag contents.
         * @param tip_font Optional FreeType face for MakeTip-style hover text.
         * @param screen_w Framebuffer width for tip clamping.
         * @param screen_h Framebuffer height for tip clamping.
         */
        void draw_tooltip(UiAssets& ui,
                          engine::IEngine& engine,
                          const Bag& bag,
                          const FreetypeFont* tip_font,
                          float screen_w,
                          float screen_h) const;

        /**
         * Draws the dragged bag icon under the cursor.
         * @param ui UI asset helper.
         * @param mx Pointer X.
         * @param my Pointer Y.
         */
        void draw_drag_ghost(UiAssets& ui, float mx, float my) const;

      private:
        /** Rebuilds the compact display without changing item storage or lock domains.
         * @param bag Current inventory contents.
         */
        void refresh_general_projection(const Bag& bag) const;

        /** @return Scroll viewport for the active inventory. */
        [[nodiscard]] const UiWidget* active_viewport() const;

        /** @return Current vertical offset in grid rows for the active inventory. */
        [[nodiscard]] int scroll_row() const;

        /** @return Maximum scroll offset for the active inventory. */
        [[nodiscard]] int max_scroll_row() const;

        /** Changes the active vertical scroll position.
         * @param row Requested row offset, clamped to the content.
         */
        void set_scroll_row(int row);

        /** Tests the inventory viewport and its scroll controls.
         * @param mx Screen X.
         * @param my Screen Y.
         * @return True inside the scroll area.
         */
        [[nodiscard]] bool scroll_area_hit(float mx, float my) const;

        /** Maps the scrollbar pointer to a row offset.
         * @param my Screen Y.
         */
        void scroll_to_pointer(float my);

        /** Builds layout text and state bindings.
         * @param bag Player bag.
         * @return Variables for the shared UI renderer.
         */
        [[nodiscard]] UiVarMap make_vars(const Bag& bag) const;

        /** @return Frame chrome style derived from the bound layout. */
        WindowFrameStyle frame_style() const;

        /**
         * Draws panel background and optional chrome pieces.
         * @param ui UI asset helper.
         */
        void draw_chrome(UiAssets& ui) const;

        /** Reloads runtime state from the layout registers.enabled array. */
        void apply_register_enabled_from_layout();

        /**
         * Resolves one wear-column slot rect.
         * @param left True for the left (armor) column.
         * @param index Slot index within the column.
         * @param sx Out left.
         * @param sy Out top.
         * @param sw Out width.
         * @param sh Out height.
         * @return True when index is valid for the column.
         */
        [[nodiscard]] bool
        wear_column_rect(int left, int index, float& sx, float& sy, float& sw, float& sh) const;

        /**
         * Tests whether the pointer hits either wear column.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True on a wear-slot hit.
         */
        [[nodiscard]] bool wear_columns_hit(float mx, float my) const;

        /** True while the window is shown. */
        bool m_open = false;

        /** Active content tab. */
        Tab m_tab = Tab::Normal;

        /** Current register page index. */
        int m_register_page = 0;

        /** Per-register writability (first two always on after layout sync). */
        std::array<bool, kMaxRegisters> m_register_enabled{true, true, false, false, false, false};

        /** Timed unlock expiries for registers 3 through 6 (steady-clock milliseconds). */
        std::array<std::uint64_t, kMaxRegisters> m_register_unlock_expire_ms{};

        /** Last clicked bag slot for double-click equip detection, or -1. */
        int m_last_click_slot = -1;

        /** Steady ms timestamp of the last bag-slot click. */
        std::uint64_t m_last_click_ms = 0;

        /** Last clicked wear-slot index for double-click unequip detection, or -1. */
        int m_last_wear_click_slot = -1;

        /** Steady ms timestamp of the last wear-slot click. */
        std::uint64_t m_last_wear_click_ms = 0;

        /** Earliest ms (steady) each iid may be worn again after a local unequip. */
        std::unordered_map<std::uint32_t, std::uint64_t> m_putoff_cooldown_ms;

        /** Bag slot under the cursor for tooltips, or -1. */
        int m_hover_slot = -1;

        /** Pointer X used to place the hover tip. */
        float m_hover_mx = 0;

        /** Pointer Y used to place the hover tip. */
        float m_hover_my = 0;

        /** Stateful control under the pointer, or empty. */
        std::string m_hovered_button;

        /** Whether the primary pointer button is down. */
        bool m_pointer_down = false;

        /** Register tab under the cursor for a timed-unlock tooltip, or -1. */
        int m_hover_register = -1;

        /** Client box left in pixels. */
        float m_x = 0;

        /** Client box top in pixels. */
        float m_y = 0;

        /** Client box width in pixels. */
        float m_w = 245;

        /** Client box height in pixels. */
        float m_h = 417;

        /** Bound item catalog, or nullptr. */
        const ItemDb* m_catalog = nullptr;

        /** Bound shinsu name table, or nullptr. */
        const ShinsuDb* m_shinsu = nullptr;

        /** Bound wear snapshot, or nullptr. */
        const Wear* m_wear = nullptr;

        /** Bound layout document, or nullptr. */
        const UiLayout* m_layout = nullptr;

        /** Selected category bits; zero displays all categories. */
        unsigned m_filter_mask = 0;
        /** Display cells resolve back to immutable source slots for all item actions. */
        mutable std::array<int, kBagSlots> m_general_projection{};

        /** Independent General and Other scroll positions, in native grid rows. */
        std::array<int, 2> m_scroll_rows{};
        /** True while dragging the vertical scrollbar. */
        bool m_scrolling = false;
        /** Dedicated Collection layout, restored beside the compact inventory. */
        UiLayout m_collection_layout;
        UiDocument m_collection_document;
        bool m_collection_open = false;
        std::uint64_t m_collection_animation_ms = 0;
        float m_collection_animation_from = 0;

        /** @return Animated Collection opening fraction. */
        [[nodiscard]] float collection_progress() const;

        /** @return Collection client X in screen pixels. */
        [[nodiscard]] float collection_x() const;

        /** @return Collection client Y in screen pixels. */
        [[nodiscard]] float collection_y() const;

        /** @return Collection display scale fitted to the viewport. */
        [[nodiscard]] float collection_scale() const;

        /** True when a collection slot supplies the current hover item. */
        bool m_collection_hover = false;
        /** Explicitly selected bag Taegeuk for the absorption action. */
        std::uint32_t m_collection_selected_iid = 0;
        float m_screen_w = 0;
        float m_screen_h = 0;

        /** Resolves a visible Collection drawer cell at a point.
         * @param mx Screen X.
         * @param my Screen Y.
         * @return Cell index, or -1.
         */
        [[nodiscard]] int collection_slot_at(float mx, float my) const;

        /** Parsed openkal.ui/v1 document. */
        UiDocument m_v1;

        /** Display scale (1 = native JSON units). Matches skill window / HUD zoom. */
        float m_scale = 1.0f;
        /** Preferred zoom before fitting the complete dialog to the viewport. */
        float m_requested_scale = 1.0f;
        /** Storage captured on item press, retained while dragging across tabs. */
        Tab m_source_tab = Tab::Normal;
        /** Tab currently targeted by drag hover. */
        Tab m_drag_hover_tab = Tab::Normal;
        /** Start of the current tab hover, in steady milliseconds. */
        std::uint64_t m_drag_hover_ms = 0;

        /** Player class index for equip/tooltip gating. */
        int m_player_clazz = 0;

        /** Riding gear markers are separate from character wear and its stat bonuses. */
        const std::array<std::uint32_t, 5>* m_riding_equipment_iids = nullptr;

        /** Player level for equip/tooltip gating. */
        int m_player_level = 1;

        /** Equipped egg item definition index for transform tips, or 0. */
        int m_transform_egg_index = 0;

        /** Equipped egg level for transform tips. */
        int m_transform_egg_level = 1;

        /** Equipped egg EXP for transform tips. */
        int m_transform_egg_exp = 0;

        /** Tooltip behavior definitions keyed by pet item definition index. */
        std::unordered_map<int, PetTooltipInfo> m_pet_tooltip_info;

        /** Authoritative expiry timestamps keyed by pet or riding item instance id. */
        std::unordered_map<std::uint32_t, std::uint64_t> m_item_expire_at;

        /** Last equip network op after EquipChanged. */
        EquipNetOp m_last_equip_op = EquipNetOp::None;

        /** item definition index of the last equip toggle. */
        std::int32_t m_last_equip_index = 0;

        /** Inventory instance id of the last equip toggle, or 0. */
        std::uint32_t m_last_equip_iid = 0;

        /** Talisman id of the last enchant drop, or 0. */
        std::uint32_t m_last_enchant_from_id = 0;

        /** Destination gear id of the last enchant drop, or 0. */
        std::uint32_t m_last_enchant_to_id = 0;

        /** Absolute bag slot of the last enchant talisman, or -1. */
        int m_last_enchant_from_slot = -1;

        /** Absolute bag slot of the last enchant destination, or -1. */
        int m_last_enchant_to_slot = -1;

        /** Inventory instance id of the last world drop, or 0. */
        std::uint32_t m_last_drop_iid = 0;

        /** Stack count of the last world drop. */
        int m_last_drop_count = 0;

        /** Inventory instance id of the last drag-to-destroy, or 0. */
        std::uint32_t m_last_destroyed_iid = 0;

        /** Absolute bag slot last destroyed, or -1. */
        int m_last_destroyed_slot = -1;

        /** Absolute bag slot last consumed, or -1. */
        int m_last_consumed_slot = -1;

        /** item definition index of the last consumed stack. */
        std::int32_t m_last_consumed_index = 0;

        /** Instance selected by the latest consumable right-click. */
        std::uint32_t m_last_consumed_iid = 0;

        /** City teleport loc from the last consumed scroll, or -1. */
        int m_last_teleport_loc = -1;

        /** HP restore from the last consumed refresh item. */
        int m_last_refresh_hp = 0;

        /** MP restore from the last consumed refresh item. */
        int m_last_refresh_mp = 0;

        /** Absolute bag slot currently dragged, or -1. */
        int m_drag_slot = -1;

        /** Slot captured on LMB down before the drag threshold, or -1. */
        int m_press_slot = -1;

        /** True when releasing this press without a drag should toggle-equip. */
        bool m_arm_equip = false;

        /** Pointer X when the bag slot was pressed. */
        float m_press_mx = 0;

        /** Pointer Y when the bag slot was pressed. */
        float m_press_my = 0;

        /** item definition index of the dragged stack. */
        std::int32_t m_drag_index = 0;

        /** Protection state captured for the dragged instance. */
        bool m_drag_enchant_protected = false;

        /** Binding flags retained while the source cell is hidden during dragging. */
        bool m_drag_account_bound = false;
        bool m_drag_character_bound = false;

        /** Count of the dragged stack. */
        int m_drag_count = 0;

        /** Animal level of the dragged egg (1 for non-eggs). */
        int m_drag_egg_level = 1;

        /** True while the window frame itself is being dragged. */
        bool m_dragging_window = false;

        /** Pointer offset from runtime state while dragging the window. */
        float m_drag_dx = 0;

        /** Pointer offset from runtime state while dragging the window. */
        float m_drag_dy = 0;

        /** Body FreeType face for chrome labels. */
        mutable FreetypeFont m_body_font;

        /** Pixel size last loaded into runtime state. */
        mutable int m_body_font_pixel_size = 0;

        /** Title FreeType face; body text uses the separate regular face. */
        mutable FreetypeFont m_title_font;

        /** Pixel size last loaded into runtime state. */
        mutable int m_title_font_pixel_size = 0;
    };

}  // namespace openkal::client
