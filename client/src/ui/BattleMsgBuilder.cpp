/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BattleMsgBuilder.h"

#include <sstream>

namespace openkal::client {

    std::string BattleMsgBuilder::damage_dealt(int damage) {
        std::ostringstream oss;
        oss << "You dealt " << damage << " damage.";
        return oss.str();
    }

    std::string BattleMsgBuilder::critical_damage_dealt(int damage) {
        std::ostringstream oss;
        oss << "You gave critical damage of " << damage << " to the enemy.";
        return oss.str();
    }

    std::string BattleMsgBuilder::explosive_blow(int damage) {
        std::ostringstream oss;
        oss << "You gave " << damage << " of Explosive Blow to the enemy.";
        return oss.str();
    }

    std::string BattleMsgBuilder::damage_taken(int damage) {
        std::ostringstream oss;
        oss << "You received " << damage << " damage.";
        return oss.str();
    }

    std::string BattleMsgBuilder::critical_damage_taken(int damage) {
        std::ostringstream oss;
        oss << "You got critical damage of " << damage << " from the enemy.";
        return oss.str();
    }

    std::string BattleMsgBuilder::evaded_attack(std::string_view text) {
        if (text.empty() || text.front() == '#') {
            return "Evaded the enemy's attack";
        }
        return std::string(text);
    }

    std::string BattleMsgBuilder::property_delta(std::string_view label, std::int64_t delta) {
        if (delta == 0) {
            return {};
        }
        std::ostringstream oss;
        if (delta > 0) {
            oss << label << " is increased by " << delta << ".";
        } else {
            oss << label << " is decreased by " << -delta << ".";
        }
        return oss.str();
    }

    std::string BattleMsgBuilder::item_acquired(std::string_view item_name, int count) {
        std::ostringstream oss;
        if (count > 1) {
            oss << "You acquired " << item_name << " x" << count << ".";
        } else {
            oss << "You acquired " << item_name << ".";
        }
        return oss.str();
    }

    std::string BattleMsgBuilder::party_item_acquired(std::string_view recipient,
                                                      std::string_view item_name,
                                                      int count) {
        std::ostringstream oss;
        oss << recipient << " acquired " << item_name;
        if (count > 1) {
            oss << " x" << count;
        }
        oss << ".";
        return oss.str();
    }

    std::string BattleMsgBuilder::item_removed(std::string_view item_name, int count) {
        std::ostringstream oss;
        oss << "Removed " << count << " " << item_name << ".";
        return oss.str();
    }

    std::string BattleMsgBuilder::money_acquired(int amount) {
        std::ostringstream oss;
        oss << "You acquired " << amount << " Geon.";
        return oss.str();
    }

    std::string BattleMsgBuilder::skill_learned(std::string_view skill_name) {
        std::ostringstream oss;
        oss << "You learned " << skill_name << ".";
        return oss.str();
    }

    std::string BattleMsgBuilder::skill_level_up(std::string_view skill_name, int level) {
        std::ostringstream oss;
        oss << skill_name << " increased to level " << level << ".";
        return oss.str();
    }

    std::string BattleMsgBuilder::system_message(std::uint8_t msg_id) {
        switch (static_cast<protocol::SystemMessageId>(msg_id)) {
        case protocol::SystemMessageId::CantDelPutOnItem:
            return "You cannot discard an equipped item.";
        case protocol::SystemMessageId::InvenIsFull:
            return "Your inventory is full.";
        case protocol::SystemMessageId::PriorityOfItem:
            return "You cannot pick up that item yet.";
        case protocol::SystemMessageId::NotEnoughInvenSlot:
            return "Not enough inventory slots.";
        case protocol::SystemMessageId::EnchantFailed:
            return "Enchanting Failed.";
        case protocol::SystemMessageId::AccountBound:
            return "Action not possible, item bound.";
        default:
            return {};
        }
    }

}  // namespace openkal::client
