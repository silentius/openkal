/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/terrain/HeightPatch.h"
#include "openkal/terrain/OplFile.h"

#include <QByteArray>
#include <QPoint>
#include <QStringList>
#include <QVector3D>

#include <deque>
#include <map>
#include <optional>

namespace openkal::world_editor {

    /** Placement with a stable editor selection identifier. */
    struct Placement {
        std::uint64_t id = 0;
        terrain::OplInstance object;
    };

    /** Loaded terrain and placements in one outdoor chunk. */
    struct WorldChunk {
        terrain::HeightPatch terrain;
        std::vector<Placement> objects;
        std::uint64_t height_revision = 0;
        std::uint64_t texture_revision = 0;
        bool objects_edited = false;
        bool objects_readable = true;
    };

    /** Operation applied by the height brush. */
    enum class HeightMode { Raise, Lower, Flatten };

    /** Outdoor chunks, shared-edge editing, undo and native export. */
    class WorldDocument final {
      public:
        /** Opens the outdoor map directory for a client root.
         * @param root Client data directory containing MAPS and optionally OBJECTS.
         * @param overlay Optional previously exported patch directory.
         * @param error Destination error message on failure.
         * @param maps_subdirectory Terrain directory relative to the client data root.
         * @return True when at least one chunk is available.
         */
        [[nodiscard]] bool open(const QString& root,
                                const QString& overlay,
                                QString& error,
                                const QString& maps_subdirectory = "MAPS");

        /** @return Available outdoor chunk coordinates in file order. */
        [[nodiscard]] std::vector<QPoint> coordinates() const;
        /** @return Client asset root used by the document. */
        [[nodiscard]] const QString& root() const {
            return m_root;
        }
        /** @return Directory containing the terrain detail textures. */
        [[nodiscard]] const QString& textures() const {
            return m_textures;
        }
        /** @return Terrain layer catalog from the environment. */
        [[nodiscard]] const std::vector<terrain::LayerInfo>& layers() const {
            return m_layers;
        }

        /** Loads a chunk if available and valid.
         * @param coordinate Chunk X/Z indices.
         * @return Loaded chunk or nullptr.
         */
        [[nodiscard]] const WorldChunk* load(QPoint coordinate);

        /** Returns an already loaded chunk.
         * @param coordinate Chunk X/Z indices.
         * @return Loaded chunk or nullptr.
         */
        [[nodiscard]] const WorldChunk* chunk(QPoint coordinate) const;

        /** @return Whether the visible document differs from its last export or open state. */
        [[nodiscard]] bool dirty() const {
            return m_state != m_saved_state;
        }
        /** @return Whether undo is available. */
        [[nodiscard]] bool can_undo() const {
            return !m_undo.empty();
        }
        /** @return Whether redo is available. */
        [[nodiscard]] bool can_redo() const {
            return !m_redo.empty();
        }

        /** Starts recording a single gesture for undo. */
        void begin_edit();
        /** Commits all changes in the gesture to the undo history. */
        void end_edit();
        /** Reverts the current gesture without changing undo or redo history. */
        void cancel_edit();
        /** Reverts the last gesture. */
        void undo();
        /** Reapplies the last reverted gesture. */
        void redo();

        /** Edits height samples, including shared edge and corner samples.
         * @param center Brush center in client coordinates.
         * @param radius Brush radius in world units.
         * @param strength Height change in world units at the center.
         * @param mode Height operation.
         * @param level Target height in flatten mode.
         * @param error Destination error message if a shared sample cannot be edited.
         */
        void sculpt(QVector3D center,
                    float radius,
                    float strength,
                    HeightMode mode,
                    float level,
                    QString& error);

        /** Paints a configured texture layer across all chunks under the brush.
         * @param center Brush center in client coordinates.
         * @param radius Brush radius in world units.
         * @param opacity Maximum opacity between zero and one.
         * @param material Environment layer index.
         * @param error Destination for a skipped or unreadable chunk message.
         */
        void
        paint(QVector3D center, float radius, float opacity, std::uint8_t material, QString& error);

