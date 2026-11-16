/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/PayBuff.h"

#include <algorithm>

namespace openkal::server {

    bool is_pay_buff_type(int buff_type) {
        switch (static_cast<protocol::BuffType>(buff_type)) {
        case protocol::BuffType::AttackFood:
        case protocol::BuffType::MagicAttackFood:
        case protocol::BuffType::PayUpAttack:
        case protocol::BuffType::PayCutDamage:
        case protocol::BuffType::PayUpXBlow3:
        case protocol::BuffType::PayUpXBlow5:
        case protocol::BuffType::PayUpHit:
        case protocol::BuffType::PayUpDodge:
        case protocol::BuffType::PayPerfectDef:
        case protocol::BuffType::PayDrainHp:
        case protocol::BuffType::StoneExp:
        case protocol::BuffType::StoneWealth:
        case protocol::BuffType::StoneFishery:
            return true;
        case protocol::BuffType::InventoryUnlock:
        case protocol::BuffType::Haste:
        case protocol::BuffType::Hp:
        case protocol::BuffType::Mp:
        case protocol::BuffType::StoneAnimalExp:
            return false;
        }
        return false;
    }

    std::uint64_t pay_buff_bstate(int buff_type) {
        if (!is_pay_buff_type(buff_type)) {
            return 0;
        }
        return protocol::buff_type_to_bstate(static_cast<protocol::BuffType>(buff_type));
    }

    std::uint64_t PayBuffSet::bstate() const {
        std::uint64_t bits = 0;
        for (const auto& e : m_entries) {
            bits |= e.bstate;
        }
        return bits;
    }

    bool PayBuffSet::has(std::uint64_t bit) const {
        return (bstate() & bit) != 0;
    }

