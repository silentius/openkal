/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/KnightSkill.h"
#include "openkal/protocol/MageSkill.h"
#include "openkal/protocol/Shortcut.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openkal::client {

    /**
     * Skill-parameter expression from jobsystem/JSON (`plus` / `mul` / …).
     * Used to fill ability tooltip placeholders like `%1!d!`.
     */
    class SkillExpr {
      public:
        /** Expression node kind. */
        enum class Kind : std::uint8_t {
            /** Missing / failed parse. */
            Null = 0,
            /** Numeric literal. */
            Number,
            /** Symbol such as skill_level or char_str. */
            Symbol,
            /** Operator call with child arguments. */
            Call,
        };

        /** @return Node kind. */
        [[nodiscard]] Kind kind() const {
            return m_kind;
        }

        /** @return Numeric literal when kind is Number. */
        [[nodiscard]] double number() const {
            return m_number;
        }

        /** @return Symbol name when kind is Symbol. */
        [[nodiscard]] const std::string& symbol() const {
            return m_symbol;
        }

        /** @return Operator name when kind is Call (plus/mul/div/minus). */
        [[nodiscard]] const std::string& op() const {
            return m_symbol;
        }

        /** @return Call arguments when kind is Call. */
        [[nodiscard]] const std::vector<SkillExpr>& args() const {
            return m_args;
        }

        /**
         * Builds a null expression.
         * @return Null expression.
         */
        [[nodiscard]] static SkillExpr make_null();

        /**
         * Builds a numeric literal.
         * @param v Number.
         * @return Number expression.
         */
        [[nodiscard]] static SkillExpr make_number(double v);

        /**
         * Builds a symbol reference.
         * @param name Symbol name.
         * @return Symbol expression.
         */
        [[nodiscard]] static SkillExpr make_symbol(std::string name);

        /**
         * Builds an operator call.
         * @param op Operator name.
         * @param args Child expressions.
         * @return Call expression.
         */
        [[nodiscard]] static SkillExpr make_call(std::string op, std::vector<SkillExpr> args);

      private:
        /** Node kind. */
        Kind m_kind = Kind::Null;

        /** Number value or unused. */
        double m_number = 0.0;

        /** Symbol name or call operator. */
        std::string m_symbol;

        /** Call children. */
        std::vector<SkillExpr> m_args;
    };

    /** Character stats used to fill ability tooltip placeholders. */
    struct SkillAbilityVars {
        /** True when `char_*` / `lightning_mastery` may resolve (else they stay "?"). */
        bool has_stats = false;

        /** Player level (`char_level`). */
        int char_level = 0;

        /** STR (`char_str`). */
        int char_str = 0;

        /** Health (`char_hth`). */
        int char_hth = 0;

        /** INT (`char_int`). */
        int char_int = 0;

        /** WIS (`char_wis`). */
        int char_wis = 0;

        /** DEX (`char_dex`). */
        int char_dex = 0;

        /** Learned Magic Mastery Lightning rank (`lightning_mastery`, skill 12). */
        int lightning_mastery = 0;

        /** Dodge before Shrewdness (`char_org_dodge`). */
        int char_org_dodge = 0;
    };

    /** One ability parameter (`parameter id expr`). */
    struct SkillParameter {
        /** Placeholder id (`%1!d!` → 1). */
        int id = 0;

        /** Expression tree. */
        SkillExpr expr;
    };

    /** One skill definition, parsed from the JSON catalog or from skill definition. */
    class SkillDef {
      public:
        /** @return Owning class (0 knight / 1 magician / 2 archer / 3 thief; negative
         * = shared). */
        [[nodiscard]] int clazz() const {
            return m_clazz;
        }

        /**
         * Sets the owning class.
         * @param v Class index.
         */
        void set_clazz(int v) {
            m_clazz = v;
        }

        /** @return Skill index within the class. */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /**
         * Sets the skill index.
         * @param v Skill index.
         */
        void set_index(int v) {
            m_index = v;
        }

        /** @return Highest learnable level. */
        [[nodiscard]] int max_level() const {
            return m_max_level;
        }

        /**
         * Sets the highest learnable level.
         * @param v Maximum level.
         */
        void set_max_level(int v) {
            m_max_level = v;
        }

        /** @return MP cost per cast. */
        [[nodiscard]] int mp() const {
            return m_mp;
        }

        /**
         * Sets the MP cost.
         * @param v MP cost.
         */
        void set_mp(int v) {
            m_mp = v;
        }

        /** @return Rage cost per cast. */
        [[nodiscard]] int rage() const {
            return m_rage;
        }

        /**
         * Sets the rage cost.
         * @param v Rage cost.
         */
        void set_rage(int v) {
            m_rage = v;
        }

        /** @return Minimum player level required to learn. */
        [[nodiscard]] int level_limit() const {
            return m_level_limit;
        }

        /**
         * Sets the minimum player level.
         * @param v Level limit.
         */
        void set_level_limit(int v) {
            m_level_limit = v;
        }

        /** @return Pre-cast delay in milliseconds. */
        [[nodiscard]] int delay_pre_ms() const {
            return m_delay_pre_ms;
        }

        /**
         * Sets the pre-cast delay.
         * @param v Delay in milliseconds.
         */
        void set_delay_pre_ms(int v) {
            m_delay_pre_ms = v;
        }

        /** @return Cooldown in milliseconds. */
        [[nodiscard]] int delay_ms() const {
            return m_delay_ms;
        }

        /**
         * Sets the cooldown.
         * @param v Delay in milliseconds.
         */
        void set_delay_ms(int v) {
            m_delay_ms = v;
        }

        /** Returns rank-specific cooldowns in milliseconds.
         * @return Values indexed by rank minus one, or empty when unevaluated.
         */
        const std::vector<int>& cooldown_by_level() const {
            return m_cooldown_by_level;
        }

        /** Sets evaluated cooldowns.
         * @param values Milliseconds indexed by rank minus one.
         */
        void set_cooldown_by_level(std::vector<int> values) {
            m_cooldown_by_level = std::move(values);
        }

        /** Returns rank-specific preparation times in milliseconds.
         * @return Values indexed by rank minus one, or empty when unevaluated.
         */
        const std::vector<int>& preskill_by_level() const {
            return m_preskill_by_level;
        }

        /** Sets evaluated preparation times.
         * @param values Milliseconds indexed by rank minus one.
         */
        void set_preskill_by_level(std::vector<int> values) {
            m_preskill_by_level = std::move(values);
        }

        /**
         * Jobsystem compare[3] cooldown in milliseconds when the expression is a
         * literal (compatibility behavior).
         * @return Cooldown milliseconds, or 0 when unused.
         */
        [[nodiscard]] int compare_cooldown_ms() const {
            return m_compare_cooldown_ms;
        }

        /**
         * Sets the compare[3] literal cooldown.
         * @param v Cooldown milliseconds.
         */
        void set_compare_cooldown_ms(int v) {
            m_compare_cooldown_ms = v;
        }

        /**
         * True when compare[3] includes jobsystem `weapon_speed` (compatibility behavior,
         * or plus(weapon_speed, N) for compatibility behavior).
         * @return True when UI cooldown follows weapon aspeed.
         */
        [[nodiscard]] bool compare_cooldown_weapon_speed() const {
            return m_compare_cooldown_weapon_speed;
        }

        /**
         * Sets the compare[3] weapon-speed flag.
         * @param v True when cooldown uses weapon aspeed.
         */
        void set_compare_cooldown_weapon_speed(bool v) {
            m_compare_cooldown_weapon_speed = v;
        }

        /**
         * Jobsystem compare[2] PRESKILL wait in milliseconds (`Send_Skill` delay1).
         * @return Precast milliseconds, or 0 when unused.
         */
        [[nodiscard]] int compare_preskill_ms() const {
            return m_compare_preskill_ms;
        }

        /**
         * Sets the compare[2] PRESKILL wait.
         * @param v Precast milliseconds.
         */
        void set_compare_preskill_ms(int v) {
            m_compare_preskill_ms = v;
        }

        /**
         * Jobsystem compare[1] cast range in server pixels when the expression is
         * a literal (Thunder Storm 190, Fire Storm 180).
         * @return Range pixels, or 0 when unused.
         */
        [[nodiscard]] int cast_range() const {
            return m_cast_range;
        }

        /**
         * Sets the compare[1] literal cast range.
         * @param v Range in server pixels.
         */
        void set_cast_range(int v) {
            m_cast_range = v;
        }

        /**
         * True when the jobsystem type includes `use` (OnAction combat/macro).
         * @return True for usable skills.
         */
        [[nodiscard]] bool usable() const {
            return m_usable;
        }

        /**
         * Sets the usable flag from the jobsystem type.
         * @param v True when type includes `use`.
         */
        void set_usable(bool v) {
            m_usable = v;
        }

        /**
         * True when the jobsystem type includes `once` (click-to-activate).
         * @return True for Meditation, Rest, storms, and similar.
         */
        [[nodiscard]] bool once() const {
            return m_once;
        }

        /**
         * Sets the once flag from the jobsystem type.
         * @param v True when type includes `once`.
         */
        void set_once(bool v) {
            m_once = v;
        }

        /**
         * True when jobsystem motion[2] is `attackauto` (compatibility behavior extra gubun).
         * Fire presentation uses motion[1] (`motion_fire`), not this flag.
         * @return True when motion[2] is attackauto.
         */
        [[nodiscard]] bool attack_auto() const {
            return m_attack_auto;
        }

        /**
         * Sets the attack-auto flag from jobsystem motion[2].
         * @param v True when motion[2] is `attackauto`.
         */
        void set_attack_auto(bool v) {
            m_attack_auto = v;
        }

        /**
         * GO_SKILL sub-index for fire (`ModelSkill` gubun 1 → `runtime state[1]`).
         * Lightning Slash uses 2 (`T*_7_A_02`); Mockery uses 3 (`T*_7_A_03`).
         * @return Motion sub, or 0 when unused.
         */
        [[nodiscard]] int motion_fire() const {
            return m_motion_fire;
        }

        /**
         * Sets the fire motion sub from jobsystem motion[1].
         * @param v GO_SKILL sub-index.
         */
        void set_motion_fire(int v) {
            m_motion_fire = v;
        }

        /**
         * GO_SKILL sub-index for prep (`runtime state[0]`).
         * @return Prep motion sub, or 0 when unused.
         */
        [[nodiscard]] int motion_prep() const {
            return m_motion_prep;
        }

        /**
         * Sets the prep motion sub from jobsystem motion[0].
         * @param v GO_SKILL sub-index.
         */
        void set_motion_prep(int v) {
            m_motion_prep = v;
        }

        /**
         * Effect stem for one JSON gubun slot (`effect[0..2]`).
         * @param gubun 0 prep / 1 fire / 2 extra.
         * @return Stem such as "Skill_R_2_1", or empty.
         */
        [[nodiscard]] const std::string& effect(int gubun) const;

        /**
         * Sets one gubun effect stem.
         * @param gubun 0 prep / 1 fire / 2 extra.
         * @param v Stem.
         */
        void set_effect(int gubun, std::string v);

        /** @return Jobsystem grade mask: 1 Common, 3 1st job, 7/11 2nd-job path,
         * 23/43 3rd-job path. */
        [[nodiscard]] int grade_mask() const {
            return m_grade_mask;
        }

        /**
         * Sets the grade mask.
         * @param v Grade mask.
         */
        void set_grade_mask(int v) {
            m_grade_mask = v;
        }

        /** @return Required prior skill index from skill definition (limit A B C D) → C; 0 =
         * none. */
        [[nodiscard]] int require_skill_index() const {
            return m_require_skill_index;
        }

        /**
         * Sets the required prior skill index.
         * @param v Skill index, or 0 for none.
         */
        void set_require_skill_index(int v) {
            m_require_skill_index = v;
        }

        /** @return Required prior skill level from skill definition (limit A B C D) → D. */
        [[nodiscard]] int require_skill_level() const {
            return m_require_skill_level;
        }

        /**
         * Sets the required prior skill level.
         * @param v Skill level.
         */
        void set_require_skill_level(int v) {
            m_require_skill_level = v;
        }

        /**
         * Character levels that grant successive auto-learn ranks.
         * @return Per-rank player levels, or empty when the default formula applies.
         */
        [[nodiscard]] const std::vector<int>& auto_learn_levels() const {
            return m_auto_learn_levels;
        }

        /**
         * Sets explicit auto-learn character levels per rank.
         * @param v Level required for rank i+1.
         */
        void set_auto_learn_levels(std::vector<int> v) {
            m_auto_learn_levels = std::move(v);
        }

        /** @return True when ranks are bought with SU in the skill window. */
        [[nodiscard]] bool manual_learn() const {
            return m_manual_learn;
        }

        /**
         * Sets the SU-learn flag from JSON `manualLearn`.
         * @param v True when auto-learn is disabled.
         */
        void set_manual_learn(bool v) {
            m_manual_learn = v;
        }

        /** @return True when the skill belongs on the Awaken tab. */
        [[nodiscard]] bool awaken() const {
            return m_awaken;
        }

        /**
         * Sets the Awaken-tab flag from JSON `awaken`.
         * @param v True when the skill is an Awaken skill.
         */
        void set_awaken(bool v) {
            m_awaken = v;
        }

        /** @return True for passive skills. */
        [[nodiscard]] bool passive() const {
            return m_passive;
        }

        /**
         * Sets the passive flag.
         * @param v True for a passive skill.
         */
        void set_passive(bool v) {
            m_passive = v;
        }

        /** @return Skill name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the skill name.
         * @param v Name.
         */
        void set_name(std::string v) {
            m_name = std::move(v);
        }

        /** @return Trailing skill definition comment. */
        [[nodiscard]] const std::string& comment() const {
            return m_comment;
        }

        /**
         * Sets the skill definition comment.
         * @param v Comment.
         */
        void set_comment(std::string v) {
            m_comment = std::move(v);
        }

        /** @return Raw description from jobsystem / skill definition (#n = newline). */
        [[nodiscard]] const std::string& desc() const {
            return m_desc;
        }

        /**
         * Sets the raw description.
         * @param v Description.
         */
        void set_desc(std::string v) {
            m_desc = std::move(v);
        }

        /** @return HyperText image stem (active006 / passive001). */
        [[nodiscard]] const std::string& image() const {
            return m_image;
        }

        /**
         * Sets the HyperText image stem.
         * @param v Image stem.
         */
        void set_image(std::string v) {
            m_image = std::move(v);
        }

        /** @return jobsystem role ("knight" / "mage" / "action" / ...). */
        [[nodiscard]] const std::string& role() const {
            return m_role;
        }

        /**
         * Sets the jobsystem role.
         * @param v Role.
         */
        void set_role(std::string v) {
            m_role = std::move(v);
        }

        /** @return jobsystem key ("mage-4" / "common-0"). */
        [[nodiscard]] const std::string& key() const {
            return m_key;
        }

        /**
         * Sets the jobsystem key.
         * @param v Key.
         */
        void set_key(std::string v) {
            m_key = std::move(v);
        }

        /**
         * Sound stem for one skill definition/JSON gubun slot (`runtime state[gubun]`).
         * @param gubun 0 prep / 1 fire / 2 extra.
         * @return Stem such as "efs011_L", or empty.
         */
        [[nodiscard]] const std::string& sound(int gubun) const;

        /**
         * Sets one gubun sound stem.
         * @param gubun 0 prep / 1 fire / 2 extra.
         * @param v Stem, possibly with `_L`.
         */
        void set_sound(int gubun, std::string v);

        /** @return Name, comment, or "#index" as a last resort. */
        [[nodiscard]] std::string display_name() const;

        /** @return HyperText stem only (active006 / Action001); resolve via
         * WidgetTools::resolve_bmp. */
        [[nodiscard]] const std::string& icon_stem() const {
            return m_image;
        }

        /** @return Plain multiline tooltip text (strips #n and simple markup). */
        [[nodiscard]] std::string tooltip_body() const;

        /** @return Raw ability template from jobsystem (`%1!d!` placeholders). */
        [[nodiscard]] const std::string& ability() const {
            return m_ability;
        }

        /**
         * Sets the ability template string.
         * @param v Ability text.
         */
        void set_ability(std::string v) {
            m_ability = std::move(v);
        }

        /** @return Ability parameters (id → expr). */
        [[nodiscard]] const std::vector<SkillParameter>& parameters() const {
            return m_parameters;
        }

        /**
         * Replaces the ability parameter list.
         * @param v Parameters.
         */
        void set_parameters(std::vector<SkillParameter> v) {
            m_parameters = std::move(v);
        }

        /**
         * Formats the ability template for a skill rank.
         * Unknown symbols become "?" in the substituted value.
         * @param skill_level Learned or preview rank (clamped to ≥ 1 for evaluation).
         * @return Multiline plain text, or empty when ability is blank.
         */
        [[nodiscard]] std::string format_ability(int skill_level) const;

        /**
         * Formats the ability template using character stats for `char_*` symbols.
         * @param skill_level Learned or preview rank (clamped to ≥ 1 for evaluation).
         * @param vars Character stats (`char_level`, `char_int`, …).
         * @return Multiline plain text, or empty when ability is blank.
         */
        [[nodiscard]] std::string format_ability(int skill_level,
                                                 const SkillAbilityVars& vars) const;

        /**
         * Evaluates one ability parameter for a skill rank.
         * @param param_id Parameter id (1-based).
         * @param skill_level Skill rank used as skill_level.
         * @return Integer value when fully resolvable; nullopt when dependent on
         *         character stats or unknown ops.
         */
        [[nodiscard]] std::optional<std::int64_t> eval_parameter(int param_id,
                                                                 int skill_level) const;

        /**
         * Evaluates one ability parameter using character stats.
         * @param param_id Parameter id (1-based).
         * @param skill_level Skill rank used as skill_level.
         * @param vars Character stats (`char_level`, `char_int`, …).
         * @return Integer value when fully resolvable; nullopt when a symbol is missing.
         */
        [[nodiscard]] std::optional<std::int64_t>
        eval_parameter(int param_id, int skill_level, const SkillAbilityVars& vars) const;

      private:
        /** Owning class; negative values mark shared/transform/monster entries. */
        int m_clazz = 0;

        /** Skill index within the class. */
        int m_index = 0;

        /** Highest learnable level. */
        int m_max_level = 1;

        /** MP cost per cast. */
        int m_mp = 0;

        /** Rage cost per cast. */
        int m_rage = 0;

        /** Minimum player level required to learn. */
        int m_level_limit = 0;

        /** Pre-cast delay in milliseconds. */
        int m_delay_pre_ms = 0;

        /** Cooldown in milliseconds. */
        int m_delay_ms = 0;

        /** Rank-specific compare timers; empty when the expression needs weapon speed. */
        std::vector<int> m_cooldown_by_level;
        std::vector<int> m_preskill_by_level;

        /** Literal compare[3] cooldown in milliseconds. */
        int m_compare_cooldown_ms = 0;

        /** True when compare[3] is `weapon_speed`. */
        bool m_compare_cooldown_weapon_speed = false;

        /** Literal compare[2] PRESKILL wait in milliseconds. */
        int m_compare_preskill_ms = 0;

        /** Literal compare[1] cast range in server pixels. */
        int m_cast_range = 0;

        /** True when type includes `use`. */
        bool m_usable = false;

        /** True when type includes `once`. */
        bool m_once = false;

        /** True when motion[2] is `attackauto`. */
        bool m_attack_auto = false;

        /** GO_SKILL sub for fire (`runtime state[1]`). */
        int m_motion_fire = 0;

        /** GO_SKILL sub for prep (`runtime state[0]`). */
        int m_motion_prep = 0;

        /** jobsystem grade mask (grade N …). */
        int m_grade_mask = 0;

        /** Required prior skill index (0 = none). */
        int m_require_skill_index = 0;

        /** Required prior skill level. */
        int m_require_skill_level = 0;

        /** Character levels that grant successive auto-learn ranks. */
        std::vector<int> m_auto_learn_levels;

        /** True when ranks are bought with SU instead of auto-learned. */
        bool m_manual_learn = false;

        /** True when the skill is shown on the Awaken tab. */
        bool m_awaken = false;

        /** True for passive skills. */
        bool m_passive = false;

        /** Skill name. */
        std::string m_name;

        /** Trailing skill definition comment. */
        std::string m_comment;

        /** Raw description (#n = newline). */
        std::string m_desc;

        /** Ability template with %N!d! placeholders. */
        std::string m_ability;

        /** Ability parameter expressions. */
        std::vector<SkillParameter> m_parameters;

        /** HyperText image stem. */
        std::string m_image;

        /** jobsystem role. */
        std::string m_role;

        /** jobsystem key. */
        std::string m_key;

        /** skill definition/JSON sound stems for gubun 0..2. */
        std::array<std::string, 3> m_sounds;

        /** jobsystem/JSON effect stems for gubun 0..2. */
        std::array<std::string, 3> m_effects;
    };

    /** How the client should act on a cast request. */
    enum class CastKind : std::uint8_t {
        /** Not castable. */
        None = 0,

        /** Offensive skill aimed at the selected target. */
        Combat,

        /** Self-targeted heal. */
        SelfHeal,

        /** Beneficial skill aimed at one living friendly player. */
        Player,

        /** Resurrection skill aimed at one dead friendly player. */
        DeadPlayer,

        /** Server-selected nearby party heal. */
        Party,

        /** Hostile area centered on the caster. */
        SelfArea,

        /** Hostile area centered on a selected monster. */
        TargetArea,

        /** Hostile area centered on supplied world coordinates. */
        GroundPosition,

        /** Haste toggle (skill 0 / compatibility behavior). */
        Boost,

        /** Sit toggle (skill 11 / C2S_REST). */
        Rest,

        /** Maguni behead (skill 1 / compatibility behavior). */
        Behead,

        /** One-shot human action-10 animation. */
        Emote,
    };

    /** Shared player interaction action. Party and Trade use the active skill slot. */
    enum class SocialAction : std::uint8_t { None, Guild, Party, Trade, Whisper, Removed };

    /** Classifies shared action labels from the supplied catalog.
     * @param skill Catalog definition. @return Social interaction or None. */
    [[nodiscard]] SocialAction social_action(const SkillDef& skill);

    /** Tests whether a player interaction uses the active mouse skill slot.
     * @param action Shared player interaction. @return True for Party and Trade. */
    [[nodiscard]] bool social_action_uses_active_slot(SocialAction action);

    /** Skill catalog loaded from the JSON tree. */
    class Catalog {
      public:
        /**
         * Loads the catalog from the JSON directory.
         * @param skills_dir JSON skills directory.
         * @return True if at least one skill was loaded.
         */
        [[nodiscard]] bool load(const std::string& skills_dir);

        /**
         * Loads every .json skill below a directory.
         * @param dir Skills directory.
         * @return True if at least one skill was loaded.
         */
        [[nodiscard]] bool load_json_dir(const std::string& dir);

        /** @return Every loaded definition. */
        [[nodiscard]] const std::vector<SkillDef>& all() const {
            return m_all;
        }

        /**
         * Collects the skills a class can see, including shared actions.
         * @param clazz Class index.
         * @return Pointers into the catalog; invalidated by a reload.
         */
        [[nodiscard]] std::vector<const SkillDef*> for_class(int clazz) const;

        /**
         * Looks up a skill, preferring the class-specific definition over shared
         * actions.
         * @param clazz Class index.
         * @param index Skill index.
         * @return Definition pointer, or nullptr.
         */
        [[nodiscard]] const SkillDef* find(int clazz, int index) const;

        /**
         * Looks up a skill by index across every class (animal / transform-into).
         * @param index Skill index.
         * @return First matching definition, or nullptr.
         */
        [[nodiscard]] const SkillDef* find_any(int index) const;

        /**
         * HyperText bmp for a skill compatibility behavior bit, from JSON `image` at catalog load.
         * @param bstate compatibility behavior bitmask (single bit or combined).
         * @return Filename such as "active096.bmp", or empty when unknown.
         */
        [[nodiscard]] std::string_view buff_icon_for(std::uint64_t bstate) const;

        /**
         * Display name for a skill compatibility behavior bit, from JSON `name` at catalog load.
         * @param bstate compatibility behavior bitmask (single bit or combined).
         * @return Skill name, or empty when unknown.
         */
        [[nodiscard]] std::string_view buff_name_for(std::uint64_t bstate) const;

        /**
         * HUD icon for a specific skill index (Defensive Incantation tiers).
         * @param clazz Class index.
         * @param index Skill index.
         * @return BMP filename, or empty.
         */
        [[nodiscard]] std::string buff_icon_for_skill(int clazz, int index) const;

        /**
         * Display name for a specific skill index.
         * @param clazz Class index.
         * @param index Skill index.
         * @return Skill name, or empty.
         */
        [[nodiscard]] std::string_view buff_name_for_skill(int clazz, int index) const;

        /**
         * Tests whether a skill is a universal Action-tab macro (trade / pick /
         * emotes).
         * @param s Skill definition.
         * @return True for shared action entries.
         */
        [[nodiscard]] static bool is_shared_action(const SkillDef& s);

        /**
         * Tests whether a skill can be cast from the bar.
         * @param s Skill definition.
         * @return True when castable.
         */
        [[nodiscard]] static bool is_castable(const SkillDef& s);

        /**
         * True when a skill-bar LMB should set the active combat skill
         * (existing slot click selects, then RMB / F fires on a target).
         * @param kind Catalog cast kind for the bound skill.
         * @return True for Combat, Behead, and TargetArea.
         */
        [[nodiscard]] static bool skill_bar_sets_active(CastKind kind);

        /**
         * True when icon / hotkey press should enter RangeSkillCast aiming
         * (Mage storms / Explosive Burst; Archer Poison Cloud / Arrow Rain).
         * @param clazz Job class.
         * @param skill_index Skill index.
         * @return True when the skill uses the ground-targeting cue.
         */
        [[nodiscard]] static bool uses_range_skill_selector(int clazz, int skill_index);

        /**
         * True when a bar / hotkey press should fire immediately instead of
         * occupying the mouse-action slot (reference type `once` / SendSkill).
         * Focus Shot starts charging; Mage storms and Archer Arrow Rain /
         * Poison Cloud enter ground aiming; Splashy Ice casts around the caster.
         * Shinsu Baby/Young/Adult (200–202) transform immediately.
         * Emotes and Meditation cast without replacing the mouse-action slot.
         * @param clazz Job class (1 = Mage, 2 = Archer).
         * @param skill_index Skill index.
         * @return True when the press should cast immediately.
         */
        [[nodiscard]] static bool skill_bar_casts_on_click(int clazz, int skill_index);

        /**
         * Attack range of a magician combat skill.
         * @param skill_index Skill index.
         * @param skill_level Learned level.
         * @return Range in server pixels.
         */
        [[nodiscard]] static int magic_skill_range(int skill_index, int skill_level = 1);

        /**
         * Tests whether a skill index is a magician offensive spell.
         * Auto-recast still requires jobsystem motion[2] `attackauto`.
         * Ground skills (storms, Explosive Burst) are not combat skills.
         * @param skill_index Skill index.
         * @return True for magician combat spells, including Hermit one-shots.
         */
        [[nodiscard]] static bool is_mage_combat_skill(int skill_index);

        /**
         * Tests whether an archer skill fires a projectile (compatibility behavior / skill arrow).
         * @param skill_index Skill index within class 2.
         * @return True for Shot, flame/stun/poison/mystic arrows, and similar.
         */
        [[nodiscard]] static bool is_archer_combat_skill(int skill_index);

        /**
         * Tests whether a knight skill is a compatibility behavior targeted attack (compatibility
         * behavior …).
         * @param skill_index Skill index within class 0.
         * @return True for Lightning Slash and the other melee blows.
         */
        [[nodiscard]] static bool is_knight_combat_skill(int skill_index);

        /**
         * Tests whether a thief skill is a targeted combat blow.
         * @param skill_index Skill index within class 3.
         * @return True for Assault, Strangle, and similar use-skills.
         */
        [[nodiscard]] static bool is_thief_combat_skill(int skill_index);

        /**
         * Tests whether RMB/auto-attack should recast a melee combat skill.
         * @param clazz Class index (0 knight / 3 thief).
         * @param skill_index Skill index.
         * @return True for knight/thief compatibility behavior-style attacks.
         */
        [[nodiscard]] static bool is_melee_combat_skill(int clazz, int skill_index);

        /**
         * Tests whether a skill is related compatibility states (hostility only, no damage).
         * @param clazz Class index.
         * @param skill_index Skill index.
         * @return True for Knight Mockery and Provocation.
         */
        [[nodiscard]] static bool is_taunt_skill(int clazz, int skill_index);

        /**
         * Client range used before sending C2S_SKILL.
         * @param clazz Class index.
         * @param def Skill definition; may be null.
         * @param skill_level Learned rank.
         * @param weapon_range Equipped weapon range.
         * @return Range in world pixels, including a small slack.
         */
        [[nodiscard]] static int
        skill_cast_range(int clazz, const SkillDef* def, int skill_level, int weapon_range);

        /**
         * Icon cooldown for this skill (compatibility behavior, runtime state[3]).
         * @param def Skill definition.
         * @param weapon_aspeed_ms Equipped weapon aspeed in milliseconds.
         * @param skill_level Learned rank.
         * @return Cooldown milliseconds; 0 when the icon should not dim.
         */
        [[nodiscard]] static int
        ui_cooldown_ms(const SkillDef& def, int weapon_aspeed_ms, int skill_level = 1);

        /**
         * Client PRESKILL wait (`Send_Skill` delay1 = runtime state[2], JSON compare id 3).
         * Falls back to skill definition delay[0] when compare[2] is absent.
         * @param def Skill definition.
         * @param skill_level Learned rank.
         * @param fire_mastery Learned Fire Mastery rank.
         * @return Precast milliseconds.
         */
        [[nodiscard]] static int
        preskill_ms(const SkillDef& def, int skill_level = 1, int fire_mastery = 0);

        /**
         * MP spent on a successful cast (compatibility behavior).
         * @param clazz Class index.
         * @param def Skill definition; may be null.
         * @param skill_level Learned rank.
         * @param character_level Player level, used by level-dependent costs.
         * @return MP cost, or 0.
         */
        [[nodiscard]] static int
        skill_spend_mp(int clazz, const SkillDef* def, int skill_level, int character_level = 1);

        /**
         * Classifies how a cast request should be handled.
         * @param clazz Class index.
         * @param skill_index Skill index.
         * @return Cast handling kind.
         */
        [[nodiscard]] static CastKind cast_kind_for(int clazz, int skill_index);

      private:
        /** HUD payload for one skill compatibility behavior bit. */
        struct BuffHudEntry {
            /** HyperText bmp filename. */
            std::string icon;

            /** Skill JSON display name. */
            std::string name;
        };

        /** Fills runtime state from loaded skill JSON images and names. */
        void rebuild_buff_icon_table();

        /**
         * Looks up HUD data for a compatibility behavior bit.
         * @param bstate compatibility behavior bitmask (prefers an exact key, then the lowest set
         * bit).
         * @return Entry pointer, or nullptr.
         */
        [[nodiscard]] const BuffHudEntry* buff_hud_entry(std::uint64_t bstate) const;

        /** Every loaded definition. */
        std::vector<SkillDef> m_all;

        /** compatibility behavior bit → HUD icon and name. */
        std::unordered_map<std::uint64_t, BuffHudEntry> m_buff_icons;
    };

    /** Client-side learned skills and hotbar. Combat ranks come from the server. */
    class PlayerSkills {
      public:
        /** Number of hotbar slots. */
        static constexpr std::size_t kHotbarSlots = protocol::kShortcutSlots;

        /** Forgets every skill and clears the bar. */
        void clear();

        /**
         * Seeds shared Action-tab macros (trade / pick / emotes). Combat skills are
         * applied from S2C_SKILLINFO / S2C_SKILLUP.
         * @param clazz Class index.
         * @param player_level Player level (unused; kept for call-site compatibility).
         * @param catalog Skill catalog.
         */
        void seed_for_class(int clazz, int player_level, const Catalog& catalog);

        /** Sets the session mode before seeding or restoring shortcuts.
         * @param enabled Whether multiplayer interactions are available. */
        void set_multiplayer(bool enabled) {
            m_multiplayer = enabled;
        }

        /** @return Skill bound to the top "mouse" slot, or -1. */
        [[nodiscard]] int active_index() const {
            return m_active_index;
        }

        /**
         * Binds the top "mouse" slot.
         * @param skill_index Skill index, or -1 to clear.
         */
        void set_active(int skill_index);

        /** @return Learned skills as index → level. */
        [[nodiscard]] const std::unordered_map<int, int>& learned() const {
            return m_learned;
        }

        /**
         * Reads one hotbar slot.
         * @param slot Slot 0..19.
         * @return Bound skill index, packed item binding, or -1 when empty or out of range.
         */
        [[nodiscard]] int hotbar_slot(int slot) const;

        /**
         * Binds one hotbar slot.
         * @param slot Slot 0..19; out-of-range values are ignored.
         * @param skill_index Skill index or packed item binding; negative values clear the slot.
         */
        void set_hotbar(int slot, int skill_index);

        /**
         * Reads the learned level of a skill.
         * @param skill_index Skill index.
         * @return Learned level, or 0.
         */
        [[nodiscard]] int level_of(int skill_index) const;

        /**
         * Tests whether a skill is learned.
         * @param skill_index Skill index.
         * @return True when learned at level 1 or above.
         */
        [[nodiscard]] bool is_learned(int skill_index) const {
            return level_of(skill_index) > 0;
        }

        /**
         * Sets the learned level of one skill (0 forgets it).
         * @param skill_index Skill index.
         * @param level Learned level.
         * @return Previous learned level.
         */
        int set_learned_level(int skill_index, int level);

        /**
         * Replaces combat learned ranks from S2C_SKILLINFO and restores shared
         * Action macros.
         * @param from_server Server ranks (index → level).
         * @param catalog Skill catalog.
         * @param clazz Class index.
         */
        void apply_server_info(const std::unordered_map<int, int>& from_server,
                               const Catalog& catalog,
                               int clazz);

        /**
         * Binds default learned combat skills when the bar is empty. A new Mage
         * receives Beheading in slot 0 and Lightning Magic in slot 1, selected as
         * the active combat skill.
         * @param catalog Skill catalog.
         * @param clazz Class index.
         */
        void ensure_default_hotbar(const Catalog& catalog, int clazz);

        /**
         * Binds the skill in a hotbar slot as active when the slot holds a skill.
         * Item shortcuts are left as the current mouse skill.
         * @param slot Slot 0..19.
         */
        void activate_hotbar_slot(int slot);

        /**
         * Restores the bar from S2C_SHORTCUT without marking it dirty.
         * @param active_index Top mouse-slot skill, or -1.
         * @param slots Hotbar bindings; -1 marks an empty slot.
         */
        void apply_shortcut(int active_index, const std::array<int, kHotbarSlots>& slots);

        /** @return True when the bar changed locally and should be sent to the server. */
        [[nodiscard]] bool hotbar_dirty() const {
            return m_hotbar_dirty;
        }

        /** Clears the pending C2S_SHORTCUT flag after a successful send. */
        void clear_hotbar_dirty() {
            m_hotbar_dirty = false;
        }

      private:
        /**
         * Marks every shared Action-tab macro as learned.
         * @param catalog Skill catalog.
         * @param clazz Class index.
         */
        void seed_shared_actions(const Catalog& catalog, int clazz);

        /** Learned skills as index → level (0 = absent). */
        std::unordered_map<int, int> m_learned;

        /** Actions barred from the mouse slot and from this session's shortcuts. */
        std::unordered_set<int> m_non_active, m_unavailable_actions;
        bool m_multiplayer = false;

        /** Skill bound to the top "mouse" slot, or -1. */
        int m_active_index = -1;

        /** Hotbar bindings; -1 marks an empty slot. */
        std::array<int, kHotbarSlots> m_hotbar{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

        /** True when the local bar should be persisted via C2S_SHORTCUT. */
        bool m_hotbar_dirty = false;
    };

}  // namespace openkal::client
