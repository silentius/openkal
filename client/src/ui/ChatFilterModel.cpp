/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ChatFilterModel.h"

#include <utility>

namespace openkal::client {

    ChatFilterModel::ChatFilterModel() {
        m_channels.fill(false);
        m_battle.fill(true);
        for (std::size_t t = 0; t < kTabCount; ++t) {
            const auto tab = static_cast<ChatTab>(t);
            m_channels[index(tab, tab_channel(tab))] = true;
        }
        m_channels[index(ChatTab::Normal, protocol::ChatChannel::Whisper)] = true;
    }

    std::size_t ChatFilterModel::index(ChatTab tab, protocol::ChatChannel channel) {
        return static_cast<std::size_t>(tab) * kChannelCount + static_cast<std::size_t>(channel);
    }

    void ChatFilterModel::set_active_tab(ChatTab tab) {
        if (static_cast<std::size_t>(tab) < kTabCount) {
            m_active_tab = tab;
        }
    }

    bool ChatFilterModel::channel_enabled(ChatTab tab, protocol::ChatChannel channel) const {
        if (static_cast<std::size_t>(tab) >= kTabCount ||
            static_cast<std::size_t>(channel) >= kChannelCount) {
            return false;
        }
        return m_channels[index(tab, channel)];
    }

    void
    ChatFilterModel::set_channel_enabled(ChatTab tab, protocol::ChatChannel channel, bool enabled) {
        if (static_cast<std::size_t>(tab) >= kTabCount ||
            static_cast<std::size_t>(channel) >= kChannelCount) {
            return;
        }
        m_channels[index(tab, channel)] = enabled;
    }

    bool ChatFilterModel::battle_enabled(protocol::BattleMsgKind kind) const {
        const auto i = static_cast<std::size_t>(kind);
        return i < kBattleCount && m_battle[i];
    }

    void ChatFilterModel::set_battle_enabled(protocol::BattleMsgKind kind, bool enabled) {
        const auto i = static_cast<std::size_t>(kind);
        if (i < kBattleCount) {
            m_battle[i] = enabled;
        }
    }

    bool ChatFilterModel::accepts(protocol::ChatChannel channel,
                                  std::optional<protocol::BattleMsgKind> battle_kind) const {
        if (battle_kind.has_value()) {
            return battle_enabled(*battle_kind);
        }
        return channel_enabled(m_active_tab, channel);
    }

    void
    ChatFilterModel::set_channel_flags(const std::array<bool, kTabCount * kChannelCount>& flags) {
        m_channels = flags;
    }

    void ChatFilterModel::set_battle_flags(const std::array<bool, kBattleCount>& flags) {
        m_battle = flags;
    }

    void
    ChatFilterModel::channel_color(protocol::ChatChannel channel, float& r, float& g, float& b) {
        switch (channel) {
        case protocol::ChatChannel::Party:
            r = 1.0f;
            g = 0.80f;
            b = 0.30f;
            break;
        case protocol::ChatChannel::Guild:
            r = 0.20f;
            g = 0.90f;
            b = 0.85f;
            break;
        case protocol::ChatChannel::Whisper:
            r = 0.35f;
            g = 0.55f;
            b = 1.0f;
            break;
        case protocol::ChatChannel::Alliance:
            r = 0.45f;
            g = 0.32f;
            b = 0.80f;
            break;
        case protocol::ChatChannel::Squad:
            r = 0.85f;
            g = 0.65f;
            b = 1.0f;
            break;
        case protocol::ChatChannel::Shout:
            r = 1.0f;
            g = 0.45f;
            b = 0.75f;
            break;
        case protocol::ChatChannel::Common:
        case protocol::ChatChannel::Count:
        default:
            r = 0.92f;
            g = 0.92f;
            b = 0.88f;
            break;
        }
    }

