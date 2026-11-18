/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "PatchExport.h"

#include <QtEndian>

#include <algorithm>
#include <cstring>
#include <limits>

namespace openkal::world_editor {
    namespace {

        // Layout shared by the existing KcmLoader and OplLoader.
        constexpr qsizetype kHeaderSize = 36;
        constexpr qsizetype kVersionOffset = 32;
        constexpr qsizetype kCellCount = map::kMapSize * map::kMapSize;
        constexpr qsizetype kSampleCount = map::kHeightSamples * map::kHeightSamples;

        std::uint32_t read_uint(const QByteArray& bytes, qsizetype offset) {
            return qFromLittleEndian<std::uint32_t>(bytes.constData() + offset);
        }

        template <class T> void append_uint(QByteArray& bytes, T value) {
            char encoded[sizeof(T)];
            qToLittleEndian(value, encoded);
            bytes.append(encoded, sizeof(encoded));
        }

        void append_float(QByteArray& bytes, float value) {
            std::uint32_t bits;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            append_uint(bytes, bits);
        }

    }  // namespace

    QByteArray encode_kcm(const QByteArray& source, const terrain::HeightPatch& patch) {
        if (source.size() < kHeaderSize || patch.heights.size() != kSampleCount) {
            return {};
        }
        const auto version = read_uint(source, kVersionOffset);
        const qsizetype layer_limit = version >= 5 ? map::kMapLayer : 4;
        const qsizetype alpha_begin = kHeaderSize + layer_limit + (version >= 6 ? 8 : 0);
        const auto layer_count = static_cast<qsizetype>(patch.layers.size());
        if (version < 4 || source.size() < alpha_begin || layer_count < 1 ||
            layer_count > layer_limit ||
            static_cast<qsizetype>(patch.alphas.size()) != (layer_count - 1) * kCellCount ||
            patch.layers.back() == 255 ||
            !std::is_sorted(patch.layers.begin(), patch.layers.end())) {
            return {};
        }

        const char* slot_begin = source.constData() + kHeaderSize;
        const auto old_count = std::count_if(slot_begin, slot_begin + layer_limit, [](char layer) {
            return static_cast<unsigned char>(layer) != 255;
        });
        constexpr qsizetype height_bytes = kSampleCount * sizeof(std::uint16_t);
        const qsizetype suffix = alpha_begin + (old_count - 1) * kCellCount + height_bytes;
        if (old_count < 1 || suffix > source.size()) {
            return {};
        }

        QByteArray result = source.first(alpha_begin);
        // Keep the original palette ordering when only heights or alpha values change.
        auto original_layers = QByteArray(slot_begin, layer_limit);
        std::sort(original_layers.begin(),
                  original_layers.end(),
                  [](unsigned char a, unsigned char b) { return a < b; });
        if (old_count != layer_count || !std::equal(patch.layers.begin(),
                                                    patch.layers.end(),
                                                    original_layers.begin(),
                                                    [](std::uint8_t a, char b) {
                                                        return a == static_cast<unsigned char>(b);
                                                    })) {
            std::fill_n(result.data() + kHeaderSize, layer_limit, static_cast<char>(255));
            std::memcpy(result.data() + kHeaderSize, patch.layers.data(), patch.layers.size());
        }
        if (!patch.alphas.empty()) {
            result.append(reinterpret_cast<const char*>(patch.alphas.data()), patch.alphas.size());
        }
        result.reserve(result.size() + kSampleCount * sizeof(std::uint16_t) + source.size() -
                       suffix);
        for (std::uint16_t sample : patch.heights) {
            append_uint(result, sample);
        }
        result.append(source.constData() + suffix, source.size() - suffix);
        return result;
    }

    QByteArray encode_opl(const QByteArray& source,
                          QPoint coordinate,
                          const std::vector<terrain::OplInstance>& instances) {
        if (instances.size() > std::numeric_limits<std::int32_t>::max()) {
            return {};
        }
        QByteArray result;
        qsizetype suffix = 0;
        bool vector_scale = true;
        if (source.isEmpty()) {
            result = QByteArray(kHeaderSize, '\0');
            qToLittleEndian<std::uint32_t>(coordinate.x(), result.data() + 8);
            qToLittleEndian<std::uint32_t>(coordinate.y(), result.data() + 12);
            qToLittleEndian<std::uint32_t>(4, result.data() + kVersionOffset);
        } else {
            if (source.size() < kHeaderSize + 4) {
                return {};
            }
            vector_scale = read_uint(source, kVersionOffset) >= 4;
            const qsizetype transform_size = vector_scale ? 40 : 32;
            const auto count = read_uint(source, kHeaderSize);
            suffix = kHeaderSize + 4;
            for (std::uint32_t i = 0; i < count; ++i) {
                if (source.size() - suffix < 4 + transform_size) {
                    return {};
                }
                const auto name_length = read_uint(source, suffix);
                if (name_length > 512 ||
                    name_length > source.size() - suffix - 4 - transform_size) {
                    return {};
                }
                suffix += 4 + name_length + transform_size;
            }
            result = source.first(kHeaderSize);
        }
        if (!vector_scale && std::any_of(instances.begin(), instances.end(), [](const auto& inst) {
                return inst.sx != inst.sy || inst.sx != inst.sz;
            })) {
            // Moving a version 4 object into a legacy OPL needs the three-component encoding.
            vector_scale = true;
            qToLittleEndian<std::uint32_t>(4, result.data() + kVersionOffset);
        }
        append_uint(result, static_cast<std::uint32_t>(instances.size()));
        for (const auto& inst : instances) {
            if (inst.name.size() > 512) {
                return {};
            }
            append_uint(result, static_cast<std::uint32_t>(inst.name.size()));
            result.append(inst.name.data(), inst.name.size());
            for (float value :
                 {inst.tx, inst.ty, inst.tz, inst.qx, inst.qy, inst.qz, inst.qw, inst.sx}) {
                append_float(result, value);
            }
            if (vector_scale) {
                append_float(result, inst.sy);
                append_float(result, inst.sz);
            }
        }
        if (suffix < source.size()) {
            result.append(source.constData() + suffix, source.size() - suffix);
        }
        return result;
    }
}  // namespace openkal::world_editor
