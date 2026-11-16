/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Skills.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/Emote.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/protocol/ThiefSkill.h"
#include "openkal/protocol/Transform.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <string_view>

namespace openkal::client {

    SkillExpr SkillExpr::make_null() {
        return SkillExpr{};
    }

    SkillExpr SkillExpr::make_number(double v) {
        SkillExpr e;
        e.m_kind = Kind::Number;
        e.m_number = v;
        return e;
    }

    SkillExpr SkillExpr::make_symbol(std::string name) {
        SkillExpr e;
        e.m_kind = Kind::Symbol;
        e.m_symbol = std::move(name);
        return e;
    }

    SkillExpr SkillExpr::make_call(std::string op, std::vector<SkillExpr> args) {
        SkillExpr e;
        e.m_kind = Kind::Call;
        e.m_symbol = std::move(op);
        e.m_args = std::move(args);
        return e;
    }

    namespace {

        int role_to_class(const std::string& role) {
            if (role == "knight") {
                return 0;
            }
            if (role == "mage" || role == "magician") {
                return 1;
            }
            if (role == "archer") {
                return 2;
            }
            if (role == "thief") {
                return 3;
            }
            if (role == "transform") {
                return -1;
            }
            if (role == "monster") {
                return -2;
            }
            if (role == "common") {
                return -3;
            }
            if (role == "yungsang") {
                return 10;
            }
            if (role == "tuguk") {
                return 14;
            }
            if (role == "gunchi") {
                return 16;
            }
            if (role == "gale") {
                return 18;
            }
            return -100;
        }

        bool type_list_has_token(const platform::Json& type, std::string_view token) {
            if (type.is_string()) {
                return type.as_string() == token;
            }
            if (!type.is_array()) {
                return false;
            }
            for (const auto& e : type.array()) {
                if (e.as_string() == token) {
                    return true;
                }
            }
            return false;
        }

        bool type_list_has_passive(const platform::Json& type) {
            return type_list_has_token(type, "passive");
        }

        bool type_list_has_use(const platform::Json& type) {
            return type_list_has_token(type, "use");
        }

        /** True when expr is the jobsystem `weapon_speed` symbol. */
        [[nodiscard]] bool expr_is_weapon_speed(const platform::Json& expr) {
            return expr.is_string() && expr.as_string() == "weapon_speed";
        }

        /**
         * Parses compare[3] as a literal, `weapon_speed`, or `plus(weapon_speed, N)`.
         * @param expr Compare expression.
         * @param ms Literal milliseconds, or the addend when weapon_speed is set.
         * @param weapon_speed True when cooldown includes equipped aspeed.
         */
        void parse_compare_cooldown_expr(const platform::Json& expr, int& ms, bool& weapon_speed) {
            if (expr.is_number()) {
                ms = expr.as_int(0);
                return;
            }
            if (expr_is_weapon_speed(expr)) {
                weapon_speed = true;
                return;
            }
            if (!expr.is_array() || expr.array().empty()) {
                return;
            }
            const auto& arr = expr.array();
            if (arr[0].is_number()) {
                ms = arr[0].as_int(0);
                return;
            }
            if (!arr[0].is_string() || arr[0].as_string() != "plus" || arr.size() != 3) {
                return;
            }
            if (expr_is_weapon_speed(arr[1]) && arr[2].is_number()) {
                weapon_speed = true;
                ms = arr[2].as_int(0);
                return;
            }
            if (expr_is_weapon_speed(arr[2]) && arr[1].is_number()) {
                weapon_speed = true;
                ms = arr[1].as_int(0);
            }
        }

        void apply_compare_cooldown(SkillDef& def, const platform::Json& compares) {
            if (!compares.is_array()) {
                return;
            }
            for (const auto& entry : compares.array()) {
                if (!entry.is_object()) {
                    continue;
                }
                const int compare_id = entry.at("id").as_int(0);
                const platform::Json& expr = entry.at("expr");
                if (compare_id == 2 && expr.is_number()) {
                    def.set_cast_range(expr.as_int(0));
                    continue;
                }
                if (compare_id == 3 && expr.is_number()) {
                    def.set_compare_preskill_ms(expr.as_int(0));
                    continue;
                }
                if (compare_id == 5 && expr.is_number()) {
                    def.set_rage(expr.as_int(def.rage()));
                    continue;
                }
                if (compare_id != 4) {
                    continue;
                }
                int ms = 0;
                bool weapon_speed = false;
                parse_compare_cooldown_expr(expr, ms, weapon_speed);
                if (ms > 0) {
                    def.set_compare_cooldown_ms(ms);
                }
                if (weapon_speed) {
                    def.set_compare_cooldown_weapon_speed(true);
                }
            }
        }

        SkillExpr parse_skill_expr(const platform::Json& j) {
            if (j.is_number()) {
                return SkillExpr::make_number(j.number());
            }
            if (j.is_string()) {
                return SkillExpr::make_symbol(j.as_string());
            }
            if (!j.is_array() || j.array().empty()) {
                return SkillExpr::make_null();
            }
            const auto& arr = j.array();
            if (arr[0].is_string()) {
                const std::string op = arr[0].as_string();
                std::vector<SkillExpr> args;
                args.reserve(arr.size() > 0 ? arr.size() - 1 : 0);
                for (std::size_t i = 1; i < arr.size(); ++i) {
                    args.push_back(parse_skill_expr(arr[i]));
                }
                return SkillExpr::make_call(op, std::move(args));
            }
            // Bare number list: treat first element as value.
            if (arr[0].is_number()) {
                return SkillExpr::make_number(arr[0].number());
            }
            return SkillExpr::make_null();
        }

        std::optional<double>
        eval_skill_expr(const SkillExpr& expr, int skill_level, const SkillAbilityVars& vars);

