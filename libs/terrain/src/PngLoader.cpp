/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/PngLoader.h"

#include "openkal/platform/File.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <png.h>
#include <utility>

namespace openkal::terrain {
    namespace {
        struct ImageCleanup {
            void operator()(png_image* image) const {
                png_image_free(image);
            }
        };
    }  // namespace

    bool load_png_rgba(const std::string& path, ImageRgba& out) {
        out = {};
        const auto bytes = platform::read_file(path);
        if (!bytes || bytes->empty()) {
            return false;
        }
        png_image image{};
        image.version = PNG_IMAGE_VERSION;
        const std::unique_ptr<png_image, ImageCleanup> cleanup(&image);
        if (!png_image_begin_read_from_memory(&image, bytes->data(), bytes->size())) {
            return false;
        }
        constexpr std::uint64_t kMaxPixels = 64u * 1024u * 1024u;
        const auto pixels = static_cast<std::uint64_t>(image.width) * image.height;
        if (image.width == 0 || image.height == 0 || pixels > kMaxPixels ||
            image.width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
            image.height > static_cast<png_uint_32>(std::numeric_limits<int>::max())) {
            return false;
        }
        image.format = PNG_FORMAT_RGBA;
        ImageRgba decoded;
        decoded.width = static_cast<int>(image.width);
        decoded.height = static_cast<int>(image.height);
        decoded.rgba.resize(static_cast<std::size_t>(pixels) * 4);
        if (!png_image_finish_read(&image, nullptr, decoded.rgba.data(), 0, nullptr)) {
            return false;
        }
        out = std::move(decoded);
        return true;
    }
}  // namespace openkal::terrain
