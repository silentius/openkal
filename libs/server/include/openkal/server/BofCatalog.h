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

#include "openkal/platform/Json.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openkal::server {

    /** Outcome of dropping a Bead of Fire onto gear. */
    enum class BofApplyResult {
        /** Target may receive BOF. */
        Ok = 0,

        /** Wrong item class, shield, or below min grade. */
        InvalidTarget = 1,

        /** Instance already has BOF. */
        AlreadyApplied = 2,

        /** Destination is currently worn. */
        Equipped = 3,
    };

    /** One grade row from config/bof.json `tiers`. */
    class BofTier {
      public:
        /** @return Armor grade this row describes. */
        [[nodiscard]] int grade() const {
            return m_grade;
        }

        /**
         * Sets the armor grade.
         * @param v Grade.
         */
        void set_grade(int v) {
            m_grade = v;
        }

        /** @return Primary points granted per BOF piece. */
        [[nodiscard]] int piece() const {
            return m_piece;
        }

        /**
         * Sets per-piece primary points.
         * @param v Points added to STR, HTH, and INT or DEX.
         */
        void set_piece(int v) {
            m_piece = v;
        }

        /** @return Extra primaries for a half set (3–4 same-grade pieces). */
        [[nodiscard]] int half_set() const {
            return m_half_set;
        }

        /**
         * Sets the half-set primary bonus.
         * @param v Points.
         */
        void set_half_set(int v) {
            m_half_set = v;
        }

        /** @return Extra primaries for a full set (5 same-grade pieces). */
        [[nodiscard]] int full_set() const {
            return m_full_set;
        }

        /**
         * Sets the full-set primary bonus.
         * @param v Points.
         */
        void set_full_set(int v) {
            m_full_set = v;
        }

      private:
        /** Armor grade. */
        int m_grade = 0;

        /** Per-piece primary points. */
        int m_piece = 0;

        /** Half-set primary points. */
        int m_half_set = 0;

        /** Full-set primary points. */
        int m_full_set = 0;
    };

    /** One equipped BOF armor piece used for set evaluation. */
    class BofWornPiece {
      public:
        /** @return Item grade. */
        [[nodiscard]] int grade() const {
            return m_grade;
        }

        /**
         * Sets the item grade.
         * @param v Grade.
         */
        void set_grade(int v) {
            m_grade = v;
        }

      private:
        /** Armor grade of the worn BOF piece. */
        int m_grade = 0;
    };

    /** Half/full set result for a list of BOF pieces. */
    class BofSetEval {
      public:
        /** @return Grade of the largest same-grade group, or 0. */
        [[nodiscard]] int grade() const {
            return m_grade;
        }

        /**
         * Sets the winning set grade.
         * @param v Grade.
         */
        void set_grade(int v) {
            m_grade = v;
        }

        /** @return Size of the largest same-grade group. */
        [[nodiscard]] int count() const {
            return m_count;
        }

        /**
         * Sets the winning group size.
         * @param v Piece count.
         */
        void set_count(int v) {
            m_count = v;
        }

        /**
         * Set completeness.
         * @return 0 = none, 1 = half, 2 = full.
         */
        [[nodiscard]] int completeness() const {
            return m_completeness;
        }

        /**
         * Sets completeness.
         * @param v 0, 1, or 2.
         */
        void set_completeness(int v) {
            m_completeness = v;
        }

      private:
        /** Winning same-grade group. */
        int m_grade = 0;

        /** Pieces in that group. */
        int m_count = 0;

        /** 0 none, 1 half, 2 full. */
        int m_completeness = 0;
    };

    /** Primary and percent bonuses from worn BOF pieces. */
    class BofCombatBonus {
      public:
        /** @return Strength added. */
        [[nodiscard]] int str() const {
            return m_str;
        }

        /**
         * Sets strength.
         * @param v Points.
         */
        void set_str(int v) {
            m_str = v;
        }

        /** @return Health added. */
        [[nodiscard]] int hth() const {
            return m_hth;
        }

        /**
         * Sets health.
         * @param v Points.
         */
        void set_hth(int v) {
            m_hth = v;
        }

        /** @return Intelligence added (mage only). */
        [[nodiscard]] int intl() const {
            return m_intl;
        }

        /**
         * Sets intelligence.
         * @param v Points.
         */
        void set_intl(int v) {
            m_intl = v;
        }

        /** @return Dexterity added (non-mage). */
        [[nodiscard]] int dex() const {
            return m_dex;
        }

        /**
         * Sets dexterity.
         * @param v Points.
         */
        void set_dex(int v) {
            m_dex = v;
        }

        /** @return Defense percent for half or full set, or 0. */
        [[nodiscard]] int defense_percent() const {
            return m_defense_percent;
        }

        /**
         * Sets the defense percent bonus.
         * @param v Percent.
         */
        void set_defense_percent(int v) {
            m_defense_percent = v;
        }

        /** @return HP/MP percent for a full set, or 0. */
        [[nodiscard]] int hp_mp_percent() const {
            return m_hp_mp_percent;
        }

        /**
         * Sets the HP/MP percent bonus.
         * @param v Percent.
         */
        void set_hp_mp_percent(int v) {
            m_hp_mp_percent = v;
        }

        /** @return Resist percent for a full set, or 0. */
        [[nodiscard]] int resist_percent() const {
            return m_resist_percent;
        }

        /**
         * Sets the resist percent bonus.
         * @param v Percent.
         */
        void set_resist_percent(int v) {
            m_resist_percent = v;
        }

        /** @return 0 none, 1 half, 2 full. */
        [[nodiscard]] int completeness() const {
            return m_completeness;
        }

        /**
         * Sets completeness.
         * @param v 0, 1, or 2.
         */
        void set_completeness(int v) {
            m_completeness = v;
        }

        /** @return Grade used for the set bonus, or 0. */
        [[nodiscard]] int set_grade() const {
            return m_set_grade;
        }

        /**
         * Sets the set grade.
         * @param v Grade.
         */
        void set_set_grade(int v) {
            m_set_grade = v;
        }

        /** @return FX shader level (snapped), or 0. */
        [[nodiscard]] int fx_level() const {
            return m_fx_level;
        }

        /**
         * Sets the FX level.
         * @param v Level.
         */
        void set_fx_level(int v) {
            m_fx_level = v;
        }

      private:
        /** Strength points. */
        int m_str = 0;

        /** Health points. */
        int m_hth = 0;

        /** Intelligence points. */
        int m_intl = 0;

        /** Dexterity points. */
        int m_dex = 0;

        /** Defense percent. */
        int m_defense_percent = 0;

        /** HP and MP percent. */
        int m_hp_mp_percent = 0;

        /** Elemental and non-elemental resist percent. */
        int m_resist_percent = 0;

        /** Set completeness. */
        int m_completeness = 0;

        /** Winning set grade. */
        int m_set_grade = 0;

        /** Snapped FX level. */
        int m_fx_level = 0;
    };

    /**
     * Beads of Fire config: per-grade piece/set stats, armor parts, and FX snap.
     */
    class BofCatalog {
      public:
        /**
         * Loads config/bof.json from a file path.
         * @param path Absolute or repo-relative JSON path.
         * @return True when the file parsed and at least minGrade is set.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /**
         * Parses a bof.json body.
         * @param text JSON text.
         * @return True when the document is accepted.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** @return True after a successful load. */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

        /** @return Minimum armor grade that may receive BOF. */
        [[nodiscard]] int min_grade() const {
            return m_min_grade;
        }

        /** @return Pieces required for half-set bonuses. */
        [[nodiscard]] int half_count() const {
            return m_half_count;
        }

        /** @return Pieces required for full-set bonuses. */
        [[nodiscard]] int full_count() const {
            return m_full_count;
        }

        /** @return Success percent 1–100 when applying a bead. */
        [[nodiscard]] int success_rate() const {
            return m_success_rate;
        }

        /** @return Defense percent granted at half or full set. */
        [[nodiscard]] int half_defense_percent() const {
            return m_half_defense_percent;
        }

        /** @return HP/MP percent granted at full set. */
        [[nodiscard]] int full_hp_mp_percent() const {
            return m_full_hp_mp_percent;
        }

        /** @return Resist percent granted at full set. */
        [[nodiscard]] int full_resist_percent() const {
            return m_full_resist_percent;
        }

        /**
         * True when class_small is a BOF armor slot (not shield/face).
         * @param class_small item definition class token.
         * @return True when BOF may be applied.
         */
        [[nodiscard]] bool is_bof_armor_part(std::string_view class_small) const;

        /**
         * Looks up or synthesizes the tier for an armor grade.
         * @param grade Item grade.
         * @return Piece / half / full values (zeros when grade is below min).
         */
        [[nodiscard]] BofTier tier_for_grade(int grade) const;

        /**
         * Snaps a grade to a shipped `ef_setitem` level.
         * @param grade Armor grade.
         * @return Nearest fxLevels entry that is <= grade, or 0.
         */
        [[nodiscard]] int fx_level_for_grade(int grade) const;

        /**
         * Mage uses INT as the third BOF primary; every other class uses DEX.
         * @param clazz Player class 0..3.
         * @return True when the third stat is intelligence.
         */
        [[nodiscard]] static bool third_stat_is_intel(int clazz);

        /**
         * Largest same-grade BOF group and half/full completeness.
         * @param pieces Worn BOF piece grades.
         * @return Set evaluation.
         */
        [[nodiscard]] BofSetEval evaluate_set(const std::vector<BofWornPiece>& pieces) const;

        /**
         * Piece + set combat bonuses for a class.
         * @param clazz Player class 0..3.
         * @param pieces Worn BOF pieces.
         * @return Aggregated bonuses.
         */
        [[nodiscard]] BofCombatBonus compute_bonus(int clazz,
                                                   const std::vector<BofWornPiece>& pieces) const;

        /**
         * Validates dropping a Bead of Fire onto a destination item.
         * @param is_defense True when the destination is defense gear.
         * @param class_small Destination class_small.
         * @param grade Destination grade.
         * @param equipped True when the destination is worn.
         * @param already_bof True when the instance already has BOF.
         * @return Apply outcome.
         */
        [[nodiscard]] BofApplyResult can_apply(bool is_defense,
                                               std::string_view class_small,
                                               int grade,
                                               bool equipped,
                                               bool already_bof) const;

        /**
         * True when a 1..100 roll is a successful BOF apply.
         * @param roll Inclusive 1..100.
         * @return True on success.
         */
        [[nodiscard]] bool roll_success(int roll) const;

      private:
        /**
         * Merges a parsed JSON object into this catalog.
         * @param root Root object.
         * @return True when minGrade is positive.
         */
        [[nodiscard]] bool merge_root(const platform::Json& root);

        /**
         * Fallback piece/half/full when the grade is not listed.
         * @param grade Armor grade.
         * @return Synthesized tier.
         */
        [[nodiscard]] BofTier fallback_tier(int grade) const;

        /** True after a successful load. */
        bool m_ready = false;

        /** Minimum apply grade. */
        int m_min_grade = 46;

        /** Half-set piece count. */
        int m_half_count = 3;

        /** Full-set piece count. */
        int m_full_count = 5;

        /** Apply success percent. */
        int m_success_rate = 100;

        /** Half/full defense percent. */
        int m_half_defense_percent = 3;

        /** Full-set HP/MP percent. */
        int m_full_hp_mp_percent = 10;

        /** Full-set resist percent. */
        int m_full_resist_percent = 10;

        /** Allowed class_small tokens. */
        std::unordered_set<std::string> m_armor_parts;

        /** Explicit tiers keyed by grade. */
        std::unordered_map<int, BofTier> m_tiers;

        /** Shipped FX levels, ascending. */
        std::vector<int> m_fx_levels;
    };

}  // namespace openkal::server
