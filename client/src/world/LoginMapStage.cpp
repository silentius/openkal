/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/LoginMapStage.h"

#include "openkal/client/SpriteFx.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/platform/Log.h"

namespace openkal::client {

    bool LoginMapStage::load(engine::IEngine& engine, const LoginMapStagePaths& paths) {
        destroy(engine);
        m_next_effect_serial = 1;
        m_effect_sync_reported = false;
        if (paths.map_root.empty()) {
            log::warn("LoginMapStage: no map root configured");
            return false;
        }
        const std::string map_file =
            map::IndoorMap::resolve_map_file(paths.map_root, "Map\\login_map");
        if (map_file.empty()) {
            log::warn("LoginMapStage: Map\\login_map is missing under " + paths.map_root);
            return false;
        }
        const bool have_object_particles = !paths.world_objects_path.empty() &&
                                           m_object_catalog.load_json(paths.world_objects_path);
        if (!have_object_particles && !paths.world_objects_path.empty()) {
            log::warn("LoginMapStage: world-object particle table unavailable at " +
                      paths.world_objects_path);
        }
        if (!m_view.load(engine,
                         paths.map_root,
                         map_file,
                         paths.origin_patch_x,
                         paths.origin_patch_y,
                         paths.origin_patch_x,
                         paths.origin_patch_y,
                         m_model_cache,
                         m_texture_cache,
                         m_floor,
                         have_object_particles ? &m_object_catalog : nullptr,
                         have_object_particles ? &m_next_effect_serial : nullptr)) {
            log::warn("LoginMapStage: failed to load " + map_file);
            return false;
        }
        log::info("LoginMapStage: loaded login_map at orgPatch 0,6");
        return true;
    }

    void LoginMapStage::destroy(engine::IEngine& engine) {
        m_live_effects.clear();
        m_next_effect_serial = 1;
        m_effect_sync_reported = false;
        m_view.unload(m_floor);
        for (auto& [path, model] : m_model_cache) {
            (void) path;
            for (auto& part : model.parts) {
                if (part.mesh) {
                    engine.destroy_mesh(part.mesh);
                }
            }
        }
        m_model_cache.clear();
        for (auto& [path, texture] : m_texture_cache) {
            (void) path;
            if (texture) {
                engine.destroy_texture(texture);
            }
        }
        m_texture_cache.clear();
    }

    float LoginMapStage::sample_floor(float world_x, float world_z, float fallback_y) const {
        return m_floor.sample(world_x, world_z, fallback_y);
    }

    void LoginMapStage::draw(engine::IEngine& engine,
                             const engine::Vec3& eye,
                             const engine::Vec3& target,
                             const engine::Mat4& view,
                             const engine::Mat4& projection,
                             float anim_time_sec) {
        if (!loaded()) {
            return;
        }
        FrustumPlanes frustum;
        WorldRenderCache::extract_frustum_planes(engine::Mat4::multiply(projection, view), frustum);
        m_view.update_visibility(eye.x, eye.y, eye.z, target.x, target.y, target.z, frustum);

        for (int pass = 0; pass < 2; ++pass) {
            const bool want_alpha = pass == 1;
            for (int offset = 0; offset < 2; ++offset) {
                const bool want_offset = offset == 1;
                if (want_offset) {
                    engine.set_polygon_offset(-4.0f, -64.0f);
                }
                const auto& props = m_view.props();
                for (std::size_t prop_index = 0; prop_index < props.size(); ++prop_index) {
                    const auto& prop = props[prop_index];
                    if (!prop.draw || !prop.gpu || prop.polygon_offset != want_offset) {
                        continue;
                    }
                    for (std::size_t part_index = 0; part_index < prop.gpu->parts.size();
                         ++part_index) {
                        if (!m_view.region_part_visible(prop_index, part_index)) {
                            continue;
                        }
                        const auto& part = prop.gpu->parts[part_index];
                        if (part.alpha_blend != want_alpha) {
                            continue;
                        }
                        float uv_u = 0.0f;
                        float uv_v = 0.0f;
                        float opacity = 1.0f;
                        WorldRenderCache::sample_prop_uv(part, anim_time_sec, uv_u, uv_v, opacity);
                        const bool blend = part.alpha_blend || opacity < 0.999f;
                        WorldRenderCache::draw_prop_part(engine,
                                                         part,
                                                         prop.model,
                                                         blend,
                                                         part.alpha_test && !blend,
                                                         uv_u,
                                                         uv_v,
                                                         opacity);
                    }
                }
                if (want_offset) {
                    engine.set_polygon_offset(0.0f, 0.0f);
                }
            }
        }
    }

    void LoginMapStage::draw_effects(engine::IEngine& engine,
                                     SpriteFx& sprite_fx,
                                     float dt,
                                     const engine::Vec3& eye) {
        if (!loaded() || !sprite_fx.ready()) {
            return;
        }

        std::unordered_set<std::uint32_t> seen;
        std::size_t configured_attachments = 0;
        const auto& props = m_view.props();
        for (std::size_t prop_index = 0; prop_index < props.size(); ++prop_index) {
            const auto& prop = props[prop_index];
            if (prop.fx_entity == 0 || prop.catalog_key.empty()) {
                continue;
            }
            const auto* attachments = m_object_catalog.find(prop.catalog_key);
            if (attachments == nullptr || attachments->empty()) {
                continue;
            }
            if (!sprite_fx.has(prop.fx_entity)) {
                for (const auto& attachment : *attachments) {
                    sprite_fx.attach_particle(engine,
                                              prop.fx_entity,
                                              attachment.name(),
                                              attachment.effect_bone(),
                                              attachment.ox(),
                                              attachment.oy(),
                                              attachment.oz());
                }
                sprite_fx.persist_attached(prop.fx_entity);
            }
            configured_attachments += attachments->size();
            sprite_fx.follow_oriented(prop.fx_entity, -1, prop.model);
            for (const auto& attachment : *attachments) {
                if (attachment.effect_bone() != -1) {
                    sprite_fx.follow_oriented(prop.fx_entity, attachment.effect_bone(), prop.model);
                }
            }
            seen.insert(prop.fx_entity);
        }
        for (const std::uint32_t entity : m_live_effects) {
            if (seen.find(entity) == seen.end()) {
                sprite_fx.detach(entity);
            }
        }
        m_live_effects.swap(seen);
        if (!m_effect_sync_reported && !m_live_effects.empty()) {
            log::info("LoginMapStage: activated " + std::to_string(configured_attachments) +
                      " particle scripts on " + std::to_string(m_live_effects.size()) +
                      " login-map objects");
            m_effect_sync_reported = true;
        }
        sprite_fx.update(dt, eye.x, eye.y, eye.z);
        sprite_fx.draw(engine);
    }

}  // namespace openkal::client
