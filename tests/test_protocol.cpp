/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Answers.h"
#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/ChatAmount.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/Cipher.h"
#include "openkal/protocol/Constants.h"
#include "openkal/protocol/Cooking.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/Effect.h"
#include "openkal/protocol/EnchantItem.h"
#include "openkal/protocol/ExpEventState.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/GetItemId.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/ItemInfo.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/KnightSkill.h"
#include "openkal/protocol/LevelUp.h"
#include "openkal/protocol/MState.h"
#include "openkal/protocol/MageSkill.h"
#include "openkal/protocol/Mail.h"
#include "openkal/protocol/MapState.h"
#include "openkal/protocol/MixingItem.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PlayerInfo.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Riding.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/Storage.h"
#include "openkal/protocol/Transform.h"
#include "openkal/protocol/TrashItem.h"
#include "openkal/protocol/UpgradeItem.h"
#include "openkal/protocol/UseItem.h"
#include "test_util.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

int main() {
    using openkal::test::expect;
    using namespace openkal::protocol;

    {
        ExpEventStateFields state;
        state.enabled = true;
        state.manager_npc_index = 5037;
        state.donation_item_index = 23016;
        state.contributed = 4294967297ull;
        state.target = 5000000000ull;
        state.next_bonus_percent = 10;
        auto frame = ExpEventState::frame(state);
        std::size_t consumed = 0;
        std::uint8_t opcode = 0;
        std::vector<std::uint8_t> payload;
        expect(FrameParser::pop(frame.data(), frame.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   opcode == static_cast<std::uint8_t>(S2C::ExpEventState),
               "event snapshot frame");
        ExpEventStateFields decoded;
        expect(ExpEventState::parse(payload.data(), payload.size(), decoded) &&
                   decoded.contributed == state.contributed && decoded.target == state.target &&
                   decoded.manager_npc_index == 5037 && decoded.donation_item_index == 23016 &&
                   decoded.next_bonus_percent == 10 && decoded.enabled,
               "event snapshot preserves 64-bit progress and configuration");
        for (std::size_t size = 0; size < payload.size(); ++size) {
            expect(!ExpEventState::parse(payload.data(), size, decoded),
                   "reject truncated event snapshot");
        }
        expect(!ExpEventState::parse(nullptr, payload.size(), decoded),
               "reject null event snapshot");
        payload.push_back(0);
        expect(!ExpEventState::parse(payload.data(), payload.size(), decoded),
               "reject trailing snapshot data");
        payload.pop_back();
        payload[0] = 2;
        expect(!ExpEventState::parse(payload.data(), payload.size(), decoded) &&
                   decoded.contributed == state.contributed,
               "reject invalid flag without replacing snapshot");
        state.active_bonus_percent = 10;
        state.remaining_seconds = 1799;
        frame = ExpEventState::frame(state);
        expect(ExpEventState::parse(frame.data() + 3, frame.size() - 3, decoded) &&
                   decoded.active_bonus_percent == 10 && decoded.remaining_seconds == 1799,
               "active snapshot round trip");
        state.remaining_seconds = 0;
        frame = ExpEventState::frame(state);
        expect(!ExpEventState::parse(frame.data() + 3, frame.size() - 3, decoded),
               "reject inconsistent countdown");
        frame = ExpEventState::frame({});
        expect(ExpEventState::parse(frame.data() + 3, frame.size() - 3, decoded) &&
                   !decoded.enabled,
               "disabled event snapshot");
    }

    expect(static_cast<int>(S2C::Code) == 42, "S2C_CODE==42");
    expect(static_cast<int>(C2S::Connect) == 8, "C2S_CONNECT==8");
    expect(kProtocolSeed == 3793 && kProtocolMagic == 9052, "seed/magic");

    {
        const auto frame = ItemWear::frame_server_put_on(101, 202, 1747);
        std::size_t consumed = 0;
        std::uint8_t opcode = 0;
        std::vector<std::uint8_t> payload;
        expect(FrameParser::pop(frame.data(), frame.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   opcode == static_cast<std::uint8_t>(S2C::PutOnItem),
               "S2C put-on item frame");
        std::uint32_t player_id = 0;
        std::uint32_t iid = 0;
        std::uint16_t item_index = 0;
        expect(parse_s2c_item_wear(payload.data(), payload.size(), player_id, iid, item_index) &&
                   player_id == 101 && iid == 202 && item_index == 1747,
               "S2C put-on item wear round trip");

        const auto timed_frame = ItemWear::frame_server_put_on(101, 202, 1747, 123456789ull);
        expect(
            FrameParser::pop(timed_frame.data(), timed_frame.size(), consumed, opcode, payload) ==
                FrameParseStatus::Complete,
            "S2C timed pet put-on frame");
        std::uint64_t pet_expire_at = 0;
        expect(parse_s2c_item_wear(
                   payload.data(), payload.size(), player_id, iid, item_index, &pet_expire_at) &&
                   pet_expire_at == 123456789ull,
               "S2C timed pet expiry round trip");
    }

    {
        const auto request = Riding::frame_client_toggle();
        std::size_t consumed = 0;
        std::uint8_t opcode = 0;
        std::vector<std::uint8_t> payload;
        expect(FrameParser::pop(request.data(), request.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   opcode == static_cast<std::uint8_t>(C2S::Riding) &&
                   Riding::parse_client_toggle(payload.data(), payload.size()),
               "C2S riding toggle round trip");

        RidingState expected;
        expected.player_id = 101;
        expected.iid = 303;
        expected.item_index = 30000;
        expected.expire_at = 1'234'567'890ull;
        expected.mounted = true;
        expected.kind = RidingKind::Battle;
        expected.grade = BattleRidingGrade::Rare;
        expected.rarity = 1;
        expected.satiety = 83;
        expected.max_satiety = 100;
        expected.revision = 7;
        expected.equipment_iids[0] = 404;
        expected.equipment_item_indices[0] = 30212;
        expected.equipment_enchants[0] = 3;
        expected.abilities[0] = 5;
        const auto response = Riding::frame_server_state(expected);
        expect(FrameParser::pop(response.data(), response.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   opcode == static_cast<std::uint8_t>(S2C::Riding),
               "S2C riding state frame");
        RidingState actual;
        expect(Riding::parse_server_state(payload.data(), payload.size(), actual) &&
                   actual.player_id == expected.player_id && actual.iid == expected.iid &&
                   actual.item_index == expected.item_index &&
                   actual.expire_at == expected.expire_at && actual.mounted &&
                   actual.kind == expected.kind && actual.grade == expected.grade &&
                   actual.rarity == expected.rarity && actual.satiety == expected.satiety &&
                   actual.max_satiety == expected.max_satiety &&
                   actual.revision == expected.revision &&
                   actual.equipment_iids == expected.equipment_iids &&
                   actual.equipment_item_indices == expected.equipment_item_indices &&
                   actual.equipment_enchants == expected.equipment_enchants &&
                   actual.abilities == expected.abilities,
               "S2C riding state round trip");

        BattleRidingRequest expected_request;
        expected_request.operation = BattleRidingOperation::Equip;
        expected_request.operation_id = 44;
        expected_request.mount_iid = 303;
        expected_request.primary_iid = 404;
        const auto battle_request = Riding::frame_client_battle_request(expected_request);
        expect(FrameParser::pop(
                   battle_request.data(), battle_request.size(), consumed, opcode, payload) ==
                   FrameParseStatus::Complete,
               "C2S Battle Riding request frame");
        BattleRidingRequest actual_request;
        expect(
            Riding::parse_client_battle_request(payload.data(), payload.size(), actual_request) &&
                actual_request.operation == expected_request.operation &&
                actual_request.operation_id == expected_request.operation_id &&
                actual_request.mount_iid == expected_request.mount_iid &&
                actual_request.primary_iid == expected_request.primary_iid,
            "C2S Battle Riding request round trip");
        expected_request.operation = BattleRidingOperation::Unequip;
        expected_request.primary_iid = 0;
        const auto unequip_request = Riding::frame_client_battle_request(expected_request);
        expect(FrameParser::pop(
                   unequip_request.data(), unequip_request.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   Riding::parse_client_battle_request(
                       payload.data(), payload.size(), actual_request) &&
                   actual_request.operation == BattleRidingOperation::Unequip &&
                   actual_request.primary_iid == 0,
               "helmet unequip preserves valid slot zero rather than treating it as a missing IID");

        expected_request.operation = BattleRidingOperation::StableExchange;
        expected_request.mount_iid = 0;
        expected_request.primary_iid = 0;
        expected_request.secondary_iid = 1234;
        const auto exchange_request = Riding::frame_client_battle_request(expected_request);
        expect(FrameParser::pop(
                   exchange_request.data(), exchange_request.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   Riding::parse_client_battle_request(
                       payload.data(), payload.size(), actual_request) &&
                   actual_request.operation == BattleRidingOperation::StableExchange &&
                   actual_request.mount_iid == 0 && actual_request.secondary_iid == 1234,
               "Stablemaster exchange permits an operation without a registered mount");
        expect(!battle_riding_operation_requires_mount(BattleRidingOperation::StableExchange) &&
                   !battle_riding_operation_requires_mount(
                       BattleRidingOperation::StableConvertDuplicate) &&
                   battle_riding_operation_requires_mount(BattleRidingOperation::StablePromote),
               "only mount-independent Stablemaster operations permit a zero mount IID");

        expected_request.operation = BattleRidingOperation::StableConvertDuplicate;
        expected_request.primary_iid = 987;
        const auto duplicate_request = Riding::frame_client_battle_request(expected_request);
        expect(FrameParser::pop(
                   duplicate_request.data(), duplicate_request.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   Riding::parse_client_battle_request(
                       payload.data(), payload.size(), actual_request) &&
                   actual_request.operation == BattleRidingOperation::StableConvertDuplicate &&
                   actual_request.mount_iid == 0 && actual_request.primary_iid == 987,
               "Stablemaster duplicate conversion permits an operation without a registered mount");

        RidingCacheResult cache_expected;
        cache_expected.cache_iid = 900;
        cache_expected.seal_item_index = 41322;
        cache_expected.seal_count = 1;
        cache_expected.reward_item_index = 41323;
        cache_expected.reward_count = 2;
        const auto cache_response = Riding::frame_server_cache_result(cache_expected);
        expect(FrameParser::pop(
                   cache_response.data(), cache_response.size(), consumed, opcode, payload) ==
                       FrameParseStatus::Complete &&
                   opcode == static_cast<std::uint8_t>(S2C::RidingCache),
               "S2C Riding Cache result frame");
        RidingCacheResult cache_actual;
        expect(Riding::parse_server_cache_result(payload.data(), payload.size(), cache_actual) &&
                   cache_actual.cache_iid == cache_expected.cache_iid &&
                   cache_actual.seal_item_index == cache_expected.seal_item_index &&
                   cache_actual.seal_count == cache_expected.seal_count &&
                   cache_actual.reward_item_index == cache_expected.reward_item_index &&
                   cache_actual.reward_count == cache_expected.reward_count,
               "S2C Riding Cache result round trip");
    }

    {
        CookingOrder expected;
        expected.blood_fish = 2;
        expected.medicine_from_blood_fish = 3;
        expected.medicine_direct = 4;
        const auto packet = Cooking::frame_c2s(kCookingPotions, expected);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(packet.data(), packet.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(C2S::Cooking),
               "c2s cooking frame");
        std::uint8_t cooking_type = 0;
        CookingOrder actual;
        expect(Cooking::parse_c2s(payload.data(), payload.size(), cooking_type, actual) &&
                   cooking_type == kCookingPotions && actual.blood_fish == 2 &&
                   actual.medicine_from_blood_fish == 3 && actual.medicine_direct == 4,
               "c2s cooking round trip");
    }

    {
        const auto request = GameExit::frame_client_request();
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(request.data(), request.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(C2S::GameExit) &&
                   GameExit::parse_client_request(payload.data(), payload.size()),
               "c2s game exit");

        const auto answer = GameExit::frame_server_answer(true);
        expect(try_pop_frame(answer.data(), answer.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(S2C::AnsGameExit),
               "s2c game exit answer");
        bool saved = false;
        expect(GameExit::parse_server_answer(payload.data(), payload.size(), saved) && saved,
               "parse game exit persistence result");
    }

    {
        const std::uint32_t packed = pack_insert_info(true, 12, 28);
        expect((packed & kInsertReplace) != 0, "replace bit set");
        expect((packed & kInsertHasEndurance) != 0, "endurance bit set");
        expect(insert_cur_end(packed) == 12, "packed current endurance");
        expect(insert_max_end(packed) == 28, "packed max endurance");
        expect(insert_cur_end(kInsertReplace) == 0, "missing endurance flag yields 0");
        expect(insert_max_end(0) == 0, "empty info has no endurance");
        const std::uint32_t worn = pack_insert_info(false, 0, 0, true);
        expect(insert_put_on(worn), "put-on bit set");
        expect((worn & kInsertReplace) == 0, "put-on does not set replace");
        expect(!insert_put_on(packed), "replace pack has no put-on");
        const std::uint32_t bound = pack_insert_info(false, 0, 0, false, true);
        expect(insert_account_bound(bound), "account-bound bit set");
        const auto protected_info = pack_insert_info(true, 3, 30, true, true, true);
        expect((protected_info & kInsertEnchantProtected) != 0 &&
                   insert_cur_end(protected_info) == 3 && insert_max_end(protected_info) == 30 &&
                   insert_put_on(protected_info) && insert_account_bound(protected_info),
               "protection coexists with endurance, worn, replace and binding flags");
        expect((packed & kInsertEnchantProtected) == 0,
               "ordinary and legacy items default to no protection");
        expect(!insert_account_bound(packed), "ordinary item has no account-bound bit");
        const auto character_bound = pack_insert_info(true, 12, 28, true, false, true, true);
        expect(insert_character_bound(character_bound) && !insert_account_bound(character_bound) &&
                   insert_put_on(character_bound) && insert_cur_end(character_bound) == 12 &&
                   insert_max_end(character_bound) == 28 &&
                   (character_bound & kInsertEnchantProtected) != 0,
               "character binding is independent of account binding and other instance flags");
        expect(!insert_character_bound(packed) && !insert_character_bound(bound),
               "legacy and account-bound packets do not imply character binding");
    }

    {
        const std::uint32_t test = 123456789u;
        const std::uint32_t ver = compute_ans_code_version(test);
        expect(ver == (test % kProtocolSeed) + kProtocolMagic, "ans version formula");
        expect(verify_ans_code_version(test, ver), "verify ans version");
        expect(!verify_ans_code_version(test, ver + 1), "reject bad version");
    }

    {
        PacketWriter w;
        expect(w.write_format("bdws", 1, 0x12345678u, 0xABCDu, "hi"), "write_format");
        const auto& p = w.payload();
        // little-endian: 01 78 56 34 12 compatibility behavior AB 68 69 00
        expect(p.size() == 10, "payload size");
        expect(p[0] == 1 && p[1] == 0x78 && p[2] == 0x56 && p[3] == 0x34 && p[4] == 0x12,
               "bd bytes");
        expect(p[5] == 0xCD && p[6] == 0xAB, "w bytes");
        expect(p[7] == 'h' && p[8] == 'i' && p[9] == 0, "s bytes");

        PacketReader r(p);
        std::uint8_t b = 0;
        std::uint32_t d = 0;
        std::uint16_t ww = 0;
        std::string s;
        expect(r.read_format("bdws", &b, &d, &ww, &s), "read_format");
        expect(b == 1 && d == 0x12345678u && ww == 0xABCDu && s == "hi", "roundtrip values");
    }

    {
        PacketWriter w;
        w.write_u32(42);
        const auto framed = w.frame(static_cast<std::uint8_t>(C2S::Connect));
        expect(framed.size() == 7, "connect frame size");  // 3 header + 4 payload
        expect(framed[2] == static_cast<std::uint8_t>(C2S::Connect), "connect opcode");

        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
               "try_pop_frame");
        expect(consumed == framed.size() && type == static_cast<std::uint8_t>(C2S::Connect),
               "pop ok");
        expect(payload.size() == 4, "connect payload");
    }

    {
        const auto connect = build_c2s_connect(1000);
        expect(!connect.empty() && connect[2] == static_cast<std::uint8_t>(C2S::Connect),
               "build_c2s_connect");

        const std::uint32_t test_val = 1500000u;
        const auto ans = build_c2s_ans_code(test_val);
        expect(!ans.empty() && ans[2] == static_cast<std::uint8_t>(C2S::AnsCode),
               "build_c2s_ans_code");

        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(ans.data(), ans.size(), consumed, type, payload), "ans frame");
        PacketReader r(payload);
        std::uint8_t flag = 0;
        std::uint32_t ver = 0;
        expect(r.read_format("bd", &flag, &ver), "ans payload");
        expect(flag == 1 && ver == compute_ans_code_version(test_val), "ans payload values");
    }

    {
        PacketWriter w;
        expect(w.write_format("dbddddbbb", 999u, 7, 1u, 2u, 3u, 4u, 5, 6, 7), "write code fields");
        S2CCodeFields f{};
        expect(parse_s2c_code(w.payload().data(), w.payload().size(), f), "parse_s2c_code");
        expect(f.protocol_test_value == 999u && f.decode_code == 7 && f.server_id == 5 &&
                   f.age_limit == 6 && f.country == 7,
               "s2c code fields");
    }

    {
        expect(static_cast<int>(LoginAns::Ok) == 1, "LOGIN_ANS ok");
        expect(static_cast<int>(CloseConnect::WrongProtocolVersion) == 10, "CC_WRONG_PROTOCOL");
    }

    // Incomplete frame
    {
        std::uint8_t buf[3] = {10, 0, 1};
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(FrameParser::pop(buf, 3, consumed, type, payload) == FrameParseStatus::Incomplete,
               "incomplete frame distinguished");
        expect(!try_pop_frame(buf, 3, consumed, type, payload), "incomplete frame rejected");
    }

    // Invalid lengths must not stall a connection forever as "incomplete" input.
    {
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        const std::uint8_t too_short[2] = {2, 0};
        expect(FrameParser::pop(too_short, sizeof(too_short), consumed, type, payload) ==
                   FrameParseStatus::Invalid,
               "short frame length rejected");
        const std::uint16_t oversized = static_cast<std::uint16_t>(kMaxPacketLength + 1);
        const std::uint8_t too_long[2] = {static_cast<std::uint8_t>(oversized & 0xFFu),
                                          static_cast<std::uint8_t>(oversized >> 8u)};
        expect(FrameParser::pop(too_long, sizeof(too_long), consumed, type, payload) ==
                   FrameParseStatus::Invalid,
               "oversized frame length rejected");
    }

    // Cipher interface: null is identity; custom plug-in can replace later.
    {
        PacketWriter w;
        w.write_u32(0x11223344u);
        auto frame = w.frame(static_cast<std::uint8_t>(C2S::Connect));
        const auto original = frame;
        encode_frame_inplace(frame.data(), frame.size());
        expect(frame == original, "null cipher encode is identity");
        decode_frame_inplace(frame.data(), frame.size());
        expect(frame == original, "null cipher decode is identity");

        struct XorCipher final : IPacketCipher {
            void encode(std::uint8_t* data, std::size_t size) override {
                for (std::size_t i = 0; i < size; ++i) {
                    data[i] ^= 0x5A;
                }
            }
            void decode(std::uint8_t* data, std::size_t size) override {
                encode(data, size);
            }
        };
        set_packet_cipher(std::make_shared<XorCipher>());
        frame = original;
        encode_frame_inplace(frame.data(), frame.size());
        expect(frame != original, "custom cipher mutates");
        expect(frame[0] == original[0] && frame[1] == original[1], "size WORD untouched");
        decode_frame_inplace(frame.data(), frame.size());
        expect(frame == original, "custom cipher roundtrip");

        PacketWriter encrypted_writer;
        encrypted_writer.write_u32(0x11223344u);
        const auto encrypted = encrypted_writer.frame(static_cast<std::uint8_t>(C2S::Connect));
        expect(encrypted != original, "packet writer uses configured cipher");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(FrameParser::pop(encrypted.data(), encrypted.size(), consumed, type, payload) ==
                   FrameParseStatus::Complete,
               "frame parser uses configured cipher");
        expect(type == static_cast<std::uint8_t>(C2S::Connect) && payload.size() == 4,
               "encrypted frame decoded");
        set_packet_cipher(nullptr);  // restore null
    }

    // S2C_PLAYERINFO encode/decode (int32-stat base layout).
    {
        expect(static_cast<int>(S2C::PlayerInfo) == 17, "S2C_PLAYERINFO==17");
        PlayerInfoMsg msg;
        msg.auth_type = 3;
        msg.exp_time = 42;
        PlayerInfoChar a;
        a.pid = 1001;
        a.name = "Hero";
        a.job = 0;
        a.level = 12;
        a.gid = 7;
        a.str = 15;
        a.hth = 14;
        a.intel = 10;
        a.wis = 9;
        a.dex = 11;
        a.face = 2;
        a.hair = 3;
        a.wear = {101, 202};
        PlayerInfoChar b;
        b.pid = 1002;
        b.name = "Mage";
        b.job = 1;
        b.level = 5;
        msg.characters.push_back(a);
        msg.characters.push_back(b);

        const auto frame = frame_s2c_playerinfo(msg);
        expect(!frame.empty(), "playerinfo frame non-empty");
        PlayerInfoMsg decoded;
        expect(parse_s2c_playerinfo(frame, decoded), "parse playerinfo");
        expect(decoded.auth_type == 3 && decoded.exp_time == 42, "playerinfo header");
        expect(decoded.characters.size() == 2, "playerinfo count");
        expect(decoded.characters[0].pid == 1001 && decoded.characters[0].name == "Hero" &&
                   decoded.characters[0].job == 0 && decoded.characters[0].level == 12 &&
                   decoded.characters[0].gid == 7 && decoded.characters[0].str == 15 &&
                   decoded.characters[0].face == 2 && decoded.characters[0].hair == 3 &&
                   decoded.characters[0].wear.size() == 2 && decoded.characters[0].wear[0] == 101 &&
                   decoded.characters[0].wear[1] == 202,
               "playerinfo char0");
        expect(decoded.characters[1].pid == 1002 && decoded.characters[1].name == "Mage" &&
                   decoded.characters[1].job == 1 && decoded.characters[1].wear.empty(),
               "playerinfo char1");

        PlayerInfoMsg empty;
        empty.auth_type = 1;
        const auto empty_frame = frame_s2c_playerinfo(empty);
        PlayerInfoMsg empty_decoded;
        expect(parse_s2c_playerinfo(empty_frame, empty_decoded), "parse empty playerinfo");
        expect(empty_decoded.characters.empty() && empty_decoded.auth_type == 1,
               "empty playerinfo");
    }

    {
        PlayerInfoMsg msg;
        PlayerInfoChar hi;
        hi.pid = 9;
        hi.name = "Wide";
        hi.job = 0;
        hi.level = 300;
        hi.gid = 400;
        hi.str = 300;
        hi.hth = 400;
        hi.intel = 500;
        hi.wis = 600;
        hi.dex = 700;
        hi.face = 1;
        hi.hair = 2;
        msg.characters.push_back(hi);
        const auto frame = frame_s2c_playerinfo(msg);
        PlayerInfoMsg decoded;
        expect(parse_s2c_playerinfo(frame, decoded), "parse wide playerinfo");
        expect(decoded.characters.size() == 1 && decoded.characters[0].level == 300 &&
                   decoded.characters[0].gid == 400 && decoded.characters[0].str == 300 &&
                   decoded.characters[0].hth == 400 && decoded.characters[0].intel == 500 &&
                   decoded.characters[0].wis == 600 && decoded.characters[0].dex == 700 &&
                   decoded.characters[0].face == 1 && decoded.characters[0].hair == 2,
               "playerinfo stats and level above 255");
    }

    {
        const auto batch =
            Property::frame_c2s_update(static_cast<std::uint8_t>(PropertyType::Str), 7);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(batch.data(), batch.size(), consumed, type, payload),
               "batched updateproperty frame");
        std::uint8_t property = 0;
        std::uint8_t count = 0;
        expect(Property::parse_c2s_update(payload.data(), payload.size(), property, count) &&
                   property == static_cast<std::uint8_t>(PropertyType::Str) && count == 7,
               "batched updateproperty payload");
        const std::uint8_t legacy[] = {static_cast<std::uint8_t>(PropertyType::Dex)};
        expect(Property::parse_c2s_update(legacy, sizeof(legacy), property, count) &&
                   property == static_cast<std::uint8_t>(PropertyType::Dex) && count == 1,
               "legacy updateproperty defaults to one step");
        const std::uint8_t zero_batch[] = {static_cast<std::uint8_t>(PropertyType::Hth), 0};
        expect(!Property::parse_c2s_update(zero_batch, sizeof(zero_batch), property, count),
               "zero updateproperty batch rejected");

        const auto scalar =
            frame_s2c_update_property(static_cast<std::uint8_t>(PropertyType::Str), 400);
        expect(!scalar.empty() && scalar[2] == static_cast<std::uint8_t>(S2C::UpdateProperty),
               "updateproperty scalar opcode");
        expect(try_pop_frame(scalar.data(), scalar.size(), consumed, type, payload),
               "updateproperty scalar frame");
        std::int32_t value = 0;
        expect(parse_s2c_update_property(payload.data(), payload.size(), property, value) &&
                   property == static_cast<std::uint8_t>(PropertyType::Str) && value == 400,
               "updateproperty scalar >255");

        const auto vital = frame_s2c_update_property_vital(
            static_cast<std::uint8_t>(PropertyType::Hp), 70000, 100000);
        expect(try_pop_frame(vital.data(), vital.size(), consumed, type, payload),
               "updateproperty vital frame");
        UpdatePropertyFields fields;
        expect(parse_s2c_update_property_any(payload.data(), payload.size(), fields) &&
                   fields.kind == UpdatePropertyKind::Vital && fields.vital_cur == 70000 &&
                   fields.vital_max == 100000,
               "updateproperty vital >65535");
    }

    {
        expect(static_cast<int>(InfoMsg::SpecialtyUp) == 37, "MSG_SPECIALTYUP==37");
        expect(kStartingSpecialty == 1u, "wandering specialty bit");
        expect(specialty_title_index(1) == 0, "title 0 for wandering");
        expect(specialty_title_index(3) == 1, "title 1 for first job");
        expect(specialty_title_index(7) == 2, "title 2 for second-job path A");
        expect(specialty_title_index(11) == 3, "title 3 for second-job path B");
        expect(specialty_title_index(23) == 4, "title 4 for third-job path A");
        expect(specialty_title_index(43) == 5, "title 5 for third-job path B");
        expect(kSpecialtyFirstJob == 3u, "first job mask");
        expect(kSpecialtySecondJobA == 7u, "second job path A");
        expect(kSpecialtySecondJobB == 11u, "second job path B");
        expect(kSpecialtyThirdJobA == 23u, "third job path A");
        expect(kSpecialtyThirdJobB == 43u, "third job path B");
        expect(specialty_meets_grade(1, 1), "wandering meets common");
        expect(!specialty_meets_grade(1, 3), "wandering lacks first job");
        expect(specialty_meets_grade(3, 3), "first job meets grade 3");
        expect(specialty_meets_grade(3, 1), "first job still meets common");
        expect(specialty_meets_grade(7, 3) && specialty_meets_grade(7, 7),
               "vagabond path keeps first job");
        expect(!specialty_meets_grade(7, 11), "vagabond is not commander");
        expect(specialty_meets_grade(11, 3) && specialty_meets_grade(11, 11),
               "commander path keeps first job");
        expect(!specialty_meets_grade(11, 7), "commander is not vagabond");
        expect(specialty_meets_grade(23, 7) && specialty_meets_grade(23, 23),
               "god of sword follows vagabond");
        expect(!specialty_meets_grade(23, 11), "god of sword is not commander");
        expect(specialty_meets_grade(43, 11) && specialty_meets_grade(43, 43),
               "general follows commander");
        expect(!specialty_meets_grade(43, 7), "general is not vagabond");
        const auto msgv =
            ItemMonster::frame_message_v(static_cast<std::uint8_t>(InfoMsg::SpecialtyUp), 3);
        expect(!msgv.empty() && msgv[2] == static_cast<std::uint8_t>(S2C::MessageV),
               "specialtyup messagev opcode");
    }

    {
        const auto atk = frame_s2c_attack(1, 2, 70000, 80000, 0);
        expect(!atk.empty() && atk[2] == static_cast<std::uint8_t>(S2C::Attack), "s2c attack op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(atk.data(), atk.size(), consumed, type, payload), "s2c attack frame");
        std::uint32_t attacker = 0, target = 0;
        std::int32_t damage = 0, palsy = 0;
        std::uint8_t flags = 0;
        expect(parse_s2c_attack(
                   payload.data(), payload.size(), attacker, target, damage, palsy, flags) &&
                   attacker == 1 && target == 2 && damage == 70000 && palsy == 80000,
               "s2c attack damage >65535");

        const auto sk = frame_s2c_skill_damage(2, 1, 2, 1, 90000, 100000, 0);
        expect(!sk.empty() && sk[2] == static_cast<std::uint8_t>(S2C::Skill), "s2c skill op");
        expect(try_pop_frame(sk.data(), sk.size(), consumed, type, payload), "s2c skill frame");
        S2CSkillFields skill_fields;
        expect(parse_s2c_skill(payload.data(), payload.size(), skill_fields) &&
                   skill_fields.damage == 90000 && skill_fields.palsy == 100000,
               "s2c skill damage >65535");
    }

    {
        const auto on = frame_c2s_move_on(10, -20, 3);
        expect(!on.empty() && on[2] == static_cast<std::uint8_t>(C2S::MoveOn),
               "c2s move on opcode");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(on.data(), on.size(), consumed, type, payload), "c2s move on frame");
        std::int8_t dx = 0, dy = 0, dz = 0;
        expect(parse_c2s_move(payload.data(), payload.size(), dx, dy, dz), "parse c2s move");
        expect(dx == 10 && dy == -20 && dz == 3, "c2s move values");

        const auto end = frame_c2s_move_end(0, 0, 0);
        expect(!end.empty() && end[2] == static_cast<std::uint8_t>(C2S::MoveEnd), "c2s move end");

        const auto s2c = frame_s2c_move_player_on(42, 5, -1, 0);
        expect(!s2c.empty() && s2c[2] == static_cast<std::uint8_t>(S2C::MovePlayerOn),
               "s2c moveplayer on");
        expect(try_pop_frame(s2c.data(), s2c.size(), consumed, type, payload), "s2c move frame");
        std::uint32_t id = 0;
        expect(parse_s2c_move_player(payload.data(), payload.size(), id, dx, dy, dz),
               "parse s2c moveplayer");
        expect(id == 42 && dx == 5 && dy == -1 && dz == 0, "s2c moveplayer values");
    }

    {
        expect(static_cast<int>(C2S::Chatting) == 17, "C2S_CHATTING==17");
        expect(static_cast<int>(S2C::Chatting) == 60, "S2C_CHATTING==60");

        const auto c2s = Chatting::frame_c2s("hello");
        expect(!c2s.empty() && c2s[2] == static_cast<std::uint8_t>(C2S::Chatting),
               "c2s chatting op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(c2s.data(), c2s.size(), consumed, type, payload),
               "c2s chatting frame");
        std::string text;
        expect(Chatting::parse_c2s(payload.data(), payload.size(), text) && text == "hello",
               "c2s chatting roundtrip");

        const auto s2c = Chatting::frame_s2c("Hero", "@Bob hi");
        expect(!s2c.empty() && s2c[2] == static_cast<std::uint8_t>(S2C::Chatting),
               "s2c chatting op");
        expect(try_pop_frame(s2c.data(), s2c.size(), consumed, type, payload),
               "s2c chatting frame");
        std::string speaker;
        expect(Chatting::parse_s2c(payload.data(), payload.size(), speaker, text) &&
                   speaker == "Hero" && text == "@Bob hi",
               "s2c chatting roundtrip");

        expect(Chatting::channel_of("hi") == ChatChannel::Common, "channel common");
        expect(Chatting::channel_of("#party") == ChatChannel::Party, "channel party");
        expect(Chatting::channel_of("$g") == ChatChannel::Guild, "channel guild");
        expect(Chatting::channel_of("@n x") == ChatChannel::Whisper, "channel whisper");
        expect(Chatting::channel_of("^yell") == ChatChannel::Shout, "channel shout");
        for (const auto text : {"#Party!", "$Guild!", "~Alliance!", "^Shout!", "!Normal!"}) {
            expect(Chatting::display_body(text) == std::string_view(text).substr(1),
                   "display omits only the channel marker");
        }
        expect(Chatting::display_body("@Bob Hello, #friend!") == "Hello, #friend!",
               "personal message display omits recipient and keeps content punctuation");
        expect(Chatting::display_body("@  Bob\t  Hello!") == "Hello!",
               "display and routing agree on whisper whitespace");
        expect(Chatting::display_body("@Bob").empty() && Chatting::display_body("@").empty() &&
                   Chatting::display_body("").empty(),
               "empty personal-message bodies are safe");
        expect(Chatting::display_body("Hello @Bob #1!") == "Hello @Bob #1!",
               "ordinary message punctuation remains unchanged");
        expect(Chatting::is_slash_command("/time"), "slash time");
        expect(!Chatting::is_slash_command("/s hello"), "slash not shout");
        expect(Chatting::command_name("/Time now") == "time", "command name");
        std::string target;
        std::string body;
        expect(Chatting::parse_whisper("@Bob hi there", target, body) && target == "Bob" &&
                   body == "hi there",
               "parse whisper");
    }

    {
        const auto rest = Skill::frame_rest(1);
        expect(!rest.empty() && rest[2] == static_cast<std::uint8_t>(C2S::Rest), "c2s rest op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(rest.data(), rest.size(), consumed, type, payload), "c2s rest frame");
        std::uint8_t flag = 0;
        expect(Skill::parse_rest(payload.data(), payload.size(), flag) && flag == 1,
               "c2s rest b=1");

        const auto stand = Skill::frame_rest(0);
        expect(try_pop_frame(stand.data(), stand.size(), consumed, type, payload),
               "c2s rest 0 frame");
        expect(Skill::parse_rest(payload.data(), payload.size(), flag) && flag == 0,
               "c2s rest b=0");

        const auto act = Skill::frame_action_rest(42, 1);
        expect(!act.empty() && act[2] == static_cast<std::uint8_t>(S2C::Action),
               "s2c action rest op");
        expect(try_pop_frame(act.data(), act.size(), consumed, type, payload),
               "s2c action rest frame");
        std::uint32_t id = 0;
        std::uint8_t action = 0;
        std::uint8_t arg = 0;
        expect(Skill::parse_action(payload.data(), payload.size(), id, action, arg),
               "parse s2c action rest");
        expect(id == 42 && action == kAtRest && arg == 1, "s2c action AT_REST sit");

        const auto pre = Skill::frame_preskill(3, 1001);
        expect(!pre.empty() && pre[2] == static_cast<std::uint8_t>(C2S::PreSkill),
               "c2s preskill op");
        expect(try_pop_frame(pre.data(), pre.size(), consumed, type, payload),
               "c2s preskill frame");
        std::uint8_t skill = 0;
        std::uint32_t target = 0;
        expect(Skill::parse_preskill(payload.data(), payload.size(), skill, target) && skill == 3 &&
                   target == 1001,
               "parse c2s preskill");

        const auto ground = Skill::frame_client_ground(47, -1234, 5678);
        expect(try_pop_frame(ground.data(), ground.size(), consumed, type, payload),
               "c2s ground skill frame");
        const std::uint8_t* ground_rest = nullptr;
        std::size_t ground_rest_size = 0;
        std::int32_t ground_x = 0;
        std::int32_t ground_y = 0;
        expect(Skill::parse_client_header(
                   payload.data(), payload.size(), skill, ground_rest, ground_rest_size) &&
                   skill == 47 &&
                   Skill::parse_client_ground(ground_rest, ground_rest_size, ground_x, ground_y) &&
                   ground_x == -1234 && ground_y == 5678,
               "ground skill coordinate roundtrip");
        expect(!Skill::parse_client_ground(ground_rest, ground_rest_size - 1, ground_x, ground_y),
               "ground skill rejects truncated coordinates");

        const auto at_skill = Skill::frame_action_skill(42, 3, 1001);
        expect(!at_skill.empty() && at_skill[2] == static_cast<std::uint8_t>(S2C::Action),
               "s2c action skill op");
        expect(try_pop_frame(at_skill.data(), at_skill.size(), consumed, type, payload),
               "s2c action skill frame");
        std::uint32_t caster = 0;
        expect(Skill::parse_action_skill(payload.data(), payload.size(), caster, skill, target) &&
                   caster == 42 && skill == 3 && target == 1001,
               "parse s2c action AT_SKILL");

        const auto knee = Skill::frame_action_knee(77);
        expect(!knee.empty() && knee[2] == static_cast<std::uint8_t>(S2C::Action),
               "s2c action knee op");
        expect(try_pop_frame(knee.data(), knee.size(), consumed, type, payload),
               "s2c action knee frame");
        expect(Skill::parse_action(payload.data(), payload.size(), id, action, arg) && id == 77 &&
                   action == kAtKnee,
               "parse s2c action AT_KNEE");

        const auto die = Skill::frame_action_die(77);
        expect(try_pop_frame(die.data(), die.size(), consumed, type, payload),
               "s2c action die frame");
        expect(Skill::parse_action(payload.data(), payload.size(), id, action, arg) && id == 77 &&
                   action == kAtDie,
               "parse s2c action AT_DIE");

        const auto beheaded = Skill::frame_action_beheaded(77);
        expect(try_pop_frame(beheaded.data(), beheaded.size(), consumed, type, payload),
               "s2c action beheaded frame");
        expect(Skill::parse_action(payload.data(), payload.size(), id, action, arg) && id == 77 &&
                   action == kAtBeheaded,
               "parse s2c action AT_BEHEADED");
    }

    {
        const auto save = Skill::frame_client_save_revival_point(12, 0x12345678u);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(save.data(), save.size(), consumed, type, payload),
               "c2s save revival point frame");
        expect(type == static_cast<std::uint8_t>(C2S::SaveRevivalPt) && payload.size() == 5 &&
                   payload[0] == 12 && payload[1] == 0x78 && payload[2] == 0x56 &&
                   payload[3] == 0x34 && payload[4] == 0x12,
               "c2s save revival point payload");
    }

    {
        UpdateItemInfoFields in;
        in.iid = 1001;
        in.index = 337;
        in.level = 2;
        in.exp = 10;
        in.added = 10;
        const auto frame = ItemInfo::frame_update(in);
        expect(!frame.empty() && frame[2] == static_cast<std::uint8_t>(S2C::UpdateItemInfo),
               "s2c updateiteminfo op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(frame.data(), frame.size(), consumed, type, payload),
               "s2c updateiteminfo frame");
        UpdateItemInfoFields out;
        expect(ItemInfo::parse_update(payload.data(), payload.size(), out), "parse updateiteminfo");
        expect(out.iid == 1001 && out.index == 337 && out.level == 2 && out.exp == 10 &&
                   out.added == 10,
               "updateiteminfo fields");
    }

    {
        expect(Quest::make_key(1, 2) == 0x00010002u, "quest make_key");
        expect(Quest::key_quest(0x00010002u) == 1 && Quest::key_flag(0x00010002u) == 2,
               "quest key split");

        const auto ask = Quest::frame_ask_npc(10101);
        expect(!ask.empty() && ask[2] == static_cast<std::uint8_t>(C2S::AskNpc), "c2s asknpc op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(ask.data(), ask.size(), consumed, type, payload), "c2s asknpc frame");
        std::uint32_t npc_id = 0;
        expect(Quest::parse_ask_npc(payload.data(), payload.size(), npc_id) && npc_id == 10101,
               "parse asknpc");

        const auto q = Quest::frame_quest(Quest::make_key(1, 2));
        expect(!q.empty() && q[2] == static_cast<std::uint8_t>(C2S::Quest), "c2s quest op");
        expect(try_pop_frame(q.data(), q.size(), consumed, type, payload), "c2s quest frame");
        std::uint32_t key = 0;
        expect(Quest::parse_quest(payload.data(), payload.size(), key) &&
                   key == Quest::make_key(1, 2),
               "parse quest");

        const auto html = Quest::frame_send_html(900002);
        expect(!html.empty() && html[2] == static_cast<std::uint8_t>(S2C::SendHtml),
               "s2c sendhtml op");
        expect(try_pop_frame(html.data(), html.size(), consumed, type, payload),
               "s2c sendhtml frame");
        std::int32_t html_id = 0;
        expect(Quest::parse_send_html(payload.data(), payload.size(), html_id) && html_id == 900002,
               "parse sendhtml");
        npc_id = 99;
        expect(Quest::parse_send_html(payload.data(), payload.size(), html_id, &npc_id) &&
                   npc_id == 0 && payload.size() == 4,
               "legacy dialog packets retain their format and have no explicit NPC context");
        const auto contextual_html = Quest::frame_send_html(900003, 12345);
        expect(try_pop_frame(
                   contextual_html.data(), contextual_html.size(), consumed, type, payload) &&
                   Quest::parse_send_html(payload.data(), payload.size(), html_id, &npc_id) &&
                   html_id == 900003 && npc_id == 12345,
               "dialog replies identify their source NPC for passive observers");
        expect(Quest::parse_send_html(payload.data(), payload.size(), html_id) && html_id == 900003,
               "existing HTML-only consumers accept contextual replies");
        expect(!Quest::parse_send_html(payload.data(), 6, html_id, &npc_id),
               "truncated source NPC fields are rejected");

        QuestFlagEntry in_flag{1, 2, 0};
        const auto save = Quest::frame_save_quest(in_flag);
        expect(!save.empty() && save[2] == static_cast<std::uint8_t>(S2C::SaveQuest),
               "s2c savequest op");
        expect(try_pop_frame(save.data(), save.size(), consumed, type, payload),
               "s2c savequest frame");
        QuestFlagEntry out_flag;
        expect(Quest::parse_save_quest(payload.data(), payload.size(), out_flag) &&
                   out_flag.quest == 1 && out_flag.flag == 2 && out_flag.clear == 0,
               "parse savequest");

        std::vector<QuestFlagEntry> bulk{{1, 1, 0}, {2, 3, 1}};
        const auto info = Quest::frame_quest_info(bulk);
        expect(!info.empty() && info[2] == static_cast<std::uint8_t>(S2C::QuestInfo),
               "s2c questinfo op");
        expect(try_pop_frame(info.data(), info.size(), consumed, type, payload),
               "s2c questinfo frame");
        std::vector<QuestFlagEntry> parsed;
        expect(Quest::parse_quest_info(payload.data(), payload.size(), parsed) &&
                   parsed.size() == 2 && parsed[0].quest == 1 && parsed[1].clear == 1,
               "parse questinfo");

        CreateNpcInfo npc_in;
        npc_in.id = 10101;
        npc_in.index = 72;
        npc_in.shape = 3;
        npc_in.x = 257032;
        npc_in.y = 257425;
        npc_in.z = 16000;
        npc_in.dir = 1000;
        const auto cn = Quest::frame_create_npc(npc_in);
        expect(!cn.empty() && cn[2] == static_cast<std::uint8_t>(S2C::CreateNpc),
               "s2c createnpc op");
        expect(try_pop_frame(cn.data(), cn.size(), consumed, type, payload), "s2c createnpc frame");
        CreateNpcInfo npc_out;
        expect(Quest::parse_create_npc(payload.data(), payload.size(), npc_out) &&
                   npc_out.id == 10101 && npc_out.index == 72 && npc_out.x == 257032,
               "parse createnpc");

        const auto rn = Quest::frame_remove_npc(10101);
        expect(!rn.empty() && rn[2] == static_cast<std::uint8_t>(S2C::RemoveNpc),
               "s2c removenpc op");
        expect(try_pop_frame(rn.data(), rn.size(), consumed, type, payload), "s2c removenpc frame");
        expect(Quest::parse_remove_npc(payload.data(), payload.size(), npc_id) && npc_id == 10101,
               "parse removenpc");
    }

    {
        const auto up = Skill::frame_skill_up(3, 2);
        expect(!up.empty() && up[2] == static_cast<std::uint8_t>(S2C::SkillUp), "s2c skillup op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(up.data(), up.size(), consumed, type, payload), "s2c skillup frame");
        std::uint8_t index = 0;
        std::uint8_t level = 0;
        expect(Skill::parse_skill_up(payload.data(), payload.size(), index, level) && index == 3 &&
                   level == 2,
               "parse s2c skillup");

        const auto learn = Skill::frame_client_learn_skill(40);
        expect(!learn.empty() && learn[2] == static_cast<std::uint8_t>(C2S::LearnSkill),
               "c2s learnskill op");
        expect(try_pop_frame(learn.data(), learn.size(), consumed, type, payload),
               "c2s learnskill frame");
        std::uint8_t learned_index = 0;
        expect(Skill::parse_client_skill_index(payload.data(), payload.size(), learned_index) &&
                   learned_index == 40,
               "parse c2s learnskill");
        const auto skill_up = Skill::frame_client_skill_up(42);
        expect(!skill_up.empty() && skill_up[2] == static_cast<std::uint8_t>(C2S::SkillUp),
               "c2s skillup op");
        expect(try_pop_frame(skill_up.data(), skill_up.size(), consumed, type, payload),
               "c2s skillup frame");
        expect(Skill::parse_client_skill_index(payload.data(), payload.size(), learned_index) &&
                   learned_index == 42,
               "parse c2s skillup index");

        std::vector<SkillRankEntry> bulk{{2, 1}, {3, 5}};
        const auto info = Skill::frame_skill_info(bulk);
        expect(!info.empty() && info[2] == static_cast<std::uint8_t>(S2C::SkillInfo),
               "s2c skillinfo op");
        expect(try_pop_frame(info.data(), info.size(), consumed, type, payload),
               "s2c skillinfo frame");
        std::vector<SkillRankEntry> parsed;
        expect(Skill::parse_skill_info(payload.data(), payload.size(), parsed) &&
                   parsed.size() == 2 && parsed[0].index == 2 && parsed[0].level == 1 &&
                   parsed[1].index == 3 && parsed[1].level == 5,
               "parse s2c skillinfo");
    }

    {
        ShortcutBar bar;
        bar.active_index = 4;
        bar.slots[0] = 4;
        bar.slots[3] = 7;
        bar.slots[10] = 12;
        bar.slots[19] = shortcut_pack_item(47);
        const auto save = Shortcut::frame_client_save(bar);
        expect(!save.empty() && save[2] == static_cast<std::uint8_t>(C2S::Shortcut),
               "c2s shortcut op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(save.data(), save.size(), consumed, type, payload),
               "c2s shortcut frame");
        ShortcutBar parsed;
        expect(Shortcut::parse_client_save(payload.data(), payload.size(), parsed) &&
                   parsed.active_index == 4 && parsed.slots[0] == 4 && parsed.slots[3] == 7 &&
                   parsed.slots[1] == -1 && parsed.slots[10] == 12 &&
                   parsed.slots[19] == shortcut_pack_item(47),
               "parse c2s shortcut");

        const auto empty = Shortcut::frame_server(nullptr);
        expect(!empty.empty() && empty[2] == static_cast<std::uint8_t>(S2C::Shortcut),
               "s2c shortcut empty op");
        expect(try_pop_frame(empty.data(), empty.size(), consumed, type, payload),
               "s2c shortcut empty frame");
        bool present = true;
        ShortcutBar loaded;
        loaded.active_index = 9;
        expect(Shortcut::parse_server(payload.data(), payload.size(), present, loaded) &&
                   !present && loaded.active_index == -1,
               "parse s2c shortcut flag 0");

        const auto filled = Shortcut::frame_server(&bar);
        expect(try_pop_frame(filled.data(), filled.size(), consumed, type, payload),
               "s2c shortcut filled frame");
        expect(Shortcut::parse_server(payload.data(), payload.size(), present, loaded) && present &&
                   loaded.active_index == 4 && loaded.slots[0] == 4 && loaded.slots[3] == 7,
               "parse s2c shortcut flag 1");

        const auto legacy = std::vector<std::uint8_t>(payload.begin(), payload.begin() + 23);
        expect(Shortcut::parse_server(legacy.data(), legacy.size(), present, loaded) &&
                   loaded.slots[3] == 7 && loaded.slots[10] == -1 && loaded.slots[19] == -1,
               "legacy ten-slot bars migrate with empty upper row");
        for (std::size_t truncated = 24; truncated < 43; ++truncated) {
            expect(!Shortcut::parse_client_save(payload.data(), truncated, parsed),
                   "reject partially received upper-row saves");
        }
        expect(shortcut_is_skill(4) && !shortcut_is_item(4), "raw skill binding");
        const int packed_med = shortcut_pack_item(47);
        expect(packed_med == (kShortcutItemFlag | 47) && shortcut_is_item(packed_med) &&
                   shortcut_item_index(packed_med) == 47,
               "item binding packs item definition index");
        bar.slots[1] = packed_med;
        const auto with_item = Shortcut::frame_client_save(bar);
        expect(try_pop_frame(with_item.data(), with_item.size(), consumed, type, payload),
               "c2s shortcut with item frame");
        expect(Shortcut::parse_client_save(payload.data(), payload.size(), parsed) &&
                   parsed.slots[1] == packed_med && parsed.slots[0] == 4,
               "parse packed item shortcut");
    }

    {
        GetItemIdArgs parsed;
        expect(GetItemId::parse_args("101", parsed) && parsed.index == 101 && parsed.prefix == 0 &&
                   parsed.count == 1,
               "getitemid index only");
        expect(GetItemId::parse_args("48 100", parsed) && parsed.index == 48 &&
                   parsed.prefix == 0 && parsed.count == 100,
               "getitemid index count");
        expect(GetItemId::parse_args("101 92 1", parsed) && parsed.index == 101 &&
                   parsed.prefix == 92 && parsed.count == 1,
               "getitemid index prefix count");
        expect(GetItemId::parse_args("  101   92   1  ", parsed) && parsed.index == 101 &&
                   parsed.prefix == 92 && parsed.count == 1,
               "getitemid trims whitespace");
        expect(!GetItemId::parse_args("", parsed), "getitemid rejects empty");
        expect(!GetItemId::parse_args("101 92 1 2", parsed), "getitemid rejects extra args");
        expect(!GetItemId::parse_args("0", parsed), "getitemid rejects index 0");
        expect(!GetItemId::parse_args("101 -1", parsed), "getitemid rejects negative count");
        expect(!GetItemId::parse_args("101 256 1", parsed), "getitemid rejects prefix above 255");
        expect(!GetItemId::parse_args("sword", parsed), "getitemid rejects non-numeric");
    }

    {
        LevelUpArgs parsed;
        expect(LevelUp::parse_args("", parsed) && parsed.op == LevelOp::Add && parsed.value == 1,
               "level empty is +=1");
        expect(LevelUp::parse_args("   ", parsed) && parsed.op == LevelOp::Add && parsed.value == 1,
               "level whitespace is +=1");
        expect(LevelUp::parse_args("+=5", parsed) && parsed.op == LevelOp::Add && parsed.value == 5,
               "level +=5");
        expect(LevelUp::parse_args("  +=12  ", parsed) && parsed.op == LevelOp::Add &&
                   parsed.value == 12,
               "level += trims");
        expect(LevelUp::parse_args("-=5", parsed) && parsed.op == LevelOp::Subtract &&
                   parsed.value == 5,
               "level -=5");
        expect(LevelUp::parse_args("=6", parsed) && parsed.op == LevelOp::Set && parsed.value == 6,
               "level =6");
        expect(!LevelUp::parse_args("5", parsed), "level rejects bare number");
        expect(!LevelUp::parse_args("+=0", parsed), "level rejects +=0");
        expect(!LevelUp::parse_args("-=0", parsed), "level rejects -=0");
        expect(!LevelUp::parse_args("=0", parsed), "level rejects =0");
        expect(!LevelUp::parse_args("+=-1", parsed), "level rejects += negative");
        expect(!LevelUp::parse_args("+=1 2", parsed), "level rejects extra args");
        expect(!LevelUp::parse_args("foo", parsed), "level rejects non-numeric");
        expect(!LevelUp::parse_args("+=", parsed), "level rejects empty += value");
    }

    {
        ChatAmountArgs parsed;
        expect(!ChatAmount::parse_args("", parsed), "amount rejects empty");
        expect(!ChatAmount::parse_args("   ", parsed), "amount rejects whitespace");
        expect(ChatAmount::parse_args("+=5", parsed) && parsed.op == ChatAmountOp::Add &&
                   parsed.value == 5,
               "amount +=5");
        expect(ChatAmount::parse_args("  -=12  ", parsed) && parsed.op == ChatAmountOp::Subtract &&
                   parsed.value == 12,
               "amount -= trims");
        expect(ChatAmount::parse_args("=0", parsed) && parsed.op == ChatAmountOp::Set &&
                   parsed.value == 0,
               "amount =0");
        expect(ChatAmount::parse_args("=1000000", parsed) && parsed.op == ChatAmountOp::Set &&
                   parsed.value == 1000000,
               "amount = large");
        expect(!ChatAmount::parse_args("5", parsed), "amount rejects bare number");
        expect(!ChatAmount::parse_args("+=0", parsed), "amount rejects +=0");
        expect(!ChatAmount::parse_args("-=0", parsed), "amount rejects -=0");
        expect(!ChatAmount::parse_args("=-1", parsed), "amount rejects = negative");
        expect(!ChatAmount::parse_args("+=-1", parsed), "amount rejects += negative");
        expect(!ChatAmount::parse_args("+=1 2", parsed), "amount rejects extra args");
        expect(!ChatAmount::parse_args("foo", parsed), "amount rejects non-numeric");
        {
            ChatAmountArgs add{ChatAmountOp::Add, 5};
            expect(ChatAmount::apply(10, add) == 15, "amount apply add");
            ChatAmountArgs sub{ChatAmountOp::Subtract, 3};
            expect(ChatAmount::apply(10, sub) == 7, "amount apply sub");
            ChatAmountArgs set{ChatAmountOp::Set, 2};
            expect(ChatAmount::apply(10, set) == 2, "amount apply set");
        }
        {
            int level = 1;
            int exp = need_transform_exp_for_level(1) + 100;
            expect(consume_transform_exp_into_levels(level, exp) && level == 2 && exp == 100,
                   "egg exp pool consumes one level");
            int max_lv = kMaxTransformLevel;
            int leftover = 50;
            expect(!consume_transform_exp_into_levels(max_lv, leftover) && leftover == 50,
                   "egg exp pool stops at max level");
        }
        expect(transform_egg_image_ex_index(1) == -1, "egg icon base lv1");
        expect(transform_egg_image_ex_index(10) == -1, "egg icon base lv10");
        expect(transform_egg_image_ex_index(11) == 0, "egg iconex 0 at 11");
        expect(transform_egg_image_ex_index(20) == 0, "egg iconex 0 at 20");
        expect(transform_egg_image_ex_index(21) == 1, "egg iconex 1 at 21");
        expect(transform_egg_image_ex_index(30) == 1, "egg iconex 1 at 30");
        expect(transform_egg_image_ex_index(31) == 2, "egg iconex 2 at 31");
    }

    {
        const auto framed = EnchantItem::frame_c2s(11, 22);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
               "enchant c2s frame");
        expect(type == static_cast<std::uint8_t>(C2S::EnchantItem), "enchant opcode");
        std::uint32_t from_iid = 0;
        std::uint32_t to_iid = 0;
        expect(EnchantItem::parse_c2s(payload.data(), payload.size(), from_iid, to_iid) &&
                   from_iid == 11 && to_iid == 22,
               "enchant parse dd");
        expect(!EnchantItem::parse_c2s(payload.data(), 3, from_iid, to_iid),
               "enchant rejects short payload");
    }

    {
        const auto framed = DropItem::frame_c2s(42, 7);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
               "dropitem c2s frame");
        expect(type == static_cast<std::uint8_t>(C2S::DropItem), "dropitem opcode");
        std::uint32_t iid = 0;
        std::uint32_t count = 0;
        expect(DropItem::parse_c2s(payload.data(), payload.size(), iid, count) && iid == 42 &&
                   count == 7,
               "dropitem parse dd");
        expect(!DropItem::parse_c2s(payload.data(), 3, iid, count),
               "dropitem rejects short payload");
    }

    {
        const auto framed = TrashItem::frame_c2s(42);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
               "trashitem c2s frame");
        expect(type == static_cast<std::uint8_t>(C2S::TrashItem), "trashitem c2s opcode");
        std::uint32_t iid = 0;
        expect(TrashItem::parse_c2s(payload.data(), payload.size(), iid) && iid == 42,
               "trashitem parse d");
        expect(!TrashItem::parse_c2s(payload.data(), 3, iid), "trashitem rejects short c2s");

        const auto s2c = TrashItem::frame_s2c(42, kItemLogDelete);
        expect(try_pop_frame(s2c.data(), s2c.size(), consumed, type, payload),
               "trashitem s2c frame");
        expect(type == static_cast<std::uint8_t>(S2C::TrashItem), "trashitem s2c opcode");
        std::uint8_t log_type = 0;
        expect(TrashItem::parse_s2c(payload.data(), payload.size(), iid, log_type) && iid == 42 &&
                   log_type == kItemLogDelete,
               "trashitem parse db");
        expect(!TrashItem::parse_s2c(payload.data(), 4, iid, log_type),
               "trashitem rejects short s2c");
    }

    {
        const auto framed = UseItem::frame_c2s(42);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
               "useitem c2s frame");
        expect(type == static_cast<std::uint8_t>(C2S::UseItem), "useitem opcode");
        std::uint32_t iid = 0;
        expect(UseItem::parse_c2s(payload.data(), payload.size(), iid) && iid == 42,
               "useitem parse d");
        expect(!UseItem::parse_c2s(payload.data(), 3, iid), "useitem rejects short c2s");
        std::uint8_t percent = 99;
        expect(UseItem::parse_c2s(payload.data(), payload.size(), iid, &percent) && percent == 0,
               "manual use keeps the four-byte payload");
        for (const std::uint8_t threshold : {25, 50, 75, 100}) {
            const auto auto_frame = UseItem::frame_c2s(42, threshold);
            expect(try_pop_frame(auto_frame.data(), auto_frame.size(), consumed, type, payload) &&
                       UseItem::parse_c2s(payload.data(), payload.size(), iid, &percent) &&
                       iid == 42 && percent == threshold,
                   "auto hp request carries its threshold");
            expect(!UseItem::auto_hp_eligible(threshold * 10, 1000, threshold) &&
                       UseItem::auto_hp_eligible(threshold * 10 - 1, 1000, threshold) &&
                       !UseItem::auto_hp_eligible(threshold * 10 + 1, 1000, threshold),
                   "auto hp triggers strictly below each threshold");
        }
        payload[4] = 60;
        expect(!UseItem::parse_c2s(payload.data(), payload.size(), iid, &percent),
               "reject unsupported threshold");
        payload.push_back(25);
        expect(!UseItem::parse_c2s(payload.data(), payload.size(), iid, &percent),
               "reject trailing auto hp request data");
        expect(!UseItem::auto_hp_eligible(0, 1000, 100) &&
                   !UseItem::auto_hp_eligible(-1, 1000, 75) &&
                   !UseItem::auto_hp_eligible(1, 0, 75) &&
                   !UseItem::auto_hp_eligible(1, 1000, 60) && UseItem::auto_hp_eligible(1, 3, 50) &&
                   UseItem::auto_hp_eligible(1000000000, 2000000000, 75),
               "auto hp rejects death and invalid stats without rounding or overflow");
    }

    {
        const auto framed = UpgradeItem::frame_c2s(UpgradeItemType::RateUp, 42);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
               "upgrade c2s frame");
        expect(type == static_cast<std::uint8_t>(C2S::UpgradeItem), "upgrade opcode");
        UpgradeItemType parsed_type = UpgradeItemType::Destroy;
        std::uint32_t iid = 0;
        expect(UpgradeItem::parse_c2s(payload.data(), payload.size(), parsed_type, iid) &&
                   parsed_type == UpgradeItemType::RateUp && iid == 42,
               "upgrade parse bd");
        expect(!UpgradeItem::parse_c2s(payload.data(), 2, parsed_type, iid),
               "upgrade rejects short payload");
    }

    {
        const auto framed = MixingItem::frame_c2s(1, 10, 11, 12, 13, 14);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
               "mixing c2s frame");
        expect(type == static_cast<std::uint8_t>(C2S::Mixing), "mixing opcode");
        std::uint8_t mode = 0;
        std::uint32_t weapon = 0;
        std::uint32_t extra = 0;
        std::uint32_t armor = 0;
        std::uint32_t stone1 = 0;
        std::uint32_t stone2 = 0;
        expect(MixingItem::parse_c2s(
                   payload.data(), payload.size(), mode, weapon, extra, armor, stone1, stone2) &&
                   mode == 1 && weapon == 10 && extra == 11 && armor == 12 && stone1 == 13 &&
                   stone2 == 14,
               "mixing parse bddddd");
        std::vector<std::uint8_t> legacy{0, 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0};
        expect(MixingItem::parse_c2s(
                   legacy.data(), legacy.size(), mode, weapon, extra, armor, stone1, stone2) &&
                   mode == 0 && weapon == 1 && extra == 0 && armor == 2 && stone1 == 3 &&
                   stone2 == 4,
               "mixing legacy 17-byte payload");
    }

    {
        InsertCharmFields in;
        in.x_attack = 3;
        in.x_magic = 4;
        in.x_hit = 5;
        in.x_defense = 6;
        in.x_dodge = 2;
        in.upgrade_level = 7;
        in.upgrade_rate = 8;
        in.mix_type = 9;
        in.bof = 1;
        in.qigong_perforated = 1;
        in.qigong_effect = 80;
        in.dragon_effect = 237;
        in.random_stat_type = 15;
        in.random_stat_value = 5;
        std::uint8_t bytes[kInsertCharmBytes];
        pack_insert_charm(bytes, in);
        const InsertCharmFields out = unpack_insert_charm(bytes, kInsertCharmBytes);
        expect(out.x_attack == 3 && out.x_magic == 4 && out.x_hit == 5 && out.x_defense == 6 &&
                   out.x_dodge == 2 && out.upgrade_level == 7 && out.upgrade_rate == 8 &&
                   out.mix_type == 9 && out.bof == 1 && out.qigong_perforated == 1 &&
                   out.qigong_effect == 80 && out.dragon_effect == 237 &&
                   out.random_stat_type == 15 && out.random_stat_value == 5,
               "insert charm roundtrip");
        const InsertCharmFields missing = unpack_insert_charm(bytes, 3);
        expect(missing.x_attack == 0 && missing.upgrade_level == 0 && missing.mix_type == 0 &&
                   missing.bof == 0,
               "short charm payload is zero");
        const InsertCharmFields eight = unpack_insert_charm(bytes, 8);
        expect(eight.mix_type == 9 && eight.bof == 0, "8-byte charm payload leaves bof 0");
        const InsertCharmFields nine = unpack_insert_charm(bytes, 9);
        expect(nine.bof == 1 && nine.qigong_perforated == 0 && nine.qigong_effect == 0,
               "9-byte charm payload leaves Qigong fields 0");
        const InsertCharmFields eleven = unpack_insert_charm(bytes, 11);
        expect(eleven.qigong_effect == 80 && eleven.dragon_effect == 0,
               "11-byte charm payload leaves Dragon Fusing 0");
        const InsertCharmFields twelve = unpack_insert_charm(bytes, 12);
        expect(twelve.dragon_effect == 237 && twelve.random_stat_type == 0 &&
                   twelve.random_stat_value == 0,
               "12-byte charm payload leaves random stat fields 0");
        const InsertCharmFields thirteen = unpack_insert_charm(bytes, 13);
        expect(thirteen.random_stat_type == 15 && thirteen.random_stat_value == 0,
               "13-byte charm payload leaves random stat value 0");
    }

    {
        using openkal::protocol::bof_gstate_completeness;
        using openkal::protocol::bof_gstate_fx_level;
        using openkal::protocol::kCgsInvincible;
        using openkal::protocol::kCgsKnee;
        using openkal::protocol::kCgsOpenBox;
        using openkal::protocol::pack_bof_gstate;
        const std::uint32_t packed =
            pack_bof_gstate(kCgsOpenBox | kCgsKnee | kCgsInvincible, 2, 65);
        expect(bof_gstate_completeness(packed) == 2, "GState BOF completeness");
        expect(bof_gstate_fx_level(packed) == 65, "GState BOF fx level");
        expect((packed & kCgsOpenBox) != 0 && (packed & kCgsKnee) != 0 &&
                   (packed & kCgsInvincible) != 0,
               "BOF bits preserve knee, invincible, and open-box");
        const std::uint32_t cleared = pack_bof_gstate(packed, 0, 0);
        expect(bof_gstate_completeness(cleared) == 0 && bof_gstate_fx_level(cleared) == 0,
               "BOF bits can clear");
        expect((cleared & kCgsOpenBox) != 0, "clearing BOF keeps open-box");
    }

    {
        using openkal::protocol::MonsterRace;
        using openkal::protocol::MonsterRaceInfo;
        expect(MonsterRaceInfo::is_normal_combat(MonsterRace::Maguni), "maguni is normal combat");
        expect(MonsterRaceInfo::is_normal_combat(MonsterRace::NotMaguni),
               "not-maguni is normal combat");
        expect(MonsterRaceInfo::is_normal_combat(MonsterRace::Cocoon), "cocoon is normal combat");
        expect(!MonsterRaceInfo::is_normal_combat(MonsterRace::Item), "chest is not normal combat");
        expect(!MonsterRaceInfo::is_normal_combat(MonsterRace::FireFlower),
               "fire flower is not normal combat");
        expect(!MonsterRaceInfo::is_normal_combat(MonsterRace::Magic), "magic field is not combat");
        expect(!MonsterRaceInfo::is_normal_combat(MonsterRace::Egg), "egg is not normal combat");
        expect(!MonsterRaceInfo::is_normal_combat(MonsterRace::Inactive),
               "inactive is not normal combat");
        expect(MonsterRaceInfo::is_normal_combat(0), "race 0 int overload");
        expect(!MonsterRaceInfo::is_normal_combat(4), "race 4 int overload");
        expect(openkal::protocol::kItemMonsterOpenRange == 24, "race 4 open range is METER*2.4");
    }

    {
        struct KnightClassificationCase {
            int index;
            KnightTargetShape target;
            KnightEffectKind effect;
            KnightSpecialKind special;
        };
        constexpr std::array classification_cases{
            KnightClassificationCase{1,
                                     KnightTargetShape::Monster,
                                     KnightEffectKind::Execute,
                                     KnightSpecialKind::Beheading},
            KnightClassificationCase{
                8, KnightTargetShape::Monster, KnightEffectKind::Aggro, KnightSpecialKind::Taunt},
            KnightClassificationCase{
                14, KnightTargetShape::Player, KnightEffectKind::Buff, KnightSpecialKind::Protect},
            KnightClassificationCase{15,
                                     KnightTargetShape::HostileCharacter,
                                     KnightEffectKind::Control,
                                     KnightSpecialKind::ShieldStun},
            KnightClassificationCase{21,
                                     KnightTargetShape::HostileCharacter,
                                     KnightEffectKind::Control,
                                     KnightSpecialKind::Rush},
            KnightClassificationCase{
                23, KnightTargetShape::Party, KnightEffectKind::Buff, KnightSpecialKind::Chant},
            KnightClassificationCase{25,
                                     KnightTargetShape::TargetArea,
                                     KnightEffectKind::Damage,
                                     KnightSpecialKind::FrontalSweep},
            KnightClassificationCase{26,
                                     KnightTargetShape::Player,
                                     KnightEffectKind::Heal,
                                     KnightSpecialKind::Sacrifice},
            KnightClassificationCase{34,
                                     KnightTargetShape::Party,
                                     KnightEffectKind::Buff,
                                     KnightSpecialKind::HealthCostBuff},
            KnightClassificationCase{35,
                                     KnightTargetShape::DeadPlayer,
                                     KnightEffectKind::Resurrection,
                                     KnightSpecialKind::Revival},
            KnightClassificationCase{
                38, KnightTargetShape::SelfArea, KnightEffectKind::Damage, KnightSpecialKind::None},
            KnightClassificationCase{42,
                                     KnightTargetShape::HostileCharacter,
                                     KnightEffectKind::Damage,
                                     KnightSpecialKind::Taunt},
            KnightClassificationCase{
                45, KnightTargetShape::None, KnightEffectKind::Passive, KnightSpecialKind::None},
            KnightClassificationCase{
                62, KnightTargetShape::Self, KnightEffectKind::Utility, KnightSpecialKind::Return},
            KnightClassificationCase{
                80, KnightTargetShape::None, KnightEffectKind::Siege, KnightSpecialKind::Siege},
            KnightClassificationCase{88,
                                     KnightTargetShape::Self,
                                     KnightEffectKind::Utility,
                                     KnightSpecialKind::WalkOnAir},
            KnightClassificationCase{91,
                                     KnightTargetShape::SelfArea,
                                     KnightEffectKind::Control,
                                     KnightSpecialKind::None},
            KnightClassificationCase{
                93, KnightTargetShape::SelfArea, KnightEffectKind::Damage, KnightSpecialKind::None},
        };
        for (const auto& test : classification_cases) {
            expect(knight_target_shape(test.index) == test.target,
                   "Knight target-shape table entry");
            expect(knight_effect_kind(test.index) == test.effect, "Knight effect-kind table entry");
            expect(knight_special_kind(test.index) == test.special,
                   "Knight special-kind table entry");
        }

        constexpr std::array known_indices{
            0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
            18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
            36, 37, 38, 39, 40, 41, 42, 45, 62, 70, 71, 80, 81, 88, 91, 93,
        };
        for (const int index : known_indices) {
            expect(knight_is_known_skill(index), "Knight catalog index is known");
            expect(knight_effect_kind(index) != KnightEffectKind::None,
                   "Knight catalog index has an effect classification");
            expect(knight_skill_mp_cost(index, 1, 70, 999) != 999,
                   "Knight catalog index has converted MP metadata");
        }
        expect(!knight_is_known_skill(43) && !knight_is_known_skill(79) &&
                   !knight_is_known_skill(82),
               "Knight catalog rejects gaps");

        struct KnightMpCase {
            int index;
            int level;
            int character_level;
            int expected;
        };
        constexpr std::array mp_cases{
            KnightMpCase{3, 4, 60, 14},
            KnightMpCase{14, 3, 60, 100},
            KnightMpCase{18, 2, 60, 340},
            KnightMpCase{25, 5, 60, 94},
            KnightMpCase{34, 1, 60, 475},
            KnightMpCase{35, 2, 60, 400},
            KnightMpCase{38, 3, 70, 260},
            KnightMpCase{40, 2, 70, 360},
            KnightMpCase{41, 5, 70, 35},
            KnightMpCase{42, 1, 75, 220},
            KnightMpCase{42, 5, 75, 320},
            KnightMpCase{62, 1, 70, 30},
            KnightMpCase{88, 1, 91, 6},
            KnightMpCase{91, 1, 91, 220},
            KnightMpCase{93, 1, 91, 295},
        };
        for (const auto& test : mp_cases) {
            expect(knight_skill_mp_cost(test.index, test.level, test.character_level, 999) ==
                       test.expected,
                   "Knight MP formula table entry");
        }
        expect(knight_skill_mp_cost(99, 1, 70, 17) == 17,
               "unknown Knight skill preserves fallback MP");
        expect(knight_area_radius_tiles(25) == 1, "Half Swing enumerates one tile");
        expect(knight_area_radius_tiles(38) == 3, "Spin Slash enumerates three caster tiles");
        expect(knight_area_radius_tiles(42) == 13,
               "Provocation of Blow enumerates a 400px caster circle");
        expect(knight_area_radius_tiles(93) == 3, "Wave of Earth enumerates three caster tiles");
        expect(knight_target_cap(25) == 0, "Half Swing has no source target cap");
        expect(knight_life_extension_percent(0) == 0,
               "unlearned Knight Life Extension is inactive");
        expect(knight_life_extension_percent(3) == 44,
               "Knight Life Extension rank three percentage");
    }

    {
        using openkal::protocol::mage_area_radius_tiles;
        using openkal::protocol::mage_effect_kind;
        using openkal::protocol::mage_life_extension_percent;
        using openkal::protocol::mage_skill_mp_cost;
        using openkal::protocol::mage_target_cap;
        using openkal::protocol::mage_target_shape;
        using openkal::protocol::MageEffectKind;
        using openkal::protocol::MageTargetShape;
        expect(mage_target_shape(38) == MageTargetShape::Player,
               "refining weapon targets a player");
        expect(mage_effect_kind(38) == MageEffectKind::Buff, "refining weapon is a buff");
        expect(mage_target_shape(37) == MageTargetShape::TargetArea,
               "cold binding is target-centered");
        expect(mage_effect_kind(37) == MageEffectKind::Control,
               "cold binding is non-damage control");
        expect(mage_target_cap(37) == 5, "cold binding caps five targets");
        expect(mage_target_cap(36) == 8, "hypnotism caps eight targets");
        expect(mage_area_radius_tiles(41) == 2, "chain lightning radius is two tiles");
        expect(mage_area_radius_tiles(43) == 4, "thunder storm radius is four tiles");
        expect(mage_area_radius_tiles(45) == 4, "ice storm radius is four tiles");
        expect(mage_area_radius_tiles(48) == 4, "fire storm radius is four tiles");
        expect(mage_area_radius_tiles(47) == 2, "explosive burst keeps two-tile radius");
        expect(openkal::protocol::mage_uses_range_skill_selector(43),
               "thunder storm uses the range-skill selector (LMB ground confirm)");
        expect(openkal::protocol::mage_uses_range_skill_selector(47),
               "explosive burst uses the range-skill selector like storms");
        expect(mage_target_shape(47) == MageTargetShape::GroundPosition,
               "explosive burst uses ground coordinates");
        expect(mage_effect_kind(43) == MageEffectKind::PersistentArea,
               "thunder storm creates a persistent area");
        expect(openkal::protocol::mage_persistent_duration_ms(43) == 16000,
               "thunder storm lifetime");
        expect(openkal::protocol::mage_persistent_tick_ms(43) == 2000,
               "thunder storm tick interval");
        expect(openkal::protocol::mage_persistent_first_tick_ms(43) == 1000,
               "thunder storm first tick within one second");
        expect(openkal::protocol::mage_persistent_first_tick_ms(47) == 0,
               "explosive burst has no persistent first tick");
        expect(openkal::protocol::mage_persistent_duration_ms(47) == 0,
               "explosive burst is not persistent");
        expect(mage_target_shape(16) == MageTargetShape::DeadPlayer,
               "revival targets a dead player");
        expect(mage_effect_kind(80) == MageEffectKind::Siege, "artillery remains a siege skill");
        expect(mage_skill_mp_cost(38, 3, 60, 50) == 86, "refining weapon uses rank-scaled MP");
        expect(mage_skill_mp_cost(37, 4, 60, 60) == 84, "cold binding uses rank-scaled MP");
        expect(mage_skill_mp_cost(15, 4, 60, 0) == 24, "meditation cost uses character level");
        expect(mage_skill_mp_cost(63, 1, 70, 0) == 102,
               "defensive incantation six has converted MP");
        expect(mage_life_extension_percent(0) == 0, "unlearned life extension is inactive");
        expect(mage_life_extension_percent(3) == 44, "life extension rank three percentage");
    }

    {
        struct ArcherClassificationCase {
            int index;
            ArcherTargetShape target;
            ArcherEffectKind effect;
            ArcherSpecialKind special;
            ArcherSourceKind source;
        };
        constexpr std::array classification_cases{
            ArcherClassificationCase{2,
                                     ArcherTargetShape::None,
                                     ArcherEffectKind::Passive,
                                     ArcherSpecialKind::None,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{4,
                                     ArcherTargetShape::Monster,
                                     ArcherEffectKind::Damage,
                                     ArcherSpecialKind::AggroReduction,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{7,
                                     ArcherTargetShape::Self,
                                     ArcherEffectKind::Buff,
                                     ArcherSpecialKind::None,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{9,
                                     ArcherTargetShape::HostileCharacter,
                                     ArcherEffectKind::DamageControl,
                                     ArcherSpecialKind::HeartBreaker,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{14,
                                     ArcherTargetShape::HostileCharacter,
                                     ArcherEffectKind::Damage,
                                     ArcherSpecialKind::None,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{15,
                                     ArcherTargetShape::HostileCharacter,
                                     ArcherEffectKind::Debuff,
                                     ArcherSpecialKind::None,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{16,
                                     ArcherTargetShape::HostileCharacter,
                                     ArcherEffectKind::Damage,
                                     ArcherSpecialKind::FocusShot,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{18,
                                     ArcherTargetShape::HostileCharacter,
                                     ArcherEffectKind::Drain,
                                     ArcherSpecialKind::None,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{24,
                                     ArcherTargetShape::Self,
                                     ArcherEffectKind::Buff,
                                     ArcherSpecialKind::Resistance,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{31,
                                     ArcherTargetShape::Party,
                                     ArcherEffectKind::Buff,
                                     ArcherSpecialKind::EmergencyEscape,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{42,
                                     ArcherTargetShape::GroundPosition,
                                     ArcherEffectKind::PersistentArea,
                                     ArcherSpecialKind::PoisonCloud,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{46,
                                     ArcherTargetShape::Self,
                                     ArcherEffectKind::Buff,
                                     ArcherSpecialKind::None,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{47,
                                     ArcherTargetShape::GroundPosition,
                                     ArcherEffectKind::PersistentArea,
                                     ArcherSpecialKind::ArrowRain,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{48,
                                     ArcherTargetShape::None,
                                     ArcherEffectKind::Passive,
                                     ArcherSpecialKind::None,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{49,
                                     ArcherTargetShape::HostileCharacter,
                                     ArcherEffectKind::Damage,
                                     ArcherSpecialKind::ArrowExplosion,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{62,
                                     ArcherTargetShape::Self,
                                     ArcherEffectKind::Utility,
                                     ArcherSpecialKind::Return,
                                     ArcherSourceKind::Known},
            ArcherClassificationCase{70,
                                     ArcherTargetShape::None,
                                     ArcherEffectKind::Passive,
                                     ArcherSpecialKind::LaterSource,
                                     ArcherSourceKind::LaterSource},
            ArcherClassificationCase{80,
                                     ArcherTargetShape::None,
                                     ArcherEffectKind::Siege,
                                     ArcherSpecialKind::Siege,
                                     ArcherSourceKind::Siege},
        };
        for (const auto& test : classification_cases) {
            expect(archer_target_shape(test.index) == test.target,
                   "Archer target-shape table entry");
            expect(archer_effect_kind(test.index) == test.effect, "Archer effect-kind table entry");
            expect(archer_special_kind(test.index) == test.special,
                   "Archer special-kind table entry");
            expect(archer_source_kind(test.index) == test.source, "Archer source-kind table entry");
        }
        expect(archer_focus_shot_bar_count(999) == 0, "Focus Shot bar is empty before 1000 ms");
        expect(archer_focus_shot_bar_count(1000) == 1, "Focus Shot bar pip 1 at 1000 ms");
        expect(archer_focus_shot_bar_count(1700) == 2, "Focus Shot bar pip 2 at 1700 ms");
        expect(archer_focus_shot_bar_count(2300) == 3, "Focus Shot bar pip 3 at 2300 ms");
        expect(archer_focus_shot_bar_count(2800) == 4, "Focus Shot bar pip 4 at 2800 ms");
        expect(archer_focus_shot_bar_count(kArcherFocusShotBarMs) == 5,
               "Focus Shot bar pip 5 at 3300 ms");
        expect(kArcherFocusShotBarMs == 3300, "non-Korean Focus Shot bar is 3300 ms");

        constexpr std::array runtime_indices{
            0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
            19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
            38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 62, 70, 71, 80, 81,
        };
        for (const int index : runtime_indices) {
            expect(archer_is_known_skill(index), "Archer catalog index is known");
            expect(archer_effect_kind(index) != ArcherEffectKind::None,
                   "Archer catalog index has an effect classification");
            expect(archer_skill_mp_cost(index, 2, 70, 999) != 999,
                   "Archer catalog index has MP metadata");
        }
        expect(runtime_indices.size() == 55, "Archer catalog contains 55 runtime indices");
        expect(!archer_is_known_skill(50) && !archer_is_known_skill(61) &&
                   !archer_is_known_skill(82),
               "Archer catalog rejects gaps");
        expect(!archer_is_blocked_skill(45) && !archer_is_blocked_skill(46) &&
                   !archer_is_blocked_skill(47) && !archer_is_blocked_skill(48) &&
                   !archer_is_blocked_skill(49) && archer_is_blocked_skill(70) &&
                   archer_is_blocked_skill(80) && archer_is_blocked_skill(99),
               "Archer blockers distinguish early, later, siege, and unknown skills");

        constexpr std::array self_buffs{7, 10, 24, 25, 26, 28, 29};
        for (const int index : self_buffs) {
            expect(archer_target_shape(index) == ArcherTargetShape::Self &&
                       archer_effect_kind(index) == ArcherEffectKind::Buff,
                   "Archer self-buff table entry");
        }
        constexpr std::array hostile_shots{14, 15, 16};
        for (const int index : hostile_shots) {
            expect(archer_target_shape(index) == ArcherTargetShape::HostileCharacter,
                   "Archer hostile shot table entry");
        }

        struct ArcherMpCase {
            int index;
            int level;
            int character_level;
            int expected;
        };
        constexpr std::array mp_cases{
            ArcherMpCase{3, 4, 60, 28},
            ArcherMpCase{4, 3, 60, 6},
            ArcherMpCase{9, 5, 60, 55},
            ArcherMpCase{14, 3, 60, 12},
            ArcherMpCase{16, 2, 60, 35},
            ArcherMpCase{21, 3, 60, 64},
            ArcherMpCase{23, 2, 60, 79},
            ArcherMpCase{24, 2, 60, 64},
            ArcherMpCase{27, 2, 60, 120},
            ArcherMpCase{28, 2, 60, 50},
            ArcherMpCase{34, 2, 60, 72},
            ArcherMpCase{42, 2, 60, 132},
            ArcherMpCase{46, 1, 70, 50},
            ArcherMpCase{47, 3, 70, 300},
            ArcherMpCase{49, 3, 70, 300},
            ArcherMpCase{62, 1, 70, 30},
        };
        for (const auto& test : mp_cases) {
            expect(archer_skill_mp_cost(test.index, test.level, test.character_level, 999) ==
                       test.expected,
                   "Archer MP formula table entry");
        }
        expect(archer_skill_mp_cost(99, 1, 70, 17) == 17,
               "unknown Archer skill preserves fallback MP");

        expect(archer_item_requirement(13) == ArcherItemRequirement::BowAndPoisonPouch &&
                   archer_required_item_index(13) == kArcherPoisonPouchItemIndex &&
                   kArcherPoisonPouchItemIndex == 360,
               "Poison Arrow requires the converted Poison Pouch");
        expect(archer_item_requirement(6) == ArcherItemRequirement::Bow &&
                   archer_required_item_index(6) == 0,
               "Flamy Arrow requires an equipped bow");
        expect(archer_item_requirement(14) == ArcherItemRequirement::Bow &&
                   archer_required_item_index(14) == 0,
               "Blow-up Arrow requires a bow without a consumable");
        expect(archer_item_requirement(24) == ArcherItemRequirement::None,
               "Archer resistance buff has no item requirement");
        expect(archer_area_radius_tiles(42) == kArcherPoisonCloudRadiusTiles &&
                   archer_target_cap(42) == 0 && kArcherPoisonCloudBuffMs == 60000 &&
                   kArcherPoisonCloudFirstTickMs == 2000 && kArcherPoisonCloudTickMs == 1000 &&
                   kArcherPoisonCloudDurationMs == 12000,
               "Poison Cloud uses CMPoisonCloud radius 2 and a 60 s linger buff");
        expect(archer_area_radius_tiles(47) == kArcherArrowRainRadiusTiles &&
                   archer_item_requirement(47) == ArcherItemRequirement::Bow,
               "Arrow Rain uses hook radius 3 and requires a bow");
        expect(archer_area_radius_tiles(49) == kArcherArrowExplosionRadiusTiles &&
                   archer_item_requirement(49) == ArcherItemRequirement::Bow &&
                   archer_skill_mp_cost(49, 1, 70, 0) == 20 + 4 * 70,
               "Arrow Explosion uses hook radius 3, a bow, and level-scaled MP");
        expect(archer_uses_range_skill_selector(42) && archer_uses_range_skill_selector(47) &&
                   !archer_uses_range_skill_selector(16) && !archer_uses_range_skill_selector(46),
               "Poison Cloud and Arrow Rain use RangeSkillCast like Mage storms");
    }

    {
        std::uint16_t wear[7]{};
        const auto local = CreatePlayer::frame(
            42, "Hero", static_cast<std::uint8_t>(0 | 0x80), 10, 20, 30, 0, 0, wear, 0, 0);
        expect(!local.empty() && local[2] == static_cast<std::uint8_t>(S2C::CreatePlayer),
               "local createplayer op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(local.data(), local.size(), consumed, type, payload),
               "local createplayer frame");
        CreatePlayerFields local_fields;
        expect(CreatePlayer::parse(payload.data(), payload.size(), local_fields) &&
                   local_fields.id == 42 && (local_fields.class_byte & 0x80) != 0 &&
                   local_fields.name == "Hero",
               "parse local createplayer");

        const auto other =
            CreatePlayer::frame(99, "DummyKnight", 0, 267618, 242946, 19630, 0, 0, wear, 0, 0);
        expect(try_pop_frame(other.data(), other.size(), consumed, type, payload),
               "other createplayer frame");
        CreatePlayerFields other_fields;
        expect(CreatePlayer::parse(payload.data(), payload.size(), other_fields) &&
                   other_fields.id == 99 && (other_fields.class_byte & 0x80) == 0 &&
                   other_fields.name == "DummyKnight" && local_fields.id == 42,
               "other createplayer does not change local id");

        const auto rm = CreatePlayer::frame_remove(99);
        expect(!rm.empty() && rm[2] == static_cast<std::uint8_t>(S2C::RemovePlayer),
               "s2c removeplayer op");
        expect(try_pop_frame(rm.data(), rm.size(), consumed, type, payload),
               "s2c removeplayer frame");
        std::uint32_t removed = 0;
        expect(CreatePlayer::parse_remove(payload.data(), payload.size(), removed) && removed == 99,
               "parse removeplayer");
    }

    {
        const auto fx = Effect::frame(42, static_cast<std::uint8_t>(EffectType::Hp));
        expect(!fx.empty() && fx[2] == static_cast<std::uint8_t>(S2C::Effect), "s2c effect hp op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(fx.data(), fx.size(), consumed, type, payload), "s2c effect hp frame");
        std::uint32_t entity_id = 0;
        std::uint8_t effect = 0;
        expect(Effect::parse(payload.data(), payload.size(), entity_id, effect) &&
                   entity_id == 42 && effect == static_cast<std::uint8_t>(EffectType::Hp),
               "parse effect hp");
        expect(refresh_effect_fx_name(static_cast<std::uint8_t>(EffectType::Hp)) ==
                   "effect_skill01",
               "hp fx name");
        expect(refresh_effect_fx_name(static_cast<std::uint8_t>(EffectType::Mp)) ==
                   "effect_skill04",
               "mp fx name");
        expect(oneshot_effect_fx_name(static_cast<std::uint8_t>(EffectType::Charm1)) == "UR_01-1",
               "charm fx still maps");

        const auto damaged = Effect::frame_damaged(99, 1234);
        expect(try_pop_frame(damaged.data(), damaged.size(), consumed, type, payload),
               "s2c effect damaged frame");
        std::uint16_t damage = 0;
        expect(Effect::parse_damaged(payload.data(), payload.size(), entity_id, damage) &&
                   entity_id == 99 && damage == 1234,
               "parse effect damaged");
    }

    {
        const auto pkt = MState::frame(7, kCmsHp | kCmsMp);
        expect(!pkt.empty() && pkt[2] == static_cast<std::uint8_t>(S2C::MState), "s2c mstate op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(pkt.data(), pkt.size(), consumed, type, payload), "s2c mstate frame");
        std::uint32_t entity_id = 0;
        std::uint64_t mstate = 0;
        expect(MState::parse(payload.data(), payload.size(), entity_id, mstate) && entity_id == 7 &&
                   mstate == (kCmsHp | kCmsMp),
               "parse mstate");
        expect(mstate_to_rest_food_icon(kCmsHp) == kRestFoodIcon, "cms hp icon");
        expect(mstate_to_rest_food_icon(kCmsMp) == kRestFoodIcon, "cms mp icon");
        expect(mstate_to_rest_food_sys(kCmsHp) == kSysRestHp, "cms hp sys");
        expect(mstate_to_rest_food_sys(kCmsMp) == kSysRestMp, "cms mp sys");
        expect(buff_type_is_rest_food(18) && buff_type_is_rest_food(19) &&
                   !buff_type_is_rest_food(51),
               "rest food type helper");
        expect(static_cast<int>(BuffType::AttackFood) == 5 &&
                   static_cast<int>(BuffType::MagicAttackFood) == 6,
               "attack food specialty ids");
        expect(buff_type_is_attack_food(5) && buff_type_is_attack_food(6) &&
                   !buff_type_is_attack_food(51),
               "attack food type helper");
        expect(buff_type_to_bstate(BuffType::AttackFood) == kCbsFoodAttack &&
                   buff_type_to_bstate(BuffType::MagicAttackFood) == kCbsFoodMagic,
               "attack food bstate bits");
        expect(kCmsMoveSpeed == 0x00000002ull, "CMS_MOVESPEED bit");
        expect(kCmsMoveStop == 0x00000200ull, "CMS_MOVESTOP bit");
    }

    {
        MapStateFields expected;
        expected.map = 3;
        expected.phase = 2;
        expected.active_mask = 0xFF;
        expected.timer = MapStateTimer::Ritual;
        expected.timer_seconds = 123;
        const auto pkt = MapState::frame(expected);
        expect(!pkt.empty() && pkt[2] == static_cast<std::uint8_t>(S2C::MapState),
               "s2c mapstate op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(pkt.data(), pkt.size(), consumed, type, payload),
               "s2c mapstate frame");
        MapStateFields actual;
        expect(MapState::parse(payload.data(), payload.size(), actual) &&
                   actual.map == expected.map && actual.phase == expected.phase &&
                   actual.active_mask == expected.active_mask && actual.timer == expected.timer &&
                   actual.timer_seconds == expected.timer_seconds,
               "parse mapstate");
    }

    {
        const auto add = BuffState::frame_add(kCbsSkillStone, 900, 63, 2411);
        expect(!add.empty() && add[2] == static_cast<std::uint8_t>(S2C::MessageV),
               "addbstate with skill index");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(add.data(), add.size(), consumed, type, payload) && payload.size() > 1,
               "addbstate frame");
        std::uint64_t bstate = 0;
        std::int32_t remain = 0;
        std::int32_t skill_index = 0;
        std::int32_t item_index = 0;
        expect(
            BuffState::parse_add(
                payload.data() + 1, payload.size() - 1, bstate, remain, skill_index, item_index) &&
                bstate == kCbsSkillStone && remain == 900 && skill_index == 63 &&
                item_index == 2411,
            "parse addbstate source ids");
        const auto add0 = BuffState::frame_add(kCbsSkillStone, 900);
        expect(try_pop_frame(add0.data(), add0.size(), consumed, type, payload),
               "addbstate default index frame");
        skill_index = -1;
        item_index = -1;
        expect(
            BuffState::parse_add(
                payload.data() + 1, payload.size() - 1, bstate, remain, skill_index, item_index) &&
                skill_index == 0 && item_index == 0,
            "default source ids are zero");
    }

    {
        ShopItemList expected;
        expected.npc_id = 1234;
        expected.offers = {{47, 20}, {48, 70}};
        const auto packet = Shop::frame_item_list_s2c(expected);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(packet.data(), packet.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(S2C::ItemList),
               "shop item list frame");
        ShopItemList actual;
        expect(Shop::parse_item_list_s2c(payload.data(), payload.size(), actual) &&
                   actual.npc_id == expected.npc_id && actual.offers.size() == 2 &&
                   actual.offers[1].index == 48 && actual.offers[1].buy == 70,
               "shop item list round trip");

        expect(actual.currency_index == 0, "legacy shops use gold");
        expected.currency_index = 23030;
        const auto tickets = Shop::frame_item_list_s2c(expected);
        expect(try_pop_frame(tickets.data(), tickets.size(), consumed, type, payload) &&
                   Shop::parse_item_list_s2c(payload.data(), payload.size(), actual) &&
                   actual.currency_index == 23030 && actual.offers[1].buy == 70,
               "item-currency offer extension round trip");
        payload.push_back(0);
        expect(!Shop::parse_item_list_s2c(payload.data(), payload.size(), actual),
               "reject malformed currency extension");

        const auto buy = Shop::frame_buy_c2s(1234, 48, 5);
        expect(try_pop_frame(buy.data(), buy.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(C2S::BuyItem),
               "shop buy frame");
        std::uint32_t npc_id = 0;
        std::uint16_t index = 0;
        std::uint16_t quantity = 0;
        expect(Shop::parse_buy_c2s(payload.data(), payload.size(), npc_id, index, quantity) &&
                   npc_id == 1234 && index == 48 && quantity == 5,
               "shop buy round trip");

        const auto sell = Shop::frame_sell_c2s(1234, 9876, 3);
        expect(try_pop_frame(sell.data(), sell.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(C2S::SellItem),
               "shop sell frame");
        std::uint32_t iid = 0;
        expect(Shop::parse_sell_c2s(payload.data(), payload.size(), npc_id, iid, quantity) &&
                   npc_id == 1234 && iid == 9876 && quantity == 3,
               "shop sell round trip");
    }

    {
        StorageInfo expected;
        expected.npc_id = 321;
        StorageItem item;
        item.iid = 987;
        item.index = 47;
        item.prefix = 2;
        item.slot = 215;
        item.count = 9;
        item.info = pack_insert_info(true, 12, 30, false, true);
        item.charm.x_attack = 4;
        item.charm.random_stat_type = static_cast<std::uint8_t>(RandomItemStatType::ArmorStrength);
        item.charm.random_stat_value = 3;
        item.transform_level = 5;
        item.transform_exp = 12345;
        expected.items.push_back(item);

        const auto packet = Storage::frame_info_s2c(expected);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(packet.data(), packet.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(S2C::StorageInfo),
               "storage info frame");
        StorageInfo actual;
        expect(Storage::parse_info_s2c(payload.data(), payload.size(), actual) &&
                   actual.npc_id == 321 && actual.items.size() == 1 && actual.items[0].iid == 987 &&
                   actual.items[0].slot == 215 && insert_account_bound(actual.items[0].info) &&
                   actual.items[0].charm.x_attack == 4 && actual.items[0].transform_exp == 12345,
               "storage info round trip");

        const auto transfer = Storage::frame_transfer_c2s(true, 321, 987, 4);
        expect(try_pop_frame(transfer.data(), transfer.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(C2S::PutInStorage),
               "storage deposit frame");
        std::uint32_t npc_id = 0;
        std::uint32_t iid = 0;
        std::uint32_t count = 0;
        expect(Storage::parse_transfer_c2s(payload.data(), payload.size(), npc_id, iid, count) &&
                   npc_id == 321 && iid == 987 && count == 4,
               "storage transfer round trip");
    }

    {
        const auto request = Mail::frame_claim_c2s(47);
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(request.data(), request.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(C2S::Mail),
               "mail claim frame");
        MailCommand command = MailCommand::List;
        std::uint32_t id = 0;
        expect(Mail::parse_c2s(payload.data(), payload.size(), command, id) &&
                   command == MailCommand::Claim && id == 47,
               "mail claim request round trip");

        MailEntry expected;
        expected.id = 47;
        expected.item_index = 237;
        expected.item_count = 3;
        expected.created_at = 1710000000;
        expected.read = true;
        expected.subject = "Level Up Reward";
        expected.body = "Congratulations";
        const auto inbox = Mail::frame_list_s2c({expected});
        expect(try_pop_frame(inbox.data(), inbox.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(S2C::Mail),
               "mail inbox frame");
        std::vector<MailEntry> actual;
        expect(Mail::parse_list_s2c(payload.data(), payload.size(), actual) && actual.size() == 1 &&
                   actual.front().id == 47 && actual.front().item_index == 237 &&
                   actual.front().item_count == 3 && !actual.front().claimed &&
                   actual.front().subject == "Level Up Reward" && actual.front().read,
               "mail inbox round trip");

        auto text_only = expected;
        text_only.item_index = 0;
        text_only.item_count = 0;
        const auto message = Mail::frame_list_s2c({text_only});
        expect(try_pop_frame(message.data(), message.size(), consumed, type, payload) &&
                   Mail::parse_list_s2c(payload.data(), payload.size(), actual) &&
                   actual.size() == 1 && actual.front().item_index == 0 &&
                   actual.front().item_count == 0,
               "mail without an attachment round trips through the game protocol");
        text_only.item_count = 1;
        expect(Mail::frame_list_s2c({text_only}).empty(),
               "mail rejects an attachment quantity without an item");
        for (auto action : {MailCommand::Read, MailCommand::Delete}) {
            const auto packet = Mail::frame_action_c2s(action, 47);
            expect(try_pop_frame(packet.data(), packet.size(), consumed, type, payload) &&
                       Mail::parse_c2s(payload.data(), payload.size(), command, id) &&
                       command == action && id == 47,
                   "mail read/delete request round trip");
            payload.push_back(0);
            expect(!Mail::parse_c2s(payload.data(), payload.size(), command, id),
                   "mail action rejects trailing bytes");
        }
        expect(Mail::frame_action_c2s(MailCommand::Read, 0).empty() &&
                   Mail::frame_action_c2s(MailCommand::Claim, 47).empty(),
               "mail action builder rejects invalid operation and zero ID");
        expected.read = false;
        expected.claimed = true;
        const auto claimed_packet = Mail::frame_list_s2c({expected});
        expect(
            try_pop_frame(claimed_packet.data(), claimed_packet.size(), consumed, type, payload) &&
                Mail::parse_list_s2c(payload.data(), payload.size(), actual) &&
                actual.front().claimed && !actual.front().read,
            "claimed and read flags remain independent");
        const MailClaimResult expected_result{47, true};
        const auto result = Mail::frame_claim_result_s2c(expected_result);
        expect(try_pop_frame(result.data(), result.size(), consumed, type, payload) &&
                   type == static_cast<std::uint8_t>(S2C::Mail),
               "mail claim result frame");
        MailClaimResult actual_result;
        expect(Mail::parse_claim_result_s2c(payload.data(), payload.size(), actual_result) &&
                   actual_result.id == 47 && actual_result.claimed,
               "mail claim result round trip");
    }

    {
        const auto pkt = Property::frame_update_mproperty(42, kPropertyMSpeed, -40);
        expect(!pkt.empty() && pkt[2] == static_cast<std::uint8_t>(S2C::UpdateMProperty),
               "s2c updatemproperty op");
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(try_pop_frame(pkt.data(), pkt.size(), consumed, type, payload),
               "s2c updatemproperty frame");
        std::uint32_t entity_id = 0;
        std::uint8_t prop = 0;
        std::int16_t value = 0;
        expect(Property::parse_update_mproperty(
                   payload.data(), payload.size(), entity_id, prop, value) &&
                   entity_id == 42 && prop == kPropertyMSpeed && value == -40,
               "parse updatemproperty mspeed");
        expect(move_speed_factor(30) == 1.3f && move_speed_factor(0) == 1.0f,
               "transform +30 mspeed is 1.3x");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
