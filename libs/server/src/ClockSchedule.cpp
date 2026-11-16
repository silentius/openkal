/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/ClockSchedule.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <unordered_set>
#include <utility>

namespace openkal::server {
    namespace {
        bool integer(const platform::Json& value, int minimum, int maximum) {
            return value.is_number() && value.number() >= minimum && value.number() <= maximum &&
                   std::floor(value.number()) == value.number();
        }
    }  // namespace

    bool ClockSchedule::load_file(const std::string& path) {
        const auto bytes = platform::File::read(path);
        if (!bytes) {
            return false;
        }
        platform::Json root;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
            !root.is_object()) {
            return false;
        }
        std::vector<Entry> entries;
        const auto* events = root.get("clock_events");
        if (events) {
            if (!events->is_array() || events->array().size() > protocol::ClockSync::kMaxEvents) {
                return false;
            }
            std::unordered_set<std::string> ids;
            for (const auto& value : events->array()) {
                Entry entry;
                entry.id = value.at("id").as_string();
                entry.name = value.at("name").as_string();
                if (entry.id.empty() || entry.id.size() > 32 || entry.name.empty() ||
                    entry.name.size() > 48 || !ids.insert(entry.id).second ||
                    entry.id.find('\0') != std::string::npos ||
                    entry.name.find('\0') != std::string::npos ||
                    !integer(value.at("hour"), 0, 23) || !integer(value.at("minute"), 0, 59)) {
                    return false;
                }
                const auto& days = value.at("weekdays");
                if (!days.is_array() || days.array().empty() || days.array().size() > 7) {
                    return false;
                }
                for (const auto& day : days.array()) {
                    if (!integer(day, 0, 6)) {
                        return false;
                    }
                    const int weekday = day.as_int();
                    if (std::find(entry.weekdays.begin(), entry.weekdays.end(), weekday) !=
                        entry.weekdays.end()) {
                        return false;
                    }
                    entry.weekdays.push_back(weekday);
                }
                entry.hour = value.at("hour").as_int();
                entry.minute = value.at("minute").as_int();
                entries.push_back(std::move(entry));
            }
        }
        m_entries = std::move(entries);
        return true;
    }

    protocol::ClockSyncFields ClockSchedule::snapshot(std::uint64_t unix_seconds,
                                                      std::uint64_t online_seconds) const {
        protocol::ClockSyncFields result;
        result.unix_seconds = unix_seconds;
        result.online_seconds = online_seconds;
        const auto now = static_cast<std::time_t>(unix_seconds);
        std::tm local{};
        if (!localtime_r(&now, &local)) {
            return result;
        }
        auto wall = local;
        result.utc_offset_seconds = static_cast<std::int32_t>(std::difftime(timegm(&wall), now));
        for (const auto& entry : m_entries) {
            protocol::ClockEvent event{entry.id, entry.name};
            // Two weeks cover two occurrences even for a single-weekday event.
            for (int day = 0; day <= 14; ++day) {
                if (std::find(entry.weekdays.begin(),
                              entry.weekdays.end(),
                              (local.tm_wday + day) % 7) == entry.weekdays.end()) {
                    continue;
                }
                auto date = local;
                date.tm_mday += day;
                date.tm_hour = entry.hour;
                date.tm_min = entry.minute;
                date.tm_sec = 0;
                date.tm_isdst = -1;
                const auto occurrence = std::mktime(&date);
                if (occurrence < now) {
                    continue;
                }
                if (!event.starts_at) {
                    event.starts_at = static_cast<std::uint64_t>(occurrence);
                } else if (static_cast<std::uint64_t>(occurrence) > event.starts_at) {
                    event.following_at = static_cast<std::uint64_t>(occurrence);
                    break;
                }
            }
            if (event.starts_at && event.following_at) {
                result.events.push_back(std::move(event));
            }
        }
        return result;
    }
}  // namespace openkal::server
