/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/AnimatedActor.h"
#include "test_util.h"

#include <string>
#include <vector>

int main() {
    using openkal::client::AnimatedActor;
    using Cue = AnimatedActor::ClipEffectCue;
    using openkal::test::expect;

    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*EFFECT_MODEL skill_w_3_1)", out);
        expect(out.size() == 1 && out[0].kind == Cue::Kind::Model && out[0].name == "skill_w_3_1",
               "lightning slash left wave EFFECT_MODEL");
    }
    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*EFFECT_MODEL skill_w_3_2)", out);
        expect(out.size() == 1 && out[0].kind == Cue::Kind::Model && out[0].name == "skill_w_3_2",
               "lightning slash right wave EFFECT_MODEL");
    }
    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*EFFECT_MODEL effect_attack01)", out);
        expect(out.size() == 1 && out[0].kind == Cue::Kind::Model &&
                   out[0].name == "effect_attack01",
               "upward / widening wound EFFECT_MODEL");
    }
    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*EFFECT_FX effect_strike01)", out);
        expect(out.size() == 1 && out[0].kind == Cue::Kind::Fx && out[0].name == "effect_strike01",
               "powerful upward slash EFFECT_FX");
    }
    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*EFFECT_FX_NR aura01)", out);
        expect(out.size() == 1 && out[0].kind == Cue::Kind::Fx && out[0].name == "aura01",
               "EFFECT_FX_NR is not parsed as a bare EFFECT_FX name");
    }
    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*SOUND efc003) (*EFFECT_MODEL skill_w_3_1)", out);
        expect(out.size() == 1 && out[0].name == "skill_w_3_1",
               "EFFECT_MODEL is found beside SOUND");
    }
    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*LOOP 12)(*EFFECT_FX davi_ef132)(*SOUND SE010003)",
                                              out);
        expect(out.size() == 1 && out[0].kind == Cue::Kind::Fx && out[0].name == "davi_ef132",
               "Arrow Rain LOOP key EFFECT_FX davi_ef132");
    }
    {
        std::vector<Cue> out;
        AnimatedActor::parse_clip_effect_cues("(*EFFECT_FX davi_ef132_01)", out);
        expect(out.size() == 1 && out[0].name == "davi_ef132_01",
               "Arrow Rain fire-rain EFFECT_FX davi_ef132_01");
    }
    {
        openkal::terrain::GbAnimClip clip;
        clip.loop_start_ms = 12.0f;
        clip.key_times = {12, 390, 400};
        clip.key_options = {
            "(*LOOP 12)(*EFFECT_FX davi_ef132)(*SOUND SE010003)",
            "(*EFFECT_FX davi_ef132_01)",
            "",
        };
        std::vector<Cue> first;
        AnimatedActor::collect_looping_clip_effect_cues(clip, -1.0f, 50.0f, first);
        expect(first.size() == 1 && first[0].name == "davi_ef132",
               "first Arrow Rain cycle fires the LOOP FX");
        std::vector<Cue> wrapped;
        AnimatedActor::collect_looping_clip_effect_cues(clip, 380.0f, 40.0f, wrapped);
        expect(wrapped.size() == 2 && wrapped[0].name == "davi_ef132_01" &&
                   wrapped[1].name == "davi_ef132",
               "PlayAniRepeat wrap re-fires rain then the LOOP bow FX");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