        /** Substitutes one texture throughout an outdoor chunk, retaining its coverage.
         * @param coordinate Chunk indices.
         * @param old_layer Texture index within the chunk's current palette.
         * @param new_layer Environment layer index currently absent from the chunk.
         * @return True when the substitution was recorded.
         */
        [[nodiscard]] bool
        replace_texture(QPoint coordinate, std::uint8_t old_layer, std::uint8_t new_layer);

        /** Adds a catalog model to the chunk under the pointer.
         * @param position Client world position.
         * @param model Client-relative asset path.
         * @return Selection ID, or zero on failure.
         */
        [[nodiscard]] std::uint64_t place(QVector3D position, const std::string& model);

        /** Moves a placement; the destination chunk owns the new OPL entry.
         * @param id Selection ID.
         * @param position New client world position.
         * @return True when the placement could be moved.
         */
        [[nodiscard]] bool move(std::uint64_t id, QVector3D position);

        /** Removes a placement.
         * @param id Selection ID.
         * @return True if the selection existed.
         */
        [[nodiscard]] bool remove(std::uint64_t id);

        /** Publishes changed native files as one new directory.
         * @param destination New directory path.
         * @param error Destination error message on failure.
         * @return File count, possibly zero, or -1 on failure.
         */
        [[nodiscard]] int export_to(const QString& destination, QString& error);

        /** Converts a placement to client world coordinates.
         * @param coordinate Owning chunk coordinate.
         * @param object Placement with patch-local X/Z.
         * @return Client world position.
         */
        [[nodiscard]] static QVector3D position(QPoint coordinate,
                                                const terrain::OplInstance& object);

        /** Converts a world position to its owning chunk coordinate.
         * @param x Client world X.
         * @param z Client world Z.
         * @return X/Z chunk indices.
         */
        [[nodiscard]] static QPoint coordinate_at(float x, float z);

        /** @param coordinate X/Z chunk indices.
         * @return Canonical outdoor filename stem.
         */
        [[nodiscard]] static QString stem(QPoint coordinate);

        /** Drops clean chunks outside the visible and active neighborhoods.
         * @param retained Chunks to keep along with their immediate neighbors.
         */
        void evict_distant(const std::vector<QPoint>& retained);

      private:
        using Key = std::pair<int, int>;

        struct Source {
            QString terrain;
            QString objects;
            bool overlay_terrain = false;
            bool overlay_objects = false;
        };

        struct Textures {
            std::vector<std::uint8_t> layers;
            std::vector<std::uint8_t> alphas;
        };

        struct Snapshot {
            std::optional<std::vector<std::uint16_t>> heights;
            std::optional<Textures> textures;
            std::optional<std::vector<Placement>> objects;
        };

        struct Edit {
            std::uint64_t previous_state = 0;
            std::map<Key, Snapshot> chunks;
        };

        struct Loaded {
            WorldChunk chunk;
            QByteArray terrain_bytes;
            QByteArray objects_bytes;
        };

        [[nodiscard]] Loaded* ensure(QPoint coordinate);
        [[nodiscard]] std::pair<Key, std::size_t> find_object(std::uint64_t id) const;
        void remember_heights(Key key, const WorldChunk& chunk);
        void remember_textures(Key key, const WorldChunk& chunk);
        void remember_objects(Key key, const WorldChunk& chunk);
        void swap_edit(Edit& edit);

        QString m_root;
        QString m_textures;
        std::vector<terrain::LayerInfo> m_layers;
        std::map<Key, Source> m_sources;
        std::map<Key, Loaded> m_chunks;
        std::optional<Edit> m_edit;
        std::deque<Edit> m_undo;
        std::deque<Edit> m_redo;
        std::uint64_t m_next_id = 1;
        std::uint64_t m_next_state = 1;
        std::uint64_t m_state = 0;
        std::uint64_t m_saved_state = 0;
    };

}  // namespace openkal::world_editor
