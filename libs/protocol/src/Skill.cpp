/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Skill.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
namespace openkal::protocol {
    namespace {
        std::vector<std::uint8_t> one(C2S op, std::uint8_t value) {
            PacketWriter w;
            w.write_u8(value);
            return w.frame(static_cast<std::uint8_t>(op));
        }
    }  // namespace
    std::vector<std::uint8_t> Skill::frame_client_self(std::uint8_t s) {
        return one(C2S::Skill, s);
    }
    std::vector<std::uint8_t> Skill::frame_client_boost(std::uint8_t e) {
        PacketWriter w;
        w.write_u8(0);
        w.write_u8(e);
        return w.frame(static_cast<std::uint8_t>(C2S::Skill));
    }
    std::vector<std::uint8_t>
    Skill::frame_client_target(std::uint8_t s, std::uint8_t k, std::uint32_t t) {
        PacketWriter w;
        w.write_u8(s);
        w.write_u8(k);
        w.write_u32(t);
        return w.frame(static_cast<std::uint8_t>(C2S::Skill));
    }
    std::vector<std::uint8_t>
    Skill::frame_client_ground(std::uint8_t s, std::int32_t x, std::int32_t y) {
        PacketWriter w;
        w.write_u8(s);
        w.write_i32(x);
        w.write_i32(y);
        return w.frame(static_cast<std::uint8_t>(C2S::Skill));
    }
    bool Skill::parse_client_header(const std::uint8_t* d,
                                    std::size_t n,
                                    std::uint8_t& s,
                                    const std::uint8_t*& r,
                                    std::size_t& rn) {
        if (!d || n < 1) {
            return false;
        }
        s = d[0];
        r = d + 1;
        rn = n - 1;
        return true;
    }
    bool Skill::parse_client_target(const std::uint8_t* d,
                                    std::size_t n,
                                    std::uint8_t& k,
                                    std::uint32_t& t) {
        if (!d || n < 5) {
            return false;
        }
        PacketReader r(d, n);
        return r.read_u8(k) && r.read_u32(t);
    }
    bool Skill::parse_client_ground(const std::uint8_t* d,
                                    std::size_t n,
                                    std::int32_t& x,
                                    std::int32_t& y) {
        if (!d || n != 8) {
            return false;
        }
        PacketReader r(d, n);
        return r.read_i32(x) && r.read_i32(y);
    }
    std::vector<std::uint8_t>
    Skill::frame_server(std::uint8_t s, std::uint32_t c, std::uint32_t t, std::uint8_t l) {
        PacketWriter w;
        w.write_u8(s);
        w.write_u32(c);
        w.write_u32(t);
        w.write_u8(0);
        w.write_u8(l);
        return w.frame(static_cast<std::uint8_t>(S2C::Skill));
    }
    std::vector<std::uint8_t> Skill::frame_server_damage(std::uint8_t s,
                                                         std::uint32_t c,
                                                         std::uint32_t t,
                                                         std::uint8_t l,
                                                         std::int32_t d,
                                                         std::int32_t p,
                                                         std::uint8_t f) {
        PacketWriter w;
        w.write_u8(s);
        w.write_u32(c);
        w.write_u32(t);
        w.write_u8(1);
        w.write_u8(l);
        w.write_i32(d);
        w.write_i32(p);
        w.write_u8(f);
        return w.frame(static_cast<std::uint8_t>(S2C::Skill));
    }
    bool Skill::parse_server(const std::uint8_t* d, std::size_t n, S2CSkillFields& o) {
        if (!d || n < 11) {
            return false;
        }
        PacketReader r(d, n);
        std::uint8_t h = 0;
        if (!r.read_u8(o.skill) || !r.read_u32(o.caster) || !r.read_u32(o.target) ||
            !r.read_u8(h) || !r.read_u8(o.level)) {
            return false;
        }
        o.has_damage = h != 0;
        if (!o.has_damage) {
            return true;
        }
        return r.read_i32(o.damage) && r.read_i32(o.palsy) && r.read_u8(o.flags);
    }
    std::vector<std::uint8_t> Skill::frame_preskill(std::uint8_t s, std::uint32_t t) {
        PacketWriter w;
        w.write_u8(s);
        w.write_u32(t);
        return w.frame(static_cast<std::uint8_t>(C2S::PreSkill));
    }
    bool
    Skill::parse_preskill(const std::uint8_t* d, std::size_t n, std::uint8_t& s, std::uint32_t& t) {
        if (!d || n < 5) {
            return false;
        }
        PacketReader r(d, n);
        return r.read_u8(s) && r.read_u32(t);
    }
    std::vector<std::uint8_t> Skill::frame_rest(std::uint8_t r) {
        return one(C2S::Rest, r);
    }
    bool Skill::parse_rest(const std::uint8_t* d, std::size_t n, std::uint8_t& r) {
        if (!d || n < 1) {
            return false;
        }
        r = d[0];
        return true;
    }
    std::vector<std::uint8_t> Skill::frame_action_rest(std::uint32_t id, std::uint8_t rest) {
        PacketWriter w;
        w.write_u32(id);
        w.write_u8(kAtRest);
        w.write_u8(rest);
        return w.frame(static_cast<std::uint8_t>(S2C::Action));
    }
    std::vector<std::uint8_t> Skill::frame_action_knee(std::uint32_t id) {
        PacketWriter w;
        w.write_u32(id);
        w.write_u8(kAtKnee);
        return w.frame(static_cast<std::uint8_t>(S2C::Action));
    }
    std::vector<std::uint8_t> Skill::frame_action_die(std::uint32_t id) {
        PacketWriter w;
        w.write_u32(id);
        w.write_u8(kAtDie);
        return w.frame(static_cast<std::uint8_t>(S2C::Action));
    }
    std::vector<std::uint8_t> Skill::frame_action_beheaded(std::uint32_t id) {
        PacketWriter w;
        w.write_u32(id);
        w.write_u8(kAtBeheaded);
        return w.frame(static_cast<std::uint8_t>(S2C::Action));
    }
    std::vector<std::uint8_t>
    Skill::frame_action_skill(std::uint32_t id, std::uint8_t s, std::uint32_t t) {
        PacketWriter w;
        w.write_u32(id);
        w.write_u8(kAtSkill);
        w.write_u8(s);
        w.write_u32(t);
        return w.frame(static_cast<std::uint8_t>(S2C::Action));
    }
    bool Skill::parse_action_skill(const std::uint8_t* d,
                                   std::size_t n,
                                   std::uint32_t& id,
                                   std::uint8_t& s,
                                   std::uint32_t& t) {
        if (!d || n < 10) {
            return false;
        }
        PacketReader r(d, n);
        std::uint8_t action = 0;
        if (!r.read_u32(id) || !r.read_u8(action) || action != kAtSkill || !r.read_u8(s) ||
            !r.read_u32(t)) {
            return false;
        }
        return true;
    }
    std::vector<std::uint8_t> Skill::frame_client_revival() {
        PacketWriter w;
        return w.frame(static_cast<std::uint8_t>(C2S::Revival));
    }
    std::vector<std::uint8_t> Skill::frame_client_save_revival_point(std::uint8_t point,
                                                                     std::uint32_t npc_id) {
        PacketWriter w;
        w.write_u8(point);
        w.write_u32(npc_id);
        return w.frame(static_cast<std::uint8_t>(C2S::SaveRevivalPt));
    }
    bool Skill::parse_action(const std::uint8_t* d,
                             std::size_t n,
                             std::uint32_t& id,
                             std::uint8_t& a,
                             std::uint8_t& arg) {
        if (!d || n < 5) {
            return false;
        }
        PacketReader r(d, n);
        if (!r.read_u32(id) || !r.read_u8(a)) {
            return false;
        }
        arg = 0;
        if (r.remaining() > 0) {
            return r.read_u8(arg);
        }
        return true;
    }
    std::vector<std::uint8_t> frame_c2s_skill_self(std::uint8_t a) {
        return Skill::frame_client_self(a);
    }
    std::vector<std::uint8_t> frame_c2s_skill_boost(std::uint8_t a) {
        return Skill::frame_client_boost(a);
    }
    std::vector<std::uint8_t>
    frame_c2s_skill_target(std::uint8_t a, std::uint8_t b, std::uint32_t c) {
        return Skill::frame_client_target(a, b, c);
    }
    bool parse_c2s_skill_header(const std::uint8_t* a,
                                std::size_t b,
                                std::uint8_t& c,
                                const std::uint8_t*& d,
                                std::size_t& e) {
        return Skill::parse_client_header(a, b, c, d, e);
    }
    bool parse_c2s_skill_target_rest(const std::uint8_t* a,
                                     std::size_t b,
                                     std::uint8_t& c,
                                     std::uint32_t& d) {
        return Skill::parse_client_target(a, b, c, d);
    }
    std::vector<std::uint8_t>
    frame_s2c_skill(std::uint8_t a, std::uint32_t b, std::uint32_t c, std::uint8_t d) {
        return Skill::frame_server(a, b, c, d);
    }
    std::vector<std::uint8_t> frame_s2c_skill_damage(std::uint8_t a,
                                                     std::uint32_t b,
                                                     std::uint32_t c,
                                                     std::uint8_t d,
                                                     std::int32_t e,
                                                     std::int32_t f,
                                                     std::uint8_t g) {
        return Skill::frame_server_damage(a, b, c, d, e, f, g);
    }
    bool parse_s2c_skill(const std::uint8_t* a, std::size_t b, S2CSkillFields& c) {
        return Skill::parse_server(a, b, c);
    }
    std::vector<std::uint8_t> frame_c2s_preskill(std::uint8_t a, std::uint32_t b) {
        return Skill::frame_preskill(a, b);
    }
    bool
    parse_c2s_preskill(const std::uint8_t* a, std::size_t b, std::uint8_t& c, std::uint32_t& d) {
        return Skill::parse_preskill(a, b, c, d);
    }
    std::vector<std::uint8_t>
    frame_s2c_action_skill(std::uint32_t a, std::uint8_t b, std::uint32_t c) {
        return Skill::frame_action_skill(a, b, c);
    }
    std::vector<std::uint8_t> frame_c2s_rest(std::uint8_t a) {
        return Skill::frame_rest(a);
    }
    bool parse_c2s_rest(const std::uint8_t* a, std::size_t b, std::uint8_t& c) {
        return Skill::parse_rest(a, b, c);
    }
    std::vector<std::uint8_t> frame_s2c_action_rest(std::uint32_t a, std::uint8_t b) {
        return Skill::frame_action_rest(a, b);
    }
    std::vector<std::uint8_t> frame_s2c_action_knee(std::uint32_t a) {
        return Skill::frame_action_knee(a);
    }
    std::vector<std::uint8_t> frame_s2c_action_die(std::uint32_t a) {
        return Skill::frame_action_die(a);
    }
    std::vector<std::uint8_t> frame_s2c_action_beheaded(std::uint32_t a) {
        return Skill::frame_action_beheaded(a);
    }
    std::vector<std::uint8_t> frame_c2s_revival() {
        return Skill::frame_client_revival();
    }
    bool parse_s2c_action(
        const std::uint8_t* a, std::size_t b, std::uint32_t& c, std::uint8_t& d, std::uint8_t& e) {
        return Skill::parse_action(a, b, c, d, e);
    }