        SkillDef skill_from_json(const platform::Json& root) {
            SkillDef def;
            const platform::Json& class_j = root.at("class");
            if (class_j.is_number()) {
                def.set_clazz(class_j.as_int(0));
            } else if (class_j.is_string()) {
                def.set_clazz(role_to_class(class_j.as_string()));
            }
            def.set_role(root.at("role").as_string());
            if (def.role().empty() && class_j.is_string()) {
                def.set_role(class_j.as_string());
            }
            if (!def.role().empty() && !class_j.is_number()) {
                def.set_clazz(role_to_class(def.role()));
            }

            def.set_index(root.at("index").as_int(0));
            def.set_max_level(root.at("maxLevel").as_int(1));
            def.set_mp(root.at("mp").as_int(0));
            def.set_rage(root.at("rage").as_int(0));
            def.set_level_limit(root.at("levelLimit").as_int(0));
            if (def.level_limit() == 0) {
                const platform::Json& limit = root.at("limit");
                if (limit.is_array() && !limit.array().empty()) {
                    def.set_level_limit(limit.array()[0].as_int(0));
                } else if (limit.is_number()) {
                    def.set_level_limit(limit.as_int(0));
                }
            }

            const platform::Json& delay = root.at("delay");
            if (delay.is_array()) {
                if (!delay.array().empty()) {
                    def.set_delay_pre_ms(delay.array()[0].as_int(0));
                }
                if (delay.array().size() >= 2) {
                    def.set_delay_ms(delay.array()[1].as_int(0));
                }
            }

            def.set_name(root.at("name").as_string());
            def.set_comment(root.at("comment").as_string());
            if (def.name().empty()) {
                def.set_name(def.comment());
            }
            {
                const platform::Json& desc = root.at("desc");
                if (desc.is_string()) {
                    def.set_desc(desc.as_string());
                } else if (desc.is_array() && !desc.array().empty()) {
                    def.set_desc(desc.array()[0].as_string());
                }
            }
            {
                const platform::Json& ability = root.at("ability");
                if (ability.is_string()) {
                    def.set_ability(ability.as_string());
                } else if (ability.is_array() && !ability.array().empty()) {
                    def.set_ability(ability.array()[0].as_string());
                }
            }
            {
                const platform::Json& parameters = root.at("parameters");
                if (parameters.is_array()) {
                    std::vector<SkillParameter> params;
                    for (const auto& entry : parameters.array()) {
                        if (!entry.is_object()) {
                            continue;
                        }
                        SkillParameter p;
                        p.id = entry.at("id").as_int(0);
                        p.expr = parse_skill_expr(entry.at("expr"));
                        if (p.id > 0) {
                            params.push_back(std::move(p));
                        }
                    }
                    def.set_parameters(std::move(params));
                }
            }
            def.set_key(root.at("key").as_string());
            const platform::Json& image = root.at("image");
            if (image.is_string()) {
                def.set_image(image.as_string());
            } else if (image.is_array() && !image.array().empty()) {
                def.set_image(image.array()[0].as_string());
            }
            const platform::Json& grade = root.at("grade");
            if (grade.is_array() && !grade.array().empty()) {
                def.set_grade_mask(grade.array()[0].as_int(0));
            } else if (grade.is_number()) {
                def.set_grade_mask(grade.as_int(0));
            }
            {
                const platform::Json& limit = root.at("limit");
                if (limit.is_array()) {
                    if (limit.array().size() >= 3) {
                        def.set_require_skill_index(limit.array()[2].as_int(0));
                    }
                    if (limit.array().size() >= 4) {
                        def.set_require_skill_level(limit.array()[3].as_int(0));
                    }
                }
            }
            {
                const platform::Json& auto_learn = root.at("autoLearnLevels");
                if (auto_learn.is_array()) {
                    std::vector<int> levels;
                    levels.reserve(auto_learn.array().size());
                    for (const auto& value : auto_learn.array()) {
                        levels.push_back(value.as_int(0));
                    }
                    def.set_auto_learn_levels(std::move(levels));
                }
            }
            {
                const platform::Json& manual = root.at("manualLearn");
                def.set_manual_learn(manual.type() == platform::Json::Type::Bool &&
                                     manual.boolean());
            }
            {
                const platform::Json& awaken = root.at("awaken");
                def.set_awaken(awaken.type() == platform::Json::Type::Bool && awaken.boolean());
            }
            {
                const platform::Json* extra = root.get("extra");
                if (extra && extra->is_object()) {
                    const platform::Json* cond = extra->get("condition");
                    if (cond && cond->is_array() && cond->array().size() >= 2) {
                        def.set_require_skill_index(
                            cond->array()[0].as_int(def.require_skill_index()));
                        def.set_require_skill_level(
                            cond->array()[1].as_int(def.require_skill_level()));
                    }
                }
            }
            def.set_passive(type_list_has_passive(root.at("type")));
            def.set_usable(type_list_has_use(root.at("type")));
            def.set_once(type_list_has_token(root.at("type"), "once"));
            {
                const platform::Json& motion = root.at("motion");
                if (motion.is_array() && !motion.array().empty()) {
                    def.set_motion_prep(motion.array()[0].as_int(0));
                    if (motion.array().size() >= 2) {
                        def.set_motion_fire(motion.array()[1].as_int(0));
                    }
                    if (motion.array().size() >= 3) {
                        def.set_attack_auto(motion.array()[2].as_string() == "attackauto");
                    }
                }
            }
            {
                const platform::Json& effect = root.at("effect");
                if (effect.is_array()) {
                    const auto& arr = effect.array();
                    for (std::size_t i = 0; i < arr.size() && i < 3; ++i) {
                        def.set_effect(static_cast<int>(i), arr[i].as_string());
                    }
                } else if (effect.is_string()) {
                    def.set_effect(0, effect.as_string());
                }
            }
            if (!def.passive() && def.image().size() >= 7 &&
                def.image().compare(0, 7, "passive") == 0) {
                def.set_passive(true);
            }
            if (def.clazz() == 2 && protocol::archer_is_known_skill(def.index())) {
                const auto effect = protocol::archer_effect_kind(def.index());
                const bool passive = effect == protocol::ArcherEffectKind::Passive ||
                                     effect == protocol::ArcherEffectKind::Siege;
                def.set_passive(passive);
                def.set_usable(!passive && !protocol::archer_is_blocked_skill(def.index()));
            }
            apply_compare_cooldown(def, root.at("compares"));
            for (const auto& entry : root.at("compares").array()) {
                const int id = entry.at("id").as_int(0);
                if (id != 3 && id != 4) {
                    continue;
                }
                if ((def.clazz() == 1 &&
                     (def.index() == 43 || def.index() == 45 || def.index() == 48)) ||
                    (def.clazz() == 2 && def.index() == 42) ||
                    (def.clazz() == 0 && def.index() == 45)) {
                    continue;
                }
                const auto& expr = entry.at("expr");
                std::vector<int> values;
                for (int rank = 1; rank <= std::max(1, def.max_level()); ++rank) {
                    const bool rank_list = expr.is_array() && !expr.array().empty() &&
                                           expr.array().front().is_number();
                    const auto& term =
                        rank_list
                            ? expr.array()[std::min<std::size_t>(rank - 1, expr.array().size() - 1)]
                            : expr;
                    const auto value = eval_skill_expr(parse_skill_expr(term), rank, {});
                    if (!value) {
                        values.clear();
                        break;
                    }
                    values.push_back(
                        static_cast<int>(std::clamp(*value, 0.0, static_cast<double>(INT32_MAX))));
                }
                if (id == 3) {
                    def.set_preskill_by_level(std::move(values));
                } else {
                    def.set_cooldown_by_level(std::move(values));
                }
            }
            {
                const platform::Json& sound = root.at("sound");
                if (sound.is_array()) {
                    const auto& arr = sound.array();
                    for (std::size_t i = 0; i < arr.size() && i < 3; ++i) {
                        def.set_sound(static_cast<int>(i), arr[i].as_string());
                    }
                } else if (sound.is_string()) {
                    def.set_sound(0, sound.as_string());
                }
            }
            return def;
        }

    }  // namespace

