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

#include "openkal/client/FreetypeFont.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/GtxLoader.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** GPU texture plus the native pixel size it was loaded with. */
    class TexInfo {
      public:
        TexInfo() = default;

        /**
         * Wraps an already created texture.
         * @param handle Engine texture handle.
         * @param width Native pixel width.
         * @param height Native pixel height.
         */
        TexInfo(engine::TextureHandle handle, int width, int height)
            : m_handle(handle), m_width(width), m_height(height) {}

        /** @return Engine texture handle, or a null handle when loading failed. */
        [[nodiscard]] engine::TextureHandle handle() const {
            return m_handle;
        }

        /** @return Native pixel width, or 0 when unloaded. */
        [[nodiscard]] int width() const {
            return m_width;
        }

        /** @return Native pixel height, or 0 when unloaded. */
        [[nodiscard]] int height() const {
            return m_height;
        }

        /**
         * Replaces the texture handle.
         * @param handle Engine texture handle.
         */
        void set_handle(engine::TextureHandle handle) {
            m_handle = handle;
        }

        /**
         * Replaces the native pixel size.
         * @param width Native pixel width.
         * @param height Native pixel height.
         */
        void set_size(int width, int height) {
            m_width = width;
            m_height = height;
        }

      private:
        /** Engine texture handle owned by the widget-tools cache. */
        engine::TextureHandle m_handle{};

        /** Native pixel width of the source bitmap. */
        int m_width = 0;

        /** Native pixel height of the source bitmap. */
        int m_height = 0;
    };

    /**
     * Shared renewal window chrome: info-menu 9-slice plus a top-right close box.
     *
     * The window size passed to the draw / hit helpers is the client (content) box;
     * the frame is placed around it using the padding below, so windows need no
     * per-frame offsets. Close defaults to the outer frame's top-right corner (the
     * X baked into the corner bitmap); the optional override moves that hitbox
     * relative to the client top-right.
     */
    class WindowFrameStyle {
      public:
        /**
         * Creates the standard Renewal info-menu frame.
         *
         * The close affordance is baked into the top-right corner bitmap. The
         * close hitbox therefore remains invisible and no separate exit glyph
         * is drawn over it.
         * @param close_button True to use the top-right bitmap with a close X.
         * @return Renewal info-menu frame description.
         */
        [[nodiscard]] static WindowFrameStyle info_menu(bool close_button);

        /** @return Outer frame padding left of the client box, in native units. */
        [[nodiscard]] float pad_left() const {
            return m_pad_left;
        }

        /** @return Outer frame padding above the client box, in native units. */
        [[nodiscard]] float pad_top() const {
            return m_pad_top;
        }

        /** @return Outer frame padding right of the client box, in native units. */
        [[nodiscard]] float pad_right() const {
            return m_pad_right;
        }

        /** @return Outer frame padding below the client box, in native units. */
        [[nodiscard]] float pad_bottom() const {
            return m_pad_bottom;
        }

        /**
         * Sets the outer frame padding around the client box.
         * @param left Padding left of the client box.
         * @param top Padding above the client box.
         * @param right Padding right of the client box.
         * @param bottom Padding below the client box.
         */
        void set_padding(float left, float top, float right, float bottom) {
            m_pad_left = left;
            m_pad_top = top;
            m_pad_right = right;
            m_pad_bottom = bottom;
        }

        /** @return True when the close hitbox is placed by the override offsets. */
        [[nodiscard]] bool close_override() const {
            return m_close_override;
        }

        /** @return True when the frame exposes an active close button. */
        [[nodiscard]] bool close_enabled() const {
            return m_close_enabled;
        }

        /** @param enabled True to enable the frame's close hitbox. */
        void set_close_enabled(bool enabled) {
            m_close_enabled = enabled;
        }

        /** @return Close hitbox X offset from the client top-right. */
        [[nodiscard]] float close_ox() const {
            return m_close_ox;
        }

        /** @return Close hitbox Y offset from the client top-right. */
        [[nodiscard]] float close_oy() const {
            return m_close_oy;
        }

        /** @return Close hitbox width in native units. */
        [[nodiscard]] float close_w() const {
            return m_close_w;
        }

        /** @return Close hitbox height in native units. */
        [[nodiscard]] float close_h() const {
            return m_close_h;
        }

        /**
         * Overrides the close hitbox relative to the client top-right corner.
         * @param ox X offset from the client top-right.
         * @param oy Y offset from the client top-right.
         * @param w Hitbox width.
         * @param h Hitbox height.
         */
        void set_close_box(float ox, float oy, float w, float h) {
            m_close_override = true;
            m_close_ox = ox;
            m_close_oy = oy;
            m_close_w = w;
            m_close_h = h;
        }

        /** @return Exit glyph X offset from the client top-right. */
        [[nodiscard]] float exit_ox() const {
            return m_exit_ox;
        }

        /** @return Exit glyph Y offset from the client top-right. */
        [[nodiscard]] float exit_oy() const {
            return m_exit_oy;
        }

        /** @return Bitmap file drawn inside the close hitbox. */
        [[nodiscard]] const std::string& exit_file() const {
            return m_exit_file;
        }

        /** @return True when the exit glyph is drawn on top of the frame. */
        [[nodiscard]] bool draw_exit() const {
            return m_draw_exit;
        }

        /**
         * Sets the exit glyph bitmap and its offsets.
         * @param file Bitmap file name.
         * @param ox X offset from the client top-right.
         * @param oy Y offset from the client top-right.
         */
        void set_exit(std::string file, float ox, float oy) {
            m_exit_file = std::move(file);
            m_exit_ox = ox;
            m_exit_oy = oy;
        }

        /**
         * Enables or disables drawing of the exit glyph.
         * @param draw True to draw the glyph.
         */
        void set_draw_exit(bool draw) {
            m_draw_exit = draw;
        }

        /** @return Title-bar drag height measured from the top of the outer frame. */
        [[nodiscard]] float title_drag_h() const {
            return m_title_drag_h;
        }

        /**
         * Sets the draggable title strip height.
         * @param height Height in native units.
         */
        void set_title_drag_h(float height) {
            m_title_drag_h = height;
        }

        /**
         * Replaces the 9-slice bitmaps.
         * @param upleft Top-left file.
         * @param upcenter Top-center file.
         * @param upright Top-right file.
         * @param left Left rail file.
         * @param right Right rail file.
         * @param downleft Bottom-left file.
         * @param downcenter Bottom-center file.
         * @param downright Bottom-right file.
         */
        void set_slice_files(std::string upleft,
                             std::string upcenter,
                             std::string upright,
                             std::string left,
                             std::string right,
                             std::string downleft,
                             std::string downcenter,
                             std::string downright) {
            m_upleft = std::move(upleft);
            m_upcenter = std::move(upcenter);
            m_upright = std::move(upright);
            m_left = std::move(left);
            m_right = std::move(right);
            m_downleft = std::move(downleft);
            m_downcenter = std::move(downcenter);
            m_downright = std::move(downright);
        }

        /** @return True when rails are tiled at native size instead of stretched. */
        [[nodiscard]] bool tile_edges() const {
            return m_tile_edges;
        }

        /**
         * Selects tiled vs stretched 9-slice edges.
         * @param tile True to repeat native-size rails.
         */
        void set_tile_edges(bool tile) {
            m_tile_edges = tile;
        }

        /** @return True to repeat rails with standard outer-rectangle geometry. */
        [[nodiscard]] bool repeat_edges() const {
            return m_repeat_edges;
        }

        /** @param repeat True to preserve rail texture density and crop partial repeats. */
        void set_repeat_edges(bool repeat) {
            m_repeat_edges = repeat;
        }

        /** @return Top-left 9-slice piece file name. */
        [[nodiscard]] const std::string& upleft() const {
            return m_upleft;
        }

        /** @return Top-center 9-slice piece file name. */
        [[nodiscard]] const std::string& upcenter() const {
            return m_upcenter;
        }

        /** @return Top-right 9-slice piece file name. */
        [[nodiscard]] const std::string& upright() const {
            return m_upright;
        }

        /** @return Left edge 9-slice piece file name. */
        [[nodiscard]] const std::string& left() const {
            return m_left;
        }

        /** @return Right edge 9-slice piece file name. */
        [[nodiscard]] const std::string& right() const {
            return m_right;
        }

        /** @return Bottom-left 9-slice piece file name. */
        [[nodiscard]] const std::string& downleft() const {
            return m_downleft;
        }

        /** @return Bottom-center 9-slice piece file name. */
        [[nodiscard]] const std::string& downcenter() const {
            return m_downcenter;
        }

        /** @return Bottom-right 9-slice piece file name. */
        [[nodiscard]] const std::string& downright() const {
            return m_downright;
        }

      private:
        /** Outer frame padding left of the client box (KalOnline info-menu fit). */
        float m_pad_left = 10.0f;

        /** Outer frame padding above the client box. */
        float m_pad_top = 40.0f;

        /** Outer frame padding right of the client box. */
        float m_pad_right = 7.0f;

        /** Outer frame padding below the client box. */
        float m_pad_bottom = 15.0f;

        /** True when the close hitbox uses the override offsets instead of the frame
         * corner. */
        bool m_close_override = false;

        /** True when the top-right corner is an active close button. */
        bool m_close_enabled = true;

        /** Close hitbox X offset from the client top-right. */
        float m_close_ox = -6.0f;

        /** Close hitbox Y offset from the client top-right. */
        float m_close_oy = -36.0f;

        /** Close hitbox width. */
        float m_close_w = 32.0f;

        /** Close hitbox height. */
        float m_close_h = 32.0f;

        /** Exit glyph X offset from the client top-right. */
        float m_exit_ox = -26.0f;

        /** Exit glyph Y offset from the client top-right. */
        float m_exit_oy = 10.0f;

        /** Bitmap drawn inside the close hitbox. */
        std::string m_exit_file = "info-exit small.bmp";

        /** True when the exit glyph is drawn. */
        bool m_draw_exit = false;

        /** Title-bar drag height measured from the top of the outer frame. */
        float m_title_drag_h = 44.0f;

        /** True when 9-slice edges repeat at native BMP size. */
        bool m_tile_edges = false;

        /** Repeats rail textures using standard outer-rectangle geometry. */
        bool m_repeat_edges = false;

        /** Top-left 9-slice piece (defaults are the skill / info-menu set). */
        std::string m_upleft = "info-menu-top left.bmp";

        /** Top-center 9-slice piece. */
        std::string m_upcenter = "info-menu-top center.bmp";

        /** Top-right 9-slice piece. */
        std::string m_upright = "info-menu-top right.bmp";

        /** Left edge 9-slice piece. */
        std::string m_left = "info-menu-left.bmp";

        /** Right edge 9-slice piece. */
        std::string m_right = "info-menu-right.bmp";

        /** Bottom-left 9-slice piece. */
        std::string m_downleft = "info-menu-bottom left.bmp";

        /** Bottom-center 9-slice piece. */
        std::string m_downcenter = "info-menu-bottom center.bmp";

        /** Bottom-right 9-slice piece. */
        std::string m_downright = "info-menu-bottom right.bmp";
    };

    /**
     * Shared UI helpers: BMP load / cache, bitmap-font text, textured quads and hit
     * tests.
     *
     * Individual HUD and window widgets live in their own headers and take a
     * reference to this class; it owns every UI texture handle it hands out and
     * releases them in destroy().
     */
    class WidgetTools {
      public:
        /**
         * Point-in-rectangle test used by every UI hitbox.
         * @param px Point X.
         * @param py Point Y.
         * @param x Rectangle left.
         * @param y Rectangle top.
         * @param w Rectangle width.
         * @param h Rectangle height.
         * @return True when the point lies inside the rectangle.
         */
        [[nodiscard]] static bool
        point_in_rect(float px, float py, float x, float y, float w, float h);

        /**
         * Loads a 24/32-bit uncompressed BMP and applies the KalOnline color keys.
         * @param path Absolute or relative file path.
         * @param out Destination image.
         * @return True on success.
         */
        [[nodiscard]] static bool load_bmp_rgba(const std::string& path, terrain::ImageRgba& out);

        /**
         * Places a client rect at the center of the screen, clamped to stay
         * on-screen.
         * @param x In/out rectangle left.
         * @param y In/out rectangle top.
         * @param w Rectangle width.
         * @param h Rectangle height.
         * @param screen_w Screen width.
         * @param screen_h Screen height.
         */
        static void
        center_rect(float& x, float& y, float w, float h, float screen_w, float screen_h);

        /**
         * Computes the outer frame rect that wraps a client box.
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param style Frame chrome description.
         * @param out_x Out frame left.
         * @param out_y Out frame top.
         * @param out_w Out frame width.
         * @param out_h Out frame height.
         */
        static void window_outer_rect(float client_x,
                                      float client_y,
                                      float client_w,
                                      float client_h,
                                      float scale,
                                      const WindowFrameStyle& style,
                                      float& out_x,
                                      float& out_y,
                                      float& out_w,
                                      float& out_h);

        /**
         * Hit test against the outer frame rect.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param style Frame chrome description.
         * @return True when the pointer is over the framed window.
         */
        [[nodiscard]] static bool window_outer_hit(float mx,
                                                   float my,
                                                   float client_x,
                                                   float client_y,
                                                   float client_w,
                                                   float client_h,
                                                   float scale = 1.0f,
                                                   const WindowFrameStyle& style = {});

        /**
         * Computes the close hitbox (frame top-right, or the style override).
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param style Frame chrome description.
         * @param out_x Out hitbox left.
         * @param out_y Out hitbox top.
         * @param out_w Out hitbox width.
         * @param out_h Out hitbox height.
         */
        static void window_close_rect(float client_x,
                                      float client_y,
                                      float client_w,
                                      float client_h,
                                      float scale,
                                      const WindowFrameStyle& style,
                                      float& out_x,
                                      float& out_y,
                                      float& out_w,
                                      float& out_h);

        /**
         * Hit test against the close hitbox.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param style Frame chrome description.
         * @return True when the pointer is over the close box.
         */
        [[nodiscard]] static bool window_close_hit(float mx,
                                                   float my,
                                                   float client_x,
                                                   float client_y,
                                                   float client_w,
                                                   float client_h,
                                                   float scale = 1.0f,
                                                   const WindowFrameStyle& style = {});

        /**
         * Computes the draggable title strip along the top of the outer frame
         * (excludes close).
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param style Frame chrome description.
         * @param out_x Out strip left.
         * @param out_y Out strip top.
         * @param out_w Out strip width.
         * @param out_h Out strip height.
         */
        static void window_title_drag_rect(float client_x,
                                           float client_y,
                                           float client_w,
                                           float client_h,
                                           float scale,
                                           const WindowFrameStyle& style,
                                           float& out_x,
                                           float& out_y,
                                           float& out_w,
                                           float& out_h);

        /** Binds an engine, seeds the bitmap search directories and bakes the glyph
         * atlas.
         * @param eng Engine used for texture creation and UI draws.
         * @param data_root Client data root (holds UI and HyperText).
         * @param runtime_data_root OpenKal RuntimeData root; empty uses the build/install root.
         */
        void init(engine::IEngine& eng,
                  const std::string& data_root,
                  const std::string& runtime_data_root = {});

        /** Releases every cached texture and the glyph atlas. */
        void destroy();

        /** @return Bound engine, or nullptr before init(). */
        [[nodiscard]] engine::IEngine* renderer() const {
            return m_engine;
        }

        /** @return Directories searched for bitmaps, in priority order. */
        [[nodiscard]] const std::vector<std::string>& search_dirs() const {
            return m_search_dirs;
        }

        /**
         * Appends a bitmap search directory when it is not already present.
         * @param dir Directory path; ignored when empty.
         */
        void add_search_dir(std::string dir);

        /** @return Glyph cell width of the bitmap font, in pixels. */
        [[nodiscard]] int font_glyph_width() const {
            return m_font_gw;
        }

        /** @return Glyph cell height of the bitmap font, in pixels. */
        [[nodiscard]] int font_glyph_height() const {
            return m_font_gh;
        }

        /**
         * Loads (or returns the cached) texture for a bitmap, GTX, or DDS file.
         * @param filename File name relative to a search directory.
         * @return Texture and native size; a null handle when the file is missing.
         */
        [[nodiscard]] TexInfo tex_info(const std::string& filename);

        /**
         * Drops a cached texture (and its resolve entry) so the next load retries
         * from disk.
         * @param filename Bitmap file name relative to a search directory.
         */
        void invalidate_tex(const std::string& filename);

        /**
         * Convenience accessor for the texture handle only.
         * @param filename Bitmap file name relative to a search directory.
         * @return Texture handle, or a null handle when the file is missing.
         */
        [[nodiscard]] engine::TextureHandle tex(const std::string& filename) {
            return tex_info(filename).handle();
        }

        /**
         * Loads an item-icon bitmap with compatibility behavior color keys (RGB 4,4,4 and
         * 15,15,15). Cached separately from the same file used as plain UI chrome.
         * @param filename File name relative to a search directory.
         * @return Texture and native size; a null handle when the file is missing.
         */
        [[nodiscard]] TexInfo item_tex_info(const std::string& filename);

        /**
         * Texture handle for an item icon (compatibility behavior color keys applied).
         * @param filename File name relative to a search directory.
         * @return Texture handle, or a null handle when the file is missing.
         */
        [[nodiscard]] engine::TextureHandle item_tex(const std::string& filename) {
            return item_tex_info(filename).handle();
        }

        /**
         * Resolves a bitmap stem or file name to an on-disk file name, fixing case
         * and extension.
         * @param stem_or_file Stem (active006) or file name (slot.bmp).
         * @return Resolved file name, or the input when nothing matched.
         */
        [[nodiscard]] std::string resolve_bmp(const std::string& stem_or_file) const;

        /**
         * Draws a bitmap at its native size.
         * @param filename Bitmap file name.
         * @param x Destination left.
         * @param y Destination top.
         * @param a Alpha multiplier.
         */
        void draw_tex_native(const std::string& filename, float x, float y, float a = 1.0f);

        /**
         * Draws a bitmap stretched into a rect.
         * @param filename Bitmap file name.
         * @param x Destination left.
         * @param y Destination top.
         * @param w Destination width.
         * @param h Destination height.
         * @param a Alpha multiplier.
         */
        void
        draw_tex(const std::string& filename, float x, float y, float w, float h, float a = 1.0f);

        /**
         * Draws a tinted bitmap stretched into a rect.
         * @param filename Bitmap file name.
         * @param x Destination left.
         * @param y Destination top.
         * @param w Destination width.
         * @param h Destination height.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param a Alpha multiplier.
         */
        void draw_tex(const std::string& filename,
                      float x,
                      float y,
                      float w,
                      float h,
                      float r,
                      float g,
                      float b,
                      float a = 1.0f);

        /**
         * Draws a bitmap rotated around the centre of its destination rectangle.
         * @param filename Bitmap file name.
         * @param x Destination left.
         * @param y Destination top.
         * @param w Destination width.
         * @param h Destination height.
         * @param rotation_rad Clockwise rotation in screen coordinates, in radians.
         * @param a Alpha multiplier.
         */
        void draw_tex_rotated(const std::string& filename,
                              float x,
                              float y,
                              float w,
                              float h,
                              float rotation_rad,
                              float a = 1.0f);

        /**
         * Draws an item-icon bitmap with compatibility behavior color keys applied.
         * @param filename Bitmap file name.
         * @param x Destination left.
         * @param y Destination top.
         * @param w Destination width.
         * @param h Destination height.
         * @param a Alpha multiplier.
         * @param grayscale True to render the icon without color.
         */
        void draw_item_tex(const std::string& filename,
                           float x,
                           float y,
                           float w,
                           float h,
                           float a = 1.0f,
                           bool grayscale = false);

        /**
         * Draws a texture sub-rect stretched into a destination rect.
         * @param filename File name relative to a search directory.
         * @param x Destination left.
         * @param y Destination top.
         * @param w Destination width.
         * @param h Destination height.
         * @param u0 Left texture coordinate in 0..1.
         * @param v0 Top texture coordinate in 0..1.
         * @param u1 Right texture coordinate in 0..1.
         * @param v1 Bottom texture coordinate in 0..1.
         * @param a Alpha multiplier.
         */
        void draw_tex_uv(const std::string& filename,
                         float x,
                         float y,
                         float w,
                         float h,
                         float u0,
                         float v0,
                         float u1,
                         float v1,
                         float a = 1.0f);

        /**
         * Draws the info-menu 9-slice frame around a client box.
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param style Frame chrome description.
         */
        void draw_window_frame(float client_x,
                               float client_y,
                               float client_w,
                               float client_h,
                               float scale = 1.0f,
                               const WindowFrameStyle& style = {});

        /**
         * Draws a vertical info-menu rail through the client box (same bitmap as the
         * side frame).
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param split_x X offset from the client left to the rail center.
         * @param scale UI scale.
         * @param style Frame chrome description.
         */
        void draw_window_vsplit(float client_x,
                                float client_y,
                                float client_w,
                                float client_h,
                                float split_x,
                                float scale = 1.0f,
                                const WindowFrameStyle& style = {});

        /**
         * Draws the exit glyph centered in the close hitbox when the style enables
         * it.
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param style Frame chrome description.
         */
        void draw_window_close(float client_x,
                               float client_y,
                               float client_w,
                               float client_h,
                               float scale = 1.0f,
                               const WindowFrameStyle& style = {});

        /**
         * Draws a left / center / right progress bar built from "<prefix>-L|C|R.bmp".
         * @param prefix Bitmap name prefix.
         * @param x Bar left.
         * @param y Bar top.
         * @param full_w Bar width at ratio 1.
         * @param h Bar height.
         * @param ratio Fill ratio, clamped to [0, 1].
         */
        void
        draw_progress_lcr(const char* prefix, float x, float y, float full_w, float h, float ratio);

        /**
         * Draws an untextured two-tone progress bar.
         * @param x Bar left.
         * @param y Bar top.
         * @param full_w Bar width at ratio 1.
         * @param h Bar height.
         * @param ratio Fill ratio, clamped to [0, 1].
         * @param r Fill red.
         * @param g Fill green.
         * @param b Fill blue.
         */
        void
        draw_bar(float x, float y, float full_w, float h, float ratio, float r, float g, float b);

        /**
         * Overlay chrome matching the chat box: black fill at 0.45 alpha and the
         * chatting-box BMP edges (3px at scale 1).
         * @param x Panel left.
         * @param y Panel top.
         * @param w Panel width.
         * @param h Panel height.
         * @param scale UI scale applied to the border thickness.
         * @param alpha Extra alpha multiplier on fill and frame.
         */
        void draw_overlay_panel(
            float x, float y, float w, float h, float scale = 1.0f, float alpha = 1.0f);

        /**
         * Thickness of draw_overlay_panel's chatting-box frame.
         * @param scale UI scale.
         * @return Border thickness in pixels.
         */
        [[nodiscard]] static float overlay_border_thickness(float scale = 1.0f);

        /**
         * Draws the skill-slot cooldown clock (dark triangle-fan pie).
         * @param x Icon left.
         * @param y Icon top.
         * @param size Icon width and height.
         * @param remaining Remaining / total in 0..1 (1 = just started).
         */
        void draw_cooldown_sweep(float x, float y, float size, float remaining);

        /**
         * Draws a right-aligned stack count on an item icon (fits at least 9999).
         * @param font FreeType face; ignored when null or not ready.
         * @param ix Icon left.
         * @param iy Icon top.
         * @param iw Icon width.
         * @param ih Icon height.
         * @param scale UI scale used for padding and font size.
         * @param count Stack size; nothing is drawn when count <= 0.
         * @param high_contrast Draws a doubled-size black count with a white outline;
         * values of 10,000 or more are abbreviated in 10K steps.
         * @param text_scale Additional multiplier for the count's text size.
         */
        void draw_stack_count(const FreetypeFont* font,
                              float ix,
                              float iy,
                              float iw,
                              float ih,
                              float scale,
                              int count,
                              bool high_contrast = false,
                              float text_scale = 1.0f) const {
            if (!font || !font->ready() || count <= 0 || !m_engine) {
                return;
            }
            const float fs =
                (high_contrast ? std::max(0.8f, scale * 1.1f) : std::max(0.4f, scale * 0.55f)) *
                text_scale;
            const std::string txt = high_contrast && count >= 10'000
                                        ? "> " + std::to_string((count / 10'000) * 10) + "K"
                                        : std::to_string(count);
            const float tw = font->text_width(txt, fs);
            const float th = font->line_height() * fs;
            const float x = ix + iw - tw - 1.0f * scale;
            const float y = iy + ih - th - 1.0f * scale;
            if (high_contrast) {
                const float outline = std::max(1.0f, scale);
                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        if (offset_x == 0 && offset_y == 0) {
                            continue;
                        }
                        font->draw(*m_engine,
                                   x + static_cast<float>(offset_x) * outline,
                                   y + static_cast<float>(offset_y) * outline,
                                   txt,
                                   1.0f,
                                   1.0f,
                                   1.0f,
                                   fs);
                    }
                }
            }
            font->draw(*m_engine,
                       x,
                       y,
                       txt,
                       high_contrast ? 0.0f : 1.0f,
                       high_contrast ? 0.0f : 1.0f,
                       high_contrast ? 0.0f : 1.0f,
                       fs);
        }

        /**
         * Measures a string in the bitmap font.
         * @param scale Glyph scale (1 = native).
         * @param text Text to measure.
         * @return Width in pixels.
         */
        [[nodiscard]] float text_width(float scale, const std::string& text) const;

        /**
         * Measures a string in the bitmap font.
         * @param scale Integer glyph scale (1 = native).
         * @param text Text to measure.
         * @return Width in pixels.
         */
        [[nodiscard]] float text_width(int scale, const std::string& text) const {
            return text_width(static_cast<float>(scale), text);
        }

        /**
         * Draws left-aligned bitmap-font text.
         * @param x Text left.
         * @param y Text top.
         * @param scale Glyph scale (1 = native).
         * @param text Text to draw.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         */
        void draw_text_glyphs(float x,
                              float y,
                              float scale,
                              const std::string& text,
                              float r = 1,
                              float g = 1,
                              float b = 1);

        /**
         * Draws left-aligned bitmap-font text.
         * @param x Text left.
         * @param y Text top.
         * @param scale Integer glyph scale (1 = native).
         * @param text Text to draw.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         */
        void draw_text_glyphs(float x,
                              float y,
                              int scale,
                              const std::string& text,
                              float r = 1,
                              float g = 1,
                              float b = 1) {
            draw_text_glyphs(x, y, static_cast<float>(scale), text, r, g, b);
        }

        /**
         * Draws bitmap-font text centered on a horizontal position.
         * @param cx Center X.
         * @param y Text top.
         * @param scale Glyph scale (1 = native).
         * @param text Text to draw.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         */
        void draw_text_centered(float cx,
                                float y,
                                float scale,
                                const std::string& text,
                                float r = 1,
                                float g = 1,
                                float b = 1);

        /**
         * Draws bitmap-font text centered on a horizontal position.
         * @param cx Center X.
         * @param y Text top.
         * @param scale Integer glyph scale (1 = native).
         * @param text Text to draw.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         */
        void draw_text_centered(float cx,
                                float y,
                                int scale,
                                const std::string& text,
                                float r = 1,
                                float g = 1,
                                float b = 1) {
            draw_text_centered(cx, y, static_cast<float>(scale), text, r, g, b);
        }

        /**
         * Draws a row of identical segments (EXP bar ticks).
         * @param segment_bmp Segment bitmap file name.
         * @param x0 First segment left.
         * @param y Segment top.
         * @param seg_w Segment width.
         * @param seg_h Segment height.
         * @param gap Gap between segments.
         * @param count Number of segments to draw.
         */
        void draw_tex_segments(const char* segment_bmp,
                               float x0,
                               float y,
                               float seg_w,
                               float seg_h,
                               float gap,
                               int count);

      private:
        /**
         * Ramps alpha over the near-black trough pixels of a Kal UI bitmap.
         * @param img Image modified in place.
         */
        static void apply_dark_patch_alpha(terrain::ImageRgba& img);

        /**
         * Keys item-icon fill RGB(4,4,4) and compatibility behavior RGB(15,15,15).
         * @param img Image modified in place.
         */
        static void apply_item_icon_color_key(terrain::ImageRgba& img);

        /**
         * Loads a UI or item-icon texture into the cache.
         * @param filename File name relative to a search directory.
         * @param item_icon True to apply compatibility behavior item color keys and cache
         * separately.
         * @param grayscale True to cache a grayscale variant with alpha preserved.
         * @return Texture and native size; a null handle when the file is missing.
         */
        [[nodiscard]] TexInfo
        load_cached_tex(const std::string& filename, bool item_icon, bool grayscale = false);

        /** Builds the 5x7 glyph bitmask table and uploads the font atlas. */
        void build_font();

        /** Engine used for texture creation and UI draws; not owned. */
        engine::IEngine* m_engine = nullptr;

        /** Directories searched for bitmaps, in priority order. */
        std::vector<std::string> m_search_dirs;

        /** Loaded textures keyed by the file name the caller asked for. */
        std::unordered_map<std::string, TexInfo> m_cache;

        /** Bold FreeType face for the shared Dragon footer inscription. */
        FreetypeFont m_dragon_brand_font;

        /** Baked bitmap-font atlas. */
        engine::TextureHandle m_font_tex{};

        /** Glyph cell width in pixels. */
        int m_font_gw = 6;

        /** Glyph cell height in pixels. */
        int m_font_gh = 10;

        /** Glyph columns in the font atlas. */
        int m_font_atlas_cols = 16;

        /** Font atlas width in pixels. */
        int m_font_atlas_w = 0;

        /** Font atlas height in pixels. */
        int m_font_atlas_h = 0;

        /** Per-glyph row bitmasks for ASCII 32..126. */
        std::vector<std::uint8_t> m_glyph_bits;

        /** Memoised results of resolve_bmp(). */
        mutable std::unordered_map<std::string, std::string> m_resolve_cache;
    };

    /** Compatibility alias used by older call sites. */
    using UiAssets = WidgetTools;

}  // namespace openkal::client
