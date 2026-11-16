/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/ManagementApi.h"

#include "test_util.h"

#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using J = platform::Json;

    void deferred_requests(server::ManagementApi& api,
                           const std::string& path,
                           const std::string& token) {
        for (int outcome = 0; outcome < 3; ++outcome) {
            bool ready = false;
            int executions = 0, checks = 0, immediate_executions = 0;
            const auto handler = [&](const J& input) -> server::ManagementReply {
                if (input.at("op").as_string() != "deferred") {
                    ++immediate_executions;
                    return J::parse(R"({"marker":7})");
                }
                ++executions;
                return server::ManagementReply{[&]() -> std::optional<J> {
                    ++checks;
                    if (!ready) {
                        return std::nullopt;
                    }
                    if (outcome == 2) {
                        throw std::runtime_error("owned deferred failure");
                    }
                    return outcome == 0 ? J::parse(R"({"ok":true})")
                                        : server::management_error("Owned delivery failed.");
                }};
            };
            const auto pump_until = [&](const auto& condition) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                do {
                    api.poll(handler);
                    if (condition()) {
                        return true;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                } while (std::chrono::steady_clock::now() < deadline);
                return false;
            };
            auto pending = std::async(std::launch::async, [&] {
                return server::management_request(path, token, J::parse(R"({"op":"deferred"})"));
            });
            test::expect(pump_until([&] { return checks >= 2; }) && executions == 1 &&
                             pending.wait_for(std::chrono::milliseconds(0)) !=
                                 std::future_status::ready,
                         "a deferred management operation executes once and remains unanswered "
                         "while pending");
            auto immediate = std::async(std::launch::async, [&] {
                return server::management_request(path, token, J::parse(R"({"op":"immediate"})"));
            });
            const bool progressed = pump_until([&] {
                return immediate.wait_for(std::chrono::milliseconds(0)) ==
                       std::future_status::ready;
            });
            test::expect(progressed && immediate.get().at("marker").as_int() == 7 &&
                             immediate_executions == 1 && executions == 1 &&
                             pending.wait_for(std::chrono::milliseconds(0)) !=
                                 std::future_status::ready,
                         "another management peer completes while the first reply is deferred");
            ready = true;
            const bool completed = pump_until([&] {
                return pending.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
            });
            const auto result = pending.get();
            const bool correct = outcome == 0 ? result.at("ok").boolean()
                                              : result.at("error").as_string() ==
                                                    (outcome == 1 ? "Owned delivery failed."
                                                                  : "Management operation failed.");
            test::expect(completed && correct && executions == 1,
                         "deferred success, delivery failure and callback exception each return "
                         "one terminal response");
            const auto final_checks = checks;
            api.poll(handler);
            test::expect(checks == final_checks && executions == 1,
                         "completed management peers cannot execute or poll their operation again");
        }
    }
}  // namespace

int main() {
    using namespace openkal;
    using test::expect;
    using J = platform::Json;
    const auto directory =
        std::filesystem::temp_directory_path() / ("openkal-management-" + std::to_string(getpid()));
    std::filesystem::create_directories(directory);
    const auto path = (directory / "api.sock").string();
    const std::string token(32, 't');
    server::ManagementApi api;
    std::string error;
    expect(!api.start(path, "short", error) && !error.empty(),
           "management rejects short authentication tokens");
    expect(api.start(path, token, error) && api.enabled(),
           "start owner-only local management endpoint");
    struct stat info {};
    expect(::stat(path.c_str(), &info) == 0 && (info.st_mode & 0777) == 0600,
           "management socket is accessible only by its owner");
    const auto request = [&](const std::string& secret, const J& command) {
        auto pending = std::async(
            std::launch::async, [&] { return server::management_request(path, secret, command); });
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        while (pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
               std::chrono::steady_clock::now() < until) {
            api.poll([&](const J& input) {
                expect(input.at("token").is_null(),
                       "management strips the token before executing world operations");
                return api.events(static_cast<std::uint64_t>(input.at("after").number()));
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return pending.get();
    };
    expect(!request(std::string(32, 'x'), J::parse("{}")).at("error").as_string().empty(),
           "wrong management token cannot invoke the handler");
    api.record_pm(7, "OwnedHero", "hello", false);
    auto result = request(token, J::parse(R"({"op":"status","after":0})"));
    expect(result.at("events").array().size() == 1 &&
               result.at("events").array()[0].at("character").as_int() == 7,
           "authenticated support event polling preserves sender identity");
    expect(request(token, J::parse(R"({"after":1})")).at("events").array().empty(),
           "PM polling cursor prevents duplicate delivery");
    for (int n = 0; n < 1005; ++n) {
        api.record_pm(7, "OwnedHero", "bounded history", true);
    }
    result = api.events(0);
    expect(result.at("gap").boolean() && result.at("more").boolean() &&
               result.at("events").array().size() == 25,
           "bounded PM history reports loss and paginates pending events");
    expect(api.events(999999).at("gap").boolean(),
           "server restart or invalid future cursor is visible to the console");
    deferred_requests(api, path, token);
    server::ManagementApi other;
    expect(!other.start(path, token, error) && std::filesystem::exists(path),
           "another endpoint cannot replace the running server socket");
    api.close();
    expect(!std::filesystem::exists(path), "shutdown removes its local socket");
    std::ofstream(path) << "owned fixture";
    expect(!api.start(path, token, error) && std::filesystem::is_regular_file(path),
           "management does not unlink unrelated filesystem entries");
    std::filesystem::remove_all(directory);
    return test::g_failures ? 1 : 0;
}