    std::vector<std::uint8_t> Skill::frame_skill_up(std::uint8_t index, std::uint8_t level) {
        PacketWriter w;
        w.write_u8(index);
        w.write_u8(level);
        return w.frame(static_cast<std::uint8_t>(S2C::SkillUp));
    }

    std::vector<std::uint8_t> Skill::frame_client_learn_skill(std::uint8_t skill) {
        return one(C2S::LearnSkill, skill);
    }

    std::vector<std::uint8_t> Skill::frame_client_skill_up(std::uint8_t skill) {
        return one(C2S::SkillUp, skill);
    }

    bool Skill::parse_client_skill_index(const std::uint8_t* data,
                                         std::size_t size,
                                         std::uint8_t& skill) {
        if (!data || size < 1) {
            return false;
        }
        skill = data[0];
        return true;
    }

    bool Skill::parse_skill_up(const std::uint8_t* data,
                               std::size_t size,
                               std::uint8_t& index,
                               std::uint8_t& level) {
        if (!data || size < 2) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u8(index) && r.read_u8(level);
    }

    std::vector<std::uint8_t> Skill::frame_skill_info(const std::vector<SkillRankEntry>& entries) {
        PacketWriter w;
        const auto count = static_cast<std::uint8_t>(entries.size() > 255 ? 255 : entries.size());
        w.write_u8(count);
        for (std::uint8_t i = 0; i < count; ++i) {
            w.write_u8(entries[i].index);
            w.write_u8(entries[i].level);
        }
        return w.frame(static_cast<std::uint8_t>(S2C::SkillInfo));
    }

    bool Skill::parse_skill_info(const std::uint8_t* data,
                                 std::size_t size,
                                 std::vector<SkillRankEntry>& entries) {
        entries.clear();
        if (!data || size < 1) {
            return false;
        }
        PacketReader r(data, size);
        std::uint8_t count = 0;
        if (!r.read_u8(count)) {
            return false;
        }
        entries.reserve(count);
        for (std::uint8_t i = 0; i < count; ++i) {
            SkillRankEntry entry;
            if (!r.read_u8(entry.index) || !r.read_u8(entry.level)) {
                return false;
            }
            entries.push_back(entry);
        }
        return true;
    }
}  // namespace openkal::protocol
