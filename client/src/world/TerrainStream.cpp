/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/TerrainStream.h"

#include "openkal/platform/File.h"

namespace openkal::client {

    TerrainStream::TerrainStream(std::string maps_dir,
                                 std::string tex_dir,
                                 std::vector<terrain::LayerInfo> env_layers)
        : m_maps_dir(std::move(maps_dir)), m_tex_dir(std::move(tex_dir)),
          m_env_layers(std::move(env_layers)), m_worker([this] { run(); }) {}

    TerrainStream::~TerrainStream() {
        m_stop_requested.store(true);
        m_request_cv.notify_all();
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    bool TerrainStream::RequestOrder::operator()(const Request& left, const Request& right) const {
        return left.priority == right.priority ? left.sequence > right.sequence
                                               : left.priority < right.priority;
    }

    bool TerrainStream::request(int patch_x, int patch_y, int priority) {
        const std::uint64_t key = patch_key(patch_x, patch_y);
        std::scoped_lock lock(m_mutex);
        if (!m_pending.insert(key).second) {
            return false;
        }
        m_requests.push({patch_x, patch_y, priority, m_next_sequence++});
        m_request_cv.notify_one();
        return true;
    }

    std::optional<TerrainStream::ReadyPatch> TerrainStream::take_ready() {
        std::scoped_lock lock(m_mutex);
        if (m_ready.empty()) {
            return std::nullopt;
        }
        ReadyPatch ready = std::move(m_ready.front());
        m_ready.pop_front();
        m_pending.erase(patch_key(ready.patch_x, ready.patch_y));
        m_request_cv.notify_one();
        return ready;
    }

    std::uint64_t TerrainStream::patch_key(int patch_x, int patch_y) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(patch_x)) << 32) |
               static_cast<std::uint32_t>(patch_y);
    }

    void TerrainStream::run() {
        while (!m_stop_requested.load()) {
            Request request;
            {
                std::unique_lock lock(m_mutex);
                m_request_cv.wait(
                    lock, [this] { return m_stop_requested.load() || !m_requests.empty(); });
                if (m_stop_requested.load()) {
                    return;
                }
                request = m_requests.top();
                m_requests.pop();
            }

            ReadyPatch ready;
            ready.patch_x = request.patch_x;
            ready.patch_y = request.patch_y;
            const std::string path =
                WorldRenderCache::patch_path(m_maps_dir, request.patch_x, request.patch_y);
            terrain::HeightPatch patch;
            if (platform::file_exists(path) && terrain::load_kcm(path, patch)) {
                patch.patch_x = request.patch_x;
                patch.patch_y = request.patch_y;
                ready.prepared =
                    WorldRenderCache::prepare_patch(std::move(patch), m_tex_dir, m_env_layers);
            }

            std::unique_lock lock(m_mutex);
            m_request_cv.wait(lock, [this] {
                return m_stop_requested.load() || m_ready.size() < kMaxReadyPatches;
            });
            if (m_stop_requested.load()) {
                return;
            }
            m_ready.push_back(std::move(ready));
        }
    }

}  // namespace openkal::client
