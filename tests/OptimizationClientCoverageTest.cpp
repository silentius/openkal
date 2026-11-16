/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/NetClient.h"
#include "openkal/map/Coords.h"
#include "openkal/client/DebugOverlay.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/Skills.h"
#include "openkal/client/JobTitles.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/Effect.h"
#include "openkal/protocol/Emote.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Guild.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/ItemInfo.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/Mail.h"
#include "openkal/protocol/MapState.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Party.h"
#include "openkal/protocol/PlayerOptions.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/protocol/MState.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/ClockSync.h"
#include "openkal/protocol/ExpEventState.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Progression.h"
#include "openkal/protocol/Riding.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/Storage.h"
#include "openkal/protocol/Trade.h"
#include "openkal/protocol/TradingPost.h"
#include "openkal/protocol/Transform.h"
#include "openkal/protocol/TrashItem.h"
#include "openkal/server/Packets.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <limits>
#include <sstream>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unistd.h>
#include <utility>
#include <vector>

#ifdef OPENKAL_TEST_WRAP_NEW
namespace {
    bool g_fail_next_allocation = false;
    bool g_track_allocations = false;
    std::size_t g_allocation_count = 0;
    std::size_t g_allocation_bytes = 0;
}  // namespace

extern "C" void* __real__Znwm(std::size_t size);
extern "C" void* __wrap__Znwm(std::size_t size) {
    if (g_track_allocations) {
        ++g_allocation_count;
        g_allocation_bytes += size;
    }
    if (g_fail_next_allocation) {
        g_fail_next_allocation = false;
        throw std::bad_alloc();
    }
    return __real__Znwm(size);
}
#endif

namespace {
    using namespace openkal;
    using test::expect;
    using Bytes = std::vector<std::uint8_t>;

    struct SessionState {
        bool alive = true;
        bool eof = false;
        unsigned sends = 0;
        unsigned fail_send = 0;
        unsigned throw_send = 0;
        unsigned throw_close = 0;
        unsigned closes = 0;
        Bytes sent;
        std::deque<Bytes> chunks;
    };

    class BufferedSession final : public server::ISession {
      public:
        explicit BufferedSession(std::shared_ptr<SessionState> state) : m_state(std::move(state)) {}
        int fd() const override {
            return -1;
        }
        bool alive() const override {
            return m_state->alive;
        }
        void close() override {
            ++m_state->closes;
            if (m_state->closes == m_state->throw_close) {
                throw std::runtime_error("scripted close failure");
            }
            m_state->alive = false;
        }
        bool send_all(const std::uint8_t* data, std::size_t size) override {
            ++m_state->sends;
            if (m_state->sends == m_state->throw_send) {
                throw std::runtime_error("scripted transport failure");
            }
            if (!m_state->alive || m_state->sends == m_state->fail_send) {
                return false;
            }
            m_state->sent.insert(m_state->sent.end(), data, data + size);
            return true;
        }
        bool recv_into(Bytes& buffer) override {
            if (!m_state->chunks.empty()) {
                const auto& chunk = m_state->chunks.front();
                buffer.insert(buffer.end(), chunk.begin(), chunk.end());
                m_state->chunks.pop_front();
            }
            return m_state->alive && !m_state->eof;
        }

      private:
        std::shared_ptr<SessionState> m_state;
    };

    std::shared_ptr<SessionState> attach(client::NetClient& client) {
        auto state = std::make_shared<SessionState>();
        client.adopt_session(std::make_unique<BufferedSession>(state));
        return state;
    }

    Bytes create(std::uint32_t id, bool local) {
        const std::uint16_t wear[7]{};
        return protocol::CreatePlayer::frame(
            id, "Owned player", local ? 0x80 : 1, 1000, 1000, 0, 0, 0, wear, 0, 0);
    }

    void append(Bytes& bytes, const Bytes& frame) {
        bytes.insert(bytes.end(), frame.begin(), frame.end());
    }

    void queue(std::shared_ptr<SessionState> state, const Bytes& frame) {
        if (state->chunks.empty()) {
            state->chunks.emplace_back();
        }
        append(state->chunks.back(), frame);
    }

    void expect_chat(client::NetClient& client, int sequence) {
        client::PendingChat event;
        expect(client.take_chat(event) && event.speaker == "Owned speaker" &&
                   event.text == "line " + std::to_string(sequence),
               "framing preserves numbered chat payload and order");
    }

    void frame_boundaries() {
        Bytes batch;
        for (int i = 0; i < 3; ++i) {
            append(batch,
                   protocol::Chatting::frame_s2c("Owned speaker", "line " + std::to_string(i)));
        }
        // Every split includes every byte in all three headers and both strings.
        for (std::size_t split = 0; split <= batch.size(); ++split) {
            client::NetClient client;
            auto state = attach(client);
            state->chunks.emplace_back(batch.begin(),
                                       batch.begin() + static_cast<std::ptrdiff_t>(split));
            client.update(0, 32);
            state->chunks.emplace_back(batch.begin() + static_cast<std::ptrdiff_t>(split),
                                       batch.end());
            client.update(0, 32);
            for (int i = 0; i < 3; ++i) {
                expect_chat(client, i);
            }
            client::PendingChat empty{"unchanged", "unchanged"};
            expect(client.connected() && !client.take_chat(empty) && empty.text == "unchanged",
                   "incomplete tails stay connected and complete delivery occurs once");
        }
        client::NetClient client;
        auto state = attach(client);
        for (const auto byte : batch) {
            state->chunks.push_back({byte});
            client.update(0, 32);
        }
        for (int i = 0; i < 3; ++i) {
            expect_chat(client, i);
        }
        queue(state, Bytes{3, 0, 255});
        queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 3"));
        client.update(0, 32);
        expect_chat(client, 3);
        expect(client.connected(), "unknown complete opcode does not stop subsequent frames");
        protocol::PacketWriter maximum;
        maximum.payload().assign(protocol::kMaxPacketLength - protocol::kPacketReservedLength,
                                 0x5a);
        queue(state, maximum.frame(255));
        queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 6"));
        client.update(0, 32);
        expect_chat(client, 6);
        expect(client.connected(),
               "the maximum legal frame length leaves the following frame intact");
        for (int invalid_length : {0, 1, 2, 8001, 65535}) {
            queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 4"));
            queue(state,
                  Bytes{static_cast<std::uint8_t>(invalid_length),
                        static_cast<std::uint8_t>(invalid_length >> 8),
                        255});
            queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 5"));
            client.update(0, 32);
            expect_chat(client, 4);
            client::PendingChat event;
            expect(!client.connected() && !client.take_chat(event),
                   "invalid length keeps prior events and discards later bytes");
            state = attach(client);
        }
        queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 7"));
        protocol::PacketWriter empty_payload;
        queue(state, empty_payload.frame(static_cast<std::uint8_t>(protocol::S2C::TradingPost)));
        queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 8"));
        client.update(0, 32);
        expect_chat(client, 7);
        expect_chat(client, 8);
        expect(!client.connected(),
               "CHARACTERIZATION: malformed payload closes transport but the current batch still "
               "drains");
    }

    void frame_handler_exception() {
        client::NetClient client;
        auto state = attach(client);
        queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 91"));
        protocol::PacketWriter malformed;
        queue(state, malformed.frame(static_cast<std::uint8_t>(protocol::S2C::TradingPost)));
        queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 92"));
        state->throw_close = 1;
        bool threw = false;
        try {
            client.update(0, 32);
        } catch (const std::runtime_error& error) {
            threw = std::string_view(error.what()) == "scripted close failure";
        }
        expect(threw && client.connected(),
               "a frame handler propagates a transport exception without replaying consumed bytes");
        expect_chat(client, 91);
        client::PendingChat event;
        expect(!client.take_chat(event), "frames following the throwing handler remain unread");
        state->throw_close = 0;
        client.update(0, 32);
        expect_chat(client, 92);
        expect(!client.take_chat(event) && state->closes == 1,
               "the next update drains only the unread suffix after a handler exception");
    }

    void fifo_queues() {
        client::NetClient client;
        auto state = attach(client);
        client::PendingSkillAction skill;
        client::PendingTransformAction transform;
        client::PendingPlayerAttack attack;
        client::PendingHitFx hit;
        client::PendingCombatFloat number;
        client::PendingDelayDamage delay;
        expect(!client.take_skill_action(skill) && !client.take_transform_action(transform) &&
                   !client.take_player_attack(attack) && !client.take_hit_fx(hit) &&
                   !client.take_combat_float(number) && !client.take_delay_damage(delay),
               "every FIFO reports empty before its first event");
        queue(state, create(1, true));
        queue(state, create(2, false));
        server::MonsterEntity monster;
        monster.id = 900;
        monster.hp = 1000000;
        queue(state, server::frame_create_monster(monster));
        client.update(0, 32);
        for (int burst : {1, 32, 256}) {
            for (int i = 0; i < burst; ++i) {
                queue(
                    state,
                    protocol::Skill::frame_action_skill(2, static_cast<std::uint8_t>(i), 1000 + i));
                queue(state, protocol::Transform::frame_action_pre(2000 + i, 1 + i % 3));
                queue(state, protocol::Transform::frame_action_form(2000 + i, 16 + i % 3));
                queue(state, protocol::Attack::frame_server(2, 3000 + i, 0, 0, protocol::kAtkAnim));
                queue(state, protocol::Attack::frame_server(1, 900, i + 1, 0, protocol::kAtkHit));
                queue(state,
                      protocol::Skill::frame_server_damage(
                          2, 2, 4000 + i, 1, i + 1, 0, protocol::kAtkHit));
                queue(state,
                      protocol::Chatting::frame_s2c("Owned speaker", "line " + std::to_string(i)));
            }
            client.update(0, 32);
            for (int i = 0; i < burst; ++i) {
                expect(client.take_skill_action(skill) && skill.caster_id == 2 &&
                           skill.skill == static_cast<std::uint8_t>(i) &&
                           skill.target_id == static_cast<unsigned>(1000 + i) && !skill.fire &&
                           client.take_skill_action(skill) && skill.skill == 2 && skill.fire &&
                           skill.target_id == static_cast<unsigned>(4000 + i),
                       "skill preparation and fire keep FIFO order across coalesced bursts");
                expect(client.take_transform_action(transform) && transform.pre &&
                           transform.entity_id == static_cast<unsigned>(2000 + i) &&
                           transform.arg == 1 + i % 3 && client.take_transform_action(transform) &&
                           !transform.pre && transform.arg == 16 + i % 3,
                       "transform preparation and apply keep distinct payloads");
                expect(client.take_player_attack(attack) && attack.attacker_id == 2 &&
                           attack.target_id == static_cast<unsigned>(3000 + i),
                       "normal attacks preserve order and target identity");
                expect(client.take_hit_fx(hit) && hit.attacker_id == 1 && hit.target_id == 900 &&
                           hit.flags == protocol::kAtkHit && hit.skill_index == -1,
                       "hit feedback retains normal attack identity and flags");
                expect(client.take_hit_fx(hit) && hit.attacker_id == 2 &&
                           hit.target_id == static_cast<unsigned>(4000 + i) &&
                           hit.flags == protocol::kAtkHit && hit.skill_index == 2,
                       "skill hit feedback follows its normal hit without losing source identity");
                expect(client.take_combat_float(number) && number.text == std::to_string(i + 1) &&
                           number.anchor_id == 900 &&
                           number.kind == client::CombatFloatKind::DamageDealt &&
                           !number.wait_for_bolt,
                       "combat floats preserve text, anchor, kind and timing");
                expect(client.take_delay_damage(delay) &&
                           delay.target_id == static_cast<unsigned>(4000 + i) &&
                           delay.damage == i + 1,
                       "delayed damage preserves target and amount in FIFO order");
                expect_chat(client, i);
            }
            expect(!client.take_skill_action(skill) && !client.take_transform_action(transform) &&
                       !client.take_player_attack(attack) && !client.take_hit_fx(hit) &&
                       !client.take_combat_float(number) && !client.take_delay_damage(delay),
                   "every FIFO drains once and can receive the next burst");
        }
    }

    template <typename Value, typename Build, typename Take, typename Verify>
    void check_fifo(client::NetClient& client,
                    const std::shared_ptr<SessionState>& session,
                    const char* name,
                    Build build,
                    Take take,
                    Verify verify) {
        Value value{};
        expect(!take(value), "additional FIFO starts empty");
        const auto append_value = [&](int i) {
            const auto frame = build(i);
            expect(!frame.empty(), "owned FIFO packet fixture is valid");
            queue(session, frame);
        };
        append_value(1);
        append_value(2);
        client.update(0, 32);
        expect(take(value) && verify(value, 1), name);
        append_value(3);
        client.update(0, 32);
        for (int i : {2, 3}) {
            expect(take(value) && verify(value, i), name);
        }
        expect(!take(value) && verify(value, 3),
               "an empty FIFO preserves its output after a partial drain and refill");
        for (int i : {1, 2, 3}) {
            append_value(i);
            client.update(0, 32);
            expect(take(value) && verify(value, i) && !take(value) && verify(value, i),
                   "alternating FIFO input and output delivers each numbered payload once");
        }
    }

    void remaining_fifo_queues() {
        client::NetClient client;
        auto session = attach(client);
        check_fifo<std::uint32_t>(
            client,
            session,
            "level-up effect FIFO retains entity order",
            [](int i) {
                return protocol::Effect::frame(
                    400 + i, static_cast<std::uint8_t>(protocol::EffectType::LevelUp));
            },
            [&](auto& value) { return client.take_level_up_effect(value); },
            [](auto value, int i) { return value == static_cast<unsigned>(400 + i); });
        check_fifo<std::pair<std::uint32_t, std::uint8_t>>(
            client,
            session,
            "charm FIFO retains entity and effect byte",
            [](int i) {
                return protocol::Effect::frame(
                    500 + i, static_cast<std::uint8_t>(protocol::EffectType::Charm1) + i);
            },
            [&](auto& value) { return client.take_charm_effect(value.first, value.second); },
            [](const auto& value, int i) {
                return value.first == static_cast<unsigned>(500 + i) &&
                       value.second == static_cast<std::uint8_t>(protocol::EffectType::Charm1) + i;
            });
        check_fifo<std::pair<std::uint32_t, std::uint16_t>>(
            client,
            session,
            "damaged-effect FIFO retains target and exact damage",
            [](int i) { return protocol::Effect::frame_damaged(600 + i, 30 + i); },
            [&](auto& value) { return client.take_damaged_effect(value.first, value.second); },
            [](const auto& value, int i) {
                return value.first == static_cast<unsigned>(600 + i) && value.second == 30 + i;
            });
        check_fifo<std::pair<int, int>>(
            client,
            session,
            "skill-up FIFO retains index and rank",
            [](int i) { return protocol::Skill::frame_skill_up(5 + i, i); },
            [&](auto& value) { return client.take_skill_up(value.first, value.second); },
            [](const auto& value, int i) { return value.first == 5 + i && value.second == i; });
        check_fifo<protocol::PlayerEmote>(
            client,
            session,
            "emote FIFO retains caster and motion",
            [](int i) { return protocol::Emote::frame_server(700 + i, i); },
            [&](auto& value) { return client.take_emote(value); },
            [](const auto& value, int i) {
                return value.player_id == static_cast<unsigned>(700 + i) && value.motion == i;
            });
        check_fifo<std::pair<std::int32_t, std::uint32_t>>(
            client,
            session,
            "HTML FIFO retains dialog and NPC correlation",
            [](int i) { return protocol::Quest::frame_send_html(10000 + i, 800 + i); },
            [&](auto& value) { return client.take_send_html(value.first, &value.second); },
            [](const auto& value, int i) {
                return value.first == 10000 + i && value.second == static_cast<unsigned>(800 + i);
            });
        queue(session, protocol::Quest::frame_send_html(20000, 1234));
        client.update(0, 32);
        std::int32_t html = 0;
        expect(client.take_send_html(html) && html == 20000 && !client.take_send_html(html),
               "HTML FIFO also supports callers that omit the optional NPC output");
        const auto shop = [](int i) {
            protocol::ShopItemList value;
            value.npc_id = 900 + i;
            value.currency_index = 23016;
            value.offers = {
                {static_cast<std::uint16_t>(23000 + i), static_cast<std::uint32_t>(100 + i)},
                {static_cast<std::uint16_t>(23100 + i), static_cast<std::uint32_t>(200 + i)}};
            return value;
        };
        check_fifo<protocol::ShopItemList>(
            client,
            session,
            "shop FIFO retains every offer, currency and NPC",
            [&](int i) { return protocol::Shop::frame_item_list_s2c(shop(i)); },
            [&](auto& value) { return client.take_shop_item_list(value); },
            [&](const auto& value, int i) {
                return protocol::Shop::frame_item_list_s2c(value) ==
                       protocol::Shop::frame_item_list_s2c(shop(i));
            });
        const auto storage = [](int i) {
            protocol::StorageInfo value;
            value.npc_id = 1000 + i;
            protocol::StorageItem item;
            item.iid = 2000 + i;
            item.index = 23000 + i;
            item.prefix = i;
            item.slot = i;
            item.count = 10 + i;
            item.info = protocol::kInsertAccountBound;
            item.charm.x_attack = i + 2;
            item.transform_level = 4 + i;
            item.transform_exp = 300 + i;
            value.items.push_back(item);
            return value;
        };
        check_fifo<protocol::StorageInfo>(
            client,
            session,
            "storage FIFO retains complete instance metadata and keeper correlation",
            [&](int i) { return protocol::Storage::frame_info_s2c(storage(i)); },
            [&](auto& value) { return client.take_storage_info(value); },
            [&](const auto& value, int i) {
                return protocol::Storage::frame_info_s2c(value) ==
                       protocol::Storage::frame_info_s2c(storage(i));
            });
        check_fifo<protocol::MailClaimResult>(
            client,
            session,
            "mail claim FIFO retains result and ID",
            [](int i) {
                return protocol::Mail::frame_claim_result_s2c(
                    {static_cast<std::uint32_t>(3000 + i), i % 2 != 0});
            },
            [&](auto& value) { return client.take_mail_claim_result(value); },
            [](const auto& value, int i) {
                return value.id == static_cast<unsigned>(3000 + i) && value.claimed == (i % 2 != 0);
            });
        check_fifo<client::PendingInsertItem>(
            client,
            session,
            "insert FIFO retains item ID, definition, count, prefix, binding, charm and expiry",
            [](int i) {
                protocol::InsertCharmFields charm;
                charm.x_attack = i + 2;
                charm.random_stat_type = 1;
                charm.random_stat_value = i;
                return server::frame_insert_item(4000 + i,
                                                 23000 + i,
                                                 i,
                                                 protocol::kInsertCharacterBound,
                                                 10 + i,
                                                 charm,
                                                 7000 + i);
            },
            [&](auto& value) { return client.take_insert_item(value); },
            [](const auto& value, int i) {
                return value.iid == static_cast<unsigned>(4000 + i) && value.index == 23000 + i &&
                       value.count == 10 + i && value.prefix == i &&
                       value.info == protocol::kInsertCharacterBound &&
                       value.charm.x_attack == i + 2 && value.charm.random_stat_type == 1 &&
                       value.charm.random_stat_value == i && value.has_expire_at &&
                       value.expire_at == static_cast<unsigned>(7000 + i);
            });
        check_fifo<client::PendingItemWear>(
            client,
            session,
            "item-wear FIFO retains equip and unequip identity and timed expiry",
            [](int i) {
                return i % 2 ? protocol::ItemWear::frame_server_put_on(
                                   42, 5000 + i, 23000 + i, 8000 + i)
                             : protocol::ItemWear::frame_server_put_off(42, 5000 + i, 23000 + i);
            },
            [&](auto& value) { return client.take_item_wear(value); },
            [](const auto& value, int i) {
                return value.player_id == 42 && value.iid == static_cast<unsigned>(5000 + i) &&
                       value.item_index == 23000 + i && value.equipped == (i % 2 != 0) &&
                       value.expire_at == static_cast<unsigned>(i % 2 ? 8000 + i : 0);
            });
        const auto riding = [](int i) {
            protocol::RidingState value;
            value.player_id = 42;
            value.iid = 6000 + i;
            value.item_index = 26000 + i;
            value.expire_at = 9000 + i;
            value.mounted = i % 2 != 0;
            value.kind = protocol::RidingKind::Battle;
            value.grade = static_cast<protocol::BattleRidingGrade>(i);
            value.rarity = i;
            value.satiety = 100 + i;
            value.max_satiety = 200 + i;
            value.revision = i;
            value.equipment_iids[2] = 8000 + i;
            value.equipment_item_indices[2] = 27000 + i;
            value.equipment_enchants[2] = i;
            value.abilities[3] = 10 + i;
            return value;
        };
        check_fifo<protocol::RidingState>(
            client,
            session,
            "riding FIFO retains the entire state, equipment, ability and revision snapshot",
            [&](int i) { return protocol::Riding::frame_server_state(riding(i)); },
            [&](auto& value) { return client.take_riding_state(value); },
            [&](const auto& value, int i) {
                return protocol::Riding::frame_server_state(value) ==
                       protocol::Riding::frame_server_state(riding(i));
            });
        check_fifo<protocol::RidingCacheResult>(
            client,
            session,
            "cache result FIFO retains the exact consumed instance and both reward quantities",
            [](int i) {
                return protocol::Riding::frame_server_cache_result(
                    {static_cast<std::uint32_t>(7000 + i),
                     static_cast<std::uint16_t>(26000 + i),
                     static_cast<std::uint16_t>(i),
                     static_cast<std::uint16_t>(23000 + i),
                     static_cast<std::uint16_t>(5 + i)});
            },
            [&](auto& value) { return client.take_riding_cache_result(value); },
            [](const auto& value, int i) {
                return value.cache_iid == static_cast<unsigned>(7000 + i) &&
                       value.seal_item_index == 26000 + i && value.seal_count == i &&
                       value.reward_item_index == 23000 + i && value.reward_count == 5 + i;
            });
        check_fifo<protocol::UpdateItemInfoFields>(
            client,
            session,
            "animal item update FIFO retains exact instance, level and EXP",
            [](int i) {
                return protocol::ItemInfo::frame_update({static_cast<std::uint32_t>(8000 + i),
                                                         static_cast<std::uint16_t>(28000 + i),
                                                         static_cast<std::uint8_t>(i),
                                                         static_cast<std::uint32_t>(1000 + i),
                                                         0});
            },
            [&](auto& value) { return client.take_update_item_info(value); },
            [](const auto& value, int i) {
                return value.iid == static_cast<unsigned>(8000 + i) && value.index == 28000 + i &&
                       value.level == i && value.exp == static_cast<unsigned>(1000 + i) &&
                       value.added == 0;
            });
        check_fifo<std::uint32_t>(
            client,
            session,
            "trash FIFO retains exact deleted instance order",
            [](int i) {
                return protocol::TrashItem::frame_s2c(9000 + i, protocol::kItemLogDelete);
            },
            [&](auto& value) { return client.take_trash_item(value); },
            [](auto value, int i) { return value == static_cast<unsigned>(9000 + i); });
        check_fifo<client::PendingMapState>(
            client,
            session,
            "map-state FIFO retains scenario, phase, mask and countdown",
            [](int i) {
                return protocol::MapState::frame({static_cast<std::uint8_t>(i),
                                                  static_cast<std::uint8_t>(i + 1),
                                                  static_cast<std::uint8_t>(1u << i),
                                                  protocol::MapStateTimer::Ritual,
                                                  static_cast<std::uint32_t>(60 + i)});
            },
            [&](auto& value) { return client.take_map_state(value); },
            [](const auto& value, int i) {
                return value.map == i && value.phase == i + 1 && value.active_mask == (1u << i) &&
                       value.timer == protocol::MapStateTimer::Ritual &&
                       value.timer_seconds == static_cast<unsigned>(60 + i);
            });
        protocol::PartyState party;
        party.id = 7;
        party.leader = 10;
        party.members = {{10, "Owned leader", 1, 10, 10, 0}, {11, "Owned member", 1, 10, 10, 0}};
        queue(session, protocol::frame_party_state(party));
        client.update(0, 32);
        expect(client.party().id == 7, "party notices start with a valid authoritative membership");
        check_fifo<protocol::PartyItemNotice>(
            client,
            session,
            "party-item FIFO retains recipient name, definition and quantity",
            [](int i) {
                return protocol::frame_party_item({7,
                                                   static_cast<std::uint32_t>(20 + i),
                                                   "Owned recipient " + std::to_string(i),
                                                   23000 + i,
                                                   i});
            },
            [&](auto& value) { return client.take_party_item(value); },
            [](const auto& value, int i) {
                return value.party == 7 && value.recipient == static_cast<unsigned>(20 + i) &&
                       value.recipient_name == "Owned recipient " + std::to_string(i) &&
                       value.item_index == 23000 + i && value.count == i;
            });
        check_fifo<client::PendingInfoMessage>(
            client,
            session,
            "info FIFO retains ordered text, money kind and color",
            [](int i) { return protocol::frame_trading_post_pickup({0, 0, 100 + i}); },
            [&](auto& value) { return client.take_info_message(value); },
            [](const auto& value, int i) {
                return value.text == "You got " + std::to_string(100 + i) + " geons" &&
                       value.kind == protocol::BattleMsgKind::MoneyGet && value.color &&
                       *value.color == std::array<float, 3>{1, 1, 1};
            });
    }

    void capped_queues() {
        for (int count : {31, 32, 33, 127, 128, 129, 131}) {
            client::NetClient client;
            auto state = attach(client);
            protocol::PlayerOptionsState options;
            platform::Json view;
            expect(!client.take_player_options(options) && !client.take_guild_view(view),
                   "bounded view queues start empty");
            for (int i = 0; i < count; ++i) {
                protocol::GuildResponse response;
                response.request_id = i + 1;
                response.data.set_type(platform::Json::Type::Object);
                queue(state, protocol::frame_guild_response(response));
                protocol::PlayerOptionsState option;
                option.request_id = i + 1;
                option.target = 500 + i;
                option.name = "player " + std::to_string(i);
                queue(state, protocol::frame_player_options_state(option));
            }
            client.update(0, 32);
            for (int i = std::max(0, count - 128); i < count; ++i) {
                expect(client.take_guild_view(view) &&
                           view.at("_request_id").as_string() == std::to_string(i + 1),
                       "guild queue evicts precisely the oldest values at cap 128");
            }
            for (int i = std::max(0, count - 32); i < count; ++i) {
                expect(client.take_player_options(options) &&
                           options.request_id == static_cast<unsigned>(i + 1) &&
                           options.target == static_cast<unsigned>(500 + i) &&
                           options.name == "player " + std::to_string(i),
                       "player option queue evicts precisely the oldest values at cap 32");
            }
            expect(!client.take_guild_view(view) && !client.take_player_options(options),
                   "capped queues deliver each retained snapshot once");
        }
    }

    void player_cancellation() {
        client::NetClient client;
        auto state = attach(client);
        queue(state, create(31, false));
        queue(state, create(32, false));
        queue(state, protocol::CreatePlayer::frame_remove(31));
        client.update(0, 32);
        protocol::CreatePlayerFields player;
        std::uint32_t removed = 0;
        expect(client.take_create_other_player(player) && player.id == 32 &&
                   !client.take_create_other_player(player) &&
                   client.take_remove_other_player(removed) && removed == 31 &&
                   !client.take_remove_other_player(removed),
               "removal cancels an unread create without disturbing unrelated create order");
        queue(state, protocol::CreatePlayer::frame_remove(33));
        queue(state, protocol::CreatePlayer::frame_remove(33));
        queue(state, create(34, false));
        queue(state, create(33, false));
        client.update(0, 32);
        expect(client.take_create_other_player(player) && player.id == 34 &&
                   client.take_create_other_player(player) && player.id == 33 &&
                   !client.take_create_other_player(player) &&
                   !client.take_remove_other_player(removed),
               "a later create cancels every unread removal of its ID while preserving other "
               "create order");
    }

    double numbered_skill_drain(int count) {
        client::NetClient client;
        auto state = attach(client);
        for (int i = 0; i < count; ++i) {
            queue(state, protocol::Skill::frame_action_skill(2, i % 8, 10000 + i));
        }
        client.update(0, 32);
        bool ordered = true;
        client::PendingSkillAction action;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < count; ++i) {
            ordered = client.take_skill_action(action) && ordered && action.caster_id == 2 &&
                      action.skill == i % 8 && action.target_id == static_cast<unsigned>(10000 + i);
        }
        const auto elapsed =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                .count();
        expect(ordered && !client.take_skill_action(action),
               "large numbered skill FIFOs drain every payload once in previous order");
        return elapsed;
    }

    std::size_t g_counted_assignments = 0;
    struct CountedAction {
        client::PendingSkillAction value;
        CountedAction() = default;
        CountedAction(const CountedAction&) = default;
        CountedAction(CountedAction&&) noexcept = default;
        CountedAction& operator=(const CountedAction& other) {
            value = other.value;
            ++g_counted_assignments;
            return *this;
        }
        CountedAction& operator=(CountedAction&& other) noexcept {
            value = std::move(other.value);
            ++g_counted_assignments;
            return *this;
        }
    };
    static_assert(sizeof(CountedAction) == sizeof(client::PendingSkillAction));

    template <class Queue> std::size_t counted_drain(int count) {
        Queue values;
        for (int i = 0; i < count; ++i) {
            values.emplace_back();
            values.back().value.target_id = i + 1;
        }
        g_counted_assignments = 0;
        bool ordered = true;
        CountedAction output;
        for (int i = 0; i < count; ++i) {
            output = values.front();
            ordered = ordered && output.value.target_id == static_cast<unsigned>(i + 1);
            if constexpr (std::is_same_v<Queue, std::vector<CountedAction>>) {
                values.erase(values.begin());
            } else {
                values.pop_front();
            }
        }
        expect(ordered && values.empty(), "counted container surrogate preserves numbered order");
        return g_counted_assignments;
    }

    void benchmark_fifo() {
        std::cout
            << "# Actual NetClient take_skill_action drain only; packet parsing/setup excluded.\n"
            << "# Assignment counts use an explicitly separate vector/deque surrogate with "
               "the same PendingSkillAction element size; they are not production counters.\n"
            << "events,samples,actual_p50_us,actual_p95_us,actual_p99_us,"
               "legacy_vector_assignments,deque_assignments\n";
        for (int count : {32, 256, 2048}) {
            std::vector<double> samples;
            for (int repeat = 0; repeat < 25; ++repeat) {
                samples.push_back(numbered_skill_drain(count));
            }
            std::sort(samples.begin(), samples.end());
            const auto legacy = counted_drain<std::vector<CountedAction>>(count);
            const auto replacement = counted_drain<std::deque<CountedAction>>(count);
            expect(legacy == static_cast<std::size_t>(count) * (count + 1) / 2 &&
                       replacement == static_cast<std::size_t>(count),
                   "counted deque drain performs linear assignments while vector front erasure "
                   "performs quadratic assignments");
            std::cout << count << ",25," << std::fixed << std::setprecision(3) << samples[12] << ','
                      << samples[23] << ',' << samples[24] << ',' << legacy << ',' << replacement
                      << '\n';
        }
#ifdef OPENKAL_TEST_WRAP_NEW
        g_allocation_count = g_allocation_bytes = 0;
        g_track_allocations = true;
        {
            client::NetClient empty;
            g_track_allocations = false;
        }
        std::cout << "# Empty NetClient construction: wrapped operator new requests, not retained "
                     "heap bytes or allocator metadata.\n"
                  << "sizeof_netclient,constructor_allocations,constructor_requested_bytes\n"
                  << sizeof(client::NetClient) << ',' << g_allocation_count << ','
                  << g_allocation_bytes << '\n';
#endif
    }

    void player_option_requests() {
        client::NetClient client;
        protocol::PlayerOptionsRequest request;
        request.action = protocol::PlayerAction::ApplyGuild;
        request.target = 77;
        request.guild = 9;
        request.expected_fee = 123;
        expect(client.send_player_options(request) == 0,
               "player-option requests need a transport before allocating an observable request");
        auto state = attach(client);
        expect(client.send_player_options(request) == 0 && state->sent.empty(),
               "player-option requests cannot send before handshake");
        queue(state, server::frame_s2c_code(345));
        expect(client.handshake(), "establish the player-option request transport");
        state->sent.clear();
        std::uint64_t previous = 0;
        for (int i = 0; i < 3; ++i) {
            const auto request_id = client.send_player_options(request);
            std::size_t consumed = 0;
            std::uint8_t type = 0;
            Bytes payload;
            protocol::PlayerOptionsRequest decoded;
            expect(request_id > previous &&
                       protocol::FrameParser::pop(
                           state->sent.data(), state->sent.size(), consumed, type, payload) ==
                           protocol::FrameParseStatus::Complete &&
                       consumed == state->sent.size() &&
                       type == static_cast<std::uint8_t>(protocol::C2S::PlayerOptions) &&
                       protocol::parse_player_options_request(
                           payload.data(), payload.size(), decoded) &&
                       decoded.request_id == request_id && decoded.action == request.action &&
                       decoded.target == request.target && decoded.guild == request.guild &&
                       decoded.expected_fee == request.expected_fee,
                   "player-option sender writes distinct ordered IDs and preserves the entire "
                   "request");
            protocol::PlayerOptionsState response;
            response.request_id = request_id;
            response.target = request.target;
            response.guild = request.guild;
            response.name = "Owned target";
            queue(state, protocol::frame_player_options_state(response));
            client.update(0, 32);
            protocol::PlayerOptionsState delivered;
            expect(client.take_player_options(delivered) && delivered.request_id == request_id &&
                       delivered.target == request.target && delivered.guild == request.guild &&
                       delivered.name == response.name && !client.take_player_options(delivered),
                   "alternating request and response consumes each correlated snapshot once");
            previous = request_id;
            state->sent.clear();
        }
        state->fail_send = state->sends + 1;
        expect(client.send_player_options(request) == 0 && state->sent.empty(),
               "player-option write failure cannot return a usable request ID");
        state->throw_send = state->sends + 1;
        bool transport_threw = false;
        try {
            (void) client.send_player_options(request);
        } catch (const std::runtime_error& error) {
            transport_threw = std::string_view(error.what()) == "scripted transport failure";
        }
        expect(
            transport_threw && client.connected() && state->sent.empty(),
            "transport exceptions propagate without inventing a response or closing the session");
#ifdef OPENKAL_TEST_WRAP_NEW
        const auto sends_before_failure = state->sends;
        bool allocation_threw = false;
        g_fail_next_allocation = true;
        try {
            (void) client.send_player_options(request);
        } catch (const std::bad_alloc&) {
            allocation_threw = true;
        }
        g_fail_next_allocation = false;
        expect(allocation_threw && state->sends == sends_before_failure && state->sent.empty() &&
                   client.connected(),
               "frame-allocation failure propagates before transport access and preserves the live "
               "session");
#endif
        expect(client.send_player_options(request) > previous && !state->sent.empty(),
               "the sender can issue a fresh request after transport and allocation exceptions");
        state->alive = false;
        client.close();
    }

    void handshake_and_exit() {
        client::NetClient absent;
        expect(!absent.handshake(), "handshake requires an adopted session");
        absent.update(0, 32);
        absent.close();
        for (unsigned fail_send : {1u, 2u}) {
            client::NetClient client;
            auto state = attach(client);
            state->fail_send = fail_send;
            queue(state, server::frame_s2c_code(12345));
            expect(!client.handshake() && !client.handshook(),
                   "connect and answer write failures cannot establish handshake");
        }
        {
            client::NetClient client;
            auto state = attach(client);
            state->eof = true;
            expect(!client.handshake() && !client.connected(), "handshake rejects EOF");
        }
        {
            client::NetClient client;
            auto state = attach(client);
            expect(!client.handshake() && !client.handshook() && client.connected(),
                   "handshake timeout preserves current unestablished connection behavior");
        }
        for (int mode = 0; mode < 7; ++mode) {
            client::NetClient client;
            auto state = attach(client);
            const auto challenge = server::frame_s2c_code(12345);
            state->chunks.push_back(Bytes(challenge.begin(), challenge.begin() + 1));
            state->chunks.push_back(Bytes(challenge.begin() + 1, challenge.end()));
            expect(client.handshake() && client.handshook(), "handshake parses a split challenge");
            Bytes expected = protocol::Handshake::build_connect(0);
            append(expected, protocol::Handshake::build_answer(12345));
            expect(state->sent == expected,
                   "handshake emits the exact connect and challenge answer");
            if (mode == 0 || mode == 1) {
                const auto answer = protocol::GameExit::frame_server_answer(mode == 0);
                state->chunks.push_back(Bytes(answer.begin(), answer.begin() + 2));
                state->chunks.push_back(Bytes(answer.begin() + 2, answer.end()));
            } else if (mode == 2) {
                state->eof = true;
            } else if (mode == 3) {
                state->fail_send = state->sends + 1;
            } else if (mode == 4) {
                protocol::TradeState trade;
                trade.phase = protocol::TradePhase::Open;
                trade.token = 1;
                trade.revision = 1;
                queue(state, protocol::frame_trade_state(trade));
                client.update(0, 32);
                expect(client.connected() && client.trade().locked(),
                       "graceful-close guard test starts with a valid locked trade");
            } else if (mode == 5) {
                state->alive = false;
            }
            const auto sends = state->sends;
            const auto serials = std::make_tuple(
                client.trade_serial(), client.trading_post_serial(), client.guild_serial());
            client.close();
            expect(!client.connected() && !client.handshook() && state->closes > 0,
                   "close resets transport and handshake for every exit result");
            expect(state->sends == sends + ((mode == 4 || mode == 5) ? 0u : 1u),
                   "only live, unlocked, established sessions send a graceful-exit request");
            const auto increments = mode == 2 ? 2u : 1u;
            expect(client.trade_serial() == std::get<0>(serials) + increments &&
                       client.trading_post_serial() == std::get<1>(serials) + increments &&
                       client.guild_serial() == std::get<2>(serials) + increments &&
                       state->closes == increments,
                   "every exit outcome preserves its exact view-serial and transport-close counts");
        }
    }

    void connection_cleanup_serials() {
        client::NetClient client;
        const auto path = (std::filesystem::temp_directory_path() /
                           ("openkal_client_cleanup_" + std::to_string(getpid()) + ".sock"))
                              .string();
        std::filesystem::remove(path);
        const auto serials = std::make_tuple(
            client.trade_serial(), client.trading_post_serial(), client.guild_serial());
        const auto advanced = [&](unsigned count) {
            return client.trade_serial() == std::get<0>(serials) + count &&
                   client.trading_post_serial() == std::get<1>(serials) + count &&
                   client.guild_serial() == std::get<2>(serials) + count;
        };
        expect(!client.connect_local_unix(path) && !client.connected() && advanced(1),
               "a failed Unix connection advances online-view serials once");
        auto listener = server::create_unix_acceptor();
        const bool listening = listener && listener->listen(path);
        expect(listening, "open an owned local Unix endpoint for connection cleanup");
        if (listening) {
            expect(client.connect_local_unix(path) && client.connected() && advanced(2),
                   "a successful Unix connection advances the same serials once");
            auto peer = listener->try_accept();
            expect(peer != nullptr, "accept the owned cleanup connection");
            expect(!client.connect_tcp("invalid-ipv4", 0) && !client.connected() && advanced(3),
                   "a failed TCP replacement clears views once and releases the previous session");
            client.close();
            expect(advanced(4), "closing after a failed replacement adds one cleanup invocation");
            if (peer) {
                peer->close();
            }
        }
        if (listener) {
            listener->close();
        }
        std::filesystem::remove(path);
    }

    void cleanup_characterization() {
        client::NetClient client;
        auto state = attach(client);
        protocol::TradeState trade;
        trade.phase = protocol::TradePhase::Open;
        trade.token = 1;
        trade.revision = 1;
        trade.partner = "Owned partner";
        queue(state, protocol::frame_trade_state(trade));
        queue(state, protocol::frame_trading_post_collection(true));
        protocol::GuildResponse guild;
        guild.data.set_type(platform::Json::Type::Object);
        queue(state, protocol::frame_guild_response(guild));
        protocol::PlayerOptionsState options;
        options.request_id = 1;
        options.target = 2;
        queue(state, protocol::frame_player_options_state(options));
        client.update(0, 32);
        expect(client.trade().locked() && client.trading_post_collection_pending(),
               "cleanup test starts with populated online views");
        const auto trade_serial = client.trade_serial();
        const auto market_serial = client.trading_post_serial();
        const auto guild_serial = client.guild_serial();
        state->eof = true;
        client.update(0, 32);
        platform::Json view;
        expect(!client.trade().locked() && !client.trading_post_collection_pending() &&
                   !client.take_player_options(options) && !client.take_guild_view(view),
               "EOF clears online views and pending view snapshots");
        expect(client.trade_serial() == trade_serial + 2 &&
                   client.trading_post_serial() == market_serial + 2 &&
                   client.guild_serial() == guild_serial + 2,
               "CHARACTERIZATION: EOF currently increments cleanup serials twice");
        client.update(0, 32);
        expect(client.trade_serial() == trade_serial + 4 &&
                   client.trading_post_serial() == market_serial + 4 &&
                   client.guild_serial() == guild_serial + 4,
               "CHARACTERIZATION: repeated update after EOF repeats both cleanup increments");
        client.close();
        expect(client.trade_serial() == trade_serial + 5 &&
                   client.trading_post_serial() == market_serial + 5 &&
                   client.guild_serial() == guild_serial + 5,
               "explicit close performs one further current cleanup increment");
        client.close();
        client.update(0, 32);
        expect(client.trade_serial() == trade_serial + 6 &&
                   client.trading_post_serial() == market_serial + 6 &&
                   client.guild_serial() == guild_serial + 6,
               "another close advances serials once while an update without a session leaves them "
               "unchanged");

        // These characterize existing reset defects; a later behavior fix must change
        // the expectation deliberately, separately from buffer/container optimization.
        state = attach(client);
        expect(client.trade_serial() == trade_serial + 7 &&
                   client.trading_post_serial() == market_serial + 7 &&
                   client.guild_serial() == guild_serial + 7,
               "adopting a replacement session retains its explicit close cleanup invocation");
        const auto old_frame = protocol::Chatting::frame_s2c("Owned speaker", "line 71");
        state->chunks.push_back(Bytes(old_frame.begin(), old_frame.begin() + 2));
        client.update(0, 32);
        client.close();
        state = attach(client);
        state->chunks.push_back(Bytes(old_frame.begin() + 2, old_frame.end()));
        client.update(0, 32);
        expect_chat(client, 71);
        expect(client.connected(),
               "CHARACTERIZATION: unread receive bytes currently survive close and adopt");
        queue(state, protocol::Chatting::frame_s2c("Owned speaker", "line 72"));
        client.update(0, 32);
        client.close();
        expect_chat(client, 72);
        expect(!client.connected(), "CHARACTERIZATION: pending chats currently survive close");
    }

    // C1_BASELINE_DRAFT_BEGIN
    /*
     * OpenKal - an open source KalOnline client and server.
     * Copyright (C) 2026 OpenKal contributors
     *
     * This program is free software: you can redistribute it and/or modify
     * it under the terms of the GNU General Public License as published by
     * the Free Software Foundation, either version 3 of the License, or
     * (at your option) any later version.
     */

    // PREPARATION DRAFT ONLY: neither compiled nor run. Public packet replay only.
    // Insert inside OptimizationClientCoverageTest.cpp's anonymous namespace after
    // ownership is released. Existing cap/FIFO/framing/lifecycle tests stay intact.
    using C1Json = platform::Json;

    C1Json c1_number(double value) {
        C1Json out;
        out.set_type(C1Json::Type::Number);
        out.set_number(value);
        return out;
    }
    C1Json c1_string(std::string value) {
        C1Json out;
        out.set_type(C1Json::Type::String);
        out.string() = std::move(value);
        return out;
    }
    C1Json c1_bool(bool value) {
        C1Json out;
        out.set_type(C1Json::Type::Bool);
        out.set_boolean(value);
        return out;
    }
    C1Json c1_array() {
        C1Json out;
        out.set_type(C1Json::Type::Array);
        return out;
    }
    C1Json c1_object() {
        C1Json out;
        out.set_type(C1Json::Type::Object);
        return out;
    }
    C1Json c1_u64(std::uint64_t value) {
        // JSON doubles cannot preserve every protocol u64 identity.
        return c1_string(std::to_string(value));
    }
    C1Json c1_trade_item(const protocol::TradeItem& item) {
        auto out = c1_object();
        auto& o = out.object();
        o["iid"] = c1_number(item.iid);
        o["index"] = c1_number(item.index);
        o["count"] = c1_number(item.count);
        o["slot"] = c1_number(item.slot);
        o["flags"] = c1_number(item.flags);
        o["pet_expire_at"] = c1_u64(item.pet_expire_at);
        o["ride_expire_at"] = c1_u64(item.ride_expire_at);
        o["attributes"] = c1_array();
        for (const auto value : item.attributes) {
            o["attributes"].array().push_back(c1_number(value));
        }
        return out;
    }
    C1Json c1_trade(const protocol::TradeState& state) {
        auto out = c1_object();
        auto& o = out.object();
        o["phase"] = c1_number(static_cast<unsigned>(state.phase));
        o["token"] = c1_number(state.token);
        o["revision"] = c1_number(state.revision);
        o["acknowledged"] = c1_number(state.acknowledged);
        o["inventory_slots"] = c1_number(state.inventory_slots);
        o["partner"] = c1_string(state.partner);
        o["status"] = c1_string(state.status);
        o["inviter"] = c1_bool(state.inviter);
        o["confirmed"] = c1_bool(state.confirmed);
        o["partner_confirmed"] = c1_bool(state.partner_confirmed);
        o["money"] = c1_number(state.money);
        o["partner_money"] = c1_number(state.partner_money);
        o["locked"] = c1_bool(state.locked());
        for (const auto& named : {std::make_pair("own", &state.own),
                                  std::make_pair("other", &state.other),
                                  std::make_pair("available", &state.available)}) {
            o[named.first] = c1_array();
            for (const auto& item : *named.second) {
                o[named.first].array().push_back(c1_trade_item(item));
            }
        }
        return out;
    }
    C1Json c1_market(const protocol::TradingPostState& state) {
        auto out = c1_object();
        auto& o = out.object();
        o["view"] = c1_number(static_cast<unsigned>(state.view));
        o["acknowledged"] = c1_u64(state.acknowledged);
        o["page"] = c1_number(state.page);
        o["total_pages"] = c1_number(state.total_pages);
        o["more"] = c1_bool(state.more);
        o["purchased"] = c1_number(state.purchased);
        o["status"] = c1_string(state.status);
        o["entries"] = c1_array();
        for (const auto& entry : state.entries) {
            auto value = c1_object();
            value.object()["id"] = c1_u64(entry.id);
            value.object()["item"] = c1_trade_item(entry.item);
            value.object()["unit_price"] = c1_number(static_cast<double>(entry.unit_price));
            value.object()["delivery"] = c1_number(static_cast<unsigned>(entry.delivery));
            o["entries"].array().push_back(std::move(value));
        }
        return out;
    }
    C1Json c1_party(const protocol::PartyState& state) {
        auto out = c1_object();
        auto& o = out.object();
        o["id"] = c1_number(state.id);
        o["leader"] = c1_number(state.leader);
        o["loot"] = c1_number(static_cast<unsigned>(state.loot));
        o["inviter"] = c1_number(state.inviter);
        o["invitation"] = c1_number(state.invitation);
        o["inviter_name"] = c1_string(state.inviter_name);
        o["members"] = c1_array();
        for (const auto& member : state.members) {
            auto value = c1_object();
            auto& m = value.object();
            m["id"] = c1_number(member.id);
            m["name"] = c1_string(member.name);
            m["level"] = c1_number(member.level);
            m["hp"] = c1_number(member.hp);
            m["max_hp"] = c1_number(member.max_hp);
            m["map"] = c1_number(member.map);
            o["members"].array().push_back(std::move(value));
        }
        return out;
    }
    C1Json c1_option(const protocol::PlayerOptionsState& state) {
        auto out = c1_object();
        auto& o = out.object();
        o["request_id"] = c1_u64(state.request_id);
        o["target"] = c1_number(state.target);
        o["guild"] = c1_number(state.guild);
        o["name"] = c1_string(state.name);
        o["guild_name"] = c1_string(state.guild_name);
        o["status"] = c1_string(state.status);
        o["application_fee"] = c1_u64(state.application_fee);
        o["actions"] = c1_array();
        for (const auto action : state.actions) {
            o["actions"].array().push_back(c1_number(static_cast<unsigned>(action)));
        }
        return out;
    }

    struct C1Queues {
        std::vector<platform::Json> guild;
        std::vector<protocol::PlayerOptionsState> options;
        std::vector<protocol::PartyItemNotice> party;
        std::vector<client::PendingInfoMessage> infos;
        std::vector<client::PendingChat> chats;
        std::vector<client::PendingMapState> maps;
    };

    template <typename T, typename Take> std::vector<T> c1_drain(Take take) {
        std::vector<T> out;
        T value;
        // Only small authored batches are used here. Existing cap tests stay
        // separate.
        for (unsigned n = 0; n < 1024; ++n) {
            if (!take(value)) {
                return out;
            }
            out.push_back(std::move(value));
        }
        expect(false, "C1 authored queue drains within its bounded watchdog");
        return out;
    }

    C1Queues c1_observe(client::NetClient& net,
                        const SessionState& session,
                        std::string_view name,
                        bool snapshot,
                        std::initializer_list<std::uint32_t> players = {}) {
        C1Queues queues;
        queues.guild =
            c1_drain<platform::Json>([&](auto& out) { return net.take_guild_view(out); });
        queues.options = c1_drain<protocol::PlayerOptionsState>(
            [&](auto& out) { return net.take_player_options(out); });
        queues.party = c1_drain<protocol::PartyItemNotice>(
            [&](auto& out) { return net.take_party_item(out); });
        queues.infos = c1_drain<client::PendingInfoMessage>(
            [&](auto& out) { return net.take_info_message(out); });
        queues.chats = c1_drain<client::PendingChat>([&](auto& out) { return net.take_chat(out); });
        queues.maps =
            c1_drain<client::PendingMapState>([&](auto& out) { return net.take_map_state(out); });
        if (!snapshot) {
            return queues;
        }
        auto record = c1_object();
        auto& o = record.object();
        o["checkpoint"] = c1_string(std::string(name));
        o["connected"] = c1_bool(net.connected());
        o["transport_alive"] = c1_bool(session.alive);
        o["transport_eof"] = c1_bool(session.eof);
        o["transport_closes"] = c1_number(session.closes);
        o["transport_sends"] = c1_number(session.sends);
        o["transport_sent"] = c1_array();
        for (const auto byte : session.sent) {
            o["transport_sent"].array().push_back(c1_number(byte));
        }
        o["local_player_id"] = c1_number(net.local_player_id());
        o["trade_serial"] = c1_u64(net.trade_serial());
        o["market_serial"] = c1_u64(net.trading_post_serial());
        o["guild_serial"] = c1_u64(net.guild_serial());
        o["collection_pending"] = c1_bool(net.trading_post_collection_pending());
        o["trade"] = c1_trade(net.trade());
        o["market"] = c1_market(net.trading_post());
        o["party"] = c1_party(net.party());
        o["guild_view"] = net.guild_view();
        const auto identity = net.char_info_stats();
        o["own_guild"] = c1_string(identity.guild);
        o["own_title"] = c1_string(identity.guild_pos);
        o["display"] = c1_array();
        for (const auto id : players) {
            auto display = c1_object();
            display.object()["player"] = c1_number(id);
            display.object()["name"] = c1_string(net.guild_name(id));
            display.object()["title"] = c1_string(net.guild_title(id));
            o["display"].array().push_back(std::move(display));
        }
        o["guild_queue"] = c1_array();
        for (const auto& view : queues.guild) {
            o["guild_queue"].array().push_back(view);
        }
        o["options_queue"] = c1_array();
        for (const auto& option : queues.options) {
            o["options_queue"].array().push_back(c1_option(option));
        }
        o["party_queue"] = c1_array();
        for (const auto& notice : queues.party) {
            auto value = c1_object();
            auto& n = value.object();
            n["party"] = c1_number(notice.party);
            n["recipient"] = c1_number(notice.recipient);
            n["recipient_name"] = c1_string(notice.recipient_name);
            n["item_index"] = c1_number(notice.item_index);
            n["count"] = c1_number(notice.count);
            o["party_queue"].array().push_back(std::move(value));
        }
        o["infos"] = c1_array();
        for (const auto& info : queues.infos) {
            auto value = c1_object();
            value.object()["text"] = c1_string(info.text);
            value.object()["kind"] =
                info.kind ? c1_number(static_cast<unsigned>(*info.kind)) : C1Json{};
            auto color = c1_array();
            if (info.color) {
                for (const auto channel : *info.color) {
                    color.array().push_back(c1_number(channel));
                }
            }
            value.object()["color"] = info.color ? std::move(color) : C1Json{};
            o["infos"].array().push_back(std::move(value));
        }
        o["chats"] = c1_array();
        for (const auto& chat : queues.chats) {
            auto value = c1_object();
            value.object()["speaker"] = c1_string(chat.speaker);
            value.object()["text"] = c1_string(chat.text);
            o["chats"].array().push_back(std::move(value));
        }
        o["maps"] = c1_array();
        for (const auto& map : queues.maps) {
            auto value = c1_object();
            value.object()["map"] = c1_number(map.map);
            value.object()["phase"] = c1_number(map.phase);
            value.object()["active_mask"] = c1_number(map.active_mask);
            value.object()["timer"] = c1_number(static_cast<unsigned>(map.timer));
            value.object()["timer_seconds"] = c1_number(map.timer_seconds);
            o["maps"].array().push_back(std::move(value));
        }
        std::cout << "c1 " << C1Json::stringify(record, 0) << '\n';
        return queues;
    }

    bool c1_same(const C1Json& a, const C1Json& b) {
        return C1Json::stringify(a, 0) == C1Json::stringify(b, 0);
    }
    void c1_expect_infos(const C1Queues& queues,
                         const std::vector<client::PendingInfoMessage>& expected) {
        expect(queues.infos.size() == expected.size(), "C1 receipt count is exact");
        for (std::size_t i = 0; i < std::min(queues.infos.size(), expected.size()); ++i) {
            expect(queues.infos[i].text == expected[i].text &&
                       queues.infos[i].kind == expected[i].kind &&
                       queues.infos[i].color == expected[i].color,
                   "C1 receipt text, type, color and authored order are exact");
        }
    }
    client::PendingInfoMessage c1_item_info(int index, int count) {
        return {"You got " + (count > 1 ? std::to_string(count) + "x " : "") + "item #" +
                    std::to_string(index),
                protocol::BattleMsgKind::ItemGet,
                std::array<float, 3>{0.40f, 0.95f, 0.48f}};
    }
    client::PendingInfoMessage c1_money_info(int count) {
        return {"You got " + std::to_string(count) + " geons",
                protocol::BattleMsgKind::MoneyGet,
                std::array<float, 3>{1.f, 1.f, 1.f}};
    }

    protocol::TradeItem c1_item(unsigned ordinal, std::uint16_t slot) {
        protocol::TradeItem item;
        item.iid = 100 + ordinal;
        item.index = 23000 + static_cast<int>(ordinal);
        item.count = 1 + static_cast<int>(ordinal);
        item.slot = slot;
        item.flags = protocol::kTradeable | protocol::kTradeAccountBound |
                     protocol::kTradeCharacterBound | protocol::kTradeEquipped;
        for (std::size_t a = 0; a < item.attributes.size(); ++a) {
            item.attributes[a] = static_cast<int>(ordinal * 100 + a) - 7;
        }
        item.pet_expire_at = 9007199254740993ULL + ordinal;
        item.ride_expire_at = 9007199254741993ULL + ordinal;
        return item;
    }
    protocol::TradeState c1_full_trade() {
        protocol::TradeState state;
        state.phase = protocol::TradePhase::Open;
        state.token = 91;
        state.revision = 17;
        state.acknowledged = 7;
        state.inventory_slots = 216;
        state.partner = "Owned partner";
        state.status = "Hidden while locked";
        state.inviter = true;
        state.confirmed = true;
        state.partner_confirmed = false;
        state.money = 99;
        state.partner_money = 17;
        state.own = {c1_item(0, 3)};
        state.other = {c1_item(1, 5), c1_item(2, 7)};
        state.available = {c1_item(3, 215), c1_item(4, 0)};
        return state;
    }
    protocol::TradingPostState c1_full_market() {
        protocol::TradingPostState state;
        state.view = protocol::TradingPostView::Collection;
        state.acknowledged = 9007199254740993ULL;
        state.page = 1;
        state.total_pages = 3;
        state.more = true;
        state.purchased = 4;
        state.status = "Owned market page";
        for (unsigned i = 0; i < 3; ++i) {
            protocol::TradingPostEntry entry;
            entry.id = 9007199254740999ULL + i;
            entry.item = c1_item(i, 0);
            entry.unit_price = i == 0 ? 0 : 2000000000;
            entry.delivery = static_cast<protocol::TradingPostDelivery>(i);
            state.entries.push_back(entry);
        }
        return state;
    }
    protocol::PartyState c1_full_party(std::uint32_t id = 7, std::uint32_t local = 7) {
        protocol::PartyState state;
        state.id = id;
        state.leader = local;
        state.loot = protocol::PartyLoot::Random;
        state.inviter = 81;
        state.invitation = 91;
        state.inviter_name = "Owned inviter";
        state.members = {{local, "Owned local", 30, 90, 100, 0},
                         {local + 1, "Owned remote", 31, 80, 110, 1}};
        return state;
    }
    protocol::PlayerOptionsState c1_full_options() {
        protocol::PlayerOptionsState state;
        state.request_id = 9007199254740993ULL;
        state.target = 8;
        state.guild = 55;
        state.name = u8"Owned target é";
        state.guild_name = "Owned guild";
        state.status = "Owned options result";
        state.application_fee = 9007199254741993ULL;
        state.actions = {protocol::PlayerAction::ApplyGuild, protocol::PlayerAction::InviteParty};
        return state;
    }
    Bytes c1_guild(std::uint64_t request, std::string_view json, bool display = false) {
        protocol::GuildResponse response;
        response.request_id = request;
        response.kind = display ? protocol::GuildMessage::Display : protocol::GuildMessage::View;
        expect(C1Json::parse(json, response.data), "C1 authored guild JSON parses");
        return protocol::frame_guild_response(response);
    }
    Bytes c1_payload_frame(std::uint8_t opcode, Bytes payload) {
        protocol::PacketWriter writer;
        writer.payload() = std::move(payload);
        return writer.frame(opcode);
    }
    Bytes c1_payload(const Bytes& frame) {
        expect(frame.size() >= protocol::kPacketReservedLength, "C1 fixture frame is complete");
        if (frame.size() < protocol::kPacketReservedLength) {
            return {};
        }
        return {frame.begin() + protocol::kPacketReservedLength, frame.end()};
    }
    Bytes c1_reframe(const Bytes& frame, std::size_t size, bool trailing = false) {
        auto payload = c1_payload(frame);
        expect(size <= payload.size(), "C1 truncation stays within its owned frame");
        if (frame.size() < protocol::kPacketReservedLength || size > payload.size()) {
            return {};
        }
        payload.resize(size);
        if (trailing) {
            payload.push_back(0x5a);
        }
        return c1_payload_frame(frame[2], std::move(payload));
    }
    std::array<Bytes, 9> c1_online_frames() {
        return {protocol::frame_trading_post_pickup({23000, 2, 17}),
                protocol::frame_trading_post_collection(true),
                protocol::frame_trading_post_state(c1_full_market()),
                protocol::frame_trade_state(c1_full_trade()),
                protocol::frame_party_item({7, 8, "Owned remote", 23001, 3}),
                c1_guild(95, R"({"own":{"name":"OwnedGuild","title":"Member"}})"),
                protocol::frame_party_state(c1_full_party()),
                protocol::frame_player_options_state(c1_full_options()),
                protocol::Chatting::frame_s2c("Owned target speaker", "Owned target text")};
    }

    void test_c1_online_malformed(bool snapshot) {
        const auto valid = c1_online_frames();
        for (std::size_t family = 0; family < valid.size(); ++family) {
            expect(valid[family].size() > protocol::kPacketReservedLength,
                   "C1 every family has a valid nonempty frame");
            if (valid[family].size() <= protocol::kPacketReservedLength) {
                continue;
            }
            const auto size = valid[family].size() - protocol::kPacketReservedLength;
            for (int boundary = 0; boundary < 3; ++boundary) {
                client::NetClient net;
                const auto session = attach(net);
                const auto trade_before = net.trade_serial();
                const auto market_before = net.trading_post_serial();
                const auto guild_before = net.guild_serial();
                const bool trailing = boundary == 2;
                const auto bytes = boundary == 0 ? 0 : trailing ? size : size - 1;
                queue(session, protocol::Chatting::frame_s2c("Owned speaker", "line 201"));
                queue(session, c1_reframe(valid[family], bytes, trailing));
                queue(session, protocol::Chatting::frame_s2c("Owned speaker", "line 202"));
                net.update(0, 32);
                const auto q = c1_observe(net,
                                          *session,
                                          "malformed-" + std::to_string(family) + "-" +
                                              std::to_string(boundary),
                                          snapshot);
                const bool accepted_chat = family == 8 && trailing;
                expect(q.chats.size() == (accepted_chat ? 3u : 2u) &&
                           q.chats.front().speaker == "Owned speaker" &&
                           q.chats.front().text == "line 201" && q.chats.back().text == "line 202",
                       "C1 malformed neighbors preserve prefix and suffix chats in order");
                if (accepted_chat && q.chats.size() == 3) {
                    expect(q.chats[1].speaker == "Owned target speaker" &&
                               q.chats[1].text == "Owned target text",
                           "C1 Chatting deliberately accepts bytes after both strings");
                }
                const bool closes = family < 4;
                expect(net.connected() == !closes && session->closes == (closes ? 1u : 0u) &&
                           session->sent.empty(),
                       "C1 only malformed market/trade closes without stopping suffix "
                       "dispatch");
                const auto cleanup = closes ? 1u : 0u;
                expect(net.trade_serial() == trade_before + cleanup &&
                           net.trading_post_serial() == market_before + cleanup &&
                           net.guild_serial() == guild_before + cleanup && q.infos.empty() &&
                           q.options.empty() && q.guild.empty() && q.party.empty(),
                       "C1 malformed payloads produce no partial effects; close cleanup "
                       "runs once");
            }
        }
        // All fixed pickup field boundaries; no invented partial item/money reward.
        for (std::size_t prefix = 0; prefix < 16; ++prefix) {
            client::NetClient net;
            const auto session = attach(net);
            queue(session, c1_reframe(valid[0], prefix));
            queue(session, protocol::Chatting::frame_s2c("Owned speaker", "line 203"));
            net.update(0, 32);
            const auto q =
                c1_observe(net, *session, "pickup-prefix-" + std::to_string(prefix), snapshot);
            expect(!net.connected() && session->closes == 1 && q.infos.empty() &&
                       q.chats.size() == 1 && q.chats[0].text == "line 203",
                   "C1 every truncated fixed pickup closes without rewarding and still "
                   "drains");
        }
        // Syntactically complete but invalid header values on each family.
        for (std::size_t family = 1; family < valid.size() - 1; ++family) {
            client::NetClient net;
            const auto session = attach(net);
            auto payload = c1_payload(valid[family]);
            if (payload.empty()) {
                continue;
            }
            if (family == 4) {
                // UpdateParty begins with the nonzero party ID, without a version byte.
                std::fill_n(payload.begin(), 4, 0);
            } else if (family == 6) {
                // PartyInfo begins with id/leader, then its one-byte loot enum.
                payload[8] = 0xff;
            } else {
                payload[0] = 0xff;
            }
            queue(session, c1_payload_frame(valid[family][2], std::move(payload)));
            queue(session, protocol::Chatting::frame_s2c("Owned speaker", "line 204"));
            net.update(0, 32);
            const auto q =
                c1_observe(net, *session, "invalid-header-" + std::to_string(family), snapshot);
            expect(net.connected() == (family >= 4) && q.infos.empty() && q.options.empty() &&
                       q.guild.empty() && q.party.empty() && q.chats.size() == 1,
                   "C1 invalid complete headers preserve each family's close-or-ignore "
                   "policy");
        }
    }

    void test_c1_trade(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        auto state = c1_full_trade();
        const auto initial = net.trade_serial();
        for (const auto phase : {protocol::TradePhase::Invitation, protocol::TradePhase::Open}) {
            state.phase = phase;
            queue(session, protocol::frame_trade_state(state));
            net.update(0, 32);
            auto expected = state;
            for (auto& item : expected.available) {
                const auto prefix = item.attribute(protocol::TradeAttribute::Prefix);
                const auto level = item.attribute(protocol::TradeAttribute::AnimalLevel);
                item.attributes.fill(0);
                item.attributes[static_cast<std::size_t>(protocol::TradeAttribute::Prefix)] =
                    prefix;
                item.attributes[static_cast<std::size_t>(protocol::TradeAttribute::AnimalLevel)] =
                    level;
                item.pet_expire_at = item.ride_expire_at = 0;
            }
            const auto q =
                c1_observe(net,
                           *session,
                           "trade-locked-" + std::to_string(static_cast<unsigned>(phase)),
                           snapshot);
            expect(net.trade().locked() && c1_same(c1_trade(net.trade()), c1_trade(expected)) &&
                       q.infos.empty(),
                   "C1 locked trade retains every offered field and compact "
                   "available-item defaults");
        }
        expect(net.trade_serial() == initial + 2,
               "C1 one trade serial per accepted locked snapshot");
        state.phase = protocol::TradePhase::Completed;
        state.status = "Owned completed status";
        for (unsigned repeat = 0; repeat < 2; ++repeat) {
            queue(session, protocol::frame_trade_state(state));
            net.update(0, 32);
            const auto q =
                c1_observe(net, *session, "trade-completed-" + std::to_string(repeat), snapshot);
            c1_expect_infos(q,
                            {c1_item_info(23001, 2),
                             c1_item_info(23002, 3),
                             c1_money_info(17),
                             {state.status, std::nullopt, std::nullopt}});
            expect(net.trade_serial() == initial + 3 + repeat && !net.trade().locked() &&
                       net.trade().own.size() == 1 && net.trade().other.size() == 2,
                   "C1 repeated completion deliberately repeats receipts and state "
                   "replacement");
        }
        state.phase = protocol::TradePhase::Closed;
        state.status = "Owned closed status";
        state.other.clear();
        state.partner_money = 0;
        queue(session, protocol::frame_trade_state(state));
        net.update(0, 32);
        const auto closed = c1_observe(net, *session, "trade-closed", snapshot);
        c1_expect_infos(closed, {{state.status, std::nullopt, std::nullopt}});
        state.status.clear();
        queue(session, protocol::frame_trade_state(state));
        net.update(0, 32);
        expect(c1_observe(net, *session, "trade-closed-empty", snapshot).infos.empty(),
               "C1 empty unlocked status and zero partner money do not create receipts");
    }

    void test_c1_market(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        auto state = c1_full_market();
        auto expected = state;
        for (auto& entry : expected.entries) {
            // Market entries intentionally do not disclose source IID/cell/binding
            // flags.
            entry.item.iid = 0;
            entry.item.slot = 0;
            entry.item.flags = protocol::kTradeable;
        }
        const auto before = net.trading_post_serial();
        queue(session, protocol::frame_trading_post_state(state));
        net.update(0, 32);
        const auto page = c1_observe(net, *session, "market-page", snapshot);
        expect(c1_same(c1_market(net.trading_post()), c1_market(expected)) &&
                   net.trading_post_serial() == before + 1 && page.infos.empty(),
               "C1 all market entries, metadata, delivery, paging and u64 identities "
               "survive");
        const auto page_serial = net.trading_post_serial();
        unsigned indicator = 0;
        for (const bool pending : {true, false, true}) {
            queue(session, protocol::frame_trading_post_collection(pending));
            net.update(0, 32);
            const auto q =
                c1_observe(net, *session, "collection-" + std::to_string(indicator++), snapshot);
            expect(net.trading_post_collection_pending() == pending &&
                       net.trading_post_serial() == page_serial &&
                       c1_same(c1_market(net.trading_post()), c1_market(expected)) &&
                       q.infos.empty(),
                   "C1 collection indicator is independent of the open market page and "
                   "serial");
        }
        queue(session, protocol::frame_trading_post_pickup({23000, 1, 0}));
        queue(session, protocol::frame_trading_post_pickup({0, 0, 23}));
        queue(session, protocol::frame_trading_post_pickup({23002, 3, 31}));
        net.update(0, 32);
        const auto receipts = c1_observe(net, *session, "market-receipts", snapshot);
        c1_expect_infos(
            receipts,
            {c1_item_info(23000, 1), c1_money_info(23), c1_item_info(23002, 3), c1_money_info(31)});
        expect(net.trading_post_serial() == page_serial && net.trading_post_collection_pending() &&
                   c1_same(c1_market(net.trading_post()), c1_market(expected)),
               "C1 committed pickup receipts do not mutate the displayed page or "
               "collection state");
        client::PendingInsertItem unexpected_insert;
        expect(!net.take_insert_item(unexpected_insert),
               "C1 pickup item/money receipts do not emit inventory insertion events");
        auto legacy = c1_payload(protocol::frame_trading_post_state(state));
        expect(legacy.size() >= 18, "C1 version-two page contains its explicit total-pages field");
        if (legacy.size() >= 18) {
            legacy[0] = 1;
            legacy.erase(legacy.begin() + 14, legacy.begin() + 18);
            queue(session,
                  c1_payload_frame(static_cast<std::uint8_t>(protocol::S2C::TradingPost), legacy));
            net.update(0, 32);
            expected.total_pages = 0;
            expect(c1_same(c1_market(net.trading_post()), c1_market(expected)) &&
                       net.trading_post_serial() == page_serial + 1,
                   "C1 legacy market page omits only the total count and still "
                   "replaces state");
            (void) c1_observe(net, *session, "market-version-one", snapshot);
        }
        queue(session, protocol::frame_trading_post_state({}));
        net.update(0, 32);
        expect(net.trading_post().entries.empty() && net.trading_post().page == 0,
               "C1 an accepted empty page replaces prior entries");
        (void) c1_observe(net, *session, "market-empty", snapshot);
    }

    void test_c1_guild(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        constexpr auto request = 9007199254740993ULL;
        const auto before = net.guild_serial();
        const std::string_view json =
            R"({"own":{"name":"OwnedGuild","title":"Member"},"_request_id":7,"members":[{"id":8,"rank":2}],"nested":{"flag":true}})";
        queue(session, c1_guild(request, json));
        net.update(0, 32);
        const auto q = c1_observe(net, *session, "guild-view", snapshot, {7, 8});
        expect(net.guild_serial() == before + 1 && q.guild.size() == 1 &&
                   net.guild_view().at("_request_id").as_string() == "9007199254740993" &&
                   q.guild[0].at("_request_id").as_string() == "9007199254740993" &&
                   net.char_info_stats().guild == "OwnedGuild" &&
                   net.char_info_stats().guild_pos == "Member",
               "C1 guild view replaces own identity and preserves exact request ID "
               "as a string");
        const auto view = net.guild_view();
        for (const auto display :
             {R"({"player":7,"visible":true,"name":"VisibleGuild","title":"Officer"})",
              R"({"player":0,"visible":true,"name":"Ignored","title":"Ignored"})",
              R"({"player":8,"visible":false,"name":"Hidden","title":"Hidden"})"}) {
            queue(session, c1_guild(41, display, true));
        }
        net.update(0, 32);
        const auto displays = c1_observe(net, *session, "guild-displays", snapshot, {0, 7, 8});
        expect(net.guild_serial() == before + 1 && displays.guild.empty() &&
                   c1_same(net.guild_view(), view) && net.char_info_stats().guild == "OwnedGuild" &&
                   net.guild_name(7) == "VisibleGuild" && net.guild_title(7) == "Officer" &&
                   net.guild_name(0).empty() && net.guild_name(8).empty() &&
                   net.guild_title(8).empty(),
               "C1 display updates have separate visibility, zero-ID and serial/view "
               "rules");
        queue(session, c1_guild(42, R"({"player":7,"visible":false,"name":"Hidden"})", true));
        queue(session,
              c1_guild(
                  42, R"({"player":8,"visible":true,"name":"Independent","title":"Guest"})", true));
        queue(session, c1_guild(43, R"({"own":{},"members":[]})"));
        net.update(0, 32);
        const auto cleared = c1_observe(net, *session, "guild-empty-own", snapshot, {7, 8});
        expect(net.guild_serial() == before + 2 && cleared.guild.size() == 1 &&
                   net.char_info_stats().guild.empty() && net.char_info_stats().guild_pos.empty() &&
                   net.guild_name(7).empty() && net.guild_name(8) == "Independent" &&
                   net.guild_title(8) == "Guest",
               "C1 empty own guild clears labels; hidden display replaces the old "
               "visible display");
        const auto retained = net.guild_view();
        auto malformed = c1_payload(c1_guild(44, json));
        if (!malformed.empty()) {
            malformed.pop_back();
            queue(session,
                  c1_payload_frame(static_cast<std::uint8_t>(protocol::S2C::Guild), malformed));
        }
        for (const auto text : {"{not-json", "[]", "{}"}) {
            protocol::PacketWriter malformed_json;
            malformed_json.write_u8(1);
            malformed_json.write_u8(std::string_view(text) == "{}" ? 2 : 0);
            malformed_json.write_u64(45);
            malformed_json.write_string(text);
            queue(session, malformed_json.frame(static_cast<std::uint8_t>(protocol::S2C::Guild)));
        }
        net.update(0, 32);
        const auto ignored = c1_observe(net, *session, "guild-invalid-retains", snapshot, {7, 8});
        expect(net.connected() && net.guild_serial() == before + 2 && ignored.guild.empty() &&
                   c1_same(net.guild_view(), retained),
               "C1 malformed guild response preserves prior view and does not "
               "enqueue a new one");
    }

    void test_c1_party(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        auto party = c1_full_party();
        queue(session, create(7, true));
        queue(session, protocol::frame_party_state(party));
        queue(session, protocol::frame_party_item({7, 8, "Owned remote", 23001, 3}));
        queue(session, protocol::frame_party_item({7, 7, "Owned local", 23002, 4}));
        queue(session, protocol::frame_party_item({9, 8, "Wrong party", 23003, 5}));
        queue(session, protocol::frame_party_item({7, 99, "Unknown member", 23004, 6}));
        for (const auto& invalid :
             std::vector<protocol::PartyItemNotice>{{0, 8, "No party", 23001, 3},
                                                    {7, 0, "No recipient", 23001, 3},
                                                    {7, 8, "", 23001, 3},
                                                    {7, 8, "Zero quantity", 23001, 0}}) {
            queue(session, protocol::frame_party_item(invalid));
        }
        net.update(0, 32);
        const auto q = c1_observe(net, *session, "party-filter", snapshot);
        expect(c1_same(c1_party(net.party()), c1_party(party)) && q.party.size() == 2 &&
                   q.party[0].recipient == 8 && q.party[0].item_index == 23001 &&
                   q.party[0].count == 3 && q.party[1].recipient == 99 &&
                   q.party[1].recipient_name == "Unknown member",
               "C1 party notices filter current party and local recipient, without a "
               "membership gate");
        // A prior accepted notice survives later membership changes in the same
        // batch.
        queue(session, protocol::frame_party_item({7, 8, "Before replace", 23005, 7}));
        party.id = 9;
        party.members[1].hp = -3;
        party.members[1].map = -1;
        queue(session, protocol::frame_party_state(party));
        queue(session, protocol::frame_party_item({7, 8, "Old party", 23006, 8}));
        queue(session, protocol::frame_party_item({9, 8, "New party", 23007, 9}));
        net.update(0, 32);
        const auto replaced = c1_observe(net, *session, "party-replace-order", snapshot);
        expect(c1_same(c1_party(net.party()), c1_party(party)) && replaced.party.size() == 2 &&
                   replaced.party[0].recipient_name == "Before replace" &&
                   replaced.party[1].recipient_name == "New party",
               "C1 party state replacement immediately changes filtering without "
               "erasing accepted notices");
        const auto frame = protocol::frame_party_state(party);
        queue(session, c1_reframe(frame, c1_payload(frame).size() - 1));
        auto duplicate_member = party;
        duplicate_member.members[1].id = duplicate_member.members[0].id;
        queue(session, protocol::frame_party_state(duplicate_member));
        net.update(0, 32);
        expect(c1_same(c1_party(net.party()), c1_party(party)) && net.connected(),
               "C1 malformed party snapshot retains all prior member fields");
        (void) c1_observe(net, *session, "party-invalid-retains", snapshot);
        queue(session, protocol::frame_party_state({}));
        queue(session, protocol::frame_party_item({9, 8, "After clear", 23008, 10}));
        net.update(0, 32);
        const auto empty = c1_observe(net, *session, "party-empty", snapshot);
        expect(net.party().id == 0 && net.party().members.empty() && empty.party.empty(),
               "C1 empty party snapshot replaces membership and rejects former-party "
               "notices");
    }

    void test_c1_options(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        const auto state = c1_full_options();
        const auto frame = protocol::frame_player_options_state(state);
        queue(session, frame);
        net.update(0, 32);
        const auto q = c1_observe(net, *session, "options-unsolicited", snapshot);
        expect(q.options.size() == 1 && c1_same(c1_option(q.options[0]), c1_option(state)) &&
                   session->sent.empty(),
               "C1 complete options response is queued even without a locally issued "
               "request");
        auto duplicate = c1_payload(frame);
        if (duplicate.size() >= 2) {
            duplicate.back() = duplicate[duplicate.size() - 2];
        }
        queue(session, frame);
        auto feedback = state;
        ++feedback.request_id;
        feedback.actions.clear();
        feedback.status = "Owned no-action feedback";
        queue(session, protocol::frame_player_options_state(feedback));
        queue(session,
              c1_payload_frame(static_cast<std::uint8_t>(protocol::S2C::PlayerOptions), duplicate));
        queue(session, c1_reframe(frame, c1_payload(frame).size() - 1));
        queue(session, c1_reframe(frame, c1_payload(frame).size(), true));
        net.update(0, 32);
        const auto retained = c1_observe(net, *session, "options-malformed-neighbors", snapshot);
        expect(net.connected() && retained.options.size() == 2 &&
                   c1_same(c1_option(retained.options[0]), c1_option(state)) &&
                   c1_same(c1_option(retained.options[1]), c1_option(feedback)),
               "C1 duplicate actions, truncation and trailing bytes do not disturb a "
               "valid queued option");
    }

    void test_c1_cross_family(bool snapshot) {
        client::NetClient first;
        client::NetClient second;
        const auto a = attach(first);
        const auto b = attach(second);
        auto trade = c1_full_trade();
        trade.phase = protocol::TradePhase::Completed;
        trade.status = "First completed";
        trade.other.resize(1);
        auto market = c1_full_market();
        const auto options = c1_full_options();
        queue(a, create(7, true));
        queue(a, protocol::frame_party_state(c1_full_party()));
        queue(a, protocol::frame_trade_state(trade));
        queue(a, protocol::MapState::frame({2, 3, 5, protocol::MapStateTimer::Lord, 17}));
        queue(a, protocol::frame_trading_post_pickup({23008, 2, 29}));
        queue(a, protocol::Chatting::frame_s2c("First", "before"));
        queue(a, c1_guild(1, R"({"own":{"name":"FirstGuild","title":"FirstTitle"}})"));
        queue(a, protocol::frame_player_options_state(options));
        queue(a, protocol::frame_party_item({7, 8, "First remote", 23009, 11}));
        queue(a, protocol::frame_trading_post_state(market));
        queue(b, create(17, true));
        queue(b, protocol::frame_party_state(c1_full_party(17, 17)));
        queue(b, c1_guild(2, R"({"own":{"name":"SecondGuild","title":"SecondTitle"}})"));
        queue(b, protocol::frame_party_item({17, 18, "Second remote", 23010, 12}));
        queue(b, protocol::Chatting::frame_s2c("Second", "isolated"));
        first.update(0, 32);
        second.update(0, 32);
        const auto qa = c1_observe(first, *a, "isolation-first", snapshot, {7, 17});
        const auto qb = c1_observe(second, *b, "isolation-second", snapshot, {7, 17});
        c1_expect_infos(qa,
                        {c1_item_info(23001, 2),
                         c1_money_info(17),
                         {"First completed", std::nullopt, std::nullopt},
                         c1_item_info(23008, 2),
                         c1_money_info(29)});
        expect(qa.options.size() == 1 && qb.options.empty() && qa.guild.size() == 1 &&
                   qb.guild.size() == 1 && qa.party.size() == 1 && qb.party.size() == 1 &&
                   qa.party[0].recipient == 8 && qb.party[0].recipient == 18 &&
                   qa.chats.size() == 1 && qa.chats[0].speaker == "First" && qb.chats.size() == 1 &&
                   qb.chats[0].speaker == "Second" && qb.infos.empty() &&
                   first.local_player_id() == 7 && second.local_player_id() == 17 &&
                   first.char_info_stats().guild == "FirstGuild" &&
                   second.char_info_stats().guild == "SecondGuild" && qa.maps.size() == 1 &&
                   qa.maps[0].map == 2 && qa.maps[0].phase == 3 && qa.maps[0].active_mask == 5 &&
                   qa.maps[0].timer == protocol::MapStateTimer::Lord &&
                   qa.maps[0].timer_seconds == 17 && qb.maps.empty() &&
                   second.trading_post().entries.empty() && second.trade().other.empty(),
               "C1 interleaved online families preserve per-queue order and complete "
               "client isolation");
        // Closing the first during dispatch still handles the second's following
        // batch.
        queue(a, c1_reframe(protocol::frame_trading_post_collection(true), 0));
        queue(a, protocol::Chatting::frame_s2c("First", "after close"));
        queue(b, protocol::frame_trading_post_collection(true));
        queue(b, protocol::Chatting::frame_s2c("Second", "after first closed"));
        first.update(0, 32);
        second.update(0, 32);
        const auto closed = c1_observe(first, *a, "isolation-first-closed", snapshot);
        const auto alive = c1_observe(second, *b, "isolation-second-alive", snapshot);
        expect(!first.connected() && second.connected() && closed.chats.size() == 1 &&
                   alive.chats.size() == 1 && second.trading_post_collection_pending() &&
                   second.char_info_stats().guild == "SecondGuild" && second.party().id == 17,
               "C1 one client's malformed close/cleanup does not erase the other's "
               "independent views");
    }

    void test_c1_atomic_trade(bool snapshot) {
        for (unsigned boundary = 0; boundary < 4; ++boundary) {
            client::NetClient net;
            const auto session = attach(net);
            const auto serial = net.trade_serial();
            auto state = c1_full_trade();
            state.phase = protocol::TradePhase::Completed;
            if (boundary == 0) {
                state.own[0].count = 0;
            } else if (boundary == 1) {
                state.other[1].iid = state.other[0].iid;
            } else if (boundary == 2) {
                // Fail only after the completed receipt items were all parsed locally.
                state.available[1].flags = 16;
            } else {
                state.phase = protocol::TradePhase::Open;
                state.token = 0;
            }
            queue(session, protocol::frame_trade_state(state));
            queue(session, protocol::Chatting::frame_s2c("Atomic", "suffix"));
            net.update(0, 32);
            const auto q =
                c1_observe(net, *session, "trade-atomic-" + std::to_string(boundary), snapshot);
            expect(!net.connected() && session->closes == 1 && net.trade_serial() == serial + 1 &&
                       net.trade().other.empty() && q.infos.empty() && q.chats.size() == 1 &&
                       q.chats[0].text == "suffix",
                   "C1 invalid completed/locked trade cannot publish partial state or rewards");
        }
    }

    void test_c1_closed_suffix(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        const auto trade_before = net.trade_serial();
        const auto market_before = net.trading_post_serial();
        const auto guild_before = net.guild_serial();
        auto completed = c1_full_trade();
        completed.phase = protocol::TradePhase::Completed;
        completed.other.resize(1);
        completed.status = "Suffix completed";
        queue(session, create(7, true));
        queue(session, c1_reframe(protocol::frame_trading_post_collection(true), 0));
        queue(session, protocol::frame_trade_state(completed));
        queue(session, c1_guild(9, R"({"own":{"name":"SuffixGuild","title":"Member"}})"));
        queue(session, protocol::frame_player_options_state(c1_full_options()));
        queue(session, protocol::frame_party_state(c1_full_party()));
        queue(session, protocol::frame_party_item({7, 8, "Suffix remote", 23007, 2}));
        queue(session, protocol::Chatting::frame_s2c("Suffix", "still drained"));
        net.update(0, 32);
        const auto q = c1_observe(net, *session, "closed-suffix-cleanup", snapshot);
        c1_expect_infos(q,
                        {c1_item_info(23001, 2),
                         c1_money_info(17),
                         {"Suffix completed", std::nullopt, std::nullopt}});
        expect(
            !net.connected() && session->closes == 1 && net.trade_serial() == trade_before + 2 &&
                net.trading_post_serial() == market_before + 1 &&
                net.guild_serial() == guild_before + 2 && q.guild.empty() && q.options.empty() &&
                q.party.empty() && q.chats.size() == 1 && q.chats[0].text == "still drained" &&
                net.trade().other.empty() && net.guild_view().is_null() && net.party().id == 0,
            "C1 suffix parsing precedes one cleanup: receipts/chats survive, online views do not");
    }

    void test_c1_online_packets(bool snapshot) {
        test_c1_online_malformed(snapshot);
        test_c1_trade(snapshot);
        test_c1_market(snapshot);
        test_c1_guild(snapshot);
        test_c1_party(snapshot);
        test_c1_options(snapshot);
        test_c1_cross_family(snapshot);
        test_c1_atomic_trade(snapshot);
        test_c1_closed_suffix(snapshot);
    }
    // C1_BASELINE_DRAFT_END
    // C2_BASELINE_DRAFT_BEGIN
    /*
     * OpenKal - an open source KalOnline client and server.
     * Copyright (C) 2026 OpenKal contributors
     *
     * This program is free software: you can redistribute it and/or modify
     * it under the terms of the GNU General Public License as published by
     * the Free Software Foundation, either version 3 of the License, or
     * (at your option) any later version.
     */

    // PREPARATION ONLY: public packet fixtures, not compiled or executed.
    // Insert inside the existing anonymous namespace, after the frozen C1 block.
    using C2Json = platform::Json;

    C2Json c2_text(std::string value) {
        C2Json out;
        out.set_type(C2Json::Type::String);
        out.string() = std::move(value);
        return out;
    }
    C2Json c2_array() {
        C2Json out;
        out.set_type(C2Json::Type::Array);
        return out;
    }
    template <typename... Values> C2Json c2_fields(Values... values) {
        auto out = c2_array();
        (out.array().push_back(c2_text(std::to_string(values))), ...);
        return out;
    }
    template <typename Values> C2Json c2_numbers(const Values& values) {
        auto out = c2_array();
        for (const auto value : values) {
            out.array().push_back(c2_text(std::to_string(value)));
        }
        return out;
    }
    std::array<std::uint8_t, 14> c2_charm(const protocol::InsertCharmFields& value) {
        return {{value.x_attack,
                 value.x_magic,
                 value.x_hit,
                 value.x_defense,
                 value.x_dodge,
                 value.upgrade_level,
                 value.upgrade_rate,
                 value.mix_type,
                 value.bof,
                 value.qigong_perforated,
                 value.qigong_effect,
                 value.dragon_effect,
                 value.random_stat_type,
                 value.random_stat_value}};
    }
    C2Json c2_mail(const std::vector<protocol::MailEntry>& values) {
        auto out = c2_array();
        for (const auto& value : values) {
            auto entry = c2_fields(value.id,
                                   value.item_index,
                                   value.item_count,
                                   value.created_at,
                                   value.claimed,
                                   value.read);
            entry.array().push_back(c2_text(value.subject));
            entry.array().push_back(c2_text(value.body));
            out.array().push_back(std::move(entry));
        }
        return out;
    }
    bool c2_same_mail(const std::vector<protocol::MailEntry>& left,
                      const std::vector<protocol::MailEntry>& right) {
        return C2Json::stringify(c2_mail(left), 0) == C2Json::stringify(c2_mail(right), 0);
    }
    struct C2Queues {
        bool dirty = false;
        std::vector<protocol::MailClaimResult> claims;
        std::vector<client::PendingInsertItem> inserts;
        std::vector<client::PendingItemWear> wear;
        std::vector<protocol::RidingState> riding;
        std::vector<protocol::RidingCacheResult> cache;
        std::vector<std::uint32_t> trash;
        std::vector<protocol::ShopItemList> shop;
        std::vector<protocol::StorageInfo> storage;
        std::vector<protocol::UpdateItemInfoFields> animal;
        std::vector<client::PendingInfoMessage> infos;
        std::vector<client::PendingChat> chats;
    };
    template <typename Value, typename Take> std::vector<Value> c2_drain(Take take) {
        std::vector<Value> out;
        Value value{};
        for (unsigned i = 0; i < 1024; ++i) {
            if (!take(value)) {
                return out;
            }
            out.push_back(std::move(value));
        }
        expect(false, "C2 authored queues drain within their bounded watchdog");
        return out;
    }
    std::size_t c2_event_count(const C2Queues& q) {
        return q.claims.size() + q.inserts.size() + q.wear.size() + q.riding.size() +
               q.cache.size() + q.trash.size() + q.shop.size() + q.storage.size() + q.animal.size();
    }
    C2Queues c2_observe(client::NetClient& net,
                        const SessionState& session,
                        const std::string& name,
                        bool snapshot,
                        const protocol::UpdateItemInfoFields* consumed_animal = nullptr) {
        C2Queues q;
        q.dirty = net.take_mail_dirty();
        expect(!net.take_mail_dirty(), "C2 inbox dirty indication is consumed exactly once");
        q.claims = c2_drain<protocol::MailClaimResult>(
            [&](auto& out) { return net.take_mail_claim_result(out); });
        q.inserts = c2_drain<client::PendingInsertItem>(
            [&](auto& out) { return net.take_insert_item(out); });
        q.wear =
            c2_drain<client::PendingItemWear>([&](auto& out) { return net.take_item_wear(out); });
        q.riding =
            c2_drain<protocol::RidingState>([&](auto& out) { return net.take_riding_state(out); });
        q.cache = c2_drain<protocol::RidingCacheResult>(
            [&](auto& out) { return net.take_riding_cache_result(out); });
        q.trash = c2_drain<std::uint32_t>([&](auto& out) { return net.take_trash_item(out); });
        q.shop = c2_drain<protocol::ShopItemList>(
            [&](auto& out) { return net.take_shop_item_list(out); });
        q.storage =
            c2_drain<protocol::StorageInfo>([&](auto& out) { return net.take_storage_info(out); });
        q.animal = c2_drain<protocol::UpdateItemInfoFields>(
            [&](auto& out) { return net.take_update_item_info(out); });
        q.infos = c2_drain<client::PendingInfoMessage>(
            [&](auto& out) { return net.take_info_message(out); });
        q.chats = c2_drain<client::PendingChat>([&](auto& out) { return net.take_chat(out); });
        if (!snapshot) {
            return q;
        }
        C2Json record;
        record.set_type(C2Json::Type::Object);
        auto& out = record.object();
        out["checkpoint"] = c2_text(name);
        out["transport"] =
            c2_fields(net.connected(), session.alive, session.eof, session.closes, session.sends);
        out["sent"] = c2_numbers(session.sent);
        out["mail"] = c2_mail(net.mail());
        out["mail_dirty"] = c2_fields(q.dirty);
        out["egg"] = c2_fields(
            net.transform_egg_index(), net.transform_egg_level(), net.transform_egg_exp());
        out["already_consumed_animal"] = consumed_animal ? c2_fields(consumed_animal->iid,
                                                                     consumed_animal->index,
                                                                     consumed_animal->level,
                                                                     consumed_animal->exp,
                                                                     consumed_animal->added)
                                                         : C2Json{};
        for (const auto* key : {"claims",
                                "inserts",
                                "wear",
                                "riding",
                                "cache",
                                "trash",
                                "shop",
                                "storage",
                                "animal",
                                "infos",
                                "chats"}) {
            out[key] = c2_array();
        }
        for (const auto& value : q.claims) {
            out["claims"].array().push_back(c2_fields(value.id, value.claimed));
        }
        for (const auto& value : q.inserts) {
            auto item = c2_fields(value.iid,
                                  value.index,
                                  value.prefix,
                                  value.count,
                                  value.info,
                                  value.expire_at,
                                  value.has_expire_at);
            item.array().push_back(c2_numbers(c2_charm(value.charm)));
            out["inserts"].array().push_back(std::move(item));
        }
        for (const auto& value : q.wear) {
            out["wear"].array().push_back(c2_fields(
                value.player_id, value.iid, value.item_index, value.expire_at, value.equipped));
        }
        for (const auto& value : q.riding) {
            auto item = c2_fields(value.player_id,
                                  value.iid,
                                  value.item_index,
                                  value.expire_at,
                                  value.mounted,
                                  static_cast<unsigned>(value.kind),
                                  static_cast<unsigned>(value.grade),
                                  value.rarity,
                                  value.satiety,
                                  value.max_satiety,
                                  value.revision);
            item.array().push_back(c2_numbers(value.equipment_iids));
            item.array().push_back(c2_numbers(value.equipment_item_indices));
            item.array().push_back(c2_numbers(value.equipment_enchants));
            item.array().push_back(c2_numbers(value.abilities));
            out["riding"].array().push_back(std::move(item));
        }
        for (const auto& value : q.cache) {
            out["cache"].array().push_back(c2_fields(value.cache_iid,
                                                     value.seal_item_index,
                                                     value.seal_count,
                                                     value.reward_item_index,
                                                     value.reward_count));
        }
        out["trash"] = c2_numbers(q.trash);
        for (const auto& value : q.shop) {
            auto item = c2_fields(value.npc_id, value.currency_index);
            auto offers = c2_array();
            for (const auto& offer : value.offers) {
                offers.array().push_back(c2_fields(offer.index, offer.buy));
            }
            item.array().push_back(std::move(offers));
            out["shop"].array().push_back(std::move(item));
        }
        for (const auto& value : q.storage) {
            auto item = c2_fields(value.npc_id);
            auto items = c2_array();
            for (const auto& stored : value.items) {
                auto fields = c2_fields(stored.iid,
                                        stored.index,
                                        stored.prefix,
                                        stored.slot,
                                        stored.count,
                                        stored.info,
                                        stored.transform_level,
                                        stored.transform_exp);
                fields.array().push_back(c2_numbers(c2_charm(stored.charm)));
                items.array().push_back(std::move(fields));
            }
            item.array().push_back(std::move(items));
            out["storage"].array().push_back(std::move(item));
        }
        for (const auto& value : q.animal) {
            out["animal"].array().push_back(
                c2_fields(value.iid, value.index, value.level, value.exp, value.added));
        }
        for (const auto& value : q.infos) {
            auto item = c2_array();
            item.array().push_back(c2_text(value.text));
            item.array().push_back(value.kind ? c2_fields(static_cast<unsigned>(*value.kind))
                                              : C2Json{});
            if (value.color) {
                auto color = c2_array();
                for (const auto channel : *value.color) {
                    C2Json number;
                    number.set_type(C2Json::Type::Number);
                    number.set_number(channel);
                    color.array().push_back(std::move(number));
                }
                item.array().push_back(std::move(color));
            } else {
                item.array().push_back(C2Json{});
            }
            out["infos"].array().push_back(std::move(item));
        }
        for (const auto& value : q.chats) {
            auto item = c2_array();
            item.array().push_back(c2_text(value.speaker));
            item.array().push_back(c2_text(value.text));
            out["chats"].array().push_back(std::move(item));
        }
        // Flush the complete record before the next case can emit a retained warning.
        std::cout << "c2 " << C2Json::stringify(record, 0) << std::endl;
        return q;
    }
    Bytes c2_payload(const Bytes& frame) {
        expect(frame.size() >= 3, "C2 authored frame has a complete header");
        return frame.size() >= 3 ? Bytes(frame.begin() + 3, frame.end()) : Bytes{};
    }
    Bytes c2_frame(protocol::S2C type, const Bytes& payload) {
        protocol::PacketWriter writer;
        writer.payload() = payload;
        return writer.frame(static_cast<std::uint8_t>(type));
    }
    C2Queues c2_marked(client::NetClient& net,
                       const std::shared_ptr<SessionState>& session,
                       const Bytes& frame,
                       const std::string& label,
                       bool snapshot) {
        expect(frame.size() >= 3, "C2 target is a real bounded frame even for an empty payload");
        queue(session, protocol::Chatting::frame_s2c("C2", "before " + label));
        queue(session, frame);
        queue(session, protocol::Chatting::frame_s2c("C2", "after " + label));
        net.update(0, 32);
        auto q = c2_observe(net, *session, label, snapshot);
        expect(net.connected() && session->alive && session->closes == 0 && q.chats.size() == 2 &&
                   q.chats[0].speaker == "C2" && q.chats[0].text == "before " + label &&
                   q.chats[1].speaker == "C2" && q.chats[1].text == "after " + label,
               "C2 reply preserves connection and same-batch marker order");
        return q;
    }
    Bytes c2_insert_payload(std::uint32_t iid = 8123,
                            std::uint16_t index = 23007,
                            std::uint32_t count = 17) {
        protocol::PacketWriter writer;
        writer.write_u32(iid);
        writer.write_u16(index);
        writer.write_u8(6);
        writer.write_u32(0x00006139u);
        writer.write_u32(count);
        const std::array<std::uint8_t, 14> charm{
            {11, 12, 13, 14, 15, 16, 17, 18, 1, 1, 21, 22, 3, 5}};
        writer.write_bytes(charm.data(), charm.size());
        writer.write_u64(9007199254740993ULL);
        writer.write_u16(0xbbaa);
        return writer.payload();
    }
    void test_c2_insert(bool snapshot) {
        const auto whole = c2_insert_payload();
        const std::array<std::array<std::uint8_t, 14>, 8> expected{
            {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
             {{11, 12, 13, 14, 15, 16, 17, 18, 0, 0, 0, 0, 0, 0}},
             {{11, 12, 13, 14, 15, 16, 17, 18, 1, 0, 0, 0, 0, 0}},
             {{11, 12, 13, 14, 15, 16, 17, 18, 1, 1, 0, 0, 0, 0}},
             {{11, 12, 13, 14, 15, 16, 17, 18, 1, 1, 21, 0, 0, 0}},
             {{11, 12, 13, 14, 15, 16, 17, 18, 1, 1, 21, 22, 0, 0}},
             {{11, 12, 13, 14, 15, 16, 17, 18, 1, 1, 21, 22, 3, 0}},
             {{11, 12, 13, 14, 15, 16, 17, 18, 1, 1, 21, 22, 3, 5}}}};
        for (std::size_t size = 0; size <= whole.size(); ++size) {
            client::NetClient net;
            const auto session = attach(net);
            const auto q = c2_marked(
                net,
                session,
                c2_frame(protocol::S2C::InsertItem, Bytes(whole.begin(), whole.begin() + size)),
                "insert-prefix-" + std::to_string(size),
                snapshot);
            expect(c2_event_count(q) == (size < 15 ? 0u : 1u) &&
                       q.inserts.size() == (size < 15 ? 0u : 1u) && !q.dirty && q.infos.empty(),
                   "C2 insert base is atomic without unrelated service events");
            if (size >= 15 && q.inserts.size() == 1) {
                const auto& value = q.inserts.front();
                const auto optional = size - 15;
                const auto stage = optional < 8 ? 0u : std::min<std::size_t>(7, optional - 7);
                expect(value.iid == 8123 && value.index == 23007 && value.prefix == 6 &&
                           value.count == 17 && value.info == 0x00006139u &&
                           c2_charm(value.charm) == expected[stage] &&
                           value.has_expire_at == (optional >= 22) &&
                           value.expire_at == (optional >= 22 ? 9007199254740993ULL : 0),
                       "C2 named charm prefix and expiry boundaries preserve every instance field");
            }
        }
        client::NetClient net;
        const auto session = attach(net);
        auto zero = c2_insert_payload(0, 0, 0);
        zero.resize(15);
        const auto q = c2_marked(net,
                                 session,
                                 c2_frame(protocol::S2C::InsertItem, zero),
                                 "insert-zero-identity-count",
                                 snapshot);
        expect(q.inserts.size() == 1 && c2_event_count(q) == 1 && q.inserts[0].iid == 0 &&
                   q.inserts[0].index == 0 && q.inserts[0].count == 0 &&
                   !q.inserts[0].has_expire_at,
               "C2 insert queue accepts zero identity and count before later bag application");
        auto permanent = whole;
        permanent.resize(37);
        std::fill(permanent.begin() + 29, permanent.end(), 0);
        const auto timed = c2_marked(net,
                                     session,
                                     c2_frame(protocol::S2C::InsertItem, permanent),
                                     "insert-explicit-zero-expiry",
                                     snapshot);
        expect(timed.inserts.size() == 1 && timed.inserts[0].has_expire_at &&
                   timed.inserts[0].expire_at == 0,
               "C2 explicit zero expiry remains distinguishable from an absent expiry extension");
    }

    std::vector<protocol::MailEntry> c2_inbox() {
        return {{601, 23007, 3, 9007199254740993ULL, false, true, "Owned \"mail\"", "First\nbody"},
                {602, 0, 0, 9007199254740995ULL, true, false, "Grüße", "Second body"}};
    }
    void test_c2_mail(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        const auto inbox = c2_inbox();
        const auto full = protocol::Mail::frame_list_s2c(inbox);
        queue(session, full);
        net.update(0, 32);
        auto q = c2_observe(net, *session, "mail-seed", snapshot);
        expect(q.dirty && c2_same_mail(net.mail(), inbox) && c2_event_count(q) == 0,
               "C2 full inbox fields replace state and publish one dirty indication");
        auto invalid_clean = c2_payload(full);
        invalid_clean.pop_back();
        q = c2_marked(net,
                      session,
                      c2_frame(protocol::S2C::Mail, invalid_clean),
                      "mail-invalid-clean",
                      snapshot);
        expect(!q.dirty && c2_same_mail(net.mail(), inbox) && c2_event_count(q) == 0,
               "C2 malformed inbox does not publish a new dirty indication after consumption");
        queue(session, protocol::Mail::frame_claim_result_s2c({601, true}));
        queue(session, protocol::Mail::frame_claim_result_s2c({602, false}));
        net.update(0, 32);
        q = c2_observe(net, *session, "mail-claims", snapshot);
        expect(!q.dirty && c2_same_mail(net.mail(), inbox) && c2_event_count(q) == 2 &&
                   q.claims.size() == 2 && q.claims[0].id == 601 && q.claims[0].claimed &&
                   q.claims[1].id == 602 && !q.claims[1].claimed,
               "C2 claim FIFO does not replace inbox state or fabricate dirty notification");
        const auto list = c2_payload(full);
        const auto claim = c2_payload(protocol::Mail::frame_claim_result_s2c({601, true}));
        std::vector<Bytes> invalid;
        invalid.push_back({});
        for (const auto command : {2, 3, 255}) {
            auto value = claim;
            value[0] = static_cast<std::uint8_t>(command);
            invalid.push_back(std::move(value));
        }
        auto value = claim;
        std::fill(value.begin() + 1, value.begin() + 5, 0);
        invalid.push_back(value);
        value = claim;
        value[5] = 2;
        invalid.push_back(value);
        value = claim;
        value.pop_back();
        invalid.push_back(value);
        value = claim;
        value.push_back(0);
        invalid.push_back(value);
        value = list;
        value.pop_back();
        invalid.push_back(value);
        value = list;
        value.push_back(0);
        invalid.push_back(value);
        value = list;
        value[21] = 4;
        invalid.push_back(value);
        value = list;
        std::fill(value.begin() + 3, value.begin() + 7, 0);
        invalid.push_back(value);
        value = list;
        value[7] = value[8] = 0;
        invalid.push_back(value);
        value = list;
        value[1] = value[2] = 255;
        invalid.push_back(value);
        invalid.push_back({static_cast<std::uint8_t>(protocol::MailCommand::List)});
        for (std::size_t i = 0; i < invalid.size(); ++i) {
            queue(session, full);
            q = c2_marked(net,
                          session,
                          c2_frame(protocol::S2C::Mail, invalid[i]),
                          "mail-invalid-" + std::to_string(i),
                          snapshot);
            expect(q.dirty && c2_same_mail(net.mail(), inbox) && c2_event_count(q) == 0 &&
                       q.infos.empty(),
                   "C2 malformed mail retains the complete prior inbox and unconsumed dirty latch");
        }
        queue(session, full);
        net.update(0, 32);
        q = c2_observe(net, *session, "mail-identical", snapshot);
        expect(q.dirty && c2_same_mail(net.mail(), inbox),
               "C2 identical authoritative inbox still publishes dirty");
        queue(session, protocol::Mail::frame_list_s2c({}));
        net.update(0, 32);
        q = c2_observe(net, *session, "mail-empty", snapshot);
        expect(q.dirty && net.mail().empty(),
               "C2 valid empty inbox replaces contents and publishes dirty");
        queue(session, protocol::Mail::frame_claim_result_s2c({601, true}));
        queue(session, full);
        queue(session, protocol::Mail::frame_claim_result_s2c({602, false}));
        net.update(0, 32);
        q = c2_observe(net, *session, "mail-interleaved", snapshot);
        expect(q.dirty && c2_same_mail(net.mail(), inbox) && q.claims.size() == 2 &&
                   q.claims[0].id == 601 && q.claims[1].id == 602,
               "C2 inbox replacement leaves accepted claim responses in FIFO order");
    }

    void test_c2_animal(bool snapshot) {
        const auto complete = c2_payload(protocol::ItemInfo::frame_update({701, 28001, 4, 127, 1}));
        for (std::size_t size = 0; size < 13; ++size) {
            client::NetClient net;
            const auto session = attach(net);
            net.set_transform_egg_progress(17, 19);
            const auto q = c2_marked(net,
                                     session,
                                     c2_frame(protocol::S2C::UpdateItemInfo,
                                              Bytes(complete.begin(), complete.begin() + size)),
                                     "animal-short-" + std::to_string(size),
                                     snapshot);
            expect(net.transform_egg_index() == 0 && net.transform_egg_level() == 17 &&
                       net.transform_egg_exp() == 19 && c2_event_count(q) == 0 && q.infos.empty(),
                   "C2 short animal update leaves immediate state and pending queues untouched");
        }
        client::NetClient net;
        const auto session = attach(net);
        queue(session, protocol::ItemInfo::frame_update({701, 28001, 0, 21, 0}));
        queue(session, protocol::ItemInfo::frame_update({702, 28002, 255, 31, 1}));
        queue(session, protocol::frame_trading_post_pickup({0, 0, 7}));
        auto last = c2_payload(protocol::ItemInfo::frame_update({0, 0, 0, 41, 65535}));
        last.push_back(0xab);
        queue(session, c2_frame(protocol::S2C::UpdateItemInfo, last));
        net.update(0, 32);
        expect(net.transform_egg_index() == 0 && net.transform_egg_level() == 1 &&
                   net.transform_egg_exp() == 41,
               "C2 latest valid arrival updates immediate egg fields before any FIFO consumption");
        protocol::UpdateItemInfoFields first;
        expect(
            net.take_update_item_info(first) && first.iid == 701 && first.index == 28001 &&
                first.level == 0 && first.exp == 21 && first.added == 0 &&
                net.transform_egg_exp() == 41,
            "C2 raw level zero remains queued while immediate progress stays at the latest clamped state");
        queue(session, protocol::ItemInfo::frame_update({703, 28003, 3, 51, 0}));
        net.update(0, 32);
        const auto q = c2_observe(net, *session, "animal-refill", snapshot, &first);
        expect(
            q.animal.size() == 3 && q.animal[0].iid == 702 && q.animal[0].level == 255 &&
                q.animal[0].added == 1 && q.animal[1].iid == 0 && q.animal[1].index == 0 &&
                q.animal[1].level == 0 && q.animal[1].exp == 41 && q.animal[1].added == 65535 &&
                q.animal[2].iid == 703 && q.animal[2].exp == 51 &&
                net.transform_egg_index() == 28003 && net.transform_egg_level() == 3 &&
                net.transform_egg_exp() == 51,
            "C2 partial drain and refill preserve raw animal FIFO fields without rolling back immediate progress");
        expect(
            q.infos.size() == 3 && q.infos[0].text == "Animal EXP is increased by 1." &&
                q.infos[0].kind == protocol::BattleMsgKind::TransformExp && !q.infos[0].color &&
                q.infos[1].text == "You got 7 geons" &&
                q.infos[1].kind == protocol::BattleMsgKind::MoneyGet && q.infos[1].color &&
                *q.infos[1].color == std::array<float, 3>{1.f, 1.f, 1.f} &&
                q.infos[2].text == "Animal EXP is increased by 65535." &&
                q.infos[2].kind == protocol::BattleMsgKind::TransformExp && !q.infos[2].color,
            "C2 unsigned added EXP and interleaved money feedback share exact common-info FIFO order");
    }

    void test_c2_wear_trash(bool snapshot) {
        protocol::PacketWriter writer;
        writer.write_u32(0);
        writer.write_u32(0);
        writer.write_u16(0);
        writer.write_u64(9007199254740993ULL);
        writer.write_u8(0xab);
        const auto whole = writer.payload();
        for (const auto type : {protocol::S2C::PutOnItem, protocol::S2C::PutOffItem}) {
            for (const std::size_t size : {0u, 9u, 10u, 17u, 18u, 19u}) {
                client::NetClient net;
                const auto session = attach(net);
                const auto label =
                    std::string(type == protocol::S2C::PutOnItem ? "wear-on-" : "wear-off-") +
                    std::to_string(size);
                const auto q = c2_marked(net,
                                         session,
                                         c2_frame(type, Bytes(whole.begin(), whole.begin() + size)),
                                         label,
                                         snapshot);
                expect(c2_event_count(q) == (size < 10 ? 0u : 1u) &&
                           q.wear.size() == (size < 10 ? 0u : 1u),
                       "C2 wear minimum length is atomic");
                if (q.wear.size() == 1) {
                    const auto& value = q.wear[0];
                    expect(
                        value.player_id == 0 && value.iid == 0 && value.item_index == 0 &&
                            value.equipped == (type == protocol::S2C::PutOnItem) &&
                            value.expire_at == (size >= 18 ? 9007199254740993ULL : 0),
                        "C2 wear opcode and optional expiry tolerate suffixes and zero identity");
                }
            }
        }
        const auto trash = c2_payload(protocol::TrashItem::frame_s2c(811, 255));
        for (unsigned i = 0; i < 5; ++i) {
            auto payload = trash;
            if (i == 0) {
                payload.resize(4);
            }
            if (i == 1) {
                std::fill(payload.begin(), payload.begin() + 4, 0);
            }
            if (i == 2) {
                payload[4] = 0;
            }
            if (i == 4) {
                payload.push_back(0xab);
            }
            client::NetClient net;
            const auto session = attach(net);
            const auto q = c2_marked(net,
                                     session,
                                     c2_frame(protocol::S2C::TrashItem, payload),
                                     "trash-" + std::to_string(i),
                                     snapshot);
            expect(c2_event_count(q) == (i < 2 ? 0u : 1u) && q.trash.size() == (i < 2 ? 0u : 1u) &&
                       (q.trash.empty() || q.trash[0] == 811),
                   "C2 trash rejects short or zero IID but ignores log byte values and suffixes");
        }
    }

    protocol::StorageInfo c2_storage() {
        protocol::StorageInfo value;
        value.npc_id = 901;
        protocol::StorageItem item;
        item.iid = 902;
        item.index = 23007;
        item.prefix = 6;
        item.slot = 215;
        item.count = 17;
        item.info = 0x00006139u;
        item.charm = {11, 12, 13, 14, 15, 16, 17, 18, 1, 1, 21, 22, 3, 5};
        item.transform_level = 255;
        item.transform_exp = 123456;
        value.items.push_back(item);
        return value;
    }
    void test_c2_services(bool snapshot) {
        protocol::RidingState riding;
        riding.player_id = 0;
        riding.iid = 0;
        riding.item_index = 0;
        riding.expire_at = 9007199254740993ULL;
        riding.mounted = true;
        riding.kind = protocol::RidingKind::Battle;
        riding.grade = protocol::BattleRidingGrade::Epic;
        riding.rarity = 5;
        riding.satiety = 19;
        riding.max_satiety = 21;
        riding.revision = 0xfedcba98u;
        riding.equipment_iids = {{101, 102, 103, 104, 105}};
        riding.equipment_item_indices = {{201, 202, 203, 204, 205}};
        riding.equipment_enchants = {{1, 2, 3, 4, 5}};
        riding.abilities = {{31, 32, 33, 34, 35, 36}};
        const protocol::RidingCacheResult cache{903, 26001, 3, 23007, 7};
        const protocol::ShopItemList shop{904, {{23007, 19}, {23008, 23}}, 0};
        const auto stored = c2_storage();
        struct ReplyCase {
            protocol::S2C type;
            Bytes payload;
            bool accepted;
            std::string name;
        };
        std::vector<ReplyCase> cases;
        const auto add = [&](protocol::S2C type, Bytes payload, bool accepted, std::string name) {
            cases.push_back({type, std::move(payload), accepted, std::move(name)});
        };
        const auto ride = c2_payload(protocol::Riding::frame_server_state(riding));
        add(protocol::S2C::Riding, ride, true, "riding-zero-identity");
        auto payload = ride;
        payload.pop_back();
        add(protocol::S2C::Riding, payload, false, "riding-short");
        payload = ride;
        payload.push_back(0);
        add(protocol::S2C::Riding, payload, false, "riding-tail");
        for (const auto& entry :
             {std::make_pair(18u, 2u), std::make_pair(19u, 2u), std::make_pair(20u, 4u)}) {
            payload = ride;
            payload[entry.first] = static_cast<std::uint8_t>(entry.second);
            add(protocol::S2C::Riding,
                payload,
                false,
                "riding-enum-" + std::to_string(entry.first));
        }
        const auto cache_payload = c2_payload(protocol::Riding::frame_server_cache_result(cache));
        add(protocol::S2C::RidingCache, cache_payload, true, "cache-valid");
        payload = cache_payload;
        payload.pop_back();
        add(protocol::S2C::RidingCache, payload, false, "cache-short");
        payload = cache_payload;
        payload.push_back(0);
        add(protocol::S2C::RidingCache, payload, false, "cache-tail");
        for (const auto offset : {0u, 4u, 6u, 8u, 10u}) {
            payload = cache_payload;
            std::fill(
                payload.begin() + offset, payload.begin() + offset + (offset == 0 ? 4 : 2), 0);
            add(protocol::S2C::RidingCache, payload, false, "cache-zero-" + std::to_string(offset));
        }
        const auto shop_payload = c2_payload(protocol::Shop::frame_item_list_s2c(shop));
        add(protocol::S2C::ItemList, shop_payload, true, "shop-ordinary");
        payload = shop_payload;
        payload.pop_back();
        add(protocol::S2C::ItemList, payload, false, "shop-short");
        payload = shop_payload;
        payload.push_back(0);
        add(protocol::S2C::ItemList, payload, false, "shop-one-tail");
        payload.push_back(0);
        add(protocol::S2C::ItemList, payload, false, "shop-zero-currency");
        payload[payload.size() - 2] = 0xe6;
        payload[payload.size() - 1] = 0x59;
        add(protocol::S2C::ItemList, payload, true, "shop-currency");
        payload.push_back(0);
        add(protocol::S2C::ItemList, payload, false, "shop-three-tail");
        const auto storage_payload = c2_payload(protocol::Storage::frame_info_s2c(stored));
        add(protocol::S2C::StorageInfo, storage_payload, true, "storage-valid");
        payload = storage_payload;
        payload.pop_back();
        add(protocol::S2C::StorageInfo, payload, false, "storage-short");
        payload = storage_payload;
        payload.push_back(0);
        add(protocol::S2C::StorageInfo, payload, false, "storage-tail");
        for (const auto& entry :
             {std::make_pair(6u, 4u), std::make_pair(10u, 2u), std::make_pair(15u, 4u)}) {
            payload = storage_payload;
            std::fill(
                payload.begin() + entry.first, payload.begin() + entry.first + entry.second, 0);
            add(protocol::S2C::StorageInfo,
                payload,
                false,
                "storage-zero-" + std::to_string(entry.first));
        }
        payload = storage_payload;
        payload[13] = 216;
        payload[14] = 0;
        add(protocol::S2C::StorageInfo, payload, false, "storage-slot-capacity");
        protocol::PacketWriter oversized;
        oversized.write_u32(901);
        oversized.write_u16(217);
        // A complete 217-row payload exceeds the outer 8000-byte frame limit.
        // Exercise the invalid declared count in a real short frame instead.
        add(protocol::S2C::StorageInfo,
            oversized.payload(),
            false,
            "storage-declared-count-capacity");
        add(protocol::S2C::ItemList, {0}, false, "shop-header-short");
        add(protocol::S2C::StorageInfo, {0}, false, "storage-header-short");
        for (const auto& value : cases) {
            client::NetClient net;
            const auto session = attach(net);
            const auto q =
                c2_marked(net, session, c2_frame(value.type, value.payload), value.name, snapshot);
            const auto target_count = value.type == protocol::S2C::Riding        ? q.riding.size()
                                      : value.type == protocol::S2C::RidingCache ? q.cache.size()
                                      : value.type == protocol::S2C::ItemList    ? q.shop.size()
                                                                                 : q.storage.size();
            expect(
                c2_event_count(q) == (value.accepted ? 1u : 0u) &&
                    target_count == (value.accepted ? 1u : 0u) && q.infos.empty() && !q.dirty,
                "C2 strict service decoder rejects malformed replies without partial queue state");
            if (!q.riding.empty()) {
                expect(
                    q.riding[0].player_id == 0 && q.riding[0].iid == 0 &&
                        q.riding[0].item_index == 0 &&
                        protocol::Riding::frame_server_state(q.riding[0]) ==
                            protocol::Riding::frame_server_state(riding),
                    "C2 riding accepts zero identities and retains every authoritative state field");
            }
            if (!q.cache.empty()) {
                const auto& result = q.cache[0];
                expect(result.cache_iid == 903 && result.seal_item_index == 26001 &&
                           result.seal_count == 3 && result.reward_item_index == 23007 &&
                           result.reward_count == 7,
                       "C2 valid cache retains both reward identities and counts");
            }
            if (!q.shop.empty()) {
                const auto& result = q.shop[0];
                expect(
                    result.npc_id == 904 && result.offers.size() == 2 &&
                        result.offers[0].index == 23007 && result.offers[0].buy == 19 &&
                        result.offers[1].index == 23008 && result.offers[1].buy == 23 &&
                        result.currency_index == (value.name == "shop-currency" ? 23014 : 0),
                    "C2 shop offers preserve authored order and distinguish ordinary versus currency suffix");
            }
            if (!q.storage.empty()) {
                const auto& result = q.storage[0];
                expect(result.npc_id == 901 && result.items.size() == 1 &&
                           result.items[0].iid == 902 && result.items[0].slot == 215 &&
                           result.items[0].count == 17 &&
                           protocol::Storage::frame_info_s2c(result) ==
                               protocol::Storage::frame_info_s2c(stored),
                       "C2 valid storage retains slot, binding, transform and all charm metadata");
            }
        }
    }

    void c2_queue_all(const std::shared_ptr<SessionState>& session, unsigned id) {
        queue(session, protocol::Mail::frame_list_s2c(c2_inbox()));
        queue(session, protocol::Mail::frame_claim_result_s2c({601, true}));
        queue(session, c2_frame(protocol::S2C::InsertItem, c2_insert_payload(id)));
        queue(session, protocol::ItemWear::frame_server_put_on(1, id, 23007, 42));
        queue(session, protocol::Riding::frame_server_state({}));
        queue(session, protocol::Riding::frame_server_cache_result({id, 26001, 1, 23007, 2}));
        queue(session, protocol::TrashItem::frame_s2c(id, 0));
        queue(session, protocol::Shop::frame_item_list_s2c({901, {{23007, 19}}, 0}));
        queue(session, protocol::Storage::frame_info_s2c(c2_storage()));
        queue(session, protocol::ItemInfo::frame_update({id, 28001, 4, 73, 0}));
    }
    void test_c2_cleanup_isolation(bool snapshot) {
        for (const bool eof : {false, true}) {
            client::NetClient net;
            const auto session = attach(net);
            c2_queue_all(session, 991);
            net.update(0, 32);
            if (eof) {
                session->eof = true;
                net.update(0, 32);
            } else {
                net.close();
            }
            const auto q =
                c2_observe(net, *session, eof ? "cleanup-eof" : "cleanup-close", snapshot);
            expect(
                !net.connected() && q.shop.size() == 1 && q.storage.size() == 1 &&
                    q.trash == std::vector<std::uint32_t>{991} &&
                    net.transform_egg_index() == 28001 && net.transform_egg_level() == 4 &&
                    net.transform_egg_exp() == 73,
                "C2 both existing cleanup paths retain shop, storage, trash and immediate egg fields");
            expect(
                eof ? q.dirty && net.mail().size() == 2 && q.claims.size() == 1 &&
                          q.inserts.size() == 1 && q.wear.size() == 1 && q.riding.size() == 1 &&
                          q.cache.size() == 1 && q.animal.size() == 1
                    : !q.dirty && net.mail().empty() && q.claims.empty() && q.inserts.empty() &&
                          q.wear.empty() && q.riding.empty() && q.cache.empty() && q.animal.empty(),
                "C2 explicit close clears only its existing service domains while EOF preserves them");
        }
        client::NetClient first;
        client::NetClient second;
        const auto a = attach(first);
        const auto b = attach(second);
        c2_queue_all(a, 992);
        c2_queue_all(b, 993);
        queue(a, c2_frame(protocol::S2C::InsertItem, {1}));
        second.update(0, 32);
        first.update(0, 32);
        const auto qa = c2_observe(first, *a, "isolation-first", snapshot);
        const auto qb = c2_observe(second, *b, "isolation-second", snapshot);
        expect(
            qa.inserts.size() == 1 && qa.inserts[0].iid == 992 && qa.animal.size() == 1 &&
                qa.animal[0].iid == 992 && qa.trash == std::vector<std::uint32_t>{992} &&
                qb.inserts.size() == 1 && qb.inserts[0].iid == 993 && qb.animal.size() == 1 &&
                qb.animal[0].iid == 993 && qb.trash == std::vector<std::uint32_t>{993} &&
                a->closes == 0 && b->closes == 0,
            "C2 interleaved clients and a malformed insert keep service identity and FIFO state isolated");
    }

    void test_c2_inventory_packets(bool snapshot) {
        test_c2_insert(snapshot);
        test_c2_mail(snapshot);
        test_c2_animal(snapshot);
        test_c2_wear_trash(snapshot);
        test_c2_services(snapshot);
        test_c2_cleanup_isolation(snapshot);
    }
    // C2_BASELINE_DRAFT_END

    // C3_BASELINE_DRAFT_BEGIN
    // Public packet contract baselines and typed observations.
    using C3Rows = std::vector<std::array<int, 3>>;
    using C3Ranks = std::vector<std::pair<int, int>>;

    C3Rows c3_quest_rows(const client::NetClient& net) {
        C3Rows rows;
        for (const auto& entry : net.quest_flags()) {
            rows.push_back({entry.quest, entry.flag, entry.clear});
        }
        return rows;
    }
    std::array<bool, 3> c3_flags(client::NetClient& net) {
        return {net.take_quest_flags_dirty(),
                net.take_quest_state_changed(),
                net.take_quest_accepted()};
    }
    Bytes c3_frame(protocol::S2C type, const Bytes& payload) {
        protocol::PacketWriter writer;
        writer.payload() = payload;
        return writer.frame(static_cast<std::uint8_t>(type));
    }
    Bytes c3_body(const Bytes& frame) {
        expect(frame.size() >= 3, "C3 authored source frame includes its header");
        return frame.size() >= 3 ? Bytes(frame.begin() + 3, frame.end()) : Bytes{};
    }
    Bytes c3_tail(const Bytes& frame) {
        auto payload = c3_body(frame);
        payload.push_back(0xa5);
        return c3_frame(static_cast<protocol::S2C>(frame[2]), payload);
    }
    template <typename T, typename Take> std::vector<T> c3_drain(Take take) {
        std::vector<T> out;
        for (int n = 0; n < 64; ++n) {
            T value{};
            if (!take(value)) {
                return out;
            }
            out.push_back(std::move(value));
        }
        expect(false, "C3 small authored queue fully drains within its watchdog");
        return out;
    }
    struct C3Observation {
        C3Rows quests;
        std::array<bool, 3> flags{};
        std::array<bool, 3> repeated_flags{};
        std::vector<std::pair<std::int32_t, std::uint32_t>> html;
        C3Ranks upgrades;
        bool learned_pending = false;
        C3Ranks learned;
        bool shortcut_pending = false;
        bool shortcut_present = true;
        protocol::ShortcutBar shortcut;
        std::vector<client::PendingChat> chats;
        std::vector<client::PendingMapState> maps;
        std::vector<client::PendingInsertItem> inserts;
        bool failed_outputs_unchanged = false;
    };
    C1Json c3_numbers(const std::vector<int>& numbers) {
        auto out = c1_array();
        for (const auto value : numbers) {
            out.array().push_back(c1_number(value));
        }
        return out;
    }
    C3Observation c3_observe(client::NetClient& net,
                             const SessionState& session,
                             std::string_view name,
                             bool snapshot) {
        C3Observation q;
        q.quests = c3_quest_rows(net);
        q.flags = c3_flags(net);
        q.repeated_flags = c3_flags(net);
        q.html = c3_drain<std::pair<std::int32_t, std::uint32_t>>(
            [&](auto& value) { return net.take_send_html(value.first, &value.second); });
        q.upgrades = c3_drain<std::pair<int, int>>(
            [&](auto& value) { return net.take_skill_up(value.first, value.second); });
        std::unordered_map<int, int> learned{{777, 8}};
        q.learned_pending = net.take_skill_info(learned);
        q.learned.assign(learned.begin(), learned.end());
        std::sort(q.learned.begin(), q.learned.end());
        learned = {{778, 9}};
        const bool learned_false =
            !net.take_skill_info(learned) && learned == std::unordered_map<int, int>{{778, 9}};
        q.shortcut.active_index = 88;
        q.shortcut.slots.fill(88);
        q.shortcut_pending = net.take_shortcut(q.shortcut_present, q.shortcut);
        bool present = true;
        protocol::ShortcutBar bar;
        bar.active_index = 89;
        bar.slots.fill(89);
        const bool shortcut_false =
            !net.take_shortcut(present, bar) && present && bar.active_index == 89 &&
            std::all_of(bar.slots.begin(), bar.slots.end(), [](int value) { return value == 89; });
        std::int32_t html = -777;
        std::uint32_t npc = 777;
        int index = -777, rank = -778;
        q.failed_outputs_unchanged = !net.take_send_html(html, &npc) && html == -777 &&
                                     npc == 777 && !net.take_skill_up(index, rank) &&
                                     index == -777 && rank == -778 && learned_false &&
                                     shortcut_false;
        expect(q.repeated_flags == std::array<bool, 3>{false, false, false} &&
                   q.failed_outputs_unchanged,
               "C3 notifications consume once and failed takes retain caller outputs");
        q.chats = c3_drain<client::PendingChat>([&](auto& out) { return net.take_chat(out); });
        q.maps =
            c3_drain<client::PendingMapState>([&](auto& out) { return net.take_map_state(out); });
        q.inserts = c3_drain<client::PendingInsertItem>(
            [&](auto& out) { return net.take_insert_item(out); });
        if (!snapshot) {
            return q;
        }
        auto record = c1_object();
        auto& o = record.object();
        o["name"] = c1_string(std::string(name));
        o["connected"] = c1_bool(net.connected());
        o["alive"] = c1_bool(session.alive);
        o["sends"] = c1_number(session.sends);
        o["closes"] = c1_number(session.closes);
        o["sent_bytes"] = c1_array();
        for (const auto byte : session.sent) {
            o["sent_bytes"].array().push_back(c1_number(byte));
        }
        o["quests"] = c1_array();
        for (const auto& row : q.quests) {
            o["quests"].array().push_back(c3_numbers({row[0], row[1], row[2]}));
        }
        o["flags"] = c1_array();
        o["repeated_flags"] = c1_array();
        for (std::size_t i = 0; i < q.flags.size(); ++i) {
            o["flags"].array().push_back(c1_bool(q.flags[i]));
            o["repeated_flags"].array().push_back(c1_bool(q.repeated_flags[i]));
        }
        o["html"] = c1_array();
        for (const auto& row : q.html) {
            auto item = c1_object();
            item.object()["html"] = c1_number(row.first);
            item.object()["npc"] = c1_number(row.second);
            o["html"].array().push_back(std::move(item));
        }
        o["learned_pending"] = c1_bool(q.learned_pending);
        o["learned"] = c1_array();
        o["upgrades"] = c1_array();
        for (const auto& row : q.learned) {
            o["learned"].array().push_back(c3_numbers({row.first, row.second}));
        }
        for (const auto& row : q.upgrades) {
            o["upgrades"].array().push_back(c3_numbers({row.first, row.second}));
        }
        o["shortcut_pending"] = c1_bool(q.shortcut_pending);
        o["shortcut_present"] = c1_bool(q.shortcut_present);
        o["shortcut_active"] = c1_number(q.shortcut.active_index);
        o["shortcut_slots"] =
            c3_numbers(std::vector<int>(q.shortcut.slots.begin(), q.shortcut.slots.end()));
        o["failed_outputs_unchanged"] = c1_bool(q.failed_outputs_unchanged);
        o["chats"] = c1_array();
        for (const auto& chat : q.chats) {
            auto item = c1_object();
            item.object()["speaker"] = c1_string(chat.speaker);
            item.object()["text"] = c1_string(chat.text);
            o["chats"].array().push_back(std::move(item));
        }
        o["maps"] = c1_array();
        for (const auto& map : q.maps) {
            o["maps"].array().push_back(c3_numbers({map.map,
                                                    map.phase,
                                                    map.active_mask,
                                                    static_cast<int>(map.timer),
                                                    static_cast<int>(map.timer_seconds)}));
        }
        o["inserts"] = c1_array();
        for (const auto& item : q.inserts) {
            auto value = c1_object();
            value.object()["iid"] = c1_number(item.iid);
            value.object()["index"] = c1_number(item.index);
            value.object()["count"] = c1_number(item.count);
            value.object()["prefix"] = c1_number(item.prefix);
            value.object()["info"] = c1_number(item.info);
            o["inserts"].array().push_back(std::move(value));
        }
        std::cout << "c3 " << C1Json::stringify(record, 0) << std::endl;
        return q;
    }
    C3Observation c3_marked(client::NetClient& net,
                            const std::shared_ptr<SessionState>& session,
                            const Bytes& frame,
                            const std::string& label,
                            bool snapshot) {
        expect(frame.size() >= 3, "C3 malformed inner payload still has a real outer frame");
        queue(session, protocol::Chatting::frame_s2c("C3", "before " + label));
        queue(session, frame);
        queue(session, protocol::Chatting::frame_s2c("C3", "after " + label));
        net.update(0, 32);
        auto q = c3_observe(net, *session, label, snapshot);
        expect(net.connected() && session->alive && session->closes == 0 && q.chats.size() == 2 &&
                   q.chats[0].speaker == "C3" && q.chats[0].text == "before " + label &&
                   q.chats[1].speaker == "C3" && q.chats[1].text == "after " + label,
               "C3 handled packet preserves connection and exact same-batch chat neighbors");
        return q;
    }
    protocol::ShortcutBar c3_bar() {
        protocol::ShortcutBar bar;
        bar.active_index = 5;
        bar.slots[0] = 0;
        bar.slots[1] = protocol::shortcut_pack_item(23016);
        bar.slots[19] = 31;
        return bar;
    }
    void test_c3_html(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        expect(!net.send_ask_npc(100) && session->sends == 0,
               "C3 unhandshaken Ask cannot establish HTML fallback correlation");
        queue(session, protocol::Quest::frame_send_html(-17));
        net.update(0, 32);
        auto q = c3_observe(net, *session, "html-fresh", snapshot);
        expect(q.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{-17, 0}},
               "C3 legacy HTML without a successful Ask retains zero NPC correlation");
        queue(session, server::frame_s2c_code(345));
        expect(net.handshake(), "C3 public Code handshake enables ordinary AskNpc sends");
        session->sent.clear();
        expect(!net.send_ask_npc(0) && net.send_ask_npc(101),
               "C3 zero Ask is rejected while a real Ask establishes correlation");
        queue(session, protocol::Quest::frame_send_html(10));
        expect(net.send_ask_npc(202), "C3 second ordinary Ask can precede receive processing");
        net.update(0, 32);
        q = c3_observe(net, *session, "html-processing-time", snapshot);
        Bytes expected = protocol::Quest::frame_ask_npc(101);
        append(expected, protocol::Quest::frame_ask_npc(202));
        expect(q.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{10, 202}} &&
                   session->sent == expected,
               "C3 buffered legacy reply uses most recent successful Ask at processing time");
        session->fail_send = session->sends + 1;
        expect(!net.send_ask_npc(303), "C3 scripted failed Ask does not establish correlation");
        session->fail_send = 0;
        queue(session, protocol::Quest::frame_send_html(11));
        queue(session, protocol::Quest::frame_send_html(12, 404));
        protocol::PacketWriter zero;
        zero.write_i32(13);
        zero.write_u32(0);
        queue(session, zero.frame(static_cast<std::uint8_t>(protocol::S2C::SendHtml)));
        net.update(0, 32);
        q = c3_observe(net, *session, "html-source-priority", snapshot);
        expect(
            q.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{11, 202},
                                                                          {12, 404},
                                                                          {13, 202}} &&
                session->sent == expected,
            "C3 failed Ask retains fallback; explicit nonzero NPC wins and explicit zero falls back");
        queue(session, protocol::Quest::frame_send_html(14, 505));
        net.update(0, 32);
        std::int32_t html = -1;
        expect(net.take_send_html(html) && html == 14 && !net.take_send_html(html) && html == 14,
               "C3 HTML supports omitted NPC output and preserves failed-take output");
        (void) c3_observe(net, *session, "html-optional-output", snapshot);
        queue(session, protocol::GameExit::frame_server_answer(true));
        net.close();
    }
    void test_c3_quests(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        queue(session, protocol::Quest::frame_quest_info({{41, 0, 0}, {41, 7, 0}, {99, 1, 1}}));
        net.update(0, 32);
        auto q = c3_observe(net, *session, "quest-duplicate-snapshot", snapshot);
        expect(q.quests == C3Rows{{41, 0, 0}, {41, 7, 0}, {99, 1, 1}} &&
                   q.flags == std::array<bool, 3>{true, false, false},
               "C3 snapshot keeps duplicate order and only marks dirty");
        queue(session, protocol::Quest::frame_save_quest({41, 3, 0}));
        net.update(0, 32);
        q = c3_observe(net, *session, "quest-first-duplicate", snapshot);
        expect(q.quests == C3Rows{{41, 3, 0}, {41, 7, 0}, {99, 1, 1}} &&
                   q.flags == std::array<bool, 3>{true, true, true},
               "C3 SaveQuest replaces first duplicate and derives acceptance from its old value");
        queue(session, protocol::Quest::frame_save_quest({41, 3, 0}));
        net.update(0, 32);
        q = c3_observe(net, *session, "quest-identical-save", snapshot);
        expect(q.flags == std::array<bool, 3>{true, true, false},
               "C3 identical active SaveQuest remains dirty and changed without new acceptance");
        queue(session, protocol::Quest::frame_save_quest({42, 1, 0}));
        queue(session, protocol::Quest::frame_quest_info({{73, 2, 0}}));
        net.update(0, 32);
        expect(net.take_quest_accepted() && net.take_quest_flags_dirty() &&
                   net.take_quest_state_changed(),
               "C3 replacing snapshot retains earlier acceptance and independent change latches");
        q = c3_observe(net, *session, "quest-independent-takes", snapshot);
        expect(q.quests == C3Rows{{73, 2, 0}} &&
                   q.flags == std::array<bool, 3>{false, false, false},
               "C3 consuming quest flags in a different order consumes each exactly once");
        queue(session, protocol::Quest::frame_quest_info({{73, 2, 0}}));
        net.update(0, 32);
        q = c3_observe(net, *session, "quest-identical-snapshot", snapshot);
        expect(q.flags == std::array<bool, 3>{true, false, false},
               "C3 identical authoritative QuestInfo republishes only dirty");
        queue(session, protocol::Quest::frame_quest_info({}));
        net.update(0, 32);
        q = c3_observe(net, *session, "quest-empty", snapshot);
        expect(q.quests.empty() && q.flags == std::array<bool, 3>{true, false, false},
               "C3 empty QuestInfo is an authoritative dirty replacement");
    }
    void test_c3_learned(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        queue(session, protocol::Skill::frame_skill_info({{4, 1}, {4, 7}, {9, 0}}));
        queue(session, protocol::Skill::frame_skill_up(4, 8));
        net.update(0, 32);
        auto q = c3_observe(net, *session, "learned-duplicates-event", snapshot);
        expect(q.learned_pending && q.learned == C3Ranks{{4, 7}, {9, 0}} &&
                   q.upgrades == C3Ranks{{4, 8}},
               "C3 learned duplicate last rank and rank0 coexist with a separate SkillUp FIFO");
        queue(session, protocol::Skill::frame_skill_up(4, 9));
        net.update(0, 32);
        q = c3_observe(net, *session, "learned-event-only", snapshot);
        expect(!q.learned_pending && q.learned == C3Ranks{{777, 8}} &&
                   q.upgrades == C3Ranks{{4, 9}},
               "C3 SkillUp alone does not fabricate or modify a learned snapshot notification");
        queue(session, protocol::Skill::frame_skill_info({{8, 1}}));
        queue(session, protocol::Skill::frame_skill_info({{10, 5}}));
        queue(session, protocol::Skill::frame_skill_up(0, 0));
        queue(session, protocol::Skill::frame_skill_up(0, 0));
        net.update(0, 32);
        q = c3_observe(net, *session, "learned-latest-fifo", snapshot);
        expect(q.learned_pending && q.learned == C3Ranks{{10, 5}} &&
                   q.upgrades == C3Ranks{{0, 0}, {0, 0}},
               "C3 learned snapshots coalesce while identical zero-valued SkillUp events do not");
        queue(session, protocol::Skill::frame_skill_info({}));
        net.update(0, 32);
        q = c3_observe(net, *session, "learned-empty", snapshot);
        expect(q.learned_pending && q.learned.empty(),
               "C3 empty SkillInfo is an unread authoritative empty map");
    }
    void test_c3_shortcuts(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        const auto wanted = c3_bar();
        queue(session, protocol::Shortcut::frame_server(&wanted));
        net.update(0, 32);
        auto q = c3_observe(net, *session, "shortcut-full", snapshot);
        expect(q.shortcut_pending && q.shortcut_present &&
                   q.shortcut.active_index == wanted.active_index &&
                   q.shortcut.slots == wanted.slots,
               "C3 complete shortcut retains all20 item, skill and empty bindings");
        protocol::PacketWriter legacy;
        legacy.write_u8(2);
        legacy.write_u16(9);
        for (int i = 0; i < 10; ++i) {
            legacy.write_u16(i == 0 ? 1 : (i == 9 ? 10 : protocol::kShortcutEmpty));
        }
        queue(session, legacy.frame(static_cast<std::uint8_t>(protocol::S2C::Shortcut)));
        net.update(0, 32);
        q = c3_observe(net, *session, "shortcut-legacy-flag2", snapshot);
        protocol::ShortcutBar expected;
        expected.active_index = 9;
        expected.slots[0] = 1;
        expected.slots[9] = 10;
        expect(q.shortcut_pending && q.shortcut_present && q.shortcut.active_index == 9 &&
                   q.shortcut.slots == expected.slots,
               "C3 any nonzero legacy presence flag fills its second row with empty slots");
        queue(session, protocol::Shortcut::frame_server(&wanted));
        queue(session, c3_frame(protocol::S2C::Shortcut, {0, 0xaa}));
        net.update(0, 32);
        q = c3_observe(net, *session, "shortcut-latest-absent", snapshot);
        expect(q.shortcut_pending && !q.shortcut_present && q.shortcut.active_index == -1 &&
                   std::all_of(q.shortcut.slots.begin(),
                               q.shortcut.slots.end(),
                               [](int value) { return value == -1; }),
               "C3 absent flag plus tail replaces an unread present shortcut exactly once");
    }
    void test_c3_malformed(bool snapshot) {
        struct Case {
            protocol::S2C type;
            Bytes payload;
            std::string name;
        };
        std::vector<Case> cases;
        const auto full_html = c3_body(protocol::Quest::frame_send_html(17, 801));
        for (std::size_t n : {0u, 1u, 2u, 3u, 5u, 6u, 7u}) {
            cases.push_back({protocol::S2C::SendHtml,
                             Bytes(full_html.begin(), full_html.begin() + n),
                             "malformed-html-" + std::to_string(n)});
        }
        const auto full_save = c3_body(protocol::Quest::frame_save_quest({41, 8, 0}));
        for (std::size_t n = 0; n < 4; ++n) {
            cases.push_back({protocol::S2C::SaveQuest,
                             Bytes(full_save.begin(), full_save.begin() + n),
                             "malformed-save-" + std::to_string(n)});
        }
        cases.push_back({protocol::S2C::QuestInfo, {}, "malformed-quest-header"});
        cases.push_back(
            {protocol::S2C::QuestInfo, {2, 73, 0, 4, 0}, "malformed-quest-later-entry"});
        cases.push_back({protocol::S2C::SkillInfo, {}, "malformed-learned-header"});
        cases.push_back({protocol::S2C::SkillInfo, {2, 11, 6}, "malformed-learned-later-entry"});
        cases.push_back({protocol::S2C::SkillUp, {}, "malformed-upgrade-empty"});
        cases.push_back({protocol::S2C::SkillUp, {12}, "malformed-upgrade-short"});
        const auto wanted = c3_bar();
        const auto full_bar = c3_body(protocol::Shortcut::frame_server(&wanted));
        cases.push_back({protocol::S2C::Shortcut, {}, "malformed-shortcut-empty"});
        for (std::size_t n : {1u, 22u, 24u, 42u}) {
            cases.push_back({protocol::S2C::Shortcut,
                             Bytes(full_bar.begin(), full_bar.begin() + n),
                             "malformed-shortcut-" + std::to_string(n)});
        }
        auto extra = full_bar;
        extra.push_back(0);
        cases.push_back({protocol::S2C::Shortcut, extra, "malformed-shortcut-tail"});
        for (const auto& value : cases) {
            client::NetClient net;
            const auto session = attach(net);
            queue(session, protocol::Quest::frame_quest_info({{41, 0, 0}, {41, 7, 0}, {99, 1, 1}}));
            queue(session, protocol::Quest::frame_save_quest({72, 1, 0}));
            queue(session, protocol::Quest::frame_send_html(90, 901));
            queue(session, protocol::Skill::frame_skill_info({{4, 7}, {9, 0}}));
            queue(session, protocol::Skill::frame_skill_up(4, 8));
            queue(session, protocol::Shortcut::frame_server(&wanted));
            const auto q =
                c3_marked(net, session, c3_frame(value.type, value.payload), value.name, snapshot);
            expect(q.quests == C3Rows{{41, 0, 0}, {41, 7, 0}, {99, 1, 1}, {72, 1, 0}} &&
                       q.flags == std::array<bool, 3>{true, true, true} &&
                       q.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{90, 901}} &&
                       q.learned_pending && q.learned == C3Ranks{{4, 7}, {9, 0}} &&
                       q.upgrades == C3Ranks{{4, 8}} && q.shortcut_pending && q.shortcut_present &&
                       q.shortcut.active_index == wanted.active_index &&
                       q.shortcut.slots == wanted.slots && q.maps.empty() && q.inserts.empty(),
                   "C3 malformed family retains every previously complete unread C3 domain");
        }
    }
    void test_c3_tails(bool snapshot) {
        const std::vector<Bytes> frames{
            c3_tail(protocol::Quest::frame_send_html(50, 500)),
            c3_tail(protocol::Quest::frame_save_quest({51, 3, 0})),
            c3_tail(protocol::Quest::frame_quest_info({{52, 4, 1}, {52, 5, 0}})),
            c3_tail(protocol::Skill::frame_skill_info({{53, 6}, {53, 7}})),
            c3_tail(protocol::Skill::frame_skill_up(54, 8)),
            c3_frame(protocol::S2C::Shortcut, {0, 0xaa, 0xbb})};
        for (std::size_t i = 0; i < frames.size(); ++i) {
            client::NetClient net;
            const auto session = attach(net);
            const auto q =
                c3_marked(net, session, frames[i], "accepted-tail-" + std::to_string(i), snapshot);
            expect(q.html.size() == (i == 0 ? 1u : 0u) && q.upgrades.size() == (i == 4 ? 1u : 0u) &&
                       q.learned_pending == (i == 3) && q.shortcut_pending == (i == 5) &&
                       q.maps.empty() && q.inserts.empty(),
                   "C3 accepted tail produces only its authored family notification");
            if (i == 0) {
                expect(q.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{50, 500}},
                       "C3 explicit HTML with tail retains signed ID and NPC");
            } else if (i == 1) {
                expect(q.quests == C3Rows{{51, 3, 0}} &&
                           q.flags == std::array<bool, 3>{true, true, true},
                       "C3 SaveQuest tail retains active confirmation and acceptance");
            } else if (i == 2) {
                expect(q.quests == C3Rows{{52, 4, 1}, {52, 5, 0}} &&
                           q.flags == std::array<bool, 3>{true, false, false},
                       "C3 QuestInfo tail retains duplicate authored row order");
            } else if (i == 3) {
                expect(q.learned == C3Ranks{{53, 7}},
                       "C3 SkillInfo tail retains last duplicate rank");
            } else if (i == 4) {
                expect(q.upgrades == C3Ranks{{54, 8}}, "C3 SkillUp tail preserves event fields");
            } else {
                expect(!q.shortcut_present && q.shortcut.active_index == -1 &&
                           q.shortcut.slots == protocol::ShortcutBar{}.slots,
                       "C3 absent Shortcut ignores extra payload and yields default empty bar");
            }
        }
        for (const auto type : {protocol::S2C::QuestInfo, protocol::S2C::SkillInfo}) {
            client::NetClient net;
            const auto session = attach(net);
            const auto q = c3_marked(net,
                                     session,
                                     c3_frame(type, {0, 0xaa}),
                                     type == protocol::S2C::QuestInfo ? "empty-quest-tail"
                                                                      : "empty-learned-tail",
                                     snapshot);
            expect(
                q.quests.empty() && q.upgrades.empty() && q.html.empty() &&
                    (type == protocol::S2C::QuestInfo
                         ? q.flags == std::array<bool, 3>{true, false, false} && !q.learned_pending
                         : q.learned_pending && q.learned.empty() &&
                               q.flags == std::array<bool, 3>{false, false, false}),
                "C3 zero-count snapshots accept tail as an authoritative empty replacement");
        }
    }
    void test_c3_isolation_and_order(bool snapshot) {
        client::NetClient first, second;
        const auto a = attach(first);
        const auto b = attach(second);
        auto bar = c3_bar();
        queue(a, protocol::Quest::frame_quest_info({{81, 0, 0}}));
        queue(a, server::frame_insert_item(701, 23016, 2, 0, 3));
        queue(a, protocol::Quest::frame_save_quest({81, 1, 0}));
        queue(a, protocol::MapState::frame({2, 3, 4, protocol::MapStateTimer::Ritual, 19}));
        queue(a, protocol::Skill::frame_skill_info({{4, 5}}));
        queue(a, protocol::Quest::frame_send_html(101, 1001));
        queue(a, protocol::Shortcut::frame_server(&bar));
        queue(b, protocol::Quest::frame_save_quest({81, 2, 0}));
        queue(b, protocol::Quest::frame_quest_info({{82, 3, 1}}));
        queue(b, protocol::Skill::frame_skill_info({{4, 6}}));
        queue(b, protocol::Quest::frame_send_html(102, 1002));
        bar.slots[19] = 32;
        queue(b, protocol::Shortcut::frame_server(&bar));
        first.update(0, 32);
        second.update(0, 32);
        const auto qa = c3_observe(first, *a, "isolation-first", snapshot);
        const auto qb = c3_observe(second, *b, "isolation-second", snapshot);
        expect(qa.quests == C3Rows{{81, 1, 0}} && qb.quests == C3Rows{{82, 3, 1}} &&
                   qa.flags == std::array<bool, 3>{true, true, true} &&
                   qb.flags == std::array<bool, 3>{true, true, true} &&
                   qa.learned == C3Ranks{{4, 5}} && qb.learned == C3Ranks{{4, 6}} &&
                   qa.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{101, 1001}} &&
                   qb.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{102, 1002}} &&
                   qa.shortcut.slots[19] == 31 && qb.shortcut.slots[19] == 32,
               "C3 two clients keep independent state and distinct packet order around snapshots");
        expect(qa.inserts.size() == 1 && qa.inserts[0].iid == 701 && qa.inserts[0].index == 23016 &&
                   qa.inserts[0].count == 3 && qa.maps.size() == 1 && qa.maps[0].map == 2 &&
                   qa.maps[0].phase == 3 && qa.maps[0].active_mask == 4 &&
                   qa.maps[0].timer_seconds == 19 && qb.inserts.empty() && qb.maps.empty(),
               "C3 dispatcher leaves interleaved C2 inventory and world-family packets observable");
        (void) c3_observe(first, *a, "isolation-first-consumed", snapshot);
        (void) c3_observe(second, *b, "isolation-second-consumed", snapshot);
    }
    void test_c3_cleanup(bool snapshot) {
        for (const bool eof : {false, true}) {
            client::NetClient net;
            const auto session = attach(net);
            const auto bar = c3_bar();
            queue(session, protocol::Quest::frame_save_quest({91, 1, 0}));
            queue(session, protocol::Quest::frame_send_html(110, 1100));
            queue(session, protocol::Skill::frame_skill_info({{9, 2}}));
            queue(session, protocol::Skill::frame_skill_up(9, 3));
            queue(session, protocol::Shortcut::frame_server(&bar));
            net.update(0, 32);
            if (eof) {
                session->eof = true;
                net.update(0, 32);
            } else {
                net.close();
            }
            const auto q =
                c3_observe(net, *session, eof ? "cleanup-eof" : "cleanup-close", snapshot);
            expect(
                !net.connected() && q.quests == (eof ? C3Rows{{91, 1, 0}} : C3Rows{}) &&
                    q.flags == (eof ? std::array<bool, 3>{true, true, true}
                                    : std::array<bool, 3>{false, false, false}) &&
                    q.html ==
                        (eof ? std::vector<std::pair<std::int32_t, std::uint32_t>>{{110, 1100}}
                             : std::vector<std::pair<std::int32_t, std::uint32_t>>{}) &&
                    q.learned_pending && q.learned == C3Ranks{{9, 2}} &&
                    q.upgrades == C3Ranks{{9, 3}} && q.shortcut_pending && q.shortcut_present &&
                    q.shortcut.slots == bar.slots,
                "C3 explicit close clears quest and HTML while EOF and learned/event/shortcut quirks remain");
        }
    }
    void test_c3_quest_packets(bool snapshot) {
        test_c3_html(snapshot);
        test_c3_quests(snapshot);
        test_c3_learned(snapshot);
        test_c3_shortcuts(snapshot);
        test_c3_malformed(snapshot);
        test_c3_tails(snapshot);
        test_c3_isolation_and_order(snapshot);
        test_c3_cleanup(snapshot);
    }
    // C3_BASELINE_DRAFT_END

    // C4_BASELINE_DRAFT_BEGIN
    using C4ClockJson = platform::Json;
    C4ClockJson c4_clock_text(std::string value) {
        C4ClockJson out;
        out.set_type(C4ClockJson::Type::String);
        out.string() = std::move(value);
        return out;
    }
    C4ClockJson c4_clock_array() {
        C4ClockJson out;
        out.set_type(C4ClockJson::Type::Array);
        return out;
    }
    template <typename... Values> C4ClockJson c4_clock_fields(Values... values) {
        auto out = c4_clock_array();
        (out.array().push_back(c4_clock_text(std::to_string(values))), ...);
        return out;
    }
    struct C4ClockObservation {
        bool ready = false;
        std::uint64_t unix_seconds = 0;
        std::string time;
        std::string online;
        std::string countdown;
        std::vector<protocol::ClockEvent> events;
        std::optional<protocol::ExpEventStateFields> event;
        std::vector<client::PendingMapState> maps;
        std::vector<client::PendingChat> chats;
    };
    template <typename Value, typename Take> std::vector<Value> c4_clock_drain(Take take) {
        std::vector<Value> values;
        Value value{};
        for (unsigned i = 0; i < 1024; ++i) {
            if (!take(value)) {
                return values;
            }
            values.push_back(std::move(value));
        }
        expect(false, "C4 authored clock-family queues drain within their bounded watchdog");
        return values;
    }
    C4ClockObservation c4_clock_observe(client::NetClient& net,
                                        const SessionState& session,
                                        client::ServerClock::TimePoint at,
                                        const char* label,
                                        bool snapshot) {
        C4ClockObservation value;
        value.ready = net.server_clock().ready();
        value.unix_seconds = net.server_clock().unix_seconds(at);
        value.time = net.server_clock().time_text(at);
        value.online = net.server_clock().online_text(at);
        value.countdown = net.server_clock().countdown_text("owned", at);
        value.events = net.server_clock().events();
        value.event = net.exp_event_state();
        value.maps = c4_clock_drain<client::PendingMapState>(
            [&](auto& out) { return net.take_map_state(out); });
        value.chats =
            c4_clock_drain<client::PendingChat>([&](auto& out) { return net.take_chat(out); });
        if (!snapshot) {
            return value;
        }
        C4ClockJson record;
        record.set_type(C4ClockJson::Type::Object);
        auto& out = record.object();
        out["checkpoint"] = c4_clock_text(label);
        out["domain"] = c4_clock_text("clock");
        out["transport"] = c4_clock_fields(
            net.connected(), session.alive, session.eof, session.closes, session.sends);
        out["sent"] = c4_clock_array();
        for (const auto byte : session.sent) {
            out["sent"].array().push_back(c4_clock_fields(byte));
        }
        out["pending_receive_chunks"] = c4_clock_array();
        for (const auto& chunk : session.chunks) {
            auto bytes = c4_clock_array();
            for (const auto byte : chunk) {
                bytes.array().push_back(c4_clock_fields(byte));
            }
            out["pending_receive_chunks"].array().push_back(std::move(bytes));
        }
        out["clock"] = c4_clock_fields(value.ready, value.unix_seconds);
        out["time"] = c4_clock_text(value.time);
        out["online"] = c4_clock_text(value.online);
        out["countdown"] = c4_clock_text(value.countdown);
        out["clock_events"] = c4_clock_array();
        for (const auto& event : value.events) {
            auto entry = c4_clock_fields(event.starts_at, event.following_at);
            entry.array().push_back(c4_clock_text(event.id));
            entry.array().push_back(c4_clock_text(event.name));
            out["clock_events"].array().push_back(std::move(entry));
        }
        if (value.event) {
            const auto& event = *value.event;
            out["community_event"] = c4_clock_fields(event.enabled,
                                                     event.manager_npc_index,
                                                     event.donation_item_index,
                                                     event.contributed,
                                                     event.target,
                                                     event.active_bonus_percent,
                                                     event.remaining_seconds,
                                                     event.next_bonus_percent);
        } else {
            out["community_event"] = C4ClockJson{};
        }
        out["maps"] = c4_clock_array();
        for (const auto& map : value.maps) {
            out["maps"].array().push_back(c4_clock_fields(map.map,
                                                          map.phase,
                                                          map.active_mask,
                                                          static_cast<unsigned>(map.timer),
                                                          map.timer_seconds));
        }
        out["chats"] = c4_clock_array();
        for (const auto& chat : value.chats) {
            auto entry = c4_clock_array();
            entry.array().push_back(c4_clock_text(chat.speaker));
            entry.array().push_back(c4_clock_text(chat.text));
            out["chats"].array().push_back(std::move(entry));
        }
        std::cout << "c4 " << C4ClockJson::stringify(record, 0) << std::endl;
        return value;
    }
    Bytes c4_clock_payload(const Bytes& frame) {
        expect(frame.size() >= 3, "C4 authored clock-family frame has a real header");
        return frame.size() >= 3 ? Bytes(frame.begin() + 3, frame.end()) : Bytes{};
    }
    Bytes c4_clock_frame(protocol::S2C type, Bytes payload) {
        protocol::PacketWriter writer;
        writer.payload() = std::move(payload);
        return writer.frame(static_cast<std::uint8_t>(type));
    }
    bool c4_same_event(const protocol::ExpEventStateFields& a,
                       const protocol::ExpEventStateFields& b) {
        return std::tie(a.enabled,
                        a.manager_npc_index,
                        a.donation_item_index,
                        a.contributed,
                        a.target,
                        a.active_bonus_percent,
                        a.remaining_seconds,
                        a.next_bonus_percent) == std::tie(b.enabled,
                                                          b.manager_npc_index,
                                                          b.donation_item_index,
                                                          b.contributed,
                                                          b.target,
                                                          b.active_bonus_percent,
                                                          b.remaining_seconds,
                                                          b.next_bonus_percent);
    }
    void test_c4_clock_snapshots(bool snapshot) {
        const protocol::ClockSyncFields first{
            1700000000, 3600, 90061, {{"owned", "Owned event", 1700183845, 1700788645}}};
        const protocol::ExpEventStateFields event{
            true, 77, 23016, 9007199254740993ULL, 9007199254740995ULL, 35, 123, 40};
        client::NetClient net;
        const auto session = attach(net);
        const auto before = std::chrono::steady_clock::now();
        queue(session, protocol::ClockSync::frame(first));
        queue(session, protocol::ExpEventState::frame(event));
        net.update(0, 32);
        const auto after = std::chrono::steady_clock::now();
        auto q = c4_clock_observe(net, *session, before, "clock-first", snapshot);
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(after - before).count();
        const auto current = net.server_clock().unix_seconds(after);
        expect(
            q.ready && q.unix_seconds == 1700000000 && q.time == "23:13:20" &&
                q.online == "25:01:01" && q.countdown == "02:03:04:05" && q.events.size() == 1 &&
                q.events[0].id == "owned" && q.events[0].name == "Owned event" &&
                q.events[0].starts_at == 1700183845 && q.events[0].following_at == 1700788645 &&
                current >= first.unix_seconds &&
                current <= first.unix_seconds + static_cast<std::uint64_t>(elapsed),
            "C4 accepted wire clock uses its real bracketed receive time and exact explicit-before presentation");
        expect(q.event && q.event->enabled && q.event->manager_npc_index == 77 &&
                   q.event->donation_item_index == 23016 &&
                   q.event->contributed == 9007199254740993ULL &&
                   q.event->target == 9007199254740995ULL && q.event->active_bonus_percent == 35 &&
                   q.event->remaining_seconds == 123 && q.event->next_bonus_percent == 40,
               "C4 community event retains every scalar and exact large unsigned contribution");
        auto invalid_event = c4_clock_payload(protocol::ExpEventState::frame(event));
        for (unsigned i = 0; i < 3; ++i) {
            auto payload = invalid_event;
            if (i == 0) {
                payload.pop_back();
            } else if (i == 1) {
                payload.push_back(0);
            } else {
                payload[0] = 2;
            }
            queue(session, protocol::Chatting::frame_s2c("C4", "before event"));
            queue(session, c4_clock_frame(protocol::S2C::ExpEventState, std::move(payload)));
            queue(session, protocol::Chatting::frame_s2c("C4", "after event"));
            net.update(0, 32);
            const auto label = "event-invalid-" + std::to_string(i);
            q = c4_clock_observe(net, *session, before, label.c_str(), snapshot);
            expect(
                net.connected() && session->closes == 0 && q.ready && q.event &&
                    c4_same_event(*q.event, event) && q.unix_seconds == first.unix_seconds &&
                    q.events.size() == 1 && q.events[0].id == "owned" &&
                    q.events[0].name == "Owned event" && q.events[0].starts_at == 1700183845 &&
                    q.events[0].following_at == 1700788645 && q.chats.size() == 2 &&
                    q.chats[0].text == "before event" && q.chats[1].text == "after event",
                "C4 malformed event preserves earlier clock/event state and same-batch neighbors");
        }
        const protocol::ClockSyncFields second{1700003600, -3600, 17, {}};
        const auto replace_before = std::chrono::steady_clock::now();
        queue(session, protocol::ClockSync::frame(first));
        queue(session, protocol::ClockSync::frame(second));
        queue(session, protocol::ExpEventState::frame({false, -2, -3, 11, 0, 0, 0, 0}));
        net.update(0, 32);
        q = c4_clock_observe(
            net, *session, replace_before, "clock-latest-disabled-event", snapshot);
        expect(
            q.ready && q.unix_seconds == second.unix_seconds && q.time == "22:13:20" &&
                q.online == "00:00:17" && q.events.empty() && q.countdown == "00:00:00:00" &&
                q.event && !q.event->enabled && q.event->manager_npc_index == -2 &&
                q.event->donation_item_index == -3 && q.event->contributed == 11 &&
                q.event->target == 0,
            "C4 latest same-batch clock replaces its events and disabled event keeps accepted raw fields");
        auto bad_clock = c4_clock_payload(protocol::ClockSync::frame(second));
        bad_clock.push_back(0);
        queue(session,
              protocol::MapState::frame(
                  {250, 249, 248, protocol::MapStateTimer::Ritual, 0xffffffffu}));
        queue(session, protocol::Chatting::frame_s2c("C4", "before bad clock"));
        queue(session, c4_clock_frame(protocol::S2C::ClockSync, std::move(bad_clock)));
        queue(session, protocol::ClockSync::frame(first));
        queue(session, protocol::ExpEventState::frame(event));
        queue(session,
              protocol::MapState::frame({247, 246, 245, protocol::MapStateTimer::Lord, 7}));
        queue(session, protocol::Chatting::frame_s2c("C4", "after bad clock"));
        net.update(0, 32);
        q = c4_clock_observe(
            net, *session, replace_before, "clock-invalid-suffix-cleanup", snapshot);
        expect(
            !net.connected() && session->closes == 1 && !q.ready && q.unix_seconds == 0 &&
                q.time == "--:--:--" && !q.event && q.events.empty() && q.maps.size() == 2 &&
                q.maps[0].map == 250 && q.maps[0].phase == 249 && q.maps[0].active_mask == 248 &&
                q.maps[0].timer == protocol::MapStateTimer::Ritual &&
                q.maps[0].timer_seconds == 0xffffffffu && q.maps[1].map == 247 &&
                q.maps[1].timer == protocol::MapStateTimer::Lord && q.maps[1].timer_seconds == 7 &&
                q.chats.size() == 2 && q.chats[1].text == "after bad clock",
            "C4 invalid clock closes once while suffix maps/chats drain before existing online cleanup");
    }

    void test_c4_event_invalid_ranges(bool snapshot) {
        const protocol::ClockSyncFields clock{
            1700000000, 0, 19, {{"owned", "Owned event", 1700000010, 1700000020}}};
        const protocol::ExpEventStateFields event{
            true, 77, 23016, 9007199254740993ULL, 9007199254740995ULL, 35, 123, 40};
        for (unsigned kind = 0; kind < 2; ++kind) {
            client::NetClient net;
            const auto session = attach(net);
            const auto before = std::chrono::steady_clock::now();
            queue(session, protocol::ClockSync::frame(clock));
            queue(session, protocol::ExpEventState::frame(event));
            auto malformed = event;
            if (kind == 0) {
                malformed.active_bonus_percent = -1;
            } else {
                malformed.remaining_seconds = 0;
            }
            queue(session, protocol::Chatting::frame_s2c("C4", "before invalid event range"));
            queue(session, protocol::ExpEventState::frame(malformed));
            queue(session, protocol::Chatting::frame_s2c("C4", "after invalid event range"));
            net.update(0, 32);
            const auto label = "event-invalid-range-" + std::to_string(kind);
            const auto q = c4_clock_observe(net, *session, before, label.c_str(), snapshot);
            expect(
                net.connected() && session->closes == 0 && q.ready &&
                    q.unix_seconds == clock.unix_seconds && q.event &&
                    c4_same_event(*q.event, event) && q.events.size() == 1 &&
                    q.events[0].id == "owned" && q.events[0].starts_at == 1700000010 &&
                    q.events[0].following_at == 1700000020 && q.maps.empty() &&
                    q.chats.size() == 2 && q.chats[0].text == "before invalid event range" &&
                    q.chats[1].text == "after invalid event range",
                "C4 invalid bonus and inconsistent countdown preserve the entire accepted event and clock");
        }
    }
    void test_c4_map_payload_boundaries(bool snapshot) {
        const protocol::MapStateFields first{
            250, 249, 248, protocol::MapStateTimer::Ritual, 0xffffffffu};
        const protocol::MapStateFields last{
            247, 246, 245, protocol::MapStateTimer::Lord, 0xfffffffeu};
        const auto complete = c4_clock_payload(protocol::MapState::frame(first));
        for (unsigned kind = 0; kind < 10; ++kind) {
            client::NetClient net;
            const auto session = attach(net);
            auto body = complete;
            if (kind < 8) {
                body.resize(kind);
            } else if (kind == 8) {
                body[3] = 255;
            } else {
                body.push_back(0);
            }
            queue(session, protocol::MapState::frame(first));
            queue(session, protocol::Chatting::frame_s2c("C4", "before invalid map"));
            queue(session, c4_clock_frame(protocol::S2C::MapState, std::move(body)));
            queue(session, protocol::MapState::frame(last));
            queue(session, protocol::Chatting::frame_s2c("C4", "after invalid map"));
            net.update(0, 32);
            const auto label = "map-invalid-" + std::to_string(kind);
            const auto q = c4_clock_observe(net, *session, {}, label.c_str(), snapshot);
            expect(
                net.connected() && session->closes == 0 && !q.ready && !q.event &&
                    q.maps.size() == 2 && q.maps[0].map == first.map &&
                    q.maps[0].phase == first.phase && q.maps[0].active_mask == first.active_mask &&
                    q.maps[0].timer == first.timer &&
                    q.maps[0].timer_seconds == first.timer_seconds && q.maps[1].map == last.map &&
                    q.maps[1].phase == last.phase && q.maps[1].active_mask == last.active_mask &&
                    q.maps[1].timer == last.timer &&
                    q.maps[1].timer_seconds == last.timer_seconds && q.chats.size() == 2 &&
                    q.chats[0].text == "before invalid map" &&
                    q.chats[1].text == "after invalid map",
                "C4 invalid map lengths and timer preserve complete ordered raw-field neighbors");
        }
    }
    void test_c4_clock_invalid_snapshots(bool snapshot) {
        const protocol::ClockSyncFields clock{1700000000,
                                              3600,
                                              19,
                                              {{"owned", "Owned event", 1700000010, 1700000020},
                                               {"other", "Other event", 1700000030, 1700000040}}};
        const protocol::ExpEventStateFields event{
            true, 77, 23016, 9007199254740993ULL, 9007199254740995ULL, 35, 123, 40};
        const auto complete = c4_clock_payload(protocol::ClockSync::frame(clock));
        for (unsigned kind = 0; kind < 5; ++kind) {
            client::NetClient net;
            const auto session = attach(net);
            auto body = complete;
            if (kind == 0) {
                body.clear();
            } else if (kind == 1) {
                body.resize(20);
            } else if (kind == 2) {
                protocol::PacketWriter invalid_offset;
                invalid_offset.write_i32(86401);
                std::copy(invalid_offset.payload().begin(),
                          invalid_offset.payload().end(),
                          body.begin() + 8);
            } else if (kind == 3) {
                const std::array<std::uint8_t, 6> old_id{{'o', 't', 'h', 'e', 'r', 0}};
                const std::array<std::uint8_t, 6> duplicate_id{{'o', 'w', 'n', 'e', 'd', 0}};
                const auto at = std::search(body.begin(), body.end(), old_id.begin(), old_id.end());
                expect(at != body.end(),
                       "C4 duplicate-ID mutation locates the authored second string");
                if (at != body.end()) {
                    std::copy(duplicate_id.begin(), duplicate_id.end(), at);
                }
            } else {
                body[20] = static_cast<std::uint8_t>(protocol::ClockSync::kMaxEvents + 1);
            }
            const auto before = std::chrono::steady_clock::now();
            queue(session, protocol::ClockSync::frame(clock));
            queue(session, protocol::ExpEventState::frame(event));
            queue(session, protocol::Property::frame_update_level(12));
            queue(session,
                  protocol::MapState::frame({1, 2, 3, protocol::MapStateTimer::Ritual, 4}));
            queue(session, protocol::Chatting::frame_s2c("C4", "before invalid clock"));
            queue(session, c4_clock_frame(protocol::S2C::ClockSync, std::move(body)));
            queue(session, protocol::Property::frame_update_level(13));
            queue(session, protocol::ClockSync::frame(clock));
            queue(session, protocol::ExpEventState::frame(event));
            queue(session, protocol::MapState::frame({5, 6, 7, protocol::MapStateTimer::Lord, 8}));
            queue(session, protocol::Chatting::frame_s2c("C4", "after invalid clock"));
            net.update(0, 32);
            const auto label = "clock-invalid-" + std::to_string(kind);
            const auto q = c4_clock_observe(net, *session, before, label.c_str(), snapshot);
            std::int32_t level = -1;
            expect(
                !net.connected() && session->closes == 1 && session->chunks.empty() && !q.ready &&
                    q.unix_seconds == 0 && q.events.empty() && !q.event && q.time == "--:--:--" &&
                    q.online == "--:--:--" && q.countdown == "00:00:00:00" &&
                    net.take_level(level) && level == 13 && q.maps.size() == 2 &&
                    q.maps[0].map == 1 && q.maps[0].phase == 2 && q.maps[0].active_mask == 3 &&
                    q.maps[0].timer_seconds == 4 && q.maps[1].map == 5 && q.maps[1].phase == 6 &&
                    q.maps[1].active_mask == 7 && q.maps[1].timer_seconds == 8 &&
                    q.chats.size() == 2 && q.chats[0].text == "before invalid clock" &&
                    q.chats[1].text == "after invalid clock",
                "C4 malformed clock closes once and continues through typed suffix before reset cleanup");
        }
    }

    std::array<std::int32_t, 25> c4_numeric_stats(const client::NetClient::CharInfoStats& s) {
        return {{s.level,
                 s.ctb,
                 s.hp,
                 s.max_hp,
                 s.mp,
                 s.max_mp,
                 s.str,
                 s.hth,
                 s.intl,
                 s.wis,
                 s.dex,
                 s.pu_point,
                 s.min_attack,
                 s.max_attack,
                 s.min_magic,
                 s.max_magic,
                 s.hit,
                 s.dodge,
                 s.defense,
                 s.absorb,
                 s.res_fire,
                 s.res_ice,
                 s.res_lightning,
                 s.res_curse,
                 s.res_palsy}};
    }
    struct C4PropertyObservation {
        client::NetClient::CharInfoStats stats;
        std::int32_t rage = 0;
        std::int32_t mortal = 0;
        std::int16_t move_speed = 0;
        bool hp_pending = false;
        bool max_hp_pending = false;
        bool mp_pending = false;
        bool max_mp_pending = false;
        bool level_pending = false;
        bool exp_pending = false;
        bool points_pending = false;
        std::int32_t hp = -1234567;
        std::int32_t max_hp = -1234567;
        std::int32_t mp = -1234567;
        std::int32_t max_mp = -1234567;
        std::int32_t level = -1234567;
        std::int64_t exp = -1234567890123LL;
        std::int64_t total = -1234567890123LL;
        std::int32_t pu = -1234567;
        std::int32_t su = -1234567;
        std::vector<client::PendingInfoMessage> infos;
        std::vector<client::PendingCombatFloat> floats;
        std::vector<client::PendingChat> chats;
    };
    C4PropertyObservation c4_property_observe(client::NetClient& net,
                                              const SessionState& session,
                                              const std::string& label,
                                              bool snapshot) {
        C4PropertyObservation q;
        q.stats = net.char_info_stats();
        q.rage = net.rage();
        q.mortal = net.mortal();
        q.move_speed = net.local_mspeed();
        q.hp_pending = net.take_cur_hp(q.hp);
        q.max_hp_pending = net.take_max_hp(q.max_hp);
        q.mp_pending = net.take_cur_mp(q.mp);
        q.max_mp_pending = net.take_max_mp(q.max_mp);
        q.level_pending = net.take_level(q.level);
        q.exp_pending = net.take_exp(q.exp, q.total);
        q.points_pending = net.take_points(q.pu, q.su);
        expect((q.hp_pending || q.hp == -1234567) && (q.max_hp_pending || q.max_hp == -1234567) &&
                   (q.mp_pending || q.mp == -1234567) &&
                   (q.max_mp_pending || q.max_mp == -1234567) &&
                   (q.level_pending || q.level == -1234567) &&
                   (q.exp_pending || (q.exp == -1234567890123LL && q.total == -1234567890123LL)) &&
                   (q.points_pending || (q.pu == -1234567 && q.su == -1234567)),
               "C4 initially false scalar takes preserve their independent sentinels");
        auto same = q;
        expect(!net.take_cur_hp(same.hp) && same.hp == q.hp && !net.take_max_hp(same.max_hp) &&
                   same.max_hp == q.max_hp && !net.take_cur_mp(same.mp) && same.mp == q.mp &&
                   !net.take_max_mp(same.max_mp) && same.max_mp == q.max_mp &&
                   !net.take_level(same.level) && same.level == q.level &&
                   !net.take_exp(same.exp, same.total) && same.exp == q.exp &&
                   same.total == q.total && !net.take_points(same.pu, same.su) && same.pu == q.pu &&
                   same.su == q.su,
               "C4 each scalar latch is consumed once and false preserves every output");
        expect(c4_numeric_stats(net.char_info_stats()) == c4_numeric_stats(q.stats) &&
                   net.rage() == q.rage && net.mortal() == q.mortal,
               "C4 consuming unread values does not mutate immediate public stats");
        q.infos = c4_clock_drain<client::PendingInfoMessage>(
            [&](auto& out) { return net.take_info_message(out); });
        q.floats = c4_clock_drain<client::PendingCombatFloat>(
            [&](auto& out) { return net.take_combat_float(out); });
        q.chats =
            c4_clock_drain<client::PendingChat>([&](auto& out) { return net.take_chat(out); });
        if (!snapshot) {
            return q;
        }
        C4ClockJson record;
        record.set_type(C4ClockJson::Type::Object);
        auto& out = record.object();
        out["checkpoint"] = c4_clock_text(label);
        out["domain"] = c4_clock_text("property");
        out["transport"] = c4_clock_fields(
            net.connected(), session.alive, session.eof, session.closes, session.sends);
        out["stats"] = c4_clock_array();
        for (const auto value : c4_numeric_stats(q.stats)) {
            out["stats"].array().push_back(c4_clock_fields(value));
        }
        out["rage_mortal_speed"] = c4_clock_fields(q.rage, q.mortal, q.move_speed);
        out["hp"] = c4_clock_fields(q.hp_pending, q.hp);
        out["max_hp"] = c4_clock_fields(q.max_hp_pending, q.max_hp);
        out["mp"] = c4_clock_fields(q.mp_pending, q.mp);
        out["max_mp"] = c4_clock_fields(q.max_mp_pending, q.max_mp);
        out["level"] = c4_clock_fields(q.level_pending, q.level);
        out["exp"] = c4_clock_fields(q.exp_pending, q.exp, q.total);
        out["points"] = c4_clock_fields(q.points_pending, q.pu, q.su);
        out["infos"] = c4_clock_array();
        for (const auto& value : q.infos) {
            auto item = c4_clock_array();
            item.array().push_back(c4_clock_text(value.text));
            item.array().push_back(value.kind ? c4_clock_fields(static_cast<unsigned>(*value.kind))
                                              : C4ClockJson{});
            if (value.color) {
                auto color = c4_clock_array();
                for (const auto channel : *value.color) {
                    C4ClockJson number;
                    number.set_type(C4ClockJson::Type::Number);
                    number.set_number(channel);
                    color.array().push_back(std::move(number));
                }
                item.array().push_back(std::move(color));
            } else {
                item.array().push_back(C4ClockJson{});
            }
            out["infos"].array().push_back(std::move(item));
        }
        out["floats"] = c4_clock_array();
        for (const auto& value : q.floats) {
            auto item = c4_clock_fields(
                static_cast<unsigned>(value.kind), value.anchor_id, value.wait_for_bolt);
            item.array().push_back(c4_clock_text(value.text));
            out["floats"].array().push_back(std::move(item));
        }
        out["chats"] = c4_clock_array();
        for (const auto& value : q.chats) {
            auto item = c4_clock_array();
            item.array().push_back(c4_clock_text(value.speaker));
            item.array().push_back(c4_clock_text(value.text));
            out["chats"].array().push_back(std::move(item));
        }
        std::cout << "c4 " << C4ClockJson::stringify(record, 0) << std::endl;
        return q;
    }
    void test_c4_word_destinations(bool snapshot) {
        using Stats = client::NetClient::CharInfoStats;
        enum class WordEffect { Stat, Hp, Mp, Pu, Su, Rage, Mortal };
        struct WordCase {
            std::uint8_t property;
            std::int32_t value;
            std::int32_t Stats::*member;
            WordEffect effect;
        };
        const auto p = [](protocol::PropertyType type) { return static_cast<std::uint8_t>(type); };
        const std::array<WordCase, 25> cases{
            {{p(protocol::PropertyType::Str), 101, &Stats::str, WordEffect::Stat},
             {p(protocol::PropertyType::Hth), 102, &Stats::hth, WordEffect::Stat},
             {p(protocol::PropertyType::Int), 103, &Stats::intl, WordEffect::Stat},
             {p(protocol::PropertyType::Wis), 104, &Stats::wis, WordEffect::Stat},
             {p(protocol::PropertyType::Dex), 105, &Stats::dex, WordEffect::Stat},
             {p(protocol::PropertyType::Hit), 106, &Stats::hit, WordEffect::Stat},
             {p(protocol::PropertyType::Dodge), 107, &Stats::dodge, WordEffect::Stat},
             {p(protocol::PropertyType::MinAttack), 108, &Stats::min_attack, WordEffect::Stat},
             {p(protocol::PropertyType::MaxAttack), 109, &Stats::max_attack, WordEffect::Stat},
             {p(protocol::PropertyType::MinMagic), 110, &Stats::min_magic, WordEffect::Stat},
             {p(protocol::PropertyType::MaxMagic), 111, &Stats::max_magic, WordEffect::Stat},
             {p(protocol::PropertyType::Defense), 112, &Stats::defense, WordEffect::Stat},
             {p(protocol::PropertyType::Absorb), 113, &Stats::absorb, WordEffect::Stat},
             {p(protocol::PropertyType::ResFire), 114, &Stats::res_fire, WordEffect::Stat},
             {p(protocol::PropertyType::ResIce), 115, &Stats::res_ice, WordEffect::Stat},
             {p(protocol::PropertyType::ResLitning), 116, &Stats::res_lightning, WordEffect::Stat},
             {p(protocol::PropertyType::ResCurse), 117, &Stats::res_curse, WordEffect::Stat},
             {p(protocol::PropertyType::ResPalsy), 118, &Stats::res_palsy, WordEffect::Stat},
             {p(protocol::PropertyType::Contribute), -119, &Stats::ctb, WordEffect::Stat},
             {p(protocol::PropertyType::CurHp), 120, &Stats::hp, WordEffect::Hp},
             {p(protocol::PropertyType::CurMp), 121, &Stats::mp, WordEffect::Mp},
             {p(protocol::PropertyType::PuPoint), 122, &Stats::pu_point, WordEffect::Pu},
             {p(protocol::PropertyType::SuPoint), 123, nullptr, WordEffect::Su},
             {protocol::kPropertyRage, -124, nullptr, WordEffect::Rage},
             {protocol::kPropertyMortal, -125, nullptr, WordEffect::Mortal}}};
        client::NetClient net;
        const auto session = attach(net);
        queue(session, protocol::Property::frame_update(p(protocol::PropertyType::PuPoint), 3));
        queue(session, protocol::Property::frame_update(p(protocol::PropertyType::SuPoint), 4));
        net.update(0, 32);
        auto q = c4_property_observe(net, *session, "word-points-seed", snapshot);
        expect(
            q.points_pending && q.pu == 3 && q.su == 4 && q.stats.pu_point == 3 &&
                q.infos.empty() && q.floats.empty(),
            "C4 public points fixture establishes known PU and SU before per-destination checks");
        auto expected = q.stats;
        std::int32_t expected_rage = q.rage;
        std::int32_t expected_mortal = q.mortal;
        std::int32_t expected_su = 4;
        for (std::size_t i = 0; i < cases.size(); ++i) {
            const auto& value = cases[i];
            queue(session, protocol::Property::frame_update(value.property, value.value));
            net.update(0, 32);
            if (value.member) {
                expected.*(value.member) = value.value;
            }
            if (value.effect == WordEffect::Rage) {
                expected_rage = value.value;
            }
            if (value.effect == WordEffect::Mortal) {
                expected_mortal = value.value;
            }
            if (value.effect == WordEffect::Su) {
                expected_su = value.value;
            }
            q = c4_property_observe(
                net, *session, "word-destination-" + std::to_string(i), snapshot);
            const bool points = value.effect == WordEffect::Pu || value.effect == WordEffect::Su;
            expect(c4_numeric_stats(q.stats) == c4_numeric_stats(expected) &&
                       q.rage == expected_rage && q.mortal == expected_mortal &&
                       q.hp_pending == (value.effect == WordEffect::Hp) &&
                       q.mp_pending == (value.effect == WordEffect::Mp) &&
                       q.points_pending == points && !q.max_hp_pending && !q.max_mp_pending &&
                       !q.level_pending && !q.exp_pending &&
                       (!q.hp_pending || q.hp == expected.hp) &&
                       (!q.mp_pending || q.mp == expected.mp) &&
                       (!points || (q.pu == expected.pu_point && q.su == expected_su)) &&
                       q.infos.empty() && q.floats.empty(),
                   "C4 each word property changes only its explicit public destination and latch");
        }
        for (const auto property :
             {p(protocol::PropertyType::ASpeed), protocol::kPropertyMSpeed, std::uint8_t{255}}) {
            queue(session, protocol::Property::frame_update(property, 333));
            net.update(0, 32);
            q = c4_property_observe(
                net, *session, "word-ignored-" + std::to_string(property), snapshot);
            expect(c4_numeric_stats(q.stats) == c4_numeric_stats(expected) &&
                       q.rage == expected_rage && q.mortal == expected_mortal &&
                       q.move_speed == 0 && !q.hp_pending && !q.mp_pending && !q.max_hp_pending &&
                       !q.max_mp_pending && !q.level_pending && !q.exp_pending &&
                       !q.points_pending && q.infos.empty() && q.floats.empty(),
                   "C4 ignored word properties do not leak into other property families");
        }
        queue(session, protocol::Property::frame_update(p(protocol::PropertyType::PuPoint), 122));
        queue(session, protocol::Property::frame_update(p(protocol::PropertyType::SuPoint), 127));
        queue(session, protocol::Property::frame_update(p(protocol::PropertyType::PuPoint), 129));
        net.update(0, 32);
        q = c4_property_observe(net, *session, "word-points-coalesced", snapshot);
        expect(q.points_pending && q.pu == 129 && q.su == 127 && q.stats.pu_point == 129,
               "C4 repeated and interleaved PU/SU updates publish their latest pair once");
    }

    unsigned c4_property_latches(const C4PropertyObservation& q) {
        return (q.hp_pending ? 1u : 0u) | (q.max_hp_pending ? 2u : 0u) | (q.mp_pending ? 4u : 0u) |
               (q.max_mp_pending ? 8u : 0u) | (q.level_pending ? 16u : 0u) |
               (q.exp_pending ? 32u : 0u) | (q.points_pending ? 64u : 0u);
    }
    void c4_feedback(const C4PropertyObservation& q,
                     const std::vector<client::PendingInfoMessage>& infos,
                     const std::vector<std::string>& heals) {
        expect(q.infos.size() == infos.size() && q.floats.size() == heals.size(),
               "C4 feedback has the exact independent info and heal counts");
        for (std::size_t i = 0; i < std::min(q.infos.size(), infos.size()); ++i) {
            expect(q.infos[i].text == infos[i].text && q.infos[i].kind == infos[i].kind &&
                       q.infos[i].color == infos[i].color,
                   "C4 feedback retains exact text, kind, color and authored FIFO order");
        }
        for (std::size_t i = 0; i < std::min(q.floats.size(), heals.size()); ++i) {
            expect(q.floats[i].kind == client::CombatFloatKind::Heal &&
                       q.floats[i].text == heals[i] && q.floats[i].anchor_id == 0 &&
                       !q.floats[i].wait_for_bolt,
                   "C4 HP restoration emits its exact immediate unanchored heal float");
        }
    }
    void test_c4_vitals_and_reconnect(bool snapshot) {
        client::NetClient net;
        auto session = attach(net);
        const auto hp = static_cast<std::uint8_t>(protocol::PropertyType::Hp);
        const auto mp = static_cast<std::uint8_t>(protocol::PropertyType::Mp);
        const auto cur_hp = static_cast<std::uint8_t>(protocol::PropertyType::CurHp);
        const auto cur_mp = static_cast<std::uint8_t>(protocol::PropertyType::CurMp);
        const auto receive = [&](const Bytes& frame, const char* label) {
            queue(session, frame);
            net.update(0, 32);
            return c4_property_observe(net, *session, label, snapshot);
        };
        auto q = receive(protocol::Property::frame_update_vital(hp, 100, 200), "hp-first-vital");
        expect(c4_property_latches(q) == 3 && q.hp == 100 && q.max_hp == 200 && q.stats.hp == 100 &&
                   q.stats.max_hp == 200,
               "C4 first vital HP publishes current and maximum without initial feedback");
        c4_feedback(q, {}, {});
        q = receive(protocol::Property::frame_update(cur_hp, 120), "hp-current-increase");
        expect(c4_property_latches(q) == 1 && q.hp == 120 && q.stats.max_hp == 200,
               "C4 current-only HP leaves the stored maximum and maximum latch unchanged");
        c4_feedback(
            q,
            {{"Health Point is increased by 20.", protocol::BattleMsgKind::Hp, std::nullopt}},
            {"20"});
        q = receive(protocol::Property::frame_update_vital(hp, 80, 300), "hp-vital-decrease");
        expect(c4_property_latches(q) == 3 && q.hp == 80 && q.max_hp == 300,
               "C4 decreasing vital HP still publishes both authoritative values");
        c4_feedback(q, {}, {});
        q = receive(protocol::Property::frame_update(cur_hp, 80), "hp-unchanged");
        expect(c4_property_latches(q) == 1 && q.hp == 80,
               "C4 unchanged current HP republishes its unread value");
        c4_feedback(q, {}, {});
        q = receive(protocol::Property::frame_update_vital(hp, 90, 400), "hp-vital-increase");
        expect(c4_property_latches(q) == 3 && q.hp == 90 && q.max_hp == 400,
               "C4 increasing vital HP replaces maximum and current together");
        c4_feedback(
            q,
            {{"Health Point is increased by 10.", protocol::BattleMsgKind::Hp, std::nullopt}},
            {"10"});
        queue(session, protocol::Property::frame_update(cur_hp, -5));
        queue(session, protocol::Property::frame_update(cur_hp, 50));
        net.update(0, 32);
        q = c4_property_observe(net, *session, "hp-negative-previous", snapshot);
        expect(c4_property_latches(q) == 1 && q.hp == 50 && q.stats.max_hp == 400,
               "C4 accepted negative previous HP suppresses its later positive transition");
        c4_feedback(q, {}, {});
        q = receive(protocol::Property::frame_update(cur_hp, 55), "hp-after-negative-baseline");
        c4_feedback(
            q,
            {{"Health Point is increased by 5.", protocol::BattleMsgKind::Hp, std::nullopt}},
            {"5"});
        expect(c4_property_latches(q) == 1 && q.hp == 55,
               "C4 nonnegative HP restores later positive delta feedback");

        q = receive(protocol::Property::frame_update(cur_mp, 100), "mp-first-current");
        expect(c4_property_latches(q) == 4 && q.mp == 100 && q.stats.max_mp == 0,
               "C4 first current MP suppresses feedback without inventing a maximum");
        c4_feedback(q, {}, {});
        q = receive(protocol::Property::frame_update_vital(mp, 80, 200), "mp-vital-decrease");
        expect(c4_property_latches(q) == 12 && q.mp == 80 && q.max_mp == 200,
               "C4 vital MP publishes both current and maximum");
        c4_feedback(
            q,
            {{"Magic Point is decreased by 20.", protocol::BattleMsgKind::Mp, std::nullopt}},
            {});
        q = receive(protocol::Property::frame_update(cur_mp, 95), "mp-current-increase");
        expect(c4_property_latches(q) == 4 && q.mp == 95 && q.stats.max_mp == 200,
               "C4 current MP leaves the prior maximum untouched");
        c4_feedback(
            q,
            {{"Magic Point is increased by 15.", protocol::BattleMsgKind::Mp, std::nullopt}},
            {});
        q = receive(protocol::Property::frame_update_vital(mp, 95, 201), "mp-unchanged");
        expect(c4_property_latches(q) == 12 && q.mp == 95 && q.max_mp == 201,
               "C4 unchanged MP current may still replace its authoritative maximum");
        c4_feedback(q, {}, {});
        queue(session, protocol::Property::frame_update(cur_mp, -7));
        queue(session, protocol::Property::frame_update(cur_mp, 50));
        net.update(0, 32);
        q = c4_property_observe(net, *session, "mp-negative-previous", snapshot);
        expect(
            c4_property_latches(q) == 4 && q.mp == 50 && q.stats.max_mp == 201,
            "C4 negative MP transition retains signed feedback then suppresses its positive successor");
        c4_feedback(
            q,
            {{"Magic Point is decreased by 102.", protocol::BattleMsgKind::Mp, std::nullopt}},
            {});
        q = receive(protocol::Property::frame_update(cur_mp, 55), "mp-after-negative-baseline");
        expect(c4_property_latches(q) == 4 && q.mp == 55,
               "C4 nonnegative MP resumes ordinary feedback");
        c4_feedback(
            q, {{"Magic Point is increased by 5.", protocol::BattleMsgKind::Mp, std::nullopt}}, {});

        queue(session, protocol::Property::frame_update(cur_hp, 60));
        queue(session, protocol::Property::frame_update_level(9));
        queue(session,
              protocol::MapState::frame({31, 32, 33, protocol::MapStateTimer::Ritual, 34}));
        net.update(0, 32);
        net.close();
        session = attach(net);
        queue(session, protocol::Property::frame_update(cur_hp, 80));
        queue(session, protocol::Property::frame_update(cur_mp, -2));
        queue(session, protocol::Property::frame_update(cur_mp, 50));
        net.update(0, 32);
        q = c4_property_observe(net, *session, "property-reconnect-retains", snapshot);
        expect(
            c4_property_latches(q) == 53 && q.hp == 80 && q.mp == 50 && q.level == 9 &&
                q.stats.max_hp == 400 && q.stats.max_mp == 201 && q.exp == 0 &&
                q.total == std::max<std::int64_t>(1, protocol::need_exp_for_level(9)),
            "C4 existing close and adopt retain scalar state and unread level/EXP/current latches");
        c4_feedback(
            q,
            {{"Health Point is increased by 5.", protocol::BattleMsgKind::Hp, std::nullopt},
             {"Health Point is increased by 20.", protocol::BattleMsgKind::Hp, std::nullopt},
             {"Magic Point is decreased by 57.", protocol::BattleMsgKind::Mp, std::nullopt}},
            {"5", "20"});
        const auto retained =
            c4_clock_observe(net, *session, {}, "reconnect-map-retention", snapshot);
        expect(!retained.ready && !retained.event && retained.maps.size() == 1 &&
                   retained.maps[0].map == 31 && retained.maps[0].phase == 32 &&
                   retained.maps[0].active_mask == 33 && retained.maps[0].timer_seconds == 34,
               "C4 close clears clock/event synchronization while preserving pending map FIFO");
    }

    void test_c4_exp_and_level(bool snapshot) {
        client::NetClient net;
        const auto session = attach(net);
        const auto receive = [&](const Bytes& frame, const char* label) {
            queue(session, frame);
            net.update(0, 32);
            return c4_property_observe(net, *session, label, snapshot);
        };
        auto q = receive(protocol::Property::frame_update_exp(101, 0), "exp-initial-zero-delta");
        expect(c4_property_latches(q) == 32 && q.stats.level == 1 && q.exp == 101 &&
                   q.total == std::max<std::int64_t>(1, protocol::need_exp_for_level(1)),
               "C4 EXP uses the current level and publishes no level latch");
        c4_feedback(q, {}, {});
        q = receive(protocol::Property::frame_update_level(12), "level-after-exp");
        expect(c4_property_latches(q) == 48 && q.level == 12 && q.stats.level == 12 &&
                   q.exp == 101 &&
                   q.total == std::max<std::int64_t>(1, protocol::need_exp_for_level(12)),
               "C4 level after EXP preserves current EXP and republishes its new total");
        c4_feedback(q, {}, {});
        q = receive(protocol::Property::frame_update_exp(201, 7), "exp-positive");
        expect(c4_property_latches(q) == 32 && q.exp == 201 && q.stats.level == 12,
               "C4 positive EXP delta leaves the established level unchanged");
        c4_feedback(
            q, {{"Exp is increased by 7.", protocol::BattleMsgKind::Exp, std::nullopt}}, {});
        q = receive(protocol::Property::frame_update_exp(190, -11), "exp-negative");
        expect(c4_property_latches(q) == 32 && q.exp == 190 && q.stats.level == 12,
               "C4 signed negative EXP delta preserves its authoritative current value");
        c4_feedback(
            q, {{"Exp is decreased by 11.", protocol::BattleMsgKind::Exp, std::nullopt}}, {});
        for (const auto value : {0, -3, protocol::kMaxPlayerLevel + 1}) {
            const auto label = "level-boundary-" + std::to_string(value);
            q = receive(protocol::Property::frame_update_level(value), label.c_str());
            const int expected = value < 1 ? 1 : value;
            expect(
                c4_property_latches(q) == 48 && q.level == expected && q.stats.level == expected &&
                    q.exp == 190 &&
                    q.total == std::max<std::int64_t>(1, protocol::need_exp_for_level(expected)) &&
                    (value <= protocol::kMaxPlayerLevel || q.total == 1),
                "C4 level has only the existing lower clamp and minimum-one out-of-table EXP total");
            c4_feedback(q, {}, {});
        }
        queue(session, protocol::Property::frame_update_exp(333, 2));
        queue(session, protocol::frame_trading_post_pickup({0, 0, 7}));
        queue(session, protocol::Property::frame_update_level(4));
        queue(session, protocol::Property::frame_update_exp(444, -3));
        net.update(0, 32);
        q = c4_property_observe(net, *session, "exp-level-common-info-order", snapshot);
        expect(c4_property_latches(q) == 48 && q.level == 4 && q.exp == 444 &&
                   q.total == std::max<std::int64_t>(1, protocol::need_exp_for_level(4)),
               "C4 interleaved EXP and Level arrivals coalesce only unread values");
        c4_feedback(q,
                    {{"Exp is increased by 2.", protocol::BattleMsgKind::Exp, std::nullopt},
                     {"You got 7 geons",
                      protocol::BattleMsgKind::MoneyGet,
                      std::array<float, 3>{1.f, 1.f, 1.f}},
                     {"Exp is decreased by 3.", protocol::BattleMsgKind::Exp, std::nullopt}},
                    {});
        queue(session, protocol::Property::frame_update_level(5));
        net.update(0, 32);
        std::int32_t level = -1;
        expect(net.take_level(level) && level == 5,
               "C4 direct level consumption reads the new level");
        q = c4_property_observe(net, *session, "level-take-preserves-exp", snapshot);
        expect(c4_property_latches(q) == 32 && q.stats.level == 5 && q.exp == 444 &&
                   q.total == std::max<std::int64_t>(1, protocol::need_exp_for_level(5)),
               "C4 consuming level does not consume unread EXP");
        queue(session, protocol::Property::frame_update_level(6));
        net.update(0, 32);
        std::int64_t exp = -1;
        std::int64_t total = -1;
        expect(net.take_exp(exp, total) && exp == 444 &&
                   total == std::max<std::int64_t>(1, protocol::need_exp_for_level(6)),
               "C4 direct EXP consumption retains authoritative level-derived total");
        q = c4_property_observe(net, *session, "exp-take-preserves-level", snapshot);
        expect(c4_property_latches(q) == 16 && q.level == 6 && q.stats.level == 6,
               "C4 consuming EXP does not consume unread level");
    }

    void test_c4_property_payload_boundaries(bool snapshot) {
        const auto property = [](protocol::PropertyType value) {
            return static_cast<std::uint8_t>(value);
        };
        const std::array<Bytes, 4> layouts{
            protocol::Property::frame_update(property(protocol::PropertyType::Str), 77),
            protocol::Property::frame_update_level(3),
            protocol::Property::frame_update_vital(property(protocol::PropertyType::Hp), 1, 2),
            protocol::Property::frame_update_exp(301, 0)};
        for (std::size_t kind = 0; kind < layouts.size(); ++kind) {
            const auto complete = c4_clock_payload(layouts[kind]);
            // Every short prefix plus one accepted trailing byte, each with fresh unread state.
            for (std::size_t size = 0; size <= complete.size(); ++size) {
                client::NetClient net;
                const auto session = attach(net);
                auto expected = net.char_info_stats();
                expected.level = 12;
                expected.hp = 100;
                expected.max_hp = 200;
                expected.mp = 50;
                expected.max_mp = 80;
                expected.pu_point = 3;
                queue(session, protocol::Property::frame_update_level(12));
                queue(session, protocol::Property::frame_update_exp(201, 0));
                queue(session,
                      protocol::Property::frame_update_vital(
                          property(protocol::PropertyType::Hp), 100, 200));
                queue(session,
                      protocol::Property::frame_update_vital(
                          property(protocol::PropertyType::Mp), 50, 80));
                queue(
                    session,
                    protocol::Property::frame_update(property(protocol::PropertyType::PuPoint), 3));
                queue(
                    session,
                    protocol::Property::frame_update(property(protocol::PropertyType::SuPoint), 4));
                queue(session, protocol::Chatting::frame_s2c("C4", "before property boundary"));
                Bytes body(complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(size));
                const bool accepted_tail = size == complete.size();
                if (accepted_tail) {
                    body.push_back(0xab);
                }
                queue(session, c4_clock_frame(protocol::S2C::UpdateProperty, std::move(body)));
                queue(session, protocol::Chatting::frame_s2c("C4", "after property boundary"));
                net.update(0, 32);
                if (accepted_tail) {
                    if (kind == 0) {
                        expected.str = 77;
                    } else if (kind == 1) {
                        expected.level = 3;
                    } else if (kind == 2) {
                        expected.hp = 1;
                        expected.max_hp = 2;
                    }
                }
                const auto label = "property-boundary-" + std::to_string(kind) + "-" +
                                   (accepted_tail ? std::string("tail") : std::to_string(size));
                const auto q = c4_property_observe(net, *session, label, snapshot);
                const auto expected_exp = accepted_tail && kind == 3 ? 301 : 201;
                expect(
                    net.connected() && session->closes == 0 && session->chunks.empty() &&
                        c4_numeric_stats(q.stats) == c4_numeric_stats(expected) &&
                        c4_property_latches(q) == 127 && q.hp == expected.hp &&
                        q.max_hp == expected.max_hp && q.mp == 50 && q.max_mp == 80 &&
                        q.level == expected.level && q.exp == expected_exp &&
                        q.total == std::max<std::int64_t>(
                                       1, protocol::need_exp_for_level(expected.level)) &&
                        q.pu == 3 && q.su == 4 && q.rage == 0 && q.mortal == 0 &&
                        q.move_speed == 0 && q.infos.empty() && q.floats.empty() &&
                        q.chats.size() == 2 && q.chats[0].speaker == "C4" &&
                        q.chats[0].text == "before property boundary" &&
                        q.chats[1].speaker == "C4" && q.chats[1].text == "after property boundary",
                    "C4 every short property layout retains complete unread state while the extra-byte neighbor applies normally");
            }
        }
    }

    void test_c4_code_and_exit(bool snapshot) {
        for (const bool replace : {false, true}) {
            client::NetClient net;
            const auto session = attach(net);
            const auto before = std::chrono::steady_clock::now();
            queue(session, server::frame_s2c_code(12345));
            auto malformed = c4_clock_payload(server::frame_s2c_code(67890));
            malformed.pop_back();
            queue(session, c4_clock_frame(protocol::S2C::Code, std::move(malformed)));
            if (replace) {
                auto trailing = c4_clock_payload(server::frame_s2c_code(67890));
                trailing.push_back(0xab);
                queue(session, c4_clock_frame(protocol::S2C::Code, std::move(trailing)));
            }
            queue(session,
                  protocol::MapState::frame({7, 8, 9, protocol::MapStateTimer::Ritual, 11}));
            queue(session, protocol::Chatting::frame_s2c("C4", "handshake suffix"));
            expect(net.handshake() && net.handshook(),
                   "C4 valid code survives malformed neighbors during the real handshake drain");
            Bytes expected = protocol::Handshake::build_connect(0);
            append(expected, protocol::Handshake::build_answer(replace ? 67890 : 12345));
            const auto q = c4_clock_observe(net,
                                            *session,
                                            before,
                                            replace ? "code-latest-B" : "code-retains-A-without-B",
                                            snapshot);
            expect(
                session->sent == expected && q.maps.size() == 1 && q.maps[0].map == 7 &&
                    q.maps[0].phase == 8 && q.maps[0].active_mask == 9 &&
                    q.maps[0].timer_seconds == 11 && q.chats.size() == 1 &&
                    q.chats[0].text == "handshake suffix",
                "C4 handshake emits exact latest-valid answer and fully drains its same-chunk suffix");
            queue(session, protocol::GameExit::frame_server_answer(true));
            net.close();
            append(expected, protocol::GameExit::frame_client_request());
            expect(
                !net.connected() && !net.handshook() && session->closes == 1 &&
                    session->sent == expected,
                "C4 established handshake fixture exits through the ordinary saved acknowledgement");
        }
        client::NetClient net;
        const auto session = attach(net);
        queue(session, server::frame_s2c_code(24680));
        expect(net.handshake(), "C4 close-drain fixture starts through the ordinary handshake");
        session->chunks.push_back(c4_clock_frame(protocol::S2C::AnsGameExit, {}));
        session->chunks.push_back(c4_clock_frame(protocol::S2C::AnsGameExit, {1, 0}));
        Bytes third = protocol::Property::frame_update_level(14);
        append(third, protocol::MapState::frame({17, 18, 19, protocol::MapStateTimer::Lord, 23}));
        append(third, c4_clock_frame(protocol::S2C::AnsGameExit, {255}));
        append(third, protocol::Chatting::frame_s2c("C4", "accepted-answer suffix"));
        session->chunks.push_back(std::move(third));
        const auto unread = protocol::Chatting::frame_s2c("C4", "next chunk remains unread");
        session->chunks.push_back(unread);
        const auto before = std::chrono::steady_clock::now();
        net.close();
        const auto q =
            c4_clock_observe(net, *session, before, "exit-invalid-then-nonzero-answer", snapshot);
        auto expected = protocol::Handshake::build_connect(0);
        append(expected, protocol::Handshake::build_answer(24680));
        append(expected, protocol::GameExit::frame_client_request());
        std::int32_t level = -1;
        expect(
            !net.connected() && session->closes == 1 && session->sent == expected &&
                session->chunks.size() == 1 && session->chunks.front() == unread &&
                q.maps.size() == 1 && q.maps[0].map == 17 &&
                q.maps[0].timer == protocol::MapStateTimer::Lord && q.chats.size() == 1 &&
                q.chats[0].text == "accepted-answer suffix" && net.take_level(level) && level == 14,
            "C4 close ignores malformed answers, accepts nonzero255, drains its suffix and leaves the next chunk unread");
    }

    void test_c4_snapshot_packets(bool snapshot) {
        test_c4_word_destinations(snapshot);
        test_c4_vitals_and_reconnect(snapshot);
        test_c4_exp_and_level(snapshot);
        test_c4_property_payload_boundaries(snapshot);
        test_c4_clock_snapshots(snapshot);
        test_c4_event_invalid_ranges(snapshot);
        test_c4_map_payload_boundaries(snapshot);
        test_c4_clock_invalid_snapshots(snapshot);
        test_c4_code_and_exit(snapshot);
    }
    // C4_BASELINE_DRAFT_END

    // C5_WORLD_PACKET_BASELINE_BEGIN
    using C5Json = platform::Json;
    C5Json c5_text(std::string text) {
        C5Json value;
        value.set_type(C5Json::Type::String);
        value.string() = std::move(text);
        return value;
    }
    C5Json c5_array() {
        C5Json value;
        value.set_type(C5Json::Type::Array);
        return value;
    }
    template <typename Value> C5Json c5_number(Value value) {
        if constexpr (std::is_floating_point_v<Value>) {
            std::ostringstream text;
            text << std::setprecision(std::numeric_limits<Value>::max_digits10) << value;
            return c5_text(text.str());
        } else {
            return c5_text(std::to_string(value));
        }
    }
    template <typename... Values> C5Json c5_fields(Values... values) {
        auto result = c5_array();
        (result.array().push_back(c5_number(values)), ...);
        return result;
    }
    template <typename Map> std::vector<std::uint32_t> c5_keys(const Map& map) {
        std::vector<std::uint32_t> keys;
        for (const auto& entry : map) {
            keys.push_back(entry.first);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }
    bool c5_near(float a, float b) {
        return std::abs(a - b) < 0.001f;
    }
    Bytes c5_frame(protocol::S2C type, Bytes body) {
        protocol::PacketWriter writer;
        writer.payload() = std::move(body);
        return writer.frame(static_cast<std::uint8_t>(type));
    }
    Bytes c5_payload(const Bytes& frame) {
        expect(frame.size() >= 3, "C5 owned builder emits a real framed packet");
        return frame.size() >= 3 ? Bytes(frame.begin() + 3, frame.end()) : Bytes{};
    }
    Bytes c5_id(protocol::S2C type, std::uint32_t id) {
        protocol::PacketWriter w;
        w.write_u32(id);
        return w.frame(static_cast<std::uint8_t>(type));
    }
    Bytes c5_gstate(std::uint32_t id, std::uint32_t value) {
        protocol::PacketWriter w;
        w.write_u32(id);
        w.write_u32(value);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::GState));
    }
    Bytes c5_monster(std::uint32_t id,
                     int x = 1000,
                     int y = 2000,
                     std::uint16_t dir = 64,
                     std::uint32_t state = 0,
                     std::int32_t hp = 200,
                     std::int32_t max_hp = 300,
                     std::uint16_t index = 77,
                     std::uint8_t race = 2,
                     const std::string& name = "Owned C5 monster") {
        protocol::PacketWriter w;
        w.write_u16(index);
        w.write_u32(id);
        w.write_u32(static_cast<std::uint32_t>(x));
        w.write_u32(static_cast<std::uint32_t>(y));
        w.write_u16(dir);
        w.write_i32(hp);
        w.write_i32(max_hp);
        w.write_u32(state);
        w.write_u32(0x81234567u);
        w.write_u64(0x123456789abcdef0ULL);
        w.write_string("ignored owned guild");
        w.write_u8(race);
        w.write_u32(0x23456789u);
        w.write_string(name);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::CreateMonster));
    }
    Bytes c5_monster_move(std::uint32_t id, int dx, int dy, bool end, std::uint8_t run) {
        protocol::PacketWriter w;
        w.write_u32(id);
        w.write_u8(static_cast<std::uint8_t>(dx));
        w.write_u8(static_cast<std::uint8_t>(dy));
        w.write_u8(run);
        return w.frame(static_cast<std::uint8_t>(end ? protocol::S2C::MoveMonsterEnd
                                                     : protocol::S2C::MoveMonsterOn));
    }
    Bytes c5_item(std::uint32_t id,
                  int x = 1000,
                  int y = 2000,
                  std::uint32_t count = 17,
                  std::uint16_t index = 31) {
        protocol::PacketWriter w;
        w.write_u16(index);
        w.write_u32(id);
        w.write_u32(static_cast<std::uint32_t>(x));
        w.write_u32(static_cast<std::uint32_t>(y));
        w.write_u32(count);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::CreateItem));
    }
    Bytes c5_player(std::uint32_t id,
                    bool self,
                    int x = 1000,
                    int y = 2000,
                    int z = -37,
                    std::uint32_t state = 0,
                    bool cosmetics = true,
                    const std::string& name = "Owned C5 player") {
        const std::uint16_t wear[7] = {101, 102, 103, 104, 105, 106, 107};
        protocol::SkinIndices skins{};
        if (cosmetics) {
            for (std::size_t i = 0; i < skins.size(); ++i) {
                skins[i] = static_cast<std::uint16_t>(301 + i);
            }
        }
        return protocol::CreatePlayer::frame(id,
                                             name,
                                             self ? 0x83 : 2,
                                             x,
                                             y,
                                             z,
                                             91,
                                             state,
                                             wear,
                                             11,
                                             12,
                                             skins,
                                             cosmetics ? 401 : 0);
    }
    Bytes c5_npc(std::uint32_t id,
                 int x = 1000,
                 int y = 2000,
                 int z = -37,
                 std::uint32_t state = 0,
                 std::uint16_t index = 88) {
        return protocol::Quest::frame_create_npc({id, index, 7, x, y, z, 99, state, 12345});
    }
    C5Json c5_player_fields(const protocol::CreatePlayerFields& p) {
        auto value =
            c5_fields(p.id, p.class_byte, p.x, p.y, p.z, p.dir, p.gstate, p.shield, p.face, p.hair);
        value.array().push_back(c5_text(p.name));
        for (auto n : p.wear) {
            value.array().push_back(c5_number(n));
        }
        for (auto n : p.skins) {
            value.array().push_back(c5_number(n));
        }
        return value;
    }
    void c5_expect_player(const protocol::CreatePlayerFields& p,
                          std::uint32_t id,
                          bool self,
                          int x,
                          int y,
                          int z,
                          std::uint32_t state,
                          bool cosmetics = true) {
        expect(p.id == id && p.name == "Owned C5 player" && p.class_byte == (self ? 0x83 : 2) &&
                   p.x == x && p.y == y && p.z == z && p.dir == 91 && p.gstate == state &&
                   p.shield == (cosmetics ? 401 : 0) && p.face == 11 && p.hair == 12,
               "C5 player keeps every authored identity, pose, class and appearance scalar");
        bool exact = true;
        for (std::size_t i = 0; i < 7; ++i) {
            exact = exact && p.wear[i] == 101 + i;
        }
        for (std::size_t i = 0; i < p.skins.size(); ++i) {
            exact = exact && p.skins[i] == (cosmetics ? 301 + i : 0);
        }
        expect(exact, "C5 all wear and cosmetic slots retain authored order");
    }
    struct C5Observation {
        bool have_self = false;
        protocol::CreatePlayerFields self;
        std::vector<protocol::CreatePlayerFields> others;
        std::vector<std::uint32_t> removed;
        std::vector<client::PendingChat> chats;
        std::vector<std::pair<std::int32_t, std::uint32_t>> html;
        bool before = false;
        std::array<int, 3> correction{{-7001, -7002, -7003}};
        bool teleport = false;
        std::array<int, 4> destination{{-8001, -8002, -8003, -8004}};
    };
    C5Observation c5_observe(client::NetClient& net,
                             const SessionState& session,
                             const std::string& name,
                             bool snapshot) {
        C5Observation q;
        q.self.id = 999999;
        q.have_self = net.take_create_player(q.self);
        auto same = q.self;
        expect(!net.take_create_player(same) && C5Json::stringify(c5_player_fields(same), 0) ==
                                                    C5Json::stringify(c5_player_fields(q.self), 0),
               "C5 local create is latest-value once and false leaves its complete output intact");
        for (unsigned n = 0; n < 1024; ++n) {
            protocol::CreatePlayerFields p;
            if (!net.take_create_other_player(p)) {
                break;
            }
            q.others.push_back(std::move(p));
            expect(n < 1023, "C5 remote creates stay inside the finite drain watchdog");
        }
        for (unsigned n = 0; n < 1024; ++n) {
            std::uint32_t id = 0;
            if (!net.take_remove_other_player(id)) {
                break;
            }
            q.removed.push_back(id);
            expect(n < 1023, "C5 removals stay inside the finite drain watchdog");
        }
        for (unsigned n = 0; n < 1024; ++n) {
            client::PendingChat chat;
            if (!net.take_chat(chat)) {
                break;
            }
            q.chats.push_back(std::move(chat));
            expect(n < 1023, "C5 crossed chat queue stays inside the finite drain watchdog");
        }
        for (unsigned n = 0; n < 1024; ++n) {
            std::int32_t html = 0;
            std::uint32_t npc = 0;
            if (!net.take_send_html(html, &npc)) {
                break;
            }
            q.html.emplace_back(html, npc);
            expect(n < 1023, "C5 correlated HTML queue stays inside the finite drain watchdog");
        }
        q.before = net.take_move_before(q.correction[0], q.correction[1], q.correction[2]);
        auto correction = q.correction;
        expect(!net.take_move_before(correction[0], correction[1], correction[2]) &&
                   correction == q.correction,
               "C5 MoveBefore take is latest-once and false preserves all coordinates");
        q.teleport = net.take_teleport(
            q.destination[0], q.destination[1], q.destination[2], q.destination[3]);
        auto destination = q.destination;
        expect(!net.take_teleport(destination[0], destination[1], destination[2], destination[3]) &&
                   destination == q.destination,
               "C5 teleport take is latest-once and false preserves map and all coordinates");
        if (!snapshot) {
            return q;
        }
        C5Json record;
        record.set_type(C5Json::Type::Object);
        auto& out = record.object();
        out["checkpoint"] = c5_text(name);
        out["transport"] = c5_fields(net.connected(),
                                     net.handshook(),
                                     session.alive,
                                     session.eof,
                                     session.closes,
                                     session.sends);
        out["sent"] = c5_array();
        for (auto b : session.sent) {
            out["sent"].array().push_back(c5_number(b));
        }
        out["chunks"] = c5_array();
        for (const auto& chunk : session.chunks) {
            auto bytes = c5_array();
            for (auto b : chunk) {
                bytes.array().push_back(c5_number(b));
            }
            out["chunks"].array().push_back(std::move(bytes));
        }
        out["local"] = c5_fields(net.local_player_id(),
                                 net.server_px(),
                                 net.server_py(),
                                 net.observed_pose_ready(),
                                 net.observed_moving(),
                                 net.observed_motion().yaw());
        const auto& pose = net.observed_motion().position();
        out["observed_xyz"] = c5_fields(pose.x, pose.y, pose.z);
        out["self"] = q.have_self ? c5_player_fields(q.self) : C5Json{};
        out["others"] = c5_array();
        for (const auto& p : q.others) {
            out["others"].array().push_back(c5_player_fields(p));
        }
        out["removed"] = c5_array();
        for (auto id : q.removed) {
            out["removed"].array().push_back(c5_number(id));
        }
        out["remote_positions"] = c5_array();
        for (auto id : c5_keys(net.remote_player_positions())) {
            const auto& p = net.remote_player_positions().at(id);
            out["remote_positions"].array().push_back(
                c5_fields(id, p.x, p.y, p.z, p.moving, net.player_gstate(id)));
        }
        out["monsters"] = c5_array();
        for (auto id : c5_keys(net.monsters())) {
            const auto& m = net.monsters().at(id);
            auto item = c5_fields(m.id(),
                                  m.index(),
                                  m.race(),
                                  m.x(),
                                  m.y(),
                                  m.dir(),
                                  m.hp(),
                                  m.max_hp(),
                                  m.gstate(),
                                  m.wx(),
                                  m.wy(),
                                  m.wz(),
                                  m.tx(),
                                  m.tz(),
                                  m.yaw(),
                                  m.moving(),
                                  m.has_abs_target(),
                                  m.stop_on_arrive(),
                                  m.move_speed(),
                                  m.walk_speed(),
                                  m.run_speed(),
                                  m.running(),
                                  m.speed_value(),
                                  m.anim_speed_scale(),
                                  m.pending_attack(),
                                  m.attack_after_move(),
                                  m.pending_die(),
                                  m.pending_knee(),
                                  m.pending_behead(),
                                  m.server_removed(),
                                  m.finishing_death(),
                                  m.kneeling(),
                                  m.ice_slowed(),
                                  m.move_stopped());
            item.array().push_back(c5_text(m.name()));
            out["monsters"].array().push_back(std::move(item));
        }
        out["npcs"] = c5_array();
        for (auto id : c5_keys(net.npcs())) {
            const auto& n = net.npcs().at(id);
            out["npcs"].array().push_back(c5_fields(n.id(),
                                                    n.index(),
                                                    n.shape(),
                                                    n.x(),
                                                    n.y(),
                                                    n.z(),
                                                    n.wx(),
                                                    n.wy(),
                                                    n.wz(),
                                                    n.gstate()));
        }
        out["ground"] = c5_array();
        for (auto id : c5_keys(net.ground_items())) {
            const auto& g = net.ground_items().at(id);
            out["ground"].array().push_back(
                c5_fields(g.id, g.index, g.count, g.wx, g.wz, g.age_sec, g.origin_wx, g.origin_wz));
        }
        out["before"] = c5_fields(q.before, q.correction[0], q.correction[1], q.correction[2]);
        out["teleport"] = c5_fields(
            q.teleport, q.destination[0], q.destination[1], q.destination[2], q.destination[3]);
        out["chats"] = c5_array();
        for (const auto& c : q.chats) {
            auto item = c5_array();
            item.array().push_back(c5_text(c.speaker));
            item.array().push_back(c5_text(c.text));
            out["chats"].array().push_back(std::move(item));
        }
        out["html"] = c5_array();
        for (const auto& h : q.html) {
            out["html"].array().push_back(c5_fields(h.first, h.second));
        }
        out["quest_flags"] = c5_array();
        for (const auto& f : net.quest_flags()) {
            out["quest_flags"].array().push_back(c5_fields(f.quest, f.flag, f.clear));
        }
        out["buff_slots"] = c5_array();
        for (const auto& b : net.buff_slots()) {
            auto item = c5_fields(b.bstate,
                                  b.sys_name_id,
                                  b.source_item_index,
                                  b.is_count,
                                  b.charges,
                                  b.expire_ms,
                                  b.always_blink,
                                  b.is_mail_notification,
                                  b.is_talent_beacon);
            item.array().push_back(c5_text(b.icon_file));
            item.array().push_back(c5_text(b.effect_name));
            out["buff_slots"].array().push_back(std::move(item));
        }
        std::cout << "c5 " << C5Json::stringify(record, 0) << std::endl;
        return q;
    }

    void test_c5_player_snapshots(bool snapshot) {
        client::NetClient net;
        const auto s = attach(net);
        queue(s, c5_player(101, true, 1000, 2000, -37, 17));
        queue(s, c5_player(102, true, 1100, 2100, -47, 18));
        queue(s, c5_player(201, false, 1200, 2200, -57, 19));
        queue(s, c5_player(202, false, 1300, 2300, -67, 20));
        queue(s, c5_player(201, false, 1400, 2400, -77, 21));
        net.update(0, 0);
        auto q = c5_observe(net, *s, "player-full-latest-and-remote-order", snapshot);
        expect(q.have_self && q.others.size() == 3 && net.local_player_id() == 102 &&
                   net.server_px() == 1100 && net.server_py() == 2100,
               "C5 local creates replace while remote duplicate events retain wire order");
        c5_expect_player(q.self, 102, true, 1100, 2100, -47, 18);
        if (q.others.size() == 3) {
            c5_expect_player(q.others[0], 201, false, 1200, 2200, -57, 19);
            c5_expect_player(q.others[1], 202, false, 1300, 2300, -67, 20);
            c5_expect_player(q.others[2], 201, false, 1400, 2400, -77, 21);
        }
        expect(net.remote_player_positions().at(201).x == 1400 &&
                   net.remote_player_positions().at(201).z == -77 && net.player_gstate(201) == 21,
               "C5 remote pose and GState maps retain the last duplicate snapshot");
        queue(s, c5_player(203, false, 1500, 2500, 7, 22, false));
        net.update(0, 0);
        q = c5_observe(net, *s, "player-legacy-appearance", snapshot);
        expect(q.others.size() == 1, "C5 legacy create remains accepted without cosmetic bytes");
        if (!q.others.empty()) {
            c5_expect_player(q.others.front(), 203, false, 1500, 2500, 7, 22, false);
        }
    }
    void test_c5_cancellation_and_state(bool snapshot) {
        client::NetClient net;
        const auto s = attach(net);
        queue(s, c5_player(301, false));
        queue(s, c5_player(302, false));
        queue(s, protocol::CreatePlayer::frame_remove(301));
        queue(s, protocol::CreatePlayer::frame_remove(301));
        queue(s, protocol::CreatePlayer::frame_remove(999));
        queue(s, c5_player(301, false, 1700, 2700));
        net.update(0, 0);
        auto q = c5_observe(net, *s, "remote-create-cancels-all-own-unread-removals", snapshot);
        expect(
            q.others.size() == 2 && q.others[0].id == 302 && q.others[1].id == 301 &&
                q.removed == std::vector<std::uint32_t>{999},
            "C5 only remote recreation cancels every removal for its own ID and preserves other order");
        queue(s, protocol::CreatePlayer::frame_remove(401));
        queue(s, c5_player(401, true, 1800, 2800));
        net.update(0, 0);
        q = c5_observe(net, *s, "self-create-retains-unread-removal", snapshot);
        expect(q.have_self && q.self.id == 401 && net.local_player_id() == 401 &&
                   q.removed == std::vector<std::uint32_t>{401},
               "C5 reachable self create intentionally retains unread removal for its own ID");
        queue(s, c5_player(401, true, 1900, 2900, 7, 123));
        queue(s, protocol::CreatePlayer::frame_remove(401));
        net.update(0, 0);
        q = c5_observe(net, *s, "remove-self-retains-identity-and-unread-create", snapshot);
        expect(q.have_self && q.self.gstate == 123 && net.local_player_id() == 401 &&
                   net.player_gstate(401) == 0 && q.removed == std::vector<std::uint32_t>{401},
               "C5 removal erases player state without erasing local ID or unread self snapshot");
        queue(s, c5_player(500, false));
        queue(s, c5_npc(500));
        queue(s, c5_monster(500));
        queue(s, c5_gstate(500, 0x1234));
        net.update(0, 0);
        expect(net.monsters().at(500).gstate() == 0x1234 && net.npcs().at(500).gstate() == 0 &&
                   net.player_gstate(500) == 0,
               "C5 GState resolves colliding ID to monster before NPC and player");
        c5_observe(net, *s, "gstate-monster-precedence", snapshot);
        queue(s, c5_id(protocol::S2C::RemoveMonster, 500));
        queue(s, c5_gstate(500, 0x2345));
        net.update(0, 0);
        expect(net.monsters().count(500) == 0 && net.npcs().at(500).gstate() == 0x2345 &&
                   net.player_gstate(500) == 0,
               "C5 GState falls back to NPC after monster removal");
        c5_observe(net, *s, "gstate-npc-precedence", snapshot);
        queue(s, protocol::Quest::frame_remove_npc(500));
        queue(s, c5_gstate(500, 0x3456));
        queue(s, c5_gstate(777, 0x4567));
        net.update(0, 0);
        expect(net.npcs().count(500) == 0 && net.player_gstate(500) == 0x3456 &&
                   net.player_gstate(777) == 0x4567 &&
                   net.remote_player_positions().count(777) == 0,
               "C5 player and unknown GState need not create a remote pose");
        c5_observe(net, *s, "gstate-player-and-unknown", snapshot);
        queue(s, c5_gstate(401, protocol::kCgsFish));
        net.update(0, 0);
        expect(net.take_fishing_progress_start() && !net.take_fishing_progress_start(),
               "C5 local Fish sets one consumable progress-start signal");
        c5_observe(net, *s, "gstate-local-fish", snapshot);
        queue(s, c5_gstate(401, 0));
        net.update(0, 0);
        expect(!net.take_fishing_progress_start() && net.player_gstate(401) == 0,
               "C5 ordinary local state replacement does not manufacture a fishing-start signal");
        c5_observe(net, *s, "gstate-local-clear", snapshot);
    }
    void test_c5_monster_refresh(bool snapshot) {
        for (const auto displacement :
             std::array<std::array<int, 2>, 4>{{{{128, 0}}, {{129, 0}}, {{90, 90}}, {{91, 91}}}}) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, c5_monster(600));
            net.update(0, 0);
            queue(s,
                  c5_monster(600,
                             1000 + displacement[0],
                             2000 + displacement[1],
                             0,
                             0,
                             150,
                             350,
                             99,
                             3,
                             "Ignored replacement name"));
            net.update(0, 0);
            const auto& m = net.monsters().at(600);
            const bool far = displacement[0] == 129 || displacement[0] == 91;
            expect(
                m.index() == 77 && m.race() == 2 && m.name() == "Owned C5 monster" &&
                    m.x() == 1000 + displacement[0] && m.y() == 2000 + displacement[1] &&
                    m.hp() == 150 && m.max_hp() == 350 && m.dir() == 0 &&
                    c5_near(m.wx(), far ? 1000.f : float(1000 + displacement[0])) &&
                    c5_near(m.wz(), far ? 2000.f : float(2000 + displacement[1])) &&
                    m.moving() == far && m.has_abs_target() == far && m.stop_on_arrive() == far,
                "C5 duplicate monster retains first identity and respects the squared128px correction boundary");
            c5_observe(net,
                       *s,
                       "monster-refresh-" + std::to_string(displacement[0]) + "-" +
                           std::to_string(displacement[1]),
                       snapshot);
        }
        client::NetClient net;
        const auto s = attach(net);
        queue(s, c5_monster(601, 9000, 17000, 64));
        queue(s, c5_monster(602, 9100, 17100, 0, protocol::kCgsKnee, 100, 200, 78, 1, ""));
        net.update(0, 1);
        const auto& m = net.monsters().at(601);
        const auto& knee = net.monsters().at(602);
        expect(
            c5_near(m.wx(), 808) && c5_near(m.wz(), 8808) && m.wy() == 0 &&
                c5_near(m.yaw(), 1.5707963f) && m.move_speed() == 20 && m.walk_speed() == 20 &&
                m.run_speed() == 40 && !m.running() && !m.moving() && knee.name() == "Mob78" &&
                knee.hp() == 0 && knee.pending_knee() && !knee.moving(),
            "C5 new monsters preserve named/fallback presentation, origin and knee-on-create state");
        c5_observe(net, *s, "monster-new-origin-and-knee", snapshot);
        queue(s, c5_monster_move(601, 100, 0, false, 1));
        net.update(0, 1);
        const float old_yaw = net.monsters().at(601).yaw();
        queue(s, c5_monster(601, 9500, 17500, 0, 0, 170, 370));
        net.update(0, 1);
        expect(
            net.monsters().at(601).x() == 9500 && net.monsters().at(601).tx() == 908 &&
                net.monsters().at(601).wx() == 808 && net.monsters().at(601).moving() &&
                net.monsters().at(601).dir() == 0 && c5_near(net.monsters().at(601).yaw(), old_yaw),
            "C5 walking duplicate refresh changes authority but retains visual target and zero-direction yaw");
        c5_observe(net, *s, "monster-walking-refresh-zero-dir", snapshot);
        queue(s, c5_monster(601, 9500, 17500, 128, 0, 180, 380));
        net.update(0, 1);
        expect(net.monsters().at(601).dir() == 128 &&
                   c5_near(net.monsters().at(601).yaw(), old_yaw),
               "C5 moving postpass restores target yaw after duplicate direction at dt0");
        c5_observe(net, *s, "monster-walking-refresh-nonzero-dir", snapshot);
        queue(s, c5_monster(601, 9900, 17900, 0, protocol::kCgsKnee));
        net.update(0, 1);
        expect(
            net.monsters().at(601).hp() == 0 && net.monsters().at(601).pending_knee() &&
                !net.monsters().at(601).moving() && net.monsters().at(601).has_abs_target(),
            "C5 knee plus far correction retains target flags but postpass stops the dead actor");
        c5_observe(net, *s, "monster-knee-far-refresh-postpass", snapshot);
        queue(s, c5_gstate(601, 0));
        net.update(0, 1);
        expect(net.monsters().at(601).hp() == 0 && net.monsters().at(601).pending_knee() &&
                   net.monsters().at(601).gstate() == 0,
               "C5 clearing knee GState does not heal or consume pending knee");
        c5_observe(net, *s, "monster-knee-cleared-without-heal", snapshot);
        queue(s, c5_monster(603, 9000, 17000, 64));
        queue(s, c5_monster(603, 9000, 17000, 0));
        net.update(0, 1);
        expect(net.monsters().at(603).dir() == 0 &&
                   c5_near(net.monsters().at(603).yaw(), 1.5707963f),
               "C5 zero direction on an idle duplicate preserves the previous facing");
        c5_observe(net, *s, "monster-idle-zero-direction", snapshot);
        queue(s, c5_monster(603, 9000, 17000, 128));
        net.update(0, 1);
        expect(c5_near(net.monsters().at(603).yaw(), 3.14159265f),
               "C5 nonzero direction on an idle duplicate replaces the facing");
        c5_observe(net, *s, "monster-idle-nonzero-direction", snapshot);
    }
    void test_c5_monster_motion_and_removal(bool snapshot) {
        for (const bool end : {false, true}) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, c5_monster(610));
            queue(s, c5_monster_move(999, 1, 1, end, 1));
            queue(s, c5_monster_move(610, -128, 127, end, 2));
            net.update(0, 0);
            expect(net.monsters().size() == 1 && net.monsters().at(610).x() == 872 &&
                       net.monsters().at(610).y() == 2127 && net.monsters().at(610).wx() == 1000 &&
                       net.monsters().at(610).running() &&
                       net.monsters().at(610).move_speed() == 40 &&
                       net.monsters().at(610).stop_on_arrive() == end,
                   "C5 signed monster deltas accumulate exactly and nonzero run selects run speed");
            c5_observe(net,
                       *s,
                       end ? "monster-end-before-arrival" : "monster-on-before-arrival",
                       snapshot);
            net.update(10, 0);
            expect(net.monsters().at(610).wx() == 872 && net.monsters().at(610).wz() == 2127 &&
                       !net.monsters().at(610).has_abs_target() &&
                       net.monsters().at(610).moving() == !end,
                   "C5 fixed large dt reaches target while only MoveEnd terminates locomotion");
            c5_observe(net, *s, end ? "monster-end-arrived" : "monster-on-arrived", snapshot);
            queue(s, c5_monster_move(610, 0, 0, true, 0));
            net.update(0, 0);
            expect(!net.monsters().at(610).moving() && !net.monsters().at(610).has_abs_target() &&
                       !net.monsters().at(610).running(),
                   "C5 zero-distance MoveEnd can resolve arrival even at dt0");
            c5_observe(net, *s, end ? "monster-zero-after-end" : "monster-zero-after-on", snapshot);
        }
        for (const bool attack_first : {false, true}) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, c5_monster(611));
            const auto attack = protocol::Attack::frame_server(611, 999, 0, 0, protocol::kAtkAnim);
            if (attack_first) {
                queue(s, attack);
            }
            queue(s, c5_monster_move(611, 40, 0, false, 1));
            if (!attack_first) {
                queue(s, attack);
            }
            net.update(0, 0);
            const auto& m = net.monsters().at(611);
            expect(
                m.x() == 1040 && m.pending_attack() == attack_first &&
                    m.attack_after_move() == !attack_first && m.has_abs_target() == !attack_first &&
                    m.wx() == (attack_first ? 1040 : 1000),
                "C5 Attack-before-move snaps a latched swing while move-before-Attack defers it");
            expect(!attack_first || (!m.running() && m.move_speed() == 20 && !m.stop_on_arrive()),
                   "C5 latched attack movement returns before changing run/speed flags");
            c5_observe(net,
                       *s,
                       attack_first ? "monster-attack-before-move" : "monster-move-before-attack",
                       snapshot);
            net.update(2, 0);
            expect(net.take_monster_attack(611) && !net.take_monster_attack(611),
                   "C5 immediate or arrived deferred attack is consumed once");
            c5_observe(net,
                       *s,
                       attack_first ? "monster-immediate-attack-consumed"
                                    : "monster-deferred-attack-consumed",
                       snapshot);
        }
        for (int kind = 0; kind < 4; ++kind) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, c5_monster(612));
            if (kind == 1) {
                queue(s, protocol::Skill::frame_action_knee(612));
            } else if (kind == 2) {
                queue(s, protocol::Skill::frame_action_die(612));
            } else if (kind == 3) {
                queue(s, protocol::Skill::frame_action_beheaded(612));
            }
            net.update(0, 0);
            if (kind == 2) {
                expect(net.take_monster_die(612) && !net.take_monster_die(612),
                       "C5 die latch consumes without ending finishing-death state");
            } else if (kind == 3) {
                expect(net.take_monster_behead(612) && !net.take_monster_behead(612),
                       "C5 behead latch consumes without ending finishing-death state");
            }
            queue(s, c5_id(protocol::S2C::RemoveMonster, 612));
            queue(s, c5_id(protocol::S2C::RemoveMonster, 999));
            net.update(0, 0);
            expect(
                net.monsters().count(612) == (kind >= 2 ? 1u : 0u),
                "C5 only finishing die/behead defer removal; healthy and knee-only actors erase");
            if (kind >= 2) {
                expect(net.monsters().at(612).server_removed() &&
                           net.monsters().at(612).finishing_death(),
                       "C5 deferred removal retains explicit server-removed and finishing flags");
            }
            c5_observe(net, *s, "monster-removal-" + std::to_string(kind), snapshot);
            net.forget_monster(612);
            expect(net.monsters().empty(),
                   "C5 public renderer completion erases any retained corpse");
            c5_observe(net, *s, "monster-forgotten-" + std::to_string(kind), snapshot);
        }
    }

    void test_c5_npc_ground_and_origins(bool snapshot) {
        client::NetClient net;
        const auto s = attach(net);
        queue(s, c5_npc(701, 9000, 17000, -37, 17));
        queue(s, c5_gstate(702, 99));
        queue(s, c5_npc(702, 9100, 17100, 41, 23));
        queue(s, c5_gstate(701, 42));
        net.update(0, 1);
        const auto& n = net.npcs().at(701);
        expect(
            n.id() == 701 && n.index() == 88 && n.shape() == 7 && n.x() == 9000 && n.y() == 17000 &&
                n.z() == -37 && n.wx() == 808 && n.wz() == 8808 && c5_near(n.wy(), -3.7f) &&
                n.gstate() == 42 && net.npcs().at(702).gstate() == 23 &&
                net.player_gstate(702) == 99,
            "C5 NPC creation replaces own state while earlier unknown-player state remains distinct");
        c5_observe(net, *s, "npc-full-origin-and-wire-precedence", snapshot);
        queue(s, c5_npc(701, 9200, 17200, 70, 55, 89));
        queue(s, protocol::Quest::frame_remove_npc(702));
        queue(s, protocol::Quest::frame_remove_npc(999));
        net.update(0, 1);
        expect(
            net.npcs().size() == 1 && net.npcs().at(701).index() == 89 &&
                net.npcs().at(701).x() == 9200 && net.npcs().at(701).wy() == 7 &&
                net.npcs().at(701).gstate() == 55,
            "C5 duplicate NPC replaces every public field and unknown remove has no other effect");
        c5_observe(net, *s, "npc-replacement-and-removal", snapshot);
        queue(s, c5_item(711, 9000, 17000, 17, 31));
        net.update(0.25f, 1);
        expect(
            net.ground_items().at(711).id == 711 && net.ground_items().at(711).index == 31 &&
                net.ground_items().at(711).count == 17 && net.ground_items().at(711).wx == 808 &&
                net.ground_items().at(711).wz == 8808 &&
                net.ground_items().at(711).age_sec == .25f &&
                net.ground_items().at(711).origin_wx == 808 &&
                net.ground_items().at(711).origin_wz == 8808,
            "C5 ground item records every authored field, own fallback origin and one dt age step");
        c5_observe(net, *s, "ground-full-and-age", snapshot);
        for (const auto count : {std::uint32_t{0}, std::uint32_t{0xffffffffu}}) {
            queue(s, c5_item(711, 9100, 17100, count, 32));
            net.update(0, 1);
            expect(
                net.ground_items().at(711).index == 32 &&
                    net.ground_items().at(711).count == (count == 0 ? 0 : -1) &&
                    net.ground_items().at(711).age_sec == 0,
                "C5 replacement resets age and preserves representable zero/negative count compatibility");
            c5_observe(net, *s, "ground-count-" + std::to_string(count), snapshot);
        }
        queue(s, c5_id(protocol::S2C::RemoveItem, 711));
        queue(s, c5_id(protocol::S2C::RemoveItem, 999));
        net.update(0, 1);
        expect(net.ground_items().empty(),
               "C5 exact ground removal and unknown removal keep no phantom entry");
        c5_observe(net, *s, "ground-removal", snapshot);
        struct OriginCase {
            int player;
            int monster;
            int expected;
        };
        const std::array<OriginCase, 8> cases{{{100, 100, 0},
                                               {48, 100, 48},
                                               {49, 100, 0},
                                               {100, 32, 32},
                                               {100, 33, 0},
                                               {5, 20, 5},
                                               {20, 5, 5},
                                               {-20, 20, 20}}};
        for (std::size_t i = 0; i < cases.size(); ++i) {
            client::NetClient origin;
            const auto state = attach(origin);
            queue(state, server::frame_s2c_code(123));
            expect(origin.handshake(),
                   "C5 origin fixture uses actual public handshake before local sync");
            origin.sync_local_move(static_cast<float>(cases[i].player), 0, 0, false, 0, 0, true);
            queue(state, c5_monster(720, cases[i].monster, 0));
            queue(state, c5_item(721, 0, 0));
            origin.update(0, 0);
            expect(
                origin.ground_items().at(721).origin_wx == cases[i].expected &&
                    origin.ground_items().at(721).origin_wz == 0,
                "C5 origin uses inclusive32/48 limits, nearest source and monster-versus-player tie");
            c5_observe(origin, *state, "ground-origin-boundary-" + std::to_string(i), snapshot);
            queue(state, protocol::GameExit::frame_server_answer(true));
            origin.close();
        }
        client::NetClient order;
        const auto state = attach(order);
        queue(state, c5_item(731, 1010, 2010));
        queue(state, c5_monster(730));
        queue(state, c5_item(732, 1010, 2010));
        queue(state, c5_id(protocol::S2C::RemoveMonster, 730));
        queue(state, c5_item(733, 1010, 2010));
        order.update(0, 0);
        expect(
            order.ground_items().at(731).origin_wx == 1010 &&
                order.ground_items().at(732).origin_wx == 1000 &&
                order.ground_items().at(733).origin_wx == 1010,
            "C5 item origins capture immediate preceding entity state and never retroactively rebind");
        c5_observe(order, *state, "ground-origin-wire-order", snapshot);
    }
    void test_c5_player_motion_and_corrections(bool snapshot) {
        for (const bool observe : {false, true}) {
            client::NetClient net;
            const auto s = attach(net);
            net.set_read_only(observe);
            queue(s, c5_player(800, true, 9000, 17000, 100));
            queue(s, c5_player(801, false, 10000, 18000, 200));
            queue(s, protocol::Move::frame_player_on(801, -128, 127, -128));
            queue(s, protocol::Move::frame_player_end(801, 7, -9, 127));
            queue(s, protocol::Move::frame_player_on(999, 1, 1, 1));
            queue(s, protocol::Move::frame_player_on(800, 32, -16, 20));
            net.update(0, 1);
            const auto& r = net.remote_player_positions().at(801);
            expect(
                r.x == 9879 && r.y == 18118 && r.z == 199 && !r.moving &&
                    net.remote_player_positions().size() == 1 &&
                    net.server_px() == (observe ? 9032 : 9000) &&
                    net.server_py() == (observe ? 16984 : 17000),
                "C5 signed remote XYZ deltas accumulate while ordinary self echoes retain sent baseline");
            if (observe) {
                expect(
                    net.observed_motion().position().x == 808 &&
                        net.observed_motion().position().y == 10 && net.observed_moving(),
                    "C5 observer separates current display from fresh authoritative target at dt0");
            }
            c5_observe(
                net, *s, observe ? "players-observer-deltas" : "players-ordinary-deltas", snapshot);
            net.update(.075f, 1);
            if (observe) {
                expect(
                    net.observed_motion().position().x == 840 &&
                        c5_near(net.observed_motion().position().y, 12) &&
                        net.observed_motion().position().z == 8792,
                    "C5 fixed observer dt reaches authored fullXYZ target without a formula clone");
            }
            c5_observe(net,
                       *s,
                       observe ? "players-observer-arrived" : "players-ordinary-unchanged",
                       snapshot);
            queue(s, protocol::CreatePlayer::frame_remove(801));
            queue(s, protocol::Move::frame_player_end(801, 1, 2, 3));
            queue(s, protocol::Move::frame_player_end(800, 0, 0, 0));
            net.update(.075f, 1);
            expect(
                net.remote_player_positions().empty() && (!observe || !net.observed_moving()),
                "C5 removed remote deltas stay ignored while explicit observer stop reaches idle");
            c5_observe(
                net, *s, observe ? "players-observer-stop" : "players-ordinary-removed", snapshot);
        }
        client::NetClient net;
        const auto s = attach(net);
        net.set_read_only(true);
        queue(s, protocol::Move::frame_before(810, 9000, 17000, -40, 55));
        net.update(0, 1);
        auto q = c5_observe(net, *s, "before-establishes-local-without-create", snapshot);
        expect(net.local_player_id() == 810 && net.server_px() == 9000 && q.before &&
                   q.correction == std::array<int, 3>{{9000, 17000, -40}} &&
                   net.observed_motion().position().x == 808 &&
                   net.observed_motion().position().y == -4,
               "C5 first unknown MoveBefore establishes local identity and full observer pose");
        queue(s, protocol::Move::frame_before(999, 1, 2, 3, 77));
        queue(s, protocol::Move::frame_before(810, 9100, 17100, -50, 66));
        queue(s, protocol::Move::frame_before(810, 9200, 17200, -60, 99));
        net.update(0, 1);
        q = c5_observe(net, *s, "before-latest-local-ignores-unrelated", snapshot);
        expect(
            q.before && q.correction == std::array<int, 3>{{9200, 17200, -60}} &&
                net.local_player_id() == 810 && !net.observed_moving(),
            "C5 local corrections coalesce and reset interpolation while unrelated IDs are ignored");
        queue(s, c5_player(820, false, 1, 2, 3));
        queue(s, c5_player(820, true, 9000, 17000, 4));
        queue(s, protocol::Move::frame_before(820, 77, 88, 99, 11));
        net.update(0, 1);
        q = c5_observe(net, *s, "before-existing-remote-precedes-local", snapshot);
        expect(q.have_self && !q.before && net.local_player_id() == 820 &&
                   net.server_px() == 9000 && net.remote_player_positions().at(820).x == 77 &&
                   net.remote_player_positions().at(820).z == 99,
               "C5 existing remote entry wins correction even after same ID becomes local");
    }
    void test_c5_teleport_retention(bool snapshot) {
        client::NetClient net;
        const auto s = attach(net);
        queue(s, server::frame_s2c_code(321));
        expect(
            net.handshake() && net.send_ask_npc(900),
            "C5 teleport fixture establishes real successful NPC correlation through public requests");
        queue(s, c5_player(9000, true, 1000, 2000, 30, 123));
        queue(s, c5_player(9001, false));
        queue(s, protocol::CreatePlayer::frame_remove(9002));
        queue(s, c5_monster(9010));
        queue(s, c5_npc(9020));
        queue(s, c5_item(9030));
        queue(s, protocol::Quest::frame_send_html(91));
        queue(s, protocol::Quest::frame_save_quest({6201, 2, 1}));
        queue(s, protocol::Skill::frame_skill_info({{3, 2}, {4, 5}}));
        queue(s, protocol::MState::frame(9000, protocol::kCmsHp | protocol::kCmsMp));
        queue(s, protocol::Teleport::frame_server(7, 3000, 4000, -50, 9));
        queue(s, protocol::Quest::frame_send_html(92));
        net.update(0, 0);
        expect(
            net.monsters().empty() && net.npcs().empty() && net.ground_items().empty() &&
                net.remote_player_positions().count(9001) == 1 && net.player_gstate(9000) == 123 &&
                net.quest_flags().size() == 1 && net.quest_flags()[0].quest == 6201 &&
                net.buff_slots().size() == 2 && net.server_px() == 1000 && net.server_py() == 2000,
            "C5 ordinary teleport clears entity/HTML views but retains remote/gstate/quest/buff and staged baseline");
        const auto q = c5_observe(net, *s, "teleport-exact-clear-retain-set", snapshot);
        expect(
            q.have_self && q.self.id == 9000 && q.others.size() == 1 && q.others[0].id == 9001 &&
                q.removed == std::vector<std::uint32_t>{9002} &&
                q.html == std::vector<std::pair<std::int32_t, std::uint32_t>>{{92, 0}} &&
                q.teleport && q.destination == std::array<int, 4>{{7, 3000, 4000, -50}},
            "C5 teleport retains player events but clears old dialog and last-asked fallback before later legacy HTML");
        std::unordered_map<int, int> ranks;
        expect(net.take_skill_info(ranks) && ranks == std::unordered_map<int, int>{{3, 2}, {4, 5}},
               "C5 teleport retains the independent pending skill snapshot");
        const Bytes before = s->sent;
        expect(net.server_px() == 1000 && net.send_teleport_ack(3000, 4000, -50) &&
                   net.server_px() == 3000 && net.server_py() == 4000,
               "C5 taking teleport does not seed baseline; successful public ACK does");
        auto expected = before;
        append(expected, protocol::Teleport::frame_client_ack(1, -50));
        expect(s->sent == expected, "C5 teleport ACK sends its exact ordinary wire bytes");
        c5_observe(net, *s, "teleport-ack-seeds-baseline", snapshot);
        queue(s, c5_monster(9040));
        queue(s, protocol::Teleport::frame_server(8, 5000, 6000, 70, 0));
        queue(s, c5_monster(9041));
        queue(s, protocol::Teleport::frame_server(9, 7000, 8000, 80, 0));
        queue(s, c5_monster(9042));
        queue(s, protocol::Chatting::frame_s2c("C5", "after teleport"));
        net.update(0, 0);
        const auto latest =
            c5_observe(net, *s, "teleport-batch-latest-and-neighbor-order", snapshot);
        expect(
            net.monsters().size() == 1 && net.monsters().count(9042) == 1 && latest.teleport &&
                latest.destination == std::array<int, 4>{{9, 7000, 8000, 80}} &&
                latest.chats.size() == 1 && latest.chats[0].text == "after teleport",
            "C5 each teleport immediately clears earlier entities and only the latest destination is unread");
        queue(s, protocol::GameExit::frame_server_answer(true));
        net.close();
        client::NetClient observer;
        const auto state = attach(observer);
        observer.set_read_only(true);
        queue(state, c5_player(9100, true, 9000, 17000, 50));
        queue(state, protocol::Move::frame_player_on(9100, 32, 0, 10));
        queue(state, protocol::Teleport::frame_server(10, 10000, 18000, -70, 3));
        observer.update(0, 1);
        expect(observer.server_px() == 10000 && observer.server_py() == 18000 &&
                   observer.observed_motion().position().x == 1808 &&
                   observer.observed_motion().position().z == 9808 &&
                   observer.observed_motion().position().y == -7 && !observer.observed_moving(),
               "C5 read-only teleport immediately resets baseline and the full observed pose");
        c5_observe(observer, *state, "teleport-observer-immediate-reset", snapshot);
    }

    void c5_seed_boundary(const std::shared_ptr<SessionState>& s) {
        queue(s, c5_player(101, true, 1000, 2000, 30));
        queue(s, c5_player(202, false, 2000, 3000, 40));
        queue(s, c5_monster(303));
        queue(s, c5_npc(404));
        queue(s, c5_item(505));
    }
    void c5_expect_ignored_boundary(client::NetClient& net,
                                    const C5Observation& q,
                                    const SessionState& s) {
        expect(
            net.connected() && s.closes == 0 && s.chunks.empty() && net.local_player_id() == 101 &&
                net.server_px() == 1000 && net.server_py() == 2000 && q.have_self &&
                q.self.id == 101 && q.others.size() == 1 && q.others[0].id == 202 &&
                q.removed.empty() && !q.before && !q.teleport &&
                net.remote_player_positions().size() == 1 &&
                net.remote_player_positions().at(202).x == 2000 &&
                net.remote_player_positions().at(202).y == 3000 &&
                net.remote_player_positions().at(202).z == 40 && net.monsters().size() == 1 &&
                net.monsters().at(303).x() == 1000 && net.monsters().at(303).y() == 2000 &&
                net.monsters().at(303).hp() == 200 && net.monsters().at(303).gstate() == 0 &&
                net.npcs().size() == 1 && net.npcs().at(404).x() == 1000 &&
                net.ground_items().size() == 1 && net.ground_items().at(505).wx == 1000 &&
                q.chats.size() == 2 && q.chats[0].text == "before malformed world" &&
                q.chats[1].text == "after malformed world",
            "C5 malformed world payload leaves typed seeded state/events intact and drains both neighbors");
    }
    void test_c5_payload_boundaries(bool snapshot) {
        struct Boundary {
            const char* name;
            protocol::S2C type;
            Bytes valid;
        };
        const std::array<Boundary, 15> cases{
            {{"player",
              protocol::S2C::CreatePlayer,
              c5_player(999, true, 1111, 2222, 33, 0, false)},
             {"remove-player",
              protocol::S2C::RemovePlayer,
              protocol::CreatePlayer::frame_remove(202)},
             {"gstate", protocol::S2C::GState, c5_gstate(202, 11)},
             {"monster", protocol::S2C::CreateMonster, c5_monster(303, 1111, 2222, 0, 0, 77)},
             {"item", protocol::S2C::CreateItem, c5_item(505, 1111, 2222, 27)},
             {"remove-item", protocol::S2C::RemoveItem, c5_id(protocol::S2C::RemoveItem, 505)},
             {"npc", protocol::S2C::CreateNpc, c5_npc(404, 1111, 2222, 33, 11)},
             {"remove-npc", protocol::S2C::RemoveNpc, protocol::Quest::frame_remove_npc(404)},
             {"monster-on", protocol::S2C::MoveMonsterOn, c5_monster_move(303, 7, -9, false, 1)},
             {"monster-end", protocol::S2C::MoveMonsterEnd, c5_monster_move(303, 7, -9, true, 1)},
             {"remove-monster",
              protocol::S2C::RemoveMonster,
              c5_id(protocol::S2C::RemoveMonster, 303)},
             {"player-on",
              protocol::S2C::MovePlayerOn,
              protocol::Move::frame_player_on(202, 7, -9, 4)},
             {"player-end",
              protocol::S2C::MovePlayerEnd,
              protocol::Move::frame_player_end(202, 7, -9, 4)},
             {"before",
              protocol::S2C::MoveBefore,
              protocol::Move::frame_before(101, 1111, 2222, 33, 9)},
             {"teleport",
              protocol::S2C::Teleport,
              protocol::Teleport::frame_server(3, 1111, 2222, 33, 1)}}};
        for (std::size_t which = 0; which < cases.size(); ++which) {
            const auto& test = cases[which];
            const Bytes complete = c5_payload(test.valid);
            for (std::size_t size = 0; size < complete.size(); ++size) {
                client::NetClient net;
                const auto s = attach(net);
                c5_seed_boundary(s);
                queue(s, protocol::Chatting::frame_s2c("C5", "before malformed world"));
                queue(s,
                      c5_frame(test.type,
                               Bytes(complete.begin(),
                                     complete.begin() + static_cast<std::ptrdiff_t>(size))));
                queue(s, protocol::Chatting::frame_s2c("C5", "after malformed world"));
                net.update(0, 0);
                const auto q =
                    c5_observe(net,
                               *s,
                               "short-" + std::string(test.name) + "-" + std::to_string(size),
                               snapshot);
                c5_expect_ignored_boundary(net, q, *s);
            }
            client::NetClient net;
            const auto s = attach(net);
            c5_seed_boundary(s);
            auto trailing = complete;
            trailing.push_back(0xab);
            queue(s, c5_frame(test.type, std::move(trailing)));
            queue(s, protocol::Chatting::frame_s2c("C5", "after trailing world"));
            net.update(0, 0);
            const auto q = c5_observe(net, *s, "trailing-" + std::string(test.name), snapshot);
            expect(net.connected() && s->closes == 0 && q.chats.size() == 1 &&
                       q.chats[0].text == "after trailing world",
                   "C5 all trailing compatibility neighbors drain without disconnecting");
            switch (which) {
            case 0:
                expect(net.local_player_id() == 101 && q.have_self && q.self.id == 101,
                       "C5 arbitrary CreatePlayer trailing bytes are rejected");
                break;
            case 1:
                expect(net.remote_player_positions().empty() && q.others.empty() &&
                           q.removed == std::vector<std::uint32_t>{202},
                       "C5 removal accepts its existing trailing payload policy");
                break;
            case 2:
                expect(net.player_gstate(202) == 11, "C5 GState accepts trailing bytes");
                break;
            case 3:
                expect(net.monsters().at(303).x() == 1111 && net.monsters().at(303).hp() == 77,
                       "C5 monster snapshot accepts trailing bytes");
                break;
            case 4:
                expect(net.ground_items().at(505).wx == 1111 &&
                           net.ground_items().at(505).count == 27,
                       "C5 item snapshot accepts trailing bytes");
                break;
            case 5:
                expect(net.ground_items().empty(), "C5 ground removal accepts trailing bytes");
                break;
            case 6:
                expect(net.npcs().at(404).x() == 1111 && net.npcs().at(404).gstate() == 11,
                       "C5 NPC snapshot accepts trailing bytes");
                break;
            case 7:
                expect(net.npcs().empty(), "C5 NPC removal accepts trailing bytes");
                break;
            case 8:
            case 9:
                expect(net.monsters().at(303).x() == 1007 && net.monsters().at(303).y() == 1991,
                       "C5 both monster delta opcodes accept trailing bytes");
                break;
            case 10:
                expect(net.monsters().empty(), "C5 monster removal accepts trailing bytes");
                break;
            case 11:
            case 12:
                expect(net.remote_player_positions().at(202).x == 2007 &&
                           net.remote_player_positions().at(202).y == 2991 &&
                           net.remote_player_positions().at(202).z == 44,
                       "C5 both player delta opcodes accept trailing bytes");
                break;
            case 13:
                expect(q.before && q.correction == std::array<int, 3>{{1111, 2222, 33}},
                       "C5 MoveBefore accepts trailing bytes");
                break;
            case 14:
                expect(q.teleport && q.destination == std::array<int, 4>{{3, 1111, 2222, 33}} &&
                           net.monsters().empty(),
                       "C5 Teleport accepts trailing bytes and still clears old world entities");
                break;
            }
        }
        const auto legacy = c5_payload(c5_player(999, true, 1111, 2222, 33, 0, false));
        const auto extended = c5_payload(c5_player(999, true, 1111, 2222, 33));
        for (int kind = 0; kind < 3; ++kind) {
            auto body = extended;
            if (kind == 0) {
                body.pop_back();
            } else if (kind == 1) {
                body[legacy.size()] = 2;
            } else {
                body.push_back(0);
            }
            client::NetClient net;
            const auto s = attach(net);
            c5_seed_boundary(s);
            queue(s, protocol::Chatting::frame_s2c("C5", "before malformed world"));
            queue(s, c5_frame(protocol::S2C::CreatePlayer, std::move(body)));
            queue(s, protocol::Chatting::frame_s2c("C5", "after malformed world"));
            net.update(0, 0);
            const auto q =
                c5_observe(net, *s, "player-invalid-cosmetics-" + std::to_string(kind), snapshot);
            c5_expect_ignored_boundary(net, q, *s);
        }
    }
    void test_c5_fragmentation_and_instances(bool snapshot) {
        const std::array<Bytes, 3> frames{{c5_player(1001, false),
                                           c5_monster(1002),
                                           protocol::Teleport::frame_server(7, 3000, 4000, 50, 0)}};
        for (std::size_t kind = 0; kind < frames.size(); ++kind) {
            const auto& frame = frames[kind];
            const std::array<std::size_t, 3> splits{{1, 3, frame.size() - 1}};
            for (auto split : splits) {
                client::NetClient net;
                const auto s = attach(net);
                queue(s, protocol::Chatting::frame_s2c("C5", "preceding complete"));
                s->chunks.front().insert(s->chunks.front().end(),
                                         frame.begin(),
                                         frame.begin() + static_cast<std::ptrdiff_t>(split));
                net.update(0, 0);
                const auto first = c5_observe(net,
                                              *s,
                                              "fragment-first-" + std::to_string(kind) + "-" +
                                                  std::to_string(split),
                                              snapshot);
                expect(
                    first.chats.size() == 1 && first.chats[0].text == "preceding complete" &&
                        first.others.empty() && net.monsters().empty() && !first.teleport &&
                        net.connected(),
                    "C5 incomplete representative world frame retains its tail after earlier event is delivered once");
                Bytes suffix(frame.begin() + static_cast<std::ptrdiff_t>(split), frame.end());
                append(suffix, protocol::Chatting::frame_s2c("C5", "following complete"));
                s->chunks.push_back(std::move(suffix));
                net.update(0, 0);
                const auto second = c5_observe(net,
                                               *s,
                                               "fragment-second-" + std::to_string(kind) + "-" +
                                                   std::to_string(split),
                                               snapshot);
                expect(
                    second.chats.size() == 1 && second.chats[0].text == "following complete" &&
                        ((kind == 0 && second.others.size() == 1 && second.others[0].id == 1001) ||
                         (kind == 1 && net.monsters().count(1002) == 1) ||
                         (kind == 2 && second.teleport &&
                          second.destination == std::array<int, 4>{{7, 3000, 4000, 50}})),
                    "C5 completing one retained frame delivers it once and continues through the typed suffix");
            }
        }
        client::NetClient a, b;
        const auto sa = attach(a), sb = attach(b);
        a.set_read_only(true);
        queue(sa, c5_player(1100, true, 9000, 17000, 50));
        queue(sb, c5_player(1200, true, 1000, 2000, -50));
        queue(sa, c5_monster(1101, 9100, 17100));
        queue(sb, c5_npc(1201, 1100, 2100));
        a.update(0, 1);
        b.update(0, 0);
        queue(sa, protocol::Teleport::frame_server(4, 10000, 18000, 100, 0));
        a.update(0, 1);
        expect(
            a.local_player_id() == 1100 && b.local_player_id() == 1200 && a.monsters().empty() &&
                b.npcs().count(1201) == 1 && a.server_px() == 10000 && b.server_px() == 1000 &&
                a.observed_motion().position().x == 1808,
            "C5 interleaved instances have independent identities, origins, observer and world state");
        c5_observe(a, *sa, "instances-A-after-teleport", snapshot);
        c5_observe(b, *sb, "instances-B-retained", snapshot);
        client::NetClient handshake;
        const auto sh = attach(handshake);
        queue(sh, c5_npc(1301, 262244, 262344, 30));
        queue(sh, server::frame_s2c_code(555));
        expect(handshake.handshake() && handshake.npcs().at(1301).wx() == 100 &&
                   handshake.npcs().at(1301).wz() == 200,
               "C5 real handshake uses unchanged origin32 frame drain before Code");
        c5_observe(handshake, *sh, "handshake-world-before-code", snapshot);
        queue(sh, protocol::GameExit::frame_server_answer(true));
        handshake.close();
        client::NetClient closing;
        const auto sc = attach(closing);
        queue(sc, server::frame_s2c_code(556));
        expect(closing.handshake(), "C5 throwing-close witness establishes ordinary handshake");
        queue(sc, c5_player(1401, false));
        queue(sc, protocol::GameExit::frame_server_answer(true));
        sc->throw_close = 1;
        bool threw = false;
        try {
            closing.close();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        expect(threw && closing.remote_player_positions().count(1401) == 1,
               "C5 close drains world packet before transport throw interrupts ordinary teardown");
        const auto q = c5_observe(closing, *sc, "close-world-before-throwing-teardown", snapshot);
        expect(q.others.size() == 1 && q.others[0].id == 1401,
               "C5 pending remote create remains observable at the real close exception boundary");
        sc->throw_close = 0;
        queue(sc, protocol::GameExit::frame_server_answer(true));
        closing.close();
    }
    // C5_REACHABLE_GUARDS_BEGIN
    void test_c5_gstate_knee_and_public_input(bool snapshot) {
        {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, c5_monster(1501));
            queue(s, c5_monster(1502, 2000, 3000));
            queue(s, c5_monster_move(1501, 40, 0, false, 1));
            queue(s, c5_gstate(1501, protocol::kCgsKnee));
            net.update(0, 0);
            const auto& m = net.monsters().at(1501);
            expect(
                m.hp() == 0 && m.gstate() == protocol::kCgsKnee && m.pending_knee() &&
                    !m.moving() && m.has_abs_target() && m.x() == 1040 && m.wx() == 1000 &&
                    m.tx() == 1040,
                "C5 actual monster GState knee stops moving HP0 actor while retaining its prior target");
            c5_observe(net, *s, "gstate-monster-knee-stops-and-latches", snapshot);
            expect(
                net.take_monster_knee(1501) && !net.take_monster_knee(1501) &&
                    !net.take_monster_knee(1502) && !net.take_monster_knee(999999) &&
                    net.monsters().at(1501).hp() == 0 && net.monsters().at(1501).kneeling(),
                "C5 public knee take consumes only its own latch and ignores healthy or missing actor IDs");
            c5_observe(net, *s, "gstate-monster-knee-consumed", snapshot);
        }
        {
            client::NetClient net;
            net.sync_local_move(10, 2, 20, true, .1f, 1, false);
            expect(!net.send_teleport_ack(1, 2, 3) && net.server_px() == 0 &&
                       net.server_py() == 0 && !net.observed_pose_ready(),
                   "C5 disconnected input and ACK cannot establish a movement baseline");
            const auto s = attach(net);
            net.sync_local_move(10, 2, 20, true, .1f, 1, false);
            expect(
                !net.send_teleport_ack(1, 2, 3) && s->sends == 0 && s->sent.empty() &&
                    net.server_px() == 0 && !net.observed_pose_ready(),
                "C5 connected but unhandshook input and ACK retain zero baseline without traffic");
            c5_observe(net, *s, "local-input-before-handshake", snapshot);
            queue(s, server::frame_s2c_code(765));
            expect(net.handshake(),
                   "C5 first-input fixture completes its ordinary protocol handshake");
            Bytes expected = protocol::Handshake::build_connect(0);
            append(expected, protocol::Handshake::build_answer(765));
            net.sync_local_move(10, 2, 20, false, 0, 1, false);
            expect(
                net.observed_pose_ready() && net.server_px() == 8202 && net.server_py() == 8212 &&
                    s->sent == expected && s->sends == 2,
                "C5 first public input converts origin1 coordinates and seeds baseline without a move packet");
            c5_observe(net, *s, "local-input-first-baseline-without-traffic", snapshot);
            net.sync_local_move(42, 2.5f, 20, true, .02f, 1, false);
            expect(
                s->sent == expected && s->sends == 2 && net.server_px() == 8202 &&
                    net.server_py() == 8212,
                "C5 subinterval movement input cannot prematurely advance authoritative baseline or write traffic");
            c5_observe(net, *s, "local-input-throttled-before-send", snapshot);
            net.sync_local_move(42, 2.5f, 20, true, .04f, 1, false);
            append(expected, protocol::Move::frame_client_on(32, 0, 5));
            expect(
                s->sent == expected && s->sends == 3 && net.server_px() == 8234 &&
                    net.server_py() == 8212,
                "C5 elapsed input sends the exact one-small-step XYZ delta and advances only the sent baseline");
            c5_observe(net, *s, "local-input-exact-move-on", snapshot);
            net.sync_local_move(43.75f, 3.f, 22.75f, false, 0, 1, false);
            append(expected, protocol::Move::frame_client_end(1, 2, 5));
            net.sync_local_move(43.75f, 3.f, 22.75f, false, 0, 1, false);
            expect(
                s->sent == expected && s->sends == 4 && net.server_px() == 8235 &&
                    net.server_py() == 8214,
                "C5 stop input sends exact truncated-coordinate MoveEnd once and identical idle input stays silent");
            c5_observe(net, *s, "local-input-stops-and-idle-noop", snapshot);
            queue(s, protocol::GameExit::frame_server_answer(true));
            net.close();
        }
    }
    void test_c5_empty_first_create(bool snapshot) {
        client::NetClient net;
        const auto s = attach(net);
        queue(s, c5_frame(protocol::S2C::CreatePlayer, {}));
        queue(s, protocol::Chatting::frame_s2c("C5", "after empty first create"));
        net.update(0, 0);
        const auto q = c5_observe(net, *s, "player-empty-first-frame", snapshot);
        expect(
            net.connected() && s->closes == 0 && !q.have_self && q.others.empty() &&
                net.local_player_id() == 0 && net.remote_player_positions().empty() &&
                q.chats.size() == 1 && q.chats[0].text == "after empty first create",
            "C5 empty initial create is ignored and the following complete frame still drains once");
    }
    // C5_REACHABLE_GUARDS_END

    void test_c5_world_packets(bool snapshot) {
        test_c5_player_snapshots(snapshot);
        test_c5_cancellation_and_state(snapshot);
        test_c5_monster_refresh(snapshot);
        test_c5_monster_motion_and_removal(snapshot);
        test_c5_npc_ground_and_origins(snapshot);
        test_c5_player_motion_and_corrections(snapshot);
        test_c5_teleport_retention(snapshot);
        test_c5_payload_boundaries(snapshot);
        test_c5_fragmentation_and_instances(snapshot);
        test_c5_gstate_knee_and_public_input(snapshot);
        test_c5_empty_first_create(snapshot);
    }
    // C5_WORLD_PACKET_BASELINE_END

    // C6_STATUS_PACKET_BASELINE_BEGIN
    using C6Json = platform::Json;
    C6Json c6_text(std::string text) {
        C6Json out;
        out.set_type(C6Json::Type::String);
        out.string() = std::move(text);
        return out;
    }
    C6Json c6_array() {
        C6Json out;
        out.set_type(C6Json::Type::Array);
        return out;
    }
    template <typename... Values> C6Json c6_fields(Values... values) {
        auto out = c6_array();
        (out.array().push_back(c6_text(std::to_string(values))), ...);
        return out;
    }
    Bytes c6_frame(protocol::S2C type, Bytes payload) {
        protocol::PacketWriter w;
        w.payload() = std::move(payload);
        return w.frame(static_cast<std::uint8_t>(type));
    }
    Bytes c6_body(const Bytes& frame) {
        expect(frame.size() >= 3, "C6 owned frame has the real three-byte header");
        return frame.size() >= 3 ? Bytes(frame.begin() + 3, frame.end()) : Bytes{};
    }
    Bytes c6_message(std::uint8_t message, bool variable = false, Bytes tail = Bytes{}) {
        Bytes body{message};
        append(body, tail);
        return c6_frame(variable ? protocol::S2C::MessageV : protocol::S2C::Message,
                        std::move(body));
    }
    std::uint64_t c6_now_ms() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count());
    }
    struct C6Bracket {
        std::uint64_t lo = 0;
        std::uint64_t hi = 0;
    };
    C6Bracket c6_update(client::NetClient& net) {
        const auto lo = c6_now_ms();
        net.update(0, 0);
        return {lo, c6_now_ms()};
    }
    struct C6Deadline {
        std::uint64_t bit = 0;
        bool beacon = false;
        C6Bracket bracket;
        std::uint64_t duration = 0;
        std::uint64_t retained = 0;
    };
    struct C6Timing {
        std::vector<C6Deadline> slots;
        std::array<std::optional<C6Deadline>, 4> registers{};
    };
    C6Timing
    c6_bracket_slots(const client::NetClient& net, C6Bracket bracket, std::uint64_t duration) {
        C6Timing out;
        for (const auto& slot : net.buff_slots()) {
            if (!slot.is_count && !slot.always_blink) {
                out.slots.push_back({slot.bstate, slot.is_talent_beacon, bracket, duration, 0});
            }
        }
        return out;
    }
    C6Timing c6_retain_times(const client::NetClient& net) {
        C6Timing out;
        for (const auto& slot : net.buff_slots()) {
            if (slot.expire_ms > 0) {
                out.slots.push_back({slot.bstate, slot.is_talent_beacon, {}, 0, slot.expire_ms});
            }
        }
        const auto& registers = net.inventory_register_expire_ms();
        for (std::size_t i = 0; i < registers.size(); ++i) {
            if (registers[i] > 0) {
                out.registers[i] = C6Deadline{0, false, {}, 0, registers[i]};
            }
        }
        return out;
    }
    C6Json c6_deadline(std::uint64_t observed, const C6Deadline* rule) {
        if (observed == 0) {
            expect(rule == nullptr || (rule->duration == 0 && rule->retained == 0),
                   "C6 explicitly expected timed/retained deadline cannot disappear into zero");
            return c6_text("zero");
        }
        expect(rule != nullptr,
               "C6 positive deadline has an explicit independent bracket or retained-value rule");
        if (!rule) {
            return c6_text("unvalidated");
        }
        if (rule->retained > 0) {
            const bool same = observed == rule->retained;
            expect(same, "C6 unaffected deadline retains its exact previously observed value");
            auto out = c6_fields(same);
            out.array().push_back(c6_text("retained-exact"));
            return out;
        }
        const bool inside = observed >= rule->bracket.lo + rule->duration &&
                            observed <= rule->bracket.hi + rule->duration;
        expect(
            inside,
            "C6 timed deadline lies within its real receive-clock bracket plus authored duration");
        auto out = c6_fields(inside, rule->duration);
        out.array().push_back(c6_text("validated-bracket"));
        return out;
    }
    bool c6_same_talent(const protocol::TalentState& a, const protocol::TalentState& b) {
        return std::tie(a.mask, a.level, a.beacon_cooldown_ms, a.beacon_active_ms, a.status) ==
               std::tie(b.mask, b.level, b.beacon_cooldown_ms, b.beacon_active_ms, b.status);
    }
    struct C6Observation {
        bool talent_pending = false;
        protocol::TalentState talent;
        bool chest_start = false;
        std::uint8_t chest_seconds = 231;
        bool chest_stop = false;
        bool enchant = false;
        std::vector<client::PendingInfoMessage> infos;
        std::vector<client::PendingChat> chats;
    };
    C6Observation c6_observe(client::NetClient& net,
                             const SessionState& session,
                             const std::string& name,
                             bool snapshot,
                             const C6Timing& timing = C6Timing{}) {
        C6Observation q;
        q.talent = {123, 99, 17, 19, protocol::TalentStatus::Stale};
        q.talent_pending = net.take_talent_state(q.talent);
        auto same = q.talent;
        expect(!net.take_talent_state(same) && c6_same_talent(same, q.talent),
               "C6 talent dirty take is once and false preserves all five caller fields");
        q.chest_start = net.take_chest_open_start(q.chest_seconds);
        auto seconds = q.chest_seconds;
        expect(!net.take_chest_open_start(seconds) && seconds == q.chest_seconds,
               "C6 chest start take is once and false preserves duration");
        q.chest_stop = net.take_chest_open_stop();
        expect(!net.take_chest_open_stop(), "C6 chest stop is an independent one-shot");
        q.enchant = net.take_enchant_failed();
        expect(!net.take_enchant_failed(), "C6 enchant latch is consumed independently once");
        for (unsigned i = 0; i < 1024; ++i) {
            client::PendingInfoMessage info;
            if (!net.take_info_message(info)) {
                break;
            }
            q.infos.push_back(std::move(info));
            expect(i < 1023, "C6 authored info queue exhausts within the finite drain watchdog");
        }
        for (unsigned i = 0; i < 1024; ++i) {
            client::PendingChat chat;
            if (!net.take_chat(chat)) {
                break;
            }
            q.chats.push_back(std::move(chat));
            expect(i < 1023, "C6 authored marker queue exhausts within the finite drain watchdog");
        }
        auto slots = c6_array();
        for (const auto& slot : net.buff_slots()) {
            auto item = c6_fields(slot.bstate,
                                  slot.sys_name_id,
                                  slot.source_item_index,
                                  slot.is_count,
                                  slot.charges,
                                  slot.always_blink,
                                  slot.is_mail_notification,
                                  slot.is_talent_beacon);
            item.array().push_back(c6_text(slot.icon_file));
            item.array().push_back(c6_text(slot.effect_name));
            const auto found =
                std::find_if(timing.slots.begin(), timing.slots.end(), [&](const C6Deadline& d) {
                    return d.bit == slot.bstate && d.beacon == slot.is_talent_beacon;
                });
            item.array().push_back(
                c6_deadline(slot.expire_ms, found == timing.slots.end() ? nullptr : &*found));
            slots.array().push_back(std::move(item));
        }
        auto registers = c6_array();
        for (std::size_t i = 0; i < 4; ++i) {
            registers.array().push_back(
                c6_deadline(net.inventory_register_expire_ms()[i],
                            timing.registers[i] ? &*timing.registers[i] : nullptr));
        }
        if (!snapshot) {
            return q;
        }
        C6Json record;
        record.set_type(C6Json::Type::Object);
        auto& out = record.object();
        out["checkpoint"] = c6_text(name);
        out["transport"] =
            c6_fields(net.connected(), session.alive, session.eof, session.closes, session.sends);
        out["slots"] = std::move(slots);
        out["registers"] = std::move(registers);
        const auto stats = net.char_info_stats();
        out["specialty_speed_chest"] = c6_fields(net.specialty(),
                                                 stats.specialty_index,
                                                 net.local_mspeed(),
                                                 net.last_chest_feedback_msg());
        out["gov_pos"] = c6_text(stats.gov_pos);
        out["talent"] = c6_fields(q.talent_pending,
                                  q.talent.mask,
                                  q.talent.level,
                                  q.talent.beacon_cooldown_ms,
                                  q.talent.beacon_active_ms,
                                  static_cast<unsigned>(q.talent.status));
        out["latches"] = c6_fields(q.chest_start, q.chest_seconds, q.chest_stop, q.enchant);
        out["consuming"] = c6_fields(net.entity_consuming(0),
                                     net.entity_consuming(41),
                                     net.entity_consuming(51),
                                     net.entity_consuming(999));
        out["monsters"] = c6_array();
        std::vector<std::uint32_t> ids;
        for (const auto& entry : net.monsters()) {
            ids.push_back(entry.first);
        }
        std::sort(ids.begin(), ids.end());
        for (auto id : ids) {
            const auto& m = net.monsters().at(id);
            auto item =
                c6_fields(id, m.speed_value(), m.ice_slowed(), m.move_stopped(), m.running());
            item.array().push_back(c6_text(std::to_string(m.move_speed())));
            item.array().push_back(c6_text(std::to_string(m.anim_speed_scale())));
            out["monsters"].array().push_back(std::move(item));
        }
        out["infos"] = c6_array();
        for (const auto& info : q.infos) {
            auto item = c6_array();
            item.array().push_back(c6_text(info.text));
            item.array().push_back(info.kind ? c6_fields(static_cast<unsigned>(*info.kind))
                                             : C6Json{});
            if (info.color) {
                auto color = c6_array();
                for (auto channel : *info.color) {
                    color.array().push_back(c6_text(std::to_string(channel)));
                }
                item.array().push_back(std::move(color));
            } else {
                item.array().push_back(C6Json{});
            }
            out["infos"].array().push_back(std::move(item));
        }
        out["chats"] = c6_array();
        for (const auto& chat : q.chats) {
            auto item = c6_array();
            item.array().push_back(c6_text(chat.speaker));
            item.array().push_back(c6_text(chat.text));
            out["chats"].array().push_back(std::move(item));
        }
        std::cout << "c6 " << C6Json::stringify(record, 0) << std::endl;
        return q;
    }
    const client::BuffSlotEntry*
    c6_slot(const client::NetClient& net, std::uint64_t bit, bool beacon = false) {
        const auto& slots = net.buff_slots();
        const auto at = std::find_if(slots.begin(), slots.end(), [&](const auto& s) {
            return s.bstate == bit && s.is_talent_beacon == beacon;
        });
        return at == slots.end() ? nullptr : &*at;
    }
    void c6_expect_slot(const client::BuffSlotEntry* slot,
                        const std::string& icon,
                        const std::string& name,
                        int source,
                        int sys,
                        bool count,
                        int charges,
                        bool blink = false,
                        bool beacon = false) {
        expect(slot && slot->icon_file == icon && slot->effect_name == name &&
                   slot->source_item_index == source && slot->sys_name_id == sys &&
                   slot->is_count == count && slot->charges == charges &&
                   slot->always_blink == blink && !slot->is_mail_notification &&
                   slot->is_talent_beacon == beacon,
               "C6 buff slot retains all independently authored presentation and source fields");
    }
    std::uint64_t c6_beacon_mask() {
        return protocol::talent_bit(31) | protocol::talent_bit(32) | protocol::talent_bit(33) |
               protocol::talent_bit(34) | protocol::talent_bit(35) | protocol::talent_bit(36);
    }

    void test_c6_talent(bool snapshot) {
        const protocol::TalentState active{
            c6_beacon_mask(), 110, 120000, 30000, protocol::TalentStatus::SaveFailed};
        client::NetClient net;
        const auto s = attach(net);
        queue(s, protocol::MState::frame(0, protocol::kCmsHp));
        queue(s, protocol::frame_talent_state(active));
        auto bracket = c6_update(net);
        auto q = c6_observe(net,
                            *s,
                            "talent-active-maxima-non-ok",
                            snapshot,
                            c6_bracket_slots(net, bracket, 30000));
        expect(
            q.talent_pending && c6_same_talent(q.talent, active) && net.buff_slots().size() == 2 &&
                net.buff_slots()[0].bstate == protocol::kCmsHp &&
                net.buff_slots()[1].is_talent_beacon,
            "C6 accepted non-Ok talent snapshot retains all fields and appends unlocked Beacon after existing slot");
        c6_expect_slot(c6_slot(net, 0, true),
                       "talents/beacon.bmp",
                       "Beacon\nAttracts idle monsters within 20 tiles.",
                       0,
                       0,
                       false,
                       0,
                       false,
                       true);
        auto refresh = active;
        refresh.beacon_active_ms = 17000;
        refresh.beacon_cooldown_ms = 23;
        queue(s, protocol::frame_talent_state(active));
        queue(s, protocol::frame_talent_state(refresh));
        bracket = c6_update(net);
        q = c6_observe(net,
                       *s,
                       "talent-latest-refresh-in-place",
                       snapshot,
                       c6_bracket_slots(net, bracket, 17000));
        expect(
            q.talent_pending && c6_same_talent(q.talent, refresh) && net.buff_slots().size() == 2 &&
                net.buff_slots()[1].is_talent_beacon,
            "C6 latest talent replaces all fields and refreshes Beacon without moving unrelated slots");
        queue(s, protocol::frame_talent_state(refresh));
        bracket = c6_update(net);
        q = c6_observe(net,
                       *s,
                       "talent-identical-republishes",
                       snapshot,
                       c6_bracket_slots(net, bracket, 17000));
        expect(q.talent_pending && c6_same_talent(q.talent, refresh),
               "C6 identical accepted snapshot republishes dirty state");
        auto cancel = refresh;
        cancel.beacon_active_ms = 0;
        queue(s, protocol::frame_talent_state(cancel));
        c6_update(net);
        q = c6_observe(net, *s, "talent-cancel-keeps-other-slot", snapshot);
        expect(q.talent_pending && net.buff_slots().size() == 1 &&
                   net.buff_slots()[0].bstate == protocol::kCmsHp,
               "C6 zero active duration removes only the Beacon slot");
        auto locked = active;
        locked.mask = 0;
        queue(s, protocol::frame_talent_state(locked));
        c6_update(net);
        q = c6_observe(net, *s, "talent-active-without-unlock", snapshot);
        expect(
            q.talent_pending && c6_same_talent(q.talent, locked) && !c6_slot(net, 0, true),
            "C6 valid active duration without unlock is authoritative state without a Beacon icon");
        const auto complete = c6_body(protocol::frame_talent_state(active));
        for (std::size_t size = 0; size < complete.size(); ++size) {
            client::NetClient malformed;
            const auto state = attach(malformed);
            queue(state, protocol::frame_talent_state(active));
            c6_update(malformed);
            const auto timing = c6_retain_times(malformed);
            queue(state, protocol::Chatting::frame_s2c("C6", "before invalid talent"));
            queue(state,
                  c6_frame(protocol::S2C::TalentTree,
                           Bytes(complete.begin(),
                                 complete.begin() + static_cast<std::ptrdiff_t>(size))));
            queue(state, protocol::Chatting::frame_s2c("C6", "after invalid talent"));
            c6_update(malformed);
            const auto observed = c6_observe(
                malformed, *state, "talent-short-" + std::to_string(size), snapshot, timing);
            expect(
                malformed.connected() && observed.talent_pending &&
                    c6_same_talent(observed.talent, active) && observed.chats.size() == 2 &&
                    observed.chats[1].text == "after invalid talent",
                "C6 every short talent payload preserves complete earlier unread state and exact buff deadline");
        }
        for (int kind = 0; kind < 8; ++kind) {
            auto invalid = active;
            if (kind == 0) {
                invalid.status = static_cast<protocol::TalentStatus>(255);
            }
            if (kind == 1) {
                invalid.mask = std::uint64_t{1} << 37;
            }
            if (kind == 2) {
                invalid.mask = protocol::talent_bit(36);
            }
            if (kind == 3) {
                invalid.level = 1;
            }
            if (kind == 4) {
                invalid.level = 0;
            }
            if (kind == 5) {
                invalid.beacon_cooldown_ms = 120001;
            }
            if (kind == 6) {
                invalid.beacon_active_ms = 30001;
            }
            auto body = c6_body(protocol::frame_talent_state(invalid));
            if (kind == 7) {
                body.push_back(0);
            }
            client::NetClient malformed;
            const auto state = attach(malformed);
            queue(state, protocol::frame_talent_state(active));
            c6_update(malformed);
            protocol::TalentState consume;
            expect(malformed.take_talent_state(consume),
                   "C6 malformed talent case consumes valid dirty seed first");
            const auto timing = c6_retain_times(malformed);
            queue(state, c6_frame(protocol::S2C::TalentTree, std::move(body)));
            queue(state, protocol::Chatting::frame_s2c("C6", "after invalid talent range"));
            c6_update(malformed);
            const auto observed = c6_observe(
                malformed, *state, "talent-invalid-" + std::to_string(kind), snapshot, timing);
            expect(
                malformed.connected() && !observed.talent_pending && observed.chats.size() == 1 &&
                    c6_slot(malformed, 0, true),
                "C6 invalid talent ranges/tail cannot manufacture new dirty notification or remove existing Beacon");
        }
    }
    void test_c6_add_remove(bool snapshot) {
        for (const auto remain : {-1, 0, 17, 2000000000}) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, protocol::BuffState::frame_add(protocol::kCbsPayUpAttack, remain));
            const auto bracket = c6_update(net);
            const auto timing =
                remain > 0
                    ? c6_bracket_slots(net, bracket, static_cast<std::uint64_t>(remain) * 1000)
                    : C6Timing{};
            c6_observe(net, *s, "ordinary-remain-" + std::to_string(remain), snapshot, timing);
            c6_expect_slot(
                c6_slot(net, protocol::kCbsPayUpAttack), "shopbuff_004.bmp", "", 0, 919, false, 0);
            expect(remain > 0 || net.buff_slots()[0].expire_ms == 0,
                   "C6 nonpositive ordinary remain is permanent without blink/count");
        }
        for (const auto remain : {-1, 0, 9}) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s,
                  protocol::BuffState::frame_add(
                      protocol::kCbsPayPerfectDef | protocol::kCbsPayDrainHp, remain));
            c6_update(net);
            c6_observe(net, *s, "count-remain-" + std::to_string(remain), snapshot);
            expect(net.buff_slots().size() == 2 &&
                       net.buff_slots()[0].bstate == protocol::kCbsPayPerfectDef &&
                       net.buff_slots()[1].bstate == protocol::kCbsPayDrainHp &&
                       net.buff_slots()[0].expire_ms == 0 && net.buff_slots()[1].expire_ms == 0,
                   "C6 ascending count bits preserve signed charges with zero expiry");
            c6_expect_slot(c6_slot(net, protocol::kCbsPayPerfectDef),
                           "shopbuff_010.bmp",
                           "",
                           0,
                           925,
                           true,
                           remain);
            c6_expect_slot(c6_slot(net, protocol::kCbsPayDrainHp),
                           "shopbuff_016.bmp",
                           "",
                           0,
                           931,
                           true,
                           remain);
        }
        client::NetClient net;
        const auto s = attach(net);
        const auto mask = protocol::kCbsPayUpAttack | protocol::kCbsSkillStone |
                          protocol::kCbsInventoryRegisterMask | (std::uint64_t{1} << 63) |
                          std::uint64_t{1};
        queue(s, protocol::BuffState::frame_add(mask, 21));
        const auto bracket = c6_update(net);
        auto timing = c6_bracket_slots(net, bracket, 21000);
        for (auto& r : timing.registers) {
            r = C6Deadline{0, false, bracket, 21000, 0};
        }
        expect(
            net.buff_slots().size() == 3 &&
                net.buff_slots()[0].bstate == protocol::kCbsPayUpAttack &&
                net.buff_slots()[1].bstate == protocol::kCbsSkillStone &&
                net.buff_slots()[2].bstate == protocol::kCbsSkillResistSpirit,
            "C6 mixed mask skips unmapped bit0 and register HUD slots while retaining ascending visible bits through63");
        c6_expect_slot(
            c6_slot(net, protocol::kCbsSkillResistSpirit), "active108.bmp", "", 0, 0, false, 0);
        for (auto deadline : net.inventory_register_expire_ms()) {
            expect(deadline == net.buff_slots()[0].expire_ms &&
                       deadline == net.buff_slots()[1].expire_ms &&
                       deadline == net.buff_slots()[2].expire_ms,
                   "C6 all timed slots/registers from one Add share the same observed clock read");
        }
        c6_observe(net, *s, "add-mixed-same-read-and-order", snapshot, timing);
        const auto stone_deadline = net.buff_slots()[1].expire_ms;
        const auto spirit_deadline = net.buff_slots()[2].expire_ms;
        const auto register_deadlines = net.inventory_register_expire_ms();
        queue(s, protocol::BuffState::frame_add(protocol::kCbsPayUpAttack, 7));
        const auto refreshed = c6_update(net);
        auto retained = timing;
        for (std::size_t i = 0; i < register_deadlines.size(); ++i) {
            retained.registers[i] = C6Deadline{0, false, {}, 0, register_deadlines[i]};
        }
        retained.slots = {{protocol::kCbsPayUpAttack, false, refreshed, 7000, 0},
                          {protocol::kCbsSkillStone, false, {}, 0, stone_deadline},
                          {protocol::kCbsSkillResistSpirit, false, {}, 0, spirit_deadline}};
        c6_observe(net, *s, "add-upsert-same-position", snapshot, retained);
        expect(net.buff_slots().size() == 3 &&
                   net.buff_slots()[0].bstate == protocol::kCbsPayUpAttack &&
                   net.buff_slots()[1].expire_ms == stone_deadline &&
                   net.buff_slots()[2].expire_ms == spirit_deadline,
               "C6 upsert refreshes only selected slot in its old vector position");
        queue(s,
              protocol::BuffState::frame_sub(protocol::kCbsPayUpAttack |
                                             protocol::kCbsInventoryRegister4));
        c6_update(net);
        auto removed = retained;
        removed.registers[1].reset();
        c6_observe(net, *s, "sub-selected-slots-and-register", snapshot, removed);
        expect(net.buff_slots().size() == 2 &&
                   net.buff_slots()[0].bstate == protocol::kCbsSkillStone &&
                   net.buff_slots()[1].bstate == protocol::kCbsSkillResistSpirit &&
                   net.inventory_register_expire_ms()[1] == 0 &&
                   net.inventory_register_expire_ms()[0] > 0,
               "C6 subtraction removes selected entries and only selected register deadlines");
        queue(s, protocol::BuffState::frame_sub(std::uint64_t{1}));
        c6_update(net);
        c6_observe(net, *s, "sub-unknown-noop", snapshot, removed);
        expect(net.buff_slots().size() == 2 &&
                   net.buff_slots()[0].bstate == protocol::kCbsSkillStone &&
                   net.buff_slots()[1].bstate == protocol::kCbsSkillResistSpirit,
               "C6 unrelated unknown subtraction does not reorder survivors");
        client::NetClient beacon;
        const auto state = attach(beacon);
        queue(state,
              protocol::frame_talent_state(
                  {c6_beacon_mask(), 110, 0, 10000, protocol::TalentStatus::Ok}));
        c6_update(beacon);
        queue(state, protocol::BuffState::frame_sub(0));
        c6_update(beacon);
        const auto q =
            c6_observe(beacon, *state, "sub-zero-removes-beacon-keeps-authority", snapshot);
        expect(
            q.talent_pending && q.talent.beacon_active_ms == 10000 && beacon.buff_slots().empty(),
            "C6 zero subtraction removes bstate0 Beacon while its authoritative talent snapshot stays pending");
        queue(state,
              protocol::frame_talent_state(
                  {c6_beacon_mask(), 110, 0, 5000, protocol::TalentStatus::Ok}));
        const auto readded = c6_update(beacon);
        c6_observe(beacon,
                   *state,
                   "talent-resynchronizes-after-sub-zero",
                   snapshot,
                   c6_bracket_slots(beacon, readded, 5000));
        expect(c6_slot(beacon, 0, true),
               "C6 later authoritative talent resynchronizes the removed Beacon icon");
        client::NetClient alias;
        const auto alias_state = attach(alias);
        queue(alias_state, protocol::MState::frame(0, protocol::kCmsHp));
        queue(alias_state,
              protocol::BuffState::frame_add(protocol::kCbsSkillThiefDanceOfKilling, 0));
        c6_update(alias);
        c6_observe(alias, *alias_state, "rest-food-cbs-alias-replacement", snapshot);
        expect(
            alias.buff_slots().size() == 1 && !alias.buff_slots()[0].always_blink &&
                alias.buff_slots()[0].icon_file == "active140.bmp",
            "C6 numeric CBS/food alias replaces shared vector entry without new namespace policy");
        queue(alias_state, protocol::BuffState::frame_sub(protocol::kCmsHp));
        c6_update(alias);
        c6_observe(alias, *alias_state, "rest-food-cbs-alias-removal", snapshot);
        expect(alias.buff_slots().empty(), "C6 same numeric subtraction removes the alias entry");
    }

    Bytes c6_monster(std::uint32_t id = 51, std::uint64_t ignored_mstate = 0) {
        protocol::PacketWriter w;
        w.write_u16(17);
        w.write_u32(id);
        w.write_u32(1000);
        w.write_u32(2000);
        w.write_u16(0);
        w.write_i32(100);
        w.write_i32(100);
        w.write_u32(0);
        w.write_u32(static_cast<std::uint32_t>(ignored_mstate));
        w.write_u64(0);
        w.write_string("");
        w.write_u8(0);
        w.write_u32(0);
        w.write_string("Owned C6 monster");
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::CreateMonster));
    }
    void test_c6_mstate_speed(bool snapshot) {
        client::NetClient net;
        const auto s = attach(net);
        queue(s, protocol::MState::frame(999, protocol::kCmsHp | protocol::kCmsMp));
        c6_update(net);
        c6_observe(net, *s, "mstate-preidentity-food-order", snapshot);
        expect(net.entity_consuming(999) && net.buff_slots().size() == 2 &&
                   net.buff_slots()[0].bstate == protocol::kCmsHp &&
                   net.buff_slots()[1].bstate == protocol::kCmsMp,
               "C6 before local identity any entity food state appends HP then MP");
        c6_expect_slot(c6_slot(net, protocol::kCmsHp), "buff034.bmp", "", 0, 671, false, 0, true);
        c6_expect_slot(c6_slot(net, protocol::kCmsMp), "buff034.bmp", "", 0, 672, false, 0, true);
        queue(s, create(41, true));
        queue(s, protocol::MState::frame(999, 0));
        c6_update(net);
        c6_observe(net, *s, "mstate-remote-food-does-not-clear-local", snapshot);
        expect(
            !net.entity_consuming(999) && net.buff_slots().size() == 2,
            "C6 after local identity remote state updates consuming but leaves local food HUD intact");
        queue(s, protocol::MState::frame(41, protocol::kCmsMp));
        c6_update(net);
        c6_observe(net, *s, "mstate-local-food-removes-only-hp", snapshot);
        expect(net.buff_slots().size() == 1 && net.buff_slots()[0].bstate == protocol::kCmsMp &&
                   net.entity_consuming(41),
               "C6 local food clear removes only the absent matching slot");
        queue(s,
              protocol::MState::frame(
                  51, protocol::kCmsMoveSpeed | protocol::kCmsMoveStop | protocol::kCmsHp));
        queue(s, c6_monster());
        c6_update(net);
        c6_observe(net, *s, "mstate-before-monster-not-retroactive", snapshot);
        expect(
            net.entity_consuming(51) && !net.monsters().at(51).ice_slowed() &&
                !net.monsters().at(51).move_stopped(),
            "C6 pre-create MState consuming is cached but visual flags are not retroactively applied");
        for (const auto state : {protocol::kCmsMoveSpeed,
                                 protocol::kCmsMoveStop,
                                 protocol::kCmsMoveSpeed | protocol::kCmsMoveStop,
                                 std::uint64_t{0}}) {
            queue(s, protocol::MState::frame(51, state));
            c6_update(net);
            c6_observe(net, *s, "mstate-monster-flags-" + std::to_string(state), snapshot);
            expect(net.monsters().at(51).ice_slowed() == (state != 0) &&
                       net.monsters().at(51).move_stopped() ==
                           ((state & protocol::kCmsMoveStop) != 0),
                   "C6 each monster MState independently controls slow tint and stop flag");
        }
        for (const auto value : {-100, 0, 50}) {
            queue(s,
                  protocol::Property::frame_update_mproperty(
                      41, protocol::kPropertyMSpeed, static_cast<std::int16_t>(value)));
            queue(s,
                  protocol::Property::frame_update_mproperty(
                      51, protocol::kPropertyMSpeed, static_cast<std::int16_t>(value)));
            c6_update(net);
            c6_observe(net, *s, "mspeed-signed-" + std::to_string(value), snapshot);
            expect(
                net.local_mspeed() == value && net.monsters().at(51).speed_value() == value &&
                    net.monsters().at(51).ice_slowed() == (value < 0) &&
                    net.monsters().at(51).move_speed() == (value < 0    ? 10.f
                                                           : value == 0 ? 20.f
                                                                        : 30.f),
                "C6 signed local and monster speed preserve independent fixed positive/negative goldens");
        }
        queue(s, protocol::Property::frame_update_mproperty(51, protocol::kPropertyMSpeed, -100));
        queue(s, protocol::MState::frame(51, 0));
        c6_update(net);
        c6_observe(net, *s, "speed-then-state-reset", snapshot);
        expect(net.monsters().at(51).speed_value() == 0 && !net.monsters().at(51).ice_slowed(),
               "C6 absent MoveSpeed resets earlier negative property speed");
        queue(s, protocol::MState::frame(51, 0));
        queue(s, protocol::Property::frame_update_mproperty(51, protocol::kPropertyMSpeed, -100));
        c6_update(net);
        c6_observe(net, *s, "state-then-speed-negative", snapshot);
        expect(net.monsters().at(51).speed_value() == -100 && net.monsters().at(51).ice_slowed(),
               "C6 opposite state/property order retains the later negative speed");
        queue(s, protocol::MState::frame(51, protocol::kCmsMoveStop));
        queue(s, protocol::Property::frame_update_mproperty(51, protocol::kPropertyMSpeed, 50));
        queue(s, protocol::Property::frame_update_mproperty(41, 0, 77));
        queue(s, protocol::Property::frame_update_mproperty(999, protocol::kPropertyMSpeed, 77));
        c6_update(net);
        c6_observe(net, *s, "stop-plus-positive-speed-and-ignored-targets", snapshot);
        expect(
            net.monsters().at(51).move_stopped() && !net.monsters().at(51).ice_slowed() &&
                net.monsters().at(51).speed_value() == 50 && net.local_mspeed() == 50 &&
                net.monsters().size() == 1,
            "C6 positive speed can clear tint while stop remains; foreign property/unknown target do not alter other state");
        client::NetClient noid;
        const auto state = attach(noid);
        queue(state, protocol::Property::frame_update_mproperty(0, protocol::kPropertyMSpeed, 50));
        c6_update(noid);
        c6_observe(noid, *state, "mspeed-zero-local-not-established", snapshot);
        expect(noid.local_mspeed() == 0,
               "C6 entity0 cannot establish local speed before a nonzero identity");
    }
    void test_c6_feedback(bool snapshot) {
        client::NetClient net;
        const auto s = attach(net);
        net.set_char_identity("Owned C6", 0, "");
        const std::array<int, 4> specialty{{0, 1, 3, 255}};
        const std::array<int, 4> indices{{0, 0, 1, 7}};
        for (std::size_t i = 0; i < specialty.size(); ++i) {
            queue(s,
                  c6_message(static_cast<std::uint8_t>(protocol::InfoMsg::SpecialtyUp),
                             i % 2 != 0,
                             {static_cast<std::uint8_t>(specialty[i]), 0xab}));
            c6_update(net);
            c6_observe(net, *s, "specialty-" + std::to_string(specialty[i]), snapshot);
            expect(
                net.specialty() == static_cast<unsigned>(specialty[i]) &&
                    net.char_info_stats().specialty_index == indices[i] &&
                    net.char_info_stats().gov_pos.empty(),
                "C6 specialty accepts raw mask/tail and uses highest-set-bit title with explicitly unloaded jobs");
        }
        queue(s, c6_message(static_cast<std::uint8_t>(protocol::InfoMsg::SpecialtyUp), true));
        c6_update(net);
        c6_observe(net, *s, "specialty-missing-value-retains", snapshot);
        expect(net.specialty() == 255 && net.char_info_stats().specialty_index == 7,
               "C6 truncated specialty leaves all previous title state intact");
        const std::array<std::uint8_t, 6> ids{{11, 17, 22, 39, 54, 77}};
        const std::array<const char*, 6> lines{{"You cannot discard an equipped item.",
                                                "Your inventory is full.",
                                                "You cannot pick up that item yet.",
                                                "Not enough inventory slots.",
                                                "Enchanting Failed.",
                                                "Action not possible, item bound."}};
        for (std::size_t i = 0; i < ids.size(); ++i) {
            queue(s, c6_message(ids[i], i % 2 != 0, {0xab}));
        }
        queue(s, c6_message(54));
        c6_update(net);
        auto q = c6_observe(net, *s, "system-feedback-fifo-and-enchant", snapshot);
        expect(q.enchant && q.infos.size() == 7 && net.last_chest_feedback_msg() == 0,
               "C6 repeated enchant keeps one latch and every system feedback FIFO entry");
        for (std::size_t i = 0; i < std::min(q.infos.size(), std::size_t{7}); ++i) {
            expect(
                q.infos[i].text == lines[i == 6 ? 4 : i] && !q.infos[i].kind && !q.infos[i].color,
                "C6 known system messages preserve independently authored exact text/null metadata");
        }
        for (bool variable : {false, true}) {
            for (const auto supplied : {0, 6, 255}) {
                queue(s, c6_message(87, variable, {static_cast<std::uint8_t>(supplied), 0xab}));
                c6_update(net);
                q = c6_observe(net,
                               *s,
                               "chest-open-" + std::to_string(variable) + "-" +
                                   std::to_string(supplied),
                               snapshot);
                expect(
                    q.chest_start && q.chest_seconds == (variable ? supplied : 6) &&
                        !q.chest_stop && q.infos.empty() && net.last_chest_feedback_msg() == 87,
                    "C6 ordinary Open ignores its tail while MessageV uses the authored duration byte");
            }
        }
        queue(s, c6_message(87, true));
        c6_update(net);
        q = c6_observe(net, *s, "chest-open-variable-default-on-truncation", snapshot);
        expect(q.chest_start && q.chest_seconds == 6,
               "C6 missing MessageV duration retains the initialized six-second default");
        for (const auto stop : {88, 89, 86}) {
            queue(s, c6_message(static_cast<std::uint8_t>(stop), true, {0xab}));
            c6_update(net);
            q = c6_observe(net, *s, "chest-stop-" + std::to_string(stop), snapshot);
            expect(!q.chest_start && q.chest_stop && net.last_chest_feedback_msg() == stop &&
                       q.infos.empty(),
                   "C6 Opened/Canceled/Used set only the stop latch and actual last ordinal");
        }
        queue(s, c6_message(87, true, {3}));
        queue(s, c6_message(87, true, {9}));
        queue(s, c6_message(88));
        queue(s, c6_message(255));
        c6_update(net);
        q = c6_observe(net, *s, "chest-coalesced-start-stop-unknown", snapshot);
        expect(
            q.chest_start && q.chest_seconds == 9 && q.chest_stop && q.infos.empty() &&
                net.last_chest_feedback_msg() == 88,
            "C6 starts coalesce latest while stop remains independent and unknown does not overwrite last chest");
    }

    struct C6Catalogs {
        std::string root;
        client::ItemDb items;
        client::Catalog skills;
        C6Catalogs() {
            root = (std::filesystem::temp_directory_path() /
                    ("openkal_c6_owned_" + std::to_string(getpid())))
                       .string();
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root + "/items");
            std::filesystem::create_directories(root + "/skills");
            std::filesystem::create_directories(root + "/prefixes");
            std::ofstream(root + "/bof.json") << R"({"minGrade":1})";
            std::ofstream(root + "/items/41001.json")
                << R"({"index":41001,"name":"Owned C6 item","image":"c6_owned_item_unique"})";
            std::ofstream(root + "/items/41002.json")
                << R"({"index":41002,"name":"Owned C6 empty image","image":""})";
            std::ofstream(root + "/skills/0_500.json")
                << R"({"class":0,"index":500,"name":"","image":""})";
            std::ofstream(root + "/skills/1_500.json")
                << R"({"class":1,"index":500,"name":"Owned C6 mage fallback","image":"c6_owned_mage_unique"})";
            std::ofstream(root + "/skills/2_501.json")
                << R"({"class":2,"index":501,"name":"Owned C6 other class","image":"c6_owned_other_unique"})";
            std::ofstream(root + "/skills/0_502.json")
                << R"({"class":0,"index":502,"name":"Owned C6 exact class","image":"c6_owned_exact_unique"})";
            std::ofstream(root + "/skills/1_17.json")
                << R"({"class":1,"index":17,"name":"Owned C6 stone","image":"c6_owned_stone_unique"})";
            expect(
                items.load(root, root, root + "/items") && skills.load_json_dir(root + "/skills"),
                "C6 loads only owned numeric item and skill catalogs for presentation precedence");
        }
    };
    void test_c6_catalog_and_optional_sources(bool snapshot) {
        C6Catalogs data;
        struct Input {
            int skill;
            int item;
            std::uint64_t bit;
            const char* icon;
            const char* name;
            int source;
        };
        const std::array<Input, 9> cases{
            {{0,
              41001,
              protocol::kCbsSkillStone,
              "c6_owned_item_unique.bmp",
              "Owned C6 item",
              41001},
             {501,
              41001,
              protocol::kCbsSkillStone,
              "c6_owned_item_unique.bmp",
              "Owned C6 other class",
              41001},
             {502,
              0,
              protocol::kCbsSkillStone,
              "c6_owned_exact_unique.bmp",
              "Owned C6 exact class",
              0},
             {501,
              0,
              protocol::kCbsSkillStone,
              "c6_owned_other_unique.bmp",
              "Owned C6 other class",
              0},
             {500,
              0,
              protocol::kCbsSkillStone,
              "c6_owned_mage_unique.bmp",
              "Owned C6 mage fallback",
              0},
             {99999,
              41001,
              protocol::kCbsSkillStone,
              "c6_owned_item_unique.bmp",
              "Owned C6 stone",
              41001},
             {501, 41001, protocol::kCbsSkillHaste, "buff021.bmp", "Owned C6 other class", 41001},
             {501,
              35,
              protocol::kCbsExpEvent,
              "c6_owned_other_unique.bmp",
              "EXP Event: +35% EXP",
              0},
             {0, 41001, std::uint64_t{1}, "c6_owned_item_unique.bmp", "Owned C6 item", 41001}}};
        for (std::size_t i = 0; i < cases.size(); ++i) {
            client::NetClient net;
            const auto s = attach(net);
            net.set_item_catalog(&data.items);
            net.set_skill_catalog(&data.skills);
            net.set_char_identity("Owned C6", 0, "");
            queue(s,
                  protocol::BuffState::frame_add(cases[i].bit, 0, cases[i].skill, cases[i].item));
            c6_update(net);
            c6_observe(net, *s, "catalog-precedence-" + std::to_string(i), snapshot);
            c6_expect_slot(c6_slot(net, cases[i].bit),
                           cases[i].icon,
                           cases[i].name,
                           cases[i].source,
                           0,
                           false,
                           0);
        }
        for (const int item : {41002, 49999}) {
            client::NetClient net;
            const auto s = attach(net);
            net.set_item_catalog(&data.items);
            net.set_skill_catalog(&data.skills);
            queue(s, protocol::BuffState::frame_add(protocol::kCbsSkillStone, 0, 501, item));
            c6_update(net);
            c6_observe(net, *s, "catalog-empty-or-missing-item-" + std::to_string(item), snapshot);
            c6_expect_slot(c6_slot(net, protocol::kCbsSkillStone),
                           "c6_owned_other_unique.bmp",
                           "Owned C6 other class",
                           item,
                           0,
                           false,
                           0);
        }
        const auto full =
            c6_body(protocol::BuffState::frame_add(protocol::kCbsSkillStone, 0, 501, 41001));
        for (const bool variable : {false, true}) {
            for (std::size_t size = 13; size <= 24; ++size) {
                client::NetClient net;
                const auto s = attach(net);
                net.set_item_catalog(&data.items);
                net.set_skill_catalog(&data.skills);
                Bytes body = full;
                body.resize(size, 0xab);
                queue(s,
                      c6_frame(variable ? protocol::S2C::MessageV : protocol::S2C::Message,
                               std::move(body)));
                c6_update(net);
                c6_observe(net,
                           *s,
                           "optional-source-" + std::to_string(variable) + "-" +
                               std::to_string(size),
                           snapshot);
                const char* icon = size >= 21   ? "c6_owned_item_unique.bmp"
                                   : size >= 17 ? "c6_owned_other_unique.bmp"
                                                : "c6_owned_stone_unique.bmp";
                const char* name = size >= 17 ? "Owned C6 other class" : "Owned C6 stone";
                c6_expect_slot(c6_slot(net, protocol::kCbsSkillStone),
                               icon,
                               name,
                               size >= 21 ? 41001 : 0,
                               0,
                               false,
                               0);
            }
        }
    }
    void test_c6_payload_boundaries(bool snapshot) {
        struct Payload {
            const char* name;
            protocol::S2C type;
            Bytes bytes;
            std::size_t minimum;
        };
        const std::array<Payload, 6> cases{
            {{"add-message",
              protocol::S2C::Message,
              c6_body(protocol::BuffState::frame_add(protocol::kCbsPayCutDamage, 0)),
              13},
             {"add-message-v",
              protocol::S2C::MessageV,
              c6_body(protocol::BuffState::frame_add(protocol::kCbsPayCutDamage, 0)),
              13},
             {"sub-message",
              protocol::S2C::Message,
              c6_body(protocol::BuffState::frame_sub(protocol::kCbsPayUpAttack)),
              9},
             {"sub-message-v",
              protocol::S2C::MessageV,
              c6_body(protocol::BuffState::frame_sub(protocol::kCbsPayUpAttack)),
              9},
             {"mstate",
              protocol::S2C::MState,
              c6_body(protocol::MState::frame(0, protocol::kCmsHp)),
              12},
             {"mproperty",
              protocol::S2C::UpdateMProperty,
              c6_body(protocol::Property::frame_update_mproperty(0, protocol::kPropertyMSpeed, 7)),
              7}}};
        for (const auto& value : cases) {
            for (std::size_t size = 0; size < value.minimum; ++size) {
                client::NetClient net;
                const auto s = attach(net);
                queue(s, protocol::BuffState::frame_add(protocol::kCbsPayUpAttack, 0));
                queue(s, protocol::Chatting::frame_s2c("C6", "before malformed status"));
                queue(s,
                      c6_frame(value.type,
                               Bytes(value.bytes.begin(),
                                     value.bytes.begin() + static_cast<std::ptrdiff_t>(size))));
                queue(s, protocol::Chatting::frame_s2c("C6", "after malformed status"));
                c6_update(net);
                const auto q =
                    c6_observe(net,
                               *s,
                               "short-" + std::string(value.name) + "-" + std::to_string(size),
                               snapshot);
                expect(
                    net.connected() && s->closes == 0 && q.chats.size() == 2 &&
                        q.chats[0].text == "before malformed status" &&
                        q.chats[1].text == "after malformed status" &&
                        net.buff_slots().size() == 1 &&
                        net.buff_slots()[0].bstate == protocol::kCbsPayUpAttack &&
                        net.local_mspeed() == 0 && !net.entity_consuming(0) && q.infos.empty() &&
                        !q.chest_start && !q.chest_stop && !q.enchant,
                    "C6 every short required status prefix preserves seeded HUD state and typed neighbors");
            }
        }
        for (const bool variable : {false, true}) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, c6_frame(variable ? protocol::S2C::MessageV : protocol::S2C::Message, {}));
            queue(s, protocol::Chatting::frame_s2c("C6", "empty message suffix"));
            c6_update(net);
            const auto q =
                c6_observe(net, *s, "empty-message-" + std::to_string(variable), snapshot);
            expect(net.buff_slots().empty() && q.infos.empty() && !q.enchant && q.chats.size() == 1,
                   "C6 empty Message/MessageV is ignored without swallowing its suffix");
        }
        for (int kind = 0; kind < 3; ++kind) {
            client::NetClient net;
            const auto s = attach(net);
            queue(s, create(41, true));
            queue(s, c6_monster());
            Bytes body;
            protocol::S2C type;
            if (kind == 0) {
                queue(s, protocol::BuffState::frame_add(protocol::kCbsPayUpAttack, 0));
                body = c6_body(protocol::BuffState::frame_sub(protocol::kCbsPayUpAttack));
                type = protocol::S2C::MessageV;
            } else if (kind == 1) {
                body = c6_body(protocol::MState::frame(41, protocol::kCmsHp));
                type = protocol::S2C::MState;
            } else {
                body = c6_body(protocol::Property::frame_update_mproperty(
                    41, protocol::kPropertyMSpeed, -100));
                type = protocol::S2C::UpdateMProperty;
            }
            body.push_back(0xab);
            queue(s, c6_frame(type, std::move(body)));
            c6_update(net);
            c6_observe(net, *s, "status-trailing-" + std::to_string(kind), snapshot);
            expect((kind == 0 && net.buff_slots().empty()) ||
                       (kind == 1 && net.entity_consuming(41) && net.buff_slots().size() == 1) ||
                       (kind == 2 && net.local_mspeed() == -100),
                   "C6 SubBState/MState/MProperty preserve existing accepted trailing bytes");
        }
    }
    void test_c6_lifetime_and_isolation(bool snapshot) {
        client::NetClient a, b;
        const auto sa = attach(a), sb = attach(b);
        a.set_char_identity("Owned C6 A", 0, "");
        queue(sa, create(41, true));
        queue(sa, c6_monster());
        queue(sa, protocol::MState::frame(41, protocol::kCmsMp));
        queue(sa, protocol::Property::frame_update_mproperty(41, protocol::kPropertyMSpeed, 50));
        queue(sa, protocol::BuffState::frame_add(protocol::kCbsInventoryRegister3, 11));
        queue(sa,
              protocol::frame_talent_state(
                  {c6_beacon_mask(), 110, 19, 0, protocol::TalentStatus::Stale}));
        queue(sa, c6_message(static_cast<std::uint8_t>(protocol::InfoMsg::SpecialtyUp), true, {3}));
        queue(sa, c6_message(87, true, {9}));
        queue(sa, c6_message(88));
        queue(sa, c6_message(54));
        const auto bracket = c6_update(a);
        C6Timing timing;
        timing.registers[0] = C6Deadline{0, false, bracket, 11000, 0};
        queue(sb, protocol::BuffState::frame_add(protocol::kCbsPayDrainHp, 4));
        c6_update(b);
        const auto other = c6_observe(b, *sb, "status-isolation-B", snapshot);
        expect(b.buff_slots().size() == 1 && b.buff_slots()[0].is_count && b.specialty() == 1 &&
                   b.local_mspeed() == 0 && !other.chest_start && !other.enchant &&
                   !other.talent_pending,
               "C6 status state and pending feedback do not leak into the second client");
        (void) c6_deadline(a.inventory_register_expire_ms()[0], &*timing.registers[0]);
        const auto retained = c6_retain_times(a);
        sa->eof = true;
        c6_update(a);
        expect(
            a.buff_slots().size() == 1 && a.entity_consuming(41) && a.monsters().count(51) == 1 &&
                a.inventory_register_expire_ms()[0] > 0,
            "C6 EOF online-view cleanup intentionally retains separate status and live monster domains");
        c6_observe(a, *sa, "status-eof-retains-domains", snapshot, retained);
        // Re-establish pending latches after the EOF observation consumed the previous ones.
        sa->eof = false;
        sa->alive = true;
        queue(sa,
              protocol::frame_talent_state(
                  {c6_beacon_mask(), 110, 19, 0, protocol::TalentStatus::Stale}));
        queue(sa, c6_message(87, true, {7}));
        queue(sa, c6_message(89));
        queue(sa, c6_message(54));
        c6_update(a);
        const auto exact_register = a.inventory_register_expire_ms()[0];
        a.close();
        const auto fresh = attach(a);
        C6Timing after_close;
        after_close.registers[0] = C6Deadline{0, false, {}, 0, exact_register};
        const auto q =
            c6_observe(a, *fresh, "status-close-adopt-exact-retain-set", snapshot, after_close);
        expect(
            a.buff_slots().empty() && !a.entity_consuming(41) && a.monsters().empty() &&
                !q.talent_pending && a.local_mspeed() == 50 && a.specialty() == 3 &&
                a.char_info_stats().specialty_index == 1 && q.chest_start && q.chest_seconds == 7 &&
                q.chest_stop && q.enchant && q.infos.size() == 1 &&
                q.infos[0].text == "Enchanting Failed." && a.last_chest_feedback_msg() == 89,
            "C6 close/adopt clears buffs/talent/entities while preserving registers/speed/specialty/chest/enchant/info");
    }
    // C6_REACHABLE_STATUS_GUARDS_BEGIN
    void test_c6_native_buff_golden(bool snapshot) {
        struct NativeBuff {
            std::uint64_t bit;
            const char* icon;
            int sys;
            bool count;
            const char* name;
        };
        const std::array<NativeBuff, 44> cases{
            {{protocol::kCbsPayUpAttack, "shopbuff_004.bmp", 919, false, ""},
             {protocol::kCbsPayCutDamage, "shopbuff_005.bmp", 920, false, ""},
             {protocol::kCbsPayUpXBlow3, "shopbuff_006.bmp", 921, false, ""},
             {protocol::kCbsPayUpXBlow5, "shopbuff_007.bmp", 922, false, ""},
             {protocol::kCbsPayUpHit, "shopbuff_008.bmp", 923, false, ""},
             {protocol::kCbsPayUpDodge, "shopbuff_009.bmp", 924, false, ""},
             {protocol::kCbsPayPerfectDef, "shopbuff_010.bmp", 925, true, ""},
             {protocol::kCbsPayDrainHp, "shopbuff_016.bmp", 931, true, ""},
             {protocol::kCbsExpEvent, "expEvent.bmp", 0, false, "EXP Event: +0% EXP"},
             {protocol::kCbsSkillStone, "active015.bmp", 0, false, ""},
             {protocol::kCbsSkillHaste, "buff021.bmp", 0, false, ""},
             {protocol::kCbsSkillMeditation, "active017.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightForceDefense, "active060.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightDefense, "active061.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightHealing, "active025.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightLightning, "active096.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightIce, "active097.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightFlame, "active098.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightAccuracy, "active033.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightBattle, "active034.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightBerserk, "active093.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightProtect, "active040.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightElasticShield, "active102.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightPerfectDefense, "active125.bmp", 0, false, ""},
             {protocol::kCbsSkillKnightConcentration, "active033.bmp", 0, false, ""},
             {protocol::kCbsSkillRefiningWeapon, "active065.bmp", 0, false, ""},
             {protocol::kCbsSkillAuraDefense, "active066.bmp", 0, false, ""},
             {protocol::kCbsSkillBlessStrength, "active067.bmp", 0, false, ""},
             {protocol::kCbsSkillBlessHealth, "active068.bmp", 0, false, ""},
             {protocol::kCbsSkillBlessAgility, "active069.bmp", 0, false, ""},
             {protocol::kCbsSkillBlessIntelligence, "active070.bmp", 0, false, ""},
             {protocol::kCbsSkillBlessCritical, "active071.bmp", 0, false, ""},
             {protocol::kCbsSkillResistFire, "active040.bmp", 0, false, ""},
             {protocol::kCbsSkillResistIce, "active041.bmp", 0, false, ""},
             {protocol::kCbsSkillResistLightning, "active042.bmp", 0, false, ""},
             {protocol::kCbsSkillEscape, "active046.bmp", 0, false, ""},
             {protocol::kCbsSkillArcherSharpness, "active154.bmp", 0, false, ""},
             {protocol::kCbsSkillAnimalRage, "active046.bmp", 0, false, ""},
             {protocol::kCbsSkillThiefDanceOfKilling, "active140.bmp", 0, false, ""},
             {protocol::kCbsSkillHiding, "active070.bmp", 0, false, ""},
             {protocol::kCbsSkillArcherChanceCapture, "passive011.bmp", 0, false, ""},
             {protocol::kCbsSkillArcherMuscleSolidation, "active036.bmp", 0, false, ""},
             {protocol::kCbsSkillResistCurse, "active107.bmp", 0, false, ""},
             {protocol::kCbsSkillResistSpirit, "active108.bmp", 0, false, ""}}};
        for (std::size_t i = 0; i < cases.size(); ++i) {
            const auto& expected = cases[i];
            client::NetClient net;
            const auto session = attach(net);
            const auto frame = protocol::BuffState::frame_add(expected.bit, expected.count ? 9 : 0);
            queue(session,
                  c6_frame(i % 2 == 0 ? protocol::S2C::Message : protocol::S2C::MessageV,
                           c6_body(frame)));
            c6_update(net);
            const auto observed =
                c6_observe(net, *session, "native-buff-golden-" + std::to_string(i), snapshot);
            expect(net.connected() && session->closes == 0 && net.buff_slots().size() == 1 &&
                       observed.infos.empty() && !observed.chest_start && !observed.chest_stop,
                   "C6 each authored native BState produces one slot without unrelated feedback");
            c6_expect_slot(c6_slot(net, expected.bit),
                           expected.icon,
                           expected.name,
                           0,
                           expected.sys,
                           expected.count,
                           expected.count ? 9 : 0);
        }
    }

    void test_c6_owned_catalog_reachable_paths(bool snapshot) {
        C6Catalogs data;
        {
            client::NetClient net;
            const auto session = attach(net);
            net.set_skill_catalog(&data.skills);
            queue(session,
                  protocol::BuffState::frame_add(
                      protocol::kCbsSkillKnightPerfectDefense, 0, 123456, 0));
            c6_update(net);
            c6_observe(net, *session, "owned-skill-miss-native-fallback", snapshot);
            c6_expect_slot(c6_slot(net, protocol::kCbsSkillKnightPerfectDefense),
                           "active125.bmp",
                           "",
                           0,
                           0,
                           false,
                           0);
        }
        std::filesystem::create_directories(data.root + "/HyperText");
        std::ofstream(data.root + "/items/41003.json")
            << R"({"index":41003,"name":"Owned C6 upper extension","image":"c6_owned_exact_upper_unique"})";
        std::ofstream(data.root + "/items/41004.json")
            << R"({"index":41004,"name":"Owned C6 lower variant","image":"C6_Owned_Lower_Variant_Unique"})";
        std::ofstream(data.root + "/HyperText/c6_owned_exact_upper_unique.BMP")
            << "Owned filename-presence fixture; no image decoding is requested.";
        std::ofstream(data.root + "/HyperText/c6_owned_lower_variant_unique.bmp")
            << "Owned filename-presence fixture; no image decoding is requested.";
        expect(
            data.items.load(data.root, data.root, data.root + "/items"),
            "C6 reloads owned item names and filename-presence fixtures without external assets");
        const std::array<int, 2> indices{{41003, 41004}};
        const std::array<const char*, 2> icons{
            {"c6_owned_exact_upper_unique.BMP", "c6_owned_lower_variant_unique.bmp"}};
        const std::array<const char*, 2> names{
            {"Owned C6 upper extension", "Owned C6 lower variant"}};
        for (std::size_t i = 0; i < indices.size(); ++i) {
            client::NetClient net;
            const auto session = attach(net);
            net.set_item_catalog(&data.items);
            queue(session,
                  protocol::BuffState::frame_add(protocol::kCbsSkillStone, 0, 0, indices[i]));
            c6_update(net);
            c6_observe(net, *session, "owned-icon-filename-" + std::to_string(i), snapshot);
            c6_expect_slot(c6_slot(net, protocol::kCbsSkillStone),
                           icons[i],
                           names[i],
                           indices[i],
                           0,
                           false,
                           0);
        }
        std::filesystem::create_directories(data.root + "/jobs");
        std::ofstream(data.root + "/jobs/knight.json")
            << R"({"class_name":"Owned Knight","titles":["Owned Knight I","Owned Knight II"]})";
        std::ofstream(data.root + "/jobs/mage.json")
            << R"({"class_name":"Owned Mage","titles":["Owned Mage I","Owned Mage II","Owned Mage III"]})";
        std::ofstream(data.root + "/jobs/archer.json")
            << R"({"class_name":"Owned Archer","titles":["Owned Archer I"]})";
        std::ofstream(data.root + "/jobs/thief.json")
            << R"({"class_name":"Owned Thief","titles":["Owned Thief I","Owned Thief II"]})";
        struct UnloadJobTitles {
            ~UnloadJobTitles() {
                (void) client::JobTitles::load({});
            }
        } unload;
        expect(client::JobTitles::load(data.root + "/jobs"),
               "C6 loads all four owned title tables without ambient catalog or missing-file logs");
        struct JobCase {
            int clazz;
            std::uint8_t specialty;
            int index;
            const char* title;
        };
        const std::array<JobCase, 8> cases{{{0, 0, 0, "Owned Knight I"},
                                            {0, 1, 0, "Owned Knight I"},
                                            {0, 3, 1, "Owned Knight II"},
                                            {0, 255, 7, "Owned Knight II"},
                                            {1, 3, 1, "Owned Mage II"},
                                            {1, 255, 7, "Owned Mage III"},
                                            {2, 255, 7, "Owned Archer I"},
                                            {3, 3, 1, "Owned Thief II"}}};
        for (std::size_t i = 0; i < cases.size(); ++i) {
            const auto& expected = cases[i];
            client::NetClient net;
            const auto session = attach(net);
            net.set_char_identity("Owned C6 title", expected.clazz, "");
            queue(session,
                  c6_message(static_cast<std::uint8_t>(protocol::InfoMsg::SpecialtyUp),
                             i % 2 != 0,
                             {expected.specialty}));
            c6_update(net);
            c6_observe(net, *session, "loaded-job-title-" + std::to_string(i), snapshot);
            const auto stats = net.char_info_stats();
            expect(
                net.specialty() == expected.specialty && stats.specialty_index == expected.index &&
                    stats.gov_pos == expected.title,
                "C6 real specialty packet chooses authored class title and clamps only table lookup");
        }
    }

    void test_c6_debug_chest_feedback(bool snapshot) {
        const bool previous = client::DebugOverlay::enabled();
        {
            struct RestoreDebug {
                bool previous;
                bool snapshot;
                ~RestoreDebug() {
                    client::DebugOverlay::set_enabled(previous);
                    if (snapshot) {
                        log::set_level(log::Level::warn);
                    }
                }
            } restore{previous, snapshot};
            client::DebugOverlay::set_enabled(true);
            if (snapshot) {
                log::set_level(log::Level::info);
            }
            client::NetClient net;
            const auto session = attach(net);
            const std::array<std::uint8_t, 4> messages{{87, 88, 89, 86}};
            for (std::size_t i = 0; i < messages.size(); ++i) {
                std::cout.flush();
                queue(session, c6_message(messages[i], true, {4}));
                c6_update(net);
                const auto q = c6_observe(
                    net, *session, "debug-chest-feedback-" + std::to_string(i), snapshot);
                expect(
                    client::DebugOverlay::enabled() &&
                        net.last_chest_feedback_msg() == messages[i] && q.chest_start == (i == 0) &&
                        q.chest_stop == (i != 0) && (i != 0 || q.chest_seconds == 4) &&
                        q.infos.empty(),
                    "C6 enabled diagnostic branch preserves each real chest start/stop ordinal without renderer calls");
            }
        }
        expect(client::DebugOverlay::enabled() == previous,
               "C6 chest diagnostic fixture restores the previous overlay flag");
    }
    void test_c6_reachable_status(bool snapshot) {
        test_c6_native_buff_golden(snapshot);
        test_c6_owned_catalog_reachable_paths(snapshot);
        expect(client::JobTitles::title(0, 0).empty() && client::JobTitles::title(1, 7).empty(),
               "C6 owned title fixture restores unloaded process-global tables");
        test_c6_debug_chest_feedback(snapshot);
    }
    // C6_REACHABLE_STATUS_GUARDS_END

    void test_c6_status_packets(bool snapshot) {
        expect(!client::JobTitles::load({}),
               "C6 fixtures explicitly use unloaded owned job-title state");
        test_c6_talent(snapshot);
        test_c6_add_remove(snapshot);
        test_c6_mstate_speed(snapshot);
        test_c6_feedback(snapshot);
        test_c6_catalog_and_optional_sources(snapshot);
        test_c6_payload_boundaries(snapshot);
        test_c6_lifetime_and_isolation(snapshot);
        test_c6_reachable_status(snapshot);
    }
    // C6_STATUS_PACKET_BASELINE_END

    // C7_COMBAT_PACKET_BASELINE_BEGIN
    using C7Json = platform::Json;
    std::vector<std::string> g_c7_checkpoint_labels;
    C7Json c7_text(std::string value) {
        C7Json out;
        out.set_type(C7Json::Type::String);
        out.string() = std::move(value);
        return out;
    }
    C7Json c7_array() {
        C7Json out;
        out.set_type(C7Json::Type::Array);
        return out;
    }
    template <typename Number> std::string c7_number(Number value) {
        if constexpr (std::is_floating_point_v<Number>) {
            std::ostringstream out;
            out << std::setprecision(std::numeric_limits<Number>::max_digits10) << value;
            return out.str();
        } else {
            return std::to_string(value);
        }
    }
    template <typename... Values> C7Json c7_fields(Values... values) {
        auto out = c7_array();
        (out.array().push_back(c7_text(c7_number(values))), ...);
        return out;
    }
    C7Json c7_encode(const client::PendingPlayerAttack& value) {
        return c7_fields(value.attacker_id, value.target_id);
    }
    C7Json c7_encode(const client::PendingHitFx& value) {
        return c7_fields(value.attacker_id, value.target_id, value.skill_index, value.flags);
    }
    C7Json c7_encode(const client::PendingDelayDamage& value) {
        return c7_fields(value.target_id, value.damage);
    }
    C7Json c7_encode(const client::PendingSkillAction& value) {
        return c7_fields(value.caster_id, value.skill, value.target_id, value.fire);
    }
    C7Json c7_encode(const client::PendingTransformAction& value) {
        return c7_fields(value.entity_id, value.pre, value.arg);
    }
    C7Json c7_encode(const protocol::PlayerEmote& value) {
        return c7_fields(value.player_id, value.motion);
    }
    C7Json c7_encode(const client::PendingCombatFloat& value) {
        auto out =
            c7_fields(static_cast<unsigned>(value.kind), value.anchor_id, value.wait_for_bolt);
        out.array().push_back(c7_text(value.text));
        return out;
    }
    C7Json c7_encode(const client::PendingInfoMessage& value) {
        auto out = c7_array();
        out.array().push_back(c7_text(value.text));
        out.array().push_back(value.kind ? c7_fields(static_cast<unsigned>(*value.kind))
                                         : C7Json{});
        out.array().push_back(
            value.color ? c7_fields((*value.color)[0], (*value.color)[1], (*value.color)[2])
                        : C7Json{});
        return out;
    }
    C7Json c7_encode(std::uint32_t value) {
        return c7_fields(value);
    }
    template <typename Value> C7Json c7_encode(const std::pair<std::uint32_t, Value>& value) {
        return c7_fields(value.first, value.second);
    }
    C7Json c7_encode(const client::PendingMonsterActionDamage& value) {
        return c7_fields(value.damage, value.flags, value.target_id);
    }
    C7Json c7_encode(const client::PendingChat& value) {
        auto out = c7_array();
        out.array().push_back(c7_text(value.speaker));
        out.array().push_back(c7_text(value.text));
        return out;
    }
    template <typename Value, typename Take> std::vector<Value> c7_drain(Take take, Value value) {
        std::vector<Value> result;
        for (unsigned watchdog = 0; watchdog < 4096; ++watchdog) {
            const auto before = C7Json::stringify(c7_encode(value), 0);
            if (!take(value)) {
                expect(
                    C7Json::stringify(c7_encode(value), 0) == before && !take(value) &&
                        C7Json::stringify(c7_encode(value), 0) == before,
                    "C7 drained FIFO returns false repeatedly without changing any output field");
                return result;
            }
            result.push_back(value);
        }
        expect(false, "C7 authored combat FIFOs stay within a bounded drain watchdog");
        return result;
    }
    struct C7MonsterObservation {
        client::NetMonster value;
        bool attack = false;
        bool die = false;
        bool knee = false;
        bool behead = false;
        bool action_pending = false;
        client::PendingMonsterActionDamage action{-123, 0xab, 0xf001};
    };
    struct C7Observation {
        std::vector<client::PendingPlayerAttack> attacks;
        std::vector<client::PendingHitFx> hits;
        std::vector<client::PendingCombatFloat> floats;
        std::vector<client::PendingDelayDamage> delays;
        std::vector<client::PendingInfoMessage> infos;
        std::vector<client::PendingSkillAction> skills;
        std::vector<client::PendingTransformAction> transforms;
        std::vector<protocol::PlayerEmote> emotes;
        std::vector<std::uint32_t> levels;
        std::vector<std::pair<std::uint32_t, std::uint16_t>> damaged;
        std::vector<std::pair<std::uint32_t, std::uint8_t>> charms;
        std::vector<client::PendingChat> chats;
        std::vector<C7MonsterObservation> monsters;
        bool local_skill = false;
        bool hit_pending = false;
        std::uint32_t hit_from = 0xf002;
        bool rest_pending = false;
        bool resting = true;
        int tflag = 0;
        std::int16_t speed = 0;
        std::int32_t local_hp = 0;
    };
    C7Observation c7_observe(client::NetClient& net,
                             const SessionState& session,
                             const std::string& label,
                             bool snapshot) {
        expect(std::find(g_c7_checkpoint_labels.begin(), g_c7_checkpoint_labels.end(), label) ==
                   g_c7_checkpoint_labels.end(),
               "C7 named observations are unique within this finite fixture run");
        g_c7_checkpoint_labels.push_back(label);
        C7Observation q;
        q.attacks = c7_drain([&](auto& out) { return net.take_player_attack(out); },
                             client::PendingPlayerAttack{0xf001, 0xf002});
        q.hits = c7_drain([&](auto& out) { return net.take_hit_fx(out); },
                          client::PendingHitFx{0xf001, 0xf002, -7, 0xab});
        q.floats = c7_drain([&](auto& out) { return net.take_combat_float(out); },
                            client::PendingCombatFloat{
                                client::CombatFloatKind::Heal, "sentinel float", 0xf001, true});
        q.delays = c7_drain([&](auto& out) { return net.take_delay_damage(out); },
                            client::PendingDelayDamage{0xf001, -77});
        q.infos = c7_drain([&](auto& out) { return net.take_info_message(out); },
                           client::PendingInfoMessage{"sentinel info",
                                                      protocol::BattleMsgKind::Mp,
                                                      std::array<float, 3>{.25f, .5f, .75f}});
        q.skills = c7_drain([&](auto& out) { return net.take_skill_action(out); },
                            client::PendingSkillAction{0xf001, 203, 0xf002, true});
        q.transforms = c7_drain([&](auto& out) { return net.take_transform_action(out); },
                                client::PendingTransformAction{0xf001, true, 203});
        q.emotes = c7_drain([&](auto& out) { return net.take_emote(out); },
                            protocol::PlayerEmote{0xf001, 203});
        q.levels = c7_drain([&](auto& out) { return net.take_level_up_effect(out); },
                            std::uint32_t{0xf001});
        q.damaged =
            c7_drain([&](auto& out) { return net.take_damaged_effect(out.first, out.second); },
                     std::pair<std::uint32_t, std::uint16_t>{0xf001, 0xabcd});
        q.charms = c7_drain([&](auto& out) { return net.take_charm_effect(out.first, out.second); },
                            std::pair<std::uint32_t, std::uint8_t>{0xf001, 203});
        q.chats = c7_drain([&](auto& out) { return net.take_chat(out); },
                           client::PendingChat{"sentinel speaker", "sentinel text"});
        q.local_skill = net.take_local_skill_cast();
        q.hit_pending = net.take_hit_from(q.hit_from);
        q.rest_pending = net.take_rest_state(q.resting);
        const auto saved_hit = q.hit_from;
        const auto saved_rest = q.resting;
        expect(!net.take_local_skill_cast() && !net.take_hit_from(q.hit_from) &&
                   q.hit_from == saved_hit && !net.take_rest_state(q.resting) &&
                   q.resting == saved_rest && (q.hit_pending || q.hit_from == 0xf002) &&
                   (q.rest_pending || q.resting),
               "C7 local skill/hit/rest latches consume once and failed takes preserve sentinels");
        q.tflag = net.transform_tflag();
        q.speed = net.local_mspeed();
        q.local_hp = net.char_info_stats().hp;
        std::vector<std::uint32_t> ids;
        for (const auto& entry : net.monsters()) {
            ids.push_back(entry.first);
        }
        std::sort(ids.begin(), ids.end());
        for (auto id : ids) {
            C7MonsterObservation m;
            m.value = net.monsters().at(id);
            m.attack = net.take_monster_attack(id);
            m.die = net.take_monster_die(id);
            m.knee = net.take_monster_knee(id);
            m.behead = net.take_monster_behead(id);
            m.action_pending = net.take_monster_action_damage(id, m.action);
            const auto action = C7Json::stringify(c7_encode(m.action), 0);
            expect(
                !net.take_monster_attack(id) && !net.take_monster_die(id) &&
                    !net.take_monster_knee(id) && !net.take_monster_behead(id) &&
                    !net.take_monster_action_damage(id, m.action) &&
                    C7Json::stringify(c7_encode(m.action), 0) == action &&
                    (m.action_pending || (m.action.damage == -123 && m.action.flags == 0xab &&
                                          m.action.target_id == 0xf001)) &&
                    net.monsters().at(id).finishing_death() == m.value.finishing_death(),
                "C7 independent monster presentation latches consume once without clearing death completion or outputs");
            q.monsters.push_back(std::move(m));
        }
        if (!snapshot) {
            return q;
        }
        C7Json record;
        record.set_type(C7Json::Type::Object);
        auto& out = record.object();
        out["checkpoint"] = c7_text(label);
        out["transport"] =
            c7_fields(net.connected(), session.alive, session.eof, session.closes, session.sends);
        out["sent"] = c7_array();
        for (auto byte : session.sent) {
            out["sent"].array().push_back(c7_fields(byte));
        }
        out["pending_receive_chunks"] = c7_array();
        for (const auto& chunk : session.chunks) {
            auto bytes = c7_array();
            for (auto byte : chunk) {
                bytes.array().push_back(c7_fields(byte));
            }
            out["pending_receive_chunks"].array().push_back(std::move(bytes));
        }
        const auto write = [&](const char* key, const auto& values) {
            out[key] = c7_array();
            for (const auto& value : values) {
                out[key].array().push_back(c7_encode(value));
            }
        };
        write("attacks", q.attacks);
        write("hits", q.hits);
        write("floats", q.floats);
        write("delays", q.delays);
        write("infos", q.infos);
        write("skills", q.skills);
        write("transforms", q.transforms);
        write("emotes", q.emotes);
        write("level_effects", q.levels);
        write("damaged_effects", q.damaged);
        write("charms", q.charms);
        write("chats", q.chats);
        out["local_latches"] =
            c7_fields(q.local_skill, q.hit_pending, q.hit_from, q.rest_pending, q.resting);
        out["local_state"] = c7_fields(q.tflag, q.speed, q.local_hp);
        out["entity_rest_states"] = c7_array();
        for (std::uint32_t id : {0u, 77u, 88u, 800u, 999u}) {
            out["entity_rest_states"].array().push_back(c7_fields(id, net.entity_resting(id)));
        }
        out["monsters"] = c7_array();
        for (const auto& observation : q.monsters) {
            const auto& m = observation.value;
            auto value = c7_array();
            value.array().push_back(c7_fields(m.id(),
                                              m.index(),
                                              m.race(),
                                              m.x(),
                                              m.y(),
                                              m.dir(),
                                              m.hp(),
                                              m.max_hp(),
                                              m.gstate()));
            value.array().push_back(c7_text(m.name()));
            value.array().push_back(c7_fields(m.wx(),
                                              m.wy(),
                                              m.wz(),
                                              m.tx(),
                                              m.tz(),
                                              m.yaw(),
                                              m.move_speed(),
                                              m.walk_speed(),
                                              m.run_speed(),
                                              m.speed_value()));
            value.array().push_back(c7_fields(m.moving(),
                                              m.has_abs_target(),
                                              m.stop_on_arrive(),
                                              m.running(),
                                              m.ice_slowed(),
                                              m.move_stopped(),
                                              m.attack_after_move(),
                                              m.server_removed(),
                                              m.finishing_death()));
            value.array().push_back(c7_fields(observation.attack,
                                              observation.die,
                                              observation.knee,
                                              observation.behead,
                                              observation.action_pending));
            value.array().push_back(c7_encode(observation.action));
            out["monsters"].array().push_back(std::move(value));
        }
        std::cout << "c7 " << C7Json::stringify(record, 0) << std::endl;
        return q;
    }
    Bytes c7_payload(const Bytes& frame) {
        expect(frame.size() >= 3, "C7 authored frame includes its real outer header");
        return frame.size() >= 3 ? Bytes(frame.begin() + 3, frame.end()) : Bytes{};
    }
    Bytes c7_frame(protocol::S2C type, Bytes body) {
        protocol::PacketWriter writer;
        writer.payload() = std::move(body);
        return writer.frame(static_cast<std::uint8_t>(type));
    }
    Bytes c7_monster_frame(std::uint32_t id, int x, int y, int hp = 100) {
        server::MonsterEntity monster;
        monster.id = id;
        monster.def_index = 501;
        monster.name = "Owned C7 monster";
        monster.x = x;
        monster.y = y;
        monster.hp = hp;
        monster.max_hp = 100;
        return server::frame_create_monster(monster);
    }
    Bytes c7_player_frame(std::uint32_t id, int clazz, bool self, int x = 1000, int y = 1000) {
        const std::uint16_t wear[7]{};
        return protocol::CreatePlayer::frame(id,
                                             "Owned C7 player",
                                             static_cast<std::uint8_t>(clazz | (self ? 0x80 : 0)),
                                             x,
                                             y,
                                             0,
                                             0,
                                             0,
                                             wear,
                                             0,
                                             0);
    }
    struct C7Fixture {
        client::NetClient net;
        std::shared_ptr<SessionState> session;
        int origin = 32;
        explicit C7Fixture(bool seeded = true,
                           int clazz = 0,
                           bool read_only = false,
                           int patch = 32)
            : session(attach(net)), origin(patch) {
            net.set_read_only(read_only);
            net.set_char_identity("Owned C7 self", clazz);
            queue(session, server::frame_s2c_code(12345));
            expect(net.handshake(),
                   "C7 fixture completes the real public BufferedSession handshake");
            if (seeded) {
                queue(session, c7_player_frame(77, clazz, true));
                queue(session, c7_player_frame(88, 1, false, 1300, 1000));
                queue(session, c7_monster_frame(800, 1100, 1000));
                queue(session, c7_monster_frame(801, 1200, 1000));
                queue(session,
                      protocol::Property::frame_update_vital(
                          static_cast<std::uint8_t>(protocol::PropertyType::Hp), 100, 200));
                net.update(0, origin);
            }
            net.sync_local_move(coords::to_client(1000, origin),
                                0,
                                coords::to_client(1000, origin),
                                false,
                                0,
                                origin,
                                true);
        }
        ~C7Fixture() {
            if (net.connected() && net.handshook()) {
                queue(session, protocol::GameExit::frame_server_answer(true));
            }
            net.close();
        }
    };
    const C7MonsterObservation* c7_monster(const C7Observation& q, std::uint32_t id) {
        const auto found = std::find_if(q.monsters.begin(), q.monsters.end(), [id](const auto& m) {
            return m.value.id() == id;
        });
        return found == q.monsters.end() ? nullptr : &*found;
    }
    C7Observation
    c7_marked(C7Fixture& f, const Bytes& packet, const std::string& label, bool snapshot) {
        queue(f.session, protocol::Chatting::frame_s2c("C7", "before"));
        queue(f.session, packet);
        queue(f.session, protocol::Chatting::frame_s2c("C7", "after"));
        f.net.update(0, f.origin);
        auto q = c7_observe(f.net, *f.session, label, snapshot);
        expect(f.net.connected() && f.session->closes == 0 && q.chats.size() == 2 &&
                   q.chats[0].speaker == "C7" && q.chats[0].text == "before" &&
                   q.chats[1].speaker == "C7" && q.chats[1].text == "after",
               "C7 recognized valid or malformed frame drains its exact same-batch chat neighbors");
        return q;
    }
    bool c7_events_empty(const C7Observation& q) {
        return q.attacks.empty() && q.hits.empty() && q.floats.empty() && q.delays.empty() &&
               q.infos.empty() && q.skills.empty() && q.transforms.empty() && q.emotes.empty() &&
               q.levels.empty() && q.damaged.empty() && q.charms.empty() && !q.local_skill &&
               !q.hit_pending && !q.rest_pending;
    }

    void test_c7_malformed_payloads(bool snapshot) {
        struct Layout {
            protocol::S2C type;
            Bytes frame;
            std::size_t first;
            std::size_t end;
            const char* name;
        };
        const std::array<Layout, 7> layouts{
            {{protocol::S2C::Attack,
              protocol::Attack::frame_server(77, 801, 12, 5, protocol::kAtkHit),
              0,
              17,
              "attack"},
             {protocol::S2C::Skill,
              protocol::Skill::frame_server(4, 77, 801, 1),
              0,
              11,
              "skill-no-damage"},
             {protocol::S2C::Skill,
              protocol::Skill::frame_server_damage(4, 77, 801, 1, 12, 5, protocol::kAtkHit),
              11,
              20,
              "skill-damage"},
             {protocol::S2C::Effect,
              protocol::Effect::frame(800, static_cast<std::uint8_t>(protocol::EffectType::Hp)),
              0,
              5,
              "effect"},
             {protocol::S2C::Effect,
              protocol::Effect::frame_damaged(800, 65535),
              5,
              7,
              "damaged-effect"},
             {protocol::S2C::Action, protocol::Skill::frame_action_rest(77, 1), 0, 5, "action"},
             {protocol::S2C::Action,
              protocol::Skill::frame_action_skill(800, 4, 77),
              5,
              10,
              "action-skill-fallback"}}};
        for (const auto& layout : layouts) {
            const auto full = c7_payload(layout.frame);
            for (std::size_t size = layout.first; size < layout.end; ++size) {
                C7Fixture f;
                const auto q = c7_marked(
                    f,
                    c7_frame(layout.type,
                             Bytes(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(size))),
                    std::string("malformed-") + layout.name + "-" + std::to_string(size),
                    snapshot);
                const auto* attacker = c7_monster(q, 800);
                const auto* victim = c7_monster(q, 801);
                expect(
                    c7_events_empty(q) && q.monsters.size() == 2 && attacker && victim &&
                        attacker->value.hp() == 100 && victim->value.hp() == 100 &&
                        attacker->value.yaw() == 0 && victim->value.yaw() == 0 &&
                        !attacker->attack && !attacker->die && !attacker->knee &&
                        !attacker->behead && !attacker->action_pending && !victim->action_pending &&
                        q.local_hp == 100 && q.tflag == 0 && q.speed == 0,
                    "C7 truncated combat layouts and short skill-action fallbacks commit no partial state or event");
            }
        }
    }
    void test_c7_strict_emotes(bool snapshot) {
        protocol::PacketWriter writer;
        writer.write_u32(999);
        writer.write_u8(10);
        const auto full = writer.payload();
        std::vector<std::pair<std::string, Bytes>> cases;
        for (std::size_t size = 0; size < 5; ++size) {
            cases.emplace_back(
                "short-" + std::to_string(size),
                Bytes(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(size)));
        }
        auto body = full;
        body.push_back(0);
        cases.emplace_back("tail", body);
        body = full;
        std::fill(body.begin(), body.begin() + 4, 0);
        cases.emplace_back("zero-player", body);
        for (std::uint8_t motion : {std::uint8_t{0}, std::uint8_t{21}, std::uint8_t{255}}) {
            body = full;
            body[4] = motion;
            cases.emplace_back("motion-" + std::to_string(motion), body);
        }
        for (const auto& entry : cases) {
            C7Fixture f;
            queue(f.session, protocol::Emote::frame_server(991, 1));
            const auto target = c7_frame(protocol::S2C::PlayerAnimation, entry.second);
            queue(f.session, target);
            queue(f.session, protocol::Emote::frame_server(992, 20));
            f.net.update(0, f.origin);
            const auto q = c7_observe(f.net, *f.session, "emote-invalid-" + entry.first, snapshot);
            expect(
                f.net.connected() && f.session->closes == 0 && q.emotes.size() == 2 &&
                    q.emotes[0].player_id == 991 && q.emotes[0].motion == 1 &&
                    q.emotes[1].player_id == 992 && q.emotes[1].motion == 20 && q.attacks.empty() &&
                    q.hits.empty() && q.skills.empty() && q.infos.empty(),
                "C7 strict invalid animation never replaces either valid identity/motion neighbor");
        }
        C7Fixture f;
        queue(f.session, protocol::Emote::frame_server(991, 1));
        queue(f.session, protocol::Emote::frame_server(999, 10));
        queue(f.session, protocol::Emote::frame_server(992, 20));
        f.net.update(0, f.origin);
        const auto q = c7_observe(f.net, *f.session, "emote-valid-middle", snapshot);
        expect(q.emotes.size() == 3 && q.emotes[1].player_id == 999 && q.emotes[1].motion == 10,
               "C7 valid unknown player identities retain exact animation FIFO order");
    }
    void test_c7_accepted_tails(bool snapshot) {
        const std::array<Bytes, 7> frames{
            {protocol::Attack::frame_server(77, 801, 12, 5, protocol::kAtkHit),
             protocol::Skill::frame_server(4, 88, 801, 1),
             protocol::Skill::frame_server_damage(4, 77, 801, 1, 12, 5, protocol::kAtkHit),
             protocol::Effect::frame(800, static_cast<std::uint8_t>(protocol::EffectType::Hp)),
             protocol::Effect::frame_damaged(800, 65535),
             protocol::Skill::frame_action_rest(77, 1),
             protocol::Skill::frame_action_skill(800, 4, 77)}};
        const std::array<protocol::S2C, 7> types{{protocol::S2C::Attack,
                                                  protocol::S2C::Skill,
                                                  protocol::S2C::Skill,
                                                  protocol::S2C::Effect,
                                                  protocol::S2C::Effect,
                                                  protocol::S2C::Action,
                                                  protocol::S2C::Action}};
        for (std::size_t kind = 0; kind < frames.size(); ++kind) {
            C7Fixture f;
            auto body = c7_payload(frames[kind]);
            body.push_back(0xab);
            const auto q = c7_marked(f,
                                     c7_frame(types[kind], std::move(body)),
                                     "accepted-tail-" + std::to_string(kind),
                                     snapshot);
            const auto* victim = c7_monster(q, 801);
            expect(victim && q.local_hp == 100 && q.transforms.empty() && q.emotes.empty() &&
                       q.levels.empty(),
                   "C7 accepted tail preserves independent scalar and unrelated event domains");
            if (kind == 0 || kind == 2) {
                expect(
                    victim && victim->value.hp() == 88 && q.hits.size() == 1 &&
                        q.hits[0].attacker_id == 77 && q.hits[0].target_id == 801 &&
                        q.hits[0].skill_index == (kind == 0 ? -1 : 4) &&
                        q.hits[0].flags == (protocol::kAtkHit | protocol::kAtkExplosive) &&
                        q.infos.size() == 2 && q.floats.size() == 2 && q.floats[0].text == "7" &&
                        q.floats[1].text == "5" && q.local_skill == (kind == 2),
                    "C7 accepted attack/damage-skill tail retains full damage components and acknowledgement policy");
            } else if (kind == 1 || kind == 6) {
                expect(
                    q.skills.size() == 1 && q.skills[0].caster_id == (kind == 1 ? 88u : 800u) &&
                        q.skills[0].skill == 4 &&
                        q.skills[0].target_id == (kind == 1 ? 801u : 77u) &&
                        q.skills[0].fire == (kind == 1) && q.hits.empty() && q.infos.empty(),
                    "C7 accepted action-skill and no-damage fire tails keep preparation/fire distinction");
            } else if (kind == 3) {
                expect(q.charms ==
                               std::vector<std::pair<std::uint32_t, std::uint8_t>>{
                                   {800, static_cast<std::uint8_t>(protocol::EffectType::Hp)}} &&
                           q.damaged.empty(),
                       "C7 generic effect tail retains the entire entity/effect pair");
            } else if (kind == 4) {
                expect(
                    q.damaged ==
                            std::vector<std::pair<std::uint32_t, std::uint16_t>>{{800, 65535}} &&
                        q.charms.empty(),
                    "C7 damaged tail retains full unsigned16 damage without generic charm output");
            } else {
                expect(q.rest_pending && q.resting && q.skills.empty() && q.infos.empty(),
                       "C7 generic action tail publishes exact local rest state");
            }
        }
        C7Fixture f;
        auto body = c7_payload(
            protocol::Skill::frame_server_damage(4, 77, 801, 1, 12, 5, protocol::kAtkHit));
        body[9] = 2;
        const auto q = c7_marked(f,
                                 c7_frame(protocol::S2C::Skill, std::move(body)),
                                 "skill-nonzero-damage-byte",
                                 snapshot);
        expect(
            q.local_skill && q.hits.size() == 1 && q.hits[0].skill_index == 4 &&
                q.floats.size() == 2,
            "C7 damage-present byte2 follows the existing nonzero policy rather than a strict boolean");
    }

    void c7_feedback(const C7Observation& q,
                     const std::vector<client::PendingInfoMessage>& infos,
                     const std::vector<client::PendingCombatFloat>& floats) {
        expect(q.infos.size() == infos.size() && q.floats.size() == floats.size(),
               "C7 damage feedback has exact info/float counts");
        for (std::size_t i = 0; i < std::min(q.infos.size(), infos.size()); ++i) {
            expect(C7Json::stringify(c7_encode(q.infos[i]), 0) ==
                       C7Json::stringify(c7_encode(infos[i]), 0),
                   "C7 damage info preserves literal text, kind, RGB and FIFO order");
        }
        for (std::size_t i = 0; i < std::min(q.floats.size(), floats.size()); ++i) {
            expect(C7Json::stringify(c7_encode(q.floats[i]), 0) ==
                       C7Json::stringify(c7_encode(floats[i]), 0),
                   "C7 damage float preserves kind, text, anchor, delay and FIFO order");
        }
    }
    bool c7_yaw(float actual, float expected) {
        return std::abs(actual - expected) < .00001f;
    }
    void test_c7_attack_animation(bool snapshot) {
        for (unsigned kind = 0; kind < 3; ++kind) {
            C7Fixture f(true, 0, kind == 2);
            const auto attacker = kind == 0 ? 88u : 77u;
            const auto q = c7_marked(f,
                                     protocol::Attack::frame_server(attacker, 801, 0, 0, 0),
                                     "attack-player-animation-" + std::to_string(kind),
                                     snapshot);
            expect(
                q.attacks.size() == (kind == 1 ? 0u : 1u) && q.hits.empty() && q.infos.empty() &&
                    q.floats.empty() &&
                    (q.attacks.empty() ||
                     (q.attacks[0].attacker_id == attacker && q.attacks[0].target_id == 801)),
                "C7 remote and read-only self attacks animate misses while ordinary self does not");
        }
        C7Fixture f;
        queue(f.session, server::frame_move_monster_on(800, 100, 0, true));
        auto q = c7_marked(f,
                           protocol::Attack::frame_server(800, 77, 0, 0, 0),
                           "attack-after-live-step",
                           snapshot);
        const auto* m = c7_monster(q, 800);
        expect(m && !m->attack && m->value.attack_after_move() && m->value.has_abs_target() &&
                   m->value.moving() && m->action_pending && m->action.damage == 0 &&
                   m->action.target_id == 77,
               "C7 monster attack during a distant live step defers its attack latch");
        f.net.update(5, f.origin);
        q = c7_observe(f.net, *f.session, "attack-arrival-latch", snapshot);
        m = c7_monster(q, 800);
        expect(m && m->attack && !m->value.attack_after_move() && !m->value.has_abs_target() &&
                   !m->value.moving() && m->value.wx() == m->value.tx() &&
                   m->value.wz() == m->value.tz(),
               "C7 fixed-delta arrival publishes one deferred monster attack and stops its step");
        C7Fixture dual;
        queue(dual.session, c7_monster_frame(88, 1100, 1000));
        q = c7_marked(
            dual,
            protocol::Attack::frame_server(
                88, 801, 12, 5, protocol::kAtkAnim | protocol::kAtkHit | protocol::kAtkFatal),
            "attack-animation-only-order",
            snapshot);
        const auto* attacker = c7_monster(q, 88);
        const auto* victim = c7_monster(q, 801);
        expect(
            attacker && victim && attacker->attack &&
                c7_yaw(attacker->value.yaw(), -1.57079632679f) && victim->value.yaw() == 0 &&
                victim->value.hp() == 100 && !attacker->action_pending && q.attacks.size() == 1 &&
                q.attacks[0].attacker_id == 88 && q.attacks[0].target_id == 801 && q.hits.empty() &&
                q.infos.empty() && q.floats.empty() && q.delays.empty() && !q.hit_pending,
            "C7 animation-only attack faces and animates its known attacker before suppressing victim-facing and every damage effect");
        C7Fixture incoming;
        q = c7_marked(
            incoming,
            protocol::Attack::frame_server(800, 77, 12, 5, protocol::kAtkAnim | protocol::kAtkHit),
            "attack-animation-only-incoming",
            snapshot);
        attacker = c7_monster(q, 800);
        expect(
            attacker && attacker->attack && !attacker->action_pending && !q.hit_pending &&
                q.infos.empty() && q.floats.empty() && q.hits.empty() && q.local_hp == 100,
            "C7 animation-only incoming attack never publishes hit-from, action damage or HP feedback");
    }
    void test_c7_attack_damage(bool snapshot) {
        struct DamageCase {
            std::int32_t damage;
            std::int32_t palsy;
            std::uint8_t flags;
            std::int32_t hp;
            std::vector<client::PendingInfoMessage> infos;
            std::vector<client::PendingCombatFloat> floats;
        };
        const auto normal = protocol::BattleMsgKind::Damage;
        const std::array<float, 3> critical{{1.f, .1f, .1f}};
        const std::vector<DamageCase> cases{
            {12,
             5,
             protocol::kAtkHit,
             88,
             {{"You dealt 7 damage.", normal, std::nullopt},
              {"You gave 5 of Explosive Blow to the enemy.", normal, std::nullopt}},
             {{client::CombatFloatKind::DamageDealt, "7", 801, false},
              {client::CombatFloatKind::DamageDealt, "5", 801, false}}},
            {12,
             -3,
             protocol::kAtkHit | protocol::kAtkFatal,
             88,
             {{"You gave critical damage of 12 to the enemy.", normal, critical}},
             {{client::CombatFloatKind::DamageDealt, "12", 801, false}}},
            {3,
             7,
             protocol::kAtkHit,
             97,
             {{"You gave 7 of Explosive Blow to the enemy.", normal, std::nullopt}},
             {{client::CombatFloatKind::DamageDealt, "7", 801, false}}},
            {150,
             0,
             protocol::kAtkHit,
             0,
             {{"You dealt 150 damage.", normal, std::nullopt}},
             {{client::CombatFloatKind::DamageDealt, "150", 801, false}}},
            {-5, 0, protocol::kAtkHit, 105, {}, {}}};
        for (std::size_t kind = 0; kind < cases.size(); ++kind) {
            C7Fixture f;
            const auto& value = cases[kind];
            const auto q = c7_marked(
                f,
                protocol::Attack::frame_server(77, 801, value.damage, value.palsy, value.flags),
                "attack-damage-" + std::to_string(kind),
                snapshot);
            const auto* victim = c7_monster(q, 801);
            expect(
                victim && victim->value.hp() == value.hp && victim->value.max_hp() == 100 &&
                    q.hits.size() == 1 && q.hits[0].attacker_id == 77 &&
                    q.hits[0].target_id == 801 && q.hits[0].skill_index == -1 &&
                    q.hits[0].flags ==
                        (value.flags | (value.palsy > 0 ? protocol::kAtkExplosive : 0)) &&
                    q.attacks.empty() && q.delays.empty() && !q.local_skill && q.local_hp == 100,
                "C7 ordinary damage keeps signed HP behavior, hit identity and independent local HP");
            c7_feedback(q, value.infos, value.floats);
        }
        for (auto target : {88u, 999u}) {
            C7Fixture f;
            const auto q =
                c7_marked(f,
                          protocol::Attack::frame_server(77, target, 12, 5, protocol::kAtkHit),
                          "attack-nonmonster-target-" + std::to_string(target),
                          snapshot);
            expect(
                q.hits.empty() && q.floats.empty() && q.monsters.size() == 2,
                "C7 ordinary attack to a player or unknown target produces no monster float or hit effect");
            c7_feedback(q,
                        {{"You dealt 7 damage.", normal, std::nullopt},
                         {"You gave 5 of Explosive Blow to the enemy.", normal, std::nullopt}},
                        {});
        }
        for (int damage : {10, 0, -5}) {
            C7Fixture f;
            const auto q =
                c7_marked(f,
                          protocol::Attack::frame_server(
                              800, 77, damage, 3, protocol::kAtkHit | protocol::kAtkFatal),
                          "attack-incoming-" + std::to_string(damage),
                          snapshot);
            const auto* m = c7_monster(q, 800);
            expect(
                q.hit_pending && q.hit_from == 800 && q.local_hp == 100 && m && m->attack &&
                    m->action_pending && m->action.damage == damage &&
                    m->action.flags ==
                        (protocol::kAtkHit | protocol::kAtkFatal | protocol::kAtkExplosive) &&
                    m->action.target_id == 77 && q.floats.empty(),
                "C7 incoming Attack publishes hit-from and complete raw monster action damage even without positive damage");
            if (damage == 10) {
                c7_feedback(
                    q, {{"You got critical damage of 10 from the enemy.", normal, critical}}, {});
            } else {
                c7_feedback(q, {}, {});
            }
        }
        C7Fixture latest;
        queue(latest.session, protocol::Attack::frame_server(800, 77, 0, 0, protocol::kAtkBlock));
        const auto q = c7_marked(latest,
                                 protocol::Attack::frame_server(999, 77, -7, 0, 0),
                                 "attack-latest-hit-from",
                                 snapshot);
        const auto* m = c7_monster(q, 800);
        expect(
            q.hit_pending && q.hit_from == 999 && m && m->action_pending && m->action.damage == 0 &&
                m->action.flags == protocol::kAtkBlock && m->action.target_id == 77 &&
                q.infos.empty(),
            "C7 latest hit-from coalesces independently from a known attacker's retained action-damage latch");
        C7Fixture pair;
        const auto pairq =
            c7_marked(pair,
                      protocol::Attack::frame_server(800, 801, 12, 3, protocol::kAtkHit),
                      "attack-monster-target-no-action-damage",
                      snapshot);
        m = c7_monster(pairq, 800);
        const auto* victim = c7_monster(pairq, 801);
        expect(
            m && victim && m->attack && !m->action_pending && victim->value.hp() == 88 &&
                pairq.hits.size() == 1 && pairq.infos.empty(),
            "C7 monster-to-monster Attack applies HP/hit while omitting player-target action damage");
    }
    void test_c7_attack_archer_identity(bool snapshot) {
        struct Identity {
            bool local;
            int clazz;
            std::uint8_t flags;
            std::uint32_t target;
            bool hit;
        };
        const std::array<Identity, 7> cases{
            {{true, 0, 0, 801, false},
             {true, 2, 0, 801, true},
             {false, 2, 0, 801, true},
             {false, 0, 0, 801, false},
             {true, 0, protocol::kAtkHit, 801, true},
             {true, 2, protocol::kAtkAnim | protocol::kAtkHit, 801, false},
             {true, 2, 0, 999, false}}};
        for (std::size_t kind = 0; kind < cases.size(); ++kind) {
            const auto& value = cases[kind];
            C7Fixture f(true, value.clazz);
            if (!value.local) {
                queue(f.session, c7_player_frame(88, value.clazz, false, 1300, 1000));
            }
            const auto attacker = value.local ? 77u : 88u;
            const auto q =
                c7_marked(f,
                          protocol::Attack::frame_server(attacker, value.target, 0, 0, value.flags),
                          "attack-archer-identity-" + std::to_string(kind),
                          snapshot);
            expect(
                q.hits.size() == (value.hit ? 1u : 0u) &&
                    q.attacks.size() == (value.local ? 0u : 1u) &&
                    (q.hits.empty() ||
                     (q.hits[0].attacker_id == attacker && q.hits[0].target_id == value.target &&
                      q.hits[0].flags == value.flags)) &&
                    q.infos.empty(),
                "C7 Archer misses reserve known-monster feedback by actual local or remote class without bypassing animation-only or unknown-target gates");
        }
    }
    void test_c7_facing_order(bool snapshot) {
        for (bool correction_first : {false, true}) {
            C7Fixture f(true, 0, false, 31);
            queue(f.session, c7_player_frame(88, 1, false, 1050, 1000));
            f.net.update(0, f.origin);
            const auto correction = server::frame_move_player_on(88, 100, 0, 0);
            const auto attack = protocol::Attack::frame_server(800, 88, 0, 0, 0);
            queue(f.session, correction_first ? correction : attack);
            queue(f.session, correction_first ? attack : correction);
            f.net.update(0, f.origin);
            const auto q = c7_observe(f.net,
                                      *f.session,
                                      correction_first ? "facing-correction-then-attack"
                                                       : "facing-attack-then-correction",
                                      snapshot);
            const auto* m = c7_monster(q, 800);
            expect(
                m && c7_yaw(m->value.yaw(), correction_first ? -1.57079632679f : 1.57079632679f) &&
                    m->attack && m->action_pending && m->action.target_id == 88,
                "C7 facing uses remote authoritative coordinates in actual same-batch order at a nondefault origin");
        }
        for (unsigned kind = 0; kind < 4; ++kind) {
            C7Fixture f;
            std::uint32_t target = kind == 0 ? 0u : kind == 1 ? 999u : 801u;
            if (kind == 2) {
                queue(f.session, c7_monster_frame(801, 1100, 1000));
            } else if (kind == 3) {
                queue(f.session, c7_monster_frame(88, 1000, 1000));
                target = 88;
            }
            const auto q = c7_marked(f,
                                     protocol::Attack::frame_server(800, target, 0, 0, 0),
                                     "facing-target-priority-" + std::to_string(kind),
                                     snapshot);
            const auto* m = c7_monster(q, 800);
            expect(
                m && (kind == 3 ? c7_yaw(m->value.yaw(), -1.57079632679f) : m->value.yaw() == 0),
                "C7 unknown/overlapping targets retain yaw while remote-player identity takes priority over a same-ID monster");
        }
        C7Fixture f;
        queue(f.session, c7_player_frame(77, 0, true, 1300, 1000));
        const auto q = c7_marked(f,
                                 protocol::Attack::frame_server(800, 77, 0, 0, 0),
                                 "facing-local-display-priority",
                                 snapshot);
        const auto* m = c7_monster(q, 800);
        expect(
            m && c7_yaw(m->value.yaw(), 1.57079632679f),
            "C7 ordinary self facing keeps public displayed local coordinates separate from a newer server snapshot");
    }

    void test_c7_skill_acknowledgement(bool snapshot) {
        C7Fixture f;
        expect(f.net.send_skill_target_monster(4, 801),
               "C7 skill expectation fixture sends through the public gated request");
        auto q = c7_marked(
            f, protocol::Skill::frame_server(4, 88, 801, 1), "skill-remote-does-not-ack", snapshot);
        expect(!q.local_skill && q.skills.size() == 1 && q.skills[0].caster_id == 88 &&
                   q.skills[0].fire,
               "C7 an identical remote cast never acknowledges the local request");
        q = c7_marked(f,
                      protocol::Skill::frame_server(5, 77, 801, 1),
                      "skill-self-wrong-skill-acks",
                      snapshot);
        expect(q.local_skill && q.skills.empty(),
               "C7 nonzero self identity acknowledges even an unexpected skill");
        q = c7_marked(f,
                      protocol::Skill::frame_server(4, 77, 800, 1),
                      "skill-self-wrong-target-acks",
                      snapshot);
        expect(q.local_skill && q.skills.empty(),
               "C7 nonzero self identity acknowledges even an unexpected target");
        queue(f.session, protocol::Skill::frame_server(6, 77, 801, 1));
        q = c7_marked(f,
                      protocol::Skill::frame_server_damage(4, 77, 801, 1, 0, 0, protocol::kAtkHit),
                      "skill-self-ack-coalesces",
                      snapshot);
        expect(
            q.local_skill && q.skills.empty() && q.hits.size() == 1,
            "C7 repeated self echoes coalesce only their local acknowledgement while typed effects remain observable");
        C7Fixture observer(true, 0, true);
        q = c7_marked(observer,
                      protocol::Skill::frame_server(4, 77, 801, 1),
                      "skill-read-only-self-fire",
                      snapshot);
        expect(
            q.local_skill && q.skills.size() == 1 && q.skills[0].caster_id == 77 &&
                q.skills[0].skill == 4 && q.skills[0].target_id == 801 && q.skills[0].fire,
            "C7 read-only self casts use the ordinary fire-presentation queue as well as acknowledgement");
        C7Fixture unknown(false);
        queue(unknown.session, c7_monster_frame(801, 1200, 1000));
        q = c7_marked(unknown,
                      protocol::Skill::frame_server_damage(4, 0, 801, 1, 5, 0, protocol::kAtkHit),
                      "skill-zero-caster-independent-damage",
                      snapshot);
        const auto* victim = c7_monster(q, 801);
        expect(
            !q.local_skill && q.skills.empty() && q.hits.size() == 1 &&
                q.hits[0].attacker_id == 0 && victim && victim->value.hp() == 95,
            "C7 zero caster creates no acknowledgement/fire while independent permitted HP/hit effects remain");
    }
    void test_c7_skill_damage_and_delay(bool snapshot) {
        struct Value {
            std::uint8_t skill;
            std::uint32_t caster;
            std::uint32_t target;
            int damage;
            int hp;
            bool delay;
            bool fire;
        };
        const std::array<Value, 7> cases{{{2, 88, 801, 12, 88, true, true},
                                          {25, 0, 999, 12, 100, true, false},
                                          {4, 88, 801, 0, 100, false, true},
                                          {4, 88, 801, -5, 105, false, true},
                                          {4, 88, 999, -5, 100, false, true},
                                          {2, 88, 801, 0, 100, false, true},
                                          {2, 88, 801, -5, 105, false, true}}};
        for (std::size_t kind = 0; kind < cases.size(); ++kind) {
            C7Fixture f;
            const auto& value = cases[kind];
            const auto q = c7_marked(
                f,
                protocol::Skill::frame_server_damage(
                    value.skill, value.caster, value.target, 1, value.damage, 3, protocol::kAtkHit),
                "skill-hp-delay-" + std::to_string(kind),
                snapshot);
            const auto* victim = c7_monster(q, 801);
            expect(
                victim && victim->value.hp() == value.hp && victim->value.max_hp() == 100 &&
                    q.skills.size() == (value.fire ? 1u : 0u) &&
                    q.delays.size() == (value.delay ? 1u : 0u) &&
                    (q.delays.empty() || (q.delays[0].target_id == value.target &&
                                          q.delays[0].damage == value.damage)) &&
                    q.hits.size() == 1 && q.hits[0].attacker_id == value.caster &&
                    q.hits[0].target_id == value.target && q.hits[0].skill_index == value.skill &&
                    q.hits[0].flags == (protocol::kAtkHit | protocol::kAtkExplosive) &&
                    q.infos.empty() && q.floats.empty() && !q.local_skill && q.local_hp == 100,
                "C7 skill HP/hit and positive full delayed damage follow separate gates even for unknown targets or nonpositive damage");
        }
        for (int damage : {10, 0, -5}) {
            C7Fixture f;
            queue(f.session, protocol::Attack::frame_server(999, 77, 0, 0, 0));
            const auto q =
                c7_marked(f,
                          protocol::Skill::frame_server_damage(
                              4, 800, 77, 1, damage, 0, protocol::kAtkHit | protocol::kAtkFatal),
                          "skill-incoming-" + std::to_string(damage),
                          snapshot);
            expect(
                q.hit_pending && q.hit_from == (damage > 0 ? 800u : 999u) && q.local_hp == 100 &&
                    q.skills.size() == 1 && q.skills[0].caster_id == 800 && q.skills[0].fire &&
                    q.hits.size() == 1 && q.hits[0].target_id == 77 && !q.local_skill,
                "C7 incoming Skill replaces an unread hit-from only for positive damage while hit effects retain their independent gate");
            if (damage == 10) {
                c7_feedback(q,
                            {{"You got critical damage of 10 from the enemy.",
                              protocol::BattleMsgKind::Damage,
                              std::array<float, 3>{1.f, .1f, .1f}}},
                            {{client::CombatFloatKind::DamageTaken, "10", 0, false}});
            } else {
                c7_feedback(q, {}, {});
            }
        }
        for (auto caster : {88u, 999u}) {
            C7Fixture f;
            const auto q = c7_marked(
                f,
                protocol::Skill::frame_server_damage(4, caster, 77, 1, 10, 0, protocol::kAtkHit),
                "skill-incoming-nonmonster-" + std::to_string(caster),
                snapshot);
            expect(
                q.hit_pending && q.hit_from == caster && q.floats.empty() && q.skills.size() == 1,
                "C7 incoming known-player and unknown casters do not create the monster-only damage-taken float");
            c7_feedback(
                q,
                {{"You received 10 damage.", protocol::BattleMsgKind::Damage, std::nullopt}},
                {});
        }
    }
    void test_c7_local_skill_floats(bool snapshot) {
        struct Value {
            std::uint8_t skill;
            bool wait;
            bool delay;
            std::uint32_t target;
        };
        const std::array<Value, 4> cases{{{2, true, true, 801},
                                          {25, true, true, 801},
                                          {4, false, false, 801},
                                          {255, false, false, 999}}};
        for (const auto& value : cases) {
            C7Fixture f(true, 1);
            const auto q = c7_marked(
                f,
                protocol::Skill::frame_server_damage(value.skill,
                                                     77,
                                                     value.target,
                                                     1,
                                                     12,
                                                     5,
                                                     protocol::kAtkHit | protocol::kAtkFatal),
                "skill-mage-float-" + std::to_string(value.skill),
                snapshot);
            expect(
                q.local_skill && q.skills.empty() && q.hits.size() == 1 &&
                    q.hits[0].skill_index == value.skill &&
                    q.hits[0].flags ==
                        (protocol::kAtkHit | protocol::kAtkFatal | protocol::kAtkExplosive) &&
                    q.delays.size() == (value.delay ? 1u : 0u) &&
                    (q.delays.empty() ||
                     (q.delays[0].damage == 12 && q.delays[0].target_id == value.target)),
                "C7 immutable Mage lookup keeps separate bolt-wait and full delayed-damage decisions");
            c7_feedback(q,
                        {{"You gave critical damage of 7 to the enemy.",
                          protocol::BattleMsgKind::Damage,
                          std::array<float, 3>{1.f, .1f, .1f}},
                         {"You gave 5 of Explosive Blow to the enemy.",
                          protocol::BattleMsgKind::Damage,
                          std::nullopt}},
                        {{client::CombatFloatKind::DamageDealt, "7", value.target, value.wait},
                         {client::CombatFloatKind::DamageDealt, "5", value.target, value.wait}});
        }
        for (bool extra : {false, true}) {
            C7Fixture f(true, 1);
            const auto q =
                c7_marked(f,
                          protocol::Skill::frame_server_damage(
                              4, 77, 801, 1, extra ? 3 : 12, extra ? 7 : -3, protocol::kAtkHit),
                          extra ? "skill-palsy-exceeds-damage" : "skill-negative-palsy",
                          snapshot);
            expect(
                q.local_skill && q.hits.size() == 1 && q.delays.empty(),
                "C7 bounded unusual skill components retain their independent acknowledgement/hit outcomes");
            if (extra) {
                c7_feedback(q,
                            {{"You gave 7 of Explosive Blow to the enemy.",
                              protocol::BattleMsgKind::Damage,
                              std::nullopt}},
                            {{client::CombatFloatKind::DamageDealt, "7", 801, false}});
            } else {
                c7_feedback(
                    q,
                    {{"You dealt 12 damage.", protocol::BattleMsgKind::Damage, std::nullopt}},
                    {{client::CombatFloatKind::DamageDealt, "12", 801, false}});
            }
        }
    }
    void test_c7_archer_explosion(bool snapshot) {
        struct Value {
            bool self;
            int clazz;
            std::uint8_t skill;
            int damage;
            bool fire;
            int hp;
        };
        const std::array<Value, 7> cases{{{false, 2, 49, 10, false, 90},
                                          {false, 2, 49, 0, true, 100},
                                          {false, 2, 49, -5, true, 105},
                                          {false, 0, 49, 10, true, 90},
                                          {false, 2, 48, 10, true, 90},
                                          {true, 2, 49, 10, false, 90},
                                          {true, 2, 49, 0, true, 100}}};
        for (std::size_t kind = 0; kind < cases.size(); ++kind) {
            const auto& value = cases[kind];
            C7Fixture f(true, value.self ? 2 : 0, value.self);
            if (!value.self) {
                queue(f.session, c7_player_frame(88, value.clazz, false, 1300, 1000));
            }
            const auto caster = value.self ? 77u : 88u;
            const auto q =
                c7_marked(f,
                          protocol::Skill::frame_server_damage(
                              value.skill, caster, 801, 1, value.damage, 0, protocol::kAtkHit),
                          "skill-archer-explosion-" + std::to_string(kind),
                          snapshot);
            const auto* victim = c7_monster(q, 801);
            expect(
                victim && victim->value.hp() == value.hp &&
                    q.skills.size() == (value.fire ? 1u : 0u) && q.local_skill == value.self &&
                    q.hits.size() == 1 && q.hits[0].target_id == 801 &&
                    q.hits[0].skill_index == value.skill && q.delays.empty(),
                "C7 positive Archer49 splash suppresses only fire presentation while preserving HP/hit and self acknowledgement");
            if (value.self && value.damage > 0) {
                c7_feedback(
                    q,
                    {{"You dealt 10 damage.", protocol::BattleMsgKind::Damage, std::nullopt}},
                    {{client::CombatFloatKind::DamageDealt, "10", 801, false}});
            } else {
                c7_feedback(q, {}, {});
            }
        }
    }

    void test_c7_effect_values(bool snapshot) {
        C7Fixture f;
        const std::array<std::uint8_t, 10> types{{1, 2, 4, 5, 6, 7, 8, 9, 10, 11}};
        for (std::size_t i = 0; i < types.size(); ++i) {
            queue(f.session,
                  protocol::Effect::frame(i == 0 ? 0 : 900u + static_cast<std::uint32_t>(i),
                                          types[i]));
        }
        queue(f.session, protocol::Effect::frame(0, 3));
        queue(f.session, protocol::Effect::frame(999, 3));
        queue(f.session, protocol::Effect::frame_damaged(0, 0));
        queue(f.session, protocol::Effect::frame_damaged(999, 65535));
        for (std::uint8_t unsupported : {std::uint8_t{0},
                                         std::uint8_t{12},
                                         std::uint8_t{22},
                                         std::uint8_t{24},
                                         std::uint8_t{255}}) {
            queue(f.session, protocol::Effect::frame(999, unsupported));
        }
        f.net.update(0, f.origin);
        auto q = c7_observe(f.net, *f.session, "effects-all-values", snapshot);
        expect(
            q.charms == std::vector<std::pair<std::uint32_t, std::uint8_t>>{{0, 1},
                                                                            {901, 2},
                                                                            {902, 4},
                                                                            {903, 5},
                                                                            {904, 6},
                                                                            {905, 7},
                                                                            {906, 8},
                                                                            {907, 9},
                                                                            {908, 10},
                                                                            {909, 11}} &&
                q.levels == std::vector<std::uint32_t>{0, 999} &&
                q.damaged ==
                    std::vector<std::pair<std::uint32_t, std::uint16_t>>{{0, 0}, {999, 65535}} &&
                q.hits.empty() && q.floats.empty() && q.infos.empty() && q.local_hp == 100,
            "C7 effect values preserve zero/unknown identity, all charm bytes, unsigned damage boundaries and independent FIFO order");
        queue(f.session, protocol::Effect::frame(11, 3));
        queue(f.session, protocol::Effect::frame(12, 3));
        queue(f.session, protocol::Effect::frame_damaged(13, 65535));
        queue(f.session, protocol::Effect::frame_damaged(14, 0));
        queue(f.session, protocol::Effect::frame(15, 1));
        queue(f.session, protocol::Effect::frame(16, 11));
        f.net.update(0, f.origin);
        std::uint32_t level = 0xf001;
        std::uint32_t entity = 0xf002;
        std::uint16_t damage = 123;
        std::uint8_t effect = 203;
        expect(
            f.net.take_level_up_effect(level) && level == 11 &&
                f.net.take_damaged_effect(entity, damage) && entity == 13 && damage == 65535 &&
                f.net.take_charm_effect(entity, effect) && entity == 15 && effect == 1,
            "C7 partial effect drains consume exactly one complete value from each independent queue");
        queue(f.session, protocol::Effect::frame(17, 3));
        queue(f.session, protocol::Effect::frame_damaged(18, 1234));
        queue(f.session, protocol::Effect::frame(19, 2));
        f.net.update(0, f.origin);
        q = c7_observe(f.net, *f.session, "effects-partial-refill", snapshot);
        expect(q.levels == std::vector<std::uint32_t>{12, 17} &&
                   q.damaged ==
                       std::vector<std::pair<std::uint32_t, std::uint16_t>>{{14, 0}, {18, 1234}} &&
                   q.charms ==
                       std::vector<std::pair<std::uint32_t, std::uint8_t>>{{16, 11}, {19, 2}},
               "C7 effect refill preserves older unread values before newly appended fields");
        q = c7_observe(f.net, *f.session, "effects-exhausted", snapshot);
        expect(c7_events_empty(q),
               "C7 fully drained effect queues do not replay across observations");
    }

    Bytes c7_action(std::uint32_t id,
                    std::uint8_t action,
                    std::optional<std::uint8_t> arg = std::nullopt) {
        protocol::PacketWriter writer;
        writer.write_u32(id);
        writer.write_u8(action);
        if (arg) {
            writer.write_u8(*arg);
        }
        return writer.frame(static_cast<std::uint8_t>(protocol::S2C::Action));
    }
    void test_c7_action_rest_and_unknown(bool snapshot) {
        C7Fixture f;
        auto q = c7_marked(f, c7_action(88, protocol::kAtRest, 1), "action-rest-remote", snapshot);
        expect(!q.rest_pending && f.net.entity_resting(88) && !f.net.entity_resting(77),
               "C7 remote rest updates its public entity state without publishing the local latch");
        q = c7_marked(
            f, c7_action(77, protocol::kAtRest, 2), "action-rest-local-nonzero", snapshot);
        expect(q.rest_pending && q.resting && f.net.entity_resting(77) && f.net.entity_resting(88),
               "C7 any nonzero local rest argument means true without changing another entity");
        q = c7_marked(f, c7_action(77, protocol::kAtRest), "action-rest-default-zero", snapshot);
        expect(q.rest_pending && !q.resting && !f.net.entity_resting(77) &&
                   f.net.entity_resting(88),
               "C7 generic five-byte rest defaults its missing argument to zero");
        queue(f.session, c7_action(77, protocol::kAtRest, 1));
        queue(f.session, c7_action(77, protocol::kAtRest, 0));
        queue(f.session, c7_action(88, protocol::kAtRest, 0));
        f.net.update(0, f.origin);
        q = c7_observe(f.net, *f.session, "action-rest-latest-false", snapshot);
        expect(
            q.rest_pending && !q.resting && !f.net.entity_resting(77) && !f.net.entity_resting(88),
            "C7 same-batch local rest coalesces to its last value independently of remote changes");
        for (std::uint8_t unknown : {std::uint8_t{0}, std::uint8_t{255}}) {
            q = c7_marked(f,
                          c7_action(800, unknown, 203),
                          "action-unknown-" + std::to_string(unknown),
                          snapshot);
            expect(
                c7_events_empty(q) && c7_monster(q, 800) && c7_monster(q, 800)->value.hp() == 100,
                "C7 unsupported generic action consumes its frame without changing monster state or queues");
        }
        C7Fixture unseeded(false);
        q = c7_marked(unseeded,
                      c7_action(999, protocol::kAtRest, 1),
                      "action-rest-before-local-id",
                      snapshot);
        expect(q.rest_pending && q.resting && unseeded.net.entity_resting(999),
               "C7 unknown local identity accepts the ordinary rest latch from a nonzero entity");
        q = c7_marked(unseeded,
                      c7_action(0, protocol::kAtRest, 0),
                      "action-rest-zero-before-local-id",
                      snapshot);
        expect(
            q.rest_pending && !q.resting && unseeded.net.entity_resting(999),
            "C7 zero-id rest while local identity is unknown updates only its own entity and the local latch");
    }
    void test_c7_action_preparation(bool snapshot) {
        C7Fixture f;
        auto q = c7_marked(f,
                           protocol::Skill::frame_action_skill(800, 4, 77),
                           "action-skill-monster-facing",
                           snapshot);
        const auto* m = c7_monster(q, 800);
        expect(
            q.skills.size() == 1 && q.skills[0].caster_id == 800 && q.skills[0].skill == 4 &&
                q.skills[0].target_id == 77 && !q.skills[0].fire && m &&
                c7_yaw(m->value.yaw(), 3.14159265f / 2) && !q.local_skill && q.hits.empty() &&
                q.infos.empty(),
            "C7 action skill queues preparation and faces its monster without acknowledging a fire echo");
        queue(f.session, protocol::Skill::frame_action_skill(77, 4, 801));
        queue(f.session, protocol::Skill::frame_action_skill(999, 255, 0));
        f.net.update(0, f.origin);
        q = c7_observe(f.net, *f.session, "action-skill-self-and-unknown", snapshot);
        expect(
            q.skills.size() == 2 && q.skills[0].caster_id == 77 && q.skills[0].skill == 4 &&
                q.skills[0].target_id == 801 && !q.skills[0].fire && q.skills[1].caster_id == 999 &&
                q.skills[1].skill == 255 && q.skills[1].target_id == 0 && !q.skills[1].fire &&
                !q.local_skill,
            "C7 skill preparation preserves arbitrary identities and bytes without fire/self-ack policy");
    }
    void test_c7_action_death_sequences(bool snapshot) {
        for (bool beheaded : {false, true}) {
            C7Fixture f;
            queue(f.session, server::frame_move_monster_on(800, 100, 0, false));
            f.net.update(0, f.origin);
            auto q = c7_marked(f,
                               protocol::Skill::frame_action_knee(800),
                               beheaded ? "death-behead-knee" : "death-die-knee",
                               snapshot);
            auto* m = c7_monster(q, 800);
            expect(
                m && m->knee && !m->die && !m->behead && m->value.hp() == 0 && !m->value.moving() &&
                    !m->value.finishing_death() && (m->value.gstate() & protocol::kCgsKnee) != 0,
                "C7 knee stops movement, sets HP and knee bit, and publishes only its independent latch");
            q = c7_marked(f,
                          beheaded ? protocol::Skill::frame_action_beheaded(800)
                                   : protocol::Skill::frame_action_die(800),
                          beheaded ? "death-beheaded-after-knee" : "death-die-after-knee",
                          snapshot);
            m = c7_monster(q, 800);
            expect(
                m && !m->knee && m->die == !beheaded && m->behead == beheaded &&
                    m->value.hp() == 0 && !m->value.moving() &&
                    (m->value.gstate() & protocol::kCgsKnee) == 0 && m->value.finishing_death(),
                "C7 final death action clears knee while retaining finishing-death beyond one-shot consumption");
            q = c7_marked(f,
                          server::frame_remove_monster(800),
                          beheaded ? "death-behead-remove-deferred" : "death-die-remove-deferred",
                          snapshot);
            m = c7_monster(q, 800);
            expect(
                m && m->value.server_removed() && m->value.finishing_death() && !m->die &&
                    !m->behead,
                "C7 consumed death latch still defers cross-family server removal until presentation finishes");
        }
        C7Fixture knee;
        queue(knee.session, protocol::Skill::frame_action_knee(800));
        auto q = c7_marked(
            knee, server::frame_remove_monster(800), "death-knee-remove-immediate", snapshot);
        expect(!c7_monster(q, 800) && q.monsters.size() == 1,
               "C7 knee alone does not trigger the final-death removal deferral");
        C7Fixture unknown;
        queue(unknown.session, protocol::Skill::frame_action_die(999));
        queue(unknown.session, protocol::Skill::frame_action_beheaded(0));
        q = c7_marked(
            unknown, protocol::Skill::frame_action_knee(998), "death-unknown-ids", snapshot);
        expect(
            c7_events_empty(q) && q.monsters.size() == 2 && c7_monster(q, 800)->value.hp() == 100,
            "C7 death and knee for unknown entities do not create model entries or queue values");
    }
    void test_c7_transform_actions(bool snapshot) {
        C7Fixture f;
        queue(f.session,
              protocol::Property::frame_update_mproperty(77, protocol::kPropertyMSpeed, 37));
        f.net.update(0, f.origin);
        auto q = c7_marked(f,
                           protocol::Transform::frame_action_pre(77, 3),
                           "transform-pre-no-immediate",
                           snapshot);
        expect(
            q.transforms.size() == 1 && q.transforms[0].entity_id == 77 && q.transforms[0].pre &&
                q.transforms[0].arg == 3 && q.tflag == 0 && q.speed == 37,
            "C7 pre-transform preserves complete event fields without changing immediate local form or speed");
        q = c7_marked(f,
                      protocol::Transform::frame_action_form(88, 0x42),
                      "transform-remote-no-immediate",
                      snapshot);
        expect(q.transforms.size() == 1 && q.transforms[0].entity_id == 88 &&
                   !q.transforms[0].pre && q.transforms[0].arg == 0x42 && q.tflag == 0 &&
                   q.speed == 37,
               "C7 nonlocal transform remains a FIFO event without touching local scalar form");
        q = c7_marked(f,
                      protocol::Transform::frame_action_form(77, 0x42),
                      "transform-local-positive-stage",
                      snapshot);
        expect(q.transforms.size() == 1 && !q.transforms[0].pre && q.transforms[0].arg == 0x42 &&
                   q.tflag == 0x42 && q.speed == 37,
               "C7 local nonzero stage stores the full packed flag while preserving current speed");
        for (std::uint8_t flag : {std::uint8_t{0x40}, std::uint8_t{0}}) {
            queue(f.session,
                  protocol::Property::frame_update_mproperty(77, protocol::kPropertyMSpeed, 37));
            q = c7_marked(f,
                          protocol::Transform::frame_action_form(77, flag),
                          "transform-local-zero-stage-" + std::to_string(flag),
                          snapshot);
            expect(
                q.transforms.size() == 1 && q.transforms[0].arg == flag && q.tflag == flag &&
                    q.speed == 0,
                "C7 low stage nibble zero resets local speed even when the race nibble remains nonzero");
        }
        queue(f.session, protocol::Transform::frame_action_pre(77, 1));
        queue(f.session, protocol::Transform::frame_action_form(77, 0x41));
        q = c7_marked(
            f, protocol::Transform::frame_action_pre(88, 2), "transform-fifo-mixed", snapshot);
        expect(
            q.transforms.size() == 3 && q.transforms[0].entity_id == 77 && q.transforms[0].pre &&
                q.transforms[0].arg == 1 && q.transforms[1].entity_id == 77 &&
                !q.transforms[1].pre && q.transforms[1].arg == 0x41 &&
                q.transforms[2].entity_id == 88 && q.transforms[2].pre &&
                q.transforms[2].arg == 2 && q.tflag == 0x41,
            "C7 transform preparation and form records preserve exact FIFO order with immediate scalar latest-value semantics");
        C7Fixture unseeded(false);
        q = c7_marked(unseeded,
                      protocol::Transform::frame_action_form(999, 0xa3),
                      "transform-before-local-id",
                      snapshot);
        expect(q.transforms.size() == 1 && q.transforms[0].entity_id == 999 &&
                   q.transforms[0].arg == 0xa3 && q.tflag == 0xa3,
               "C7 unknown local identity accepts immediate packed transform from any entity");
    }

    void test_c7_mixed_world_and_combat(bool snapshot) {
        C7Fixture f;
        queue(f.session, c7_monster_frame(900, 1400, 1000));
        queue(f.session, server::frame_move_monster_on(900, 100, 0, false));
        queue(f.session, protocol::Attack::frame_server(900, 801, 4, 0, protocol::kAtkHit));
        queue(f.session, protocol::MState::frame(900, protocol::kCmsMoveStop));
        queue(f.session, protocol::Skill::frame_server(4, 88, 801, 1));
        queue(f.session, protocol::Skill::frame_action_skill(800, 4, 77));
        queue(f.session, protocol::Skill::frame_action_knee(900));
        queue(f.session, protocol::Skill::frame_action_beheaded(900));
        queue(f.session, server::frame_remove_monster(900));
        f.net.update(0, f.origin);
        const auto q =
            c7_observe(f.net, *f.session, "mixed-create-move-combat-death-remove", snapshot);
        const auto* removed = c7_monster(q, 900);
        const auto* victim = c7_monster(q, 801);
        expect(
            removed && removed->value.x() == 1500 && removed->value.hp() == 0 && removed->behead &&
                !removed->knee && removed->value.finishing_death() &&
                removed->value.server_removed() && !removed->value.moving() &&
                removed->value.move_stopped() && removed->value.ice_slowed() &&
                (removed->value.gstate() & protocol::kCgsKnee) == 0 && victim &&
                victim->value.hp() == 96 && q.hits.size() == 1 && q.hits[0].attacker_id == 900 &&
                q.hits[0].target_id == 801 && q.hits[0].flags == protocol::kAtkHit,
            "C7 mixed world/combat packets expose the final ordered motion, damage, state and deferred-removal result");
        expect(q.skills.size() == 2 && q.skills[0].caster_id == 88 && q.skills[0].skill == 4 &&
                   q.skills[0].target_id == 801 && q.skills[0].fire &&
                   q.skills[1].caster_id == 800 && q.skills[1].skill == 4 &&
                   q.skills[1].target_id == 77 && !q.skills[1].fire,
               "C7 fire and preparation share one FIFO in their actual same-batch arrival order");
    }
    void test_c7_split_frames(bool snapshot) {
        for (bool action : {false, true}) {
            C7Fixture f;
            const auto packet = action ? protocol::Skill::frame_action_skill(800, 25, 77)
                                       : protocol::Skill::frame_server_damage(
                                             25, 77, 801, 1, 12, 5, protocol::kAtkHit);
            Bytes prefix = protocol::Emote::frame_server(999, 7);
            append(prefix, Bytes(packet.begin(), packet.end() - 1));
            queue(f.session, prefix);
            f.net.update(0, f.origin);
            auto q = c7_observe(
                f.net, *f.session, action ? "split-action-prefix" : "split-skill-prefix", snapshot);
            expect(
                q.emotes.size() == 1 && q.emotes[0].player_id == 999 && q.emotes[0].motion == 7 &&
                    q.skills.empty() && q.hits.empty() && q.infos.empty() && q.delays.empty() &&
                    !q.local_skill && c7_monster(q, 801)->value.hp() == 100,
                "C7 split combat frame preserves the completed prior event without committing a partial suffix");
            queue(f.session, Bytes(packet.end() - 1, packet.end()));
            queue(f.session, protocol::Emote::frame_server(998, 8));
            f.net.update(0, f.origin);
            q = c7_observe(f.net,
                           *f.session,
                           action ? "split-action-complete" : "split-skill-complete",
                           snapshot);
            expect(
                q.emotes.size() == 1 && q.emotes[0].player_id == 998 && q.emotes[0].motion == 8,
                "C7 completed suffix neither replays the earlier Emote nor drops the next complete event");
            if (action) {
                expect(
                    q.skills.size() == 1 && q.skills[0].caster_id == 800 &&
                        q.skills[0].skill == 25 && q.skills[0].target_id == 77 &&
                        !q.skills[0].fire && q.hits.empty() && q.delays.empty() && !q.local_skill,
                    "C7 a split skill preparation is emitted exactly once after its final byte arrives");
            } else {
                expect(
                    q.skills.empty() && q.hits.size() == 1 && q.delays.size() == 1 &&
                        q.delays[0].target_id == 801 && q.delays[0].damage == 12 && q.local_skill &&
                        c7_monster(q, 801)->value.hp() == 88,
                    "C7 a split damage-bearing self Skill commits its complete damage, effect and acknowledgement once");
                c7_feedback(q,
                            {{"You dealt 7 damage.", protocol::BattleMsgKind::Damage, std::nullopt},
                             {"You gave 5 of Explosive Blow to the enemy.",
                              protocol::BattleMsgKind::Damage,
                              std::nullopt}},
                            {{client::CombatFloatKind::DamageDealt, "7", 801, false},
                             {client::CombatFloatKind::DamageDealt, "5", 801, false}});
            }
            f.net.update(0, f.origin);
            q = c7_observe(f.net,
                           *f.session,
                           action ? "split-action-exhausted" : "split-skill-exhausted",
                           snapshot);
            expect(c7_events_empty(q) && q.chats.empty(),
                   "C7 an empty subsequent update cannot replay a completed split frame");
        }
    }
    void test_c7_two_client_isolation(bool snapshot) {
        C7Fixture mage(true, 1, false, 31);
        C7Fixture archer(true, 2, true, 33);
        queue(archer.session, c7_player_frame(88, 2, false, 1300, 1000));
        queue(mage.session,
              protocol::Skill::frame_server_damage(2, 77, 801, 1, 12, 0, protocol::kAtkHit));
        queue(archer.session,
              protocol::Skill::frame_server_damage(49, 88, 801, 1, 5, 0, protocol::kAtkHit));
        mage.net.update(0, mage.origin);
        auto a = c7_observe(mage.net, *mage.session, "isolation-mage-first", snapshot);
        expect(a.local_skill && a.floats.size() == 1 && a.floats[0].wait_for_bolt &&
                   a.delays.size() == 1 && a.delays[0].damage == 12 && a.skills.empty() &&
                   c7_monster(a, 801)->value.hp() == 88,
               "C7 first client's Mage identity and origin apply only its own queued damage");
        archer.net.update(0, archer.origin);
        auto b = c7_observe(archer.net, *archer.session, "isolation-archer-second", snapshot);
        expect(
            !b.local_skill && b.skills.empty() && b.infos.empty() && b.floats.empty() &&
                b.delays.empty() && b.hits.size() == 1 && b.hits[0].attacker_id == 88 &&
                c7_monster(b, 801)->value.hp() == 95 &&
                c7_monster(b, 800)->value.wx() != c7_monster(a, 800)->value.wx(),
            "C7 second client's Archer suppression, hit identity, HP and origin remain independent");
        queue(mage.session, protocol::Transform::frame_action_form(77, 0x63));
        queue(archer.session, protocol::Skill::frame_action_rest(77, 1));
        archer.net.update(0, archer.origin);
        b = c7_observe(archer.net, *archer.session, "isolation-archer-rest", snapshot);
        expect(b.rest_pending && b.resting && b.transforms.empty() && b.tflag == 0 &&
                   b.infos.empty(),
               "C7 interleaved local latches and transform state do not cross client instances");
        mage.net.update(0, mage.origin);
        a = c7_observe(mage.net, *mage.session, "isolation-mage-transform", snapshot);
        expect(
            !a.rest_pending && a.transforms.size() == 1 && a.transforms[0].arg == 0x63 &&
                a.tflag == 0x63 && c7_monster(a, 801)->value.hp() == 88,
            "C7 delayed first-client work preserves its previous HP while accepting only its own form event");
    }
    void c7_queue_cleanup_seed(C7Fixture& f) {
        queue(f.session, protocol::Attack::frame_server(88, 801, 0, 0, 0));
        queue(f.session, protocol::Attack::frame_server(77, 801, 12, 5, protocol::kAtkHit));
        queue(f.session,
              protocol::Skill::frame_server_damage(2, 77, 801, 1, 6, 0, protocol::kAtkHit));
        queue(f.session,
              protocol::Skill::frame_server_damage(4, 800, 77, 1, 5, 0, protocol::kAtkHit));
        queue(f.session, protocol::Skill::frame_action_skill(800, 4, 77));
        queue(f.session, protocol::Transform::frame_action_form(77, 0x42));
        queue(f.session, protocol::Emote::frame_server(999, 10));
        queue(f.session, protocol::Effect::frame(999, 3));
        queue(f.session, protocol::Effect::frame(999, 11));
        queue(f.session, protocol::Effect::frame_damaged(999, 65535));
        queue(f.session, protocol::Skill::frame_action_rest(77, 1));
        queue(f.session,
              protocol::Property::frame_update_mproperty(77, protocol::kPropertyMSpeed, 37));
        f.net.update(0, f.origin);
    }
    void test_c7_cleanup_contracts(bool snapshot) {
        for (bool explicit_close : {false, true}) {
            C7Fixture f(true, 1);
            c7_queue_cleanup_seed(f);
            if (explicit_close) {
                queue(f.session, protocol::GameExit::frame_server_answer(true));
                f.net.close();
            } else {
                f.session->eof = true;
                f.net.update(0, f.origin);
            }
            auto q = c7_observe(f.net,
                                *f.session,
                                explicit_close ? "cleanup-explicit-close" : "cleanup-eof",
                                snapshot);
            expect(
                !f.net.connected() && !f.session->alive && f.session->closes == 1 &&
                    q.local_skill && q.hit_pending && q.hit_from == 800 && q.rest_pending &&
                    q.resting && q.tflag == 0x42 && q.speed == 37 && q.transforms.size() == 1 &&
                    q.transforms[0].entity_id == 77 && !q.transforms[0].pre &&
                    q.transforms[0].arg == 0x42 && q.infos.size() == 4 && q.floats.size() == 4 &&
                    q.delays.size() == 1 && q.delays[0].target_id == 801 && q.delays[0].damage == 6,
                "C7 EOF and explicit close both retain existing info/float/delay/transform queues, scalar form/speed and local one-shot latches");
            c7_feedback(q,
                        {{"You dealt 7 damage.", protocol::BattleMsgKind::Damage, std::nullopt},
                         {"You gave 5 of Explosive Blow to the enemy.",
                          protocol::BattleMsgKind::Damage,
                          std::nullopt},
                         {"You dealt 6 damage.", protocol::BattleMsgKind::Damage, std::nullopt},
                         {"You received 5 damage.", protocol::BattleMsgKind::Damage, std::nullopt}},
                        {{client::CombatFloatKind::DamageDealt, "7", 801, false},
                         {client::CombatFloatKind::DamageDealt, "5", 801, false},
                         {client::CombatFloatKind::DamageDealt, "6", 801, true},
                         {client::CombatFloatKind::DamageTaken, "5", 0, false}});
            if (explicit_close) {
                expect(
                    q.monsters.empty() && q.attacks.empty() && q.skills.empty() &&
                        q.emotes.empty() && q.hits.empty() && q.levels.empty() &&
                        q.charms.empty() && q.damaged.empty() && !f.net.entity_resting(77),
                    "C7 explicit close selectively clears entity and presentation domains while retained queues stay observable");
            } else {
                expect(
                    q.monsters.size() == 2 && q.attacks.size() == 1 &&
                        q.attacks[0].attacker_id == 88 && q.attacks[0].target_id == 801 &&
                        q.skills.size() == 2 && q.skills[0].caster_id == 800 && q.skills[0].fire &&
                        q.skills[1].caster_id == 800 && !q.skills[1].fire && q.emotes.size() == 1 &&
                        q.hits.size() == 3 && q.levels == std::vector<std::uint32_t>{999} &&
                        q.charms ==
                            std::vector<std::pair<std::uint32_t, std::uint8_t>>{{999, 11}} &&
                        q.damaged ==
                            std::vector<std::pair<std::uint32_t, std::uint16_t>>{{999, 65535}} &&
                        f.net.entity_resting(77),
                    "C7 EOF only clears online views and retains unread combat/entity presentation state");
            }
            q = c7_observe(f.net,
                           *f.session,
                           explicit_close ? "cleanup-explicit-exhausted" : "cleanup-eof-exhausted",
                           snapshot);
            expect(
                c7_events_empty(q) && q.tflag == 0x42 && q.speed == 37,
                "C7 consumed retained queues and latches do not replay while scalar form/speed remain stored");
        }
    }

    // C7_REACHABLE_PUBLIC_GUARDS_BEGIN
    void test_c7_reachable_public_paths(bool snapshot) {
        C7Fixture missing;
        client::PendingMonsterActionDamage sentinel{-321, 0xa7, 0xf009};
        expect(
            !missing.net.take_monster_action_damage(0xf008, sentinel) && sentinel.damage == -321 &&
                sentinel.flags == 0xa7 && sentinel.target_id == 0xf009 &&
                !missing.net.take_monster_action_damage(0xf008, sentinel) &&
                sentinel.damage == -321 && sentinel.flags == 0xa7 && sentinel.target_id == 0xf009,
            "C7 missing monster ActionDamage fails repeatedly without changing any output field");
        const auto absent = c7_observe(
            missing.net, *missing.session, "public-missing-monster-action-damage", snapshot);
        expect(
            c7_events_empty(absent) && absent.monsters.size() == 2,
            "C7 missing monster ActionDamage leaves the actual presentation queues and monsters intact");

        C7Fixture ground(true, 1);
        auto expected_sent = ground.session->sent;
        const auto request = protocol::Skill::frame_client_ground(2, 1200, 1000);
        expected_sent.insert(expected_sent.end(), request.begin(), request.end());
        expect(
            ground.net.send_skill_ground(2, 1200, 1000) && ground.session->sent == expected_sent,
            "C7 public ground skill sends the exact ordinary targetless request after its real handshake");
        const auto q = c7_marked(ground,
                                 protocol::Skill::frame_server_damage(
                                     2, 77, 801, 1, 9, 2, protocol::kAtkHit | protocol::kAtkFatal),
                                 "public-ground-skill-self-damage-echo",
                                 snapshot);
        const auto* victim = c7_monster(q, 801);
        const std::array<float, 3> critical_color{1.0f, 0.1f, 0.1f};
        expect(
            q.local_skill && !q.hit_pending && !q.rest_pending && q.skills.empty() && victim &&
                victim->value.hp() == 91 && q.infos.size() == 2 &&
                q.infos[0].text == "You gave critical damage of 7 to the enemy." &&
                q.infos[0].kind == protocol::BattleMsgKind::Damage &&
                q.infos[0].color == critical_color &&
                q.infos[1].text == "You gave 2 of Explosive Blow to the enemy." &&
                q.infos[1].kind == protocol::BattleMsgKind::Damage && !q.infos[1].color &&
                q.floats.size() == 2 && q.floats[0].kind == client::CombatFloatKind::DamageDealt &&
                q.floats[0].text == "7" && q.floats[0].anchor_id == 801 &&
                q.floats[0].wait_for_bolt &&
                q.floats[1].kind == client::CombatFloatKind::DamageDealt &&
                q.floats[1].text == "2" && q.floats[1].anchor_id == 801 &&
                q.floats[1].wait_for_bolt && q.delays.size() == 1 && q.delays[0].target_id == 801 &&
                q.delays[0].damage == 9 && q.hits.size() == 1 && q.hits[0].attacker_id == 77 &&
                q.hits[0].target_id == 801 && q.hits[0].skill_index == 2 &&
                q.hits[0].flags ==
                    (protocol::kAtkHit | protocol::kAtkFatal | protocol::kAtkExplosive) &&
                q.attacks.empty() && q.transforms.empty() && q.emotes.empty() && q.levels.empty() &&
                q.damaged.empty() && q.charms.empty(),
            "C7 matching self ground echo acknowledges once and preserves every public damage, delay and feedback field");
        expect(
            g_c7_checkpoint_labels.size() == 182 &&
                g_c7_checkpoint_labels[180] == "public-missing-monster-action-damage" &&
                g_c7_checkpoint_labels[181] == "public-ground-skill-self-damage-echo",
            "C7 appends exactly two reachable public observations after the unchanged180 checkpoint prefix");
    }
    // C7_REACHABLE_PUBLIC_GUARDS_END

    void test_c7_combat_packets(bool snapshot) {
        g_c7_checkpoint_labels.clear();
        test_c7_malformed_payloads(snapshot);
        test_c7_strict_emotes(snapshot);
        test_c7_accepted_tails(snapshot);
        test_c7_attack_animation(snapshot);
        test_c7_attack_damage(snapshot);
        test_c7_attack_archer_identity(snapshot);
        test_c7_facing_order(snapshot);
        test_c7_skill_acknowledgement(snapshot);
        test_c7_skill_damage_and_delay(snapshot);
        test_c7_local_skill_floats(snapshot);
        test_c7_archer_explosion(snapshot);
        test_c7_effect_values(snapshot);
        test_c7_action_rest_and_unknown(snapshot);
        test_c7_action_preparation(snapshot);
        test_c7_action_death_sequences(snapshot);
        test_c7_transform_actions(snapshot);
        test_c7_mixed_world_and_combat(snapshot);
        test_c7_split_frames(snapshot);
        test_c7_two_client_isolation(snapshot);
        test_c7_cleanup_contracts(snapshot);
        const std::vector<std::string> expected{
            "malformed-attack-0",
            "malformed-attack-1",
            "malformed-attack-2",
            "malformed-attack-3",
            "malformed-attack-4",
            "malformed-attack-5",
            "malformed-attack-6",
            "malformed-attack-7",
            "malformed-attack-8",
            "malformed-attack-9",
            "malformed-attack-10",
            "malformed-attack-11",
            "malformed-attack-12",
            "malformed-attack-13",
            "malformed-attack-14",
            "malformed-attack-15",
            "malformed-attack-16",
            "malformed-skill-no-damage-0",
            "malformed-skill-no-damage-1",
            "malformed-skill-no-damage-2",
            "malformed-skill-no-damage-3",
            "malformed-skill-no-damage-4",
            "malformed-skill-no-damage-5",
            "malformed-skill-no-damage-6",
            "malformed-skill-no-damage-7",
            "malformed-skill-no-damage-8",
            "malformed-skill-no-damage-9",
            "malformed-skill-no-damage-10",
            "malformed-skill-damage-11",
            "malformed-skill-damage-12",
            "malformed-skill-damage-13",
            "malformed-skill-damage-14",
            "malformed-skill-damage-15",
            "malformed-skill-damage-16",
            "malformed-skill-damage-17",
            "malformed-skill-damage-18",
            "malformed-skill-damage-19",
            "malformed-effect-0",
            "malformed-effect-1",
            "malformed-effect-2",
            "malformed-effect-3",
            "malformed-effect-4",
            "malformed-damaged-effect-5",
            "malformed-damaged-effect-6",
            "malformed-action-0",
            "malformed-action-1",
            "malformed-action-2",
            "malformed-action-3",
            "malformed-action-4",
            "malformed-action-skill-fallback-5",
            "malformed-action-skill-fallback-6",
            "malformed-action-skill-fallback-7",
            "malformed-action-skill-fallback-8",
            "malformed-action-skill-fallback-9",
            "emote-invalid-short-0",
            "emote-invalid-short-1",
            "emote-invalid-short-2",
            "emote-invalid-short-3",
            "emote-invalid-short-4",
            "emote-invalid-tail",
            "emote-invalid-zero-player",
            "emote-invalid-motion-0",
            "emote-invalid-motion-21",
            "emote-invalid-motion-255",
            "emote-valid-middle",
            "accepted-tail-0",
            "accepted-tail-1",
            "accepted-tail-2",
            "accepted-tail-3",
            "accepted-tail-4",
            "accepted-tail-5",
            "accepted-tail-6",
            "skill-nonzero-damage-byte",
            "attack-player-animation-0",
            "attack-player-animation-1",
            "attack-player-animation-2",
            "attack-after-live-step",
            "attack-arrival-latch",
            "attack-animation-only-order",
            "attack-animation-only-incoming",
            "attack-damage-0",
            "attack-damage-1",
            "attack-damage-2",
            "attack-damage-3",
            "attack-damage-4",
            "attack-nonmonster-target-88",
            "attack-nonmonster-target-999",
            "attack-incoming-10",
            "attack-incoming-0",
            "attack-incoming--5",
            "attack-latest-hit-from",
            "attack-monster-target-no-action-damage",
            "attack-archer-identity-0",
            "attack-archer-identity-1",
            "attack-archer-identity-2",
            "attack-archer-identity-3",
            "attack-archer-identity-4",
            "attack-archer-identity-5",
            "attack-archer-identity-6",
            "facing-attack-then-correction",
            "facing-correction-then-attack",
            "facing-target-priority-0",
            "facing-target-priority-1",
            "facing-target-priority-2",
            "facing-target-priority-3",
            "facing-local-display-priority",
            "skill-remote-does-not-ack",
            "skill-self-wrong-skill-acks",
            "skill-self-wrong-target-acks",
            "skill-self-ack-coalesces",
            "skill-read-only-self-fire",
            "skill-zero-caster-independent-damage",
            "skill-hp-delay-0",
            "skill-hp-delay-1",
            "skill-hp-delay-2",
            "skill-hp-delay-3",
            "skill-hp-delay-4",
            "skill-hp-delay-5",
            "skill-hp-delay-6",
            "skill-incoming-10",
            "skill-incoming-0",
            "skill-incoming--5",
            "skill-incoming-nonmonster-88",
            "skill-incoming-nonmonster-999",
            "skill-mage-float-2",
            "skill-mage-float-25",
            "skill-mage-float-4",
            "skill-mage-float-255",
            "skill-negative-palsy",
            "skill-palsy-exceeds-damage",
            "skill-archer-explosion-0",
            "skill-archer-explosion-1",
            "skill-archer-explosion-2",
            "skill-archer-explosion-3",
            "skill-archer-explosion-4",
            "skill-archer-explosion-5",
            "skill-archer-explosion-6",
            "effects-all-values",
            "effects-partial-refill",
            "effects-exhausted",
            "action-rest-remote",
            "action-rest-local-nonzero",
            "action-rest-default-zero",
            "action-rest-latest-false",
            "action-unknown-0",
            "action-unknown-255",
            "action-rest-before-local-id",
            "action-rest-zero-before-local-id",
            "action-skill-monster-facing",
            "action-skill-self-and-unknown",
            "death-die-knee",
            "death-die-after-knee",
            "death-die-remove-deferred",
            "death-behead-knee",
            "death-beheaded-after-knee",
            "death-behead-remove-deferred",
            "death-knee-remove-immediate",
            "death-unknown-ids",
            "transform-pre-no-immediate",
            "transform-remote-no-immediate",
            "transform-local-positive-stage",
            "transform-local-zero-stage-64",
            "transform-local-zero-stage-0",
            "transform-fifo-mixed",
            "transform-before-local-id",
            "mixed-create-move-combat-death-remove",
            "split-skill-prefix",
            "split-skill-complete",
            "split-skill-exhausted",
            "split-action-prefix",
            "split-action-complete",
            "split-action-exhausted",
            "isolation-mage-first",
            "isolation-archer-second",
            "isolation-archer-rest",
            "isolation-mage-transform",
            "cleanup-eof",
            "cleanup-eof-exhausted",
            "cleanup-explicit-close",
            "cleanup-explicit-exhausted",
        };
        expect(g_c7_checkpoint_labels == expected,
               "C7 executes every finite planned observation exactly once in declared order");
        test_c7_reachable_public_paths(snapshot);
    }

    // C7_COMBAT_PACKET_BASELINE_END

}  // namespace

int main(int argc, char** argv) {

    // C7_COMBAT_CLI_BEGIN
    if (argc > 1 && (std::string_view(argv[1]) == "--combat-packet-tests" ||
                     std::string_view(argv[1]) == "--combat-packet-snapshot")) {
        const bool snapshot = std::string_view(argv[1]) == "--combat-packet-snapshot";
        if (snapshot) {
            openkal::log::set_level(openkal::log::Level::warn);
        }
        test_c7_combat_packets(snapshot);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    // C7_COMBAT_CLI_END

    if (argc > 1 && (std::string_view(argv[1]) == "--status-packet-tests" ||
                     std::string_view(argv[1]) == "--status-packet-snapshot")) {
        const bool snapshot = std::string_view(argv[1]) == "--status-packet-snapshot";
        if (snapshot) {
            openkal::log::set_level(openkal::log::Level::warn);
        }
        test_c6_status_packets(snapshot);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }

    if (argc > 1 && (std::string_view(argv[1]) == "--world-packet-tests" ||
                     std::string_view(argv[1]) == "--world-packet-snapshot")) {
        const bool snapshot = std::string_view(argv[1]) == "--world-packet-snapshot";
        if (snapshot) {
            openkal::log::set_level(openkal::log::Level::warn);
        }
        test_c5_world_packets(snapshot);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }

    if (argc > 1 && (std::string_view(argv[1]) == "--snapshot-packet-tests" ||
                     std::string_view(argv[1]) == "--snapshot-packet-snapshot")) {
        const bool snapshot = std::string_view(argv[1]) == "--snapshot-packet-snapshot";
        if (snapshot) {
            openkal::log::set_level(openkal::log::Level::warn);
        }
        test_c4_snapshot_packets(snapshot);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }

    if (argc > 1 && (std::string_view(argv[1]) == "--quest-packet-tests" ||
                     std::string_view(argv[1]) == "--quest-packet-snapshot")) {
        const bool snapshot = std::string_view(argv[1]) == "--quest-packet-snapshot";
        if (snapshot) {
            openkal::log::set_level(openkal::log::Level::warn);
        }
        test_c3_quest_packets(snapshot);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }

    if (argc > 1 && (std::string_view(argv[1]) == "--inventory-packet-tests" ||
                     std::string_view(argv[1]) == "--inventory-packet-snapshot")) {
        const bool snapshot = std::string_view(argv[1]) == "--inventory-packet-snapshot";
        if (snapshot) {
            openkal::log::set_level(openkal::log::Level::warn);
        }
        test_c2_inventory_packets(snapshot);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }

    if (argc > 1 && (std::string_view(argv[1]) == "--online-packet-tests" ||
                     std::string_view(argv[1]) == "--online-packet-snapshot")) {
        const bool snapshot = std::string_view(argv[1]) == "--online-packet-snapshot";
        if (snapshot) {
            openkal::log::set_level(openkal::log::Level::warn);
        }
        test_c1_online_packets(snapshot);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        benchmark_fifo();
        return openkal::test::g_failures ? 1 : 0;
    }
    frame_boundaries();
    frame_handler_exception();
    fifo_queues();
    remaining_fifo_queues();
    capped_queues();
    player_cancellation();
    for (int count : {32, 256, 2048}) {
        (void) numbered_skill_drain(count);
    }
    player_option_requests();
    handshake_and_exit();
    connection_cleanup_serials();
    cleanup_characterization();
    test_c1_online_packets(false);
    test_c2_inventory_packets(false);
    test_c4_snapshot_packets(false);
    test_c3_quest_packets(false);
    test_c5_world_packets(false);
    test_c6_status_packets(false);
    test_c7_combat_packets(false);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
