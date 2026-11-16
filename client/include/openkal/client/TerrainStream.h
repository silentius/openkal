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

#include "openkal/client/WorldRenderCache.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace openkal::client {

    /** Background compatibility behavior loader for outdoor terrain patches. */
    class TerrainStream {
      public:
        /** A patch that is ready for a render-thread GPU upload. */
        class ReadyPatch {
          public:
            /** Outdoor patch X coordinate. */
            int patch_x = 0;

            /** Outdoor patch Y coordinate. */
            int patch_y = 0;

            /** compatibility behavior-prepared patch, absent when the source cannot be loaded. */
            std::optional<WorldRenderCache::PreparedPatch> prepared;
        };

        /**
         * Starts the single background worker.
         * @param maps_dir Directory containing KCM source files.
         * @param tex_dir Directory containing terrain detail textures.
         * @param env_layers Terrain layer definitions used while preparing patches.
         */
        TerrainStream(std::string maps_dir,
                      std::string tex_dir,
                      std::vector<terrain::LayerInfo> env_layers);

        /** Stops the worker and releases pending compatibility behavior data. */
        ~TerrainStream();

        TerrainStream(const TerrainStream&) = delete;

        TerrainStream& operator=(const TerrainStream&) = delete;

        /**
         * Queues a patch unless it is already queued or being prepared.
         * @param patch_x Outdoor patch X coordinate.
         * @param patch_y Outdoor patch Y coordinate.
         * @param priority Higher values are prepared first.
         * @return True when a new request was queued.
         */
        [[nodiscard]] bool request(int patch_x, int patch_y, int priority);

        /**
         * Removes the next completed patch without touching GPU resources.
         * @return A completed patch when one is available.
         */
        [[nodiscard]] std::optional<ReadyPatch> take_ready();

      private:
        /** One pending terrain request ordered by priority and FIFO sequence. */
        class Request {
          public:
            /** Outdoor patch X coordinate. */
            int patch_x = 0;

            /** Outdoor patch Y coordinate. */
            int patch_y = 0;

            /** Higher values are processed first. */
            int priority = 0;

            /** FIFO sequence for equal priorities. */
            std::uint64_t sequence = 0;
        };

        /** Orders highest-priority and oldest requests first. */
        class RequestOrder {
          public:
            /**
             * Compares two requests for the priority queue.
             * @param left First request.
             * @param right Second request.
             * @return True when left must follow right.
             */
            [[nodiscard]] bool operator()(const Request& left, const Request& right) const;
        };

        /** Runs file I/O, decoding, and mesh generation on the worker thread. */
        void run();

        /** Builds a stable key from an outdoor patch coordinate. */
        [[nodiscard]] static std::uint64_t patch_key(int patch_x, int patch_y);

        /** Source directory for KCM files. */
        std::string m_maps_dir;

        /** Source directory for terrain detail textures. */
        std::string m_tex_dir;

        /** Immutable terrain-layer definitions used by the worker. */
        std::vector<terrain::LayerInfo> m_env_layers;

        /** Guards request, ready, and pending collections. */
        std::mutex m_mutex;

        /** Wakes the worker when a new request arrives or shutdown begins. */
        std::condition_variable_any m_request_cv;

        /** Pending requests ordered by player-facing priority. */
        std::priority_queue<Request, std::vector<Request>, RequestOrder> m_requests;

        /** Finished compatibility behavior patches awaiting a render-thread upload. */
        std::deque<ReadyPatch> m_ready;

        /** Keys that are queued, loading, or waiting in the ready queue. */
        std::unordered_set<std::uint64_t> m_pending;

        /** Monotonically increasing FIFO sequence. */
        std::uint64_t m_next_sequence = 0;

        /** Maximum number of completed compatibility behavior patches retained before GPU upload.
         */
        static constexpr std::size_t kMaxReadyPatches = 4;

        /** Signals that the worker must leave its wait loop. */
        std::atomic_bool m_stop_requested = false;

        /** Owns and stops the background loader. */
        std::thread m_worker;
    };

}  // namespace openkal::client
