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

#include "openkal/client/AnimSound.h"
#include "openkal/client/ArrowHitFeedback.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/GbModel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    class Audio;
    class SpriteFx;
    enum class SkillEt : std::uint16_t;

    /** Render state of one submesh of a cached skill effect model. */
    class SkillModelPart {
      public:
        /** @return Diffuse texture of the submesh. */
        [[nodiscard]] engine::TextureHandle texture() const {
            return m_texture;
        }

        /**
         * Sets the diffuse texture.
         * @param v Texture handle.
         */
        void set_texture(engine::TextureHandle v) {
            m_texture = v;
        }

        /** @return True when the submesh renders with alpha blending. */
        [[nodiscard]] bool alpha_blend() const {
            return m_alpha_blend;
        }

        /**
         * Sets the alpha blending flag.
         * @param v True to blend.
         */
        void set_alpha_blend(bool v) {
            m_alpha_blend = v;
        }

        /** @return True when the submesh renders with alpha testing. */
        [[nodiscard]] bool alpha_test() const {
            return m_alpha_test;
        }

        /**
         * Sets the alpha testing flag.
         * @param v True to alpha test.
         */
        void set_alpha_test(bool v) {
            m_alpha_test = v;
        }

        /** @return True when the submesh renders with backface culling disabled. */
        [[nodiscard]] bool two_sided() const {
            return m_two_sided;
        }

        /**
         * Sets the two-sided flag.
         * @param v True to disable backface culling.
         */
        void set_two_sided(bool v) {
            m_two_sided = v;
        }

      private:
        /** Diffuse texture of the submesh. */
        engine::TextureHandle m_texture{};

        /** Render with alpha blending. */
        bool m_alpha_blend = false;

        /** Render with alpha testing. */
        bool m_alpha_test = false;

        /** Render without backface culling. */
        bool m_two_sided = true;
    };

    /**
     * Bind-pose cache of one skill effect model (compatibility behavior .gb), shared by every
     * live instance spawned from it.
     */
    class SkillModelCache {
      public:
        /**
         * Loads a skill .gb with its skeleton, first clip and textures.
         * @param engine Engine used to create the textures.
         * @param gb_path Path of the skill .gb mesh.
         * @param tex_cache Texture cache shared across the effect system, keyed by
         * texture name.
         * @return True when the model was loaded and is ready to spawn.
         */
        [[nodiscard]] bool load(engine::IEngine& engine,
                                const std::string& gb_path,
                                std::unordered_map<std::string, engine::TextureHandle>& tex_cache);

        /** @return True once the model is loaded and can be spawned. */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

        /** @return True when instances should face the camera instead of their spawn
         * yaw. */
        [[nodiscard]] bool billboard() const {
            return m_billboard;
        }

        /**
         * Forces the camera-facing flag, overriding the heuristic applied on load.
         * @param v True to make instances face the camera.
         */
        void set_billboard(bool v) {
            m_billboard = v;
        }

        /** @return Bind-pose meshes in load order. */
        [[nodiscard]] const std::vector<terrain::GbMeshCpu>& bind_meshes() const {
            return m_bind_meshes;
        }

        /** @return Render state per bind-pose mesh, parallel to bind_meshes(). */
        [[nodiscard]] const std::vector<SkillModelPart>& parts() const {
            return m_parts;
        }

        /** @return Skeleton bones in skeleton order; empty for static models. */
        [[nodiscard]] const std::vector<terrain::GbBone>& bones() const {
            return m_bones;
        }

        /** @return First animation clip of the model. */
        [[nodiscard]] const terrain::GbAnimClip& anim() const {
            return m_anim;
        }

      private:
        /** Bind-pose meshes in load order. */
        std::vector<terrain::GbMeshCpu> m_bind_meshes;

        /** Render state per bind-pose mesh. */
        std::vector<SkillModelPart> m_parts;

        /** Skeleton bones in skeleton order. */
        std::vector<terrain::GbBone> m_bones;

        /** First animation clip of the model. */
        terrain::GbAnimClip m_anim;

        /** True once the model is loaded. */
        bool m_ready = false;

        /** True when instances should face the camera. */
        bool m_billboard = false;
    };

    /**
     * OpenKal combat presentation: projectiles, procedural ribbons, rings and clip effects.
     *
     * Cast ET_LIGHTNING does NOT use Skill_R_4_1/4_2 as the bolt/impact. It
     * creates: 1) an additive camera-facing polyline (Lightning.GTX), jiggled every
     * frame + UV scroll, 2) the particle "lightning_flare" on EFFECTBONE_EFFECT
     * (jusulsa.txt), 3) the particle "lightning_impact" at the target SPINE1
     * (jusulsa.txt). Prep may also attach Skill_R_4_0 on the wand tip
     * (compatibility behavior).
     */
    class CombatFx {
      public:
        /** Mage fire/ice projectile used for trail particles and crash FX. */
        enum class MageBoltKind : std::uint8_t {
            None = 0,
            Fire,
            Ice,
        };

        /** Crash of a related compatibility states (`OnCrash` + ActionDamage latch). */
        class ProjectileCrash {
          public:
            /** @return Fire vs ice crash. */
            [[nodiscard]] MageBoltKind kind() const {
                return m_kind;
            }

            /**
             * Sets the crash kind.
             * @param v Fire or Ice.
             */
            void set_kind(MageBoltKind v) {
                m_kind = v;
            }

            /** @return Crash world X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /**
             * Sets crash world X.
             * @param v World X.
             */
            void set_x(float v) {
                m_x = v;
            }

            /** @return Crash world Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /**
             * Sets crash world Y.
             * @param v World Y.
             */
            void set_y(float v) {
                m_y = v;
            }

            /** @return Crash world Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /**
             * Sets crash world Z.
             * @param v World Z.
             */
            void set_z(float v) {
                m_z = v;
            }

            /** @return Target entity for attached small fires, or 0. */
            [[nodiscard]] std::uint32_t target_entity() const {
                return m_target_entity;
            }

            /**
             * Sets the follow target.
             * @param v Net entity id.
             */
            void set_target_entity(std::uint32_t v) {
                m_target_entity = v;
            }

            /** @return Monsterbone impact scale used for small-fire scatter. */
            [[nodiscard]] float impact_scale() const {
                return m_impact_scale;
            }

            /**
             * Sets the impact scale.
             * @param v Scale from the target bone height.
             */
            void set_impact_scale(float v) {
                m_impact_scale = v;
            }

          private:
            /** Fire vs ice. */
            MageBoltKind m_kind = MageBoltKind::Fire;

            /** Crash world X. */
            float m_x = 0.0f;

            /** Crash world Y. */
            float m_y = 0.0f;

            /** Crash world Z. */
            float m_z = 0.0f;

            /** Target entity for attached particles. */
            std::uint32_t m_target_entity = 0;

            /** Monsterbone impact scale. */
            float m_impact_scale = 1.0f;
        };

        /**
         * Loads the effect textures, the shared quads, the arrow mesh and the prep
         * skill model.
         * @param engine Engine used to create the meshes and textures.
         * @param data_root Client data root.
         * @return True once the effect system can draw.
         */
        [[nodiscard]] bool init(engine::IEngine& engine, const std::string& data_root);

        /**
         * Attaches the SFX mixer used by skill-model keyframes and projectile cues.
         * @param audio Mixer; nullptr disables combat SFX.
         */
        void set_audio(Audio* audio);

        /**
         * Attaches the sprite shader manager used for bolt trails and crash bursts.
         * @param sprite_fx Particle/FX manager; nullptr disables named particles.
         */
        void set_sprite_fx(SpriteFx* sprite_fx);

        /**
         * Plays a named WAV at a world position (no-op without a mixer).
         * @param name Stem or `_L` token.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void play_sfx(std::string_view name, float x, float y, float z);

        /** compatibility behavior texture / particle family. */
        enum class LightningKind : std::uint8_t {
            /** ET_LIGHTNING / ET_SPARK (Lightning.dds, lightning_flare). */
            Yellow = 0,
            /** ET_MANA_BURN / ET_LIGHTNING_RED (LightningRed.dds). */
            Red,
            /** ET_SPIRIT_BLAST (Lightning02.dds with EF106 caster particles). */
            Spirit,
        };

        /**
         * Spawns a lightning cast, matching the compatibility behavior particle spawn
         * and polyline.
         * @param engine Engine used to create the prep meshes.
         * @param tip_x Cast origin X (wand tip).
         * @param tip_y Cast origin Y.
         * @param tip_z Cast origin Z.
         * @param bx Impact point X (target SPINE1).
         * @param by Impact point Y.
         * @param bz Impact point Z.
         * @param impact_scale Reserved impact scale from the monsterbone target
         * height.
         * @param beam_count compatibility behavior `runtime state` (1 for ET_LIGHTNING, 2 for
         * ET_SPARK).
         * @param kind Yellow mage bolt versus red Mana Burn assets.
         */
        void spawn_lightning(engine::IEngine& engine,
                             float tip_x,
                             float tip_y,
                             float tip_z,
                             float bx,
                             float by,
                             float bz,
                             float impact_scale = 1.0f,
                             int beam_count = 1,
                             LightningKind kind = LightningKind::Yellow);

        /**
         * Spawns a flying ice bolt (`compatibility behavior`: Skill_R_3_1 at 500 u/s, wand tip →
         * SPINE1).
         * @param engine Engine used to create the projectile meshes.
         * @param tip_x Cast origin X (wand tip).
         * @param tip_y Cast origin Y.
         * @param tip_z Cast origin Z.
         * @param bx Impact point X (target SPINE1).
         * @param by Impact point Y.
         * @param bz Impact point Z.
         * @param target_entity Net id used to attach crash particles, or 0.
         * @param impact_scale Monsterbone scale for small-fire scatter.
         */
        void spawn_ice_bolt(engine::IEngine& engine,
                            float tip_x,
                            float tip_y,
                            float tip_z,
                            float bx,
                            float by,
                            float bz,
                            std::uint32_t target_entity = 0,
                            float impact_scale = 1.0f);

        /**
         * Spawns a flying fire bolt (`compatibility behavior`: Skill_R_2_1 wand tip → SPINE1).
         * @param engine Engine used to create the projectile meshes.
         * @param tip_x Cast origin X (wand tip).
         * @param tip_y Cast origin Y.
         * @param tip_z Cast origin Z.
         * @param bx Impact point X (target SPINE1).
         * @param by Impact point Y.
         * @param bz Impact point Z.
         * @param target_entity Net id used to attach crash particles, or 0.
         * @param impact_scale Monsterbone scale for small-fire scatter.
         */
        void spawn_fire_bolt(engine::IEngine& engine,
                             float tip_x,
                             float tip_y,
                             float tip_z,
                             float bx,
                             float by,
                             float bz,
                             std::uint32_t target_entity = 0,
                             float impact_scale = 1.0f);

        /**
         * Spawns a flying arrow between two world positions (bow tip → target spine).
         * @param effect Archer projectile effect type.
         * @param ax Origin X.
         * @param ay Origin Y.
         * @param az Origin Z.
         * @param bx Impact X.
         * @param by Impact Y.
         * @param bz Impact Z.
         * @param extra Mysterious Arrow parabola extra (0 / 1/3 / 2/3).
         * @param target_entity Net id for the impact hit sprite and stuck mesh, or 0.
         * @param flags Compatibility behavior `runtime state` (bit 2 selects effect_attack02_c).
         * @param impact_scale Monsterbone `target[3] * scale / 16` used to scatter
         *        the stuck arrow in spine-local space.
         * @param attacker_entity Normal-arrow shooter ID used to synchronize confirmed hit
         * feedback.
         */
        void spawn_arrow(SkillEt effect,
                         float ax,
                         float ay,
                         float az,
                         float bx,
                         float by,
                         float bz,
                         float extra = 0.0f,
                         std::uint32_t target_entity = 0,
                         std::uint8_t flags = 1,
                         float impact_scale = 1.0f,
                         std::uint32_t attacker_entity = 0);

        /**
         * Spawns an untyped normal arrow for legacy attack call sites.
         * @param ax Origin X.
         * @param ay Origin Y.
         * @param az Origin Z.
         * @param bx Impact X.
         * @param by Impact Y.
         * @param bz Impact Z.
         */
        void spawn_arrow(float ax, float ay, float az, float bx, float by, float bz);

        /**
         * Spawns compatibility behavior: invisible 200 u/s projectile from the target spine
         * back to the caster (`projectile initialization` inverse).
         * @param ax Target spine X.
         * @param ay Target spine Y.
         * @param az Target spine Z.
         * @param bx Caster effect-bone X.
         * @param by Caster effect-bone Y.
         * @param bz Caster effect-bone Z.
         */
        void spawn_vitality_drain(float ax, float ay, float az, float bx, float by, float bz);

        /**
         * Spawns the devil-troop spit cloud (`compatibility behavior`: `ghost_blue`,
         * 100 u/s, spine → target spine).
         * @param ax Origin X (caster EFFECTBONE from the GB key).
         * @param ay Origin Y.
         * @param az Origin Z.
         * @param bx Initial home X (target SPINE1).
         * @param by Initial home Y.
         * @param bz Initial home Z.
         */
        void spawn_devil_spirit_spit(float ax, float ay, float az, float bx, float by, float bz);

        /**
         * Updates the live home point for homing projectiles (`compatibility behavior`
         * re-reads the target bone every tick).
         * @param x Target X.
         * @param y Target Y.
         * @param z Target Z.
         */
        void set_homing_target(float x, float y, float z);

        /**
         * Hit SFX for a confirmed attack. Texture tint is applied on the monster.
         * @param engine Unused; kept for call-site compatibility.
         * @param x Impact X.
         * @param y Impact Y.
         * @param z Impact Z.
         * @param flags S2C_ATTACK flags (hit / fatal / explosive).
         * @param scale Unused; kept for call-site compatibility.
         * @param arrow True to play efc007 (compatibility behavior); false for melee
         * (hit SFX is animation-keyed on the actor).
         */
        void spawn_hit(engine::IEngine& engine,
                       float x,
                       float y,
                       float z,
                       std::uint8_t flags,
                       float scale = 1.0f,
                       bool arrow = false);

        /**
         * Spawns the level-up effect attached to a moving character.
         * @param engine Engine used to create instance meshes.
         * @param x World X (player feet).
         * @param y World Y.
         * @param z World Z.
         * @param follow True to re-position every frame via follow_attached().
         * @param life_ms Playback window; the clip is time-scaled to fit.
         * @param entity Optional recipient followed through character bone updates.
         */
        void spawn_level_up(engine::IEngine& engine,
                            float x,
                            float y,
                            float z,
                            bool follow = true,
                            float life_ms = 1200.0f,
                            std::uint32_t entity = 0);

        /**
         * Spawns effect_behead at a world position when the GB is present.
         * @param engine Engine used to create instance meshes.
         * @param x World X (head).
         * @param y World Y.
         * @param z World Z.
         */
        void spawn_behead(engine::IEngine& engine, float x, float y, float z);

        /**
         * Spawns a clip `(*EFFECT_MODEL)` mesh (`compatibility behavior`
         * `AddTarget`). Uses `Skill/{name}.gb` when that file exists, otherwise
         * `Effect/{name}.gb`.
         * @param engine Engine used to create instance meshes.
         * @param name Keyframe stem (`skill_w_3_1`, `effect_attack01`).
         * @param x World X (impact / effect bone).
         * @param y World Y.
         * @param z World Z.
         * @param yaw Facing angle in radians (character yaw).
         * @param delay_s Seconds to wait before spawning (FX stage `delay`).
         * @param duration_s FX stage duration in seconds, or 0 to use the clip length.
         * @param follow_entity Net id to re-position each frame, or 0.
         * @param persist True to keep the last frame until kill_clip_entity().
         * @param loop True to wrap the clip until kill_clip_entity() (FX `restart`).
         * @param effect_bone EFFECTBONE slot to follow, or -1 for the character origin.
         * @param ox FX stage offset X.
         * @param oy FX stage offset Y.
         * @param oz FX stage offset Z.
         * @param use_effect_bone_rotation True to copy the EFFECTBONE matrix.
         */
        void spawn_clip_model(engine::IEngine& engine,
                              std::string_view name,
                              float x,
                              float y,
                              float z,
                              float yaw,
                              float delay_s = 0.0f,
                              float duration_s = 0.0f,
                              std::uint32_t follow_entity = 0,
                              bool persist = false,
                              bool loop = false,
                              int effect_bone = -1,
                              float ox = 0.0f,
                              float oy = 0.0f,
                              float oz = 0.0f,
                              bool use_effect_bone_rotation = false);

        /**
         * Starts a looping FX-stage sound attached to a clip follow entity.
         * @param entity Key passed to spawn_clip_model / kill_clip_entity.
         * @param name Sound stem (e.g. SE010009).
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void
        play_clip_loop_sfx(std::uint32_t entity, std::string_view name, float x, float y, float z);

        /**
         * Moves clip models spawned with a follow entity (Cold Binding ice).
         * @param entity Net entity id passed to spawn_clip_model.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void follow_clip_entity(std::uint32_t entity, float x, float y, float z);

        /**
         * Moves and orients clip models spawned with a follow entity.
         * @param entity Net entity id passed to spawn_clip_model.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param yaw Facing angle in radians.
         */
        void follow_clip_entity(std::uint32_t entity, float x, float y, float z, float yaw);

        /**
         * Rebinds followed clip models with compatibility behavior matrices.
         * @param entity Net entity id passed to spawn_clip_model.
         * @param origin Character world matrix.
         * @param effect_bones EFFECTBONE world matrices, or nullptr.
         * @param effect_bone_count Length of effect_bones.
         */
        void follow_clip_bones(std::uint32_t entity,
                               const engine::Mat4& origin,
                               const engine::Mat4* effect_bones,
                               int effect_bone_count);

        /**
         * Parents crashed compatibility behavior meshes to EFFECTBONE_SPINE1 (`runtime state`).
         * @param entity Net id stored on the arrow as `target_entity`.
         * @param spine Current spine-1 world matrix.
         */
        void follow_arrows(std::uint32_t entity, const engine::Mat4& spine);

        /**
         * Kills clip models spawned with a follow entity.
         * @param entity Net entity id passed to spawn_clip_model.
         */
        void kill_clip_entity(std::uint32_t entity);

        /**
         * Attaches a looping PRESKILL model to a moving bone (`runtime state`).
         * @param engine Engine used to create instance meshes.
         * @param x Attach X (EFFECTBONE_EFFECT).
         * @param y Attach Y.
         * @param z Attach Z.
         * @param yaw Character facing in radians (not world-fixed).
         * @param entity Caster whose preparation effect is replaced.
         * @param model_name JSON `effect[0]` stem (`Skill_M_6_0`). Empty uses
         * `Skill_M_3_0` (compatibility behavior / ChargedArrowPrep).
         */
        void spawn_cast_fx(engine::IEngine& engine,
                           float x,
                           float y,
                           float z,
                           float yaw,
                           std::string_view model_name = {},
                           std::uint32_t entity = 0);

        /**
         * Repositions one caster's PRESKILL follow instances.
         * @param x Attach X.
         * @param y Attach Y.
         * @param z Attach Z.
         * @param entity Caster entity id.
         * @param yaw Character facing in radians.
         */
        void follow_cast_fx(float x, float y, float z, float yaw, std::uint32_t entity = 0);

        /** Marks one caster's PRESKILL instances dead (meshes freed on update).
         * @param entity Caster entity id.
         */
        void clear_cast_fx(std::uint32_t entity = 0);

        /**
         * Spawns the compatibility behavior DrawDonut (healing.dds, 2s, additive).
         * @param x Target origin X.
         * @param y Target origin Y.
         * @param z Target origin Z.
         * @param follow_entity Net entity to track, or 0 to stay at spawn.
         */
        void spawn_heal_donut(float x, float y, float z, std::uint32_t follow_entity);

        /**
         * Repositions live heal donuts attached to a moving target.
         * @param entity Target entity key.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void follow_heal_donut(std::uint32_t entity, float x, float y, float z);

        /**
         * Spawns compatibility behavior DrawDonut (`whiteres.dds`, 0.5 s, expanding ring).
         * @param x Target origin X (`runtime state._41`).
         * @param y Target origin Y.
         * @param z Target origin Z.
         * @param follow_entity Net entity to track, or 0 to stay at spawn.
         */
        void spawn_ice_nova(float x, float y, float z, std::uint32_t follow_entity);

        /**
         * Repositions live Ice Nova donuts attached to a moving target.
         * @param entity Target entity key.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void follow_ice_nova(std::uint32_t entity, float x, float y, float z);

        /**
         * Spawns compatibility behavior: 3 additive lig01 polylines, 3 s, around the caster
         * spine.
         * @param x Spine X.
         * @param y Spine Y.
         * @param z Spine Z.
         * @param follow_entity Caster net id, or 0 to stay at spawn.
         */
        void spawn_stone_shield_prep(float x, float y, float z, std::uint32_t follow_entity);

        /**
         * Repositions live Stone Shield prep ribbons attached to a moving caster.
         * @param entity Caster entity key.
         * @param x Spine X.
         * @param y Spine Y.
         * @param z Spine Z.
         */
        void follow_stone_shield_prep(std::uint32_t entity, float x, float y, float z);

        /**
         * Spawns compatibility behavior: 2 additive GenericGlow polylines, 10 s, around the
         * caster spine. Not Skill_M_3_0 (that is archer compatibility behavior / Staggering Blow).
         * @param x Spine X.
         * @param y Spine Y.
         * @param z Spine Z.
         * @param follow_entity Caster net id, or 0 to stay at spawn.
         */
        void spawn_meditation(float x, float y, float z, std::uint32_t follow_entity);

        /**
         * Repositions live Meditation ribbons attached to a moving caster.
         * @param entity Caster entity key.
         * @param x Spine X.
         * @param y Spine Y.
         * @param z Spine Z.
         */
        void follow_meditation(std::uint32_t entity, float x, float y, float z);

        /**
         * Moves every following skill instance (level-up FX) to a world position.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void follow_attached(float x, float y, float z);

        /**
         * Advances every live effect and re-skins the running skill instances.
         * @param engine Engine owning the instance meshes.
         * @param dt Frame delta in seconds.
         * @param camera_yaw_rad Camera yaw in radians, used to orient billboards.
         * @param eye_x Camera eye X.
         * @param eye_y Camera eye Y.
         * @param eye_z Camera eye Z.
         */
        void update(engine::IEngine& engine,
                    float dt,
                    float camera_yaw_rad,
                    float eye_x,
                    float eye_y,
                    float eye_z);

        /**
         * Draws every live effect: prep meshes, lightning ribbons, spit clouds,
         * particles and arrows.
         * @param engine Engine used to submit the draws.
         */
        void draw(engine::IEngine& engine);

        /** @return True once init() has completed. */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

        /** @return Number of live lightning bolts. */
        [[nodiscard]] std::size_t bolt_count() const {
            return m_bolts.size();
        }

        /** @return Number of live particles. */
        [[nodiscard]] std::size_t particle_count() const {
            return m_particles.size();
        }

        /** @return Number of arrows still in flight. */
        [[nodiscard]] std::size_t arrow_count() const {
            return m_arrows.size();
        }

        /** @return Number of running skill model instances. */
        [[nodiscard]] std::size_t skill_anim_count() const {
            return m_skill_anims.size();
        }

        /**
         * Pops one fire/ice bolt crash (ActionDamage latch + crash particles).
         * @param out Receives the crash.
         * @return True when a crash was pending.
         */
        [[nodiscard]] bool take_projectile_crash(ProjectileCrash& out);

        /** Records a normal Archer attack result; misses must also be supplied.
         * @param attacker Shooter entity ID.
         * @param target Target entity ID.
         * @param flags Server attack flags.
         */
        void note_arrow_result(std::uint32_t attacker, std::uint32_t target, std::uint8_t flags);

        /** Consumes one confirmed normal-arrow outcome after its actual projectile impact.
         * @param out Receives attacker, target, flags and world-space endpoints.
         * @return True when feedback is ready.
         */
        [[nodiscard]] bool take_arrow_feedback(ArrowHitFeedback::Impact& out);

      private:
        /** One lightning polyline between a cast origin and an impact point. */
        class Bolt {
          public:
            /**
             * Creates a bolt with a full lifetime.
             * @param ax Origin X.
             * @param ay Origin Y.
             * @param az Origin Z.
             * @param bx Impact X.
             * @param by Impact Y.
             * @param bz Impact Z.
             * @param segments Number of polyline points.
             * @param kind Lightning visual family.
             */
            Bolt(float ax,
                 float ay,
                 float az,
                 float bx,
                 float by,
                 float bz,
                 int segments,
                 LightningKind kind = LightningKind::Yellow);

            /**
             * Ages the bolt.
             * @param dt Frame delta in seconds.
             */
            void advance(float dt) {
                m_age += dt;
            }

            /** @return True once the bolt outlived its lifetime. */
            [[nodiscard]] bool expired() const {
                return m_age >= m_life;
            }

            /** @return Origin X. */
            [[nodiscard]] float ax() const {
                return m_ax;
            }

            /** @return Origin Y. */
            [[nodiscard]] float ay() const {
                return m_ay;
            }

            /** @return Origin Z. */
            [[nodiscard]] float az() const {
                return m_az;
            }

            /** @return Impact X. */
            [[nodiscard]] float bx() const {
                return m_bx;
            }

            /** @return Impact Y. */
            [[nodiscard]] float by() const {
                return m_by;
            }

            /** @return Impact Z. */
            [[nodiscard]] float bz() const {
                return m_bz;
            }

            /** @return Seconds since the bolt was spawned. */
            [[nodiscard]] float age() const {
                return m_age;
            }

            /** @return Total lifetime in seconds. */
            [[nodiscard]] float life() const {
                return m_life;
            }

            /** @return Number of polyline points. */
            [[nodiscard]] int segments() const {
                return m_segments;
            }

            /** @return Lightning visual family. */
            [[nodiscard]] LightningKind kind() const {
                return m_kind;
            }

          private:
            /** Origin X. */
            float m_ax = 0.0f;

            /** Origin Y. */
            float m_ay = 0.0f;

            /** Origin Z. */
            float m_az = 0.0f;

            /** Impact X. */
            float m_bx = 0.0f;

            /** Impact Y. */
            float m_by = 0.0f;

            /** Impact Z. */
            float m_bz = 0.0f;

            /** Seconds since spawn. */
            float m_age = 0.0f;

            /** Total lifetime in seconds. */
            float m_life = 1.0f;

            /** Number of polyline points. */
            int m_segments = 8;

            /** Ribbon texture and particle family. */
            LightningKind m_kind = LightningKind::Yellow;
        };

        /** Compatibility behavior DrawDonut: additive rising cylinder, 2s, follows target. */
        class HealDonut {
          public:
            /**
             * Creates a heal donut at the target origin.
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             * @param follow_entity Net entity to track, or 0.
             */
            HealDonut(float x, float y, float z, std::uint32_t follow_entity);

            /**
             * Ages the donut.
             * @param dt Frame delta in seconds.
             */
            void advance(float dt) {
                m_age += dt;
            }

            /**
             * Updates the world origin while the target moves.
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             */
            void set_world_pos(float x, float y, float z) {
                m_x = x;
                m_y = y;
                m_z = z;
            }

            /** @return True while the donut is still drawn. */
            [[nodiscard]] bool alive() const {
                return m_age < m_life;
            }

            /** @return World X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return World Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return World Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return Seconds since spawn. */
            [[nodiscard]] float age() const {
                return m_age;
            }

            /** @return Total lifetime in seconds. */
            [[nodiscard]] float life() const {
                return m_life;
            }

            /** @return Follow entity, or 0. */
            [[nodiscard]] std::uint32_t follow_entity() const {
                return m_follow_entity;
            }

          private:
            /** World X. */
            float m_x = 0.0f;

            /** World Y. */
            float m_y = 0.0f;

            /** World Z. */
            float m_z = 0.0f;

            /** Seconds since spawn. */
            float m_age = 0.0f;

            /** Total lifetime in seconds (compatibility behavior runtime state). */
            float m_life = 2.0f;

            /** Target entity to follow, or 0. */
            std::uint32_t m_follow_entity = 0;
        };

        /** Compatibility behavior DrawDonut: additive expanding ring, 0.5 s, follows target. */
        class IceNova {
          public:
            /**
             * Creates an Ice Nova at the target origin.
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             * @param follow_entity Net entity to track, or 0.
             */
            IceNova(float x, float y, float z, std::uint32_t follow_entity);

            /**
             * Ages the nova.
             * @param dt Frame delta in seconds.
             */
            void advance(float dt) {
                m_age += dt;
            }

            /**
             * Updates the world origin while the target moves.
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             */
            void set_world_pos(float x, float y, float z) {
                m_x = x;
                m_y = y;
                m_z = z;
            }

            /** @return True while the nova is still drawn. */
            [[nodiscard]] bool alive() const {
                return m_age < m_life;
            }

            /** @return World X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return World Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return World Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return Seconds since spawn. */
            [[nodiscard]] float age() const {
                return m_age;
            }

            /** @return Total lifetime in seconds. */
            [[nodiscard]] float life() const {
                return m_life;
            }

            /** @return Follow entity, or 0. */
            [[nodiscard]] std::uint32_t follow_entity() const {
                return m_follow_entity;
            }

          private:
            /** World X. */
            float m_x = 0.0f;

            /** World Y. */
            float m_y = 0.0f;

            /** World Z. */
            float m_z = 0.0f;

            /** Seconds since spawn. */
            float m_age = 0.0f;

            /** Total lifetime in seconds (`compatibility behavior`). */
            float m_life = 0.5f;

            /** Target entity to follow, or 0. */
            std::uint32_t m_follow_entity = 0;
        };

        /** Compatibility behavior: 3 rotating additive polylines, 3 s. */
        class StoneShieldPrep {
          public:
            /** Number of orbiting ribbons. */
            static constexpr int kRibbons = 3;

            /** Samples per ribbon (`runtime state[0]`). */
            static constexpr int kPoints = 16;

            /**
             * Creates a Stone Shield prep at the caster spine.
             * @param x Spine X.
             * @param y Spine Y.
             * @param z Spine Z.
             * @param follow_entity Caster net id, or 0.
             */
            StoneShieldPrep(float x, float y, float z, std::uint32_t follow_entity);

            /**
             * Ages the effect and spins the ribbon angles (`FrameMove`).
             * @param dt Frame delta in seconds.
             */
            void advance(float dt);

            /**
             * Updates the spine origin while the caster moves.
             * @param x Spine X.
             * @param y Spine Y.
             * @param z Spine Z.
             */
            void set_world_pos(float x, float y, float z) {
                m_x = x;
                m_y = y;
                m_z = z;
            }

            /** @return True while the 3 s window is open. */
            [[nodiscard]] bool alive() const {
                return m_age < m_life;
            }

            /** @return Spine X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return Spine Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return Spine Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return Seconds since spawn. */
            [[nodiscard]] float age() const {
                return m_age;
            }

            /** @return Total lifetime in seconds. */
            [[nodiscard]] float life() const {
                return m_life;
            }

            /** @return Follow entity, or 0. */
            [[nodiscard]] std::uint32_t follow_entity() const {
                return m_follow_entity;
            }

            /**
             * Returns one ribbon sample in world space after shifting the trail.
             * @param ribbon Ribbon index 0..2.
             * @param xs Receives X samples.
             * @param ys Receives Y samples.
             * @param zs Receives Z samples.
             * @param as Receives alpha samples.
             */
            void build_ribbon(int ribbon,
                              std::array<float, kPoints>& xs,
                              std::array<float, kPoints>& ys,
                              std::array<float, kPoints>& zs,
                              std::array<float, kPoints>& as);

          private:
            /** Local X of each ribbon sample (before adding spine). */
            std::array<std::array<float, kPoints>, kRibbons> m_px{};

            /** Local Y of each ribbon sample. */
            std::array<std::array<float, kPoints>, kRibbons> m_py{};

            /** Local Z of each ribbon sample. */
            std::array<std::array<float, kPoints>, kRibbons> m_pz{};

            /** Per-ribbon spin angle in radians (`runtime state`). */
            std::array<float, kRibbons> m_angles{};

            /** Spine X. */
            float m_x = 0.0f;

            /** Spine Y. */
            float m_y = 0.0f;

            /** Spine Z. */
            float m_z = 0.0f;

            /** Seconds since spawn. */
            float m_age = 0.0f;

            /** Total lifetime in seconds (`runtime state`). */
            float m_life = 3.0f;

            /** Caster entity to follow, or 0. */
            std::uint32_t m_follow_entity = 0;
        };

        /** Compatibility behavior: 2 additive GenericGlow polylines, 10 s, around spine. */
        class MeditationFx {
          public:
            /** Number of opposing ribbons (`runtime state[2]`). */
            static constexpr int kRibbons = 2;

            /** Samples per ribbon (`runtime state`). */
            static constexpr int kPoints = 16;

            /**
             * Creates Meditation at the caster spine.
             * @param x Spine X.
             * @param y Spine Y.
             * @param z Spine Z.
             * @param follow_entity Caster net id, or 0.
             */
            MeditationFx(float x, float y, float z, std::uint32_t follow_entity);

            /**
             * Ages the effect and spins the ribbon angles (`FrameMove`, `time * 8`).
             * @param dt Frame delta in seconds.
             */
            void advance(float dt);

            /**
             * Updates the spine origin while the caster moves.
             * @param x Spine X.
             * @param y Spine Y.
             * @param z Spine Z.
             */
            void set_world_pos(float x, float y, float z) {
                m_x = x;
                m_y = y;
                m_z = z;
            }

            /** @return True while the 10 s window is open. */
            [[nodiscard]] bool alive() const {
                return m_age < m_life;
            }

            /** @return Spine X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return Spine Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return Spine Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return Follow entity, or 0. */
            [[nodiscard]] std::uint32_t follow_entity() const {
                return m_follow_entity;
            }

            /**
             * Returns one ribbon sample in world space after shifting the trail.
             * @param ribbon Ribbon index 0..1.
             * @param xs Receives X samples.
             * @param ys Receives Y samples.
             * @param zs Receives Z samples.
             * @param as Receives alpha samples.
             */
            void build_ribbon(int ribbon,
                              std::array<float, kPoints>& xs,
                              std::array<float, kPoints>& ys,
                              std::array<float, kPoints>& zs,
                              std::array<float, kPoints>& as);

          private:
            /** Local X of each ribbon sample (before adding spine). */
            std::array<std::array<float, kPoints>, kRibbons> m_px{};

            /** Local Y of each ribbon sample. */
            std::array<std::array<float, kPoints>, kRibbons> m_py{};

            /** Local Z of each ribbon sample. */
            std::array<std::array<float, kPoints>, kRibbons> m_pz{};

            /** Per-ribbon spin angle in radians (`runtime state`). */
            std::array<float, kRibbons> m_angles{};

            /** Spine X. */
            float m_x = 0.0f;

            /** Spine Y. */
            float m_y = 0.0f;

            /** Spine Z. */
            float m_z = 0.0f;

            /** Seconds since spawn. */
            float m_age = 0.0f;

            /** Total lifetime in seconds (`runtime state`). */
            float m_life = 10.0f;

            /** Caster entity to follow, or 0. */
            std::uint32_t m_follow_entity = 0;
        };

        /** View-oriented additive particle (subset of the jusulsa sprite layers). */
        class Particle {
          public:
            /**
             * Creates a particle at full opacity.
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             * @param vx Velocity X in units per second.
             * @param vy Velocity Y in units per second.
             * @param vz Velocity Z in units per second.
             * @param life Lifetime in seconds.
             * @param size0 Size at spawn.
             * @param size1 Size at death.
             * @param r Red tint.
             * @param g Green tint.
             * @param b Blue tint.
             * @param a0 Alpha at spawn.
             * @param rot Initial roll in radians.
             * @param rot_speed Roll speed in radians per second.
             * @param tex Sprite texture.
             */
            Particle(float x,
                     float y,
                     float z,
                     float vx,
                     float vy,
                     float vz,
                     float life,
                     float size0,
                     float size1,
                     float r,
                     float g,
                     float b,
                     float a0,
                     float rot,
                     float rot_speed,
                     engine::TextureHandle tex);

            /**
             * Integrates position and roll, and kills the particle once its lifetime
             * runs out.
             * @param dt Frame delta in seconds.
             */
            void advance(float dt);

            /** @return True while the particle is still simulated and drawn. */
            [[nodiscard]] bool alive() const {
                return m_alive;
            }

            /** @return World X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return World Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return World Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return Seconds since spawn. */
            [[nodiscard]] float age() const {
                return m_age;
            }

            /** @return Lifetime in seconds. */
            [[nodiscard]] float life() const {
                return m_life;
            }

            /** @return Size at spawn. */
            [[nodiscard]] float size0() const {
                return m_size0;
            }

            /** @return Size at death. */
            [[nodiscard]] float size1() const {
                return m_size1;
            }

            /** @return Red tint. */
            [[nodiscard]] float r() const {
                return m_r;
            }

            /** @return Green tint. */
            [[nodiscard]] float g() const {
                return m_g;
            }

            /** @return Blue tint. */
            [[nodiscard]] float b() const {
                return m_b;
            }

            /** @return Alpha at spawn. */
            [[nodiscard]] float a0() const {
                return m_a0;
            }

            /** @return Current roll in radians. */
            [[nodiscard]] float rot() const {
                return m_rot;
            }

            /** @return Sprite texture. */
            [[nodiscard]] engine::TextureHandle tex() const {
                return m_tex;
            }

          private:
            /** World X. */
            float m_x = 0.0f;

            /** World Y. */
            float m_y = 0.0f;

            /** World Z. */
            float m_z = 0.0f;

            /** Velocity X in units per second. */
            float m_vx = 0.0f;

            /** Velocity Y in units per second. */
            float m_vy = 0.0f;

            /** Velocity Z in units per second. */
            float m_vz = 0.0f;

            /** Seconds since spawn. */
            float m_age = 0.0f;

            /** Lifetime in seconds. */
            float m_life = 1.0f;

            /** Size at spawn. */
            float m_size0 = 1.0f;

            /** Size at death. */
            float m_size1 = 1.0f;

            /** Red tint. */
            float m_r = 1.0f;

            /** Green tint. */
            float m_g = 1.0f;

            /** Blue tint. */
            float m_b = 1.0f;

            /** Alpha at spawn. */
            float m_a0 = 1.0f;

            /** Current roll in radians. */
            float m_rot = 0.0f;

            /** Roll speed in radians per second. */
            float m_rot_speed = 0.0f;

            /** Sprite texture. */
            engine::TextureHandle m_tex{};

            /** False once the lifetime ran out. */
            bool m_alive = true;
        };

        /** Straight-flying arrow projectile. */
        class Arrow {
          public:
            /** Number of samples in the ghost_grey ribbon. */
            static constexpr int kTrailPoints = 16;

            /**
             * Creates an arrow flying towards a fixed point.
             * @param effect Archer projectile effect type.
             * @param x Start X.
             * @param y Start Y.
             * @param z Start Z.
             * @param tx Destination X.
             * @param ty Destination Y.
             * @param tz Destination Z.
             * @param extra Mysterious Arrow parabola extra.
             * @param target_entity Impact follow entity, or 0.
             * @param flags Compatibility behavior runtime state.
             * @param visible False for compatibility behavior (Render is empty).
             * @param impact_scale Stuck-arrow scatter scale (`target[3] / 16`).
             * @param feedback_token Normal-arrow confirmation/arrival matching token.
             */
            Arrow(SkillEt effect,
                  float x,
                  float y,
                  float z,
                  float tx,
                  float ty,
                  float tz,
                  float extra = 0.0f,
                  std::uint32_t target_entity = 0,
                  std::uint8_t flags = 1,
                  bool visible = true,
                  float impact_scale = 1.0f,
                  std::uint64_t feedback_token = 0);

            /**
             * Moves the arrow towards its destination and kills it on arrival.
             * @param dt Frame delta in seconds.
             */
            void step(float dt);

            /** @return True while the arrow is in flight. */
            [[nodiscard]] bool alive() const {
                return m_alive;
            }

            /** @return True after the arrow reached its target. */
            [[nodiscard]] bool crashed() const {
                return m_crashed;
            }

            /** @return Archer projectile effect type. */
            [[nodiscard]] SkillEt effect() const {
                return m_effect;
            }

            /** @return Ribbon red tint. */
            [[nodiscard]] float r() const {
                return m_r;
            }

            /** @return Ribbon green tint. */
            [[nodiscard]] float g() const {
                return m_g;
            }

            /** @return Ribbon blue tint. */
            [[nodiscard]] float b() const {
                return m_b;
            }

            /** @return True for ONE/ONE; false for silent SRC_ALPHA/INV_SRC_ALPHA. */
            [[nodiscard]] bool additive() const {
                return m_additive;
            }

            /** @return True once when impact audio should be emitted. */
            [[nodiscard]] bool take_impact();

            /**
             * Returns one ribbon sample.
             * @param index Sample index, zero is the projectile head.
             * @param ox Receives X.
             * @param oy Receives Y.
             * @param oz Receives Z.
             * @param oa Receives alpha.
             */
            void trail_point(int index, float& ox, float& oy, float& oz, float& oa) const;

            /** @return Current X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return Current Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return Current Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return Destination X. */
            [[nodiscard]] float tx() const {
                return m_tx;
            }

            /** @return Destination Y. */
            [[nodiscard]] float ty() const {
                return m_ty;
            }

            /** @return Destination Z. */
            [[nodiscard]] float tz() const {
                return m_tz;
            }

            /**
             * @return Yaw in radians from the last in-flight dest-pos
             * (`compatibility behavior`). Kept after crash so the embedded mesh does not
             * snap to identity (south-north).
             */
            [[nodiscard]] float yaw() const {
                return m_yaw;
            }

            /**
             * @return Pitch in radians from the last in-flight dest-pos.
             */
            [[nodiscard]] float pitch() const {
                return m_pitch;
            }

            /** @return Mysterious Arrow extra (0 for other arrows). */
            [[nodiscard]] float extra() const {
                return m_extra;
            }

            /** @return Impact follow entity, or 0. */
            [[nodiscard]] std::uint32_t target_entity() const {
                return m_target_entity;
            }

            /** @return Normal-arrow confirmation/arrival matching token, or zero. */
            [[nodiscard]] std::uint64_t feedback_token() const {
                return m_feedback_token;
            }

            /** @return Compatibility behavior runtime state. */
            [[nodiscard]] std::uint8_t flags() const {
                return m_flags;
            }

            /** @return False for compatibility behavior (no mesh or ribbon). */
            [[nodiscard]] bool visible() const {
                return m_visible;
            }

            /** Parents the crashed mesh to the live spine bone (`compatibility behavior` /
             * `Render`).
             * @param spine Current EFFECTBONE_SPINE1 world matrix.
             */
            void follow_spine(const engine::Mat4& spine);

            /** @return True after the crashed mesh is parented to the spine bone. */
            [[nodiscard]] bool stuck() const {
                return m_attached;
            }

            /** World matrix of a stuck arrow (`runtime state * spine`).
             * @return Column-major model matrix used for drawing.
             */
            [[nodiscard]] const engine::Mat4& world_matrix() const {
                return m_world;
            }

          private:
            /** Archer projectile effect type. */
            SkillEt m_effect;

            /** Current X. */
            float m_x = 0.0f;

            /** Current Y. */
            float m_y = 0.0f;

            /** Current Z. */
            float m_z = 0.0f;

            /** Destination X. */
            float m_tx = 0.0f;

            /** Destination Y. */
            float m_ty = 0.0f;

            /** Destination Z. */
            float m_tz = 0.0f;

            /** Yaw in radians from last in-flight dest-pos (`compatibility behavior`). */
            float m_yaw = 0.0f;

            /** Pitch in radians from last in-flight dest-pos. */
            float m_pitch = 0.0f;

            /** Linear flight start X (`runtime state`). */
            float m_src_x = 0.0f;

            /** Linear flight start Y. */
            float m_src_y = 0.0f;

            /** Linear flight start Z. */
            float m_src_z = 0.0f;

            /** Linear flight position X (`runtime state` before parabola). */
            float m_lin_x = 0.0f;

            /** Linear flight position Y. */
            float m_lin_y = 0.0f;

            /** Linear flight position Z. */
            float m_lin_z = 0.0f;

            /** Source-to-dest length (`runtime state`). */
            float m_dist = 1.0f;

            /** Parabola height (12 for Mysterious Arrow). */
            float m_parabola_height = 0.0f;

            /** Parabola angle in radians (`extra * 2π`). */
            float m_parabola_angle = 0.0f;

            /** Mysterious Arrow extra. */
            float m_extra = 0.0f;

            /** Impact follow entity. */
            std::uint32_t m_target_entity = 0;

            /** Identifies this arrow's confirmed outcome independently of other shooters. */
            std::uint64_t m_feedback_token = 0;

            /** Compatibility behavior runtime state. */
            std::uint8_t m_flags = 1;

            /** False for compatibility behavior. */
            bool m_visible = true;

            /** Flight speed in units per second (compatibility behavior projectile initialization).
             */
            float m_speed = 500.0f;

            /** Seconds since spawn; keeps very short shots visible for a frame or two.
             */
            float m_age = 0.0f;

            /** Remaining embedded-arrow display time after impact. */
            float m_crash_timer = 4.0f;

            /** Ribbon red tint. */
            float m_r = 0.5f;

            /** Ribbon green tint. */
            float m_g = 0.5f;

            /** Ribbon blue tint. */
            float m_b = 0.5f;

            /** True for the ONE/ONE blend mode. */
            bool m_additive = true;

            /** Ribbon X samples. */
            std::array<float, kTrailPoints> m_trail_x{};

            /** Ribbon Y samples. */
            std::array<float, kTrailPoints> m_trail_y{};

            /** Ribbon Z samples. */
            std::array<float, kTrailPoints> m_trail_z{};

            /** Ribbon alpha samples. */
            std::array<float, kTrailPoints> m_trail_a{};

            /** True after reaching the target. */
            bool m_crashed = false;

            /** True after update emitted impact audio. */
            bool m_impact_reported = false;

            /** False once the arrow reached its destination. */
            bool m_alive = true;

            /** Monsterbone `target[3] * scale / 16` used to scatter the stuck mesh. */
            float m_impact_scale = 1.0f;

            /** Spine-local matrix computed once on crash (`compatibility behavior`). */
            engine::Mat4 m_local{};

            /** Current stuck world matrix (`runtime state * spine`). */
            engine::Mat4 m_world{};

            /** True after `follow_spine` baked `runtime state`. */
            bool m_attached = false;
        };

        /** Homing spit cloud (`compatibility behavior`). */
        class Spit {
          public:
            /** Number of trail samples per ribbon (`runtime state[0]`). */
            static constexpr int kTrailPoints = 16;

            /**
             * Creates a spit flying toward a target bone.
             * @param x Start X.
             * @param y Start Y.
             * @param z Start Z.
             * @param tx Initial destination X.
             * @param ty Initial destination Y.
             * @param tz Initial destination Z.
             */
            Spit(float x, float y, float z, float tx, float ty, float tz);

            /**
             * Homes toward the target and advances the two additive trails.
             * @param dt Frame delta in seconds.
             * @param home True to replace the destination with hx/hy/hz.
             * @param hx Live target X.
             * @param hy Live target Y.
             * @param hz Live target Z.
             */
            void step(float dt, bool home, float hx, float hy, float hz);

            /** @return True while the cloud is in flight or lingering after crash. */
            [[nodiscard]] bool alive() const {
                return m_alive;
            }

            /** @return True after the projectile reached the target. */
            [[nodiscard]] bool crashed() const {
                return m_crashed;
            }

            /** @return Current X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return Current Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return Current Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /**
             * Trail sample of one ribbon.
             * @param trail Ribbon index (0 = body width 4, 1 = orbit width 1).
             * @param i Sample index, 0 is the head.
             * @param ox Receives X.
             * @param oy Receives Y.
             * @param oz Receives Z.
             * @param oa Receives alpha (0..1).
             */
            void trail_point(int trail, int i, float& ox, float& oy, float& oz, float& oa) const;

          private:
            /** Current X. */
            float m_x = 0.0f;

            /** Current Y. */
            float m_y = 0.0f;

            /** Current Z. */
            float m_z = 0.0f;

            /** Destination X. */
            float m_tx = 0.0f;

            /** Destination Y. */
            float m_ty = 0.0f;

            /** Destination Z. */
            float m_tz = 0.0f;

            /** Initial source-to-dest length (`runtime state`). */
            float m_dist = 1.0f;

            /** Previous parabola position X. */
            float m_para_x = 0.0f;

            /** Previous parabola position Y. */
            float m_para_y = 0.0f;

            /** Previous parabola position Z. */
            float m_para_z = 0.0f;

            /** Flight tangent X (`runtime state`). */
            float m_dir_x = 0.0f;

            /** Flight tangent Y. */
            float m_dir_y = 0.0f;

            /** Flight tangent Z. */
            float m_dir_z = 1.0f;

            /** Orbit offset X (`runtime state`). */
            float m_orbit_x = 0.0f;

            /** Orbit offset Y. */
            float m_orbit_y = 0.0f;

            /** Orbit offset Z. */
            float m_orbit_z = 0.0f;

            /** Flight speed in units per second. */
            float m_speed = 100.0f;

            /** Remaining linger after crash, in seconds. */
            float m_crash_timer = 0.5f;

            /** Accumulated seconds toward the next 30 Hz trail sample. */
            float m_trail_accum = 0.0f;

            /** Trail X of ribbon 0. */
            std::array<float, kTrailPoints> m_t0x{};

            /** Trail Y of ribbon 0. */
            std::array<float, kTrailPoints> m_t0y{};

            /** Trail Z of ribbon 0. */
            std::array<float, kTrailPoints> m_t0z{};

            /** Trail alpha of ribbon 0. */
            std::array<float, kTrailPoints> m_t0a{};

            /** Trail X of ribbon 1. */
            std::array<float, kTrailPoints> m_t1x{};

            /** Trail Y of ribbon 1. */
            std::array<float, kTrailPoints> m_t1y{};

            /** Trail Z of ribbon 1. */
            std::array<float, kTrailPoints> m_t1z{};

            /** Trail alpha of ribbon 1. */
            std::array<float, kTrailPoints> m_t1a{};

            /** True after reaching the target. */
            bool m_crashed = false;

            /** False once the linger timer expires. */
            bool m_alive = true;
        };

        /** One running instance of a cached skill model, with its own skinned GPU
         * meshes. */
        class SkillAnim {
          public:
            /**
             * Creates an instance placed in the world; the meshes are built by
             * create_meshes().
             * @param model Cached model the instance plays; must outlive the instance.
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             * @param yaw Facing angle in radians, ignored while billboarding.
             * @param scale Uniform model scale.
             * @param life_ms Lifetime in milliseconds.
             * @param billboard True to face the camera instead of the spawn yaw.
             * @param follow True to re-position via follow_attached().
             * @param cast_fx True for PRESKILL follow instances (`runtime state[0]`).
             */
            SkillAnim(const SkillModelCache* model,
                      float x,
                      float y,
                      float z,
                      float yaw,
                      float scale,
                      float life_ms,
                      bool billboard,
                      bool follow = false,
                      bool cast_fx = false);

            /**
             * Uploads one GPU mesh per bind-pose mesh of the model.
             * @param engine Engine used to create the meshes.
             */
            void create_meshes(engine::IEngine& engine);

            /**
             * Re-skins the GPU meshes at the current clip time.
             * @param engine Engine used to upload the skinned vertices.
             */
            void skin(engine::IEngine& engine);

            /**
             * Ages the instance and kills it once its lifetime runs out.
             * @param dt_ms Frame delta in milliseconds.
             */
            void advance(float dt_ms);

            /**
             * Fires `(*SOUND)` keyframes of the skill clip at the instance origin.
             * @param audio Mixer used to play cues.
             */
            void poll_sounds(Audio& audio);

            /**
             * Stops keyframe voices started by this instance.
             * @param audio Mixer whose voices to stop.
             */
            void stop_sounds(Audio& audio);

            /**
             * Releases the GPU meshes of the instance.
             * @param engine Engine owning the meshes.
             */
            void destroy_meshes(engine::IEngine& engine);

            /**
             * Draws every part of the instance.
             * @param engine Engine used to submit the draws.
             * @param camera_yaw_rad Camera yaw in radians, used while billboarding.
             */
            void draw(engine::IEngine& engine, float camera_yaw_rad) const;

            /** @return True while the instance is still playing. */
            [[nodiscard]] bool alive() const {
                return m_alive;
            }

            /** @return True when the instance should track a moving attach point. */
            [[nodiscard]] bool follows() const {
                return m_follow;
            }

            /** @return True when this instance is a PRESKILL follow effect. */
            [[nodiscard]] bool cast_fx() const {
                return m_cast_fx;
            }

            /** Marks the instance dead so update() can free its meshes. */
            void kill() {
                m_alive = false;
            }

            /**
             * Updates the world attach point (for following instances).
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             */
            void set_world_pos(float x, float y, float z) {
                m_x = x;
                m_y = y;
                m_z = z;
                m_have_world_matrix = false;
            }

            /**
             * Sets facing for non-billboard instances.
             * @param yaw Angle in radians.
             */
            void set_yaw(float yaw) {
                m_yaw = yaw;
            }

            /**
             * Sets a full attach matrix (FX `model` stages bound to EFFECTBONE).
             * @param world Column-major world matrix including the FX offset.
             */
            void set_world_matrix(const engine::Mat4& world) {
                m_world = world;
                m_have_world_matrix = true;
                m_x = world.m[12];
                m_y = world.m[13];
                m_z = world.m[14];
            }

            /**
             * Sets the EFFECTBONE follow slot used by follow_clip_bones().
             * @param v Slot index, or -1 for the character origin.
             */
            void set_clip_effect_bone(int v) {
                m_clip_effect_bone = v;
            }

            /** @return EFFECTBONE slot, or -1 for origin. */
            [[nodiscard]] int clip_effect_bone() const {
                return m_clip_effect_bone;
            }

            /**
             * Sets the FX stage offset applied in attach-matrix axes.
             * @param ox Local X.
             * @param oy Local Y.
             * @param oz Local Z.
             */
            void set_clip_offset(float ox, float oy, float oz) {
                m_clip_ox = ox;
                m_clip_oy = oy;
                m_clip_oz = oz;
            }

            /** @return FX stage offset X. */
            [[nodiscard]] float clip_ox() const {
                return m_clip_ox;
            }

            /** @return FX stage offset Y. */
            [[nodiscard]] float clip_oy() const {
                return m_clip_oy;
            }

            /** @return FX stage offset Z. */
            [[nodiscard]] float clip_oz() const {
                return m_clip_oz;
            }

            /**
             * Sets the FX `useEffectBoneRotation` flag.
             * @param v True to copy the EFFECTBONE matrix.
             */
            void set_use_effect_bone_rotation(bool v) {
                m_use_effect_bone_rotation = v;
            }

            /** @return True when the FX stage sets `useEffectBoneRotation`. */
            [[nodiscard]] bool use_effect_bone_rotation() const {
                return m_use_effect_bone_rotation;
            }

            /**
             * Forces camera-facing off so 3D FX GBs (Hail ice, freeze block) keep
             * their authored orientation instead of the tiny-quad sprite heuristic.
             * @param v True to face the camera.
             */
            void set_follow_billboard(bool v) {
                m_follow_billboard = v;
            }

            /**
             * Sets the net entity this clip should follow (Cold Binding ice).
             * @param v Entity id, or 0.
             */
            void set_clip_follow_entity(std::uint32_t v) {
                m_clip_follow_entity = v;
            }

            /** @return Follow entity for persist clips, or 0. */
            [[nodiscard]] std::uint32_t clip_follow_entity() const {
                return m_clip_follow_entity;
            }

            /**
             * Plays the GB at native speed (FX `duration` is lifetime, not a time-warp).
             */
            void disable_clip_stretch() {
                m_anim_rate = 1.0f;
            }

            /**
             * Keeps the last frame until kill() (FX model duration 0).
             */
            void hold_until_killed() {
                m_life_ms = 1.0e9f;
                m_anim_rate = 1.0f;
            }

            /**
             * Wraps the clip until kill() (FX `restart 1` attach).
             * @param period_ms Restart period in milliseconds; 0 uses the clip length.
             */
            void loop_until_killed(float period_ms) {
                m_life_ms = 1.0e9f;
                m_anim_rate = 1.0f;
                m_loop_clip = true;
                m_loop_period_ms = period_ms > 0.0f ? period_ms : 0.0f;
            }

            /**
             * Makes the instance fly toward a world point and die on arrival
             * (`compatibility behavior`).
             * @param tx Destination X.
             * @param ty Destination Y.
             * @param tz Destination Z.
             * @param speed Units per second (ice/fire bolts use 500).
             */
            void set_flight(float tx, float ty, float tz, float speed);

            /**
             * Tags a flying instance as a mage fire/ice bolt for trail + crash FX.
             * @param kind Fire or Ice.
             * @param target_entity Crash attach entity, or 0.
             * @param impact_scale Monsterbone scale for small-fire scatter.
             */
            void set_mage_bolt(MageBoltKind kind, std::uint32_t target_entity, float impact_scale);

            /** @return World X. */
            [[nodiscard]] float x() const {
                return m_x;
            }

            /** @return World Y. */
            [[nodiscard]] float y() const {
                return m_y;
            }

            /** @return World Z. */
            [[nodiscard]] float z() const {
                return m_z;
            }

            /** @return True while the instance is a flying projectile. */
            [[nodiscard]] bool flying() const {
                return m_flying;
            }

            /** @return Fire/ice bolt kind, or None. */
            [[nodiscard]] MageBoltKind mage_bolt_kind() const {
                return m_mage_bolt;
            }

            /** @return Crash attach entity, or 0. */
            [[nodiscard]] std::uint32_t target_entity() const {
                return m_target_entity;
            }

            /** @return Monsterbone impact scale. */
            [[nodiscard]] float impact_scale() const {
                return m_impact_scale;
            }

          private:
            /** Cached model the instance plays; owned by CombatFx. */
            const SkillModelCache* m_model = nullptr;

            /** World X. */
            float m_x = 0.0f;

            /** World Y. */
            float m_y = 0.0f;

            /** World Z. */
            float m_z = 0.0f;

            /** Facing angle in radians. */
            float m_yaw = 0.0f;

            /** Uniform model scale. */
            float m_scale = 1.0f;

            /** Playback cursor in milliseconds (scaled clip time). */
            float m_age_ms = 0.0f;

            /** Lifetime in milliseconds. */
            float m_life_ms = 1000.0f;

            /** Multiplier so the full clip fits into runtime state when shorter. */
            float m_anim_rate = 1.0f;

            /** True to face the camera instead of runtime state. */
            bool m_follow_billboard = false;

            /** True to re-position via follow_attached(). */
            bool m_follow = false;

            /** True for PRESKILL follow instances; loops until kill(). */
            bool m_cast_fx = false;

            /** True while the instance is a projectile (`compatibility behavior` / `compatibility
             * behavior`). */
            bool m_flying = false;

            /** Flight destination X. */
            float m_tx = 0.0f;

            /** Flight destination Y. */
            float m_ty = 0.0f;

            /** Flight destination Z. */
            float m_tz = 0.0f;

            /** Flight speed in units per second. */
            float m_speed = 500.0f;

            /** Pitch toward the destination in radians, used while flying. */
            float m_pitch = 0.0f;

            /** Fire/ice trail + crash kind; None for other skill meshes. */
            MageBoltKind m_mage_bolt = MageBoltKind::None;

            /** Crash attach entity for mage bolts. */
            std::uint32_t m_target_entity = 0;

            /** Persist-clip follow entity (Cold Binding ice), or 0. */
            std::uint32_t m_clip_follow_entity = 0;

            /** EFFECTBONE slot for follow_clip_bones(), or -1 for origin. */
            int m_clip_effect_bone = -1;

            /** FX stage offset X. */
            float m_clip_ox = 0.0f;

            /** FX stage offset Y. */
            float m_clip_oy = 0.0f;

            /** FX stage offset Z. */
            float m_clip_oz = 0.0f;

            /** FX `useEffectBoneRotation`. */
            bool m_use_effect_bone_rotation = false;

            /** True when runtime state is an authored attach matrix. */
            bool m_have_world_matrix = false;

            /** Full attach matrix when runtime state. */
            engine::Mat4 m_world{};

            /** True to wrap clip time until kill() (FX `restart`). */
            bool m_loop_clip = false;

            /** Restart period in milliseconds; 0 uses the GB clip length. */
            float m_loop_period_ms = 0.0f;

            /** Monsterbone scale for firebolt_smallfire scatter. */
            float m_impact_scale = 1.0f;

            /** GPU meshes, one per bind-pose mesh of the model. */
            std::vector<engine::MeshHandle> m_live_meshes;

            /** compatibility behavior skinning targets, one per bind-pose mesh of the model. */
            std::vector<terrain::GbMeshCpu> m_work_meshes;

            /** False once the lifetime ran out. */
            bool m_alive = true;

            /** Keyframe SOUND tracker for this instance. */
            AnimSound m_sounds;
        };

        /** FX `model` stage waiting for its shader `delay` before spawn. */
        class PendingClipModel {
          public:
            /**
             * Queues one clip model spawn.
             * @param name Effect/Skill GB stem.
             * @param x World X.
             * @param y World Y.
             * @param z World Z.
             * @param yaw Facing angle in radians.
             * @param delay_s Seconds remaining before spawn.
             * @param duration_s FX stage duration in seconds, or 0 for clip length.
             * @param follow_entity Net id to re-position each frame, or 0.
             * @param persist True to keep the last frame until kill_clip_entity().
             * @param loop True to wrap the clip until kill_clip_entity().
             * @param effect_bone EFFECTBONE slot, or -1 for origin.
             * @param ox FX stage offset X.
             * @param oy FX stage offset Y.
             * @param oz FX stage offset Z.
             * @param use_effect_bone_rotation True to copy the EFFECTBONE matrix.
             */
            PendingClipModel(std::string name,
                             float x,
                             float y,
                             float z,
                             float yaw,
                             float delay_s,
                             float duration_s,
                             std::uint32_t follow_entity,
                             bool persist,
                             bool loop = false,
                             int effect_bone = -1,
                             float ox = 0.0f,
                             float oy = 0.0f,
                             float oz = 0.0f,
                             bool use_effect_bone_rotation = false);

            /** @return Effect/Skill GB stem. */
            [[nodiscard]] const std::string& name() const;

            /** @return World X. */
            [[nodiscard]] float x() const;

            /** @return World Y. */
            [[nodiscard]] float y() const;

            /** @return World Z. */
            [[nodiscard]] float z() const;

            /** @return Facing angle in radians. */
            [[nodiscard]] float yaw() const;

            /** @return Seconds remaining before spawn. */
            [[nodiscard]] float delay() const;

            /**
             * Ages the wait timer.
             * @param v Seconds remaining.
             */
            void set_delay(float v);

            /** @return FX stage duration in seconds, or 0 for clip length. */
            [[nodiscard]] float duration() const;

            /** @return Net id to re-position each frame, or 0. */
            [[nodiscard]] std::uint32_t follow_entity() const;

            /** @return True to keep the last frame until kill_clip_entity(). */
            [[nodiscard]] bool persist() const;

            /** @return True to wrap the clip until kill_clip_entity(). */
            [[nodiscard]] bool loop() const;

            /** @return EFFECTBONE slot, or -1 for origin. */
            [[nodiscard]] int effect_bone() const;

            /** @return FX stage offset X. */
            [[nodiscard]] float ox() const;

            /** @return FX stage offset Y. */
            [[nodiscard]] float oy() const;

            /** @return FX stage offset Z. */
            [[nodiscard]] float oz() const;

            /** @return True when the FX stage sets `useEffectBoneRotation`. */
            [[nodiscard]] bool use_effect_bone_rotation() const;

          private:
            /** Effect/Skill GB stem. */
            std::string m_name;

            /** World X. */
            float m_x = 0.0f;

            /** World Y. */
            float m_y = 0.0f;

            /** World Z. */
            float m_z = 0.0f;

            /** Facing angle in radians. */
            float m_yaw = 0.0f;

            /** Seconds remaining before spawn. */
            float m_delay = 0.0f;

            /** FX stage duration in seconds, or 0 for clip length. */
            float m_duration = 0.0f;

            /** Net id to re-position each frame, or 0. */
            std::uint32_t m_follow_entity = 0;

            /** True to keep the last frame until kill_clip_entity(). */
            bool m_persist = false;

            /** True to wrap the clip until kill_clip_entity(). */
            bool m_loop = false;

            /** EFFECTBONE slot, or -1 for origin. */
            int m_effect_bone = -1;

            /** FX stage offset X. */
            float m_ox = 0.0f;

            /** FX stage offset Y. */
            float m_oy = 0.0f;

            /** FX stage offset Z. */
            float m_oz = 0.0f;

            /** FX `useEffectBoneRotation`. */
            bool m_use_effect_bone_rotation = false;
        };

        /**
         * Spawns one instance of a cached skill model.
         * @param engine Engine used to create the instance meshes.
         * @param model Cached model to instance; must stay alive while the instance
         * plays.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param yaw Facing angle in radians.
         * @param scale Uniform model scale.
         * @param life_ms Lifetime in milliseconds; values at or below zero use the
         * clip duration.
         * @param billboard True to face the camera.
         * @param follow True to re-position via follow_attached().
         * @param cast_fx True for PRESKILL follow instances.
         */
        void spawn_skill_instance(engine::IEngine& engine,
                                  const SkillModelCache& model,
                                  float x,
                                  float y,
                                  float z,
                                  float yaw,
                                  float scale,
                                  float life_ms,
                                  bool billboard,
                                  bool follow = false,
                                  bool cast_fx = false);

        /**
         * Spawns one additive particle.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param vx Velocity X in units per second.
         * @param vy Velocity Y in units per second.
         * @param vz Velocity Z in units per second.
         * @param life Lifetime in seconds.
         * @param size0 Size at spawn.
         * @param size1 Size at death.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param a0 Alpha at spawn.
         * @param rot_speed Roll speed in radians per second.
         * @param tex Sprite texture.
         */
        void spawn_particle(float x,
                            float y,
                            float z,
                            float vx,
                            float vy,
                            float vz,
                            float life,
                            float size0,
                            float size1,
                            float r,
                            float g,
                            float b,
                            float a0,
                            float rot_speed,
                            engine::TextureHandle tex);

        /**
         * Draws one camera-facing additive quad.
         * @param engine Engine used to submit the draw.
         * @param tex Sprite texture.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param size Quad edge length in world units.
         */
        void draw_billboard(engine::IEngine& engine,
                            engine::TextureHandle tex,
                            float x,
                            float y,
                            float z,
                            float size) const;

        /**
         * Draws one additive camera-facing polyline (`DrawPolyLine`).
         * @param engine Engine used to submit the draw.
         * @param tex Ribbon texture.
         * @param xs Sample X.
         * @param ys Sample Y.
         * @param zs Sample Z.
         * @param as Sample alpha (0..1), used as width fade.
         * @param n Sample count.
         * @param width Half-width at alpha 1.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param additive True for ONE/ONE; false for SRC_ALPHA/INV_SRC_ALPHA.
         */
        void draw_polyline(engine::IEngine& engine,
                           engine::TextureHandle tex,
                           const float* xs,
                           const float* ys,
                           const float* zs,
                           const float* as,
                           int n,
                           float width,
                           float r = 1.0f,
                           float g = 1.0f,
                           float b = 1.0f,
                           bool additive = true);

        /** Lightning.GTX ribbon texture. */
        engine::TextureHandle m_lightning_tex{};

        /** LightningRed.GTX ribbon texture (`ET_MANA_BURN`). */
        engine::TextureHandle m_lightning_red_tex{};

        /** Lightning02.GTX ribbon texture (`ET_SPIRIT_BLAST`). */
        engine::TextureHandle m_lightning_spirit_tex{};

        /** lig01.GTX cast flare sprite. */
        engine::TextureHandle m_flare_tex{};

        /** star4.GTX impact star sprite. */
        engine::TextureHandle m_impact_star_tex{};

        /** LightningSpark.GTX impact spark sprite. */
        engine::TextureHandle m_impact_spark_tex{};

        /** `ghost_blue` spit cloud texture (`compatibility behavior`). */
        engine::TextureHandle m_ghost_blue_tex{};

        /** `ghost_grey` Archer arrow ribbon texture. */
        engine::TextureHandle m_ghost_grey_tex{};

        /** Arrow projectile mesh (Item/Weapon/M_arrow.gb). */
        engine::MeshHandle m_arrow_mesh{};

        /** Arrow projectile texture. */
        engine::TextureHandle m_arrow_tex{};

        /** Scratch mesh rebuilt every frame for the lightning ribbons. */
        engine::MeshHandle m_ribbon_mesh{};

        /** Unit quad reused by every particle billboard. */
        engine::MeshHandle m_particle_quad{};

        /** Static compatibility behavior DrawDonut mesh (radius 5, height 30, tess 4). */
        engine::MeshHandle m_heal_donut_mesh{};

        /** healing.dds / healing.GTX for the heal donut. */
        engine::TextureHandle m_heal_tex{};

        /** Scratch mesh rebuilt every Ice Nova draw (`DrawDonut` tess 6). */
        engine::MeshHandle m_ice_nova_mesh{};

        /** `whiteres.dds` for compatibility behavior. */
        engine::TextureHandle m_ice_nova_tex{};

        /** `GenericGlow.dds` for compatibility behavior. */
        engine::TextureHandle m_meditation_tex{};

        /** True once init() has completed. */
        bool m_ready = false;

        /** Client data root used to resolve clip-effect GBs. */
        std::string m_data_root;

        /** Skill_R_4_0 wand-tip prep model. */
        SkillModelCache m_skill_r4_0;

        /** Skill_R_3_1 flying ice bolt (`compatibility behavior`). */
        SkillModelCache m_skill_r3_1;

        /** Skill_R_2_1 flying fire bolt (`compatibility behavior`). */
        SkillModelCache m_skill_r2_1;

        /** Skill_M_3_0 archer compatibility behavior PRESKILL (`runtime state[0]`). */
        SkillModelCache m_skill_m30;

        /** effect_levelup.gb cached model. */
        SkillModelCache m_level_up;

        /** effect_behead.gb cached model (optional). */
        SkillModelCache m_behead;

        /** Effect texture cache keyed by texture name. */
        std::unordered_map<std::string, engine::TextureHandle> m_tex_cache;

        /** Live lightning bolts. */
        std::vector<Bolt> m_bolts;

        /** Live compatibility behavior donuts. */
        std::vector<HealDonut> m_heal_donuts;

        /** Live compatibility behavior donuts. */
        std::vector<IceNova> m_ice_novas;

        /** Live compatibility behavior ribbons. */
        std::vector<StoneShieldPrep> m_stone_shields;

        /** Live compatibility behavior ribbons. */
        std::vector<MeditationFx> m_meditations;

        /** Pending fire/ice bolt crashes for ActionDamage floats. */
        std::vector<ProjectileCrash> m_projectile_crashes;

        /** Live particles. */
        std::vector<Particle> m_particles;

        /**
         * Clip `(*EFFECT_MODEL)` caches. Declared before runtime state so live
         * instances still point at valid models while they are destroyed.
         */
        std::unordered_map<std::string, std::unique_ptr<SkillModelCache>> m_clip_models;

        /** Running skill model instances. */
        std::vector<SkillAnim> m_skill_anims;

        /** FX model stages waiting for `delay` before becoming SkillAnim instances. */
        std::vector<PendingClipModel> m_pending_clips;

        /** Arrows in flight. */
        std::vector<Arrow> m_arrows;

        /** Matches network results to normal-arrow arrival events. */
        ArrowHitFeedback m_arrow_feedback;

        /** Devil-troop spit clouds in flight. */
        std::vector<Spit> m_spits;

        /** Live homing target X (local player SPINE1). */
        float m_home_x = 0.0f;

        /** Live homing target Y. */
        float m_home_y = 0.0f;

        /** Live homing target Z. */
        float m_home_z = 0.0f;

        /** True after set_homing_target() has been called this session. */
        bool m_have_home = false;

        /** Camera yaw of the last update, in radians. */
        float m_cam_yaw = 0.0f;

        /** Camera eye X of the last update. */
        float m_cam_x = 0.0f;

        /** Camera eye Y of the last update. */
        float m_cam_y = 0.0f;

        /** Camera eye Z of the last update. */
        float m_cam_z = 0.0f;

        /** SFX mixer; nullptr skips combat sounds. */
        Audio* m_audio = nullptr;

        /** Looping FX-stage sounds keyed by clip follow entity. */
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> m_clip_loop_sound_ids;

        /** Particle/FX manager for bolt trails and crash bursts. */
        SpriteFx* m_sprite_fx = nullptr;
    };

}  // namespace openkal::client
