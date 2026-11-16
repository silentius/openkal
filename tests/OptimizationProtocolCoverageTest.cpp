/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Constants.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/PacketWriter.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
    using namespace openkal::protocol;
    using openkal::test::expect;

    std::vector<std::uint8_t> numbered_batch(std::size_t count) {
        std::vector<std::uint8_t> bytes;
        for (std::size_t i = 0; i < count; ++i) {
            PacketWriter writer;
            writer.write_u32(static_cast<std::uint32_t>(i));
            const auto frame = writer.frame(0xD3);
            bytes.insert(bytes.end(), frame.begin(), frame.end());
        }
        return bytes;
    }

    void batched_compaction() {
        for (std::size_t count : {1, 32, 256, 2048}) {
            auto bytes = numbered_batch(count);
            const auto complete_size = bytes.size();
            const auto next = numbered_batch(1);
            bytes.insert(bytes.end(), next.begin(), next.begin() + 2);
            FrameDrain drain(bytes);
            std::uint8_t type = 0;
            std::vector<std::uint8_t> payload;
            bool ordered = true;
            for (std::size_t i = 0; i < count; ++i) {
                ordered = ordered && drain.pop(type, payload) == FrameParseStatus::Complete &&
                          type == 0xD3 && payload.size() == 4 && payload[0] == (i & 255) &&
                          payload[1] == (i >> 8) && payload[2] == 0 && payload[3] == 0;
            }
            expect(ordered, "batch cursor decodes every numbered frame once in order");
            expect(bytes.size() == complete_size + 2,
                   "draining 1/32/256/2048 frames performs no intermediate suffix movement");
            expect(drain.pop(type, payload) == FrameParseStatus::Incomplete && payload.empty(),
                   "a final partial header remains incomplete");
            expect(drain.compact() == 2 &&
                       bytes == std::vector<std::uint8_t>(next.begin(), next.begin() + 2),
                   "exactly the two remaining bytes move once regardless of frame count");
            expect(drain.compact() == 0, "an already compacted batch moves no bytes");
        }

        auto bytes = numbered_batch(2);
        const auto second =
            std::vector<std::uint8_t>(bytes.begin() + bytes.size() / 2, bytes.end());
        try {
            FrameDrain drain(bytes);
            std::uint8_t type = 0;
            std::vector<std::uint8_t> payload;
            expect(drain.pop(type, payload) == FrameParseStatus::Complete,
                   "decode before callback failure");
            throw std::runtime_error("owned callback failure");
        } catch (const std::runtime_error&) {
        }
        expect(bytes == second,
               "unwinding commits only frames already consumed before the callback failed");

        bytes = numbered_batch(1);
        bytes.insert(bytes.end(), {0, 0, 0});
        {
            FrameDrain drain(bytes);
            std::uint8_t type = 0;
            std::vector<std::uint8_t> payload;
            expect(drain.pop(type, payload) == FrameParseStatus::Complete &&
                       drain.pop(type, payload) == FrameParseStatus::Invalid,
                   "valid prefix does not hide a malformed trailing frame");
        }
        expect(bytes == std::vector<std::uint8_t>({0, 0, 0}),
               "a caller may retain the invalid suffix while consuming its valid prefix");
        {
            FrameDrain drain(bytes);
            drain.discard();
        }
        expect(bytes.empty(), "explicit discard leaves no stale cursor prefix");
    }

    void benchmark() {
        std::cout << "frames,workers,legacy_us,cursor_us,legacy_bytes_moved,cursor_bytes_moved\n";
        for (std::size_t count : {1, 32, 256, 2048}) {
            const auto batch = numbered_batch(count);
            for (int workers : {1, 16, 64}) {
                const int repetitions = std::max(1, 8192 / static_cast<int>(count) / workers);
                std::size_t moved[2]{};
                long long elapsed[2]{};
                for (int variant = 0; variant < 2; ++variant) {
                    const auto start = std::chrono::steady_clock::now();
                    for (int repeat = 0; repeat < repetitions * workers; ++repeat) {
                        auto bytes = batch;
                        std::uint8_t type = 0;
                        if (variant == 0) {
                            for (;;) {
                                std::size_t consumed = 0;
                                std::vector<std::uint8_t> payload;
                                if (FrameParser::pop(
                                        bytes.data(), bytes.size(), consumed, type, payload) !=
                                    FrameParseStatus::Complete) {
                                    break;
                                }
                                moved[variant] += bytes.size() - consumed;
                                bytes.erase(bytes.begin(),
                                            bytes.begin() + static_cast<std::ptrdiff_t>(consumed));
                            }
                        } else {
                            FrameDrain drain(bytes);
                            std::vector<std::uint8_t> payload;
                            while (drain.pop(type, payload) == FrameParseStatus::Complete) {
                            }
                            moved[variant] += drain.compact();
                        }
                    }
                    elapsed[variant] = std::chrono::duration_cast<std::chrono::microseconds>(
                                           std::chrono::steady_clock::now() - start)
                                           .count();
                }
                std::cout << count << ',' << workers << ',' << elapsed[0] << ',' << elapsed[1]
                          << ',' << moved[0] << ',' << moved[1] << '\n';
            }
        }
    }
}  // namespace

