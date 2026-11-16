/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MonsterCatalog.h"
#include "openkal/platform/Path.h"
#include "test_util.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#ifndef OPENKAL_TEST_RESOURCE_ROOT
#error OPENKAL_TEST_RESOURCE_ROOT required
#endif

int main() {
    using openkal::client::MonsterFxAttach;
    using openkal::test::expect;

    {
        std::vector<MonsterFxAttach> out;
        MonsterFxAttach::append_looping_effect_fx("(*LOOP 0) (*EFFECT_FX effect_ef079)", out);
        expect(out.size() == 1, "thunder idle EFFECT_FX");
        expect(out[0].name() == "effect_ef079", "thunder FX name");
    }

    {
        std::vector<MonsterFxAttach> out;
        MonsterFxAttach::append_looping_effect_fx("(*LOOP 0) (*EFFECT_FX effect_ef087)", out);
        expect(out.size() == 1 && out[0].name() == "effect_ef087", "ice storm EFFECT_FX");
    }

    {
        std::vector<MonsterFxAttach> out;
        MonsterFxAttach::append_looping_effect_fx("(*EFFECT_FX effect_ef092a)", out);
        MonsterFxAttach::append_looping_effect_fx("(*EFFECT_FX effect_ef092a)", out);
        expect(out.size() == 1, "duplicate EFFECT_FX names are ignored");
    }

    {
        std::vector<MonsterFxAttach> out;
        MonsterFxAttach::append_looping_effect_fx(
            "(*EFFECT_FX effect_ef092a) (*EFFECT_FX effect_ef092c)", out);
        expect(out.size() == 2, "multiple EFFECT_FX on one key");
        expect(out[0].name() == "effect_ef092a" && out[1].name() == "effect_ef092c",
               "fire storm FX names");
    }

    {
        std::vector<MonsterFxAttach> out;
        MonsterFxAttach::append_looping_effect_fx("(*EFFECT_FX_NR effect_oneshot)", out);
        expect(out.empty(), "EFFECT_FX_NR is oneshot and skipped");
    }

    {
        std::vector<MonsterFxAttach> out;
        MonsterFxAttach::append_looping_effect_fx("(*PROJECTILE 2 1)", out);
        expect(out.empty(), "non-FX options are ignored");
    }

    {
        using openkal::client::MonsterIdleFxCue;
        std::vector<MonsterIdleFxCue> cues;
        MonsterIdleFxCue::append_from_option("(*EFFECT_FX effect_ef092a)", 0.0f, cues);
        MonsterIdleFxCue::append_from_option("(*EFFECT_FX effect_ef092c)", 400.0f, cues);
        expect(cues.size() == 2, "timed idle EFFECT_FX cues");
        expect(cues[0].name() == "effect_ef092a" && cues[0].time_ms() == 0.0f, "first fire cue");
        expect(cues[1].name() == "effect_ef092c" && cues[1].time_ms() == 400.0f, "later fire cue");
    }

    {
        using openkal::client::MonsterIdleFxCue;
        std::vector<MonsterIdleFxCue> cues;
        MonsterIdleFxCue::append_from_option("(*EFFECT_FX_NR effect_oneshot)", 0.0f, cues);
        expect(cues.empty(), "idle EFFECT_FX_NR is skipped");
    }

    {
        using openkal::client::MonsterIdleFxCue;
        const std::vector<std::string> options = {"(*EFFECT_FX_NR effect_spark)"};
        const std::vector<std::uint16_t> times = {250};
        std::vector<MonsterIdleFxCue> cues;
        MonsterIdleFxCue::append_skill_clip(options, times, cues);
        expect(cues.size() == 1 && cues[0].name() == "effect_spark" && cues[0].time_ms() == 250.0f,
               "Monster ModelSkill keeps non-repeating EFFECT_FX cues");
    }

    {
        using openkal::client::MonsterIdleFxCue;
        std::vector<MonsterFxAttach> looping;
        std::vector<MonsterIdleFxCue> cues;
        MonsterIdleFxCue::append_idle_gb_key(
            "(*LOOP 0) (*EFFECT_FX effect_ef079)", 0.0f, looping, cues);
        MonsterIdleFxCue::append_idle_gb_key(
            "(*LOOP 0) (*EFFECT_FX effect_ef087)", 0.0f, looping, cues);
        MonsterIdleFxCue::append_idle_gb_key("(*EFFECT_FX effect_ef092a)", 0.0f, looping, cues);
        MonsterIdleFxCue::append_idle_gb_key("(*EFFECT_FX effect_ef092c)", 400.0f, looping, cues);
        expect(looping.size() == 2, "LOOP idle keys become field attaches");
        expect(looping[0].name() == "effect_ef079" && looping[1].name() == "effect_ef087",
               "thunder/ice looping names");
        expect(cues.size() == 2, "non-LOOP idle keys stay timed cues");
        expect(cues[0].name() == "effect_ef092a" && cues[1].name() == "effect_ef092c",
               "fire storm timed names");
    }

    {
        using openkal::client::MonsterIdleFxCue;
        std::vector<MonsterFxAttach> looping;
        std::vector<MonsterIdleFxCue> cues;
        const std::vector<std::string> options = {"(*LOOP 0)", "(*EFFECT_FX effect_ef068a)"};
        const std::vector<std::uint16_t> times = {0, 0};
        MonsterIdleFxCue::append_idle_clip(options, times, looping, cues);
        expect(looping.size() == 1 && looping[0].name() == "effect_ef068a",
               "split LOOP and EFFECT_FX keys become a looping poison-cloud attach");
        expect(cues.empty(), "split LOOP idle clips do not emit timed oneshots");
    }

    {
        using openkal::client::MonsterIdleFxCue;
        std::vector<MonsterFxAttach> looping;
        std::vector<MonsterIdleFxCue> cues;
        const std::vector<std::string> options = {"(*EFFECT_FX effect_ef092a)",
                                                  "(*EFFECT_FX effect_ef092c)"};
        const std::vector<std::uint16_t> times = {0, 400};
        MonsterIdleFxCue::append_idle_clip(options, times, looping, cues);
        expect(looping.empty(), "Fire Storm idle stays timed without LOOP");
        expect(cues.size() == 2 && cues[0].name() == "effect_ef092a" &&
                   cues[1].name() == "effect_ef092c",
               "Fire Storm split EFFECT_FX keys stay timed cues");
    }

    {
        using openkal::client::MonsterCatalog;
        MonsterCatalog storm_catalog;
        const std::string storm_dir =
            openkal::platform::join_path(OPENKAL_TEST_RESOURCE_ROOT, "config/monsters");
        expect(storm_catalog.load_json_dir(storm_dir), "load production monster catalog");
        const auto* thunder = storm_catalog.find_rig(228);
        const auto* ice = storm_catalog.find_rig(229);
        const auto* fire = storm_catalog.find_rig(230);
        expect(thunder != nullptr && thunder->bone_no() == 900,
               "Thunder Storm controller has its field bone");
        expect(ice != nullptr && ice->bone_no() == 901, "Ice Storm controller has its field bone");
        expect(fire != nullptr && fire->bone_no() == 902,
               "Fire Storm controller has its field bone");
    }

    {
        using openkal::client::MonsterCatalog;
        const char* tmp = std::getenv("TMPDIR");
        if (tmp == nullptr || tmp[0] == '\0') {
            tmp = "/tmp";
        }
        const std::string dir =
            std::string(tmp) + "/openkal_monster_clothes_" + std::to_string(::getpid());
        ::mkdir(dir.c_str(), 0755);
        auto write_json = [&](const char* name, const std::string& body) {
            std::ofstream out(openkal::platform::join_path(dir, name));
            out << body;
        };
        write_json("0538_banshee.json",
                   R"({
  "index": 538,
  "appearance": { "boneId": 107, "scale": 1.3, "level": 81, "motion": "AAAAAAAAAA" },
  "stats": { "level": 81 }
})");
        write_json("0539_banshee.json",
                   R"({
  "index": 539,
  "clothesIndex": 538,
  "stats": { "level": 82 }
})");
        MonsterCatalog json_cat;
        write_json("0698.json",
                   R"({
  "index": 698,
  "appearance": { "boneId": 169, "scale": 1.4, "scaleParam": 8, "level": 83, "motion": "AAAAAAAAAA" },
  "stats": { "level": 83 }
})");
        write_json("0699.json",
                   R"({
  "index": 699,
  "clothesIndex": 698,
  "stats": { "level": 84 }
})");
        write_json("0399_visual_attack.json",
                   R"({
  "index": 399,
  "appearance": {
    "boneId": 71,
    "scale": 0.4,
    "level": 69,
    "motion": "AAAAAAAAAA",
    "basicAttackVisual": { "action": 7, "motion": 2 }
  },
  "stats": { "level": 85 }
})");
        expect(json_cat.load_json_dir(dir), "load shared-clothes JSON");
        const auto* base = json_cat.find_rig(538);
        const auto* variant = json_cat.find_rig(539);
        expect(base != nullptr && base->bone_no() == 107, "base banshee appearance");
        expect(variant != nullptr && variant->bone_no() == 107 && variant->clothes_index() == 538,
               "variant reuses banshee clothes 538");
        expect(variant != nullptr && variant->level() == 82, "variant keeps its own level");
        const auto* soldier = json_cat.find_rig(698);
        const auto* soldier_lv = json_cat.find_rig(699);
        expect(soldier != nullptr && soldier->bone_no() == 169, "crazy demon soldier appearance");
        expect(soldier_lv != nullptr && soldier_lv->bone_no() == 169 &&
                   soldier_lv->clothes_index() == 698,
               "soldier variant reuses clothes 698");
        const auto* poison_spitter = json_cat.find_rig(399);
        expect(poison_spitter != nullptr && poison_spitter->basic_attack_visual_motion() == 2,
               "basic attack visual uses the configured Action 7 motion");
    }

    {
        using openkal::client::MonsterCatalog;
        const char* tmp = std::getenv("TMPDIR");
        if (tmp == nullptr || tmp[0] == '\0') {
            tmp = "/tmp";
        }
        const std::string root =
            std::string(tmp) + "/openkal_monster_motion_" + std::to_string(::getpid());
        const std::string monster = openkal::platform::join_path(root, "Monster");
        const std::string motion = openkal::platform::join_path(monster, "Motion");
        ::mkdir(root.c_str(), 0755);
        ::mkdir(monster.c_str(), 0755);
        ::mkdir(motion.c_str(), 0755);
        {
            std::ofstream out(openkal::platform::join_path(motion, "t111_8_a_01.GB"));
            out << "fixture";
        }
        {
            std::ofstream out(openkal::platform::join_path(motion, "T111_7_A_01.gb"));
            out << "fixture";
        }
        {
            std::ofstream out(openkal::platform::join_path(motion, "T111_7_A_02.gb"));
            out << "fixture";
        }
        const std::string attack = MonsterCatalog::motion_gb(root, 111, 8, 'A');
        expect(attack == openkal::platform::join_path(motion, "t111_8_a_01.GB"),
               "resolve skeleton archer attack motion regardless of filename case");
        const std::string skill_prep = MonsterCatalog::motion_gb(root, 111, 7, 'A', 1);
        const std::string skill_fire = MonsterCatalog::motion_gb(root, 111, 7, 'A', 2);
        expect(skill_prep == openkal::platform::join_path(motion, "T111_7_A_01.gb"),
               "resolve Monster ModelSkill prepare motion");
        expect(skill_fire == openkal::platform::join_path(motion, "T111_7_A_02.gb"),
               "resolve Monster ModelSkill fire motion");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
