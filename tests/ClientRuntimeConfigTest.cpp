/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientRuntimeConfig.h"

#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/File.h"
#include "test_util.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

int main() {
    using openkal::test::expect;
    const std::string save = "/tmp/openkal_runtime_settings_" + std::to_string(getpid());
    std::filesystem::remove_all(save);
    setenv("OPENKAL_SAVE_DIR", save.c_str(), 1);
    std::string profile = std::string(OPENKAL_REPO_ROOT) + "/config/profiles/default.profile";
    char program[] = "test";
    char* arguments[] = {program, profile.data(), nullptr};
    auto runtime = openkal::client::ClientRuntimeConfig::load(2, arguments);
    expect(runtime.has_value(), "load packaged profile without loose configuration");
    if (!runtime) {
        return 1;
    }
    const auto path = save + "/client_settings.profile";
    expect(runtime->profile_path() == path, "packaged profile uses writable client settings");
    expect(!std::filesystem::exists(path),
           "first settings draft does not require an existing file");
    auto settings = runtime->client_settings();
    settings.audio.bgm_volume = 2;
    settings.display.width = 1366;
    std::string error;
    expect(runtime->save_client_settings(settings, error),
           "first save creates missing directory and profile");
    expect(std::filesystem::is_regular_file(path), "settings exist as a physical file");
    auto again = openkal::client::ClientRuntimeConfig::load(2, arguments);
    expect(again && again->client_settings().audio.bgm_volume == 2 &&
               again->client_settings().display.width == 1366,
           "client settings survive restart");
    settings.audio.bgm_volume = 4;
    expect(runtime->save_client_settings(settings, error), "subsequent save updates file");
    expect(openkal::platform::File::write(path, "# external change\n"), "write concurrent edit");
    expect(!runtime->save_client_settings(settings, error), "preserve conflict protection");
    expect(runtime->client_settings().audio.bgm_volume == 4,
           "failed save preserves runtime settings");
    std::filesystem::remove_all(save);
    unsetenv("OPENKAL_SAVE_DIR");
    openkal::platform::ConfigArchive::unmount();
    return openkal::test::g_failures ? 1 : 0;
}
