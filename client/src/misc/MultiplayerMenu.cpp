/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MultiplayerMenu.h"

#include "openkal/client/AccountTextInput.h"
#include "openkal/client/AutoLogin.h"
#include "openkal/client/ClientServerConfig.h"
#include "openkal/client/ClientTlsConfig.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/platform/Path.h"
#include "openkal/server/AccountPassword.h"

#include <array>
#include <chrono>
#include <future>

namespace openkal::client {
    namespace {
        std::array<std::string, 3> status_lines(std::string_view text, std::size_t width) {
            std::array<std::string, 3> lines;
            for (auto& line : lines) {
                if (text.empty()) {
                    break;
                }
                std::size_t size = std::min(width, text.size());
                if (size < text.size()) {
                    const auto space = text.rfind(' ', size);
                    if (space != std::string_view::npos && space > 0) {
                        size = space;
                    }
                }
                line = std::string(text.substr(0, size));
                text.remove_prefix(size);
                while (!text.empty() && text.front() == ' ') {
                    text.remove_prefix(1);
                }
            }
            if (!text.empty()) {
                lines.back() += "...";
            }
            return lines;
        }
    }  // namespace

    MultiplayerChoice MultiplayerMenu::run(engine::IEngine& engine,
                                           WidgetTools& ui,
                                           const FreetypeFont& font,
                                           const UiSettings& settings,
                                           const std::string& root,
                                           const std::string& message,
                                           bool start_online) {
        UiLayout mode_layout, online_layout;
        UiDocument modes, online;
        if (!mode_layout.load_file(platform::join_path(root, "ui/play_mode.json")) ||
            !online_layout.load_file(platform::join_path(root, "ui/multiplayer_login.json"))) {
            return {};
        }
        UiView::reload(modes, &mode_layout);
        UiView::reload(online, &online_layout);
        bool show_online = start_online, connected = false, registering = false, cancelled = false,
             quitting = false, show_passwords = false;
        const auto endpoint = load_client_server_config(root);
        std::array<AccountTextInput, 4> fields;
        const std::array<std::string, 4> keys{"username", "email", "password", "confirmation"};
        int focused = 0;
        std::string status = message;
        auto lobby = std::make_unique<server::LobbyClient>();
        const auto back_to_startup = [] {
            MultiplayerChoice choice;
            choice.startup = true;
            return choice;
        };
        std::future<bool> pending;
        enum class Work { Connect, Login, Register };
        Work work = Work::Connect;
        std::optional<AutoLoginCredentials> auto_login;
        const auto begin_connect = [&] {
            if (!endpoint) {
                auto_login.reset();
                status = "Multiplayer server configuration is missing or invalid.";
                return;
            }
            work = Work::Connect;
            status = "Connecting securely...";
            pending = std::async(std::launch::async, [&] {
                return lobby->connect(endpoint->host,
                                      endpoint->port,
                                      client_tls_config(endpoint->host, endpoint->tls_server_name));
            });
        };
        const auto begin_login = [&](const std::string& name, const std::string& password) {
            work = Work::Login;
            status = "Signing in...";
            pending = std::async(std::launch::async,
                                 [&, name, password] { return lobby->login(name, password); });
        };
        const auto begin_online = [&] {
            auto_login = load_auto_login();
            if (auto_login) {
                fields[0].clear();
                (void) fields[0].insert(auto_login->id, 32);
            }
            begin_connect();
        };
        if (start_online) {
            begin_online();
        }
        engine::FrameInput in;
        for (;;) {
            if (!engine.poll(in)) {
                if (!pending.valid()) {
                    return {};
                }
                quitting = true;
                cancelled = true;
            }
            if (in.quit) {
                if (!pending.valid()) {
                    return {};
                }
                quitting = true;
                cancelled = true;
            }
            if (pending.valid() &&
                pending.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                bool ok = false;
                try {
                    ok = pending.get();
                } catch (...) {
                    status = "Connection request failed.";
                }
                fields[2].clear();
                fields[3].clear();
                show_passwords = false;
                if (cancelled) {
                    auto_login.reset();
                    lobby->close();
                    connected = false;
                    show_online = false;
                    cancelled = false;
                    if (quitting) {
                        return {};
                    }
                    if (start_online) {
                        return back_to_startup();
                    }
                } else if (!ok) {
                    auto_login.reset();
                    if (!lobby->error().empty()) {
                        status = lobby->error();
                    }
                    if (status.empty()) {
                        status = "Connection request failed.";
                    }
                    if (work == Work::Connect || !lobby->connected()) {
                        connected = false;
                        registering = false;
                        focused = 0;
                    }
                } else if (work == Work::Connect) {
                    connected = true;
                    status = "Connected. Sign in or create an account.";
                    focused = 0;
                    if (auto_login) {
                        begin_login(auto_login->id, auto_login->password);
                        auto_login.reset();
                    }
                } else if (work == Work::Register) {
                    registering = false;
                    status = "Account created. Enter your password to sign in.";
                    focused = 2;
                } else {
                    MultiplayerChoice choice;
                    choice.quit = false;
                    choice.multiplayer = true;
                    choice.server_key = endpoint->host + "-" + std::to_string(endpoint->port);
                    choice.lobby = std::move(lobby);
                    return choice;
                }
            }
            const bool busy = pending.valid();
            if (in.key_escape_pressed) {
                if (busy) {
                    cancelled = true;
                    status = "Cancelling request...";
                } else if (show_online) {
                    lobby->close();
                    if (start_online) {
                        return back_to_startup();
                    }
                    connected = false;
                    registering = false;
                    show_online = false;
                    fields[2].clear();
                    fields[3].clear();
                    show_passwords = false;
                } else {
                    return {};
                }
            }
            if (show_online && connected && !busy && in.key_tab_pressed) {
                fields[focused].deselect();
                const std::vector<int> active =
                    registering ? std::vector<int>{0, 1, 2, 3} : std::vector<int>{0, 2};
                auto current = std::find(active.begin(), active.end(), focused);
                const int index =
                    current == active.end() ? 0 : static_cast<int>(current - active.begin());
                focused = active[static_cast<std::size_t>(
                    (index + (in.key_shift ? static_cast<int>(active.size()) - 1 : 1)) %
                    static_cast<int>(active.size()))];
            }
            if (show_online && connected && !busy && focused >= 0) {
                auto& value = fields[focused];
                const std::size_t limit = focused == 0 ? 32 : focused == 1 ? 254 : 128;
                if (in.key_ctrl) {
                    if (in.key_a) {
                        value.select_all();
                    }
                    if ((in.key_c_pressed || in.key_x_pressed) && value.selected()) {
                        if (engine.set_clipboard_text(value.value())) {
                            if (in.key_x_pressed) {
                                value.clear();
                            }
                        } else {
                            status = "Clipboard is unavailable.";
                        }
                    }
                    if (in.key_v_pressed && !value.insert(engine.clipboard_text(), limit)) {
                        status = "Paste rejected: maximum " + std::to_string(limit) +
                                 " bytes, without control characters.";
                    }
                } else if (in.text_char_count > 0 &&
                           !value.insert(std::string_view(in.text_chars, in.text_char_count),
                                         limit)) {
                    status = "Maximum " + std::to_string(limit) + " bytes; no control characters.";
                }
                if (in.key_backspace_pressed || (in.key_delete_pressed && value.selected())) {
                    value.backspace();
                }
            }
            const auto& password = fields[2].value();
            const bool password_valid = server::valid_account_password(password);
            const bool passwords_match = !password.empty() && password == fields[3].value();
            UiVarMap vars{
                {"mode.title", "OpenKal"},
                {"mode.single", "Single Player"},
                {"mode.multi", "Multiplayer"},
                {"mode.quit", "Quit"},
                {"mode.status", status},
                {"online.title", registering ? "Create account" : "Multiplayer"},
                {"online.controls", "Tab: next field. Ctrl+A/C/X/V: select/copy/cut/paste."},
                {"online.username_label", "Username"},
                {"online.email_label", "Email address"},
                {"online.password_label", "Password (12 to 128 bytes)"},
                {"online.confirmation_label", "Repeat password"},
                {"online.toggle_passwords", show_passwords ? "Hide passwords" : "Show passwords"},
                {"online.password_feedback",
                 std::to_string(password.size()) + " / 128 bytes - " +
                     (password_valid ? "valid password" : "minimum 12; no controls")},
                {"online.password_color", password_valid ? "#8DCE9C" : "#E5B58A"},
                {"online.confirmation_feedback",
                 fields[3].value().empty() ? "Repeat your password."
                 : passwords_match         ? "Passwords match."
                                           : "Passwords do not match."},
                {"online.confirmation_color", passwords_match ? "#8DCE9C" : "#E5B58A"},
                {"online.connect", "Connect"},
                {"online.login", "Sign in"},
                {"online.register", "Create account"},
                {"online.back", "Back"},
                {"online.status", status},
                {"online.connect_visible", connected ? "false" : "true"},
                {"online.account_fields", connected ? "true" : "false"},
                {"online.registering", registering ? "true" : "false"},
                {"online.registration_allowed",
                 connected && !registering && lobby->registration_allowed() ? "true" : "false"},
                {"online.login_visible", connected && !registering ? "true" : "false"},
                {"online.create_visible", connected && registering ? "true" : "false"}};
            const auto lines = status_lines(status, show_online ? 45 : 34);
            for (std::size_t i = 0; i < lines.size(); ++i) {
                vars["online.status_line" + std::to_string(i)] = lines[i];
                vars["mode.status_line" + std::to_string(i)] = lines[i];
            }
            for (std::size_t i = 0; i < fields.size(); ++i) {
                std::string shown = i >= 2 && !show_passwords
                                        ? std::string(fields[i].value().size(), '*')
                                        : fields[i].value();
                if (shown.size() > 38) {
                    auto start = shown.size() - 38;
                    while (start < shown.size() &&
                           (static_cast<unsigned char>(shown[start]) & 0xc0) == 0x80) {
                        ++start;
                    }
                    shown = "..." + shown.substr(start);
                }
                vars["online." + keys[i]] =
                    shown + (!busy && focused == static_cast<int>(i) ? "_" : "");
                vars["online." + keys[i] + "_background"] =
                    focused == static_cast<int>(i) && fields[i].selected() ? "#305577" : "#0B100F";
            }
            const auto& doc = show_online ? online : modes;
            ui_fill_theme_vars(doc, vars);
            const float scale = std::min({settings.resolve_scale(static_cast<float>(in.height)),
                                          static_cast<float>(in.width) / 650.0f,
                                          static_cast<float>(in.height) / 800.0f});
            const float w = doc.window_w() * scale, h = doc.window_h() * scale,
                        x = (in.width - w) * .5f, y = (in.height - h) * .5f;
            class Host final : public UiViewHost {
              public:
                Host(bool enabled, bool password_valid, bool passwords_match)
                    : m_enabled(enabled), m_password_valid(password_valid),
                      m_passwords_match(passwords_match) {}
                bool button_enabled(std::string_view action) const override {
                    return m_enabled && (action != "online.login" || m_password_valid) &&
                           (action != "online.submit_registration" ||
                            (m_password_valid && m_passwords_match));
                }

              private:
                bool m_enabled;
                bool m_password_valid;
                bool m_passwords_match;
            } host(!busy, password_valid, passwords_match);
            const auto hit =
                UiView::hit(doc, &ui, in.mouse_x, in.mouse_y, x, y, w, h, scale, vars, &host);
            host.set_pointer_hit(hit.widget_id(), in.mouse_left);
            if ((in.mouse_left_clicked || in.key_enter_pressed) && !busy) {
                const auto action = in.key_enter_pressed
                                        ? (!show_online  ? std::string("mode.single")
                                           : !connected  ? std::string("online.connect")
                                           : registering ? std::string("online.submit_registration")
                                                         : std::string("online.login"))
                                        : hit.action();
                if (!show_online) {
                    if (action == "mode.single") {
                        MultiplayerChoice choice;
                        choice.quit = false;
                        return choice;
                    }
                    if (action == "mode.multi") {
                        show_online = true;
                        status.clear();
                        focused = 0;
                        begin_online();
                    }
                    if (action == "mode.quit") {
                        return {};
                    }
                } else {
                    for (std::size_t i = 0; i < keys.size(); ++i) {
                        if (hit.widget_id() == keys[i]) {
                            fields[focused].deselect();
                            focused = static_cast<int>(i);
                        }
                    }
                    if (action == "online.toggle_passwords") {
                        show_passwords = !show_passwords;
                    } else if (action == "online.back") {
                        show_passwords = false;
                        if (registering) {
                            registering = false;
                            fields[2].clear();
                            fields[3].clear();
                        } else {
                            lobby->close();
                            if (start_online) {
                                return back_to_startup();
                            }
                            connected = false;
                            show_online = false;
                            fields[2].clear();
                            fields[3].clear();
                        }
                    } else if (action == "online.connect") {
                        begin_connect();
                    } else if (action == "online.register" && !registering) {
                        registering = true;
                        show_passwords = false;
                        status = "Email is required; email verification is not enabled yet.";
                        focused = 1;
                    } else if (action == "online.submit_registration") {
                        if (!password_valid) {
                            status =
                                "Password must have 12 to 128 bytes without control characters.";
                        } else if (!passwords_match) {
                            status = "Passwords do not match.";
                        } else {
                            work = Work::Register;
                            status = "Creating account...";
                            const auto name = fields[0].value(), email = fields[1].value();
                            pending = std::async(std::launch::async, [&, name, email, password] {
                                return lobby->register_account(name, email, password);
                            });
                        }
                    } else if (action == "online.login") {
                        if (!password_valid) {
                            status =
                                "Password must have 12 to 128 bytes without control characters.";
                        } else {
                            begin_login(fields[0].value(), password);
                        }
                    }
                }
            }
            engine.begin_frame(.035f, .045f, .055f);
            UiView::draw(engine, ui, doc, x, y, w, h, scale, vars, font, font, &host, true);
            engine.draw_ui_rect(in.mouse_x, in.mouse_y, 3, 12, .9f, .85f, .7f, 1);
            engine.draw_ui_rect(in.mouse_x, in.mouse_y, 9, 3, .9f, .85f, .7f, 1);
            engine.end_frame();
        }
    }
}  // namespace openkal::client
