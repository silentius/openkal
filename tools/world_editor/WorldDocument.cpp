/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "WorldDocument.h"

#include "PatchExport.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/OplLoader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>

namespace openkal::world_editor {
    namespace {

        constexpr int kSide = map::kMapSize;
        constexpr int kSamples = map::kHeightSamples;
        constexpr int kGrid = map::kPatchGrid;
        constexpr int kCell = map::kTerrainCellSize;
        constexpr int kPlane = kSide * kSide;
        constexpr float kExtent = terrain::kOplPatchScale;
        constexpr float kHalfWorld = terrain::kOplFullScale / 2.0f;
        constexpr std::size_t kHistoryLimit = 24;

        QString case_path(const QString& root, const char* relative) {
            return QString::fromStdString(
                platform::resolve_path_case(root.toStdString(), relative));
        }

        QByteArray read_file(const QString& path) {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
        }

        bool valid_coordinate(QPoint point) {
            return point.x() >= 0 && point.x() < kGrid && point.y() >= 0 && point.y() < kGrid;
        }

        std::pair<int, int> key_at(QPoint point) {
            return {point.x(), point.y()};
        }

        using LayerWeights = std::array<float, map::kMapLayer>;

        LayerWeights cell_weights(const terrain::HeightPatch& patch, int cell) {
            LayerWeights weights{};
            float uncovered = 1.0f;
            for (std::size_t i = patch.layers.size() - 1; i > 0; --i) {
                const float alpha = patch.alphas[(i - 1) * kPlane + cell] / 255.0f;
                weights[i] = uncovered * alpha;
                uncovered *= 1.0f - alpha;
            }
            weights[0] = uncovered;
            return weights;
        }

        void write_weights(terrain::HeightPatch& patch, int cell, const LayerWeights& weights) {
            float covered = weights[0];
            for (std::size_t i = 1; i < patch.layers.size(); ++i) {
                covered += weights[i];
                // An opaque upper layer can hide several planes. Keep those bytes intact.
                if (covered > 0.0f) {
                    patch.alphas[(i - 1) * kPlane + cell] = static_cast<std::uint8_t>(
                        std::clamp(std::lround(255.0f * weights[i] / covered), 0l, 255l));
                }
            }
        }

        // Convert to effective opacity so the brush can paint underneath other layer IDs.
        void tint_cell(terrain::HeightPatch& patch, std::size_t chosen, int cell, float amount) {
            auto weights = cell_weights(patch, cell);
            for (float& weight : weights) {
                weight *= 1.0f - amount;
            }
            weights[chosen] += amount;
            write_weights(patch, cell, weights);
        }

        void set_position(terrain::OplInstance& object, QPoint owner, QVector3D position) {
            object.tx = (position.x() - (owner.x() - kGrid / 2) * kExtent) / kExtent;
            object.ty = (position.z() - (owner.y() - kGrid / 2) * kExtent) / kExtent;
            object.tz = position.y();
        }

        // OplLoader reserves the count from its header. Reject implausible counts before loading.
        bool valid_placement_size(const QByteArray& bytes) {
            if (bytes.size() < 40) {
                return false;
            }
            const auto version = qFromLittleEndian<std::uint32_t>(bytes.constData() + 32);
            const auto count = qFromLittleEndian<std::uint32_t>(bytes.constData() + 36);
            return count <= (bytes.size() - 40) / (version >= 4 ? 44 : 36);
        }

    }  // namespace