    namespace {

        std::optional<double>
        eval_skill_expr(const SkillExpr& expr, int skill_level, const SkillAbilityVars& vars) {
            switch (expr.kind()) {
            case SkillExpr::Kind::Null:
                return std::nullopt;
            case SkillExpr::Kind::Number:
                return expr.number();
            case SkillExpr::Kind::Symbol:
                if (expr.symbol() == "skill_level") {
                    return static_cast<double>(std::max(1, skill_level));
                }
                if (!vars.has_stats) {
                    return std::nullopt;
                }
                if (expr.symbol() == "char_level") {
                    return static_cast<double>(vars.char_level);
                }
                if (expr.symbol() == "char_str") {
                    return static_cast<double>(vars.char_str);
                }
                if (expr.symbol() == "char_hth") {
                    return static_cast<double>(vars.char_hth);
                }
                if (expr.symbol() == "char_int") {
                    return static_cast<double>(vars.char_int);
                }
                if (expr.symbol() == "char_wis") {
                    return static_cast<double>(vars.char_wis);
                }
                if (expr.symbol() == "char_dex") {
                    return static_cast<double>(vars.char_dex);
                }
                if (expr.symbol() == "lightning_mastery") {
                    return static_cast<double>(vars.lightning_mastery);
                }
                if (expr.symbol() == "char_org_dodge") {
                    return static_cast<double>(vars.char_org_dodge);
                }
                return std::nullopt;
            case SkillExpr::Kind::Call: {
                const std::string& op = expr.op();
                const auto& args = expr.args();
                if (op == "plus") {
                    double sum = 0.0;
                    for (const SkillExpr& a : args) {
                        const auto v = eval_skill_expr(a, skill_level, vars);
                        if (!v) {
                            return std::nullopt;
                        }
                        sum += *v;
                    }
                    return sum;
                }
                if (op == "mul") {
                    double prod = 1.0;
                    for (const SkillExpr& a : args) {
                        const auto v = eval_skill_expr(a, skill_level, vars);
                        if (!v) {
                            return std::nullopt;
                        }
                        prod *= *v;
                    }
                    return prod;
                }
                if (op == "minus" && args.size() == 2) {
                    const auto a = eval_skill_expr(args[0], skill_level, vars);
                    const auto b = eval_skill_expr(args[1], skill_level, vars);
                    if (!a || !b) {
                        return std::nullopt;
                    }
                    return *a - *b;
                }
                if (op == "min" && args.size() == 2) {
                    const auto a = eval_skill_expr(args[0], skill_level, vars);
                    const auto b = eval_skill_expr(args[1], skill_level, vars);
                    if (!a || !b) {
                        return std::nullopt;
                    }
                    return std::min(*a, *b);
                }
                if (op == "div" && args.size() == 2) {
                    const auto a = eval_skill_expr(args[0], skill_level, vars);
                    const auto b = eval_skill_expr(args[1], skill_level, vars);
                    if (!a || !b || *b == 0.0) {
                        return std::nullopt;
                    }
                    return static_cast<double>(static_cast<std::int64_t>(*a) /
                                               static_cast<std::int64_t>(*b));
                }
                return std::nullopt;
            }
            }
            return std::nullopt;
        }

