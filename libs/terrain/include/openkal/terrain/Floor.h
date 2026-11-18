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
#include "openkal/terrain/HeightField.h"
#include "openkal/terrain/HeightPatch.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::terrain {

    /**
     * Walkable OPL prop triangles in world space (Kal outdoor GetFloor subset).
     *
     * Triangles are stored flat, nine floats each: ax,ay,az, bx,by,bz, cx,cy,cz.
     * The axis aligned bounds are kept alongside so the sampler can reject props by
     * a cheap XZ test.
     */
    class FloorProp {
      public:
        /**
         * Reserves storage for the triangle soup.
         * @param float_count Number of floats to reserve (nine per triangle).
         */
        void reserve_tris(std::size_t float_count) {
            m_tris.reserve(float_count);
        }

        /**
         * Appends one world-space vertex and grows the bounds to include it.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void add_vertex(float x, float y, float z);

        /**
         * Widens the horizontal bounds so props are not culled at their exact edge.
         * @param pad Padding in world units, applied on both X and Z.
         */
        void pad_bounds_xz(float pad);

        /**
         * Tests whether a world position falls inside the horizontal bounds.
         * @param world_x World X.
         * @param world_z World Z.
         * @return True when the position is inside the padded XZ bounds.
         */
        [[nodiscard]] bool contains_xz(float world_x, float world_z) const;

        /**
         * Records the outdoor patch this deck was streamed with.
         * @param patch_x Horizontal patch index.
         * @param patch_y Vertical patch index.
         */
        void set_patch(int patch_x, int patch_y) {
            m_patch_x = patch_x;
            m_patch_y = patch_y;
        }

        /** @return Horizontal patch index this deck belongs to. */
        [[nodiscard]] int patch_x() const {
            return m_patch_x;
        }

        /** @return Vertical patch index this deck belongs to. */
        [[nodiscard]] int patch_y() const {
            return m_patch_y;
        }

        /** @return Flat triangle soup, nine floats per triangle. */
        [[nodiscard]] const std::vector<float>& tris() const {
            return m_tris;
        }

        /** @return True while no vertex has been added. */
        [[nodiscard]] bool empty() const {
            return m_tris.empty();
        }

        /** @return Lower X bound. */
        [[nodiscard]] float min_x() const {
            return m_min_x;
        }

        /** @return Upper X bound. */
        [[nodiscard]] float max_x() const {
            return m_max_x;
        }

        /** @return Lower Y bound. */
        [[nodiscard]] float min_y() const {
            return m_min_y;
        }

        /** @return Upper Y bound. */
        [[nodiscard]] float max_y() const {
            return m_max_y;
        }

        /** @return Lower Z bound. */
        [[nodiscard]] float min_z() const {
            return m_min_z;
        }

        /** @return Upper Z bound. */
        [[nodiscard]] float max_z() const {
            return m_max_z;
        }

        /** Builds the XZ triangle bins used by floor queries. */
        void build_index();

        /** @return True after build_index() stored at least one bin. */
        [[nodiscard]] bool indexed() const {
            return !m_bins.empty();
        }

        /** Triangle start offsets (into tris()) whose XZ overlaps this world point.
         * @param world_x World X.
         * @param world_z World Z.
         * @return Bin list, or nullptr when the index is empty (test every triangle).
         */
        [[nodiscard]] const std::vector<std::size_t>* bin_at(float world_x, float world_z) const;

        /** XZ bin size for indoor/outdoor floor queries. */
        static constexpr float kBinSize = 64.0f;

      private:
        /** World-space triangle soup, nine floats per triangle. */
        std::vector<float> m_tris;

        /** Lower X bound, padded by pad_bounds_xz(). */
        float m_min_x = 1e30f;

        /** Upper X bound, padded by pad_bounds_xz(). */
        float m_max_x = -1e30f;

        /** Lower Y bound. */
        float m_min_y = 1e30f;

        /** Upper Y bound. */
        float m_max_y = -1e30f;

        /** Lower Z bound, padded by pad_bounds_xz(). */
        float m_min_z = 1e30f;

        /** Upper Z bound, padded by pad_bounds_xz(). */
        float m_max_z = -1e30f;

        /** Outdoor patch X this deck was streamed with. */
        int m_patch_x = 0;

        /** Outdoor patch Y this deck was streamed with. */
        int m_patch_y = 0;

        /** XZ bins → triangle start offsets (nine floats per triangle). */
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_bins;
    };

    /** Which faces FloorField::raycast may hit. */
    enum class RayHitFilter {
        /** Floors, walls, and ceilings. */
        All,

        /** Vertical faces only (|ny/nlen| < 0.45). */
        Walls,

        /** Walls and steep faces encountered uphill by a horizontal ray. */
        StepObstacles,

        /** Up-facing slopes too steep to stand on. */
        UpwardSlopes,

        /** Down-facing faces only (ny/nlen <= -0.45). */
        Ceilings,
    };

    /**
     * Standing surface of the streamed world: KCM terrain plus walkable prop
     * decks (outdoor OPL stairs/bridges, indoor dungeon compatibility behavior).
     */
    class FloorField {
      public:
        /**
         * Adds a loaded KCM patch to the terrain height field.
         * @param patch Patch to take ownership of.
         */
        void add_patch(terrain::HeightPatch patch);

        /**
         * Adds the walkable triangles of an OPL prop instance. Foliage assets are
         * skipped. Collision-only GBs (river-bridge planks) are included via compatibility
         * behavior.
         * @param gb_path Path of the prop .gb mesh.
         * @param world World matrix of the instance.
         * @param patch_x Horizontal patch index the instance belongs to.
         * @param patch_y Vertical patch index the instance belongs to.
         */
        void
        add_prop(const std::string& gb_path, const engine::Mat4& world, int patch_x, int patch_y);

        /**
         * Adds render-mesh triangles used as camera blockers (walls, ceilings).
         * Indoor region GBs call this so zoom-out cannot leave the dungeon hull.
         * @param gb_path Path of the region .gb mesh.
         * @param world World matrix of the instance.
         * @param patch_x Horizontal patch index the instance belongs to.
         * @param patch_y Vertical patch index the instance belongs to.
         */
        void add_blocker(const std::string& gb_path,
                         const engine::Mat4& world,
                         int patch_x,
                         int patch_y);

        /**
         * Adds a physical hull from GB compatibility behavior (solid render if the GB has no
         * compatibility behavior). Indoor MAP_OBJECT placements and outdoor OPL props call this so
         * statues and tall structures block walking without becoming standable roofs. Camera rays
         * also hit these hulls.
         * @param gb_path Path of the object .gb mesh.
         * @param world World matrix of the instance.
         * @param patch_x Horizontal patch index the instance belongs to.
         * @param patch_y Vertical patch index the instance belongs to.
         */
        void add_obstacle(const std::string& gb_path,
                          const engine::Mat4& world,
                          int patch_x,
                          int patch_y);

        /**
         * Closest hit of a world-space ray against camera blockers.
         * Two-sided; hits ceilings and walls that standing rays reject.
         * @param ox Ray origin X.
         * @param oy Ray origin Y.
         * @param oz Ray origin Z.
         * @param dx Ray direction X (not required to be unit length).
         * @param dy Ray direction Y.
         * @param dz Ray direction Z.
         * @param max_t Longest accepted hit distance along the ray.
         * @param out_t Receives the hit distance along the ray.
         * @param filter Which face orientations count as a hit.
         * @return True when a triangle is closer than max_t.
         */
        [[nodiscard]] bool raycast(float ox,
                                   float oy,
                                   float oz,
                                   float dx,
                                   float dy,
                                   float dz,
                                   float max_t,
                                   float& out_t,
                                   RayHitFilter filter = RayHitFilter::All) const;

        /**
         * Closest hit of a camera ray against walkable decks (indoor compatibility behavior
         * floors). Tests FLOOR geometry: upward faces with ny >=
         * kSlideSlope, no INNER_HEIGHT filter.
         * @param ox Ray origin X.
         * @param oy Ray origin Y.
         * @param oz Ray origin Z.
         * @param dx Ray direction X (not required to be unit length).
         * @param dy Ray direction Y.
         * @param dz Ray direction Z.
         * @param max_t Longest accepted hit distance along the ray.
         * @param out_t Receives the hit distance along the ray.
         * @return True when a walkable triangle is closer than max_t.
         */
        [[nodiscard]] bool raycast_walkable(float ox,
                                            float oy,
                                            float oz,
                                            float dx,
                                            float dy,
                                            float dz,
                                            float max_t,
                                            float& out_t) const;

        /**
         * True when dungeon geometry sits on the segment A→B, short of B by slack.
         * Used to hide indoor nameplates when a wall stands between player and mob.
         * @param ax Segment start X.
         * @param ay Segment start Y.
         * @param az Segment start Z.
         * @param bx Segment end X.
         * @param by Segment end Y.
         * @param bz Segment end Z.
         * @param slack Distance kept clear at B so props beside the target do not
         * hide it.
         * @return True when a blocker is hit before B minus slack.
         */
        [[nodiscard]] bool segment_blocked(float ax,
                                           float ay,
                                           float az,
                                           float bx,
                                           float by,
                                           float bz,
                                           float slack = kLosSlack) const;

        /**
         * Adds an already-transformed walkable deck. Used by tests and by add_prop.
         * @param prop World-space triangle soup to take ownership of.
         */
        void add_deck(FloorProp prop);

        /**
         * Adds an already-transformed camera-blocker hull. Used by tests.
         * @param prop World-space triangle soup to take ownership of.
         */
        void add_blocker_deck(FloorProp prop);

        /**
         * Drops a streamed KCM patch and its walkable OPL decks.
         * @param patch_x Horizontal patch index.
         * @param patch_y Vertical patch index.
         */
        void remove_patch(int patch_x, int patch_y);

        /** @return Terrain height field holding the loaded KCM patches. */
        [[nodiscard]] const terrain::HeightField& height_field() const {
            return m_height_field;
        }

        /** @return Walkable prop decks added so far. */
        [[nodiscard]] const std::vector<FloorProp>& props() const {
            return m_props;
        }

        /**
         * Samples the bare terrain, ignoring prop decks.
         * @param world_x World X.
         * @param world_z World Z.
         * @param fallback Height returned when no patch covers the position.
         * @return Interpolated terrain height.
         */
        [[nodiscard]] float
        sample_terrain(float world_x, float world_z, float fallback = 0.0f) const;

        /** Standing height. Outdoor GetFloor rays from terrain + 100*METER and never
         * returns below terrain. MovePlayer probes from the body, so fallback (current
         * Y) is included in the ray origin — otherwise a gorge bridge (runtime state) is
         * missed because KCM is the canyon floor. Indoor maps have no KCM: the ray
         * continues below the last Y so a dungeon ledge drops onto the floor mesh.
         * Surfaces more than kStepHeight above fallback (arch lintels, high decks)
         * are ignored so mouse-walk goes through doorways instead of onto them.
         * @param world_x World X.
         * @param world_z World Z.
         * @param fallback Current / last standing height, also used when no patch
         * covers the position and no walkable deck is hit.
         * @return Standing height at the position.
         */
        [[nodiscard]] float sample(float world_x, float world_z, float fallback = 0.0f) const;

        /** Resolves the ordinary standing height and occupancy in one geometry query.
         * @param world_x World X.
         * @param world_z World Z.
         * @param current_y Current feet height.
         * @param out_y Resolved standing height, including when occupancy is rejected.
         * @return True when the position can be occupied.
         */
        [[nodiscard]] bool
        resolve_standing(float world_x, float world_z, float current_y, float& out_y) const;

        /** Standing-height query. Indoor: first walkable deck from the map
         * ceiling down (LOW_FLOOR), independent of the local player height.
         * Outdoor: terrain plus the highest deck within 100*METER (no step-up
         * limit). MovePlayer uses sample() instead, so outdoor arches stay
         * passable.
         * @param world_x World X.
         * @param world_z World Z.
         * @return Standing height, or 0 when indoor and nothing was hit.
         */
        [[nodiscard]] float get_floor(float world_x, float world_z) const;

        /** True when indoor camera/wall hulls are loaded. */
        [[nodiscard]] bool has_blockers() const {
            return !m_blockers.empty();
        }

        /** True when the player may occupy this XZ at current_y.
         * False when the only walkable surface is a deck more than kStepHeight
         * above the player (dungeon ledge, outdoor arch lintel), or when indoor
         * and no walkable deck is hit. Outdoor terrain under a high deck stays
         * occupyable.
         * @param world_x World X.
         * @param world_z World Z.
         * @param current_y Current feet height used as the step-up reference.
         * @return False when the XZ is an unreachable raised floor.
         */
        [[nodiscard]] bool can_occupy(float world_x, float world_z, float current_y) const;

        /** Maximum step-up height (0.70 * kMeter); larger
         * vertical gaps start a fall instead of a snap.
         */
        static constexpr float kStepHeight = 7.0f;

        /** Walkable-slope threshold (cos 40 degrees); steeper faces are walls.
         */
        static constexpr float kSlideSlope = 0.766f;

        /** Clearance at the far end of a nameplate LOS segment. */
        static constexpr float kLosSlack = 8.0f;

        /**
         * Minimum prop height above terrain that counts as a standable deck rather
         * than sampling noise.
         */
        static constexpr float kStandableDeckEpsilon = 1.5f;

        /** Vertical tolerance for bridge decks near the water plane. */
        static constexpr float kWaterBridgeSlack = 48.0f;

        /**
         * Highest standable river-bridge deck above the water plane. Narootuh
         * bridge_01's arch crown sits ~208 above water; roofs over a river stay out.
         */
        static constexpr float kMaxBridgeAboveWater = 256.0f;

      private:
        /**
         * Returns the cached local-space triangle soup of a prop, loading it on first
         * use. Prefers GB compatibility behavior collision (GetFloor); falls back to solid
         * render triangles.
         * @param gb_path Path of the prop .gb mesh.
         * @return Flat triangle soup in model space; empty for foliage and unloadable
         * meshes.
         */
        [[nodiscard]] const std::vector<float>& local_tris_for(const std::string& gb_path);

        /**
         * Returns the cached camera-blocker triangle soup of a prop.
         * Uses solid render triangles (including two-sided walls/ceilings).
         * @param gb_path Path of the prop .gb mesh.
         * @return Flat triangle soup in model space.
         */
        [[nodiscard]] const std::vector<float>& blocker_tris_for(const std::string& gb_path);

        /**
         * Intersects a two-sided ray with one triangle.
         * @param ox Ray origin X.
         * @param oy Ray origin Y.
         * @param oz Ray origin Z.
         * @param dx Ray direction X.
         * @param dy Ray direction Y.
         * @param dz Ray direction Z.
         * @param max_t Longest accepted hit distance.
         * @param ax First vertex X.
         * @param ay First vertex Y.
         * @param az First vertex Z.
         * @param bx Second vertex X.
         * @param by Second vertex Y.
         * @param bz Second vertex Z.
         * @param cx Third vertex X.
         * @param cy Third vertex Y.
         * @param cz Third vertex Z.
         * @param out_t Receives the hit distance along the ray.
         * @return True on a hit closer than max_t.
         */
        [[nodiscard]] static bool ray_tri_any(float ox,
                                              float oy,
                                              float oz,
                                              float dx,
                                              float dy,
                                              float dz,
                                              float max_t,
                                              float ax,
                                              float ay,
                                              float az,
                                              float bx,
                                              float by,
                                              float bz,
                                              float cx,
                                              float cy,
                                              float cz,
                                              float& out_t);

        /** Indoor/outdoor stand query used by sample() and can_occupy().
         * @param world_x World X.
         * @param world_z World Z.
         * @param fallback Current / last standing height.
         * @param out_y Receives the standing height.
         * @param from_sky True to match GetFloor (highest deck, no step-up limit).
         * @return True when the XZ may be occupied at fallback.
         */
        [[nodiscard]] bool query_stand(float world_x,
                                       float world_z,
                                       float fallback,
                                       float& out_y,
                                       bool from_sky = false) const;

        /**
         * Closest hit of a world-space ray against one deck list.
         * @param decks Hulls to test.
         * @param ox Ray origin X.
         * @param oy Ray origin Y.
         * @param oz Ray origin Z.
         * @param dx Ray direction X.
         * @param dy Ray direction Y.
         * @param dz Ray direction Z.
         * @param max_t Longest accepted hit distance.
         * @param out_t Receives the hit distance along the ray.
         * @param filter Which face orientations count as a hit.
         * @param walkable_only True to keep only floors with ny >= kSlideSlope.
         * @param stop_when_blocked Stop once a hit blocks the segment before its endpoint slack.
         * @param slack Distance next to the endpoint that does not count as blocked.
         * @return True when a triangle is closer than max_t.
         */
        [[nodiscard]] bool raycast_hulls(const std::vector<FloorProp>& decks,
                                         float ox,
                                         float oy,
                                         float oz,
                                         float dx,
                                         float dy,
                                         float dz,
                                         float max_t,
                                         float& out_t,
                                         RayHitFilter filter,
                                         bool walkable_only,
                                         bool stop_when_blocked = false,
                                         float slack = 0.0f) const;

        /**
         * Intersects a straight-down ray with a triangle that can support walking.
         * @param ox Ray origin X.
         * @param oy Ray origin Y.
         * @param oz Ray origin Z.
         * @param max_t Longest accepted hit distance.
         * @param ax First vertex X.
         * @param ay First vertex Y.
         * @param az First vertex Z.
         * @param bx Second vertex X.
         * @param by Second vertex Y.
         * @param bz Second vertex Z.
         * @param cx Third vertex X.
         * @param cy Third vertex Y.
         * @param cz Third vertex Z.
         * @param out_t Receives the hit distance along the ray.
         * @param allow_short_risers Whether to accept narrow outdoor step faces.
         * @return True on a hit closer than max_t.
         */
        [[nodiscard]] static bool ray_tri_down(float ox,
                                               float oy,
                                               float oz,
                                               float max_t,
                                               float ax,
                                               float ay,
                                               float az,
                                               float bx,
                                               float by,
                                               float bz,
                                               float cx,
                                               float cy,
                                               float cz,
                                               float& out_t,
                                               bool allow_short_risers);

        /** Terrain height field built from the loaded KCM patches. */
        terrain::HeightField m_height_field;

        /** Walkable prop decks in world space. */
        std::vector<FloorProp> m_props;

        /** Width of one broad-phase cell for walkable prop decks. */
        static constexpr float kPropGridCellSize = 512.0f;

        /** Spatial grid from an XZ cell to candidate indices in runtime state. */
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_prop_grid;

        /** Spatial grid for blocker hulls, sharing the walkable-prop cell size. */
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_blocker_grid;

        /**
         * Adds one prop index to every broad-phase cell overlapped by its bounds.
         * @param prop_index Index of the prop in runtime state.
         * @param blocker True to index a blocker instead of a walkable deck.
         */
        void add_prop_to_grid(std::size_t prop_index, bool blocker = false);

        /** Rebuilds the broad-phase grid after runtime state indices change. */
        void rebuild_prop_grid();

        /**
         * Returns the candidate prop indices for one world position.
         * @param world_x World X.
         * @param world_z World Z.
         * @return Candidate prop indices, or nullptr when the grid cell is empty.
         */
        [[nodiscard]] const std::vector<std::size_t>* prop_candidates(float world_x,
                                                                      float world_z) const;

        /**
         * Builds a stable key from one broad-phase grid coordinate.
         * @param grid_x Broad-phase X coordinate.
         * @param grid_z Broad-phase Z coordinate.
         * @return Packed grid key.
         */
        [[nodiscard]] static std::uint64_t prop_grid_key(int grid_x, int grid_z);

        /**
         * Transforms a model-space triangle soup into a world-space blocker hull.
         * @param local Model-space xyz triplets.
         * @param world World matrix of the instance.
         * @param patch_x Horizontal patch index the instance belongs to.
         * @param patch_y Vertical patch index the instance belongs to.
         */
        void add_blocker_from_local(const std::vector<float>& local,
                                    const engine::Mat4& world,
                                    int patch_x,
                                    int patch_y);

        /** Camera-blocker hulls (indoor walls, ceilings, and object compatibility behavior). */
        std::vector<FloorProp> m_blockers;

        /** Model-space triangle soups keyed by .gb path, shared by every instance. */
        std::unordered_map<std::string, std::vector<float>> m_local_tris_cache;

        /** Model-space camera-blocker soups keyed by .gb path. */
        std::unordered_map<std::string, std::vector<float>> m_blocker_tris_cache;
    };

}  // namespace openkal::terrain