    bool WorldDocument::open(const QString& root, const QString& overlay, QString& error) {
        WorldDocument next;
        const QDir directory(root);
        const bool is_maps = directory.dirName().compare("MAPS", Qt::CaseInsensitive) == 0;
        next.m_root =
            is_maps ? QFileInfo(directory.absolutePath()).absolutePath() : directory.absolutePath();
        const auto maps = is_maps ? directory.absolutePath() : case_path(next.m_root, "MAPS");
        if (root.isEmpty() || maps.isEmpty() || !QFileInfo(maps).isDir()) {
            error = "Choose a client data directory containing MAPS.";
            return false;
        }
        if (!overlay.isEmpty() && !QFileInfo(overlay).isDir()) {
            error = "The exported patch directory does not exist.";
            return false;
        }

        const QRegularExpression filename("^n_(\\d{3})_(\\d{3})\\.(kcm|opl)$",
                                          QRegularExpression::CaseInsensitiveOption);
        const auto scan = [&](const QString& path, bool from_overlay) {
            const auto files = QDir(path).entryInfoList(QDir::Files, QDir::Name);
            for (const auto& file : files) {
                const auto match = filename.match(file.fileName());
                if (!match.hasMatch()) {
                    continue;
                }
                const QPoint coordinate(match.captured(1).toInt(), match.captured(2).toInt());
                if (!valid_coordinate(coordinate)) {
                    continue;
                }
                const auto extension = match.captured(3);
                auto& source = next.m_sources[key_at(coordinate)];
                const bool terrain = extension.compare("kcm", Qt::CaseInsensitive) == 0;
                auto& current = terrain ? source.terrain : source.objects;
                auto& from_current_overlay =
                    terrain ? source.overlay_terrain : source.overlay_objects;
                // Prefer lowercase filenames in case both variants are present.
                if (current.isEmpty() || (from_overlay && !from_current_overlay) ||
                    (extension == extension.toLower() &&
                     QFileInfo(current).suffix() != extension)) {
                    current = file.absoluteFilePath();
                    from_current_overlay = from_overlay;
                }
            }
        };
        scan(maps, false);
        if (!overlay.isEmpty()) {
            scan(overlay, true);
        }
        for (auto it = next.m_sources.begin(); it != next.m_sources.end();) {
            if (it->second.terrain.isEmpty()) {
                it = next.m_sources.erase(it);
            } else {
                ++it;
            }
        }
        if (next.m_sources.empty()) {
            error = QString("No outdoor n_XXX_YYY.kcm files were found in %1.").arg(maps);
            return false;
        }
        next.m_textures = case_path(maps, "Tex");
        const auto environment = case_path(maps, "n.env");
        if (QFileInfo(environment).isFile() &&
            !terrain::load_env_layers(environment.toStdString(), next.m_layers)) {
            next.m_layers.clear();
        }
        *this = std::move(next);
        return true;
    }

    std::vector<QPoint> WorldDocument::coordinates() const {
        std::vector<QPoint> result;
        result.reserve(m_sources.size());
        for (const auto& [key, source] : m_sources) {
            result.emplace_back(key.first, key.second);
        }
        return result;
    }

    const WorldChunk* WorldDocument::load(QPoint coordinate) {
        const auto loaded = ensure(coordinate);
        return loaded ? &loaded->chunk : nullptr;
    }

    const WorldChunk* WorldDocument::chunk(QPoint coordinate) const {
        const auto found = m_chunks.find(key_at(coordinate));
        return found == m_chunks.end() ? nullptr : &found->second.chunk;
    }

    WorldDocument::Loaded* WorldDocument::ensure(QPoint coordinate) {
        const Key key = key_at(coordinate);
        const auto loaded = m_chunks.find(key);
        if (loaded != m_chunks.end()) {
            return &loaded->second;
        }
        const auto source = m_sources.find(key);
        if (source == m_sources.end()) {
            return nullptr;
        }

        Loaded entry;
        auto& chunk = entry.chunk;
        entry.terrain_bytes = read_file(source->second.terrain);
        if (entry.terrain_bytes.isEmpty() ||
            !terrain::load_kcm(source->second.terrain.toStdString(), chunk.terrain)) {
            return nullptr;
        }
        chunk.terrain.patch_x = coordinate.x();
        chunk.terrain.patch_y = coordinate.y();
        if (!source->second.objects.isEmpty()) {
            terrain::OplFile opl;
            entry.objects_bytes = read_file(source->second.objects);
            chunk.objects_readable = valid_placement_size(entry.objects_bytes) &&
                                     terrain::load_opl(source->second.objects.toStdString(), opl);
            if (chunk.objects_readable) {
                chunk.objects.reserve(opl.instances.size());
                for (auto& object : opl.instances) {
                    chunk.objects.push_back({m_next_id++, std::move(object)});
                }
            }
        }
        return &m_chunks.emplace(key, std::move(entry)).first->second;
    }

    void WorldDocument::evict_distant(const std::vector<QPoint>& retained) {
        QSet<QPoint> keep;
        for (const auto coordinate : retained) {
            for (int z = -1; z <= 1; ++z) {
                for (int x = -1; x <= 1; ++x) {
                    keep.insert(coordinate + QPoint(x, z));
                }
            }
        }
        for (auto it = m_chunks.begin(); it != m_chunks.end();) {
            const auto& chunk = it->second.chunk;
            const auto& key = it->first;
            if (!keep.contains(QPoint(key.first, key.second)) && !chunk.height_revision &&
                !chunk.texture_revision && !chunk.objects_edited) {
                it = m_chunks.erase(it);
            } else {
                ++it;
            }
        }
    }