        std::string strip_kal_markup(const std::string& raw) {
            std::string out;
            out.reserve(raw.size());
            for (std::size_t i = 0; i < raw.size(); ++i) {
                if (raw[i] == '#' && i + 1 < raw.size()) {
                    const char tag = raw[i + 1];
                    if (tag == 'n' || tag == 'N') {
                        out.push_back('\n');
                        i += 1;
                        continue;
                    }
                    if ((tag == 'c' || tag == 'C' || tag == 'p' || tag == 'P') &&
                        i + 2 < raw.size() && raw[i + 2] == '(') {
                        const auto end = raw.find(')', i + 3);
                        if (end != std::string::npos) {
                            i = end;
                            continue;
                        }
                    }
                }
                out.push_back(raw[i]);
            }
            while (!out.empty() && (out.front() == '\n' || out.front() == ' ')) {
                out.erase(out.begin());
            }
            while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) {
                out.pop_back();
            }
            return out;
        }

    }  // namespace

    const std::string& SkillDef::sound(int gubun) const {
        static const std::string empty;
        // Mage Speed Up uses a silent target effect even with older skill catalogs.
        if (clazz() == 1 && index() == 26) {
            return empty;
        }
        if (gubun < 0 || gubun >= static_cast<int>(m_sounds.size())) {
            return empty;
        }
        return m_sounds[static_cast<std::size_t>(gubun)];
    }

    void SkillDef::set_sound(int gubun, std::string v) {
        if (gubun < 0 || gubun >= static_cast<int>(m_sounds.size())) {
            return;
        }
        m_sounds[static_cast<std::size_t>(gubun)] = std::move(v);
    }

    const std::string& SkillDef::effect(int gubun) const {
        static const std::string empty;
        if (gubun < 0 || gubun >= static_cast<int>(m_effects.size())) {
            return empty;
        }
        return m_effects[static_cast<std::size_t>(gubun)];
    }

    void SkillDef::set_effect(int gubun, std::string v) {
        if (gubun < 0 || gubun >= static_cast<int>(m_effects.size())) {
            return;
        }
        m_effects[static_cast<std::size_t>(gubun)] = std::move(v);
    }

    std::string SkillDef::display_name() const {
        if (!m_name.empty()) {
            return m_name;
        }
        if (!m_comment.empty()) {
            return m_comment;
        }
        return "#" + std::to_string(m_index);
    }

    std::string SkillDef::tooltip_body() const {
        return strip_kal_markup(m_desc);
    }

    std::optional<std::int64_t> SkillDef::eval_parameter(int param_id, int skill_level) const {
        return eval_parameter(param_id, skill_level, SkillAbilityVars{});
    }

    std::optional<std::int64_t>
    SkillDef::eval_parameter(int param_id, int skill_level, const SkillAbilityVars& vars) const {
        for (const SkillParameter& p : m_parameters) {
            if (p.id != param_id) {
                continue;
            }
            const auto v = eval_skill_expr(p.expr, skill_level, vars);
            if (!v) {
                return std::nullopt;
            }
            return static_cast<std::int64_t>(std::llround(*v));
        }
        return std::nullopt;
    }

    std::string SkillDef::format_ability(int skill_level) const {
        return format_ability(skill_level, SkillAbilityVars{});
    }

    std::string SkillDef::format_ability(int skill_level, const SkillAbilityVars& vars) const {
        if (m_index >= protocol::kTalentSkillFirst && m_index <= protocol::kTalentSkillLast) {
            std::uint64_t mask = 0;
            for (int i = 0; i < protocol::kTalentNodeCount - 1; ++i) {
                const auto& node = protocol::talent_nodes()[i];
                if (static_cast<int>(node.path) == m_index - protocol::kTalentSkillFirst &&
                    node.ordinal < skill_level) {
                    mask |= protocol::talent_bit(i);
                }
            }
            const auto bonus = protocol::talent_bonuses(mask);
            switch (m_index - protocol::kTalentSkillFirst) {
            case 0:
                return "Min/Max Physical +" + std::to_string(bonus.physical) + "\nDefense " +
                       std::to_string(bonus.defense) + "\nExplosive Blow power +" +
                       std::to_string(bonus.explosive_percent) + "%";
            case 1:
                return "Min/Max Magical +" + std::to_string(bonus.magical) + "\nMana " +
                       std::to_string(bonus.mana_percent) + "%\nExplosive Blow power +" +
                       std::to_string(bonus.explosive_percent) + "%";
            case 2:
                return "Defense +" + std::to_string(bonus.defense) + "\nMana +" +
                       std::to_string(bonus.mana) + "\nMax Physical " +
                       std::to_string(bonus.max_physical) + "\nAuto HP +" +
                       std::to_string(bonus.hp_per_pulse) + " / 2s";
            case 3:
                return "Auto MP +" + std::to_string(bonus.mp_per_pulse) +
                       " / 2s\nAuto Pick: " + std::to_string(bonus.pickup_count) + " items / 5s";
            case 4:
                return "Speed +" + std::to_string(bonus.speed_percent) + "%\nEvasion " +
                       std::to_string(bonus.evasion);
            }
        }
        if (m_ability.empty()) {
            return {};
        }
        const int lv = std::max(1, skill_level);
        std::string filled;
        filled.reserve(m_ability.size() + 16);
        for (std::size_t i = 0; i < m_ability.size(); ++i) {
            if (m_ability[i] == '%' && i + 1 < m_ability.size() && m_ability[i + 1] == '%') {
                filled.push_back('%');
                ++i;
                continue;
            }
            // Kal printf-style placeholder: %1!d! / %2!d! / %1!s! …
            if (m_ability[i] == '%' && i + 1 < m_ability.size() &&
                std::isdigit(static_cast<unsigned char>(m_ability[i + 1]))) {
                std::size_t j = i + 1;
                int id = 0;
                while (j < m_ability.size() &&
                       std::isdigit(static_cast<unsigned char>(m_ability[j]))) {
                    id = id * 10 + (m_ability[j] - '0');
                    ++j;
                }
                if (j < m_ability.size() && m_ability[j] == '!') {
                    const auto bang2 = m_ability.find('!', j + 1);
                    if (bang2 != std::string::npos) {
                        const auto val = eval_parameter(id, lv, vars);
                        if (val) {
                            filled += std::to_string(*val);
                        } else {
                            filled.push_back('?');
                        }
                        i = bang2;
                        continue;
                    }
                }
            }
            filled.push_back(m_ability[i]);
        }
        return strip_kal_markup(filled);
    }

    namespace {

        std::vector<const SkillDef*> sorted_class_skills(const Catalog& cat, int clazz) {
            auto list = cat.for_class(clazz);
            std::stable_sort(list.begin(), list.end(), [](const SkillDef* a, const SkillDef* b) {
                if (a->passive() != b->passive()) {
                    return !a->passive() && b->passive();
                }
                if (a->level_limit() != b->level_limit()) {
                    return a->level_limit() < b->level_limit();
                }
                return a->index() < b->index();
            });
            return list;
        }

    }  // namespace

    bool Catalog::is_shared_action(const SkillDef& s) {
        return s.role() == "action" ||
               (s.key().size() >= 7 && s.key().compare(0, 7, "action-") == 0);
    }

    SocialAction social_action(const SkillDef& skill) {
        if (!Catalog::is_shared_action(skill)) {
            return SocialAction::None;
        }
        std::string name;
        for (unsigned char ch : skill.name()) {
            if (std::isalnum(ch)) {
                name += static_cast<char>(std::tolower(ch));
            }
        }
        if (name == "guild") {
            return SocialAction::Guild;
        }
        if (name == "party") {
            return SocialAction::Party;
        }
        if (name == "trade") {
            return SocialAction::Trade;
        }
        if (name == "whisper" || name == "wisper") {
            return SocialAction::Whisper;
        }
        if (name == "tsrelation" || name == "teachersstudentrelation") {
            return SocialAction::Removed;
        }
        return SocialAction::None;
    }

    bool social_action_uses_active_slot(SocialAction action) {
        return action == SocialAction::Party || action == SocialAction::Trade;
    }

    bool Catalog::is_castable(const SkillDef& s) {
        if (s.passive()) {
            return false;
        }
        if (is_shared_action(s)) {
            return s.index() == 0 || s.index() == 11 ||
                   protocol::Emote::motion_for_shortcut(s.index()) != 0;
        }
        if (s.index() == 0 || s.index() == 11) {
            return true;
        }
        if (s.index() == 1) {
            return true;
        }
        // Meditation and other active "once" skills have no MP, delay, or `use`.
        if (s.once()) {
            return true;
        }
        if (s.mp() > 0 || s.delay_ms() > 0 || s.delay_pre_ms() > 0) {
            return true;
        }
        // skill definition delay is often 0 for melee blows; jobsystem compare[3] is the
        // icon timer (compatibility behavior). Thief skills store MP in params.
        return s.usable() && (s.compare_cooldown_ms() > 0 || s.compare_cooldown_weapon_speed());
    }

    bool Catalog::skill_bar_sets_active(CastKind kind) {
        return kind == CastKind::Combat || kind == CastKind::Behead || kind == CastKind::TargetArea;
    }

    bool Catalog::uses_range_skill_selector(int clazz, int skill_index) {
        if (clazz == 1) {
            return protocol::mage_uses_range_skill_selector(skill_index);
        }
        if (clazz == 2) {
            return protocol::archer_uses_range_skill_selector(skill_index);
        }
        return false;
    }

    bool Catalog::skill_bar_casts_on_click(int clazz, int skill_index) {
        if (skill_index == 0 || skill_index == 11 ||
            protocol::Emote::motion_for_shortcut(skill_index) != 0 ||
            (clazz == 1 && skill_index == 15)) {
            return true;
        }
        if (protocol::is_transform_into_skill(skill_index) ||
            protocol::is_animal_skill(skill_index)) {
            return true;
        }
        if (clazz == 2 &&
            protocol::archer_special_kind(skill_index) == protocol::ArcherSpecialKind::FocusShot) {
            return true;
        }
        // Compatibility behavior: type `once` calls SendSkill instead of
        // SetActionMacro. Storms / Arrow Rain / Poison Cloud enter RangeSkillCast;
        // Splashy Ice fires immediately.
        if (uses_range_skill_selector(clazz, skill_index)) {
            return true;
        }
        return clazz == 1 && skill_index == 24;
    }

    CastKind Catalog::cast_kind_for(int clazz, int skill_index) {
        if (protocol::Emote::motion_for_shortcut(skill_index) != 0) {
            return CastKind::Emote;
        }
        if (skill_index == protocol::kBeaconSkill) {
            return CastKind::SelfHeal;
        }
        if (skill_index == 0) {
            return CastKind::Boost;
        }
        if (skill_index == 11) {
            return CastKind::Rest;
        }
        if (skill_index == 1) {
            return CastKind::Behead;
        }
        if (protocol::is_animal_skill(skill_index)) {
            return protocol::animal_skill_is_self(skill_index) ? CastKind::SelfHeal
                                                               : CastKind::Combat;
        }
        if (clazz == 1) {
            switch (protocol::mage_target_shape(skill_index)) {
            case protocol::MageTargetShape::Self:
                return CastKind::SelfHeal;
            case protocol::MageTargetShape::Monster:
            case protocol::MageTargetShape::HostileCharacter:
                return CastKind::Combat;
            case protocol::MageTargetShape::Player:
                return CastKind::Player;
            case protocol::MageTargetShape::DeadPlayer:
                return CastKind::DeadPlayer;
            case protocol::MageTargetShape::Party:
                return CastKind::Party;
            case protocol::MageTargetShape::SelfArea:
                return CastKind::SelfArea;
            case protocol::MageTargetShape::TargetArea:
                return CastKind::TargetArea;
            case protocol::MageTargetShape::GroundPosition:
                return CastKind::GroundPosition;
            case protocol::MageTargetShape::None:
            default:
                return CastKind::None;
            }
        }
        if (clazz == 0) {
            if (protocol::knight_special_kind(skill_index) ==
                protocol::KnightSpecialKind::WalkOnAir) {
                return CastKind::SelfHeal;
            }
            switch (protocol::knight_target_shape(skill_index)) {
            case protocol::KnightTargetShape::Self:
                return CastKind::SelfHeal;
            case protocol::KnightTargetShape::Monster:
            case protocol::KnightTargetShape::HostileCharacter:
            case protocol::KnightTargetShape::TargetArea:
                return CastKind::Combat;
            case protocol::KnightTargetShape::Player:
                return CastKind::Player;
            case protocol::KnightTargetShape::DeadPlayer:
                return CastKind::DeadPlayer;
            case protocol::KnightTargetShape::Party:
                return CastKind::Party;
            case protocol::KnightTargetShape::SelfArea:
                return CastKind::SelfArea;
            case protocol::KnightTargetShape::None:
            default:
                return CastKind::None;
            }
        }
        if (clazz == 2) {
            if (protocol::archer_is_blocked_skill(skill_index)) {
                return CastKind::None;
            }
            // Arrow of Silence, Powerful Silence, and Mana Burn are player-only.
            if (skill_index == 15 || skill_index == 27 || skill_index == 34) {
                return CastKind::Player;
            }
            switch (protocol::archer_target_shape(skill_index)) {
            case protocol::ArcherTargetShape::Self:
                return CastKind::SelfHeal;
            case protocol::ArcherTargetShape::Monster:
            case protocol::ArcherTargetShape::HostileCharacter:
                return CastKind::Combat;
            case protocol::ArcherTargetShape::Party:
                return CastKind::Party;
            case protocol::ArcherTargetShape::TargetArea:
                return CastKind::TargetArea;
            case protocol::ArcherTargetShape::GroundPosition:
                return CastKind::GroundPosition;
            case protocol::ArcherTargetShape::None:
            default:
                return CastKind::None;
            }
        }
        if (clazz == 3) {
            if (protocol::thief_is_awaken_skill(skill_index)) {
                return CastKind::None;
            }
            switch (protocol::thief_target_shape(skill_index)) {
            case protocol::ThiefTargetShape::Self:
                return CastKind::SelfHeal;
            case protocol::ThiefTargetShape::HostileCharacter:
                return CastKind::Combat;
            case protocol::ThiefTargetShape::Party:
                return CastKind::Party;
            case protocol::ThiefTargetShape::SelfArea:
                return CastKind::SelfArea;
            case protocol::ThiefTargetShape::TargetArea:
                return CastKind::TargetArea;
            case protocol::ThiefTargetShape::None:
            default:
                return CastKind::None;
            }
        }
        return CastKind::Combat;
    }

    int Catalog::magic_skill_range(int skill_index, int skill_level) {
        const int lv = std::max(1, skill_level);
        switch (skill_index) {
        case 2:
            return 110 + 3 * lv;
        case 3:
            return 120 + 4 * lv;
        case 4:
            return 110 + 6 * lv;
        case 65:
        case 67:
            return 300;
        default:
            return 120;
        }
    }

    bool Catalog::is_mage_combat_skill(int skill_index) {
        switch (skill_index) {
        case 2:  // Fire Magic
        case 3:  // Ice Magic
        case 4:  // Lightning Magic
        case 9:
        case 23:
        case 24:
        case 25:
        case 27:
        case 31:  // Lightning Summons
        case 32:  // Hail
        case 33:  // Flame Bomb
        case 37:  // Cold Binding
        case 40:
        case 41:  // Chain Lightning
        case 42:  // Thunder
        case 44:  // Ice Restriction
        case 46:  // Explosion
        case 65:
        case 67:
            return true;
        default:
            return false;
        }
    }

    bool Catalog::is_archer_combat_skill(int skill_index) {
        const CastKind kind = cast_kind_for(2, skill_index);
        return kind == CastKind::Combat || kind == CastKind::TargetArea;
    }

    bool Catalog::is_knight_combat_skill(int skill_index) {
        switch (skill_index) {
        case 3:   // compatibility behavior / Lightning Slash
        case 5:   // compatibility behavior
        case 7:   // compatibility behavior
        case 9:   // compatibility behavior
        case 13:  // compatibility behavior
        case 15:  // Shield Attack
        case 16:  // compatibility behavior
        case 17:  // compatibility behavior
        case 25:  // compatibility behavior
        case 41:  // powerful widening wound
        case 42:  // Provocation of Blow
            return true;
        default:
            return false;
        }
    }

    bool Catalog::is_thief_combat_skill(int skill_index) {
        const CastKind kind = cast_kind_for(3, skill_index);
        return kind == CastKind::Combat || kind == CastKind::TargetArea ||
               kind == CastKind::SelfArea;
    }

    bool Catalog::is_melee_combat_skill(int clazz, int skill_index) {
        if (clazz == 0) {
            return is_knight_combat_skill(skill_index);
        }
        if (clazz == 3) {
            return is_thief_combat_skill(skill_index);
        }
        return false;
    }

    bool Catalog::is_taunt_skill(int clazz, int skill_index) {
        return clazz == 0 && (skill_index == 4 || skill_index == 8);
    }

    int
    Catalog::skill_cast_range(int clazz, const SkillDef* def, int skill_level, int weapon_range) {
        const int lv = std::max(1, skill_level);
        const int idx = def ? def->index() : 0;
        if (is_taunt_skill(clazz, idx)) {
            if (idx == 4) {
                // compatibility behavior compare[1] = 115 + 7 * skill_level.
                return 115 + 7 * lv;
            }
            // compatibility behavior compare[1] = 136 + 8 * skill_level.
            return 136 + 8 * lv;
        }
        if (clazz == 0 && idx == 21) {
            return 60 + 20 * lv;
        }
        if (clazz == 2 && idx == 16) {
            // Focus Shot compare[1] = weapon_range + 60.
            return std::max(1, weapon_range) + 60;
        }
        if (def && def->cast_range() > 0) {
            return def->cast_range();
        }
        return std::max(1, weapon_range) + 8;
    }

    int Catalog::ui_cooldown_ms(const SkillDef& def, int weapon_aspeed_ms, int skill_level) {
        const auto& values = def.cooldown_by_level();
        if (!values.empty()) {
            return values[std::clamp(skill_level, 1, static_cast<int>(values.size())) - 1];
        }
        // compatibility behavior / GetDelayTime use runtime state[3] (JSON compare id 4).
        // Client PRESKILL wait is runtime state[2] (see preskill_ms). delay[] is server
        // GetDelay (pre + cd), not the icon.
        if (def.compare_cooldown_weapon_speed()) {
            // plus(weapon_speed, N) stores N in compare_cooldown_ms (compatibility behavior).
            return std::max(1, weapon_aspeed_ms + std::max(0, def.compare_cooldown_ms()));
        }
        return std::max(0, def.compare_cooldown_ms());
    }

    int Catalog::preskill_ms(const SkillDef& def, int skill_level, int fire_mastery) {
        if (def.clazz() == 1 && def.index() == 2) {
            return std::max(0, 1300 - 1300 * 5 * std::max(0, fire_mastery) / 100);
        }
        const auto& values = def.preskill_by_level();
        if (!values.empty()) {
            return values[std::clamp(skill_level, 1, static_cast<int>(values.size())) - 1];
        }
        if (def.compare_preskill_ms() > 0) {
            return def.compare_preskill_ms();
        }
        return std::max(0, def.delay_pre_ms());
    }

    int
    Catalog::skill_spend_mp(int clazz, const SkillDef* def, int skill_level, int character_level) {
        if (!def) {
            return 0;
        }
        const int lv = std::max(1, skill_level);
        int mp = std::max(0, def->mp());
        if (clazz == 0) {
            mp = protocol::knight_skill_mp_cost(def->index(), lv, character_level, def->mp());
        } else if (clazz == 1) {
            mp = protocol::mage_skill_mp_cost(def->index(), lv, character_level, def->mp());
        } else if (clazz == 2) {
            mp = protocol::archer_skill_mp_cost(def->index(), lv, character_level, def->mp());
        } else if (clazz == 3) {
            mp = protocol::thief_skill_mp_cost(def->index(), lv, character_level, def->mp());
        }
        return mp;
    }

    const SkillDef* Catalog::find(int clazz, int index) const {
        const SkillDef* shared = nullptr;
        for (const auto& s : m_all) {
            if (s.index() != index) {
                continue;
            }
            if (s.clazz() == clazz) {
                return &s;
            }
            if (is_shared_action(s)) {
                shared = &s;
            }
        }
        return shared;
    }

    const SkillDef* Catalog::find_any(int index) const {
        for (const auto& s : m_all) {
            if (s.index() == index) {
                return &s;
            }
        }
        return nullptr;
    }

    void Catalog::rebuild_buff_icon_table() {
        m_buff_icons.clear();
        for (const auto& skill : m_all) {
            const std::uint64_t bit = protocol::skill_to_bstate(skill.clazz(), skill.index());
            if (bit == 0 || skill.image().empty()) {
                continue;
            }
            BuffHudEntry entry;
            entry.icon = skill.image();
            if (entry.icon.find('.') == std::string::npos) {
                entry.icon += ".bmp";
            }
            entry.name = skill.name();
            m_buff_icons[bit] = std::move(entry);
        }
    }

    const Catalog::BuffHudEntry* Catalog::buff_hud_entry(std::uint64_t bstate) const {
        if (bstate == 0) {
            return nullptr;
        }
        const auto exact = m_buff_icons.find(bstate);
        if (exact != m_buff_icons.end()) {
            return &exact->second;
        }
        for (int i = 0; i < 64; ++i) {
            const std::uint64_t bit = std::uint64_t{1} << i;
            if ((bstate & bit) == 0) {
                continue;
            }
            const auto it = m_buff_icons.find(bit);
            if (it != m_buff_icons.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

    std::string_view Catalog::buff_icon_for(std::uint64_t bstate) const {
        const BuffHudEntry* entry = buff_hud_entry(bstate);
        return entry != nullptr ? entry->icon : std::string_view{};
    }

    std::string_view Catalog::buff_name_for(std::uint64_t bstate) const {
        const BuffHudEntry* entry = buff_hud_entry(bstate);
        return entry != nullptr ? entry->name : std::string_view{};
    }

    std::string Catalog::buff_icon_for_skill(int clazz, int index) const {
        const SkillDef* skill = find(clazz, index);
        if (!skill) {
            skill = find_any(index);
        }
        if (!skill || skill->image().empty()) {
            return {};
        }
        std::string icon = skill->image();
        if (icon.find('.') == std::string::npos) {
            icon += ".bmp";
        }
        return icon;
    }

    std::string_view Catalog::buff_name_for_skill(int clazz, int index) const {
        const SkillDef* skill = find(clazz, index);
        if (!skill) {
            skill = find_any(index);
        }
        return skill != nullptr ? skill->name() : std::string_view{};
    }

    namespace {
        void append_talent_skills(std::vector<SkillDef>& all) {
            const char* names[] = {"Physical Talents",
                                   "Magical Talents",
                                   "Defensive Talents",
                                   "Quality of Life Talents",
                                   "Upgrade Talents"};
            const char* images[] = {"physical", "magical", "defense", "auto_mp", "speed"};
            const int max_ranks[] = {10, 10, 5, 6, 5};
            for (int clazz = 0; clazz < 4; ++clazz) {
                for (int i = 0; i < protocol::kTalentSkillCount; ++i) {
                    SkillDef def;
                    def.set_clazz(clazz);
                    def.set_index(protocol::kTalentSkillFirst + i);
                    def.set_name(names[i]);
                    def.set_image(std::string("talents/") + images[i] + ".bmp");
                    def.set_grade_mask(1);
                    def.set_level_limit(1);
                    def.set_max_level(max_ranks[i]);
                    def.set_passive(true);
                    def.set_desc("Passive bonuses from committed Talent Tree nodes. All nodes add "
                                 "together. Open Talent Tree to inspect each bonus and penalty.");
                    all.push_back(std::move(def));
                }
                SkillDef beacon;
                beacon.set_clazz(clazz);
                beacon.set_index(protocol::kBeaconSkill);
                beacon.set_name("Beacon");
                beacon.set_image("talents/beacon.bmp");
                beacon.set_grade_mask(1);
                beacon.set_level_limit(1);
                beacon.set_max_level(1);
                beacon.set_delay_ms(static_cast<int>(protocol::kBeaconCooldownMs));
                beacon.set_desc("Self buff unlocked by the final Upgrades talent. Attracts idle "
                                "monsters without taking another player's aggro.");
                beacon.set_ability("Attracts idle monsters within 20 tiles.\nDuration: 30 "
                                   "seconds\nCooldown: 120 seconds");
                all.push_back(std::move(beacon));
            }
        }
    }  // namespace

    bool Catalog::load_json_dir(const std::string& dir) {
        m_all.clear();
        if (!platform::is_directory(dir)) {
            append_talent_skills(m_all);
            rebuild_buff_icon_table();
            return false;
        }
        for (const auto& rel : platform::list_files_recursive(dir)) {
            if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const auto slash = rel.find_last_of('/');
            const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            if (base == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(dir, rel);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                !root.is_object()) {
                continue;
            }
            SkillDef def = skill_from_json(root);
            if (def.index() < 0 ||
                (def.index() >= protocol::kTalentSkillFirst &&
                 def.index() <= protocol::kTalentSkillLast) ||
                def.index() == protocol::kBeaconSkill) {
                continue;
            }
            m_all.push_back(std::move(def));
        }
        const bool have_external = !m_all.empty();
        append_talent_skills(m_all);
        rebuild_buff_icon_table();
        log::info("skills json loaded: " + std::to_string(m_all.size()) + " from " + dir);
        return have_external;
    }

    bool Catalog::load(const std::string& skills_dir) {
        return load_json_dir(skills_dir);
    }

    std::vector<const SkillDef*> Catalog::for_class(int clazz) const {
        std::vector<const SkillDef*> out;
        for (const auto& s : m_all) {
            if (s.clazz() == clazz || is_shared_action(s)) {
                out.push_back(&s);
            }
        }
        return out;
    }

    void PlayerSkills::clear() {
        m_learned.clear();
        m_non_active.clear();
        m_unavailable_actions.clear();
        m_active_index = -1;
        m_hotbar.fill(-1);
        m_hotbar_dirty = false;
    }

    void PlayerSkills::seed_shared_actions(const Catalog& catalog, int clazz) {
        m_non_active.clear();
        m_unavailable_actions.clear();
        for (const SkillDef* s : catalog.for_class(clazz)) {
            if (s && !social_action_uses_active_slot(social_action(*s)) &&
                (social_action(*s) != SocialAction::None ||
                 Catalog::skill_bar_casts_on_click(clazz, s->index()))) {
                m_non_active.insert(s->index());
            }
            if (s && (social_action(*s) == SocialAction::Removed ||
                      (!m_multiplayer && social_action(*s) != SocialAction::None))) {
                m_unavailable_actions.insert(s->index());
                m_non_active.insert(s->index());
                m_learned.erase(s->index());
                continue;
            }
            if (s && Catalog::is_shared_action(*s)) {
                m_learned[s->index()] = 1;
            }
        }
        if (m_non_active.count(m_active_index)) {
            m_active_index = -1;
            m_hotbar_dirty = true;
        }
        for (auto& slot : m_hotbar) {
            if (m_unavailable_actions.count(slot)) {
                slot = -1;
                m_hotbar_dirty = true;
            }
        }
    }

    void PlayerSkills::seed_for_class(int clazz, int player_level, const Catalog& catalog) {
        (void) player_level;
        clear();
        seed_shared_actions(catalog, clazz);
    }

    int PlayerSkills::level_of(int skill_index) const {
        auto it = m_learned.find(skill_index);
        return it == m_learned.end() ? 0 : it->second;
    }

    int PlayerSkills::set_learned_level(int skill_index, int level) {
        const int prev = level_of(skill_index);
        if (level <= 0) {
            m_learned.erase(skill_index);
        } else {
            m_learned[skill_index] = level;
        }
        return prev;
    }

    void PlayerSkills::apply_server_info(const std::unordered_map<int, int>& from_server,
                                         const Catalog& catalog,
                                         int clazz) {
        m_learned = from_server;
        seed_shared_actions(catalog, clazz);
        ensure_default_hotbar(catalog, clazz);
    }

    void PlayerSkills::ensure_default_hotbar(const Catalog& catalog, int clazz) {
        const int slot0 = m_hotbar[0];
        const bool slot0_kept =
            slot0 >= 0 && (protocol::shortcut_is_item(slot0) || protocol::is_animal_skill(slot0) ||
                           protocol::is_transform_into_skill(slot0) || is_learned(slot0));
        if (slot0_kept) {
            if (m_active_index < 0 && protocol::shortcut_is_skill(slot0) &&
                !Catalog::skill_bar_casts_on_click(clazz, slot0)) {
                set_active(slot0);
            }
            return;
        }
        for (const SkillDef* s : sorted_class_skills(catalog, clazz)) {
            if (!is_learned(s->index()) || Catalog::is_shared_action(*s) ||
                !Catalog::is_castable(*s)) {
                continue;
            }
            if (s->index() == 0 || s->index() == 11) {
                continue;
            }
            m_hotbar[0] = s->index();
            set_active(s->index());
            break;
        }

        // A new Mage needs a ranged combat skill in addition to Beheading. Keep
        // Beheading in the first slot, but select Lightning Magic so the default
        // right-click action is magical rather than a physical swing.
        constexpr int mage_class = 1;
        constexpr int lightning_magic_skill = 4;
        if (clazz != mage_class || m_hotbar[1] >= 0 || !is_learned(lightning_magic_skill)) {
            return;
        }
        const SkillDef* lightning_magic = catalog.find(clazz, lightning_magic_skill);
        if (lightning_magic && Catalog::is_castable(*lightning_magic)) {
            m_hotbar[1] = lightning_magic_skill;
            m_active_index = lightning_magic_skill;
        }
    }

    int PlayerSkills::hotbar_slot(int slot) const {
        if (slot < 0 || slot >= static_cast<int>(m_hotbar.size())) {
            return -1;
        }
        return m_hotbar[static_cast<std::size_t>(slot)];
    }

    void PlayerSkills::set_hotbar(int slot, int skill_index) {
        if (slot < 0 || slot >= static_cast<int>(m_hotbar.size())) {
            return;
        }
        if (m_unavailable_actions.count(skill_index)) {
            return;
        }
        const int value = skill_index < 0 ? -1 : skill_index;
        auto& dest = m_hotbar[static_cast<std::size_t>(slot)];
        if (dest == value) {
            return;
        }
        dest = value;
        m_hotbar_dirty = true;
    }

    void PlayerSkills::set_active(int skill_index) {
        if (skill_index == 0 || skill_index == 11 || m_non_active.count(skill_index) ||
            protocol::Emote::motion_for_shortcut(skill_index) != 0) {
            return;
        }
        const int value = skill_index < 0 ? -1 : skill_index;
        if (m_active_index == value) {
            return;
        }
        m_active_index = value;
        m_hotbar_dirty = true;
    }

    void PlayerSkills::activate_hotbar_slot(int slot) {
        const int idx = hotbar_slot(slot);
        if (protocol::shortcut_is_skill(idx)) {
            set_active(idx);
        }
    }

    void PlayerSkills::apply_shortcut(int active_index,
                                      const std::array<int, kHotbarSlots>& slots) {
        const bool invalid_active = active_index == 0 || active_index == 11 ||
                                    m_non_active.count(active_index) ||
                                    protocol::Emote::motion_for_shortcut(active_index) != 0;
        m_active_index = invalid_active ? -1 : active_index;
        m_hotbar_dirty = m_hotbar_dirty || invalid_active;
        m_hotbar = slots;
        for (auto& slot : m_hotbar) {
            if (m_unavailable_actions.count(slot)) {
                slot = -1;
                m_hotbar_dirty = true;
            }
        }
    }

}  // namespace openkal::client
