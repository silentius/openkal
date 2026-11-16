/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/CollectionEquipment.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cmath>

namespace openkal::server {
    namespace {
        constexpr int kTriagramStats[24][15] = {
            {26, 32, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92, 98, 104, 110},
            {32, 43, 53, 64, 75, 86, 96, 107, 118, 129, 139, 150, 161, 172, 182},
            {42, 60, 77, 95, 112, 130, 147, 165, 182, 200, 217, 235, 252, 270, 287},
            {9, 12, 14, 16, 18, 21, 23, 25, 27, 30, 32, 34, 36, 39, 41},
            {15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85},
            {21, 30, 38, 47, 56, 65, 73, 82, 91, 100, 108, 117, 126, 135, 143},
            {1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8},
            {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17},
            {6, 9, 12, 15, 18, 20, 23, 26, 29, 32, 34, 37, 40, 43, 46},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
            {1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4},
            {1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
            {1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4},
            {1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
            {1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4},
            {1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
            {1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4},
            {1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
            {1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4},
            {1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}};
        constexpr int kYinYangRate[3][15] = {
            {10, 13, 16, 20, 25, 32, 40, 50, 64, 80, 101, 127, 160, 202, 254},
            {10, 13, 17, 21, 28, 36, 46, 59, 77, 100, 129, 166, 215, 277, 357},
            {10, 13, 17, 22, 29, 39, 51, 66, 87, 114, 149, 195, 255, 335, 438}};
        constexpr int kTaegeukPrefix[184][8] = {
            {100, 0, 0, 0, 0, 0, 0, 0},  {0, 36, 0, 0, 0, 0, 0, 0},   {0, 0, 6, 0, 0, 0, 0, 0},
            {0, 0, 0, 2, 0, 0, 0, 0},    {0, 0, 0, 0, 2, 0, 0, 0},    {0, 0, 0, 0, 0, 2, 0, 0},
            {0, 0, 0, 0, 0, 0, 2, 0},    {0, 0, 0, 0, 0, 0, 0, 2},    {150, 0, 0, 0, 0, 0, 0, 0},
            {0, 70, 0, 0, 0, 0, 0, 0},   {0, 0, 14, 0, 0, 0, 0, 0},   {0, 0, 0, 2, 0, 0, 0, 0},
            {0, 0, 0, 0, 2, 0, 0, 0},    {0, 0, 0, 0, 0, 2, 0, 0},    {0, 0, 0, 0, 0, 0, 2, 0},
            {0, 0, 0, 0, 0, 0, 0, 2},    {224, 0, 0, 0, 0, 0, 0, 0},  {0, 112, 0, 0, 0, 0, 0, 0},
            {0, 0, 36, 0, 0, 0, 0, 0},   {0, 0, 0, 4, 0, 0, 0, 0},    {0, 0, 0, 0, 4, 0, 0, 0},
            {0, 0, 0, 0, 0, 4, 0, 0},    {0, 0, 0, 0, 0, 0, 4, 0},    {0, 0, 0, 0, 0, 0, 0, 4},
            {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},    {160, 0, 0, 0, 0, 0, 0, 0},
            {0, 60, 0, 0, 0, 0, 0, 0},   {0, 0, 12, 0, 0, 0, 0, 0},   {0, 0, 0, 4, 0, 0, 0, 0},
            {0, 0, 0, 0, 4, 0, 0, 0},    {0, 0, 0, 0, 0, 4, 0, 0},    {0, 0, 0, 0, 0, 0, 4, 0},
            {0, 0, 0, 0, 0, 0, 0, 4},    {258, 0, 0, 0, 0, 0, 0, 0},  {0, 120, 0, 0, 0, 0, 0, 0},
            {0, 0, 24, 0, 0, 0, 0, 0},   {0, 0, 0, 6, 0, 0, 0, 0},    {0, 0, 0, 0, 6, 0, 0, 0},
            {0, 0, 0, 0, 0, 6, 0, 0},    {0, 0, 0, 0, 0, 0, 6, 0},    {0, 0, 0, 0, 0, 0, 0, 6},
            {400, 0, 0, 0, 0, 0, 0, 0},  {0, 200, 0, 0, 0, 0, 0, 0},  {0, 0, 64, 0, 0, 0, 0, 0},
            {0, 0, 0, 10, 0, 0, 0, 0},   {0, 0, 0, 0, 10, 0, 0, 0},   {0, 0, 0, 0, 0, 10, 0, 0},
            {0, 0, 0, 0, 0, 0, 10, 0},   {0, 0, 0, 0, 0, 0, 0, 10},   {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},    {220, 0, 0, 0, 0, 0, 0, 0},  {0, 84, 0, 0, 0, 0, 0, 0},
            {0, 0, 18, 0, 0, 0, 0, 0},   {0, 0, 0, 6, 0, 0, 0, 0},    {0, 0, 0, 0, 6, 0, 0, 0},
            {0, 0, 0, 0, 0, 6, 0, 0},    {0, 0, 0, 0, 0, 0, 6, 0},    {0, 0, 0, 0, 0, 0, 0, 6},
            {366, 0, 0, 0, 0, 0, 0, 0},  {0, 170, 0, 0, 0, 0, 0, 0},  {0, 0, 34, 0, 0, 0, 0, 0},
            {0, 0, 0, 10, 0, 0, 0, 0},   {0, 0, 0, 0, 10, 0, 0, 0},   {0, 0, 0, 0, 0, 10, 0, 0},
            {0, 0, 0, 0, 0, 0, 10, 0},   {0, 0, 0, 0, 0, 0, 0, 10},   {556, 0, 0, 0, 0, 0, 0, 0},
            {0, 228, 0, 0, 0, 0, 0, 0},  {0, 0, 92, 0, 0, 0, 0, 0},   {0, 0, 0, 16, 0, 0, 0, 0},
            {0, 0, 0, 0, 16, 0, 0, 0},   {0, 0, 0, 0, 0, 16, 0, 0},   {0, 0, 0, 0, 0, 0, 16, 0},
            {0, 0, 0, 0, 0, 0, 0, 16},   {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},    {0, 0, 0, 0, 0, 0, 0, 0},
            {100, 0, 6, 0, 0, 0, 0, 0},  {0, 36, 0, 0, 0, 2, 0, 0},   {0, 0, 6, 2, 0, 0, 0, 0},
            {0, 0, 0, 2, 0, 0, 0, 2},    {0, 0, 0, 2, 2, 0, 0, 0},    {0, 0, 6, 0, 0, 2, 0, 0},
            {0, 0, 0, 0, 0, 2, 2, 0},    {100, 0, 0, 0, 0, 0, 0, 2},  {150, 0, 14, 0, 0, 0, 0, 0},
            {0, 70, 0, 0, 0, 2, 0, 0},   {0, 0, 14, 2, 0, 0, 0, 0},   {0, 0, 0, 2, 0, 0, 0, 2},
            {0, 0, 0, 2, 2, 0, 0, 0},    {0, 0, 14, 0, 0, 2, 0, 0},   {0, 0, 0, 0, 0, 2, 2, 0},
            {150, 0, 0, 0, 0, 0, 0, 2},  {224, 0, 36, 0, 0, 0, 0, 0}, {0, 112, 0, 0, 0, 4, 0, 0},
            {0, 0, 36, 4, 0, 0, 0, 0},   {0, 0, 0, 4, 0, 0, 0, 4},    {0, 0, 0, 4, 4, 0, 0, 0},
            {0, 0, 36, 0, 0, 4, 0, 0},   {0, 0, 0, 0, 0, 4, 4, 0},    {224, 0, 0, 0, 0, 0, 0, 4},
            {160, 0, 12, 0, 0, 0, 0, 0}, {0, 60, 0, 0, 0, 4, 0, 0},   {0, 0, 12, 4, 0, 0, 0, 0},
            {0, 0, 0, 4, 0, 0, 0, 4},    {0, 0, 0, 4, 4, 0, 0, 0},    {0, 0, 12, 0, 0, 4, 0, 0},
            {0, 0, 0, 0, 0, 4, 4, 0},    {160, 0, 0, 0, 0, 0, 0, 4},  {258, 0, 24, 0, 0, 0, 0, 0},
            {0, 120, 0, 0, 0, 6, 0, 0},  {0, 0, 24, 6, 0, 0, 0, 0},   {0, 0, 0, 6, 0, 0, 0, 6},
            {0, 0, 0, 6, 6, 0, 0, 0},    {0, 0, 24, 0, 0, 6, 0, 0},   {0, 0, 0, 0, 0, 6, 6, 0},
            {258, 0, 0, 0, 0, 0, 0, 6},  {400, 0, 64, 0, 0, 0, 0, 0}, {0, 200, 0, 0, 0, 10, 0, 0},
            {0, 0, 64, 10, 0, 0, 0, 0},  {0, 0, 0, 10, 0, 0, 0, 10},  {0, 0, 0, 10, 10, 0, 0, 0},
            {0, 0, 64, 0, 0, 10, 0, 0},  {0, 0, 0, 0, 0, 10, 10, 0},  {400, 0, 0, 0, 0, 0, 0, 10},
            {800, 0, 0, 0, 0, 0, 0, 0},  {0, 400, 0, 0, 0, 0, 0, 0},  {0, 0, 128, 0, 0, 0, 0, 0},
            {0, 0, 0, 20, 0, 0, 0, 0},   {0, 0, 0, 0, 20, 0, 0, 0},   {0, 0, 0, 0, 0, 20, 0, 0},
            {0, 0, 0, 0, 0, 0, 20, 0},   {0, 0, 0, 0, 0, 0, 0, 20},   {220, 0, 18, 0, 0, 0, 0, 0},
            {0, 84, 0, 0, 0, 6, 0, 0},   {0, 0, 18, 6, 0, 0, 0, 0},   {0, 0, 0, 6, 0, 0, 0, 6},
            {0, 0, 0, 6, 6, 0, 0, 0},    {0, 0, 18, 0, 0, 6, 0, 0},   {0, 0, 0, 0, 0, 6, 6, 0},
            {220, 0, 0, 0, 0, 0, 0, 6},  {366, 0, 34, 0, 0, 0, 0, 0}, {0, 170, 0, 0, 0, 10, 0, 0},
            {0, 0, 34, 10, 0, 0, 0, 0},  {0, 0, 0, 10, 0, 0, 0, 10},  {0, 0, 0, 10, 10, 0, 0, 0},
            {0, 0, 34, 0, 0, 10, 0, 0},  {0, 0, 0, 0, 0, 10, 10, 0},  {366, 0, 0, 0, 0, 0, 0, 10},
            {576, 0, 92, 0, 0, 0, 0, 0}, {0, 288, 0, 0, 0, 16, 0, 0}, {0, 0, 92, 16, 0, 0, 0, 0},
            {0, 0, 0, 16, 0, 0, 0, 16},  {0, 0, 0, 16, 16, 0, 0, 0},  {0, 0, 92, 0, 0, 16, 0, 0},
            {0, 0, 0, 0, 0, 16, 16, 0},  {576, 0, 0, 0, 0, 0, 0, 16}, {1200, 0, 0, 0, 0, 0, 0, 0},
            {0, 600, 0, 0, 0, 0, 0, 0},  {0, 0, 192, 0, 0, 0, 0, 0},  {0, 0, 0, 30, 0, 0, 0, 0},
            {0, 0, 0, 0, 30, 0, 0, 0},   {0, 0, 0, 0, 0, 30, 0, 0},   {0, 0, 0, 0, 0, 0, 30, 0},
            {0, 0, 0, 0, 0, 0, 0, 30}};

    }  // namespace

    CollectionDef collection_def(int index) {
        if (index >= kYinYangMirrorFirst && index < kYinYangMirrorFirst + 3) {
            return {CollectionKind::Mirror, index - kYinYangMirrorFirst + 1};
        }
        if (index >= kRawTrigramFirst && index < kRawTrigramFirst + 24) {
            return {CollectionKind::Trigram,
                    (index - kRawTrigramFirst) / 8 + 1,
                    (index - kRawTrigramFirst) % 8,
                    true};
        }
        if (index >= kTrigramFirst && index < kTrigramFirst + 24) {
            return {CollectionKind::Trigram,
                    (index - kTrigramFirst) / 8 + 1,
                    (index - kTrigramFirst) % 8};
        }
        if (index >= kTaegeukFirst && index < kTaegeukFirst + 3) {
            return {CollectionKind::Taegeuk, index - kTaegeukFirst + 1};
        }
        if (index >= kHaninMirrorFirst && index < kHaninMirrorFirst + 3) {
            return {CollectionKind::HaninMirror, index - kHaninMirrorFirst + 1};
        }
        if (index >= kEssenceFirst && index < kEssenceFirst + 3) {
            return {CollectionKind::Essence, index - kEssenceFirst + 1};
        }
        if (index == kYeoijuAttack) {
            return {CollectionKind::YeoijuAttack, 1};
        }
        if (index == kYeoijuDefense) {
            return {CollectionKind::YeoijuDefense, 1};
        }
        return {};
    }

    bool collection_accepts(int index, int slot) {
        const auto d = collection_def(index);
        if (d.material) {
            return false;
        }
        switch (d.kind) {
        case CollectionKind::Mirror:
            return slot == 0;
        case CollectionKind::Trigram:
            return slot == d.part + 1;
        case CollectionKind::Taegeuk:
            return slot == 9;
        case CollectionKind::HaninMirror:
            return slot == 10;
        case CollectionKind::Essence:
            return slot >= 11 && slot <= 15;
        case CollectionKind::YeoijuAttack:
            return slot >= 16 && slot <= 20;
        case CollectionKind::YeoijuDefense:
            return slot >= 21 && slot <= 25;
        default:
            return false;
        }
    }

    std::array<std::uint32_t, kCollectionSlots>
    collection_slots(const std::vector<CollectionItem>& items) {
        std::array<std::uint32_t, kCollectionSlots> result{};
        auto sorted = items;
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.iid < b.iid;
        });
        for (const auto& item : sorted) {
            if (!item.equipped || !item.iid) {
                continue;
            }
            for (int slot = 0; slot < kCollectionSlots; ++slot) {
                if (!result[slot] && collection_accepts(item.index, slot)) {
                    result[slot] = item.iid;
                    break;
                }
            }
        }
        return result;
    }

    std::string collection_wear_error(const std::vector<CollectionItem>& items,
                                      const CollectionItem& item,
                                      bool equip) {
        const auto d = collection_def(item.index);
        if (d.kind == CollectionKind::None || d.material) {
            return "This item cannot be equipped in the collection.";
        }
        const auto slots = collection_slots(items);
        const auto at = [&](int slot) -> const CollectionItem* {
            for (const auto& entry : items) {
                if (entry.iid == slots[slot] && entry.equipped) {
                    return &entry;
                }
            }
            return nullptr;
        };
        if (!equip) {
            if (d.kind == CollectionKind::Trigram && (slots[0] || slots[9])) {
                return "Remove the Yin-Yang Mirror and Taegeuk first.";
            }
            if (d.kind == CollectionKind::HaninMirror) {
                for (int s = 11; s <= 15; ++s) {
                    if (slots[s]) {
                        return "Remove the Hanin essences first.";
                    }
                }
            }
            return {};
        }
        if (item.equipped) {
            return "This item is already equipped.";
        }
        bool free = false;
        for (int s = 0; s < kCollectionSlots; ++s) {
            if (collection_accepts(item.index, s) && !slots[s]) {
                free = true;
            }
        }
        if (!free) {
            return "Remove an equipped item from this collection slot first.";
        }
        if (d.kind == CollectionKind::Trigram) {
            if (item.level < 1 || item.level > 15) {
                return "A trigram must be enchanted before it can be equipped.";
            }
            for (int s = 1; s <= 8; ++s) {
                if (const auto* other = at(s)) {
                    if (collection_def(other->index).grade != d.grade) {
                        return "All eight trigrams must have the same grade.";
                    }
                }
            }
        }
        if (d.kind == CollectionKind::Mirror || d.kind == CollectionKind::Taegeuk) {
            for (int s = 1; s <= 8; ++s) {
                const auto* other = at(s);
                if (!other) {
                    return "Equip all eight trigrams first.";
                }
                if (collection_def(other->index).grade > d.grade) {
                    return "The trigrams exceed this item's grade.";
                }
            }
        }
        if (d.kind == CollectionKind::Mirror) {
            if (const auto* tae = at(9)) {
                if (collection_def(tae->index).grade > d.grade) {
                    return "The Taegeuk exceeds this mirror's grade.";
                }
            }
        }
        if (d.kind == CollectionKind::Taegeuk) {
            if (const auto* mirror = at(0)) {
                if (collection_def(mirror->index).grade < d.grade) {
                    return "The mirror's grade is too low.";
                }
            }
        }
        if (d.kind == CollectionKind::Essence) {
            if (!slots[10]) {
                return "Equip Hanin's Mirror first.";
            }
            if (item.option < 100 || item.option > 121 || item.value <= 0) {
                return "This essence has no valid ability.";
            }
            if (item.option == 112 || item.option >= 120) {
                for (int s = 11; s <= 15; ++s) {
                    if (const auto* other = at(s)) {
                        if (other->option == item.option) {
                            return "Only one essence of this ability can be equipped.";
                        }
                    }
                }
            }
        }
        return {};
    }

    const char* collection_option_name(int option) {
        static constexpr const char* names[] = {"Strength",
                                                "Wisdom",
                                                "Intelligence",
                                                "Health",
                                                "Agility",
                                                "Hit",
                                                "Evasion",
                                                "Defense",
                                                "Resistance",
                                                "Absorption",
                                                "HP %",
                                                "MP %",
                                                "Movement speed %",
                                                "Min. attack %",
                                                "Max. attack %",
                                                "Critical chance %",
                                                "Critical damage %",
                                                "Explosive damage %",
                                                "Min. magic %",
                                                "Max. magic %",
                                                "Experience %",
                                                "Ancient experience %"};
        return option >= 100 && option <= 121 ? names[option - 100] : "";
    }

    CollectionBonuses collection_bonuses(const std::vector<CollectionItem>& items, bool pvp) {
        CollectionBonuses result;
        const auto slots = collection_slots(items);
        const auto at = [&](int slot) -> const CollectionItem* {
            for (const auto& entry : items) {
                if (entry.iid == slots[slot] && entry.equipped) {
                    return &entry;
                }
            }
            return nullptr;
        };
        const auto* hanin = at(10);
        if (hanin) {
            const double amplification = 1.0 + (collection_def(hanin->index).grade - 1) * 0.1;
            for (int slot = 11; slot <= 15; ++slot) {
                if (const auto* essence = at(slot)) {
                    if (essence->option >= 100 && essence->option <= 121 && essence->value > 0) {
                        result.ability[essence->option - 100] +=
                            std::min(120, essence->value) * amplification;
                    }
                }
            }
        }
        if (pvp) {
            const double speed = result.ability[12];
            result = {};
            result.ability[12] = speed;
            return result;
        }
        const auto* mirror = at(0);
        if (!mirror) {
            return result;
        }
        int lowest = 15;
        int grade = 0;
        for (int slot = 1; slot <= 8; ++slot) {
            const auto* trigram = at(slot);
            if (!trigram || trigram->level < 1 || trigram->level > 15) {
                return result;
            }
            const int current_grade = collection_def(trigram->index).grade;
            if (grade && grade != current_grade) {
                return result;
            }
            grade = current_grade;
            lowest = std::min(lowest, trigram->level);
        }
        if (grade < 1 || grade > 3 || collection_def(mirror->index).grade < grade) {
            return result;
        }
        const int attack[] = {2, 7, 15};
        result.attack += attack[collection_def(mirror->index).grade - 1];
        const auto add = [&](int part, int value) {
            if (part == 0) {
                result.hp += value;
            } else if (part == 1) {
                result.mp += value;
            } else if (part == 2) {
                result.attack += value;
            } else {
                constexpr int map[] = {0, 4, 2, 1, 3};
                result.ability[map[part - 3]] += value;
            }
        };
        for (int slot = 1; slot <= 8; ++slot) {
            const auto* trigram = at(slot);
            const int value = kTriagramStats[(slot - 1) * 3 + grade - 1][trigram->level - 1];
            add(slot - 1, value * (100 + kYinYangRate[grade - 1][lowest - 1]) / 100);
        }
        if (const auto* taegeuk = at(9)) {
            const int tae_grade = collection_def(taegeuk->index).grade;
            if (tae_grade >= grade && tae_grade <= collection_def(mirror->index).grade) {
                if (taegeuk->option >= 105 && taegeuk->option <= 107) {
                    result.ability[taegeuk->option - 100] += std::clamp(taegeuk->value, 0, 25);
                }
                if (taegeuk->absorption >= 1 && taegeuk->absorption <= 184 && lowest >= 5) {
                    for (int part = 0; part < 8; ++part) {
                        add(part, kTaegeukPrefix[taegeuk->absorption - 1][part]);
                    }
                }
            }
        }
        return result;
    }

    std::string collection_absorption_error(const std::vector<CollectionItem>& items,
                                            const CollectionItem& taegeuk) {
        const auto def = collection_def(taegeuk.index);
        if (def.kind != CollectionKind::Taegeuk || taegeuk.equipped) {
            return "Select an unequipped Taegeuk.";
        }
        if (taegeuk.absorption != 0) {
            return "This Taegeuk already has an absorption effect.";
        }
        const auto slots = collection_slots(items);
        int grade = 0;
        for (int slot = 1; slot <= 8; ++slot) {
            const CollectionItem* trigram = nullptr;
            for (const auto& item : items) {
                if (item.iid == slots[slot] && item.equipped) {
                    trigram = &item;
                }
            }
            if (!trigram || trigram->level < 5) {
                return "Equip all eight trigrams at +5 or higher.";
            }
            const int current = collection_def(trigram->index).grade;
            if ((grade && grade != current) || current > def.grade) {
                return "The trigrams must share a grade no higher than the Taegeuk.";
            }
            grade = current;
        }
        return {};
    }

    int collection_absorption_roll(const std::vector<CollectionItem>& items,
                                   const CollectionItem& taegeuk,
                                   int choice) {
        if (choice < 0 || choice > 7 || !collection_absorption_error(items, taegeuk).empty()) {
            return 0;
        }
        int lowest = 15;
        for (const auto& item : items) {
            if (item.equipped && collection_def(item.index).kind == CollectionKind::Trigram) {
                lowest = std::min(lowest, item.level);
            }
        }
        // Correct the reversed 41..18 legacy range to the contiguous 41..48 block.
        static constexpr int normal[] = {1, 9, 33, 65, 17, 41, 73, 49, 81, 145, 177};
        static constexpr int senior[] = {97, 105, 121, 153, 113, 129, 161, 137, 169, 169, 169};
        const int stage = std::clamp(lowest, 5, 15) - 5;
        return (collection_def(taegeuk.index).grade == 3 ? senior[stage] : normal[stage]) + choice;
    }

    bool CollectionUpgrades::load(const std::string& directory) {
        m_chances = {};
        std::array<std::vector<double>, 3> next;
        const char* files[] = {"trigram.json", "hanin.json", "yeoiju.json"};
        const int limits[] = {15, 2, 20};
        for (int i = 0; i < 3; ++i) {
            const auto bytes = platform::read_file(platform::join_path(directory, files[i]));
            if (!bytes) {
                return false;
            }
            platform::Json root;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root)) {
                return false;
            }
            const auto& levels = root.at("success_percent");
            if (!levels.is_array() ||
                levels.array().size() != static_cast<std::size_t>(limits[i])) {
                return false;
            }
            for (const auto& value : levels.array()) {
                if (!value.is_number() || !std::isfinite(value.number()) || value.number() < 0 ||
                    value.number() > 100) {
                    return false;
                }
                next[i].push_back(value.number());
            }
        }
        m_chances = std::move(next);
        return true;
    }

    double CollectionUpgrades::chance(CollectionKind kind, int level) const {
        int table =
            kind == CollectionKind::Trigram                                                   ? 0
            : kind == CollectionKind::Essence                                                 ? 1
            : (kind == CollectionKind::YeoijuAttack || kind == CollectionKind::YeoijuDefense) ? 2
                                                                                              : -1;
        if (table < 0 || level < 1 || level > static_cast<int>(m_chances[table].size())) {
            return 0;
        }
        return m_chances[table][static_cast<std::size_t>(level - 1)];
    }

    bool CollectionUpgrades::succeeds(CollectionKind kind, int level, double roll) const {
        return std::isfinite(roll) && roll >= 0 && roll < 1 && roll < chance(kind, level) / 100.0;
    }
}  // namespace openkal::server