int main(int argc, char** argv) {
    using openkal::protocol::FrameParser;
    using openkal::protocol::FrameParseStatus;
    using openkal::test::expect;

    std::size_t consumed = 17;
    std::uint8_t opcode = 0xA5;
    std::vector<std::uint8_t> payload{42};
    expect(FrameParser::pop(nullptr, 0, consumed, opcode, payload) ==
                   FrameParseStatus::Incomplete &&
               consumed == 0 && payload.empty() && opcode == 0xA5,
           "empty stream resets outputs without changing the previously decoded opcode");
    for (auto size : {std::size_t{1}, std::size_t{2}, std::size_t{8000}}) {
        consumed = 17;
        payload = {42};
        expect(FrameParser::pop(nullptr, size, consumed, opcode, payload) ==
                       FrameParseStatus::Invalid &&
                   consumed == 0 && payload.empty() && opcode == 0xA5,
               "nonnull byte count with null storage is invalid without dereference");
    }

    openkal::protocol::PacketWriter writer;
    writer.write_u32(0x76543210);
    writer.write_string("owned payload");
    const auto frame = writer.frame(0xD3);
    for (std::size_t split = 0; split < frame.size(); ++split) {
        consumed = 99;
        payload = {42};
        expect(FrameParser::pop(frame.data(), split, consumed, opcode, payload) ==
                       FrameParseStatus::Incomplete &&
                   consumed == 0 && payload.empty(),
               "every incomplete header and payload split consumes no bytes");
    }
    expect(FrameParser::pop(frame.data(), frame.size(), consumed, opcode, payload) ==
                   FrameParseStatus::Complete &&
               consumed == frame.size() && opcode == 0xD3 && payload == writer.payload(),
           "complete parser output preserves exact opcode and payload bytes");

    for (std::uint16_t size : std::array<std::uint16_t, 4>{
             0, 1, 2, static_cast<std::uint16_t>(openkal::protocol::kMaxPacketLength + 1)}) {
        const std::array<std::uint8_t, 3> header{
            static_cast<std::uint8_t>(size), static_cast<std::uint8_t>(size >> 8), 0};
        expect(!FrameParser::try_pop(header.data(), header.size(), consumed, opcode, payload) &&
                   consumed == 0 && payload.empty(),
               "compatibility parser rejects invalid lengths without consuming a prefix");
    }
    expect(
        openkal::protocol::try_pop_frame(frame.data(), frame.size(), consumed, opcode, payload) &&
            consumed == frame.size() && opcode == 0xD3 && payload == writer.payload(),
        "free compatibility wrapper produces the same exact framed result");
    batched_compaction();
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        benchmark();
    }
    return openkal::test::g_failures ? 1 : 0;
}
