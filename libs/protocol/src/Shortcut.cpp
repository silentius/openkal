/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Shortcut.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {
    namespace {

        [[nodiscard]] std::uint16_t encode_slot(int index) {
            if (index < 0) {
                return kShortcutEmpty;
            }
            return static_cast<std::uint16_t>(index);
        }

        [[nodiscard]] int decode_slot(std::uint16_t word) {
            if (word == kShortcutEmpty) {
                return -1;
            }
            return static_cast<int>(word);
        }

        void write_bar(PacketWriter& w, const ShortcutBar& bar) {
            w.write_u16(encode_slot(bar.active_index));
            for (int slot : bar.slots) {
                w.write_u16(encode_slot(slot));
            }
        }

        [[nodiscard]] bool read_bar(PacketReader& r, ShortcutBar& bar, std::size_t slots) {
            std::uint16_t active = 0;
            if (!r.read_u16(active)) {
                return false;
            }
            bar = ShortcutBar{};
            bar.active_index = decode_slot(active);
            for (std::size_t i = 0; i < slots; ++i) {
                std::uint16_t word = 0;
                if (!r.read_u16(word)) {
                    return false;
                }
                bar.slots[i] = decode_slot(word);
            }
            return true;
        }

    }  // namespace

    std::vector<std::uint8_t> Shortcut::frame_client_save(const ShortcutBar& bar) {
        PacketWriter w;
        w.write_u8(1);
        write_bar(w, bar);
        return w.frame(static_cast<std::uint8_t>(C2S::Shortcut));
    }

    bool Shortcut::parse_client_save(const std::uint8_t* data, std::size_t size, ShortcutBar& bar) {
        if (!data || size < 1) {
            return false;
        }
        PacketReader r(data, size);
        std::uint8_t flag = 0;
        if (!r.read_u8(flag) || flag == 0) {
            return false;
        }
        if (size != 23 && size != 43) {
            return false;
        }
        return read_bar(r, bar, (size - 3) / 2);
    }

    std::vector<std::uint8_t> Shortcut::frame_server(const ShortcutBar* bar) {
        PacketWriter w;
        if (!bar) {
            w.write_u8(0);
            return w.frame(static_cast<std::uint8_t>(S2C::Shortcut));
        }
        w.write_u8(1);
        write_bar(w, *bar);
        return w.frame(static_cast<std::uint8_t>(S2C::Shortcut));
    }

    bool Shortcut::parse_server(const std::uint8_t* data,
                                std::size_t size,
                                bool& present,
                                ShortcutBar& bar) {
        present = false;
        bar = ShortcutBar{};
        if (!data || size < 1) {
            return false;
        }
        PacketReader r(data, size);
        std::uint8_t flag = 0;
        if (!r.read_u8(flag)) {
            return false;
        }
        if (flag == 0) {
            return true;
        }
        if ((size != 23 && size != 43) || !read_bar(r, bar, (size - 3) / 2)) {
            return false;
        }
        present = true;
        return true;
    }

}  // namespace openkal::protocol
