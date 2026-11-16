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

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** One converted `(shinsuinfo)` row (key = stage*10 + race). */
    class ShinsuInfo {
      public:
        /** @return Packed key. */
        [[nodiscard]] int key() const {
            return m_key;
        }

        /**
         * Sets the packed key.
         * @param v Key.
         */
        void set_key(int v) {
            m_key = v;
        }

        /** @return item definition race. */
        [[nodiscard]] int race() const {
            return m_race;
        }

        /**
         * Sets the race.
         * @param v Race.
         */
        void set_race(int v) {
            m_race = v;
        }

        /** @return Stage 1–4. */
        [[nodiscard]] int stage() const {
            return m_stage;
        }

        /**
         * Sets the stage.
         * @param v Stage.
         */
        void set_stage(int v) {
            m_stage = v;
        }

        /** @return Bone table key. */
        [[nodiscard]] int bone() const {
            return m_bone;
        }

        /**
         * Sets the bone key.
         * @param v Bone key.
         */
        void set_bone(int v) {
            m_bone = v;
        }

        /** @return Model scale (macro.dat first number). */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the model scale.
         * @param v Scale.
         */
        void set_scale(float v) {
            m_scale = v;
        }

        /** @return Animation mspeed pair from shinsuinfo. */
        [[nodiscard]] const std::vector<int>& mspeed() const {
            return m_mspeed;
        }

        /**
         * Sets the mspeed pair.
         * @param v Values.
         */
        void set_mspeed(std::vector<int> v) {
            m_mspeed = std::move(v);
        }

        /** Attached particle from shinsuinfo. */
        class Particle {
          public:
            /** @return Particle shader name. */
            [[nodiscard]] const std::string& name() const {
                return m_name;
            }

            /**
             * Sets the particle name.
             * @param v Name.
             */
            void set_name(std::string v) {
                m_name = std::move(v);
            }

            /** @return Effect bone index. */
            [[nodiscard]] int bone() const {
                return m_bone;
            }

            /**
             * Sets the effect bone.
             * @param v Bone.
             */
            void set_bone(int v) {
                m_bone = v;
            }

            /** @return World-axis offset X from shinsuinfo. */
            [[nodiscard]] float ox() const {
                return m_ox;
            }

            /**
             * Sets the world-axis offset X.
             * @param v Offset X.
             */
            void set_ox(float v) {
                m_ox = v;
            }

            /** @return World-axis offset Y from shinsuinfo. */
            [[nodiscard]] float oy() const {
                return m_oy;
            }

            /**
             * Sets the world-axis offset Y.
             * @param v Offset Y.
             */
            void set_oy(float v) {
                m_oy = v;
            }

            /** @return World-axis offset Z from shinsuinfo. */
            [[nodiscard]] float oz() const {
                return m_oz;
            }

            /**
             * Sets the world-axis offset Z.
             * @param v Offset Z.
             */
            void set_oz(float v) {
                m_oz = v;
            }

          private:
            /** Particle shader name. */
            std::string m_name;

            /** Effect bone index. */
            int m_bone = 0;

            /** World-axis offset X. */
            float m_ox = 0.0f;

            /** World-axis offset Y. */
            float m_oy = 0.0f;

            /** World-axis offset Z. */
            float m_oz = 0.0f;
        };

        /** @return Attached particles. */
        [[nodiscard]] const std::vector<Particle>& particles() const {
            return m_particles;
        }

        /**
         * Sets attached particles.
         * @param v Particles.
         */
        void set_particles(std::vector<Particle> v) {
            m_particles = std::move(v);
        }

      private:
        /** Packed key (stage*10 + race). */
        int m_key = 0;

        /** item definition race. */
        int m_race = 0;

        /** Stage 1–4. */
        int m_stage = 0;

        /** Bone table key. */
        int m_bone = 0;

        /** Model scale. */
        float m_scale = 1.0f;

        /** Animation mspeed pair. */
        std::vector<int> m_mspeed;

        /** Attached particles. */
        std::vector<Particle> m_particles;
    };

    /** Converted external asset pack shinsuname / shinsuinfo / shinsubone tables. */
    class ShinsuDb {
      public:
        /**
         * Loads JSON from a directory (`names.json`, `info.json`, `bones.json`).
         * @param dir config/shinsu path.
         * @return True when at least names or info loaded.
         */
        [[nodiscard]] bool load_dir(const std::string& dir);

        /**
         * Stage display name for a race (H-window).
         * @param race item definition race.
         * @param stage 1–3.
         * @return Localized name, or a fallback.
         */
        [[nodiscard]] std::string stage_name(int race, int stage) const;

        /**
         * Looks up shinsuinfo by packed key.
         * @param key stage*10 + race.
         * @return Info, or nullptr.
         */
        [[nodiscard]] const ShinsuInfo* find(int key) const;

        /**
         * Looks up shinsuinfo by race and stage.
         * @param race item definition race.
         * @param stage 1–3.
         * @return Info, or nullptr.
         */
        [[nodiscard]] const ShinsuInfo* find(int race, int stage) const;

        /**
         * Maps an EFFECTBONE slot through `shinsubone` to a skeleton index.
         * @param bone_table_key shinsuinfo `bone` / shinsubone `key`.
         * @param effect_slot EFFECTBONE index (0..7).
         * @return Skeleton bone, or -1 when the table or slot is unknown.
         */
        [[nodiscard]] int skeleton_bone(int bone_table_key, int effect_slot) const;

      private:
        /** Names keyed race -> stage -> string. */
        std::unordered_map<int, std::unordered_map<int, std::string>> m_stage_names;

        /** Info keyed by packed key. */
        std::unordered_map<int, ShinsuInfo> m_info;

        /** Skeleton indices keyed by shinsubone key, then EFFECTBONE slot. */
        std::unordered_map<int, std::array<int, 8>> m_bones;
    };

}  // namespace openkal::client