    void WorldDocument::begin_edit() {
        if (!m_edit) {
            m_edit.emplace();
            m_edit->previous_state = m_state;
        }
    }

    void WorldDocument::end_edit() {
        if (m_edit && !m_edit->chunks.empty()) {
            m_redo.clear();
            m_state = m_next_state++;
            m_undo.push_back(std::move(*m_edit));
            if (m_undo.size() > kHistoryLimit) {
                m_undo.pop_front();
            }
        }
        m_edit.reset();
    }

    void WorldDocument::cancel_edit() {
        if (m_edit) {
            swap_edit(*m_edit);
            m_edit.reset();
        }
    }

    void WorldDocument::undo() {
        end_edit();
        if (!m_undo.empty()) {
            swap_edit(m_undo.back());
            m_redo.push_back(std::move(m_undo.back()));
            m_undo.pop_back();
        }
    }

    void WorldDocument::redo() {
        end_edit();
        if (!m_redo.empty()) {
            swap_edit(m_redo.back());
            m_undo.push_back(std::move(m_redo.back()));
            m_redo.pop_back();
        }
    }

    void WorldDocument::remember_heights(Key key, const WorldChunk& chunk) {
        begin_edit();
        auto& heights = m_edit->chunks[key].heights;
        if (!heights) {
            heights = chunk.terrain.heights;
        }
    }

    void WorldDocument::remember_textures(Key key, const WorldChunk& chunk) {
        begin_edit();
        auto& textures = m_edit->chunks[key].textures;
        if (!textures) {
            textures = Textures{chunk.terrain.layers, chunk.terrain.alphas};
        }
    }

    void WorldDocument::remember_objects(Key key, const WorldChunk& chunk) {
        begin_edit();
        auto& objects = m_edit->chunks[key].objects;
        if (!objects) {
            objects = chunk.objects;
        }
    }

    void WorldDocument::swap_edit(Edit& edit) {
        std::swap(m_state, edit.previous_state);
        for (auto& [key, snapshot] : edit.chunks) {
            auto& chunk = m_chunks.at(key).chunk;
            if (snapshot.heights) {
                snapshot.heights->swap(chunk.terrain.heights);
                ++chunk.height_revision;
            }
            if (snapshot.textures) {
                snapshot.textures->layers.swap(chunk.terrain.layers);
                snapshot.textures->alphas.swap(chunk.terrain.alphas);
                ++chunk.texture_revision;
            }
            if (snapshot.objects) {
                snapshot.objects->swap(chunk.objects);
            }
        }
    }

