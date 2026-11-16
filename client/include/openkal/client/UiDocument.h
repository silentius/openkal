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

#include "openkal/platform/Json.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** Document kind for an openkal.ui/v1 layout. */
    enum class UiKind {
        Window = 0,
        Hud,
    };

    /** Chrome assembly mode. */
    enum class UiChromeKind {
        NineSlice = 0,
        Bitmap,
        Pieces,
    };

    /** How 9-slice rails fill the outer rect. */
    enum class UiChromeEdges {
        Tile = 0,
        Stretch,
        Repeat, /**< Native-size repeats with outer-rectangle geometry. */
    };

    /** Pixel rectangle within an image asset; zero size selects the full image. */
    struct UiImageSource {
        float x = 0.0f; /**< Left source pixel. */
        float y = 0.0f; /**< Top source pixel. */
        float w = 0.0f; /**< Source width. */
        float h = 0.0f; /**< Source height. */
    };

    /** Widget type in an openkal.ui/v1 widgets array. */
    enum class UiWidgetType {
        Image = 0,
        Text,
        Button,
        Progress,
        IconSlot,
        IconGrid,
        Tabs,
        Fill,
        Border,
        Input,
        List,
        Canvas,
        Unknown,
    };

    /** One tab strip entry. */
    class UiTabItem {
      public:
        /** @return Tab id stored in the bound variable. */
        [[nodiscard]] const std::string& id() const {
            return m_id;
        }

        /** @param v Tab id. */
        void set_id(std::string v) {
            m_id = std::move(v);
        }

        /** @return Caption, may contain ${…}. */
        [[nodiscard]] const std::string& label() const {
            return m_label;
        }

        /** @param v Caption. */
        void set_label(std::string v) {
            m_label = std::move(v);
        }

        /** @return Selected BMP. */
        [[nodiscard]] const std::string& file_on() const {
            return m_file_on;
        }

        /** @param v Selected BMP. */
        void set_file_on(std::string v) {
            m_file_on = std::move(v);
        }

        /** @return Idle BMP. */
        [[nodiscard]] const std::string& file_off() const {
            return m_file_off;
        }

        /** @param v Idle BMP. */
        void set_file_off(std::string v) {
            m_file_off = std::move(v);
        }

        /** @return Client-box X. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @param v Client-box X. */
        void set_x(float v) {
            m_x = v;
        }

        /** @return Width. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @param v Width. */
        void set_w(float v) {
            m_w = v;
        }

      private:
        /** Tab id. */
        std::string m_id;

        /** Caption. */
        std::string m_label;

        /** Selected BMP. */
        std::string m_file_on;

        /** Idle BMP. */
        std::string m_file_off;

        /** Client-box X. */
        float m_x = 0.0f;

        /** Width. */
        float m_w = 0.0f;
    };

    /** One widgets[] entry. */
    class UiWidget {
      public:
        /** @return Widget type. */
        [[nodiscard]] UiWidgetType type() const {
            return m_type;
        }

        /** @param v Widget type. */
        void set_type(UiWidgetType v) {
            m_type = v;
        }

        /** @return Stable id. */
        [[nodiscard]] const std::string& id() const {
            return m_id;
        }

        /** @param v Stable id. */
        void set_id(std::string v) {
            m_id = std::move(v);
        }

        /** @return Client-box X. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @param v Client-box X. */
        void set_x(float v) {
            m_x = v;
        }

        /** @return Client-box Y. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @param v Client-box Y. */
        void set_y(float v) {
            m_y = v;
        }

        /** @return Width (0 = native BMP). */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @param v Width. */
        void set_w(float v) {
            m_w = v;
        }

        /** @return Height (0 = native BMP). */
        [[nodiscard]] float h() const {
            return m_h;
        }

        /** @param v Height. */
        void set_h(float v) {
            m_h = v;
        }

        /** @return True when the widget is drawn. */
        [[nodiscard]] bool visible() const {
            return m_visible;
        }

        /** @param v Visibility. */
        void set_visible(bool v) {
            m_visible = v;
        }

        /** @return Condition string, or empty. */
        [[nodiscard]] const std::string& visible_when() const {
            return m_visible_when;
        }

        /** @param v Condition. */
        void set_visible_when(std::string v) {
            m_visible_when = std::move(v);
        }

        /** @return Interpolated text template. */
        [[nodiscard]] const std::string& text() const {
            return m_text;
        }

        /** @param v Text template. */
        void set_text(std::string v) {
            m_text = std::move(v);
        }

        /** @return Whether an input wraps and displays multiple lines. */
        [[nodiscard]] bool multiline() const {
            return m_multiline;
        }

        /** @param value Enables multiline input rendering. */
        void set_multiline(bool value) {
            m_multiline = value;
        }

        /** @return Color (#RRGGBB or ${theme.colors.*}). */
        [[nodiscard]] const std::string& color() const {
            return m_color;
        }

        /** @param v Color. */
        void set_color(std::string v) {
            m_color = std::move(v);
        }

        /** @return BMP file or progress prefix. */
        [[nodiscard]] const std::string& file() const {
            return m_file;
        }

        /** @param v File. */
        void set_file(std::string v) {
            m_file = std::move(v);
        }

        /** @return Progress rendering mode: lcr (default) or crop. */
        const std::string& fill_mode() const {
            return m_fill_mode;
        }

        /** @param value Progress rendering mode. */
        void set_fill_mode(std::string value) {
            m_fill_mode = std::move(value);
        }

        /** @return Crop direction: left_to_right or bottom_to_top. */
        const std::string& direction() const {
            return m_direction;
        }

        /** @param value Progress crop direction. */
        void set_direction(std::string value) {
            m_direction = std::move(value);
        }

        /** @return Optional source rectangle in asset pixels. */
        const UiImageSource& source() const {
            return m_source;
        }

        /** @param source Source rectangle in asset pixels. */
        void set_source(UiImageSource source) {
            m_source = source;
        }

        /** @return True when the image repeats at native size inside its bounds. */
        bool repeat() const {
            return m_repeat;
        }

        /** @param repeat Repeat the selected source rectangle at native size. */
        void set_repeat(bool repeat) {
            m_repeat = repeat;
        }

        /** @return Catalog bind path. */
        [[nodiscard]] const std::string& bind() const {
            return m_bind;
        }

        /** @param v Bind path. */
        void set_bind(std::string v) {
            m_bind = std::move(v);
        }

        /** @return Click action. */
        [[nodiscard]] const std::string& action() const {
            return m_action;
        }

        /** @param v Action. */
        void set_action(std::string v) {
            m_action = std::move(v);
        }

        /** @return Button / tab label template. */
        [[nodiscard]] const std::string& label() const {
            return m_label;
        }

        /** @param v Label. */
        void set_label(std::string v) {
            m_label = std::move(v);
        }

        /** @return Label X offset from the button top-left, in native pixels. */
        [[nodiscard]] float label_ox() const {
            return m_label_ox;
        }

        /**
         * @param v Offset from the button top-left, in native pixels.
         */
        void set_label_ox(float v) {
            m_label_ox = v;
        }

        /** @return Label Y offset from the button top-left, in native pixels. */
        [[nodiscard]] float label_oy() const {
            return m_label_oy;
        }

        /**
         * @param v Offset from the button top-left, in native pixels.
         */
        void set_label_oy(float v) {
            m_label_oy = v;
        }

        /** @return Default button BMP. */
        [[nodiscard]] const std::string& file_default() const {
            return m_file_default;
        }

        /** @param v File. */
        void set_file_default(std::string v) {
            m_file_default = std::move(v);
        }

        /** @return Empty-slot BMP. */
        [[nodiscard]] const std::string& empty_file() const {
            return m_empty_file;
        }

        /** @param v File. */
        void set_empty_file(std::string v) {
            m_empty_file = std::move(v);
        }

        /** @return Slot frame BMP. */
        [[nodiscard]] const std::string& frame_file() const {
            return m_frame_file;
        }

        /** @param v File. */
        void set_frame_file(std::string v) {
            m_frame_file = std::move(v);
        }

        /** @return Frame width. */
        [[nodiscard]] float frame_w() const {
            return m_frame_w;
        }

        /** @param v Width. */
        void set_frame_w(float v) {
            m_frame_w = v;
        }

        /** @return Frame height. */
        [[nodiscard]] float frame_h() const {
            return m_frame_h;
        }

        /** @param v Height. */
        void set_frame_h(float v) {
            m_frame_h = v;
        }

        /** @return Grid columns. */
        [[nodiscard]] int cols() const {
            return m_cols;
        }

        /** @param v Columns. */
        void set_cols(int v) {
            m_cols = v;
        }

        /** @return Grid rows. */
        [[nodiscard]] int rows() const {
            return m_rows;
        }

        /** @param v Rows. */
        void set_rows(int v) {
            m_rows = v;
        }

        /** @return Cell width. */
        [[nodiscard]] float cell_w() const {
            return m_cell_w;
        }

        /** @param v Width. */
        void set_cell_w(float v) {
            m_cell_w = v;
        }

        /** @return Cell height. */
        [[nodiscard]] float cell_h() const {
            return m_cell_h;
        }

        /** @param v Height. */
        void set_cell_h(float v) {
            m_cell_h = v;
        }

        /** @return Column pitch. */
        [[nodiscard]] float pitch_x() const {
            return m_pitch_x;
        }

        /** @param v Pitch. */
        void set_pitch_x(float v) {
            m_pitch_x = v;
        }

        /** @return Row pitch. */
        [[nodiscard]] float pitch_y() const {
            return m_pitch_y;
        }

        /** @param v Pitch. */
        void set_pitch_y(float v) {
            m_pitch_y = v;
        }

        /** @return FreeType pixel size (0 = theme default). */
        [[nodiscard]] int pixel_size() const {
            return m_pixel_size;
        }

        /** @param v Pixel size. */
        void set_pixel_size(int v) {
            m_pixel_size = v;
        }

        /** @return True for a bold face. */
        [[nodiscard]] bool bold() const {
            return m_bold;
        }

        /** @param v Bold flag. */
        void set_bold(bool v) {
            m_bold = v;
        }

        /** @return Align: left, center, or right. */
        [[nodiscard]] const std::string& align() const {
            return m_align;
        }

        /** @param v Align. */
        void set_align(std::string v) {
            m_align = std::move(v);
        }

        /** @return Pending-color condition, or empty. */
        [[nodiscard]] const std::string& pending_when() const {
            return m_pending_when;
        }

        /** @param v Condition. */
        void set_pending_when(std::string v) {
            m_pending_when = std::move(v);
        }

        /** @return Drop-accept tags. */
        [[nodiscard]] const std::vector<std::string>& accept() const {
            return m_accept;
        }

        /** @param v Tags. */
        void set_accept(std::vector<std::string> v) {
            m_accept = std::move(v);
        }

        /** @return Tab items. */
        [[nodiscard]] const std::vector<UiTabItem>& tabs() const {
            return m_tabs;
        }

        /** @return Mutable tab items. */
        [[nodiscard]] std::vector<UiTabItem>& tabs() {
            return m_tabs;
        }

        /** @return Alpha 0..1. */
        [[nodiscard]] float alpha() const {
            return m_alpha;
        }

        /** @param v Alpha. */
        void set_alpha(float v) {
            m_alpha = v;
        }

        /** @return Canvas kind (minimap / level_orb). */
        [[nodiscard]] const std::string& canvas_kind() const {
            return m_canvas_kind;
        }

        /** @param v Kind. */
        void set_canvas_kind(std::string v) {
            m_canvas_kind = std::move(v);
        }

        /** @return Canvas clipping shape: rect (default) or circle. */
        [[nodiscard]] const std::string& shape() const {
            return m_shape;
        }

        /** Sets the canvas clipping shape.
         * @param value Circle or rect; unknown values use rect.
         */
        void set_shape(std::string_view value) {
            m_shape = value == "circle" ? "circle" : "rect";
        }

        /** @return Optional `_doc` note. */
        [[nodiscard]] const std::string& doc_note() const {
            return m_doc_note;
        }

        /** @param v `_doc` note. */
        void set_doc_note(std::string v) {
            m_doc_note = std::move(v);
        }

        /** @return On-tab color. */
        [[nodiscard]] const std::string& color_on() const {
            return m_color_on;
        }

        /** @param v Color. */
        void set_color_on(std::string v) {
            m_color_on = std::move(v);
        }

        /** @return Off-tab color. */
        [[nodiscard]] const std::string& color_off() const {
            return m_color_off;
        }

        /** @param v Color. */
        void set_color_off(std::string v) {
            m_color_off = std::move(v);
        }

        /** @return Hover button BMP. */
        [[nodiscard]] const std::string& file_over() const {
            return m_file_over;
        }

        /** @param v File. */
        void set_file_over(std::string v) {
            m_file_over = std::move(v);
        }

        /** @return Pressed / selected button BMP. */
        [[nodiscard]] const std::string& file_on() const {
            return m_file_on;
        }

        /** @param v File. */
        void set_file_on(std::string v) {
            m_file_on = std::move(v);
        }

        /** @return Disabled button BMP. */
        [[nodiscard]] const std::string& file_off() const {
            return m_file_off;
        }

        /** @param v File. */
        void set_file_off(std::string v) {
            m_file_off = std::move(v);
        }

        /** @return Grid page expression, or empty. */
        [[nodiscard]] const std::string& page() const {
            return m_page;
        }

        /** @param v Page expression. */
        void set_page(std::string v) {
            m_page = std::move(v);
        }

        /** @return Grid filter expression, or empty. */
        [[nodiscard]] const std::string& filter() const {
            return m_filter;
        }

        /** @param v Filter expression. */
        void set_filter(std::string v) {
            m_filter = std::move(v);
        }

        /** @return ID of the widget bounding this vertically scrolled content. */
        const std::string& viewport() const {
            return m_viewport;
        }

        /** Sets the viewport widget ID.
         * @param value Bounding widget ID, or empty for unscrolled content.
         */
        void set_viewport(std::string value) {
            m_viewport = std::move(value);
        }

        /** @return Catalog path supplying the vertical scroll offset in native pixels. */
        const std::string& scroll_bind() const {
            return m_scroll_bind;
        }

        /** Sets the vertical scroll binding.
         * @param value Catalog path.
         */
        void set_scroll_bind(std::string value) {
            m_scroll_bind = std::move(value);
        }

        /** @return First source cell after clipping a grid to complete rows. */
        int first_slot() const {
            return m_first_slot;
        }

        /** Sets the transient source-cell offset used by a clipped grid.
         * @param value First source cell.
         */
        void set_first_slot(int value) {
            m_first_slot = value;
        }

        /** @return True when the slot shows a tooltip. */
        [[nodiscard]] bool tooltip() const {
            return m_tooltip;
        }

        /** @param v Tooltip flag. */
        void set_tooltip(bool v) {
            m_tooltip = v;
        }

        /** @return True when the slot may be dragged. */
        [[nodiscard]] bool drag() const {
            return m_drag;
        }

        /** @param v Drag flag. */
        void set_drag(bool v) {
            m_drag = v;
        }

        /** @return Badge text template. */
        [[nodiscard]] const std::string& badge_text() const {
            return m_badge_text;
        }

        /** @param v Badge text. */
        void set_badge_text(std::string v) {
            m_badge_text = std::move(v);
        }

        /** @return Badge visibility condition. */
        [[nodiscard]] const std::string& badge_visible_when() const {
            return m_badge_visible_when;
        }

        /** @param v Condition. */
        void set_badge_visible_when(std::string v) {
            m_badge_visible_when = std::move(v);
        }

        /** @return X from the client right edge, or a negative sentinel when unused. */
        [[nodiscard]] float x_from_right() const {
            return m_x_from_right;
        }

        /** @param v Offset from the client right edge to the widget origin. */
        void set_x_from_right(float v) {
            m_x_from_right = v;
        }

        /** @return Y from the client bottom, or a negative sentinel when unused. */
        [[nodiscard]] float y_from_bottom() const {
            return m_y_from_bottom;
        }

        /** @param v Offset from the client bottom. */
        void set_y_from_bottom(float v) {
            m_y_from_bottom = v;
        }

        /** @return Border thickness. */
        [[nodiscard]] float thickness() const {
            return m_thickness;
        }

        /** @param v Thickness. */
        void set_thickness(float v) {
            m_thickness = v;
        }

        /** @return Top border BMP. */
        [[nodiscard]] const std::string& border_top() const {
            return m_border_top;
        }

        /** @param v File. */
        void set_border_top(std::string v) {
            m_border_top = std::move(v);
        }

        /** @return Bottom border BMP. */
        [[nodiscard]] const std::string& border_bottom() const {
            return m_border_bottom;
        }

        /** @param v File. */
        void set_border_bottom(std::string v) {
            m_border_bottom = std::move(v);
        }

        /** @return Left border BMP. */
        [[nodiscard]] const std::string& border_left() const {
            return m_border_left;
        }

        /** @param v File. */
        void set_border_left(std::string v) {
            m_border_left = std::move(v);
        }

        /** @return Right border BMP. */
        [[nodiscard]] const std::string& border_right() const {
            return m_border_right;
        }

        /** @param v File. */
        void set_border_right(std::string v) {
            m_border_right = std::move(v);
        }

        /** @return Grid selection color. */
        [[nodiscard]] const std::string& select_color() const {
            return m_select_color;
        }

        /** @param v Color. */
        void set_select_color(std::string v) {
            m_select_color = std::move(v);
        }

        /** @return List row height. */
        [[nodiscard]] float line_height() const {
            return m_line_height;
        }

        /** @param v Height. */
        void set_line_height(float v) {
            m_line_height = v;
        }

        /** @return Canvas center X (level orb), or 0 to use x. */
        [[nodiscard]] float cx() const {
            return m_cx;
        }

        /** @param v Center X. */
        void set_cx(float v) {
            m_cx = v;
        }

        /** @return Canvas center Y (level orb), or 0 to use y. */
        [[nodiscard]] float cy() const {
            return m_cy;
        }

        /** @param v Center Y. */
        void set_cy(float v) {
            m_cy = v;
        }

        /** @return Level-orb inner radius. */
        [[nodiscard]] float inner() const {
            return m_inner;
        }

        /** @param v Inner radius. */
        void set_inner(float v) {
            m_inner = v;
        }

      private:
        /** Widget type. */
        UiWidgetType m_type = UiWidgetType::Unknown;

        /** Stable id. */
        std::string m_id;

        /** Client-box X. */
        float m_x = 0.0f;

        /** Client-box Y. */
        float m_y = 0.0f;

        /** Width. */
        float m_w = 0.0f;

        /** Height. */
        float m_h = 0.0f;

        /** Draw flag. */
        bool m_visible = true;

        /** Visibility condition. */
        std::string m_visible_when;

        /** Text template. */
        std::string m_text;
        /** Top-aligned, wrapping input content. */
        bool m_multiline = false;

        /** Color. */
        std::string m_color;

        /** File / prefix. */
        std::string m_file;
        std::string m_fill_mode = "lcr";
        std::string m_direction = "left_to_right";

        /** Optional image crop and native-size tiling. */
        UiImageSource m_source;
        bool m_repeat = false;

        /** Bind path. */
        std::string m_bind;

        /** Action. */
        std::string m_action;

        /** Label template. */
        std::string m_label;

        /** Label X offset from the button top-left. */
        float m_label_ox = 0.0f;

        /** Label Y offset from the button top-left. */
        float m_label_oy = 0.0f;

        /** Default button BMP. */
        std::string m_file_default;

        /** Empty slot BMP. */
        std::string m_empty_file;

        /** Frame BMP. */
        std::string m_frame_file;

        /** Frame width. */
        float m_frame_w = 0.0f;

        /** Frame height. */
        float m_frame_h = 0.0f;

        /** Grid columns. */
        int m_cols = 1;

        /** Grid rows. */
        int m_rows = 1;

        /** Cell width. */
        float m_cell_w = 32.0f;

        /** Cell height. */
        float m_cell_h = 32.0f;

        /** Column pitch. */
        float m_pitch_x = 32.0f;

        /** Row pitch. */
        float m_pitch_y = 32.0f;

        /** FreeType size. */
        int m_pixel_size = 0;

        /** Bold face. */
        bool m_bold = false;

        /** Text align. */
        std::string m_align{"left"};

        /** Pending-color condition. */
        std::string m_pending_when;

        /** Accept tags. */
        std::vector<std::string> m_accept;

        /** Tab strip. */
        std::vector<UiTabItem> m_tabs;

        /** Alpha. */
        float m_alpha = 1.0f;

        /** Canvas kind. */
        std::string m_canvas_kind;
        /** Canvas clipping shape. */
        std::string m_shape = "rect";

        /** Tab on color. */
        std::string m_color_on;

        /** Tab off color. */
        std::string m_color_off;

        /** Hover button BMP. */
        std::string m_file_over;

        /** Pressed / selected button BMP. */
        std::string m_file_on;

        /** Disabled button BMP. */
        std::string m_file_off;

        /** Grid page expression. */
        std::string m_page;

        /** Widget defining the vertical viewport bounds. */
        std::string m_viewport;
        /** Catalog path containing the native vertical scroll offset. */
        std::string m_scroll_bind;
        /** Transient first cell in a clipped grid, never serialized. */
        int m_first_slot = 0;

        /** Grid filter expression. */
        std::string m_filter;

        /** Tooltip flag. */
        bool m_tooltip = false;

        /** Drag flag. */
        bool m_drag = false;

        /** Badge text template. */
        std::string m_badge_text;

        /** Badge visibility condition. */
        std::string m_badge_visible_when;

        /** Y from client bottom; negative means unused. */
        float m_x_from_right = -1.0f;
        float m_y_from_bottom = -1.0f;

        /** Border thickness. */
        float m_thickness = 0.0f;

        /** Top border BMP. */
        std::string m_border_top;

        /** Bottom border BMP. */
        std::string m_border_bottom;

        /** Left border BMP. */
        std::string m_border_left;

        /** Right border BMP. */
        std::string m_border_right;

        /** Grid selection color. */
        std::string m_select_color;

        /** List row height. */
        float m_line_height = 14.0f;

        /** Canvas center X. */
        float m_cx = 0.0f;

        /** Canvas center Y. */
        float m_cy = 0.0f;

        /** Level-orb inner radius. */
        float m_inner = 0.0f;

        /** Optional `_doc` note. */
        std::string m_doc_note;
    };

    /** 9-slice BMP names for one chrome preset. */
    class UiChromeFiles {
      public:
        /** @return Top-left file. */
        [[nodiscard]] const std::string& upleft() const {
            return m_upleft;
        }

        /** @param v File. */
        void set_upleft(std::string v) {
            m_upleft = std::move(v);
        }

        /** @return Top-center file. */
        [[nodiscard]] const std::string& upcenter() const {
            return m_upcenter;
        }

        /** @param v File. */
        void set_upcenter(std::string v) {
            m_upcenter = std::move(v);
        }

        /** @return Top-right file. */
        [[nodiscard]] const std::string& upright() const {
            return m_upright;
        }

        /** @param v File. */
        void set_upright(std::string v) {
            m_upright = std::move(v);
        }

        /** @return Left rail file. */
        [[nodiscard]] const std::string& left() const {
            return m_left;
        }

        /** @param v File. */
        void set_left(std::string v) {
            m_left = std::move(v);
        }

        /** @return Right rail file. */
        [[nodiscard]] const std::string& right() const {
            return m_right;
        }

        /** @param v File. */
        void set_right(std::string v) {
            m_right = std::move(v);
        }

        /** @return Bottom-left file. */
        [[nodiscard]] const std::string& downleft() const {
            return m_downleft;
        }

        /** @param v File. */
        void set_downleft(std::string v) {
            m_downleft = std::move(v);
        }

        /** @return Bottom-center file. */
        [[nodiscard]] const std::string& downcenter() const {
            return m_downcenter;
        }

        /** @param v File. */
        void set_downcenter(std::string v) {
            m_downcenter = std::move(v);
        }

        /** @return Bottom-right file. */
        [[nodiscard]] const std::string& downright() const {
            return m_downright;
        }

        /** @param v File. */
        void set_downright(std::string v) {
            m_downright = std::move(v);
        }

        /** @return Close glyph file. */
        [[nodiscard]] const std::string& exit_file() const {
            return m_exit;
        }

        /** @param v File. */
        void set_exit_file(std::string v) {
            m_exit = std::move(v);
        }

      private:
        /** Top-left. */
        std::string m_upleft{"info-menu-top left.bmp"};

        /** Top-center. */
        std::string m_upcenter{"info-menu-top center.bmp"};

        /** Top-right. */
        std::string m_upright{"info-menu-top right.bmp"};

        /** Left rail. */
        std::string m_left{"info-menu-left.bmp"};

        /** Right rail. */
        std::string m_right{"info-menu-right.bmp"};

        /** Bottom-left. */
        std::string m_downleft{"info-menu-bottom left.bmp"};

        /** Bottom-center. */
        std::string m_downcenter{"info-menu-bottom center.bmp"};

        /** Bottom-right. */
        std::string m_downright{"info-menu-bottom right.bmp"};

        /** Close glyph. */
        std::string m_exit{"info-exit small.bmp"};
    };

    /** Parsed openkal.ui/v1 document. */
    class UiDocument {
      public:
        /** @return True when schema is openkal.ui/v1. */
        [[nodiscard]] bool is_v1() const {
            return m_schema == "openkal.ui/v1";
        }

        /** @return Schema string. */
        [[nodiscard]] const std::string& schema() const {
            return m_schema;
        }

        /** @return Layout id. */
        [[nodiscard]] const std::string& id() const {
            return m_id;
        }

        /** @param v Layout id. */
        void set_id(std::string v) {
            m_id = std::move(v);
        }

        /** @return Document kind. */
        [[nodiscard]] UiKind kind() const {
            return m_kind;
        }

        /** @param v Kind. */
        void set_kind(UiKind v) {
            m_kind = v;
        }

        /** @return Client width. */
        [[nodiscard]] float window_w() const {
            return m_window_w;
        }

        /** @param v Width. */
        void set_window_w(float v) {
            m_window_w = v;
        }

        /** @return Client height. */
        [[nodiscard]] float window_h() const {
            return m_window_h;
        }

        /** @param v Height. */
        void set_window_h(float v) {
            m_window_h = v;
        }

        /** @return True when the window may be dragged. */
        [[nodiscard]] bool draggable() const {
            return m_draggable;
        }

        /** @param v Drag flag. */
        void set_draggable(bool v) {
            m_draggable = v;
        }

        /** @return Title-bar drag height. */
        [[nodiscard]] float title_drag_h() const {
            return m_title_drag_h;
        }

        /** @param v Height. */
        void set_title_drag_h(float v) {
            m_title_drag_h = v;
        }

        /** @return Theme FreeType size. */
        [[nodiscard]] int font_pixel_size() const {
            return m_font_pixel_size;
        }

        /** @param v Size. */
        void set_font_pixel_size(int v) {
            m_font_pixel_size = v;
        }

        /** @return Theme label color. */
        [[nodiscard]] const std::string& color_label() const {
            return m_color_label;
        }

        /** @param v Color. */
        void set_color_label(std::string v) {
            m_color_label = std::move(v);
        }

        /** @return Theme value color. */
        [[nodiscard]] const std::string& color_value() const {
            return m_color_value;
        }

        /** @param v Color. */
        void set_color_value(std::string v) {
            m_color_value = std::move(v);
        }

        /** @return Theme pending color. */
        [[nodiscard]] const std::string& color_pending() const {
            return m_color_pending;
        }

        /** @param v Color. */
        void set_color_pending(std::string v) {
            m_color_pending = std::move(v);
        }

        /** @return Theme title color. */
        [[nodiscard]] const std::string& color_title() const {
            return m_color_title;
        }

        /** @param v Color. */
        void set_color_title(std::string v) {
            m_color_title = std::move(v);
        }

        /** @return Chrome kind. */
        [[nodiscard]] UiChromeKind chrome_kind() const {
            return m_chrome_kind;
        }

        /** @param v Kind. */
        void set_chrome_kind(UiChromeKind v) {
            m_chrome_kind = v;
        }

        /** @return Chrome preset name. */
        [[nodiscard]] const std::string& chrome_preset() const {
            return m_chrome_preset;
        }

        /** @param v Preset. */
        void set_chrome_preset(std::string v) {
            m_chrome_preset = std::move(v);
        }

        /** @return Edge mode. */
        [[nodiscard]] UiChromeEdges chrome_edges() const {
            return m_chrome_edges;
        }

        /** @param v Edges. */
        void set_chrome_edges(UiChromeEdges v) {
            m_chrome_edges = v;
        }

        /** @return Pad left. */
        [[nodiscard]] float pad_left() const {
            return m_pad_left;
        }

        /** @param v Pad. */
        void set_pad_left(float v) {
            m_pad_left = v;
        }

        /** @return Pad top. */
        [[nodiscard]] float pad_top() const {
            return m_pad_top;
        }

        /** @param v Pad. */
        void set_pad_top(float v) {
            m_pad_top = v;
        }

        /** @return Pad right. */
        [[nodiscard]] float pad_right() const {
            return m_pad_right;
        }

        /** @param v Pad. */
        void set_pad_right(float v) {
            m_pad_right = v;
        }

        /** @return Pad bottom. */
        [[nodiscard]] float pad_bottom() const {
            return m_pad_bottom;
        }

        /** @param v Pad. */
        void set_pad_bottom(float v) {
            m_pad_bottom = v;
        }

        /** @return HUD bitmap file. */
        [[nodiscard]] const std::string& chrome_file() const {
            return m_chrome_file;
        }

        /** @param v File. */
        void set_chrome_file(std::string v) {
            m_chrome_file = std::move(v);
        }

        /** @return HUD fallback width when the BMP is missing. */
        [[nodiscard]] float fallback_w() const {
            return m_fallback_w;
        }

        /** @param v Width. */
        void set_fallback_w(float v) {
            m_fallback_w = v;
        }

        /** @return HUD fallback height when the BMP is missing. */
        [[nodiscard]] float fallback_h() const {
            return m_fallback_h;
        }

        /** @param v Height. */
        void set_fallback_h(float v) {
            m_fallback_h = v;
        }

        /** @return Close ox. */
        [[nodiscard]] float close_ox() const {
            return m_close_ox;
        }

        /** @param v Offset. */
        void set_close_ox(float v) {
            m_close_override = true;
            m_close_ox = v;
        }

        /** @return Close oy. */
        [[nodiscard]] float close_oy() const {
            return m_close_oy;
        }

        /** @param v Offset. */
        void set_close_oy(float v) {
            m_close_override = true;
            m_close_oy = v;
        }

        /** @return Close width. */
        [[nodiscard]] float close_w() const {
            return m_close_w;
        }

        /** @param v Width. */
        void set_close_w(float v) {
            m_close_override = true;
            m_close_w = v;
        }

        /** @return Close height. */
        [[nodiscard]] float close_h() const {
            return m_close_h;
        }

        /** @param v Height. */
        void set_close_h(float v) {
            m_close_override = true;
            m_close_h = v;
        }

        /** @return True when JSON explicitly supplies close-box coordinates. */
        [[nodiscard]] bool close_override() const {
            return m_close_override;
        }

        /** @return True when the frame uses the top-right close variant. */
        [[nodiscard]] bool chrome_close_button() const {
            return m_chrome_close_button;
        }

        /** @param enabled True to use the frame's close-button variant. */
        void set_chrome_close_button(bool enabled) {
            m_chrome_close_button = enabled;
        }

        /** @return Resolved 9-slice files. */
        [[nodiscard]] const UiChromeFiles& chrome_files() const {
            return m_chrome_files;
        }

        /** @param files Resolved 9-slice files. */
        void set_chrome_files(UiChromeFiles files) {
            m_chrome_files = std::move(files);
        }

        /** @return Optional `_doc` note. */
        [[nodiscard]] const std::string& doc_note() const {
            return m_doc_note;
        }

        /** @param v `_doc` note. */
        void set_doc_note(std::string v) {
            m_doc_note = std::move(v);
        }

        /** @return Widgets in draw order. */
        [[nodiscard]] const std::vector<UiWidget>& widgets() const {
            return m_widgets;
        }

        /** @return Mutable widgets. */
        [[nodiscard]] std::vector<UiWidget>& widgets() {
            return m_widgets;
        }

        /**
         * Parses a JSON object as openkal.ui/v1.
         * @param root Document root.
         * @return True when schema is v1 and the object parsed.
         */
        [[nodiscard]] bool load(const platform::Json& root);

        /**
         * Serializes this document to JSON.
         * @return Root object.
         */
        [[nodiscard]] platform::Json to_json() const;

        /**
         * Looks up a widget by id.
         * @param id Widget id.
         * @return Widget, or nullptr.
         */
        [[nodiscard]] const UiWidget* find_widget(std::string_view id) const;

        /**
         * Looks up a widget by id.
         * @param id Widget id.
         * @return Widget, or nullptr.
         */
        [[nodiscard]] UiWidget* find_widget(std::string_view id);

      private:
        /** Schema string. */
        std::string m_schema;

        /** Layout id. */
        std::string m_id;

        /** Kind. */
        UiKind m_kind = UiKind::Window;

        /** Client width. */
        float m_window_w = 245.0f;

        /** Client height. */
        float m_window_h = 417.0f;

        /** Drag flag. */
        bool m_draggable = true;

        /** Title drag height. */
        float m_title_drag_h = 44.0f;

        /** Theme font size. */
        int m_font_pixel_size = 10;

        /** Label color. */
        std::string m_color_label{"#E8DCC0"};

        /** Value color. */
        std::string m_color_value{"#FFFAEB"};

        /** Pending color. */
        std::string m_color_pending{"#C8E090"};

        /** Title color. */
        std::string m_color_title{"#F2F0C0"};

        /** Chrome kind. */
        UiChromeKind m_chrome_kind = UiChromeKind::NineSlice;

        /** Preset name. */
        std::string m_chrome_preset{"info_menu"};

        /** Edge mode. */
        UiChromeEdges m_chrome_edges = UiChromeEdges::Tile;

        /** Pad left. */
        float m_pad_left = 10.0f;

        /** Pad top. */
        float m_pad_top = 40.0f;

        /** Pad right. */
        float m_pad_right = 7.0f;

        /** Pad bottom. */
        float m_pad_bottom = 15.0f;

        /** True when the frame's top-right corner contains a close button. */
        bool m_chrome_close_button = true;

        /** HUD bitmap. */
        std::string m_chrome_file;

        /** HUD fallback width. */
        float m_fallback_w = 0.0f;

        /** HUD fallback height. */
        float m_fallback_h = 0.0f;

        /** Close ox. */
        float m_close_ox = -25.0f;

        /** Close oy. */
        float m_close_oy = -32.0f;

        /** Close w. */
        float m_close_w = 22.0f;

        /** Close h. */
        float m_close_h = 22.0f;

        /** True when a document explicitly overrides the standard close hitbox. */
        bool m_close_override = false;

        /** Resolved files. */
        UiChromeFiles m_chrome_files;

        /** Optional `_doc` note. */
        std::string m_doc_note;

        /** Widgets. */
        std::vector<UiWidget> m_widgets;
    };

    /** String catalog used by ${group.key} interpolation. */
    using UiVarMap = std::unordered_map<std::string, std::string>;

    /** A colored byte range in interpolated UTF-8 text. */
    struct UiTextColorRun {
        /** First byte in the displayed string. */
        std::size_t start = 0;
        /** Number of bytes receiving this color. */
        std::size_t length = 0;
        /** RGB color in #RRGGBB notation. */
        std::string color;
    };

    /**
     * Resolves a named 9-slice preset.
     * @param preset Preset id.
     * @return File names.
     */
    [[nodiscard]] UiChromeFiles ui_chrome_preset_files(std::string_view preset);

    /**
     * Applies a chrome preset, including typical padding.
     * @param doc Document to update.
     * @param preset Preset id.
     */
    void ui_apply_chrome_preset(UiDocument& doc, std::string_view preset);

    /**
     * @param t Widget type.
     * @return JSON type name.
     */
    [[nodiscard]] const char* ui_widget_type_name(UiWidgetType t);

    /**
     * @param name JSON type name.
     * @return Widget type.
     */
    [[nodiscard]] UiWidgetType ui_widget_type_from_name(std::string_view name);

    /**
     * Replaces ${group.key} and ${group.key:suffix} in a template.
     * @param templ Source text.
     * @param vars Catalog.
     * @param colors Optional output, cleared and filled with :geons amount colors.
     * @return Interpolated string.
     */
    [[nodiscard]] std::string ui_interpolate(std::string_view templ,
                                             const UiVarMap& vars,
                                             std::vector<UiTextColorRun>* colors = nullptr);

    /**
     * Evaluates visible_when / pending_when.
     * @param expr Condition.
     * @param vars Catalog.
     * @return True when the widget should use the true branch.
     */
    [[nodiscard]] bool ui_eval_condition(std::string_view expr, const UiVarMap& vars);

    /**
     * Parses #RRGGBB into 0..1 RGB.
     * @param text Color text after interpolation.
     * @param r Out red.
     * @param g Out green.
     * @param b Out blue.
     * @return True when parsed.
     */
    [[nodiscard]] bool ui_parse_hex_color(std::string_view text, float& r, float& g, float& b);

    /**
     * Fills theme.* keys from a document.
     * @param doc Document.
     * @param vars Destination map.
     */
    void ui_fill_theme_vars(const UiDocument& doc, UiVarMap& vars);

    /**
     * Fills dummy preview values for the Qt editor.
     * @param vars Destination map (theme keys should already be set).
     */
    void ui_fill_dummy_preview(UiVarMap& vars);

    /**
     * Parses `group.key` rows from VARIABLES.md.
     * @param markdown File text.
     * @return Catalog keys in document order.
     */
    [[nodiscard]] std::vector<std::string> ui_parse_variable_keys(std::string_view markdown);

    /**
     * Maps stats.str … stats.dex binds to pending indices 0..4.
     * @param bind Bind path.
     * @return Index, or -1.
     */
    [[nodiscard]] int ui_primary_index(std::string_view bind);

    /** Destination rect for one 9-slice piece. */
    class UiChromeRect {
      public:
        UiChromeRect() = default;

        /**
         * @param x Left.
         * @param y Top.
         * @param w Width.
         * @param h Height.
         */
        UiChromeRect(float x, float y, float w, float h) : m_x(x), m_y(y), m_w(w), m_h(h) {}

        /** @return Left. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Top. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return Width. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @return Height. */
        [[nodiscard]] float h() const {
            return m_h;
        }

      private:
        /** Left. */
        float m_x = 0.0f;

        /** Top. */
        float m_y = 0.0f;

        /** Width. */
        float m_w = 0.0f;

        /** Height. */
        float m_h = 0.0f;
    };

    /** Computed 9-slice destinations in screen pixels. */
    class UiNineSliceLayout {
      public:
        /** @return Outer frame rect. */
        [[nodiscard]] const UiChromeRect& outer() const {
            return m_outer;
        }

        /** @param v Outer frame. */
        void set_outer(UiChromeRect v) {
            m_outer = v;
        }

        /** @return Top-left corner. */
        [[nodiscard]] const UiChromeRect& tl() const {
            return m_tl;
        }

        /** @param v Top-left. */
        void set_tl(UiChromeRect v) {
            m_tl = v;
        }

        /** @return Top-right corner. */
        [[nodiscard]] const UiChromeRect& tr() const {
            return m_tr;
        }

        /** @param v Top-right. */
        void set_tr(UiChromeRect v) {
            m_tr = v;
        }

        /** @return Bottom-left corner. */
        [[nodiscard]] const UiChromeRect& bl() const {
            return m_bl;
        }

        /** @param v Bottom-left. */
        void set_bl(UiChromeRect v) {
            m_bl = v;
        }

        /** @return Bottom-right corner. */
        [[nodiscard]] const UiChromeRect& br() const {
            return m_br;
        }

        /** @param v Bottom-right. */
        void set_br(UiChromeRect v) {
            m_br = v;
        }

        /** @return Top rail span. */
        [[nodiscard]] const UiChromeRect& top() const {
            return m_top;
        }

        /** @param v Top rail. */
        void set_top(UiChromeRect v) {
            m_top = v;
        }

        /** @return Bottom rail span. */
        [[nodiscard]] const UiChromeRect& bot() const {
            return m_bot;
        }

        /** @param v Bottom rail. */
        void set_bot(UiChromeRect v) {
            m_bot = v;
        }

        /** @return Left rail span. */
        [[nodiscard]] const UiChromeRect& left() const {
            return m_left;
        }

        /** @param v Left rail. */
        void set_left(UiChromeRect v) {
            m_left = v;
        }

        /** @return Right rail span. */
        [[nodiscard]] const UiChromeRect& right() const {
            return m_right;
        }

        /** @param v Right rail. */
        void set_right(UiChromeRect v) {
            m_right = v;
        }

        /** @return Native top-center tile width (scaled). */
        [[nodiscard]] float top_tile_w() const {
            return m_top_tile_w;
        }

        /** @param v Tile width. */
        void set_top_tile_w(float v) {
            m_top_tile_w = v;
        }

        /** @return Native bottom-center tile width (scaled). */
        [[nodiscard]] float bot_tile_w() const {
            return m_bot_tile_w;
        }

        /** @param v Tile width. */
        void set_bot_tile_w(float v) {
            m_bot_tile_w = v;
        }

        /** @return Native left-rail tile height (scaled). */
        [[nodiscard]] float left_tile_h() const {
            return m_left_tile_h;
        }

        /** @param v Tile height. */
        void set_left_tile_h(float v) {
            m_left_tile_h = v;
        }

        /** @return Native right-rail tile height (scaled). */
        [[nodiscard]] float right_tile_h() const {
            return m_right_tile_h;
        }

        /** @param v Tile height. */
        void set_right_tile_h(float v) {
            m_right_tile_h = v;
        }

      private:
        /** Outer frame. */
        UiChromeRect m_outer;

        /** Top-left. */
        UiChromeRect m_tl;

        /** Top-right. */
        UiChromeRect m_tr;

        /** Bottom-left. */
        UiChromeRect m_bl;

        /** Bottom-right. */
        UiChromeRect m_br;

        /** Top rail. */
        UiChromeRect m_top;

        /** Bottom rail. */
        UiChromeRect m_bot;

        /** Left rail. */
        UiChromeRect m_left;

        /** Right rail. */
        UiChromeRect m_right;

        /** Top tile width. */
        float m_top_tile_w = 0.0f;

        /** Bottom tile width. */
        float m_bot_tile_w = 0.0f;

        /** Left tile height. */
        float m_left_tile_h = 0.0f;

        /** Right tile height. */
        float m_right_tile_h = 0.0f;
    };

    /**
     * Lays out a 9-slice around a client box.
     *
     * Stretch mode (`tile_edges` false) pins rails to the outer pad rect (Skill /
     * Transform). Tile mode hugs the client box with a 5 px overlap so renewal
     * Info / Inventory corners (tall medallions, wide bottom flourishes) meet the
     * rails the way the stacked pieces did.
     *
     * Native piece sizes are unscaled pixels; the result is in screen pixels.
     *
     * @param client_x Client left.
     * @param client_y Client top.
     * @param client_w Client width.
     * @param client_h Client height.
     * @param scale Display scale.
     * @param pad_l Pad left.
     * @param pad_t Pad top.
     * @param pad_r Pad right.
     * @param pad_b Pad bottom.
     * @param tl_nw Top-left native width.
     * @param tl_nh Top-left native height.
     * @param tc_nw Top-center native width.
     * @param tc_nh Top-center native height.
     * @param tr_nw Top-right native width.
     * @param tr_nh Top-right native height.
     * @param l_nw Left native width.
     * @param l_nh Left native height.
     * @param r_nw Right native width.
     * @param r_nh Right native height.
     * @param bl_nw Bottom-left native width.
     * @param bl_nh Bottom-left native height.
     * @param bc_nw Bottom-center native width.
     * @param bc_nh Bottom-center native height.
     * @param br_nw Bottom-right native width.
     * @param br_nh Bottom-right native height.
     * @param tile_edges True for Info-style tiling.
     * @return Screen-space piece rects.
     */
    [[nodiscard]] UiNineSliceLayout ui_compute_nine_slice(float client_x,
                                                          float client_y,
                                                          float client_w,
                                                          float client_h,
                                                          float scale,
                                                          float pad_l,
                                                          float pad_t,
                                                          float pad_r,
                                                          float pad_b,
                                                          float tl_nw,
                                                          float tl_nh,
                                                          float tc_nw,
                                                          float tc_nh,
                                                          float tr_nw,
                                                          float tr_nh,
                                                          float l_nw,
                                                          float l_nh,
                                                          float r_nw,
                                                          float r_nh,
                                                          float bl_nw,
                                                          float bl_nh,
                                                          float bc_nw,
                                                          float bc_nh,
                                                          float br_nw,
                                                          float br_nh,
                                                          bool tile_edges);

    /** Resolves vertical scrolling and clips to complete widgets or complete icon-grid rows.
     * @param widget Source widget, whose coordinates remain unchanged.
     * @param document Document containing the viewport widget.
     * @param vars Catalog supplying the scroll offset in native pixels.
     * @return Display widget, hidden when no complete content fits the viewport.
     */
    [[nodiscard]] UiWidget
    ui_scrolled_widget(const UiWidget& widget, const UiDocument& document, const UiVarMap& vars);

}  // namespace openkal::client