    void ChatFilterModel::battle_color(std::optional<protocol::BattleMsgKind> kind,
                                       float& r,
                                       float& g,
                                       float& b) {
        if (!kind.has_value()) {
            r = 1.0f;
            g = 0.85f;
            b = 0.35f;
            return;
        }
        switch (*kind) {
        case protocol::BattleMsgKind::Damage:
            r = 1.0f;
            g = 0.45f;
            b = 0.35f;
            break;
        case protocol::BattleMsgKind::Skill:
            r = 0.72f;
            g = 0.35f;
            b = 0.95f;
            break;
        case protocol::BattleMsgKind::Hp:
            r = 0.45f;
            g = 1.0f;
            b = 0.45f;
            break;
        case protocol::BattleMsgKind::Mp:
            r = 0.45f;
            g = 0.65f;
            b = 1.0f;
            break;
        case protocol::BattleMsgKind::Exp:
            r = 1.0f;
            g = 0.95f;
            b = 0.35f;
            break;
        case protocol::BattleMsgKind::TransformExp:
            r = 1.0f;
            g = 0.8f;
            b = 0.45f;
            break;
        case protocol::BattleMsgKind::ItemGet:
            r = 0.65f;
            g = 1.0f;
            b = 0.75f;
            break;
        case protocol::BattleMsgKind::MoneyGet:
            r = 1.0f;
            g = 1.0f;
            b = 1.0f;
            break;
        default:
            r = 1.0f;
            g = 0.85f;
            b = 0.35f;
            break;
        }
    }

    const char* ChatFilterModel::tab_id(ChatTab tab) {
        switch (tab) {
        case ChatTab::Party:
            return "party";
        case ChatTab::Guild:
            return "guild";
        case ChatTab::Alliance:
            return "alliance";
        case ChatTab::Squad:
            return "squad";
        case ChatTab::Shout:
            return "shout";
        default:
            return "normal";
        }
    }

    protocol::ChatChannel ChatFilterModel::tab_channel(ChatTab tab) {
        switch (tab) {
        case ChatTab::Party:
            return protocol::ChatChannel::Party;
        case ChatTab::Guild:
            return protocol::ChatChannel::Guild;
        case ChatTab::Alliance:
            return protocol::ChatChannel::Alliance;
        case ChatTab::Squad:
            return protocol::ChatChannel::Squad;
        case ChatTab::Shout:
            return protocol::ChatChannel::Shout;
        default:
            return protocol::ChatChannel::Common;
        }
    }

    const char* ChatFilterModel::tab_label(ChatTab tab) {
        switch (tab) {
        case ChatTab::Party:
            return "Party";
        case ChatTab::Guild:
            return "Guild";
        case ChatTab::Alliance:
            return "Alliance";
        case ChatTab::Squad:
            return "Squad";
        case ChatTab::Shout:
            return "Shout";
        case ChatTab::Normal:
        case ChatTab::Count:
        default:
            return "Normal";
        }
    }

    const char* ChatFilterModel::channel_label(protocol::ChatChannel channel) {
        switch (channel) {
        case protocol::ChatChannel::Party:
            return "Party";
        case protocol::ChatChannel::Guild:
            return "Guild";
        case protocol::ChatChannel::Whisper:
            return "Personal messages";
        case protocol::ChatChannel::Alliance:
            return "Alliance";
        case protocol::ChatChannel::Squad:
            return "Squad";
        case protocol::ChatChannel::Shout:
            return "Shout";
        case protocol::ChatChannel::Common:
        case protocol::ChatChannel::Count:
        default:
            return "Normal";
        }
    }

    const char* ChatFilterModel::battle_label(protocol::BattleMsgKind kind) {
        switch (kind) {
        case protocol::BattleMsgKind::Damage:
            return "Damage";
        case protocol::BattleMsgKind::Skill:
            return "Skill";
        case protocol::BattleMsgKind::Hp:
            return "HP";
        case protocol::BattleMsgKind::Mp:
            return "MP";
        case protocol::BattleMsgKind::Rage:
            return "Rage";
        case protocol::BattleMsgKind::Exp:
            return "EXP";
        case protocol::BattleMsgKind::TransformExp:
            return "Animal EXP";
        case protocol::BattleMsgKind::ItemUse:
            return "Item use";
        case protocol::BattleMsgKind::ItemGet:
            return "Item get";
        case protocol::BattleMsgKind::MoneyGet:
            return "Geon";
        case protocol::BattleMsgKind::Count:
        default:
            return "Battle";
        }
    }

    void ChatLog::push(ChatLine line) {
        if (m_lines.size() >= kCapacity) {
            m_lines.erase(m_lines.begin());
        }
        m_lines.push_back(std::move(line));
    }

}  // namespace openkal::client
