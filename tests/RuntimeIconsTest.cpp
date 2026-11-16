/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WidgetTools.h"
#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "test_util.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

int main() {
    using openkal::client::WidgetTools;
    using openkal::platform::ConfigArchive;
    using openkal::platform::File;
    using openkal::test::expect;

    const auto root = std::filesystem::temp_directory_path() /
                      ("openkal-runtime-icons-" + std::to_string(::getpid()));
    const auto external = root / "external";
    const auto resources = root / "packaged";
    std::filesystem::create_directories(external / "HyperText");
    const std::string runtime = (resources / "RuntimeData").string();
    std::string error;
    expect(ConfigArchive::mount(resources.string(), OPENKAL_TEST_CONFIG_ARCHIVE, &error),
           "mount generated package at a root without loose icons");

    openkal::terrain::ImageRgba image;
    expect(WidgetTools::load_bmp_rgba(File::join(runtime, "HyperText/expEvent.bmp"), image),
           "read generated EXP icon directly from package");
    expect(image.width == 32 && image.height == 32, "packaged BMP has inventory dimensions");
    expect(!image.rgba.empty() && image.rgba[3] == 0, "black background becomes transparent");

    for (const char* family : {"medicine", "magicPotion", "medicineAutoHp"}) {
        for (const char* size : {"Small", "Medium", "Large", "Huge"}) {
            const auto icon = std::string(family) + size + ".bmp";
            expect(WidgetTools::load_bmp_rgba(File::join(runtime, "HyperText/" + icon), image) &&
                       image.width == 32 && image.height == 32,
                   "all medicine and magic potion sizes ship as generated inventory icons");
        }
    }

    expect(WidgetTools::load_bmp_rgba(File::join(runtime, "HyperText/revisionHammer.bmp"), image) &&
               image.width == 32 && image.height == 32 && !image.rgba.empty() && image.rgba[3] == 0,
           "Revision Hammer ships its transparent hammer-and-chisel inventory icon");

    expect(WidgetTools::load_bmp_rgba(File::join(runtime, "HyperText/talismanOfEnchantment.bmp"),
                                      image) &&
               image.width == 32 && image.height == 32 && !image.rgba.empty() && image.rgba[3] == 0,
           "Talisman of Enchantment ships a transparent packaged inventory icon");

    std::vector<std::vector<std::uint8_t>> ranked_talisman_pixels;
    for (const char* numeral : {"I", "II", "III", "IIII", "V"}) {
        expect(WidgetTools::load_bmp_rgba(
                   File::join(runtime,
                              std::string("HyperText/talismanOfEnchantment") + numeral + ".bmp"),
                   image) &&
                   image.width == 32 && image.height == 32 && !image.rgba.empty() &&
                   image.rgba[3] == 0,
               "each talisman grade ships its painted numeral in a transparent 32px icon");
        expect(std::find(ranked_talisman_pixels.begin(),
                         ranked_talisman_pixels.end(),
                         image.rgba) == ranked_talisman_pixels.end(),
               "painted talisman grades remain distinct without a runtime text overlay");
        ranked_talisman_pixels.push_back(image.rgba);
    }

    // A differently sized external icon makes the precedence observable through TexInfo.
    auto external_bmp = File::read(File::join(runtime, "HyperText/invUnlock.bmp"));
    expect(external_bmp.has_value(), "packaged inventory icon exists");
    if (external_bmp) {
        (*external_bmp)[18] = 1;
        (*external_bmp)[22] = 1;
        expect(File::write((external / "HyperText/INVUNLOCK.BMP").string(),
                           external_bmp->data(),
                           external_bmp->size()),
               "write conflicting external icon");
        expect(File::write((external / "HyperText/externalOnly.bmp").string(),
                           external_bmp->data(),
                           external_bmp->size()),
               "write external fallback icon");
    }

    std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
    WidgetTools ui;
    ui.init(*engine, external.string(), runtime);
    const auto icon = ui.item_tex_info("INVUNLOCK.BMP");
    expect(icon.handle() && icon.width() == 32 && icon.height() == 32,
           "packaged icon wins over external icon despite casing");
    const auto fallback = ui.item_tex_info("externalOnly");
    expect(fallback.handle() && fallback.width() == 1 && fallback.height() == 1,
           "unowned icons still load from external client data");
    const auto exp = ui.item_tex_info("expEvent");
    expect(exp.handle() && exp.width() == 32 && exp.height() == 32,
           "EXP icon loads without any external counterpart");
    const auto globe = ui.tex_info("dragon_hud/orb_hp_full.bmp");
    expect(globe.handle() && globe.width() == 84 && globe.height() == 84,
           "HUD textures retain native size in packaged runtime");
    expect(WidgetTools::load_bmp_rgba(File::join(runtime, "UI/dragon_hud/orb_hp_full.bmp"), image),
           "load transparent HUD globe from package");
    expect(image.width == 84 && image.rgba[3] == 0 && image.rgba[(42 * 84 + 42) * 4 + 3] > 240,
           "HUD alpha preserves transparent corners and opaque center");
    const auto minimap = ui.tex_info("dragon_minimap/frame.bmp");
    expect(minimap.handle() && minimap.width() == 241 && minimap.height() == 258,
           "Dragon minimap frame retains native dimensions in package");
    expect(WidgetTools::load_bmp_rgba(File::join(runtime, "UI/dragon_minimap/frame.bmp"), image),
           "load Dragon minimap frame from package");
    expect(image.width == 241 && image.height == 258 && image.rgba[3] == 0 &&
               image.rgba[(149 * 241 + 130) * 4 + 3] == 0,
           "minimap frame keeps its aperture and outer corners transparent");
    ui.destroy();
    ConfigArchive::unmount();

    WidgetTools loose;
    loose.init(*engine, external.string());
    expect(loose.search_dirs().front() == File::join(OPENKAL_RUNTIME_DATA_ROOT, "UI"),
           "default lookup prioritizes the configured build/install icon root");
    loose.destroy();
    loose.init(*engine, external.string(), OPENKAL_TEST_GENERATED_RUNTIME_ROOT);
    const auto own = loose.item_tex_info("invUnlock");
    expect(own.handle() && own.width() == 32 && own.height() == 32,
           "default developer lookup uses generated loose icons");
    loose.destroy();
    std::filesystem::remove_all(root);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
