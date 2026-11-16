/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/EnvLoader.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"

#include <cstring>
#include <string>

namespace openkal::terrain {
    namespace {

        bool read_pod(const std::vector<std::uint8_t>& src,
                      std::size_t& offset,
                      void* dst,
                      std::size_t nbytes) {
            if (offset + nbytes > src.size()) {
                return false;
            }
            std::memcpy(dst, src.data() + offset, nbytes);
            offset += nbytes;
            return true;
        }

    }  // namespace

    bool EnvLoader::load(const std::string& path,
                         std::vector<LayerInfo>& out,
                         std::vector<DecoInfo>& decorations) {
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::error("load_env_layers: cannot read " + path);
            return false;
        }
        if (bytes->size() < 36) {
            return false;
        }

        std::uint32_t version = 0;
        std::memcpy(&version, bytes->data() + 32, 4);
        std::size_t offset = 36;

        std::int32_t ndeco = 0;
        if (!read_pod(*bytes, offset, &ndeco, sizeof(ndeco))) {
            return false;
        }
        if (ndeco < 0) {
            return false;
        }
        decorations.clear();
        decorations.reserve(static_cast<std::size_t>(ndeco));
        for (std::int32_t i = 0; i < ndeco; ++i) {
            std::int32_t index = 0;
            std::int32_t size = 0;
            if (!read_pod(*bytes, offset, &index, 4) || !read_pod(*bytes, offset, &size, 4)) {
                return false;
            }
            if (size < 0 || offset + static_cast<std::size_t>(size) > bytes->size()) {
                return false;
            }
            DecoInfo info;
            info.index = index;
            info.model_name.assign(reinterpret_cast<const char*>(bytes->data() + offset),
                                   static_cast<std::size_t>(size));
            while (!info.model_name.empty() &&
                   static_cast<unsigned char>(info.model_name.back()) <= 0x20u) {
                info.model_name.pop_back();
            }
            offset += static_cast<std::size_t>(size);
            decorations.push_back(std::move(info));
        }

        // Sun light table (version >= 4): 24 * 5 * 4 bytes.
        if (version >= 4) {
            if (offset + 24 * 5 * 4 > bytes->size()) {
                return false;
            }
            offset += 24 * 5 * 4;
        }

        std::uint32_t layer_count = 0;
        if (!read_pod(*bytes, offset, &layer_count, sizeof(layer_count))) {
            return false;
        }
        out.clear();
        out.reserve(layer_count);
        for (std::uint32_t i = 0; i < layer_count; ++i) {
            LayerInfo info;
            std::int32_t name_len = 0;
            if (!read_pod(*bytes, offset, &info.scale_u, 4) ||
                !read_pod(*bytes, offset, &info.scale_v, 4) ||
                !read_pod(*bytes, offset, &name_len, 4)) {
                return false;
            }
            if (name_len < 0 || offset + static_cast<std::size_t>(name_len) > bytes->size()) {
                return false;
            }
            info.texture_name.assign(reinterpret_cast<const char*>(bytes->data() + offset),
                                     static_cast<std::size_t>(name_len));
            offset += static_cast<std::size_t>(name_len);
            out.push_back(std::move(info));
        }
        log::info("load_environment: " + std::to_string(out.size()) + " layers, " +
                  std::to_string(decorations.size()) + " decorations from " + path);
        return true;
    }

    bool EnvLoader::load(const std::string& path, std::vector<LayerInfo>& out) {
        std::vector<DecoInfo> decorations;
        return load(path, out, decorations);
    }

    bool load_env_layers(const std::string& path, std::vector<LayerInfo>& out) {
        return EnvLoader::load(path, out);
    }

    bool load_environment(const std::string& path,
                          std::vector<LayerInfo>& layers,
                          std::vector<DecoInfo>& decorations) {
        return EnvLoader::load(path, layers, decorations);
    }

}  // namespace openkal::terrain
