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

#include "openkal/engine/engine.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /**
     * Looping sprite / FX attachments loaded from Data/Effect/Sprite and
     * Data/Effect/Fx (compatibility behavior + compatibility behavior).
     */
    class SpriteFx {
      public:
        /** `distribution` token (`prtShaderStage_t`). */
        enum class SpriteDistribution : std::uint8_t {
            /** Axis-aligned half-extents (`box`). */
            Box = 0,
            /** Center XYZ plus radius W (`sphere`). */
            Sphere,
            /** XZ ellipse plus Y height (`cylinder`). */
            Cylinder,
        };

        /** One FX `model` stage (stem, offset, delay, duration). */
        class FxModelSpawn {
          public:
            /** @return Effect/Skill GB stem. */
            [[nodiscard]] const std::string& stem() const {
                return m_stem;
            }

            /**
             * Sets the Effect/Skill GB stem.
             * @param v Model name without a folder prefix.
             */
            void set_stem(std::string v) {
                m_stem = std::move(v);
            }

            /** @return Extra offset X from the FX stage. */
            [[nodiscard]] float ox() const {
                return m_ox;
            }

            /**
             * Sets the extra offset X.
             * @param v World-axis X.
             */
            void set_ox(float v) {
                m_ox = v;
            }

            /** @return Extra offset Y from the FX stage. */
            [[nodiscard]] float oy() const {
                return m_oy;
            }

            /**
             * Sets the extra offset Y.
             * @param v World-axis Y.
             */
            void set_oy(float v) {
                m_oy = v;
            }

            /** @return Extra offset Z from the FX stage. */
            [[nodiscard]] float oz() const {
                return m_oz;
            }

            /**
             * Sets the extra offset Z.
             * @param v World-axis Z.
             */
            void set_oz(float v) {
                m_oz = v;
            }

            /** @return FX stage delay in seconds. */
            [[nodiscard]] float delay() const {
                return m_delay;
            }

            /**
             * Sets the FX stage delay before the model plays.
             * @param v Seconds.
             */
            void set_delay(float v) {
                m_delay = v;
            }

            /** @return Stage duration in seconds (default 3). */
            [[nodiscard]] float duration() const {
                return m_duration;
            }

            /**
             * Sets the FX stage duration.
             * @param v Seconds.
             */
            void set_duration(float v) {
                m_duration = v;
            }

            /** @return FX `restart` flag. */
            [[nodiscard]] int restart() const {
                return m_restart;
            }

            /**
             * Sets the FX `restart` flag.
             * @param v Non-zero to loop delay + duration.
             */
            void set_restart(int v) {
                m_restart = v;
            }

            /**
             * EFFECTBONE slot from `effectBoneIndex` (or -1 for origin / feet).
             * @return Slot index.
             */
            [[nodiscard]] int effect_bone() const {
                return m_effect_bone;
            }

            /**
             * Sets the EFFECTBONE slot from the FX stage.
             * @param v Slot index, or -1 for origin / feet.
             */
            void set_effect_bone(int v) {
                m_effect_bone = v;
            }

            /** True when the FX stage sets `useEffectBoneRotation`. */
            [[nodiscard]] bool use_effect_bone_rotation() const {
                return m_use_effect_bone_rotation;
            }

            /**
             * Sets the FX `useEffectBoneRotation` flag.
             * @param v True to copy the EFFECTBONE matrix, not only its translation.
             */
            void set_use_effect_bone_rotation(bool v) {
                m_use_effect_bone_rotation = v;
            }

          private:
            /** Effect/Skill GB stem. */
            std::string m_stem;

            /** Extra offset X from the FX stage. */
            float m_ox = 0.0f;

            /** Extra offset Y from the FX stage. */
            float m_oy = 0.0f;

            /** Extra offset Z from the FX stage. */
            float m_oz = 0.0f;

            /** FX stage delay in seconds. */
            float m_delay = 0.0f;

            /** Stage duration in seconds. */
            float m_duration = 3.0f;

            /** FX `restart` flag (non-zero loops delay + duration). */
            int m_restart = 0;

            /** EFFECTBONE slot from `effectBoneIndex` (-1 = origin / feet). */
            int m_effect_bone = -1;

            /** FX `useEffectBoneRotation` (full bone matrix vs character rotation). */
            bool m_use_effect_bone_rotation = false;
        };

        /**
         * Indexes every sprite / FX script and resets renderer state.
         * @param engine Engine used to release resources from an earlier initialization.
         * @param data_root Client data root.
         * @return True once scripts can be resolved (missing folders still return
         * true).
         */
        [[nodiscard]] bool init(engine::IEngine& engine, const std::string& data_root);

        /**
         * Releases GPU resources created by init().
         * @param engine Engine owning the meshes and textures.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Attaches a looping particle shader to an entity slot list.
         * @param engine Engine used to load stage textures.
         * @param entity Entity key (monster net id, or NPC id with the high bit set).
         * @param name Particle shader name (e.g. revenspirit_head).
         * @param effect_bone EFFECTBONE index, or -1 for origin / feet.
         * @param ox World-axis offset X added to the follow point.
         * @param oy World-axis offset Y.
         * @param oz World-axis offset Z.
         */
        void attach_particle(engine::IEngine& engine,
                             std::uint32_t entity,
                             std::string_view name,
                             int effect_bone,
                             float ox,
                             float oy,
                             float oz);

        /**
         * Attaches an FX shader by spawning each of its particle stages.
         * @param engine Engine used to load stage textures.
         * @param entity Entity key.
         * @param name FX shader name.
         * @param effect_bone EFFECTBONE index from monsterinfo, or -1.
         * @param delay_override Replacement delay in seconds for every particle stage,
         * or a negative value to keep each authored delay.
         */
        void attach_fx(engine::IEngine& engine,
                       std::uint32_t entity,
                       std::string_view name,
                       int effect_bone,
                       float delay_override = -1.0f);

        /**
         * Restarts attached FX stages for the entity lifetime (initial
         * `AddFxToTarget` with `nForced < 0`).
         * @param entity Entity key.
         */
        void loop_attached(std::uint32_t entity);

        /**
         * Keeps attached particle stages for the entity lifetime. Packed
         * `effect_ef068a` uses `duration 2` / `restart 0` while the sprite
         * stages use `cycles 0`; gating those 2 s would fade the cloud out.
         * @param entity Entity key.
         */
        void persist_attached(std::uint32_t entity);

        /**
         * True when the entity already has at least one looping emitter.
         * @param entity Entity key.
         * @return True when attach_* has succeeded for this entity.
         */
        [[nodiscard]] bool has(std::uint32_t entity) const;

        /**
         * Direct skeleton bone from `realeffectBoneIndex` for a follow slot.
         * @param entity Entity key.
         * @param effect_slot EFFECTBONE slot used as the follow key.
         * @return Skeleton bone, or -1 when the stage did not set one.
         */
        [[nodiscard]] int real_effect_bone(std::uint32_t entity, int effect_slot) const;

        /**
         * Moves every emitter of an entity whose EFFECTBONE matches.
         * @param entity Entity key.
         * @param effect_bone EFFECTBONE slot, or -1 for origin / feet.
         * @param x World X of the bone (before offsets).
         * @param y World Y.
         * @param z World Z.
         */
        void follow_bone(std::uint32_t entity, int effect_bone, float x, float y, float z);

        /**
         * Scales an entity's attached particle geometry and local kinematics.
         * @param entity Entity key.
         * @param scale Uniform target-model scale; non-positive values use 1.
         */
        void set_target_scale(std::uint32_t entity, float scale);

        /**
         * Moves matching emitters with a full world matrix (weapon mix: local
         * direction / offset follow the blade).
         * @param entity Entity key.
         * @param effect_bone EFFECTBONE slot, or -1 for origin / feet.
         * @param world Column-major world matrix of the attach bone.
         */
        void follow_oriented(std::uint32_t entity, int effect_bone, const engine::Mat4& world);

        /**
         * Anchors matching emitters to the exact endpoints of a persistent beam.
         * The directional stages are rendered as ribbons between those endpoints,
         * rather than being extended through a scaled attach matrix.
         * @param entity Entity key.
         * @param effect_bone EFFECTBONE slot, or -1 for origin / feet.
         * @param source World-space beam origin.
         * @param target World-space beam endpoint.
         */
        void follow_beam(std::uint32_t entity,
                         int effect_bone,
                         const engine::Vec3& source,
                         const engine::Vec3& target);

        /**
         * Removes every looping emitter of an entity.
         * @param entity Entity key.
         */
        void detach(std::uint32_t entity);

        /**
         * Advances looping ages.
         * @param dt Frame delta in seconds.
         * @param eye_x Camera eye X.
         * @param eye_y Camera eye Y.
         * @param eye_z Camera eye Z.
         */
        void update(float dt, float eye_x, float eye_y, float eye_z);

        /** Scales every authored emitter cull distance.
         * @param scale Positive quality-dependent distance multiplier.
         */
        void set_cull_distance_scale(float scale);

        /**
         * Draws every live looping sprite (camera billboards, plus aimed ribbons).
         * @param engine Engine used to submit the draws.
         */
        void draw(engine::IEngine& engine) const;

        /** @return Emitters considered during the most recent draw. */
        [[nodiscard]] std::size_t last_draw_emitters() const {
            return m_last_draw_emitters;
        }

        /** @return Emitters rejected by authored cull distance during the most recent draw. */
        [[nodiscard]] std::size_t last_culled_emitters() const {
            return m_last_culled_emitters;
        }

        /** @return Billboard quads submitted during the most recent draw. */
        [[nodiscard]] std::size_t last_draw_billboards() const {
            return m_last_draw_billboards;
        }

        /** @return Aimed ribbon quads submitted during the most recent draw. */
        [[nodiscard]] std::size_t last_draw_ribbons() const {
            return m_last_draw_ribbons;
        }

        /** @return GPU sprite batches submitted during the most recent draw. */
        [[nodiscard]] std::size_t last_draw_batches() const {
            return m_last_draw_batches;
        }

        /** @return True once init() has completed. */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

        /**
         * Sorted particle shader names from Effect/Sprite.
         * @return Script names.
         */
        [[nodiscard]] std::vector<std::string> sprite_names() const;

        /**
         * Sorted FX shader names from Effect/Fx.
         * @return Script names.
         */
        [[nodiscard]] std::vector<std::string> fx_names() const;

        /**
         * True when a particle shader is indexed (case-insensitive).
         * @param name Shader name.
         * @return True when the script exists.
         */
        [[nodiscard]] bool has_sprite(std::string_view name) const;

        /**
         * True when an FX shader is indexed (case-insensitive).
         * @param name Shader name.
         * @return True when the script exists.
         */
        [[nodiscard]] bool has_fx(std::string_view name) const;

        /**
         * Particle names referenced by an FX shader.
         * @param name FX shader name.
         * @return Particle shader names in stage order.
         */
        [[nodiscard]] std::vector<std::string> fx_particles(std::string_view name) const;

        /**
         * FX `model` stages for one shader, including offset and delay.
         * @param name FX shader name.
         * @return Model stages in script order, or empty when unknown.
         */
        [[nodiscard]] std::vector<FxModelSpawn> fx_model_spawns(std::string_view name) const;

        /**
         * Model stems referenced by an FX shader (`model "…"` stages).
         * @param name FX shader name.
         * @return Model names without a folder prefix.
         */
        [[nodiscard]] std::vector<std::string> fx_models(std::string_view name) const;

        /**
         * Approximate world radius of a particle shader for camera framing.
         * @param engine Engine used to parse/load the shader.
         * @param name Particle shader name.
         * @return Radius in world units (at least 8).
         */
        [[nodiscard]] float sprite_radius(engine::IEngine& engine, std::string_view name);

        /**
         * Approximate world radius of an FX shader (max of its particle stages).
         * @param engine Engine used to parse/load particle stages.
         * @param name FX shader name.
         * @return Radius in world units (at least 8).
         */
        [[nodiscard]] float fx_radius(engine::IEngine& engine, std::string_view name);

        /**
         * Moves every emitter of an entity, ignoring EFFECTBONE.
         * @param entity Entity key.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void follow_all(std::uint32_t entity, float x, float y, float z);

        /**
         * Resets looping ages of an entity's emitters.
         * @param entity Entity key.
         */
        void rewind(std::uint32_t entity);

        /**
         * Moves one-shot emitters spawned with a follow entity
         * (compatibility behavior runtime state). Origin / feet stages
         * (`effectBoneIndex < 0`) use the same point as bone stages.
         * @param entity Net entity id passed to spawn_oneshot_fx.
         * @param x World X of the follow bone / spine.
         * @param y World Y.
         * @param z World Z.
         */
        void follow_oneshots(std::uint32_t entity, float x, float y, float z);

        /**
         * Moves followed one-shots, keeping origin stages on the character matrix
         * (FrameMove: `effectBoneIndex < 0` uses `runtime state`, not a bone).
         * @param entity Net entity id passed to spawn_oneshot_fx.
         * @param x World X of the follow bone / spine.
         * @param y World Y of the follow bone / spine.
         * @param z World Z of the follow bone / spine.
         * @param origin_x World X of the feet / character origin.
         * @param origin_y World Y of the feet / character origin.
         * @param origin_z World Z of the feet / character origin.
         */
        void follow_oneshots(std::uint32_t entity,
                             float x,
                             float y,
                             float z,
                             float origin_x,
                             float origin_y,
                             float origin_z);

        /**
         * Moves followed one-shots using the compatibility behavior attach
         * matrix: `useEffectBoneRotation` copies the EFFECTBONE matrix, otherwise
         * the character matrix with bone translation. Origin stages keep `origin`.
         * @param entity Net entity id passed to spawn_oneshot_fx.
         * @param origin Character world matrix (`runtime state`).
         * @param effect_bones EFFECTBONE world matrices, or nullptr.
         * @param effect_bone_count Length of effect_bones (EFFECTBONE_COUNT).
         */
        void follow_oneshots(std::uint32_t entity,
                             const engine::Mat4& origin,
                             const engine::Mat4* effect_bones,
                             int effect_bone_count);

        /**
         * Compatibility behavior attach matrix (character vs EFFECTBONE
         * rotation, then bone translation).
         * @param origin Character world matrix.
         * @param bone EFFECTBONE world matrix.
         * @param use_effect_bone_rotation True to copy bone rotation.
         * @return Attach matrix before the FX stage offset.
         */
        [[nodiscard]] static engine::Mat4 effect_bone_attach_matrix(const engine::Mat4& origin,
                                                                    const engine::Mat4& bone,
                                                                    bool use_effect_bone_rotation);

        /**
         * Applies an FX stage offset in the attach matrix's local axes
         * (`mat._41 += row * offset`).
         * @param mat Attach matrix.
         * @param ox Local X.
         * @param oy Local Y.
         * @param oz Local Z.
         * @return Matrix with translation shifted by the offset.
         */
        [[nodiscard]] static engine::Mat4
        offset_in_matrix_axes(engine::Mat4 mat, float ox, float oy, float oz);

        /**
         * True when any stage in an FX script body sets `useEffectBoneRotation`.
         * @param body Shader body starting at '{'.
         * @return True when at least one stage uses bone rotation.
         */
        [[nodiscard]] bool parsed_fx_uses_effect_bone_rotation(std::string_view body) const;

        /**
         * Oneshot attach lifetime from an FX script: max(delay + duration) per
         * stage. Duration 0 uses the initial 3 s editor default so delayed
         * follow-up particles (Arrow Rain `davi_ef132_02`) are not culled.
         * @param body Shader body starting at '{'.
         * @return Seconds the oneshot emitters must stay attached.
         */
        [[nodiscard]] float parsed_fx_oneshot_life(std::string_view body) const;

        /**
         * `animTexture` frame count of the first particle stage (1 = whole texture).
         * @param body Shader body starting at '{'.
         * @return Frame count, or 1 when the stage has no atlas.
         */
        [[nodiscard]] int parsed_sprite_anim_frames(std::string_view body) const;

        /**
         * Authored particle shader cull distance.
         * @param body Shader body starting at '{'.
         * @return Parsed distance, or the default of 768 world units.
         */
        [[nodiscard]] float parsed_sprite_cull_distance(std::string_view body) const;

        /**
         * `distribution` kind of the first particle stage.
         * @param body Shader body starting at '{'.
         * @return Box, sphere, or cylinder.
         */
        [[nodiscard]] SpriteDistribution parsed_sprite_distribution(std::string_view body) const;

        /**
         * True when the first particle stage sets `initialAngle`.
         * @param body Shader body starting at '{'.
         * @return True when the start angle is authored, not random.
         */
        [[nodiscard]] bool parsed_sprite_has_initial_angle(std::string_view body) const;

        /**
         * `initialAngle` of the first particle stage in degrees.
         * @param body Shader body starting at '{'.
         * @return Authored degrees, or 0 when the token is absent.
         */
        [[nodiscard]] float parsed_sprite_initial_angle(std::string_view body) const;

        /**
         * Spawns a one-shot FX shader at a world point (compatibility behavior
         * for hits such as `palsy dmg2`).
         * @param engine Engine used to load stage textures.
         * @param name FX shader name.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param follow_entity Optional net id to follow each frame (0 = world-fixed).
         * @return Sound names referenced by the FX / particle shaders (may be empty).
         */
        std::vector<std::string> spawn_oneshot_fx(engine::IEngine& engine,
                                                  std::string_view name,
                                                  float x,
                                                  float y,
                                                  float z,
                                                  std::uint32_t follow_entity = 0);

        /**
         * Spawns a one-shot particle shader at a world point (`AddParticle` /
         * `AddParticleToTargetBone`).
         * @param engine Engine used to load stage textures.
         * @param name Particle shader name.
         * @param x World X (follow bone / spawn origin).
         * @param y World Y.
         * @param z World Z.
         * @param follow_entity Optional net id to follow each frame (0 = world-fixed).
         * @param effect_bone EFFECTBONE slot, or -1 for origin.
         * @param ox Offset X added to the follow point.
         * @param oy Offset Y.
         * @param oz Offset Z.
         */
        void spawn_oneshot_particle(engine::IEngine& engine,
                                    std::string_view name,
                                    float x,
                                    float y,
                                    float z,
                                    std::uint32_t follow_entity = 0,
                                    int effect_bone = -1,
                                    float ox = 0.0f,
                                    float oy = 0.0f,
                                    float oz = 0.0f);

        /** @return Emitter key of the last successful spawn_oneshot_fx() call. */
        [[nodiscard]] std::uint32_t last_oneshot_key() const {
            return m_last_oneshot_key;
        }

        /**
         * Lists sound stems referenced by an FX script and its particle shaders.
         * @param engine Engine used to parse particle shaders.
         * @param name FX shader name.
         * @return Unique sound names (may be empty).
         */
        [[nodiscard]] std::vector<std::string> fx_sounds(engine::IEngine& engine,
                                                         std::string_view name);

      private:
        /** One parsed sprite stage (subset of prtShaderStage_t). */
        class SpriteStage {
          public:
            /** Particle quad facing (`orientation`). */
            enum class Orientation : std::uint8_t {
                /** Camera-facing (`view`). */
                View = 0,
                /** Camera right, world up (`viewy`). */
                ViewY,
                /** Path-aligned ribbon (`aimed`). */
                Aimed,
                /** Aimed ribbon locked to world Y (`aimedy`). */
                AimedY,
                /** YZ plane (`x`). */
                AxisX,
                /** XZ ground plane (`y`). */
                AxisY,
                /** XY plane (`z`). */
                AxisZ,
            };

            /** @return Texture file name (GTX stem or full name). */
            [[nodiscard]] const std::string& texture() const {
                return m_texture;
            }

            /**
             * Sets the texture file name.
             * @param v Texture name.
             */
            void set_texture(std::string v) {
                m_texture = std::move(v);
            }

            /** @return Size at spawn. */
            [[nodiscard]] float size0() const {
                return m_size0;
            }

            /**
             * Sets the size at spawn.
             * @param v Size in world units.
             */
            void set_size0(float v) {
                m_size0 = v;
            }

            /** @return Size at death. */
            [[nodiscard]] float size1() const {
                return m_size1;
            }

            /**
             * Sets the size at death.
             * @param v Size in world units.
             */
            void set_size1(float v) {
                m_size1 = v;
            }

            /** @return Billboard width scale at spawn (`aspect` start). */
            [[nodiscard]] float aspect0() const {
                return m_aspect0;
            }

            /**
             * Sets the aspect at spawn.
             * @param v Width scale (1 = square).
             */
            void set_aspect0(float v) {
                m_aspect0 = v;
            }

            /** @return Billboard width scale at death (`aspect` end). */
            [[nodiscard]] float aspect1() const {
                return m_aspect1;
            }

            /**
             * Sets the aspect at death.
             * @param v Width scale (1 = square).
             */
            void set_aspect1(float v) {
                m_aspect1 = v;
            }

            /** @return Red tint. */
            [[nodiscard]] float r() const {
                return m_r;
            }

            /**
             * Sets the red tint.
             * @param v Red in 0..1.
             */
            void set_r(float v) {
                m_r = v;
            }

            /** @return Green tint. */
            [[nodiscard]] float g() const {
                return m_g;
            }

            /**
             * Sets the green tint.
             * @param v Green in 0..1.
             */
            void set_g(float v) {
                m_g = v;
            }

            /** @return Blue tint. */
            [[nodiscard]] float b() const {
                return m_b;
            }

            /**
             * Sets the blue tint.
             * @param v Blue in 0..1.
             */
            void set_b(float v) {
                m_b = v;
            }

            /** @return Alpha at spawn. */
            [[nodiscard]] float a() const {
                return m_a;
            }

            /**
             * Sets the alpha at spawn.
             * @param v Alpha in 0..1.
             */
            void set_a(float v) {
                m_a = v;
            }

            /** @return Fade red. */
            [[nodiscard]] float fade_r() const {
                return m_fade_r;
            }

            /**
             * Sets the fade red.
             * @param v Red in 0..1.
             */
            void set_fade_r(float v) {
                m_fade_r = v;
            }

            /** @return Fade green. */
            [[nodiscard]] float fade_g() const {
                return m_fade_g;
            }

            /**
             * Sets the fade green.
             * @param v Green in 0..1.
             */
            void set_fade_g(float v) {
                m_fade_g = v;
            }

            /** @return Fade blue. */
            [[nodiscard]] float fade_b() const {
                return m_fade_b;
            }

            /**
             * Sets the fade blue.
             * @param v Blue in 0..1.
             */
            void set_fade_b(float v) {
                m_fade_b = v;
            }

            /** @return Fade alpha. */
            [[nodiscard]] float fade_a() const {
                return m_fade_a;
            }

            /**
             * Sets the fade alpha.
             * @param v Alpha in 0..1.
             */
            void set_fade_a(float v) {
                m_fade_a = v;
            }

            /** @return True when fadeColor was present in the script. */
            [[nodiscard]] bool has_fade() const {
                return m_has_fade;
            }

            /**
             * Sets whether fadeColor was parsed.
             * @param v True when fadeColor was present.
             */
            void set_has_fade(bool v) {
                m_has_fade = v;
            }

            /**
             * `fade` midpoint (0–1) between fadeColor and color.
             * @return Fraction of life used for fade-in; default 0.5.
             */
            [[nodiscard]] float fade() const {
                return m_fade;
            }

            /**
             * Sets the fade midpoint.
             * @param v `fade` value.
             */
            void set_fade(float v) {
                m_fade = v;
            }

            /** @return Particle count of the stage. */
            [[nodiscard]] int count() const {
                return m_count;
            }

            /**
             * Sets the particle count.
             * @param v Count.
             */
            void set_count(int v) {
                m_count = v;
            }

            /** @return Loop period in seconds. */
            [[nodiscard]] float time() const {
                return m_time;
            }

            /**
             * Sets the loop period.
             * @param v Seconds.
             */
            void set_time(float v) {
                m_time = v;
            }

            /** @return Quiet gap after `time` before the next cycle (deadTime). */
            [[nodiscard]] float dead_time() const {
                return m_dead_time;
            }

            /**
             * Sets the quiet gap between cycles.
             * @param v Seconds; 0 means the next cycle starts immediately.
             */
            void set_dead_time(float v) {
                m_dead_time = v;
            }

            /** @return Optional sound stem from the particle shader. */
            [[nodiscard]] const std::string& sound() const {
                return m_sound;
            }

            /**
             * Sets the optional particle sound stem.
             * @param v Sound name (e.g. efc008).
             */
            void set_sound(std::string v) {
                m_sound = std::move(v);
            }

            /** @return True when blendFunc is add / addblend. */
            [[nodiscard]] bool additive() const {
                return m_additive;
            }

            /**
             * Sets the additive blend flag (ONE/ONE or addblend).
             * @param v True for additive blending.
             */
            void set_additive(bool v) {
                m_additive = v;
            }

            /** @return True when blendFunc is addblend (SRC_ALPHA/ONE). */
            [[nodiscard]] bool addblend() const {
                return m_addblend;
            }

            /**
             * Sets the addblend flag.
             * @param v True for SRC_ALPHA/ONE.
             */
            void set_addblend(bool v) {
                m_addblend = v;
            }

            /** @return Speed at spawn (envelope start). */
            [[nodiscard]] float speed0() const {
                return m_speed0;
            }

            /**
             * Sets speed at spawn.
             * @param v Speed.
             */
            void set_speed0(float v) {
                m_speed0 = v;
            }

            /** @return Speed at death (envelope end). */
            [[nodiscard]] float speed1() const {
                return m_speed1;
            }

            /**
             * Sets speed at death.
             * @param v Speed.
             */
            void set_speed1(float v) {
                m_speed1 = v;
            }

            /** @return Gravity subtracted from local Y over the particle lifetime. */
            [[nodiscard]] float gravity() const {
                return m_gravity;
            }

            /**
             * Sets gravity.
             * @param v Gravity.
             */
            void set_gravity(float v) {
                m_gravity = v;
            }

            /** @return Rotation degrees per second at spawn. */
            [[nodiscard]] float rot0() const {
                return m_rot0;
            }

            /**
             * Sets rotation at spawn.
             * @param v Degrees per second.
             */
            void set_rot0(float v) {
                m_rot0 = v;
            }

            /** @return Rotation degrees per second at death. */
            [[nodiscard]] float rot1() const {
                return m_rot1;
            }

            /**
             * Sets rotation at death.
             * @param v Degrees per second.
             */
            void set_rot1(float v) {
                m_rot1 = v;
            }

            /** @return True when the stage sets `initialAngle` (not a random start). */
            [[nodiscard]] bool has_initial_angle() const {
                return m_has_initial_angle;
            }

            /**
             * Marks a fixed start angle from `initialAngle`.
             * @param v True when the shader token is present.
             */
            void set_has_initial_angle(bool v) {
                m_has_initial_angle = v;
            }

            /** @return Start angle in degrees (`initialAngle`, else unused). */
            [[nodiscard]] float initial_angle() const {
                return m_initial_angle;
            }

            /**
             * Sets the authored start angle.
             * @param v Degrees (medicine column uses 90).
             */
            void set_initial_angle(float v) {
                m_initial_angle = v;
            }

            /** @return Spawn stagger (0 = all at t=0, 1 = spread across the lifetime). */
            [[nodiscard]] float bunching() const {
                return m_bunching;
            }

            /**
             * Sets spawn stagger.
             * @param v Bunching factor.
             */
            void set_bunching(float v) {
                m_bunching = v;
            }

            /** @return Repeat count (0 = loop forever). */
            [[nodiscard]] int cycles() const {
                return m_cycles;
            }

            /**
             * Sets how many times the stage repeats.
             * @param v 0 for infinite, 1 for a single burst.
             */
            void set_cycles(int v) {
                m_cycles = v;
            }

            /** @return 0 standard, 1 helix, 2 spherical, 3 files, 4 cone. */
            [[nodiscard]] int path_mode() const {
                return m_path_mode;
            }

            /**
             * Sets the custom path mode.
             * @param v 0 standard, 1 helix, 2 spherical, 3 files, 4 cone.
             */
            void set_path_mode(int v) {
                m_path_mode = v;
            }

            /** @return Helix radial angle scale (`customPathValue[0]`). */
            [[nodiscard]] float path0() const {
                return m_path0;
            }

            /**
             * Sets helix radial angle scale.
             * @param v Radians over the particle lifetime.
             */
            void set_path0(float v) {
                m_path0 = v;
            }

            /** @return Helix Y travel half-range (`customPathValue[1]`). */
            [[nodiscard]] float path1() const {
                return m_path1;
            }

            /**
             * Sets helix Y travel half-range.
             * @param v Local Y units.
             */
            void set_path1(float v) {
                m_path1 = v;
            }

            /** @return Helix X spawn radius (`customPathValue[2]`). */
            [[nodiscard]] float path2() const {
                return m_path2;
            }

            /**
             * Sets helix X spawn radius.
             * @param v Local X units.
             */
            void set_path2(float v) {
                m_path2 = v;
            }

            /** @return Helix Y spawn half-range (`customPathValue[3]`). */
            [[nodiscard]] float path3() const {
                return m_path3;
            }

            /**
             * Sets helix Y spawn half-range.
             * @param v Local Y units.
             */
            void set_path3(float v) {
                m_path3 = v;
            }

            /** @return Helix Z spawn radius (`customPathValue[4]`). */
            [[nodiscard]] float path4() const {
                return m_path4;
            }

            /**
             * Sets helix Z spawn radius.
             * @param v Local Z units.
             */
            void set_path4(float v) {
                m_path4 = v;
            }

            /** @return Parsed `orientation` (view / viewy / aimed / x / y / z). */
            [[nodiscard]] Orientation orientation() const {
                return m_orientation;
            }

            /**
             * Sets particle facing.
             * @param v Orientation token.
             */
            void set_orientation(Orientation v) {
                m_orientation = v;
            }

            /** @return True when the stage uses `orientation aimed` or `aimedy`. */
            [[nodiscard]] bool aimed() const {
                return m_orientation == Orientation::Aimed || m_orientation == Orientation::AimedY;
            }

            /** @return Extra trail samples (`trails`). */
            [[nodiscard]] int trails() const {
                return m_trails;
            }

            /**
             * Sets how many ghost samples follow the particle.
             * @param v `trails` count.
             */
            void set_trails(int v) {
                m_trails = v;
            }

            /** @return Trail look-back as a fraction of `time` (`trailTime`). */
            [[nodiscard]] float trail_time() const {
                return m_trail_time;
            }

            /**
             * Sets trail look-back.
             * @param v `trailTime`.
             */
            void set_trail_time(float v) {
                m_trail_time = v;
            }

            /** @return 0 none, 1 cone, 2 outward. */
            [[nodiscard]] int dir_mode() const {
                return m_dir_mode;
            }

            /**
             * Sets the direction mode.
             * @param v 0 none, 1 cone, 2 outward.
             */
            void set_dir_mode(int v) {
                m_dir_mode = v;
            }

            /** @return Cone half-angle in degrees, or outward weight. */
            [[nodiscard]] float dir0() const {
                return m_dir0;
            }

            /**
             * Sets direction value 0.
             * @param v Cone angle or outward weight.
             */
            void set_dir0(float v) {
                m_dir0 = v;
            }

            /** @return Direction axis X. */
            [[nodiscard]] float dir_x() const {
                return m_dir_x;
            }

            /**
             * Sets direction axis X.
             * @param v Axis X.
             */
            void set_dir_x(float v) {
                m_dir_x = v;
            }

            /** @return Direction axis Y. */
            [[nodiscard]] float dir_y() const {
                return m_dir_y;
            }

            /**
             * Sets direction axis Y.
             * @param v Axis Y.
             */
            void set_dir_y(float v) {
                m_dir_y = v;
            }

            /** @return Direction axis Z. */
            [[nodiscard]] float dir_z() const {
                return m_dir_z;
            }

            /**
             * Sets direction axis Z.
             * @param v Axis Z.
             */
            void set_dir_z(float v) {
                m_dir_z = v;
            }

            /** @return Sphere radius or cylinder inner thickness (0..1). */
            [[nodiscard]] float dist_w() const {
                return m_dist_w;
            }

            /**
             * Sets the sphere radius or cylinder inner thickness.
             * @param v Sphere radius, or cylinder thickness in 0..1.
             */
            void set_dist_w(float v) {
                m_dist_w = v;
            }

            /** @return `distribution` kind. */
            [[nodiscard]] SpriteDistribution dist_mode() const {
                return m_dist_mode;
            }

            /**
             * Sets the `distribution` kind.
             * @param v Box, sphere, or cylinder.
             */
            void set_dist_mode(SpriteDistribution v) {
                m_dist_mode = v;
            }

            /** @return Shader-local offset X. */
            [[nodiscard]] float ox() const {
                return m_ox;
            }

            /**
             * Sets the shader-local offset X.
             * @param v World-axis X.
             */
            void set_ox(float v) {
                m_ox = v;
            }

            /** @return Shader-local offset Y. */
            [[nodiscard]] float oy() const {
                return m_oy;
            }

            /**
             * Sets the shader-local offset Y.
             * @param v World-axis Y.
             */
            void set_oy(float v) {
                m_oy = v;
            }

            /** @return Shader-local offset Z. */
            [[nodiscard]] float oz() const {
                return m_oz;
            }

            /**
             * Sets the shader-local offset Z.
             * @param v World-axis Z.
             */
            void set_oz(float v) {
                m_oz = v;
            }

            /** @return Box distribution half-extent X. */
            [[nodiscard]] float dist_x() const {
                return m_dist_x;
            }

            /**
             * Sets the box distribution half-extent X.
             * @param v Half-extent.
             */
            void set_dist_x(float v) {
                m_dist_x = v;
            }

            /** @return Box distribution half-extent Y. */
            [[nodiscard]] float dist_y() const {
                return m_dist_y;
            }

            /**
             * Sets the box distribution half-extent Y.
             * @param v Half-extent.
             */
            void set_dist_y(float v) {
                m_dist_y = v;
            }

            /** @return Box distribution half-extent Z. */
            [[nodiscard]] float dist_z() const {
                return m_dist_z;
            }

            /**
             * Sets the box distribution half-extent Z.
             * @param v Half-extent.
             */
            void set_dist_z(float v) {
                m_dist_z = v;
            }

            /** @return True when draws this stage only in post-process. */
            [[nodiscard]] bool post_process() const {
                return m_post_process;
            }

            /**
             * Sets whether the stage is a post-process shader (bumpenvmap / heathaze).
             * @param v True to skip the normal particle pass.
             */
            void set_post_process(bool v) {
                m_post_process = v;
            }

            /** @return Loaded sprite texture. */
            [[nodiscard]] engine::TextureHandle tex() const {
                return m_tex;
            }

            /**
             * Sets the loaded sprite texture.
             * @param v Texture handle.
             */
            void set_tex(engine::TextureHandle v) {
                m_tex = v;
            }

            /** @return Atlas frame count from `animTexture` (1 = whole texture). */
            [[nodiscard]] int anim_frames() const {
                return m_anim_frames;
            }

            /**
             * Sets the atlas frame count.
             * @param v Frame count, at least 1.
             */
            void set_anim_frames(int v) {
                m_anim_frames = v;
            }

            /** @return Animation frames per second from `animTexture`. */
            [[nodiscard]] int anim_fps() const {
                return m_anim_fps;
            }

            /**
             * Sets the animation frame rate.
             * @param v Frames per second.
             */
            void set_anim_fps(int v) {
                m_anim_fps = v;
            }

          private:
            /** Texture file name. */
            std::string m_texture;

            /** Atlas frame count (`animTexture`); 1 uses UV 0..1. */
            int m_anim_frames = 1;

            /** Animation frames per second (`animTexture`). */
            int m_anim_fps = 8;

            /** Size at spawn. */
            float m_size0 = 1.0f;

            /** Size at death. */
            float m_size1 = 1.0f;

            /** Billboard width scale at spawn. */
            float m_aspect0 = 1.0f;

            /** Billboard width scale at death. */
            float m_aspect1 = 1.0f;

            /** Red tint. */
            float m_r = 1.0f;

            /** Green tint. */
            float m_g = 1.0f;

            /** Blue tint. */
            float m_b = 1.0f;

            /** Alpha at spawn. */
            float m_a = 1.0f;

            /** Fade red. */
            float m_fade_r = 1.0f;

            /** Fade green. */
            float m_fade_g = 1.0f;

            /** Fade blue. */
            float m_fade_b = 1.0f;

            /** Fade alpha. */
            float m_fade_a = 0.0f;

            /** True when fadeColor was parsed. */
            bool m_has_fade = false;

            /** `fade` midpoint (default 0.5). */
            float m_fade = 0.5f;

            /** Particle count. */
            int m_count = 10;

            /** Loop period in seconds. */
            float m_time = 1.5f;

            /** Quiet gap after each cycle (`deadTime`). */
            float m_dead_time = 0.0f;

            /** Optional sound stem. */
            std::string m_sound;

            /** Additive blend (add or addblend). */
            bool m_additive = true;

            /** True when blendFunc is addblend. */
            bool m_addblend = true;

            /** Speed at spawn. */
            float m_speed0 = 0.0f;

            /** Speed at death. */
            float m_speed1 = 0.0f;

            /** Gravity (subtracted from local Y over the lifetime). */
            float m_gravity = 0.0f;

            /** Rotation degrees per second at spawn. */
            float m_rot0 = 0.0f;

            /** Rotation degrees per second at death. */
            float m_rot1 = 0.0f;

            /** True when `initialAngle` was parsed. */
            bool m_has_initial_angle = false;

            /** Authored start angle in degrees (`initialAngle`). */
            float m_initial_angle = 0.0f;

            /** Spawn stagger. */
            float m_bunching = 1.0f;

            /** Repeat count (0 = infinite). */
            int m_cycles = 0;

            /** 0 standard, 1 helix, 2 spherical, 3 files, 4 cone. */
            int m_path_mode = 0;

            /** Helix radial angle scale. */
            float m_path0 = 0.0f;

            /** Helix Y travel half-range. */
            float m_path1 = 0.0f;

            /** Helix X spawn radius. */
            float m_path2 = 0.0f;

            /** Helix Y spawn half-range. */
            float m_path3 = 0.0f;

            /** Helix Z spawn radius. */
            float m_path4 = 0.0f;

            /** `orientation` (default `view`). */
            Orientation m_orientation = Orientation::View;

            /** Extra trail samples. */
            int m_trails = 0;

            /** Trail look-back fraction of particle `time`. */
            float m_trail_time = 1.0f;

            /** 0 none, 1 cone, 2 outward. */
            int m_dir_mode = 0;

            /** Cone half-angle in degrees, or outward weight. */
            float m_dir0 = 0.0f;

            /** Direction axis X. */
            float m_dir_x = 0.0f;

            /** Direction axis Y. */
            float m_dir_y = 1.0f;

            /** Direction axis Z. */
            float m_dir_z = 0.0f;

            /** Sphere radius or cylinder inner thickness. */
            float m_dist_w = 0.0f;

            /** `distribution` kind. */
            SpriteDistribution m_dist_mode = SpriteDistribution::Box;

            /** Shader-local offset X. */
            float m_ox = 0.0f;

            /** Shader-local offset Y. */
            float m_oy = 0.0f;

            /** Shader-local offset Z. */
            float m_oz = 0.0f;

            /** Box distribution half-extent X. */
            float m_dist_x = 0.0f;

            /** Box distribution half-extent Y. */
            float m_dist_y = 0.0f;

            /** Box distribution half-extent Z. */
            float m_dist_z = 0.0f;

            /** True for bumpenvmap / heathaze (post-process pass only). */
            bool m_post_process = false;

            /** Loaded sprite texture. */
            engine::TextureHandle m_tex{};
        };

        /** Parsed particle shader. */
        class SpriteShader {
          public:
            /** @return Authored camera cull distance. */
            [[nodiscard]] float cull_distance() const {
                return m_cull_distance;
            }

            /**
             * Sets the authored camera cull distance.
             * @param value Distance in world units.
             */
            void set_cull_distance(float value) {
                m_cull_distance = value;
            }

            /** @return Stages in script order. */
            [[nodiscard]] const std::vector<SpriteStage>& stages() const {
                return m_stages;
            }

            /** @return Mutable stages in script order. */
            [[nodiscard]] std::vector<SpriteStage>& stages() {
                return m_stages;
            }

          private:
            /** Camera distance beyond which the emitter is not rendered. */
            float m_cull_distance = 768.0f;

            /** Parsed stages. */
            std::vector<SpriteStage> m_stages;
        };

        /** One particle stage referenced by an FX shader. */
        class FxParticleRef {
          public:
            /** @return Particle shader name. */
            [[nodiscard]] const std::string& particle() const {
                return m_particle;
            }

            /**
             * Sets the particle shader name.
             * @param v Shader name.
             */
            void set_particle(std::string v) {
                m_particle = std::move(v);
            }

            /** @return FX model stem, or empty. */
            [[nodiscard]] const std::string& model() const {
                return m_model;
            }

            /**
             * Sets the FX model stem.
             * @param v Model name without folder prefix.
             */
            void set_model(std::string v) {
                m_model = std::move(v);
            }

            /** @return Optional sound stem from the FX stage. */
            [[nodiscard]] const std::string& sound() const {
                return m_sound;
            }

            /**
             * Sets the optional FX-stage sound stem.
             * @param v Sound name.
             */
            void set_sound(std::string v) {
                m_sound = std::move(v);
            }

            /** @return Stage duration in seconds (default 3). */
            [[nodiscard]] float duration() const {
                return m_duration;
            }

            /**
             * Sets the FX stage duration.
             * @param v Seconds.
             */
            void set_duration(float v) {
                m_duration = v;
            }

            /** @return Extra offset X from the FX stage. */
            [[nodiscard]] float ox() const {
                return m_ox;
            }

            /**
             * Sets the extra offset X.
             * @param v World-axis X.
             */
            void set_ox(float v) {
                m_ox = v;
            }

            /** @return Extra offset Y from the FX stage. */
            [[nodiscard]] float oy() const {
                return m_oy;
            }

            /**
             * Sets the extra offset Y.
             * @param v World-axis Y.
             */
            void set_oy(float v) {
                m_oy = v;
            }

            /** @return Extra offset Z from the FX stage. */
            [[nodiscard]] float oz() const {
                return m_oz;
            }

            /**
             * Sets the extra offset Z.
             * @param v World-axis Z.
             */
            void set_oz(float v) {
                m_oz = v;
            }

            /**
             * True when the FX stage sets `effectBoneIndex`.
             * @return True when attach should prefer the stage bone over the caller bone.
             */
            [[nodiscard]] bool has_effect_bone() const {
                return m_has_effect_bone;
            }

            /**
             * EFFECTBONE slot from `effectBoneIndex` (or -1 for origin / feet).
             * @return Slot index when has_effect_bone(), otherwise undefined.
             */
            [[nodiscard]] int effect_bone() const {
                return m_effect_bone;
            }

            /**
             * Sets the EFFECTBONE slot from the FX stage.
             * @param v Slot index, or -1 for origin / feet.
             */
            void set_effect_bone(int v) {
                m_effect_bone = v;
                m_has_effect_bone = true;
            }

            /** @return FX stage delay in seconds. */
            [[nodiscard]] float delay() const {
                return m_delay;
            }

            /**
             * Sets the FX stage delay before the particle plays.
             * @param v Seconds.
             */
            void set_delay(float v) {
                m_delay = v;
            }

            /**
             * True when the stage should restart after delay + duration.
             * @return True when restart is non-zero.
             */
            [[nodiscard]] int restart() const {
                return m_restart;
            }

            /**
             * Sets the FX restart flag.
             * @param v Non-zero to loop delay/duration.
             */
            void set_restart(int v) {
                m_restart = v;
            }

            /** @return True when realeffectBoneIndex was parsed. */
            [[nodiscard]] bool has_real_effect_bone() const {
                return m_has_real_effect_bone;
            }

            /** @return Direct skeleton bone from realeffectBoneIndex. */
            [[nodiscard]] int real_effect_bone() const {
                return m_real_effect_bone;
            }

            /**
             * Sets the direct skeleton bone override.
             * @param v Skeleton bone index.
             */
            void set_real_effect_bone(int v) {
                m_real_effect_bone = v;
                m_has_real_effect_bone = true;
            }

            /** True when the FX stage sets `useEffectBoneRotation`. */
            [[nodiscard]] bool use_effect_bone_rotation() const {
                return m_use_effect_bone_rotation;
            }

            /**
             * Sets the FX `useEffectBoneRotation` flag.
             * @param v True to copy the EFFECTBONE matrix, not only its translation.
             */
            void set_use_effect_bone_rotation(bool v) {
                m_use_effect_bone_rotation = v;
            }

          private:
            /** Particle shader name. */
            std::string m_particle;

            /** FX model stem. */
            std::string m_model;

            /** Optional sound stem. */
            std::string m_sound;

            /** Stage duration in seconds. */
            float m_duration = 3.0f;

            /** Extra offset X. */
            float m_ox = 0.0f;

            /** Extra offset Y. */
            float m_oy = 0.0f;

            /** Extra offset Z. */
            float m_oz = 0.0f;

            /** EFFECTBONE slot from the FX stage. */
            int m_effect_bone = -1;

            /** True when runtime state was set from the shader. */
            bool m_has_effect_bone = false;

            /** Wait before the particle is visible. */
            float m_delay = 0.0f;

            /** Non-zero to loop delay + duration. */
            int m_restart = 0;

            /** Direct skeleton bone from realeffectBoneIndex. */
            int m_real_effect_bone = -1;

            /** True when runtime state was set. */
            bool m_has_real_effect_bone = false;

            /** FX `useEffectBoneRotation`. */
            bool m_use_effect_bone_rotation = false;
        };

        /** One live looping emitter attached to an entity. */
        class Emitter {
          public:
            /**
             * Creates an emitter at a world point.
             * @param stages Parsed sprite stages; copied so the catalog can move.
             * @param cull_distance Authored camera cull distance.
             * @param effect_bone EFFECTBONE slot, or -1.
             * @param ox Monsterinfo offset X.
             * @param oy Monsterinfo offset Y.
             * @param oz Monsterinfo offset Z.
             */
            Emitter(std::vector<SpriteStage> stages,
                    float cull_distance,
                    int effect_bone,
                    float ox,
                    float oy,
                    float oz);

            /**
             * Ages the emitter.
             * @param dt Frame delta in seconds.
             */
            void advance(float dt) {
                m_age += dt;
            }

            /** Resets the looping age to zero. */
            void rewind() {
                m_age = 0.0f;
            }

            /**
             * Moves the follow point (identity rotation, world translation).
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             */
            void set_world(float x, float y, float z) {
                m_x = x;
                m_y = y;
                m_z = z;
                m_world = engine::Mat4::translation(x, y, z);
                m_has_beam_target = false;
            }

            /**
             * Sets the attach matrix used to transform local particle kinematics.
             * @param world Column-major world matrix.
             */
            void set_world_matrix(const engine::Mat4& world) {
                m_world = world;
                m_x = world.m[12];
                m_y = world.m[13];
                m_z = world.m[14];
                m_has_beam_target = false;
            }

            /** @return Uniform scale inherited from the attached target. */
            [[nodiscard]] float target_scale() const {
                return m_target_scale;
            }

            /**
             * Sets the uniform scale inherited from the attached target.
             * @param scale Non-positive values use 1.
             */
            void set_target_scale(float scale) {
                m_target_scale = scale > 0.0f ? scale : 1.0f;
            }

            /**
             * Sets exact world-space endpoints for a persistent beam.
             * @param source World-space beam origin.
             * @param target World-space beam endpoint.
             */
            void set_beam(const engine::Vec3& source, const engine::Vec3& target) {
                set_world(source.x, source.y, source.z);
                m_beam_target_x = target.x;
                m_beam_target_y = target.y;
                m_beam_target_z = target.z;
                m_has_beam_target = true;
            }

            /** @return True when the emitter is rendered between fixed endpoints. */
            [[nodiscard]] bool has_beam_target() const {
                return m_has_beam_target;
            }

            /** @return World-space beam endpoint. */
            [[nodiscard]] engine::Vec3 beam_target() const {
                return {m_beam_target_x, m_beam_target_y, m_beam_target_z};
            }

            /** @return Attach world matrix. */
            [[nodiscard]] const engine::Mat4& world() const {
                return m_world;
            }

            /** @return Follow point X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return Follow point Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return Follow point Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return EFFECTBONE slot, or -1 for origin. */
            [[nodiscard]] int effect_bone() const {
                return m_effect_bone;
            }

            /** @return Monsterinfo offset X. */
            [[nodiscard]] float ox() const {
                return m_ox;
            }

            /** @return Monsterinfo offset Y. */
            [[nodiscard]] float oy() const {
                return m_oy;
            }

            /** @return Monsterinfo offset Z. */
            [[nodiscard]] float oz() const {
                return m_oz;
            }

            /** @return Seconds since attach. */
            [[nodiscard]] float age() const {
                return m_age;
            }

            /** @return Parsed stages. */
            [[nodiscard]] const std::vector<SpriteStage>& stages() const {
                return m_stages;
            }

            /** @return Authored camera cull distance. */
            [[nodiscard]] float cull_distance() const {
                return m_cull_distance;
            }

            /** @return FX-stage delay in seconds. */
            [[nodiscard]] float fx_delay() const {
                return m_fx_delay;
            }

            /**
             * Sets the FX-stage delay.
             * @param v Seconds.
             */
            void set_fx_delay(float v) {
                m_fx_delay = v;
            }

            /** @return FX-stage duration in seconds. */
            [[nodiscard]] float fx_duration() const {
                return m_fx_duration;
            }

            /**
             * Sets the FX-stage duration used for delay/restart gating.
             * @param v Seconds.
             */
            void set_fx_duration(float v) {
                m_fx_duration = v;
            }

            /** @return FX restart flag. */
            [[nodiscard]] int fx_restart() const {
                return m_fx_restart;
            }

            /**
             * Sets the FX restart flag.
             * @param v Non-zero to loop.
             */
            void set_fx_restart(int v) {
                m_fx_restart = v;
            }

            /** @return True when a realeffectBoneIndex override is set. */
            [[nodiscard]] bool has_real_effect_bone() const {
                return m_has_real_effect_bone;
            }

            /** @return Direct skeleton bone, or -1. */
            [[nodiscard]] int real_effect_bone() const {
                return m_real_effect_bone;
            }

            /**
             * Sets the direct skeleton bone override.
             * @param v Skeleton bone index.
             */
            void set_real_effect_bone(int v) {
                m_real_effect_bone = v;
                m_has_real_effect_bone = true;
            }

            /** True when the FX stage sets `useEffectBoneRotation`. */
            [[nodiscard]] bool use_effect_bone_rotation() const {
                return m_use_effect_bone_rotation;
            }

            /**
             * Sets the FX `useEffectBoneRotation` flag.
             * @param v True to copy the EFFECTBONE matrix, not only its translation.
             */
            void set_use_effect_bone_rotation(bool v) {
                m_use_effect_bone_rotation = v;
            }

          private:
            /** Parsed stages. */
            std::vector<SpriteStage> m_stages;

            /** Camera distance beyond which the emitter is not rendered. */
            float m_cull_distance = 768.0f;

            /** Follow point X. */
            float m_x = 0.0f;

            /** Follow point Y. */
            float m_y = 0.0f;

            /** Follow point Z. */
            float m_z = 0.0f;

            /** Attach world matrix (identity + translation until follow_oriented). */
            engine::Mat4 m_world{};

            /** Uniform scale inherited from the attached model. */
            float m_target_scale = 1.0f;

            /** World-space beam endpoint X. */
            float m_beam_target_x = 0.0f;

            /** World-space beam endpoint Y. */
            float m_beam_target_y = 0.0f;

            /** World-space beam endpoint Z. */
            float m_beam_target_z = 0.0f;

            /** True when directional stages form a beam between fixed endpoints. */
            bool m_has_beam_target = false;

            /** EFFECTBONE slot, or -1. */
            int m_effect_bone = -1;

            /** Monsterinfo offset X. */
            float m_ox = 0.0f;

            /** Monsterinfo offset Y. */
            float m_oy = 0.0f;

            /** Monsterinfo offset Z. */
            float m_oz = 0.0f;

            /** Seconds since attach. */
            float m_age = 0.0f;

            /** FX-stage delay before the particle is drawn. */
            float m_fx_delay = 0.0f;

            /** FX-stage visible duration when delay/restart gating is active. */
            float m_fx_duration = 0.0f;

            /** Non-zero to loop delay + duration. */
            int m_fx_restart = 0;

            /** Direct skeleton bone from realeffectBoneIndex. */
            int m_real_effect_bone = -1;

            /** True when runtime state was set. */
            bool m_has_real_effect_bone = false;

            /** FX `useEffectBoneRotation`. */
            bool m_use_effect_bone_rotation = false;
        };

        /**
         * Indexes every quoted shader name in a Sprite or Fx directory.
         * @param dir Directory to scan.
         * @param decode_dat True to Decode(4) .dat files.
         * @param out Map filled with shader name → script body starting at '{'.
         */
        void index_scripts(const std::string& dir,
                           bool decode_dat,
                           std::unordered_map<std::string, std::string>& out);

        /**
         * Parses a particle shader body (starting at '{') into stages.
         * @param engine Engine used to load textures.
         * @param body Script body.
         * @return Parsed shader, or empty stages on failure.
         */
        [[nodiscard]] SpriteShader parse_sprite(engine::IEngine& engine, std::string_view body);

        /**
         * Parses a particle shader body without loading textures.
         * @param body Script body.
         * @return Parsed shader, or empty stages on failure.
         */
        [[nodiscard]] SpriteShader parse_sprite_fields(std::string_view body) const;

        /**
         * Parses an FX shader body into particle references.
         * @param body Script body.
         * @return Particle stages of the FX shader.
         */
        [[nodiscard]] std::vector<FxParticleRef> parse_fx(std::string_view body) const;

        /**
         * Loads or reuses an Effect/Tex texture.
         * @param engine Engine used to create the texture.
         * @param name Texture file name.
         * @return Texture handle (1x1 fallback when missing).
         */
        [[nodiscard]] engine::TextureHandle load_tex(engine::IEngine& engine,
                                                     const std::string& name);

        /**
         * Resolves a particle shader, parsing on first use.
         * @param engine Engine used to load textures.
         * @param name Shader name.
         * @return Pointer to the cached shader, or nullptr when unknown.
         */
        [[nodiscard]] const SpriteShader* find_sprite(engine::IEngine& engine,
                                                      std::string_view name);

        /**
         * Looks up a script body by case-insensitive name.
         * @param scripts Name → body map.
         * @param name Shader name.
         * @return Body pointer, or nullptr.
         */
        [[nodiscard]] const std::string*
        find_script(const std::unordered_map<std::string, std::string>& scripts,
                    std::string_view name) const;

        /**
         * Sorted names from a script map.
         * @param scripts Name → body map.
         * @return Names sorted case-insensitively.
         */
        [[nodiscard]] static std::vector<std::string>
        sorted_names(const std::unordered_map<std::string, std::string>& scripts);

        /**
         * Bounding radius of a parsed particle shader.
         * @param shader Parsed shader.
         * @return Radius in world units.
         */
        [[nodiscard]] static float shader_radius(const SpriteShader& shader);

        /**
         * Appends one particle quad with `orientation` axes.
         * @param vertices Batch vertex destination.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param size Quad height in world units.
         * @param aspect Width scale (`aspect`; 1 = square).
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param a Alpha.
         * @param angle_rad Spin around the facing axis.
         * @param orientation `orientation` token.
         * @param u0 Texture U at the left (animTexture frame).
         * @param u1 Texture U at the right (animTexture frame).
         */
        void append_billboard(std::vector<engine::SpriteBatchVertex>& vertices,
                              float x,
                              float y,
                              float z,
                              float size,
                              float aspect,
                              float r,
                              float g,
                              float b,
                              float a,
                              float angle_rad,
                              SpriteStage::Orientation orientation,
                              float u0 = 0.0f,
                              float u1 = 1.0f) const;

        /**
         * Appends one aimed ribbon segment between two world points (initial
         * ORIENTATION_AIMED).
         * @param vertices Batch vertex destination.
         * @param x0 Start world X.
         * @param y0 Start world Y.
         * @param z0 Start world Z.
         * @param x1 End world X.
         * @param y1 End world Y.
         * @param z1 End world Z.
         * @param size Ribbon width in world units (head particle size).
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param a Alpha.
         * @param v0 Texture V at the start (trail slice).
         * @param v1 Texture V at the end.
         */
        void append_aimed_segment(std::vector<engine::SpriteBatchVertex>& vertices,
                                  float x0,
                                  float y0,
                                  float z0,
                                  float x1,
                                  float y1,
                                  float z1,
                                  float size,
                                  float r,
                                  float g,
                                  float b,
                                  float a,
                                  float v0,
                                  float v1) const;

        /**
         * Local particle position at a lifetime fraction (helix or cone/speed).
         * @param stage Parsed stage.
         * @param em_ox Emitter offset X.
         * @param em_oy Emitter offset Y.
         * @param em_oz Emitter offset Z.
         * @param seed Per-particle hash seed.
         * @param frac Lifetime fraction (may be outside 0..1 for trails).
         * @param lx Filled with local X.
         * @param ly Filled with local Y.
         * @param lz Filled with local Z.
         */
        static void stage_local_pos(const SpriteStage& stage,
                                    float em_ox,
                                    float em_oy,
                                    float em_oz,
                                    std::uint32_t seed,
                                    float frac,
                                    float& lx,
                                    float& ly,
                                    float& lz);

        /** One persistent compatibility behavior-side batch for a shared texture and blend mode. */
        class SpriteBatch {
          public:
            /**
             * Creates an empty sprite batch.
             * @param texture Shared texture.
             * @param alpha_blend True for source-alpha blending.
             * @param additive True for additive destination blending.
             */
            SpriteBatch(engine::TextureHandle texture, bool alpha_blend, bool additive)
                : m_texture(texture), m_alpha_blend(alpha_blend), m_additive(additive) {}

            /**
             * Checks whether render state can share this batch.
             * @param texture Candidate texture.
             * @param alpha_blend Candidate source-alpha flag.
             * @param additive Candidate additive flag.
             * @return True when all render state matches.
             */
            [[nodiscard]] bool
            matches(engine::TextureHandle texture, bool alpha_blend, bool additive) const {
                return m_texture.id == texture.id && m_alpha_blend == alpha_blend &&
                       m_additive == additive;
            }

            /** @return Shared sprite texture. */
            [[nodiscard]] engine::TextureHandle texture() const {
                return m_texture;
            }

            /** @return True for source-alpha blending. */
            [[nodiscard]] bool alpha_blend() const {
                return m_alpha_blend;
            }

            /** @return True for additive destination blending. */
            [[nodiscard]] bool additive() const {
                return m_additive;
            }

            /** @return Mutable streamed vertices. */
            [[nodiscard]] std::vector<engine::SpriteBatchVertex>& vertices() {
                return m_vertices;
            }

            /** @return Streamed vertices. */
            [[nodiscard]] const std::vector<engine::SpriteBatchVertex>& vertices() const {
                return m_vertices;
            }

            /** Clears vertices while preserving capacity for the next frame. */
            void clear() {
                m_vertices.clear();
            }

          private:
            /** Shared texture. */
            engine::TextureHandle m_texture{};

            /** Source-alpha blend flag. */
            bool m_alpha_blend = false;

            /** Additive destination blend flag. */
            bool m_additive = false;

            /** Triangle-list vertices retained across frames. */
            std::vector<engine::SpriteBatchVertex> m_vertices;
        };

        /** Client data root. */
        std::string m_data_root;

        /** True once init() has completed. */
        bool m_ready = false;

        /** Raw particle script bodies keyed by shader name. */
        std::unordered_map<std::string, std::string> m_sprite_scripts;

        /** Raw FX script bodies keyed by shader name. */
        std::unordered_map<std::string, std::string> m_fx_scripts;

        /** Parsed particle shaders keyed by name. */
        std::unordered_map<std::string, SpriteShader> m_sprites;

        /** Effect texture cache keyed by file name. */
        std::unordered_map<std::string, engine::TextureHandle> m_tex_cache;

        /** Persistent sprite batches reused to avoid per-frame heap churn. */
        mutable std::vector<SpriteBatch> m_draw_batches;

        /** Live emitters keyed by entity id. */
        std::unordered_map<std::uint32_t, std::vector<Emitter>> m_emitters;

        /** Emitters considered during the most recent draw. */
        mutable std::size_t m_last_draw_emitters = 0;

        /** Emitters rejected by authored cull distance during the most recent draw. */
        mutable std::size_t m_last_culled_emitters = 0;

        /** Quality-dependent multiplier applied to authored cull distances. */
        float m_cull_distance_scale = 1.0f;

        /** Billboard quads submitted during the most recent draw. */
        mutable std::size_t m_last_draw_billboards = 0;

        /** Aimed ribbon quads submitted during the most recent draw. */
        mutable std::size_t m_last_draw_ribbons = 0;

        /** GPU sprite batches submitted during the most recent draw. */
        mutable std::size_t m_last_draw_batches = 0;

        /** One-shot burst keyed in runtime state; expires after remaining seconds. */
        class Oneshot {
          public:
            /** @return Emitter map key. */
            [[nodiscard]] std::uint32_t key() const {
                return m_key;
            }

            /**
             * Sets the emitter map key.
             * @param v Key with bit 30 set.
             */
            void set_key(std::uint32_t v) {
                m_key = v;
            }

            /** @return Seconds until detach. */
            [[nodiscard]] float remaining() const {
                return m_remaining;
            }

            /**
             * Sets remaining lifetime.
             * @param v Seconds.
             */
            void set_remaining(float v) {
                m_remaining = v;
            }

            /** @return Net entity id to follow, or 0 when world-fixed. */
            [[nodiscard]] std::uint32_t follow_entity() const {
                return m_follow_entity;
            }

            /**
             * Sets the net entity the burst should follow.
             * @param v Entity id, or 0 for a world-fixed burst.
             */
            void set_follow_entity(std::uint32_t v) {
                m_follow_entity = v;
            }

          private:
            /** Emitter map key. */
            std::uint32_t m_key = 0;

            /** Seconds until detach. */
            float m_remaining = 0.0f;

            /** Net entity to follow each frame; 0 stays at spawn position. */
            std::uint32_t m_follow_entity = 0;
        };

        /** Live one-shot bursts. */
        std::vector<Oneshot> m_oneshots;

        /** Next one-shot entity key (bit 30 set so it stays off net ids). */
        std::uint32_t m_next_oneshot = 0x40000000u;

        /** Emitter key written by the last successful spawn_oneshot_fx(). */
        std::uint32_t m_last_oneshot_key = 0;

        /** Camera eye X of the last update. */
        float m_cam_x = 0.0f;

        /** Camera eye Y of the last update. */
        float m_cam_y = 0.0f;

        /** Camera eye Z of the last update. */
        float m_cam_z = 0.0f;
    };

}  // namespace openkal::client