    void WorldDocument::sculpt(QVector3D center,
                               float radius,
                               float strength,
                               HeightMode mode,
                               float level,
                               QString& error) {
        const float cx = (center.x() + kHalfWorld) / kCell;
        const float cz = (center.z() + kHalfWorld) / kCell;
        const float reach = radius / kCell;
        const int limit = kGrid * kSide;
        const int min_x = std::max(0, static_cast<int>(std::ceil(cx - reach)));
        const int min_z = std::max(0, static_cast<int>(std::ceil(cz - reach)));
        const int max_x = std::min(limit, static_cast<int>(std::floor(cx + reach)));
        const int max_z = std::min(limit, static_cast<int>(std::floor(cz + reach)));
        if (min_x > max_x || min_z > max_z || radius <= 0 || strength <= 0) {
            return;
        }

        // Include owners on the far side of the first edge as well as the last edge.
        std::map<Key, WorldChunk*> owners;
        for (int z = std::max(0, (min_z - 1) / kSide); z <= std::min(kGrid - 1, max_z / kSide);
             ++z) {
            for (int x = std::max(0, (min_x - 1) / kSide); x <= std::min(kGrid - 1, max_x / kSide);
                 ++x) {
                if (auto* loaded = ensure({x, z})) {
                    owners.emplace(Key{x, z}, &loaded->chunk);
                } else if (m_sources.count({x, z})) {
                    error =
                        QString(
                            "Could not read %1. Height stroke skipped to preserve shared edges.")
                            .arg(stem({x, z}));
                    return;
                }
            }
        }

        for (int z = min_z; z <= max_z; ++z) {
            for (int x = min_x; x <= max_x; ++x) {
                const float falloff = 1.0f - std::hypot(x - cx, z - cz) / reach;
                if (falloff <= 0.0f) {
                    continue;
                }
                struct Sample {
                    Key key;
                    WorldChunk* chunk;
                    std::uint16_t* value;
                };
                std::array<Sample, 4> samples{};
                int count = 0;
                float total = 0;
                for (int pz = std::max(0, (z - 1) / kSide); pz <= std::min(kGrid - 1, z / kSide);
                     ++pz) {
                    for (int px = std::max(0, (x - 1) / kSide);
                         px <= std::min(kGrid - 1, x / kSide);
                         ++px) {
                        const auto found = owners.find({px, pz});
                        if (found != owners.end()) {
                            auto* chunk = found->second;
                            auto& height =
                                chunk->terrain
                                    .heights[(z - pz * kSide) * kSamples + x - px * kSide];
                            samples[count++] = {found->first, chunk, &height};
                            total += height;
                        }
                    }
                }
                if (count == 0) {
                    continue;
                }
                const float current = total / count;
                const float step = strength * falloff;
                float delta = step;
                if (mode == HeightMode::Lower) {
                    delta = -step;
                } else if (mode == HeightMode::Flatten) {
                    delta = std::clamp(level - current, -step, step);
                }
                const auto height = static_cast<std::uint16_t>(
                    std::clamp(std::lround(current + delta), 0l, 65535l));
                for (int i = 0; i < count; ++i) {
                    const auto& sample = samples[i];
                    if (*sample.value != height) {
                        remember_heights(sample.key, *sample.chunk);
                        *sample.value = height;
                        ++sample.chunk->height_revision;
                    }
                }
            }
        }
    }

    void WorldDocument::paint(
        QVector3D center, float radius, float opacity, std::uint8_t material, QString& error) {
        if (radius <= 0 || opacity <= 0 || material >= 255 || material >= m_layers.size()) {
            return;
        }
        const QPoint lo = coordinate_at(center.x() - radius, center.z() - radius);
        const QPoint hi = coordinate_at(center.x() + radius, center.z() + radius);
        for (int z = std::max(0, lo.y()); z <= std::min(kGrid - 1, hi.y()); ++z) {
            for (int x = std::max(0, lo.x()); x <= std::min(kGrid - 1, hi.x()); ++x) {
                if (auto* loaded = ensure({x, z})) {
                    auto& chunk = loaded->chunk;
                    auto& patch = chunk.terrain;
                    const float local_x = (center.x() - (x - kGrid / 2) * kExtent) / kCell - 0.5f;
                    const float local_z = (center.z() - (z - kGrid / 2) * kExtent) / kCell - 0.5f;
                    const float reach = radius / kCell;
                    const int min_x = std::max(0, static_cast<int>(std::ceil(local_x - reach)));
                    const int min_z = std::max(0, static_cast<int>(std::ceil(local_z - reach)));
                    const int max_x =
                        std::min(kSide - 1, static_cast<int>(std::floor(local_x + reach)));
                    const int max_z =
                        std::min(kSide - 1, static_cast<int>(std::floor(local_z + reach)));
                    const auto it =
                        std::lower_bound(patch.layers.begin(), patch.layers.end(), material);
                    const auto index = static_cast<std::size_t>(it - patch.layers.begin());
                    const bool exists = it != patch.layers.end() && *it == material;
                    if (exists && patch.layers.size() == 1) {
                        continue;
                    }
                    const auto coverage = [&](int col, int row) {
                        return std::min(1.0f, opacity) *
                               (1.0f - std::hypot(col - local_x, row - local_z) / reach);
                    };
                    constexpr float minimum_opacity = 0.5f / 255.0f;
                    if (min_x > max_x || min_z > max_z ||
                        coverage(std::clamp(static_cast<int>(std::lround(local_x)), min_x, max_x),
                                 std::clamp(static_cast<int>(std::lround(local_z)), min_z, max_z)) <
                            minimum_opacity) {
                        continue;
                    }
                    const auto layer_limit = patch.version < 5 ? 4u : map::kMapLayer;
                    if (!exists && patch.layers.size() >= layer_limit) {
                        error = QString("%1 has a full texture palette. Choose one of its layers.")
                                    .arg(stem({x, z}));
                        continue;
                    }

                    remember_textures({x, z}, chunk);
                    ++chunk.texture_revision;
                    if (!exists) {
                        patch.layers.insert(patch.layers.begin() + index, material);
                        // A new lowest layer must initially be hidden by the former base.
                        const auto plane = index == 0 ? 0 : index - 1;
                        patch.alphas.insert(
                            patch.alphas.begin() + plane * kPlane, kPlane, index == 0 ? 255 : 0);
                    }
                    for (int row = min_z; row <= max_z; ++row) {
                        for (int col = min_x; col <= max_x; ++col) {
                            const float amount = coverage(col, row);
                            if (amount >= minimum_opacity) {
                                tint_cell(patch, index, row * kSide + col, amount);
                            }
                        }
                    }
                } else if (m_sources.count({x, z})) {
                    error = QString("Could not read %1 for texture painting.").arg(stem({x, z}));
                }
            }
        }
    }

