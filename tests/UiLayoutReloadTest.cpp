/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/UiLayout.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>

int main() {
    using openkal::test::expect;
    const auto root = std::filesystem::temp_directory_path() /
                      ("openkal-ui-reload-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto path = root / "test.json";
    const auto write = [&](int width) {
        std::ofstream file(path);
        file << R"({"schema":"openkal.ui/v1","window":{"w":)" << width << "}}";
    };
    write(100);
    openkal::client::UiLayoutStore store;
    store.set_directory(root.string());
    const auto& first = store.get("test");
    expect(first.loaded() && first.get_i("window.w") == 100, "load layout on first use");
    const auto time = std::filesystem::last_write_time(path);
    write(200);
    std::filesystem::last_write_time(path, time + std::chrono::seconds(2));
    expect(&store.get("test") == &first && first.get_i("window.w") == 100,
           "repeated reads use the cached layout until a development reload");
#if defined(OPENKAL_DISABLE_UI_HOT_RELOAD)
    expect(store.poll() == 0 && first.get_i("window.w") == 100,
           "release keeps the layout after the file changes");
    auto copy = first;
    expect(!copy.reload_if_stale() && copy.get_i("window.w") == 100,
           "release also disables direct hot reload requests");
#else
    expect(store.poll() == 1 && first.get_i("window.w") == 200,
           "development applies changed layouts");
#endif
    std::filesystem::remove_all(root);
    expect(store.poll() == 0 && store.get("test").loaded(),
           "cached UI remains usable after its source disappears");
    return openkal::test::g_failures == 0 ? 0 : 1;
}
