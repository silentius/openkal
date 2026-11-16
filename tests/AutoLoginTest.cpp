/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/AutoLogin.h"

#include "test_util.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    using openkal::client::load_auto_login;
    using openkal::test::expect;
    const auto previous = std::filesystem::current_path();
    const auto directory =
        std::filesystem::temp_directory_path() / ("openkal_auto_login_" + std::to_string(getpid()));
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    expect(!load_auto_login(), "missing optional file leaves manual login available");
    const auto write = [](const std::string& text) {
        std::ofstream file("auto_login.json", std::ios::binary | std::ios::trunc);
        file << text;
        file.close();
        expect(file.good(), "write project-owned synthetic credentials");
    };
    write(R"({"login":[{"id":"FirstUser","password":"a long pass phrase"},
                       {"id":"SecondUser","password":"another password"}]})");
    auto credentials = load_auto_login();
    expect(credentials && credentials->id == "FirstUser" &&
               credentials->password == "a long pass phrase",
           "default path uses the working directory and selects the first login");
    write(R"({"login":[{"id":"ChangedUser","password":"quote\"slash\\password"}]})");
    credentials = load_auto_login();
    expect(credentials && credentials->id == "ChangedUser" &&
               credentials->password == "quote\"slash\\password",
           "reread changed credentials and decode JSON escapes without altering the password");
    for (const std::string text :
         {"",
          "{",
          "{}",
          "[]",
          R"({"login":{}})",
          R"({"login":[]})",
          R"({"login":[null]})",
          R"({"login":[{"id":"User"}]})",
          R"({"login":[{"id":123,"password":"long enough password"}]})",
          R"({"login":[{"id":"User","password":123}]})",
          R"({"login":[{"id":"","password":"long enough password"}]})",
          R"({"login":[{"id":"User","password":"short"}]})",
          R"({"login":[{"id":"User\n","password":"long enough password"}]})",
          R"({"login":[{"id":"User","password":"password\nwith control"}]})",
          R"({"login":[{"id":"User","password":"long enough password",}]})",
          R"({"login":[{}, {"id":"User","password":"long enough password"}]})"}) {
        write(text);
        expect(!load_auto_login(), "malformed or invalid first entry falls back to manual login");
    }
    write("{\"login\":[{\"id\":\"" + std::string(33, 'u') +
          "\",\"password\":\"long enough password\"}]}");
    expect(!load_auto_login(), "overlong username is not truncated");
    write("{\"login\":[{\"id\":\"User\",\"password\":\"" + std::string(129, 'p') + "\"}]}");
    expect(!load_auto_login(), "overlong password is not truncated");
    std::filesystem::remove("auto_login.json");
    expect(!load_auto_login(), "removing the file disables subsequent auto-login attempts");
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(directory);
    return openkal::test::g_failures ? 1 : 0;
}