    bool WorldDocument::replace_texture(QPoint coordinate,
                                        std::uint8_t old_layer,
                                        std::uint8_t new_layer) {
        auto* loaded = ensure(coordinate);
        if (!loaded || new_layer >= 255 || new_layer >= m_layers.size()) {
            return false;
        }
        auto& chunk = loaded->chunk;
        auto& patch = chunk.terrain;
        auto layers = patch.layers;
        const auto old = std::find(layers.begin(), layers.end(), old_layer);
        if (old == layers.end() || std::binary_search(layers.begin(), layers.end(), new_layer) ||
            std::count(layers.begin(), layers.end(), old_layer) != 1) {
            return false;
        }
        *old = new_layer;
        auto sorted = layers;
        std::sort(sorted.begin(), sorted.end());
        remember_textures(key_at(coordinate), chunk);
        if (layers != sorted) {
            std::array<std::size_t, map::kMapLayer> new_order{};
            for (std::size_t i = 0; i < layers.size(); ++i) {
                new_order[i] = std::find(layers.begin(), layers.end(), sorted[i]) - layers.begin();
            }
            for (int cell = 0; cell < kPlane; ++cell) {
                const auto before = cell_weights(patch, cell);
                LayerWeights after{};
                for (std::size_t i = 0; i < layers.size(); ++i) {
                    after[i] = before[new_order[i]];
                }
                write_weights(patch, cell, after);
            }
        }
        patch.layers = std::move(sorted);
        ++chunk.texture_revision;
        end_edit();
        return true;
    }

    std::pair<WorldDocument::Key, std::size_t> WorldDocument::find_object(std::uint64_t id) const {
        for (const auto& [key, loaded] : m_chunks) {
            const auto& objects = loaded.chunk.objects;
            const auto found = std::find_if(
                objects.begin(), objects.end(), [id](const Placement& p) { return p.id == id; });
            if (found != objects.end()) {
                return {key, static_cast<std::size_t>(found - objects.begin())};
            }
        }
        return {{-1, -1}, 0};
    }

    std::uint64_t WorldDocument::place(QVector3D position, const std::string& model) {
        const QPoint coordinate = coordinate_at(position.x(), position.z());
        auto* loaded = ensure(coordinate);
        if (!loaded || !loaded->chunk.objects_readable || model.empty() || model.size() > 512) {
            return 0;
        }
        auto& chunk = loaded->chunk;
        remember_objects(key_at(coordinate), chunk);
        auto& placement = chunk.objects.emplace_back();
        placement.id = m_next_id++;
        placement.object.name = model;
        set_position(placement.object, coordinate, position);
        chunk.objects_edited = true;
        return placement.id;
    }

    bool WorldDocument::move(std::uint64_t id, QVector3D position) {
        const auto [key, index] = find_object(id);
        if (key.first < 0) {
            return false;
        }
        const QPoint target = coordinate_at(position.x(), position.z());
        auto* destination = ensure(target);
        if (!destination || !destination->chunk.objects_readable) {
            return false;
        }
        auto& origin = m_chunks.at(key).chunk;
        const auto& object = origin.objects[index].object;
        if (WorldDocument::position({key.first, key.second}, object) == position) {
            return true;
        }
        remember_objects(key, origin);
        origin.objects_edited = true;
        if (key == key_at(target)) {
            set_position(origin.objects[index].object, target, position);
        } else {
            auto& dest = destination->chunk;
            remember_objects(key_at(target), dest);
            dest.objects.reserve(dest.objects.size() + 1);
            auto placement = std::move(origin.objects[index]);
            set_position(placement.object, target, position);
            dest.objects.push_back(std::move(placement));
            origin.objects.erase(origin.objects.begin() + index);
            dest.objects_edited = true;
        }
        return true;
    }

