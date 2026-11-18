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
#include "WorldDocument.h"
#include "fixture_util.h"
#include "openkal/terrain/OplLoader.h"
#include "test_util.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>

#include <array>

namespace {

    QByteArray bytes_at(const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }

    bool write_bytes(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }

}  // namespace

int main() {
    using namespace openkal::world_editor;
    using openkal::test::expect;
    namespace terrain = openkal::terrain;
    namespace map = openkal::map;

    QTemporaryDir temporary;
    const auto client = temporary.filePath("client");
    const auto maps = QDir(client).filePath("MAPS");
    const std::array<QPoint, 4> coordinates = {
        QPoint(31, 31), QPoint(32, 31), QPoint(31, 32), QPoint(32, 32)};
    const auto chunk_file = [](const QString& directory, QPoint point, const QString& extension) {
        return QDir(directory).filePath(WorldDocument::stem(point) + extension);
    };
    const auto kcm_path = [&](QPoint point) { return chunk_file(maps, point, ".KCM"); };
    const auto left_opl = chunk_file(maps, coordinates[0], ".opl");
    const auto right_opl = chunk_file(maps, coordinates[1], ".opl");

    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        const auto point = coordinates[i];
        const auto filename = kcm_path(point);
        expect(openkal::test::write_kcm_v7(
                   filename.toStdString(), point.x(), point.y(), 1, 2, 100 + i * 10),
               "create synthetic KCM");
        expect(write_bytes(filename, bytes_at(filename) + "KCM extension bytes"),
               "append opaque KCM data");
    }
    expect(openkal::test::write_env_layers(QDir(maps).filePath("n.env").toStdString(),
                                           {{"test_sand", 32},
                                            {"test_grass", 64},
                                            {"test_rock", 32},
                                            {"test_pine", 32},
                                            {"test_fern", 32},
                                            {"test_snow", 32},
                                            {"test_ice", 32},
                                            {"test_mud", 32},
                                            {"test_pebbles", 32}}),
           "create synthetic environment");

    const QByteArray legacy_header = [] {
        QByteArray data(40, '\0');
        qToLittleEndian<std::uint32_t>(32, data.data() + 8);
        qToLittleEndian<std::uint32_t>(31, data.data() + 12);
        qToLittleEndian<std::uint32_t>(3, data.data() + 32);
        return data;
    }();
    expect(write_bytes(right_opl, legacy_header + "right extension"),
           "create legacy destination OPL");
    terrain::OplInstance original;
    original.name = "OBJECTS/test/model.gb";
    original.tx = 0.9f;
    original.ty = 0.9f;
    original.tz = 145;
    original.sy = 2;
    original.sz = 3;
    const auto left_bytes = encode_opl({}, coordinates[0], {original}) + "left extension";
    expect(write_bytes(left_opl, left_bytes), "create source OPL");

    WorldDocument world;
    QString error;
    if (!world.open(client, {}, error) || !world.load(coordinates[0])) {
        expect(false, "open synthetic world");
        return 1;
    }
    expect(world.coordinates().size() == 4 && world.layers().size() == 9,
           "discover chunks and terrain materials");
    expect(!world.chunk(coordinates[3]), "adjacent chunk initially unloaded");

    constexpr int n = map::kHeightSamples;
    constexpr int s = map::kMapSize;
    const std::array<int, 4> seam = {s * n + s, s * n, s, 0};
    const auto height = [&](std::size_t i) {
        return world.chunk(coordinates[i])->terrain.heights[seam[i]];
    };
    world.begin_edit();
    world.sculpt({0, 0, 0}, 96, 30, HeightMode::Raise, 0, error);
    world.end_edit();
    if (!world.chunk(coordinates[3])) {
        expect(false, "height editing loads shared corner owners");
        return 1;
    }
    for (std::size_t i = 0; i < seam.size(); ++i) {
        expect(height(i) == 145, "raise the shared corner in each owner");
    }
    expect(world.dirty() && world.can_undo(), "height gesture is one undoable edit");
    world.undo();
    for (std::size_t i = 0; i < seam.size(); ++i) {
        expect(height(i) == 100 + 10 * i, "undo restores the original heights");
    }
    expect(!world.dirty() && world.can_redo(), "undo reaches the open state");
    world.sculpt({0, 0, 0}, 96, 60, HeightMode::Raise, 0, error);
    world.cancel_edit();
    for (std::size_t i = 0; i < seam.size(); ++i) {
        expect(height(i) == 100 + 10 * i, "cancel restores each chunk's shared height");
    }
    expect(!world.dirty() && !world.can_undo() && world.can_redo(),
           "cancel preserves the undo and redo history");
    world.redo();

    world.paint({0, 0, 0}, 96, 1, 0, error);
    world.end_edit();
    const std::array<int, 4> painted = {s * s - 1, s * (s - 1), s - 1, 0};
    for (std::size_t i = 0; i < painted.size(); ++i) {
        const auto& patch = world.chunk(coordinates[i])->terrain;
        expect(patch.layers == std::vector<std::uint8_t>{0, 1, 2},
               "insert the new lowest texture layer");
        expect(patch.alphas[painted[i]] < 128 && patch.alphas[s * s + painted[i]] < 128,
               "paint the lower texture through both upper layers across the corner");
        const int untouched = s * (s / 2) + s / 2;
        expect(patch.alphas[untouched] == 255 && patch.alphas[s * s + untouched] == 128,
               "preserve unpainted texture composition");
    }

    const auto& old_objects = world.chunk(coordinates[0])->objects;
    if (old_objects.size() != 1) {
        expect(false, "load original placement");
        return 1;
    }
    const auto id = old_objects.front().id;
    const QVector3D target(256, 200, -64);
    expect(world.move(id, target), "preview a move into another chunk");
    world.cancel_edit();
    expect(world.chunk(coordinates[0])->objects.size() == 1 &&
               world.chunk(coordinates[0])->objects.front().id == id &&
               world.chunk(coordinates[1])->objects.empty(),
           "cancel restores ownership of a moved object");
    expect(world.move(id, target), "move a model across a chunk boundary");
    world.end_edit();
    expect(world.chunk(coordinates[0])->objects.empty() &&
               world.chunk(coordinates[1])->objects.size() == 1,
           "transfer ownership to the new chunk");
    const auto placed = world.place({100, 100, 200}, "OBJECTS/test/placed.gb");
    world.end_edit();
    expect(placed != 0, "place a catalog asset in a chunk without OPL");
    expect(world.remove(id), "remove a moved object");
    world.end_edit();
    expect(world.chunk(coordinates[1])->objects.empty(), "remove the selected placement");
    world.undo();
    expect(world.chunk(coordinates[1])->objects.size() == 1, "undo the removal");

    const auto output = temporary.filePath("patch");
    expect(world.export_to(client, error) == -1 && world.dirty(),
           "refuse to overwrite an existing directory");
    expect(world.export_to(output, error) == 7 && !world.dirty(),
           "publish a complete native export");
    expect(bytes_at(left_opl) == left_bytes, "leave input placements untouched");
    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        const auto filename = chunk_file(output, coordinates[i], ".KCM");
        terrain::HeightPatch patch;
        expect(terrain::load_kcm(filename.toStdString(), patch) && patch.heights[seam[i]] == 145 &&
                   patch.layers[0] == 0 && patch.alphas[painted[i]] < 128 &&
                   patch.deco_masks[0] == 1,
               "read exported KCM with the OpenKal loader");
        expect(bytes_at(filename).endsWith("KCM extension bytes"),
               "retain opaque terrain metadata");
    }

    terrain::OplFile opl;
    const auto exported_left = chunk_file(output, coordinates[0], ".opl");
    const auto exported_right = chunk_file(output, coordinates[1], ".opl");
    const auto exported_new = chunk_file(output, coordinates[3], ".opl");
    expect(terrain::load_opl(exported_left.toStdString(), opl) && opl.instances.empty(),
           "save an empty source OPL after a cross-chunk move");
    expect(terrain::load_opl(exported_right.toStdString(), opl) && opl.version == 4 &&
               opl.instances.size() == 1 && opl.instances[0].sy == 2 && opl.instances[0].sz == 3 &&
               WorldDocument::position(coordinates[1], opl.instances[0]) == target,
           "save the moved object and retain nonuniform scale in a legacy target");
    expect(bytes_at(exported_left).endsWith("left extension") &&
               bytes_at(exported_right).endsWith("right extension"),
           "retain opaque placement metadata");
    expect(terrain::load_opl(exported_new.toStdString(), opl) && opl.instances.size() == 1 &&
               opl.instances[0].name == "OBJECTS/test/placed.gb",
           "write a new loadable placement file");

    WorldDocument reopened;
    expect(reopened.open(client, output, error) && !reopened.dirty(),
           "open a native export over the inputs");
    const auto* loaded = reopened.load(coordinates[1]);
    expect(loaded && loaded->objects.size() == 1 && loaded->terrain.heights[seam[1]] == 145,
           "overlay takes precedence over original map files");
    const auto second = temporary.filePath("second");
    expect(reopened.export_to(second, error) == 7,
           "retain every earlier change in a successive export");
    expect(bytes_at(chunk_file(second, coordinates[3], ".opl")) == bytes_at(exported_new),
           "carry over unloaded exported chunks byte for byte");

    const auto full_path = kcm_path(coordinates[0]);
    terrain::HeightPatch full;
    if (!terrain::load_kcm(full_path.toStdString(), full)) {
        expect(false, "read source terrain for a full palette fixture");
        return 1;
    }
    full.layers = {0, 1, 2, 3, 4, 5, 6, 7};
    full.alphas.assign(7 * s * s, 0);
    std::fill(full.alphas.begin() + 6 * s * s, full.alphas.end(), 128);
    expect(write_bytes(full_path, encode_kcm(bytes_at(full_path), full)),
           "create full texture palette fixture");
    WorldDocument full_world;
    if (!full_world.open(client, {}, error) || !full_world.load(coordinates[0])) {
        expect(false, "open the full texture palette fixture");
        return 1;
    }
    error.clear();
    full_world.paint({-4096, 0, -4096}, 64, 1, 8, error);
    full_world.end_edit();
    expect(!error.isEmpty() && !full_world.dirty(),
           "an additional paint layer cannot overfill a palette");
    expect(full_world.replace_texture(coordinates[0], 0, 8),
           "replace an existing layer in a full palette");
    const auto& replacement = full_world.chunk(coordinates[0])->terrain;
    expect(replacement.layers.front() == 1 && replacement.layers.back() == 8 &&
               replacement.alphas[5 * s * s] == 255 && replacement.alphas[6 * s * s] == 127,
           "the replacement keeps its coverage when moved above the other layers");
    full_world.undo();
    expect(replacement.layers == full.layers && replacement.alphas == full.alphas,
           "undo restores the original layers and blend planes");

    constexpr int overview_step = 8;
    constexpr int overview_side = s / overview_step;
    std::vector<terrain::Vertex> left_mesh, right_mesh;
    std::vector<std::uint32_t> indices;
    world.chunk(coordinates[0])->terrain.build_mesh(left_mesh, indices, overview_step);
    expect(left_mesh.size() == (overview_side + 1) * (overview_side + 1) &&
               indices.size() == overview_side * overview_side * 6,
           "build a smaller terrain mesh for a distant chunk");
    world.chunk(coordinates[1])->terrain.build_mesh(right_mesh, indices, overview_step);
    if (!left_mesh.empty() && right_mesh.size() == left_mesh.size()) {
        const auto& left_corner = left_mesh.back().position;
        const auto& right_corner = right_mesh[right_mesh.size() - overview_side - 1].position;
        expect(left_corner.x == right_corner.x && left_corner.y == right_corner.y &&
                   left_corner.z == right_corner.z,
               "neighboring overview meshes share their edited corner");
    }

    reopened.evict_distant({{0, 0}});
    expect(!reopened.chunk(coordinates[1]), "clean out-of-view chunks can be evicted");
    world.evict_distant({{0, 0}});
    expect(world.chunk(coordinates[0]) && world.chunk(coordinates[1]),
           "edited terrain and moved placements survive cache eviction");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
