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
#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "test_util.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

    void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
        output.push_back(static_cast<std::uint8_t>(value));
        output.push_back(static_cast<std::uint8_t>(value >> 8u));
    }

    void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
        append_u16(output, static_cast<std::uint16_t>(value));
        append_u16(output, static_cast<std::uint16_t>(value >> 16u));
    }

    struct ZipEntry {
        std::string name;
        std::string contents;
        std::uint32_t offset = 0;
    };

    std::vector<std::uint8_t> make_stored_zip(std::vector<ZipEntry>& entries) {
        std::vector<std::uint8_t> output;
        for (auto& entry : entries) {
            entry.offset = static_cast<std::uint32_t>(output.size());
            append_u32(output, 0x04034b50);
            append_u16(output, 20);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u32(output, 0);
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u16(output, static_cast<std::uint16_t>(entry.name.size()));
            append_u16(output, 0);
            output.insert(output.end(), entry.name.begin(), entry.name.end());
            output.insert(output.end(), entry.contents.begin(), entry.contents.end());
        }

        const std::uint32_t central_offset = static_cast<std::uint32_t>(output.size());
        for (const auto& entry : entries) {
            append_u32(output, 0x02014b50);
            append_u16(output, 20);
            append_u16(output, 20);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u32(output, 0);
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u16(output, static_cast<std::uint16_t>(entry.name.size()));
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u32(output, 0);
            append_u32(output, entry.offset);
            output.insert(output.end(), entry.name.begin(), entry.name.end());
        }
        const std::uint32_t central_size =
            static_cast<std::uint32_t>(output.size()) - central_offset;
        append_u32(output, 0x06054b50);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, static_cast<std::uint16_t>(entries.size()));
        append_u16(output, static_cast<std::uint16_t>(entries.size()));
        append_u32(output, central_size);
        append_u32(output, central_offset);
        append_u16(output, 0);
        return output;
    }

}  // namespace

int main() {
    using openkal::platform::ConfigArchive;
    using openkal::platform::File;
    using openkal::platform::Path;
    using openkal::test::expect;

    const std::string root = openkal::test::temp_dir("config_archive");
    const std::string config_root = File::join(root, "config");
    const std::string archive_path = File::join(root, "config.pak");
    std::vector<ZipEntry> entries = {
        {"config/layout.json", "{\"layout\":true}"},
        {"config/catalog/items.json", "{\"item\":1}"},
        {"RuntimeData/areas/n_032_032.akarea", "area"},
        {"RuntimeData/sounds/quest_done.mp3", "audio"},
    };
    expect(openkal::test::write_bytes(archive_path, make_stored_zip(entries)),
           "write stored config archive");

    std::string error;
    expect(ConfigArchive::mount(root, archive_path, &error), "mount stored config archive");
    expect(Path::is_directory(config_root), "archive config root is directory");
    expect(Path::is_directory(File::join(config_root, "catalog")), "archive child is directory");
    expect(File::exists(File::join(config_root, "layout.json")), "archive file exists");
    expect(File::exists(File::join(root, "RuntimeData/sounds/quest_done.mp3")),
           "archive RuntimeData file exists");
    const std::string areas_root = File::join(root, "RuntimeData/areas");
    expect(Path::is_directory(areas_root), "archive area-map root is directory");
    expect(File::exists(File::join(areas_root, "n_032_032.akarea")), "archive area map exists");
    expect(!File::exists(File::join(config_root, "missing.json")), "missing archive file absent");

    const auto bytes = File::read(File::join(config_root, "catalog/items.json"));
    expect(bytes && std::string(bytes->begin(), bytes->end()) == "{\"item\":1}",
           "read archive entry through File");
    const auto audio = File::read(File::join(root, "RuntimeData/sounds/quest_done.mp3"));
    expect(audio && std::string(audio->begin(), audio->end()) == "audio",
           "read RuntimeData entry through File");
    const auto area = File::read(File::join(areas_root, "n_032_032.akarea"));
    expect(area && std::string(area->begin(), area->end()) == "area",
           "read area map through config.pak");
    expect(!File::mtime_sec(File::join(config_root, "layout.json")).has_value(),
           "archive entry has no mtime");

    const auto files = Path::list_directory(config_root);
    expect(files.size() == 1 && files.front() == "layout.json", "archive direct file listing");
    const auto names = Path::list_directory_entries(config_root);
    expect(names.size() == 2 && names[0] == "catalog" && names[1] == "layout.json",
           "archive directory entry listing");
    const auto recursive = Path::list_files_recursive(config_root);
    expect(recursive.size() == 2 && recursive[0] == "catalog/items.json" &&
               recursive[1] == "layout.json",
           "archive recursive listing");

    ConfigArchive::unmount();
    expect(!Path::is_directory(config_root), "archive unmount restores filesystem lookup");

    const std::string loose_root = File::join(root, "loose-config");
    openkal::test::mkdir_p(loose_root);
    const auto loose_source = ConfigArchive::select(root, "loose-config", &error);
    expect(loose_source && loose_source->configuration_root == loose_root &&
               !loose_source->archive_path,
           "existing configuration_root selects loose files");
    expect(!File::exists(File::join(config_root, "layout.json")),
           "loose selection leaves config.pak unmounted");

    const auto packaged_source = ConfigArchive::select(root, {}, &error);
    expect(packaged_source && packaged_source->configuration_root == config_root &&
               packaged_source->archive_path == archive_path,
           "absent configuration_root falls back to config.pak");
    expect(File::exists(File::join(config_root, "layout.json")),
           "fallback package exposes configuration files");
    ConfigArchive::unmount();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