    int PayBuffSet::find_index(int buff_type) const {
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].buff_type == buff_type) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void PayBuffSet::erase_at(int index) {
        if (index < 0 || index >= static_cast<int>(m_entries.size())) {
            return;
        }
        m_entries.erase(m_entries.begin() + index);
    }

    void PayBuffSet::clear() {
        m_entries.clear();
    }

    std::int32_t PayBuffSet::remain_of(const PayBuffEntry& entry, std::int64_t now_unix) {
        if (entry.is_count) {
            return entry.charges;
        }
        if (entry.expire_unix <= now_unix) {
            return 0;
        }
        const std::int64_t left = entry.expire_unix - now_unix;
        if (left > 0x7fffffff) {
            return 0x7fffffff;
        }
        return static_cast<std::int32_t>(left);
    }

    int PayBuffSet::percent_of(int buff_type) const {
        const int index = find_index(buff_type);
        if (index < 0) {
            return 0;
        }
        return m_entries[static_cast<std::size_t>(index)].stack_pack;
    }

    bool PayBuffSet::apply(int buff_type,
                           int remain,
                           std::int64_t now_unix,
                           std::int32_t& out_remain,
                           std::uint64_t& out_bstate,
                           std::int32_t source_item_index,
                           int amount) {
        out_remain = 0;
        out_bstate = 0;
        const bool food = protocol::buff_type_is_attack_food(buff_type);
        if (!is_pay_buff_type(buff_type) || remain <= 0 || (food && amount <= 0)) {
            return false;
        }
        const auto type = static_cast<protocol::BuffType>(buff_type);
        const std::uint64_t bit = protocol::buff_type_to_bstate(type);
        if (bit == 0) {
            return false;
        }

        // compatibility behavior: EB+3% and EB+5% are mutually exclusive.
        if (type == protocol::BuffType::PayUpXBlow3 && has(protocol::kCbsPayUpXBlow5)) {
            return false;
        }
        if (type == protocol::BuffType::PayUpXBlow5 && has(protocol::kCbsPayUpXBlow3)) {
            return false;
        }

        const bool count = protocol::buff_type_is_count(type);
        const int existing = find_index(buff_type);
        if (existing >= 0) {
            PayBuffEntry& e = m_entries[static_cast<std::size_t>(existing)];
            if (count) {
                // Same compatibility behavior type but different pack size (100/300/500) cannot
                // mix.
                if (e.stack_pack > 0 && e.stack_pack != remain) {
                    return false;
                }
                const std::int64_t sum =
                    static_cast<std::int64_t>(e.charges) + static_cast<std::int64_t>(remain);
                if (sum > protocol::kPayBuffChargeCap) {
                    return false;
                }
                e.charges = static_cast<std::int32_t>(sum);
                if (e.stack_pack <= 0) {
                    e.stack_pack = remain;
                }
                out_remain = e.charges;
            } else if (food) {
                e.expire_unix = now_unix + static_cast<std::int64_t>(remain);
                e.stack_pack = amount;
                out_remain = remain;
            } else {
                // Timed: compatibility behavior rejects re-apply; allow only in last minute.
                const std::int32_t left = remain_of(e, now_unix);
                if (left > protocol::kPayBuffRefreshWindowSec) {
                    return false;
                }
                e.expire_unix = now_unix + static_cast<std::int64_t>(remain);
                out_remain = remain;
            }
            if (source_item_index > 0) {
                e.source_item_index = source_item_index;
            }
            out_bstate = e.bstate;
            return true;
        }

        PayBuffEntry e;
        e.buff_type = buff_type;
        e.bstate = bit;
        e.is_count = count;
        e.source_item_index = source_item_index;
        if (count) {
            e.charges = remain;
            e.stack_pack = remain;
            out_remain = e.charges;
        } else {
            e.expire_unix = now_unix + static_cast<std::int64_t>(remain);
            if (food) {
                e.stack_pack = amount;
            }
            out_remain = remain;
        }
        out_bstate = bit;
        m_entries.push_back(e);
        return true;
    }

    std::uint64_t PayBuffSet::tick(std::int64_t now_unix) {
        std::uint64_t cleared = 0;
        for (int i = static_cast<int>(m_entries.size()) - 1; i >= 0; --i) {
            PayBuffEntry& e = m_entries[static_cast<std::size_t>(i)];
            if (e.is_count) {
                continue;
            }
            if (e.expire_unix <= now_unix) {
                cleared |= e.bstate;
                erase_at(i);
            }
        }
        return cleared;
    }

    bool PayBuffSet::consume_charge(int buff_type, std::int32_t& out_remain, bool& out_removed) {
        out_remain = 0;
        out_removed = false;
        const int idx = find_index(buff_type);
        if (idx < 0) {
            return false;
        }
        PayBuffEntry& e = m_entries[static_cast<std::size_t>(idx)];
        if (!e.is_count || e.charges <= 0) {
            return false;
        }
        --e.charges;
        if (e.charges <= 0) {
            out_removed = true;
            out_remain = 0;
            erase_at(idx);
            return true;
        }
        out_remain = e.charges;
        return true;
    }

    void PayBuffSet::load(const std::vector<BuffRemainRecord>& rows, std::int64_t now_unix) {
        clear();
        for (const auto& row : rows) {
            if (!is_pay_buff_type(row.buff_type) || row.remain <= 0) {
                continue;
            }
            const auto type = static_cast<protocol::BuffType>(row.buff_type);
            const std::uint64_t bit = protocol::buff_type_to_bstate(type);
            if (bit == 0) {
                continue;
            }
            PayBuffEntry e;
            e.buff_type = row.buff_type;
            e.bstate = bit;
            e.is_count = row.is_count != 0 || protocol::buff_type_is_count(type);
            e.stack_pack = row.stack_pack;
            e.source_item_index = row.source_item_index;
            if (e.is_count) {
                e.charges = row.remain;
                if (e.stack_pack <= 0) {
                    e.stack_pack = row.remain;
                }
            } else {
                e.expire_unix = now_unix + static_cast<std::int64_t>(row.remain);
            }
            m_entries.push_back(e);
        }
    }

    std::vector<BuffRemainRecord> PayBuffSet::snapshot(std::int32_t character_id,
                                                       std::int64_t now_unix) const {
        std::vector<BuffRemainRecord> out;
        out.reserve(m_entries.size());
        for (const auto& e : m_entries) {
            const std::int32_t rem = remain_of(e, now_unix);
            if (rem <= 0) {
                continue;
            }
            BuffRemainRecord row;
            row.character_id = character_id;
            row.buff_type = e.buff_type;
            row.remain = rem;
            row.is_count = e.is_count ? 1 : 0;
            row.stack_pack = e.stack_pack;
            row.source_item_index = e.source_item_index;
            out.push_back(row);
        }
        return out;
    }

}  // namespace openkal::server
