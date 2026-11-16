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
#include "openkal/map/IndoorMap.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/EnvLoader.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/GtxLoader.h"
#include "openkal/terrain/HeightPatch.h"
#include "test_util.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

int main() {
    using openkal::test::expect;

    const std::string maps = openkal::test::temp_dir("assets");
    const std::string tex = openkal::platform::join_path(maps, "Tex");
    expect(openkal::test::write_env_layers(openkal::platform::join_path(maps, "n.env"),
                                           {{"t0", 8}, {"t1", 4}},
                                           {{3, "DATA\\OBJECTS\\common\\grass"}}),
           "write n.env");
    expect(openkal::test::write_solid_dds(
               openkal::platform::join_path(tex, "t0.dds"), 8, 8, 40, 80, 40),
           "write t0.dds");
    expect(openkal::test::write_solid_dds(
               openkal::platform::join_path(tex, "t1.dds"), 8, 8, 80, 60, 40),
           "write t1.dds");
    expect(openkal::test::write_kcm_v7(
               openkal::platform::join_path(maps, "n_037_034.kcm"), 37, 34, 0, 1, 1500),
           "write kcm");

    std::vector<openkal::terrain::LayerInfo> layers;
    std::vector<openkal::terrain::DecoInfo> decorations;
    expect(openkal::terrain::load_environment(
               openkal::platform::join_path(maps, "n.env"), layers, decorations) &&
               layers.size() == 2 && decorations.size() == 1 && decorations.front().index == 3,
           "env layers");

    {
        openkal::terrain::HeightPatch patch;
        expect(openkal::terrain::load_kcm(openkal::platform::join_path(maps, "n_037_034.kcm"),
                                          patch) &&
                   patch.layers.size() >= 2 && !patch.deco_masks.empty() &&
                   patch.deco_masks.front() == 1 && patch.deco_palette.front() == 3,
               "load_kcm");
        patch.patch_x = 37;
        patch.patch_y = 34;
        openkal::terrain::ImageRgba img;
        expect(patch.bake_diffuse(tex, layers, img, 64) && !img.rgba.empty(), "bake_diffuse");

        std::vector<openkal::terrain::TerrainLayerGpu> gpu_layers;
        expect(patch.prepare_layers(tex, layers, gpu_layers) && gpu_layers.size() >= 2,
               "prepare_layers");
        expect(gpu_layers[0].repeat > 1.0f && !gpu_layers[0].detail.rgba.empty(), "detail loaded");
    }

    {
        openkal::terrain::ImageRgba img;
        expect(openkal::terrain::load_gtx_rgba(openkal::platform::join_path(tex, "t0.dds"), img) &&
                   img.width == 8 && img.height == 8,
               "dds load");
    }

    {
        // Type-10 RLE true-color TGA (same packing as Effect/Tex/fire_black.tga).
        const std::string tga = openkal::platform::join_path(tex, "rle.tga");
        std::vector<std::uint8_t> bytes(18 + 1 + 4, 0);
        bytes[2] = 10;  // RLE true-color
        bytes[12] = 2;  // width 2
        bytes[13] = 0;
        bytes[14] = 1;  // height 1
        bytes[15] = 0;
        bytes[16] = 32;    // bpp
        bytes[18] = 0x81;  // RLE packet, 2 pixels
        bytes[19] = 10;    // B
        bytes[20] = 20;    // G
        bytes[21] = 30;    // R
        bytes[22] = 255;   // A
        expect(openkal::platform::write_file(tga, bytes.data(), bytes.size()), "write rle tga");
        openkal::terrain::ImageRgba img;
        expect(openkal::terrain::load_tga_rgba(tga, img) && img.width == 2 && img.height == 1,
               "rle tga load");
        expect(!img.rgba.empty() && img.rgba[0] == 30 && img.rgba[1] == 20 && img.rgba[2] == 10 &&
                   img.rgba[3] == 255,
               "rle tga pixel");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
