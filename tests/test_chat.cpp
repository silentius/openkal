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
#include "openkal/client/ChatCommandRegistry.h"
#include "openkal/client/ChatFilterModel.h"
#include "openkal/client/ChatWidget.h"
#include "openkal/client/ItemDb.h"
#include "openkal/map/Portal.h"
#include "openkal/protocol/ChatAmount.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/GetItemId.h"
#include "openkal/protocol/LevelUp.h"
#include "openkal/protocol/Transform.h"
#include "openkal/server/chat/ChatService.h"
#include "test_util.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

int main() {
    using openkal::protocol::BattleMsgKind;
    using openkal::protocol::ChatChannel;
    using openkal::server::ChatAction;
    using openkal::server::ChatService;
    using openkal::test::expect;

    {
        openkal::client::ChatFilterModel filters;
        expect(filters.channel_enabled(openkal::client::ChatTab::Normal, ChatChannel::Common),
               "normal common on");
        expect(filters.channel_enabled(openkal::client::ChatTab::Normal, ChatChannel::Whisper),
               "normal whisper on");
        expect(filters.channel_enabled(openkal::client::ChatTab::Party, ChatChannel::Party),
               "party tab party on");
        expect(!filters.channel_enabled(openkal::client::ChatTab::Party, ChatChannel::Common),
               "party tab common off");
        expect(filters.battle_enabled(BattleMsgKind::Damage), "damage on by default");
        filters.set_battle_enabled(BattleMsgKind::Exp, false);
        expect(!filters.accepts(ChatChannel::Common, BattleMsgKind::Exp), "exp filtered");
        expect(filters.accepts(ChatChannel::Whisper, std::nullopt), "whisper accepted on normal");
    }

    {
        using openkal::client::ChatFilterModel;
        using openkal::client::ChatTab;
        ChatFilterModel filters;
        for (std::size_t t = 0; t < ChatFilterModel::kTabCount; ++t) {
            const auto tab = static_cast<ChatTab>(t);
            filters.set_active_tab(tab);
            for (std::size_t c = 0; c < ChatFilterModel::kChannelCount; ++c) {
                const auto channel = static_cast<ChatChannel>(c);
                const bool expected = channel == ChatFilterModel::tab_channel(tab) ||
                                      (tab == ChatTab::Normal && channel == ChatChannel::Whisper);
                expect(filters.accepts(channel, std::nullopt) == expected,
                       "focused tab defaults exclude unrelated channels");
            }
        }
        filters.set_channel_enabled(ChatTab::Party, ChatChannel::Whisper, true);
        expect(!filters.channel_enabled(ChatTab::Guild, ChatChannel::Whisper),
               "channel options are independent between tabs");
        filters.set_battle_enabled(BattleMsgKind::Damage, false);
        for (std::size_t t = 0; t < ChatFilterModel::kTabCount; ++t) {
            filters.set_active_tab(static_cast<ChatTab>(t));
            expect(!filters.accepts(ChatChannel::Common, BattleMsgKind::Damage),
                   "battle options are global across tabs");
        }
        float r = 0, g = 0, b = 0;
        ChatFilterModel::channel_color(ChatChannel::Guild, r, g, b);
        expect(g > .8f && b > .8f && r < .3f, "guild chat is turquoise");
        ChatFilterModel::channel_color(ChatChannel::Alliance, r, g, b);
        expect(b > r && r > g && r < .6f && g < .5f, "alliance is dark blue-purple");
        ChatFilterModel::channel_color(ChatChannel::Shout, r, g, b);
        expect(r > .9f && b > .6f && g < .6f, "shout chat is pink");
        ChatFilterModel::channel_color(ChatChannel::Whisper, r, g, b);
        expect(b > .9f && b > r && b > g, "personal messages are blue");
    }

    {
        expect(openkal::client::BattleMsgBuilder::damage_dealt(42) == "You dealt 42 damage.",
               "skill/attack damage chat");
        expect(openkal::client::BattleMsgBuilder::critical_damage_dealt(42) ==
                   "You gave critical damage of 42 to the enemy.",
               "sys 186 critical damage dealt chat");
        expect(openkal::client::BattleMsgBuilder::explosive_blow(123) ==
                   "You gave 123 of Explosive Blow to the enemy.",
               "sys 871 explosive blow chat");
        expect(openkal::client::BattleMsgBuilder::damage_taken(7) == "You received 7 damage.",
               "damage taken chat");
        expect(openkal::client::BattleMsgBuilder::critical_damage_taken(7) ==
                   "You got critical damage of 7 from the enemy.",
               "sys 185 critical damage taken chat");
        expect(openkal::client::BattleMsgBuilder::evaded_attack("Evaded the enemy's attack") ==
                   "Evaded the enemy's attack",
               "sys 140 evade chat");
        expect(openkal::client::BattleMsgBuilder::item_acquired("Herb", 1) == "You acquired Herb.",
               "item get chat");
        expect(openkal::client::BattleMsgBuilder::item_removed("Demon's blood", 4) ==
                   "Removed 4 Demon's blood.",
               "item removal chat");
        expect(openkal::client::BattleMsgBuilder::evaded_attack("#140") ==
                   "Evaded the enemy's attack",
               "missing sys 140 fallback");
        expect(openkal::client::BattleMsgBuilder::skill_learned("Lightning Slash") ==
                   "You learned Lightning Slash.",
               "skill learned chat");
        expect(openkal::client::BattleMsgBuilder::skill_level_up("Lightning Slash", 3) ==
                   "Lightning Slash increased to level 3.",
               "skill rank-up chat");
        expect(openkal::client::BattleMsgBuilder::system_message(
                   static_cast<std::uint8_t>(openkal::protocol::SystemMessageId::AccountBound)) ==
                   "Action not possible, item bound.",
               "account-bound system message");
    }

    {
        float r = 0, g = 0, b = 0;
        openkal::client::ChatFilterModel::battle_color(BattleMsgKind::Skill, r, g, b);
        expect(r > g && b > g && b > 0.8f, "skill battle color is purple");
    }

    {
        ChatService svc;
        std::uint64_t next = 0;
        auto first = svc.evaluate("hello", "Hero", 1000, next);
        expect(first.action() == ChatAction::LocalBroadcast, "local broadcast");
        expect(first.outbound_text() == "hello", "outbound text");
        expect(next == 1000 + ChatService::kRateLimitMs, "rate limit armed");

        auto limited = svc.evaluate("again", "Hero", 1500, next);
        expect(limited.action() == ChatAction::RateLimited, "rate limited");

        next = 0;
        auto whisper = svc.evaluate("@Bob hi", "Hero", 2000, next);
        expect(whisper.action() == ChatAction::Whisper, "whisper action");
        expect(whisper.whisper_target() == "Bob", "whisper target");

        next = 0;
        auto party = svc.evaluate("#hello", "Hero", 3000, next);
        expect(party.action() == ChatAction::PartyBroadcast && party.outbound_text() == "#hello",
               "party routing preserves its channel marker");
        expect(next == 3000 + ChatService::kPartyRateLimitMs, "party interval remains 500 ms");
        for (const auto text : {"^hello", "/s hello", "/y hello", "/S\thello"}) {
            next = 0;
            const auto shout = svc.evaluate(text, "Hero", 4000, next);
            expect(shout.action() == ChatAction::ShoutBroadcast &&
                       shout.outbound_text() == "^hello",
                   "shout aliases broadcast with a filterable pink channel marker");
        }
        next = 0;
        expect(svc.evaluate("^", "Hero", 4000, next).action() == ChatAction::Ignore,
               "empty shout is ignored");
        next = 0;
        expect(svc.evaluate("~hello", "Hero", 4000, next).action() == ChatAction::StubChannel,
               "alliance remains unavailable until authenticated alliances exist");
        expect(openkal::protocol::Chatting::channel_of("&hello") == ChatChannel::Common &&
                   openkal::protocol::Chatting::channel_of("*hello") == ChatChannel::Common,
               "removed systems have no special channel routing");

        next = 0;
        auto time_cmd = svc.evaluate("/time", "Hero", 4000, next);
        expect(time_cmd.action() == ChatAction::CommandReply, "time command");
        expect(!time_cmd.reply().empty(), "time reply");
        expect(next == 0, "slash commands skip chat rate limit");
    }

    {
        ChatService svc;
        bool flying = false;
        svc.commands().register_command("fly", [&](const openkal::server::ChatCommandContext&) {
            flying = !flying;
            return openkal::server::ChatCommandResult(flying ? "Fly mode on." : "Fly mode off.");
        });
        std::uint64_t next = 0;
        auto on = svc.evaluate("/fly", "Hero", 1000, next);
        expect(on.action() == ChatAction::CommandReply, "fly on action");
        expect(on.reply() == "Fly mode on.", "fly on reply");
        expect(flying, "fly latched on");
        auto off = svc.evaluate("/fly", "Hero", 1001, next);
        expect(off.reply() == "Fly mode off.", "fly off reply");
        expect(!flying, "fly latched off");
        expect(next == 0, "fly does not arm chat rate limit");
    }

    {
        ChatService svc;
        bool god = false;
        svc.commands().register_command("godmode", [&](const openkal::server::ChatCommandContext&) {
            god = !god;
            return openkal::server::ChatCommandResult(god ? "God mode on." : "God mode off.");
        });
        std::uint64_t next = 0;
        auto on = svc.evaluate("/godmode", "Hero", 1000, next);
        expect(on.action() == ChatAction::CommandReply, "godmode on action");
        expect(on.reply() == "God mode on.", "godmode on reply");
        expect(god, "godmode latched on");
        auto off = svc.evaluate("/godmode", "Hero", 1001, next);
        expect(off.reply() == "God mode off.", "godmode off reply");
        expect(!god, "godmode latched off");
    }

    {
        ChatService svc;
        bool reset = false;
        svc.commands().register_command("reset", [&](const openkal::server::ChatCommandContext&) {
            reset = true;
            return openkal::server::ChatCommandResult("Reset.");
        });
        std::uint64_t next = 0;
        auto reply = svc.evaluate("/reset", "Hero", 1000, next);
        expect(reply.action() == ChatAction::CommandReply, "reset action");
        expect(reply.reply() == "Reset.", "reset reply");
        expect(reset, "reset command ran");
        expect(next == 0, "reset skips chat rate limit");
    }

    {
        ChatService svc;
        bool spawned = false;
        svc.commands().register_command(
            "debug", [&](const openkal::server::ChatCommandContext& ctx) {
                std::string args(ctx.args());
                for (char& c : args) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (args == "on") {
                    spawned = true;
                    return openkal::server::ChatCommandResult("Debug dummies spawned.");
                }
                if (args == "off") {
                    spawned = false;
                    return openkal::server::ChatCommandResult("Debug dummies removed.");
                }
                return openkal::server::ChatCommandResult("Usage: /debug on|off");
            });
        std::uint64_t next = 0;
        auto usage = svc.evaluate("/debug", "Hero", 1000, next);
        expect(usage.reply() == "Usage: /debug on|off", "debug usage");
        expect(!spawned, "usage does not spawn");
        auto on = svc.evaluate("/debug on", "Hero", 1001, next);
        expect(on.action() == ChatAction::CommandReply, "debug on action");
        expect(on.reply() == "Debug dummies spawned.", "debug on reply");
        expect(spawned, "debug on latched");
        auto off = svc.evaluate("/debug off", "Hero", 1002, next);
        expect(off.reply() == "Debug dummies removed.", "debug off reply");
        expect(!spawned, "debug off latched");
        expect(next == 0, "debug skips chat rate limit");
    }

    {
        ChatService svc;
        svc.commands().register_command(
            "teleport", [&](const openkal::server::ChatCommandContext& ctx) {
                int dungeon = 0;
                int area = 0;
                if (openkal::map::PortalCatalog::parse_dungeon_arg(ctx.args(), dungeon)) {
                    return openkal::server::ChatCommandResult("Teleporting to D" +
                                                              std::to_string(dungeon) + " portal.");
                }
                std::string place;
                if (openkal::map::PortalCatalog::parse_place_arg(ctx.args(), place)) {
                    return openkal::server::ChatCommandResult(
                        openkal::map::PortalCatalog::place_teleport_reply(place));
                }
                if (openkal::map::PortalCatalog::parse_area_arg(ctx.args(), area)) {
                    return openkal::server::ChatCommandResult("Teleporting to monster area " +
                                                              std::to_string(area) + ".");
                }
                return openkal::server::ChatCommandResult(
                    std::string(openkal::map::kTeleportUsage));
            });
        std::uint64_t next = 0;
        auto usage = svc.evaluate("/teleport", "Hero", 1000, next);
        expect(usage.reply() == openkal::map::kTeleportUsage, "teleport usage");
        auto bad = svc.evaluate("/teleport narootuh", "Hero", 1001, next);
        expect(bad.reply() == openkal::map::kTeleportUsage, "teleport rejects non-dungeon");
        auto ok = svc.evaluate("/teleport d2", "Hero", 1002, next);
        expect(ok.reply() == "Teleporting to D2 portal.", "teleport d2 reply");
        auto d5_ok = svc.evaluate("/teleport d5", "Hero", 1004, next);
        expect(d5_ok.reply() == "Teleporting to D5 portal.", "teleport d5 reply");
        auto d6_ok = svc.evaluate("/teleport d6", "Hero", 1006, next);
        expect(d6_ok.reply() == "Teleporting to D6 portal.", "teleport d6 reply");
        auto emok_ok = svc.evaluate("/teleport emok", "Hero", 1005, next);
        expect(emok_ok.reply() == "Teleporting to Emok Island.", "teleport emok reply");
        auto vod_ok = svc.evaluate("/teleport vod", "Hero", 1007, next);
        expect(vod_ok.reply() == "Teleporting to Valley of Devah.", "teleport vod reply");
        auto lord_ok = svc.evaluate("/teleport lord", "Hero", 1008, next);
        expect(lord_ok.reply() == "Teleporting to Doggebi Lord stage.", "teleport lord reply");
        auto area_ok = svc.evaluate("/teleport area 108", "Hero", 1003, next);
        expect(area_ok.reply() == "Teleporting to monster area 108.", "teleport area 108 reply");
    }

    {
        openkal::client::ChatCommandRegistry local;
        bool flying = false;
        local.register_command("fly", [&](const openkal::client::ClientChatCommandContext&) {
            flying = !flying;
            return openkal::client::ClientChatCommandResult(
                true, flying ? "Fly mode on." : std::string{});
        });
        auto on = local.try_dispatch("/fly");
        expect(on.handled() && flying, "client fly handled");
        auto unknown = local.try_dispatch("/time");
        expect(!unknown.handled(), "unknown slash still forwards");
    }

    {
        openkal::client::ChatCommandRegistry local;
        local.register_command(
            "teleport", [&](const openkal::client::ClientChatCommandContext& ctx) {
                int dungeon = 0;
                int area = 0;
                std::string place;
                if (openkal::map::PortalCatalog::parse_dungeon_arg(ctx.args(), dungeon) ||
                    openkal::map::PortalCatalog::parse_place_arg(ctx.args(), place) ||
                    openkal::map::PortalCatalog::parse_area_arg(ctx.args(), area)) {
                    (void) dungeon;
                    (void) place;
                    (void) area;
                    return openkal::client::ClientChatCommandResult(false, {});
                }
                return openkal::client::ClientChatCommandResult(true, openkal::map::kTeleportUsage);
            });
        auto usage = local.try_dispatch("/teleport");
        expect(usage.handled() && usage.info_line() == openkal::map::kTeleportUsage,
               "client teleport usage stays local");
        auto ok = local.try_dispatch("/teleport d3");
        expect(!ok.handled(), "valid /teleport forwards to server");
        auto emok_ok = local.try_dispatch("/teleport emok");
        expect(!emok_ok.handled(), "valid /teleport emok forwards to server");
        auto vod_ok = local.try_dispatch("/teleport vod");
        expect(!vod_ok.handled(), "valid /teleport vod forwards to server");
        auto lord_ok = local.try_dispatch("/teleport lord");
        expect(!lord_ok.handled(), "valid /teleport lord forwards to server");
        auto area_ok = local.try_dispatch("/teleport area 108");
        expect(!area_ok.handled(), "valid /teleport area forwards to server");
    }

    {
        openkal::client::ChatCommandRegistry local;
        bool debug = false;
        local.register_command("debug", [&](const openkal::client::ClientChatCommandContext&) {
            debug = !debug;
            return openkal::client::ClientChatCommandResult(
                true, debug ? "Debug overlay on." : "Debug overlay off.");
        });
        auto on = local.try_dispatch("/debug");
        expect(on.handled() && debug, "client debug on");
        expect(on.info_line() == "Debug overlay on.", "client debug on text");
        auto off = local.try_dispatch("/debug");
        expect(off.handled() && !debug, "client debug off");
        expect(off.info_line() == "Debug overlay off.", "client debug off text");
    }

    {
        ChatService svc;
        int got_index = 0;
        int got_prefix = 0;
        int got_count = 0;
        svc.commands().register_command(
            "getitemid", [&](const openkal::server::ChatCommandContext& ctx) {
                openkal::protocol::GetItemIdArgs parsed;
                if (!openkal::protocol::GetItemId::parse_args(ctx.args(), parsed)) {
                    return openkal::server::ChatCommandResult(
                        std::string(openkal::protocol::kGetItemIdUsage));
                }
                got_index = parsed.index;
                got_prefix = parsed.prefix;
                got_count = parsed.count;
                return openkal::server::ChatCommandResult("ok");
            });
        std::uint64_t next = 0;
        auto usage = svc.evaluate("/getitemid", "Hero", 1000, next);
        expect(usage.reply() == openkal::protocol::kGetItemIdUsage, "getitemid usage");
        auto sword = svc.evaluate("/getitemid 101", "Hero", 1001, next);
        expect(sword.reply() == "ok" && got_index == 101 && got_prefix == 0 && got_count == 1,
               "getitemid 101");
        auto meds = svc.evaluate("/getitemid 48 100", "Hero", 1002, next);
        expect(meds.reply() == "ok" && got_index == 48 && got_prefix == 0 && got_count == 100,
               "getitemid 48 100");
        auto king = svc.evaluate("/getitemid 101 92 1", "Hero", 1003, next);
        expect(king.reply() == "ok" && got_index == 101 && got_prefix == 92 && got_count == 1,
               "getitemid 101 92 1");
    }

    {
        openkal::client::ChatCommandRegistry local;
        local.register_command(
            "getitemid", [&](const openkal::client::ClientChatCommandContext& ctx) {
                openkal::protocol::GetItemIdArgs parsed;
                if (!openkal::protocol::GetItemId::parse_args(ctx.args(), parsed)) {
                    return openkal::client::ClientChatCommandResult(
                        true, openkal::protocol::kGetItemIdUsage);
                }
                (void) parsed;
                return openkal::client::ClientChatCommandResult(false, {});
            });
        auto usage = local.try_dispatch("/getitemid");
        expect(usage.handled() && usage.info_line() == openkal::protocol::kGetItemIdUsage,
               "client getitemid usage stays local");
        auto ok = local.try_dispatch("/getitemid 101 92 1");
        expect(!ok.handled(), "valid /getitemid forwards to server");
    }

    {
        ChatService svc;
        int granted = 0;
        svc.commands().register_command(
            "level", [&](const openkal::server::ChatCommandContext& ctx) {
                openkal::protocol::LevelUpArgs parsed;
                if (!openkal::protocol::LevelUp::parse_args(ctx.args(), parsed)) {
                    return openkal::server::ChatCommandResult(
                        std::string(openkal::protocol::kLevelUsage));
                }
                granted = parsed.value;
                return openkal::server::ChatCommandResult("Leveled up to " +
                                                          std::to_string(1 + granted) + ".");
            });
        std::uint64_t next = 0;
        auto one = svc.evaluate("/level", "Hero", 1000, next);
        expect(one.reply() == "Leveled up to 2." && granted == 1, "level default +=1");
        auto five = svc.evaluate("/level +=5", "Hero", 1001, next);
        expect(five.reply() == "Leveled up to 6." && granted == 5, "level +=5");
        auto usage = svc.evaluate("/level foo", "Hero", 1002, next);
        expect(usage.reply() == openkal::protocol::kLevelUsage, "level usage");
        expect(next == 0, "level does not arm chat rate limit");
    }

    {
        openkal::client::ChatCommandRegistry local;
        local.register_command("level", [&](const openkal::client::ClientChatCommandContext& ctx) {
            openkal::protocol::LevelUpArgs parsed;
            if (!openkal::protocol::LevelUp::parse_args(ctx.args(), parsed)) {
                return openkal::client::ClientChatCommandResult(true,
                                                                openkal::protocol::kLevelUsage);
            }
            (void) parsed;
            return openkal::client::ClientChatCommandResult(false, {});
        });
        auto usage = local.try_dispatch("/level foo");
        expect(usage.handled() && usage.info_line() == openkal::protocol::kLevelUsage,
               "client level usage stays local");
        auto one = local.try_dispatch("/level");
        expect(!one.handled(), "valid /level forwards to server");
        auto five = local.try_dispatch("/level +=5");
        expect(!five.handled(), "valid /level +=5 forwards to server");
        auto set = local.try_dispatch("/level =6");
        expect(!set.handled(), "valid /level =6 forwards to server");
        auto down = local.try_dispatch("/level -=5");
        expect(!down.handled(), "valid /level -=5 forwards to server");
    }

    {
        ChatService svc;
        std::int64_t exp = 10;
        svc.commands().register_command("exp", [&](const openkal::server::ChatCommandContext& ctx) {
            openkal::protocol::ChatAmountArgs parsed;
            if (!openkal::protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                return openkal::server::ChatCommandResult(
                    std::string(openkal::protocol::kExpUsage));
            }
            exp = openkal::protocol::ChatAmount::apply(exp, parsed);
            if (exp < 0) {
                exp = 0;
            }
            return openkal::server::ChatCommandResult("EXP is now " + std::to_string(exp) + ".");
        });
        std::uint64_t next = 0;
        auto usage = svc.evaluate("/exp", "Hero", 1000, next);
        expect(usage.reply() == openkal::protocol::kExpUsage, "exp usage");
        auto add = svc.evaluate("/exp +=20", "Hero", 1001, next);
        expect(add.reply() == "EXP is now 30." && exp == 30, "exp +=20");
        auto set = svc.evaluate("/exp =7", "Hero", 1002, next);
        expect(set.reply() == "EXP is now 7." && exp == 7, "exp =7");
        auto sub = svc.evaluate("/exp -=100", "Hero", 1003, next);
        expect(sub.reply() == "EXP is now 0." && exp == 0, "exp -= clamps");
        expect(next == 0, "exp does not arm chat rate limit");
    }

    {
        ChatService svc;
        int egg_index = 337;
        int egg_level = 1;
        int egg_exp = 0;
        svc.commands().register_command(
            "eggexp", [&](const openkal::server::ChatCommandContext& ctx) {
                openkal::protocol::ChatAmountArgs parsed;
                if (!openkal::protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                    return openkal::server::ChatCommandResult(
                        std::string(openkal::protocol::kEggExpUsage));
                }
                if (egg_index <= 0) {
                    return openkal::server::ChatCommandResult("No transform egg equipped.");
                }
                const std::int64_t pool = openkal::protocol::ChatAmount::apply(egg_exp, parsed);
                egg_exp = pool < 0 ? 0 : static_cast<int>(pool);
                (void) openkal::protocol::consume_transform_exp_into_levels(egg_level, egg_exp);
                return openkal::server::ChatCommandResult("Egg EXP is now " +
                                                          std::to_string(egg_exp) + " (level " +
                                                          std::to_string(egg_level) + ").");
            });
        std::uint64_t next = 0;
        auto usage = svc.evaluate("/eggexp", "Hero", 1000, next);
        expect(usage.reply() == openkal::protocol::kEggExpUsage, "eggexp usage");
        auto add = svc.evaluate("/eggexp +=100", "Hero", 1001, next);
        expect(add.reply() == "Egg EXP is now 100 (level 1)." && egg_exp == 100, "eggexp +=100");
        const int need = openkal::protocol::need_transform_exp_for_level(1);
        auto dump = svc.evaluate("/eggexp +=" + std::to_string(need), "Hero", 1002, next);
        expect(egg_level == 2 && egg_exp == 100, "eggexp dump levels egg");
        expect(dump.reply().find("level 2") != std::string::npos, "eggexp dump reply");
        egg_index = 0;
        auto none = svc.evaluate("/eggexp +=1", "Hero", 1003, next);
        expect(none.reply() == "No transform egg equipped.", "eggexp requires egg");
    }

    {
        ChatService svc;
        int rage = 0;
        svc.commands().register_command(
            "rage", [&](const openkal::server::ChatCommandContext& ctx) {
                openkal::protocol::ChatAmountArgs parsed;
                if (!openkal::protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                    return openkal::server::ChatCommandResult(
                        std::string(openkal::protocol::kRageUsage));
                }
                const std::int64_t pool = openkal::protocol::ChatAmount::apply(rage, parsed);
                rage = static_cast<int>(
                    std::clamp(pool,
                               static_cast<std::int64_t>(0),
                               static_cast<std::int64_t>(openkal::protocol::max_rage())));
                return openkal::server::ChatCommandResult("Rage is now " + std::to_string(rage) +
                                                          ".");
            });
        std::uint64_t next = 0;
        auto usage = svc.evaluate("/rage", "Hero", 1000, next);
        expect(usage.reply() == openkal::protocol::kRageUsage, "rage usage");
        auto add = svc.evaluate("/rage +=500", "Hero", 1001, next);
        expect(add.reply() == "Rage is now 500." && rage == 500, "rage +=500");
        auto set = svc.evaluate("/rage =0", "Hero", 1002, next);
        expect(set.reply() == "Rage is now 0." && rage == 0, "rage =0");
        auto cap = svc.evaluate(
            "/rage =" + std::to_string(openkal::protocol::max_rage() + 50), "Hero", 1003, next);
        expect(rage == openkal::protocol::max_rage(), "rage clamps to cap");
        expect(cap.reply() == "Rage is now " + std::to_string(openkal::protocol::max_rage()) + ".",
               "rage cap reply");
    }

    {
        openkal::client::ChatCommandRegistry local;
        local.register_command("exp", [&](const openkal::client::ClientChatCommandContext& ctx) {
            openkal::protocol::ChatAmountArgs parsed;
            if (!openkal::protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                return openkal::client::ClientChatCommandResult(true, openkal::protocol::kExpUsage);
            }
            (void) parsed;
            return openkal::client::ClientChatCommandResult(true, {});
        });
        auto usage = local.try_dispatch("/exp foo");
        expect(usage.handled() && usage.info_line() == openkal::protocol::kExpUsage,
               "client exp usage stays local");
        auto ok = local.try_dispatch("/exp +=5");
        expect(ok.handled() && ok.info_line().empty(), "valid /exp is sent to server");
        auto set = local.try_dispatch("/exp =0");
        expect(set.handled() && set.info_line().empty(), "valid /exp =0 is sent to server");
    }

    {
        openkal::client::ChatCommandRegistry local;
        local.register_command("eggexp", [&](const openkal::client::ClientChatCommandContext& ctx) {
            openkal::protocol::ChatAmountArgs parsed;
            if (!openkal::protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                return openkal::client::ClientChatCommandResult(true,
                                                                openkal::protocol::kEggExpUsage);
            }
            (void) parsed;
            return openkal::client::ClientChatCommandResult(true, {});
        });
        auto usage = local.try_dispatch("/eggexp");
        expect(usage.handled() && usage.info_line() == openkal::protocol::kEggExpUsage,
               "client eggexp usage stays local");
        auto ok = local.try_dispatch("/eggexp +=10");
        expect(ok.handled() && ok.info_line().empty(), "valid /eggexp is sent to server");
    }

    {
        openkal::client::ChatCommandRegistry local;
        local.register_command("rage", [&](const openkal::client::ClientChatCommandContext& ctx) {
            openkal::protocol::ChatAmountArgs parsed;
            if (!openkal::protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                return openkal::client::ClientChatCommandResult(true,
                                                                openkal::protocol::kRageUsage);
            }
            (void) parsed;
            return openkal::client::ClientChatCommandResult(true, {});
        });
        auto usage = local.try_dispatch("/rage foo");
        expect(usage.handled() && usage.info_line() == openkal::protocol::kRageUsage,
               "client rage usage stays local");
        auto ok = local.try_dispatch("/rage +=1");
        expect(ok.handled() && ok.info_line().empty(), "valid /rage is sent to server");
    }

    {
        openkal::client::ItemDef potion;
        potion.set_refresh_hp(50);
        expect(potion.is_consumable(), "hp potion is consumable");
        openkal::client::ItemDef mana;
        mana.set_refresh_mp(30);
        expect(mana.is_consumable(), "mp potion is consumable");
        openkal::client::ItemDef scroll;
        scroll.set_teleport_loc(1);
        expect(scroll.is_consumable(), "teleport scroll is consumable");
        openkal::client::ItemDef charm;
        charm.set_usable(true);
        expect(!charm.is_consumable(), "changeprefix charm is not a potion");
    }

    {
        constexpr float kPanelX = 20.0f;
        constexpr float kPanelY = 400.0f;
        constexpr float kPanelW = 280.0f;
        constexpr float kPanelH = 140.0f;
        constexpr float kScale = 1.0f;
        const auto options =
            openkal::client::ChatWidget::options_rect(kPanelX, kPanelY, kPanelW, kPanelH, kScale);
        expect(std::abs((options.y + options.h) - (kPanelY + kPanelH)) < 0.01f,
               "options bottom flush with chat");
        expect(options.x > kPanelX + kPanelW, "options sit to the right of chat");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
