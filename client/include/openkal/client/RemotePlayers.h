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

#include "openkal/client/AnimatedActor.h"
#include "openkal/client/Player.h"
#include "openkal/client/RemotePlayerMotion.h"
#include "openkal/client/WorldPick.h"
#include "openkal/protocol/CreatePlayer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openkal::client {

    class SkillDef;

    /** One visible remote player with its network pose and animated actor. */
    class RemotePlayer {
      public:
        RemotePlayer() = default;

        /** @return Entity id. */
        [[nodiscard]] std::uint32_t id() const {
            return m_id;
        }

        /**
         * Sets the entity id.
         * @param id Entity id.
         */
        void set_id(std::uint32_t id) {
            m_id = id;
        }

        /** @return Display name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the display name.
         * @param name Display name.
         */
        void set_name(std::string name) {
            m_name = std::move(name);
        }

        /** @return Player class. */
        [[nodiscard]] Class clazz() const {
            return m_clazz;
        }

        /**
         * Sets the player class.
         * @param clazz Player class.
         */
        void set_clazz(Class clazz) {
            m_clazz = clazz;
        }

        /** @return Client world X. */
        [[nodiscard]] float wx() const {
            return m_motion.position().x;
        }

        /** @return Client world Y (feet). */
        [[nodiscard]] float wy() const {
            return m_motion.position().y;
        }

        /** @return Client world Z. */
        [[nodiscard]] float wz() const {
            return m_motion.position().z;
        }

        /**
         * Sets the world-space feet pose.
         * @param wx World X.
         * @param wy World Y.
         * @param wz World Z.
         */
        void set_pose(float wx, float wy, float wz) {
            m_motion.reset({wx, wy, wz});
        }

        /** Sets a network target for interpolated position and facing.
         * @param wx Client world X.
         * @param wy Floor height at the position.
         * @param wz Client world Z.
         * @param moving True while movement packets indicate running.
         */
        void apply_network_pose(float wx, float wy, float wz, bool moving);

        /** Advances the displayed pose between server snapshots.
         * @param dt Render-frame delta seconds.
         */
        void update_motion(float dt);

        /** Returns whether the displayed player is still in locomotion.
         * @return True while moving or finishing the final interpolated segment.
         */
        bool moving() const {
            return m_motion.moving();
        }

        /** @return Model matrix at the current position and facing. */
        [[nodiscard]] engine::Mat4 model_matrix() const;

        /**
         * Approximate spine / buff-impact point.
         * @param x Out world X.
         * @param y Out world Y.
         * @param z Out world Z.
         */
        void spine_world(float& x, float& y, float& z) const;

        /** Faces a combat target without changing the network position.
         * @param x Target world X.
         * @param z Target world Z.
         */
        void face_target(float x, float z);

        /** Presents a confirmed swing after completing its approach in PvP.
         * @param target_x Target world X.
         * @param target_z Target world Z.
         * @param player_target True for attacks on another player.
         */
        void present_basic_attack(float target_x, float target_z, bool player_target);

        /** Applies the appearance carried by a create-player snapshot.
         * @param engine Mesh owner.
         * @param data_root Client data root.
         * @param tex_cache Shared texture cache.
         * @param fields Network appearance.
         * @param catalog Item definitions used to resolve equipment.
         * @return True when the actor is loaded.
         */
        [[nodiscard]] bool
        apply_appearance(engine::IEngine& engine,
                         const std::string& data_root,
                         std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                         const protocol::CreatePlayerFields& fields,
                         const ItemDb& catalog);

        /** Plays a network skill phase using the caster's catalog motion.
         * @param engine Renderer used to refresh effect sockets.
         * @param def Skill definition, or nullptr when unknown.
         * @param skill Skill index.
         * @param fire True for execution, false for preparation.
         */
        void present_skill(engine::IEngine& engine, const SkillDef* def, int skill, bool fire);

        /** @return Worn equipment used when returning to the human form. */
        const Wear& wear() const {
            return m_wear;
        }

        /** @return Current combat target entity. */
        std::uint32_t combat_target() const {
            return m_combat_target;
        }

        /** Sets the current combat target.
         * @param id Target entity id.
         */
        void set_combat_target(std::uint32_t id) {
            m_combat_target = id;
        }

        /** @return Skinned actor. */
        [[nodiscard]] AnimatedActor& actor() {
            return m_actor;
        }

        /** @return Skinned actor. */
        [[nodiscard]] const AnimatedActor& actor() const {
            return m_actor;
        }

        /** @return True while the remote is sitting. */
        [[nodiscard]] bool resting() const {
            return m_resting;
        }

        /**
         * Sets the rest pose flag from S2C_ACTION AT_REST.
         * @param resting True while sitting.
         */
        void set_resting(bool resting) {
            m_resting = resting;
        }

        /** @return True while related compatibility states is set. */
        [[nodiscard]] bool consuming() const {
            return m_consuming;
        }

        /**
         * Sets the consume-clip flag from S2C_MSTATE.
         * @param consuming True while sit food is active.
         */
        void set_consuming(bool consuming) {
            m_consuming = consuming;
        }

      private:
        /** Entity id. */
        std::uint32_t m_id = 0;

        /** Display name. */
        std::string m_name;

        /** Player class. */
        Class m_clazz = Class::Knight;

        /** Interpolated feet pose, facing and locomotion state. */
        RemotePlayerMotion m_motion;

        /** True while compatibility behavior is active. */
        bool m_resting = false;

        /** True while related compatibility states is set. */
        bool m_consuming = false;

        /** Last applied network appearance, used to avoid redundant mesh reloads. */
        protocol::CreatePlayerFields m_appearance;

        /** Equipment snapshot used by the human model and transform fallback. */
        Wear m_wear;

        /** Target used by animation-timed projectiles and clip effects. */
        std::uint32_t m_combat_target = 0;

        /** Idle skinned mesh. */
        AnimatedActor m_actor;
    };

    /** Owns remote CreatePlayer actors for rendering, nameplates, and LMB pick. */
    class RemotePlayers {
      public:
        RemotePlayers() = default;

        /**
         * Inserts or updates a remote from S2C_CreatePlayer (no hero bit).
         * @param engine Engine used to load meshes.
         * @param data_root Client data root.
         * @param tex_cache Shared texture cache.
         * @param fields CreatePlayer payload.
         * @param catalog Item definitions for worn meshes.
         * @param wx Client world X.
         * @param wy Client world Y (already snapped to the floor).
         * @param wz Client world Z.
         * @return True when the actor is present afterwards.
         */
        [[nodiscard]] bool upsert(engine::IEngine& engine,
                                  const std::string& data_root,
                                  std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                                  const protocol::CreatePlayerFields& fields,
                                  const ItemDb& catalog,
                                  float wx,
                                  float wy,
                                  float wz);

        /**
         * Destroys one remote.
         * @param engine Engine owning GPU meshes.
         * @param id Entity id.
         */
        void remove(engine::IEngine& engine, std::uint32_t id);

        /**
         * Releases every GPU mesh.
         * @param engine Engine owning GPU meshes.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Looks up a remote by id.
         * @param id Entity id.
         * @return Pointer, or nullptr.
         */
        [[nodiscard]] RemotePlayer* find(std::uint32_t id);

        /**
         * Looks up a remote by id.
         * @param id Entity id.
         * @return Pointer, or nullptr.
         */
        [[nodiscard]] const RemotePlayer* find(std::uint32_t id) const;

        /** Sets the remote body budget for subsequent updates, picking and drawing.
         * @param ids Visible remote player entity ids.
         */
        void set_render_selection(std::unordered_set<std::uint32_t> ids);

        /** Tests whether a remote body belongs to the current render selection.
         * @param id Remote player entity id.
         * @return True before a budget is set, or when the id is selected.
         */
        [[nodiscard]] bool is_rendered(std::uint32_t id) const;

        /** @return Live remotes in spawn order. */
        [[nodiscard]] std::vector<RemotePlayer*> all();

        /** @return Live remotes in spawn order. */
        [[nodiscard]] std::vector<const RemotePlayer*> all() const;

        /**
         * Ray-tests NPC-sized AABBs and returns the closest hit.
         * @param cam Follow camera.
         * @param player Local player.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param screen_w Screen width.
         * @param screen_h Screen height.
         * @param hit_t Out ray t of the hit.
         * @return Entity id, or 0.
         */
        [[nodiscard]] std::uint32_t pick(const FollowCamera& cam,
                                         const LocalPlayer& player,
                                         float mx,
                                         float my,
                                         float screen_w,
                                         float screen_h,
                                         float& hit_t) const;

        /**
         * Advances remote player animation clips.
         * @param engine Engine used for skinning.
         * @param dt Frame delta seconds.
         * @param audio Positional animation sound player.
         */
        void update(engine::IEngine& engine, float dt, Audio* audio);

        /**
         * Draws every remote in one alpha pass.
         * @param engine Renderer.
         * @param want_alpha True for the transparent pass.
         * @param view World-to-camera matrix for transparent weapon ordering.
         */
        void draw(engine::IEngine& engine, const engine::Mat4& view, bool want_alpha) const;

      private:
        /** Explicit body selection; motion and animation timelines continue for hidden actors. */
        std::unordered_set<std::uint32_t> m_rendered_ids;

        /** False until the runtime supplies a body budget, preserving unlimited standalone use. */
        bool m_render_selection_active = false;

        /** Live remotes keyed by entity id. */
        std::unordered_map<std::uint32_t, std::unique_ptr<RemotePlayer>> m_players;

        /** Insertion order for stable draw/pick. */
        std::vector<std::uint32_t> m_order;
    };

}  // namespace openkal::client
