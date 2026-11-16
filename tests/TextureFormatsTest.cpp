/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/GtxLoader.h"
#include "openkal/terrain/PngLoader.h"
#include "test_util.h"

#include <filesystem>
#include <png.h>

namespace {
    const std::vector<std::uint8_t> kCorners = {
        255, 0, 0, 128, 0, 255, 0, 255, 0, 0, 255, 0, 255, 255, 255, 64};

    bool write_png(const std::string& path) {
        png_image image{};
        image.version = PNG_IMAGE_VERSION;
        image.width = image.height = 2;
        image.format = PNG_FORMAT_RGBA;
        return png_image_write_to_file(&image, path.c_str(), 0, kCorners.data(), 0, nullptr) != 0;
    }

    bool write_tga(const std::string& path, bool top, bool right, bool rle) {
        std::vector<std::uint8_t> data(18, 0);
        data[2] = rle ? 10 : 2;
        data[12] = data[14] = 2;
        data[16] = 32;
        data[17] = static_cast<std::uint8_t>(8 | (top ? 32 : 0) | (right ? 16 : 0));
        if (rle) {
            data.push_back(3);  // Four literal pixels in an RLE stream.
        }
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                const auto i =
                    static_cast<std::size_t>(((top ? y : 1 - y) * 2 + (right ? 1 - x : x)) * 4);
                data.insert(data.end(),
                            {kCorners[i + 2], kCorners[i + 1], kCorners[i], kCorners[i + 3]});
            }
        }
        return openkal::test::write_bytes(path, data);
    }
}  // namespace

int main() {
    using openkal::test::expect;
    const auto root = openkal::test::temp_dir("texture_formats");
    std::filesystem::create_directories(root + "/Tex");
    const auto png = root + "/Tex/CoRnEr.PnG";
    const auto tga = root + "/Tex/boot.TgA";
    const auto model = root + "/owned.gb";
    expect(write_png(png), "write owned RGBA PNG with four distinct corners");
    openkal::terrain::ImageRgba image;
    expect(openkal::terrain::load_png_rgba(png, image) && image.width == 2 && image.height == 2 &&
               image.rgba == kCorners,
           "PNG preserves row order, RGB and independent alpha");
    expect(openkal::test::write_rigid_gb_v10(model, "corner.png"), "write owned GB fixture");
    expect(openkal::terrain::load_gb_texture(model, "corner.PNG", image) && image.rgba == kCorners,
           "GB texture resolver loads mixed-case PNG files");
    for (const bool top : {false, true}) {
        for (const bool right : {false, true}) {
            for (const bool rle : {false, true}) {
                expect(write_tga(tga, top, right, rle), "write owned oriented TGA");
                expect(openkal::terrain::load_gb_texture(model, "BOOT.tga", image) &&
                           image.rgba == kCorners,
                       "GB texture resolver preserves all TGA origins and alpha");
            }
        }
    }
    std::vector<std::uint8_t> run(18, 0);
    run[2] = 10;
    run[12] = 2;
    run[14] = 1;
    run[16] = 24;
    run.insert(run.end(), {0x81, 30, 20, 10});
    expect(openkal::test::write_bytes(tga, run) && openkal::terrain::load_tga_rgba(tga, image) &&
               image.rgba == std::vector<std::uint8_t>({10, 20, 30, 255, 10, 20, 30, 255}),
           "TGA repeated packet expands opaque 24-bit pixels");
    const auto dds = root + "/Tex/cOrNeR.dDs";
    expect(openkal::test::write_solid_dds(dds, 4, 4, 12, 34, 56),
           "write legacy precedence fixture");
    expect(openkal::terrain::load_gb_texture(model, "corner.png", image) && image.width == 4 &&
               image.rgba[0] == 12 && image.rgba[1] == 34 && image.rgba[2] == 56,
           "mixed-case DDS keeps precedence over newer PNG alternatives");
    expect(openkal::test::write_bytes(png, {1, 2, 3, 4}) &&
               !openkal::terrain::load_png_rgba(png, image) && image.width == 0 &&
               image.rgba.empty(),
           "invalid PNG clears a previous image");
    run.resize(18);
    run[0] = 255;
    expect(openkal::test::write_bytes(tga, run) && !openkal::terrain::load_tga_rgba(tga, image) &&
               image.rgba.empty(),
           "truncated TGA identifier is rejected before pixel access");
    std::filesystem::remove_all(root);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
