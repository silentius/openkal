/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/DialogCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

namespace openkal::client {
    namespace {

        /**
         * Splits Kal dialog markup line text on '#n' into multiple UI lines.
         * @param text Raw dialog text.
         * @return Expanded display lines.
         */
        [[nodiscard]] std::vector<std::string> split_dialog_lines(std::string_view text) {
            constexpr std::string_view kBreak = "#n";
            std::vector<std::string> lines;
            if (text.empty()) {
                lines.emplace_back();
                return lines;
            }

            std::size_t start = 0;
            while (start <= text.size()) {
                const std::size_t pos = text.find(kBreak, start);
                if (pos == std::string_view::npos) {
                    lines.emplace_back(text.substr(start));
                    break;
                }
                lines.emplace_back(text.substr(start, pos - start));
                start = pos + kBreak.size();
            }
            return lines;
        }

    }  // namespace

    bool DialogCatalog::load_dir(const std::string& dir) {
        m_by_id.clear();
        if (!platform::is_directory(dir)) {
            return false;
        }
        for (const auto& name : platform::list_directory(dir)) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const std::string path = platform::join_path(dir, name);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                continue;
            }
            DialogDef def;
            def.id = root.at("id").as_int(0);
            def.caption = root.at("caption").as_string("Talk");
            const platform::Json& window = root.at("window");
            if (window.is_object()) {
                def.width = static_cast<float>(window.at("w").as_float(0.0));
                def.height = static_cast<float>(window.at("h").as_float(0.0));
            }
            const platform::Json& lines = root.at("lines");
            if (lines.is_array()) {
                for (const auto& line : lines.array()) {
                    const std::vector<std::string> expanded = split_dialog_lines(line.as_string());
                    def.lines.insert(def.lines.end(), expanded.begin(), expanded.end());
                }
            }
            const platform::Json& links = root.at("links");
            if (links.is_array()) {
                for (const auto& lj : links.array()) {
                    if (!lj.is_object()) {
                        continue;
                    }
                    DialogLink link;
                    link.type = lj.at("type").as_string("close");
                    {
                        const std::vector<std::string> expanded =
                            split_dialog_lines(lj.at("text").as_string());
                        for (std::size_t i = 0; i < expanded.size(); ++i) {
                            if (i != 0) {
                                link.text += '\n';
                            }
                            link.text += expanded[i];
                        }
                    }
                    const platform::Json& q = lj.at("quest");
                    if (q.is_array() && q.array().size() >= 2) {
                        link.quest_index = static_cast<std::uint16_t>(q.array()[0].as_int(0));
                        link.quest_flag = static_cast<std::uint8_t>(q.array()[1].as_int(0));
                    }
                    def.links.push_back(std::move(link));
                }
            }
            if (def.id != 0) {
                m_by_id[def.id] = std::move(def);
            }
        }
        log::info("DialogCatalog: loaded " + std::to_string(m_by_id.size()) + " dialogs");
        return !m_by_id.empty();
    }

    const DialogDef* DialogCatalog::find(std::int32_t id) const {
        const auto it = m_by_id.find(id);
        return it == m_by_id.end() ? nullptr : &it->second;
    }

}  // namespace openkal::client
