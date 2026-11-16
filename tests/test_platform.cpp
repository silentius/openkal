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
#include "openkal/platform/Clock.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/Profile.h"
#include "openkal/platform/RuntimeProfile.h"
#include "test_util.h"

#include <chrono>
#include <clocale>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

int main() {
    using openkal::test::expect;

    {
        const auto a = openkal::platform::monotonic_ms();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const auto b = openkal::platform::monotonic_ms();
        expect(b >= a + 4, "monotonic_ms advances");
    }

    expect(openkal::platform::unix_time_seconds() > 1700000000, "unix_time_seconds sane");

    {
        const auto p = openkal::platform::join_path("a/b", "c");
        expect(p == "a/b/c", "join_path basic");
        expect(openkal::platform::join_path("a/", "/b") == "a/b", "join_path slash");
    }

    {
        const char* tmp = std::getenv("TMPDIR");
        const std::string dir = tmp ? tmp : "/tmp";
        const std::string file = openkal::platform::join_path(dir, "openkal_plat_test.txt");
        {
            std::ofstream out(file, std::ios::binary);
            out << "ok";
        }
        expect(openkal::platform::file_exists(file), "file_exists");
        const auto bytes = openkal::platform::read_file(file);
        expect(bytes && bytes->size() == 2 && (*bytes)[0] == 'o', "read_file");
        std::remove(file.c_str());
        expect(!openkal::platform::read_file(dir).has_value(), "read_file rejects directory");
    }

    {
        const char* tmp = std::getenv("TMPDIR");
        const std::string dir = tmp ? tmp : "/tmp";
        expect(openkal::platform::is_directory(dir), "is_directory");
        (void) openkal::platform::list_directory(dir);
        expect(true, "list_directory");
    }

    {
        const auto cfg = openkal::platform::config_dir();
        expect(cfg.find("openkal") != std::string::npos, "config_dir contains openkal");
    }

    {
        const std::string path = openkal::platform::Profile::default_path(OPENKAL_REPO_ROOT);
        auto profile = openkal::platform::Profile::load(path);
        expect(static_cast<bool>(profile), "load default profile");
        if (profile) {
            expect(profile->name() == "default", "profile name");
            expect(!profile->network_enabled(), "network disabled in default profile");
            expect(!profile->data_root().empty(), "data_root set");
            expect(!profile->maps_subdir().empty(), "maps_subdir set");
            expect(profile->configuration_root().empty(),
                   "default profile selects packaged configuration");
            expect(profile->spawn_server_x() > 0 && profile->spawn_server_y() > 0,
                   "spawn coords set");
        }
    }

    {
        expect(openkal::platform::is_profile_path("openkal.profile"), "profile extension");
        expect(openkal::platform::is_profile_path("OPENKAL.PROFILE"),
               "profile extension ignores case");
        expect(!openkal::platform::is_profile_path("model.gb"), "non-profile extension");
    }

    {
        const std::string root = openkal::test::temp_dir("runtime_profile");
        const std::string data_root = openkal::platform::join_path(root, "client_data");
        const std::string map_root = openkal::platform::join_path(root, "map");
        const std::string server_map_root = openkal::platform::join_path(root, "server_map");
        const std::string configuration_root = openkal::platform::join_path(root, "custom_config");
        const std::string tool_configuration_root =
            openkal::platform::join_path(root, "tool_custom_config");
        openkal::test::mkdir_p(data_root);
        openkal::test::mkdir_p(map_root);
        openkal::test::mkdir_p(server_map_root);
        openkal::test::mkdir_p(configuration_root);
        openkal::test::mkdir_p(tool_configuration_root);
        const std::string profile_path = openkal::platform::join_path(root, "openkal.profile");
        {
            std::ofstream output(profile_path);
            output << "name=tool-test\n"
                   << "data_root=" << data_root << '\n'
                   << "map_root=" << map_root << '\n'
                   << "server_map_root=" << server_map_root << '\n'
                   << "configuration_root=" << configuration_root << '\n';
        }

        std::string error;
        const auto runtime = openkal::platform::RuntimeProfile::load(root, profile_path, &error);
        expect(static_cast<bool>(runtime), "load runtime profile");
        if (runtime) {
            expect(runtime->profile_path == profile_path, "runtime profile path");
            expect(runtime->profile.data_root() == data_root, "runtime data root");
            expect(runtime->profile.map_root() == map_root, "runtime map root");
            expect(runtime->profile.server_map_root() == server_map_root,
                   "runtime server map root");
            expect(runtime->configuration.configuration_root == configuration_root,
                   "runtime configuration root");
            expect(!runtime->configuration.archive_path.has_value(),
                   "runtime loose configuration selected");
        }
        expect(error.empty(), "runtime profile has no error");

        error.clear();
        const auto tool_runtime = openkal::platform::RuntimeProfile::load_for_tool(
            root, profile_path, &error, tool_configuration_root);
        expect(static_cast<bool>(tool_runtime), "load runtime profile with tool config override");
        if (tool_runtime) {
            expect(tool_runtime->requested_configuration_root == tool_configuration_root,
                   "tool config override is recorded");
            expect(tool_runtime->configuration.configuration_root == tool_configuration_root,
                   "tool config override selects loose root");
        }
        expect(error.empty(), "tool config override has no error");
    }

    {
        using openkal::platform::resolve_client_asset;
        using openkal::platform::strip_client_data_prefix;
        expect(strip_client_data_prefix("DATA\\OBJECTS\\x") == "OBJECTS/x", "strip DATA");
        expect(strip_client_data_prefix("Data/OBJECTS/x") == "OBJECTS/x", "strip Data");
        expect(strip_client_data_prefix("data/OBJECTS/x") == "OBJECTS/x", "strip data");

        const std::string root = openkal::test::temp_dir("platform_data");
        const std::string tree_dir = openkal::platform::join_path(root, "OBJECTS/b/1/land");
        const std::string stair_dir = openkal::platform::join_path(root, "OBJECTS/b/1/structure");
        openkal::test::mkdir_p(tree_dir);
        openkal::test::mkdir_p(stair_dir);
        expect(openkal::test::write_bytes(openkal::platform::join_path(tree_dir, "[a]tree_d02.gb"),
                                          {1, 2, 3}),
               "write tree gb stub");
        expect(openkal::test::write_bytes(openkal::platform::join_path(stair_dir, "w_stair01.gb"),
                                          {4, 5, 6}),
               "write stair gb stub");

        const auto p1 = resolve_client_asset(root, "Data\\OBJECTS\\b\\1\\land\\[a]tree_d02", ".gb");
        expect(p1.find("/Data/") == std::string::npos, "no nested Data dir");
        expect(p1.find("OBJECTS") != std::string::npos, "has OBJECTS");
        expect(openkal::platform::file_exists(p1), "tree gb resolves on disk");

        const auto p2 =
            resolve_client_asset(root, "DATA\\OBJECTS\\b\\1\\structure\\w_stair01", ".gb");
        expect(openkal::platform::file_exists(p2), "stair gb resolves on disk");

        expect(resolve_client_asset(root, "", ".gb").empty(), "empty client path");
        expect(resolve_client_asset(root, "data\\", ".gb").empty(), "data-only client path");
    }

    {
        const std::string root = openkal::test::temp_dir("clothes_case");
        expect(openkal::test::write_bytes(openkal::platform::join_path(root, "CW_12_H01.gb"), {1}),
               "write mixed-case helmet stub");
        const auto p = openkal::platform::Path::resolve_path_case(root, "Cw_12_h01.gb");
        expect(openkal::platform::file_exists(p), "helmet gb resolves ignoring case");
        expect(p.find("CW_12_H01.gb") != std::string::npos, "resolved on-disk helmet casing");
    }

    {
        const char* previous = std::setlocale(LC_NUMERIC, nullptr);
        const std::string previous_copy = previous ? previous : "C";
        if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") == nullptr) {
            (void) std::setlocale(LC_NUMERIC, "de_DE");
        }

        openkal::platform::Json value;
        value.set_number(0.5);
        const std::string text = openkal::platform::json_stringify(value, 0);
        expect(text.find("0.5") != std::string::npos, "json writes fractional 0.5 with period");
        expect(text.find(',') == std::string::npos, "json number has no locale comma");
        std::setlocale(LC_NUMERIC, previous_copy.c_str());
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