    bool WorldDocument::remove(std::uint64_t id) {
        const auto [key, index] = find_object(id);
        if (key.first < 0) {
            return false;
        }
        auto& chunk = m_chunks.at(key).chunk;
        remember_objects(key, chunk);
        chunk.objects.erase(chunk.objects.begin() + index);
        chunk.objects_edited = true;
        return true;
    }

    int WorldDocument::export_to(const QString& destination, QString& error) {
        end_edit();
        const QFileInfo output(destination);
        auto parent = output.absoluteDir();
        if (!parent.exists() || output.exists() || output.isSymLink() ||
            output.fileName().isEmpty()) {
            error = "Choose a new export directory inside an existing folder.";
            return -1;
        }

        QTemporaryDir stage(parent.filePath(".openkal-world-XXXXXX"));
        if (!stage.isValid()) {
            error = stage.errorString();
            return -1;
        }
        int count = 0;
        const auto write = [&](const QString& name, const QByteArray& bytes) {
            QFile file(stage.filePath(name));
            if (bytes.isEmpty() || !file.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
                file.write(bytes) != bytes.size() || !file.flush()) {
                error = QString("Could not export %1: %2").arg(name, file.errorString());
                return false;
            }
            ++count;
            return true;
        };
        for (const auto& [key, source] : m_sources) {
            const auto found = m_chunks.find(key);
            const auto* loaded = found == m_chunks.end() ? nullptr : &found->second;
            if (loaded && (loaded->chunk.height_revision || loaded->chunk.texture_revision)) {
                const auto bytes = encode_kcm(loaded->terrain_bytes, loaded->chunk.terrain);
                if ((source.overlay_terrain || bytes != loaded->terrain_bytes) &&
                    !write(QFileInfo(source.terrain).fileName(), bytes)) {
                    return -1;
                }
            } else if (source.overlay_terrain &&
                       !write(QFileInfo(source.terrain).fileName(), read_file(source.terrain))) {
                return -1;
            }
            if (loaded && loaded->chunk.objects_edited) {
                std::vector<terrain::OplInstance> objects;
                objects.reserve(loaded->chunk.objects.size());
                for (const auto& placement : loaded->chunk.objects) {
                    objects.push_back(placement.object);
                }
                const QPoint coordinate(key.first, key.second);
                const auto bytes = encode_opl(loaded->objects_bytes, coordinate, objects);
                const auto name = source.objects.isEmpty() ? stem(coordinate) + ".opl"
                                                           : QFileInfo(source.objects).fileName();
                const bool new_table = loaded->objects_bytes.isEmpty();
                const bool changed = new_table ? !objects.empty() : bytes != loaded->objects_bytes;
                if ((changed || source.overlay_objects) && !write(name, bytes)) {
                    return -1;
                }
            } else if (source.overlay_objects &&
                       !write(QFileInfo(source.objects).fileName(), read_file(source.objects))) {
                return -1;
            }
        }
        if (count > 0) {
            if (!parent.rename(QFileInfo(stage.path()).fileName(), output.fileName())) {
                error = "Could not publish the export directory.";
                return -1;
            }
            stage.setAutoRemove(false);
        }
        m_saved_state = m_state;
        return count;
    }

    QVector3D WorldDocument::position(QPoint coordinate, const terrain::OplInstance& object) {
        float x, y, z;
        terrain::opl_world_translation(object, coordinate.x(), coordinate.y(), x, y, z);
        return {x, y, z};
    }

    QPoint WorldDocument::coordinate_at(float x, float z) {
        return {static_cast<int>(std::floor((x + kHalfWorld) / kExtent)),
                static_cast<int>(std::floor((z + kHalfWorld) / kExtent))};
    }

    QString WorldDocument::stem(QPoint coordinate) {
        return QString("n_%1_%2")
            .arg(coordinate.x(), 3, 10, QChar('0'))
            .arg(coordinate.y(), 3, 10, QChar('0'));
    }

}  // namespace openkal::world_editor
