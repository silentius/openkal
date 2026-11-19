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
#include "openkal/client/UiDocument.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    class UiLayout;

    /** Resolved icon for one icon_slot / icon_grid cell. */
    class UiSlotIcon {
      public:
        /** @return Icon BMP, or empty. */
        [[nodiscard]] const std::string& file() const {
            return m_file;
        }

        /** @param v File. */
        void set_file(std::string v) {
            m_file = std::move(v);
        }

        /** @return Stack count. */
        [[nodiscard]] int count() const {
            return m_count;
        }

        /** @param v Count. */
        void set_count(int v) {
            m_count = v;
        }

        /** @return True to use the compact black count with a white outline. */
        [[nodiscard]] bool high_contrast_count() const {
            return m_high_contrast_count;
        }

        /** Selects the outlined inventory count. @param value True for high contrast. */
        void set_high_contrast_count(bool value) {
            m_high_contrast_count = value;
        }

        /** @return Skill rank. */
        [[nodiscard]] int level() const {
            return m_level;
        }

        /** @param v Level. */
        void set_level(int v) {
            m_level = v;
        }

        /** @return True when the cell has no icon. */
        [[nodiscard]] bool empty() const {
            return m_empty;
        }

        /** @param v Empty flag. */
        void set_empty(bool v) {
            m_empty = v;
        }

        /** @return True when this bag cell is the worn instance. */
        [[nodiscard]] bool equipped() const {
            return m_equipped;
        }

        /** @param v Equipped flag. */
        void set_equipped(bool v) {
            m_equipped = v;
        }

        /** @return True when the item is permanently account-bound. */
        [[nodiscard]] bool account_bound() const {
            return m_account_bound;
        }

        /** @param v Account-bound flag. */
        void set_account_bound(bool v) {
            m_account_bound = v;
        }

        /** @return True when the item is permanently character-bound. */
        [[nodiscard]] bool character_bound() const {
            return m_character_bound;
        }

        /** Sets character binding. @param value True to show locks on both sides. */
        void set_character_bound(bool value) {
            m_character_bound = value;
        }

        /** @return True while one protected enchantment attempt remains. */
        bool enchant_protected() const {
            return m_enchant_protected;
        }

        /** Sets the protection trail. @param value True to display protection. */
        void set_enchant_protected(bool value) {
            m_enchant_protected = value;
        }

        /** @return True when the cell is selected. */
        [[nodiscard]] bool selected() const {
            return m_selected;
        }

        /** @param v Selected flag. */
        void set_selected(bool v) {
            m_selected = v;
        }

        /** @return True when the icon is rendered without color. */
        bool grayscale() const {
            return m_grayscale;
        }

        /** Sets the icon's color presentation. @param v True to use grayscale. */
        void set_grayscale(bool v) {
            m_grayscale = v;
        }

        /** @return Remaining fraction of the item-use cooldown. */
        [[nodiscard]] float cooldown_fraction() const {
            return m_cooldown_fraction;
        }

        /** @param fraction Remaining fraction from 0 (ready) to 1 (just started). */
        void set_cooldown_fraction(float fraction) {
            m_cooldown_fraction = fraction;
        }

      private:
        /** Icon BMP. */
        std::string m_file;

        /** Stack count. */
        int m_count = 0;

        /** Uses the outlined inventory stack-count style. */
        bool m_high_contrast_count = false;

        /** Skill rank. */
        int m_level = 0;

        /** Empty flag. */
        bool m_empty = true;

        /** Worn-instance flag. */
        bool m_equipped = false;

        /** Permanent account-bound flag. */
        bool m_account_bound = false;

        /** Permanent character-bound flag. */
        bool m_character_bound = false;

        /** One remaining protected enchantment attempt. */
        bool m_enchant_protected = false;

        /** Selection flag. */
        bool m_selected = false;

        /** Grayscale presentation flag. */
        bool m_grayscale = false;

        /** Remaining fraction of the item-use cooldown. */
        float m_cooldown_fraction = 0.0f;
    };

    /** One row for a list widget. */
    class UiListRow {
      public:
        /** @return Display text. */
        [[nodiscard]] const std::string& text() const {
            return m_text;
        }

        /** @param v Text. */
        void set_text(std::string v) {
            m_text = std::move(v);
        }

        /** @return Optional action. */
        [[nodiscard]] const std::string& action() const {
            return m_action;
        }

        /** @param v Action. */
        void set_action(std::string v) {
            m_action = std::move(v);
        }

        /** @return Optional row id. */
        [[nodiscard]] const std::string& id() const {
            return m_id;
        }

        /** @param v Id. */
        void set_id(std::string v) {
            m_id = std::move(v);
        }

      private:
        /** Display text. */
        std::string m_text;

        /** Optional action. */
        std::string m_action;

        /** Optional row id. */
        std::string m_id;
    };

    /**
     * Domain callbacks for binds that UiView cannot interpolate from the var map
     * alone (icons, progress ratios, canvas, list rows).
     */
    class UiViewHost {
      public:
        virtual ~UiViewHost() = default;

        /**
         * Resolves one icon cell.
         * @param bind Bind path.
         * @param index Cell index (0 for icon_slot).
         * @param out Slot data.
         * @return True when the bind is known.
         */
        [[nodiscard]] virtual bool
        resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const {
            (void) bind;
            (void) index;
            (void) out;
            return false;
        }

        /**
         * @param bind Progress bind path.
         * @return Fill ratio 0..1.
         */
        [[nodiscard]] virtual float progress_ratio(std::string_view bind) const {
            (void) bind;
            return 0.0f;
        }

        /**
         * Draws a canvas widget.
         * @param engine Engine.
         * @param ui Helpers.
         * @param w Widget.
         * @param sx Screen X.
         * @param sy Screen Y.
         * @param sw Screen W.
         * @param sh Screen H.
         * @param scale UI scale.
         * @param vars Interpolation catalog.
         * @param font Body face.
         */
        virtual void draw_canvas(engine::IEngine& engine,
                                 WidgetTools& ui,
                                 const UiWidget& w,
                                 float sx,
                                 float sy,
                                 float sw,
                                 float sh,
                                 float scale,
                                 const UiVarMap& vars,
                                 const FreetypeFont& font) const {
            (void) engine;
            (void) ui;
            (void) w;
            (void) sx;
            (void) sy;
            (void) sw;
            (void) sh;
            (void) scale;
            (void) vars;
            (void) font;
        }

        /**
         * @param bind List bind path.
         * @param w Widget.
         * @param vars Catalog.
         * @return Rows to draw.
         */
        [[nodiscard]] virtual std::vector<UiListRow>
        list_rows(std::string_view bind, const UiWidget& w, const UiVarMap& vars) const {
            (void) bind;
            (void) w;
            (void) vars;
            return {};
        }

        /**
         * @param action Button action.
         * @return False to draw the button dimmed / off.
         */
        [[nodiscard]] virtual bool button_enabled(std::string_view action) const {
            (void) action;
            return true;
        }

        /** @param widget Button with its action and binding.
         * @return Whether the button should use its enabled appearance. */
        [[nodiscard]] virtual bool widget_enabled(const UiWidget& widget) const {
            return button_enabled(widget.action());
        }

        /** @param widget_id Button widget identifier.
         * @return Whether the pointer hovers over this button. */
        [[nodiscard]] virtual bool button_hovered(std::string_view widget_id) const {
            return !widget_id.empty() && widget_id == m_pointer_widget;
        }

        /** @param widget_id Button widget identifier.
         * @return Whether this button is pressed. */
        [[nodiscard]] virtual bool button_pressed(std::string_view widget_id) const {
            return m_pointer_pressed && button_hovered(widget_id);
        }

        /** Updates button feedback from the document's current pointer hit.
         * @param widget_id Hovered widget identifier, or empty when nothing is hit.
         * @param pressed Whether the primary pointer button is held.
         */
        void set_pointer_hit(std::string_view widget_id, bool pressed) {
            m_pointer_widget = widget_id;
            m_pointer_pressed = pressed;
        }

      private:
        std::string m_pointer_widget;
        bool m_pointer_pressed = false;
    };

    /** Kind of a pointer hit against a v1 document. */
    enum class UiHitKind {
        None = 0,
        Close,
        TitleDrag,
        Button,
        Tab,
        IconSlot,
        IconGridCell,
        ListRow,
        Input,
        Canvas,
        Consume,
    };

    /** Result of UiView::hit. */
    class UiHit {
      public:
        /** @return Hit kind. */
        [[nodiscard]] UiHitKind kind() const {
            return m_kind;
        }

        /** @param v Kind. */
        void set_kind(UiHitKind v) {
            m_kind = v;
        }

        /** @return Widget id. */
        [[nodiscard]] const std::string& widget_id() const {
            return m_widget_id;
        }

        /** @param v Id. */
        void set_widget_id(std::string v) {
            m_widget_id = std::move(v);
        }

        /** @return Button / row action. */
        [[nodiscard]] const std::string& action() const {
            return m_action;
        }

        /** @param v Action. */
        void set_action(std::string v) {
            m_action = std::move(v);
        }

        /** @return Bind path. */
        [[nodiscard]] const std::string& bind() const {
            return m_bind;
        }

        /** @param v Bind. */
        void set_bind(std::string v) {
            m_bind = std::move(v);
        }

        /** @return Tab id when kind is Tab. */
        [[nodiscard]] const std::string& tab_id() const {
            return m_tab_id;
        }

        /** @param v Tab id. */
        void set_tab_id(std::string v) {
            m_tab_id = std::move(v);
        }

        /** @return Grid / list index, or -1. */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /** @param v Index. */
        void set_index(int v) {
            m_index = v;
        }

      private:
        /** Hit kind. */
        UiHitKind m_kind = UiHitKind::None;

        /** Widget id. */
        std::string m_widget_id;

        /** Action. */
        std::string m_action;

        /** Bind. */
        std::string m_bind;

        /** Tab id. */
        std::string m_tab_id;

        /** Cell / row index. */
        int m_index = -1;
    };

    /** Shared draw / hit helpers for openkal.ui/v1 documents. */
    class UiView {
      public:
        /** Draws the binding frame with one account lock or two character locks.
         * @param ui UI assets and renderer.
         * @param x Left edge.
         * @param y Top edge.
         * @param w Icon width.
         * @param h Icon height.
         * @param account_bound True to show the right-hand lock.
         * @param character_bound True to show both locks; takes precedence.
         */
        static void draw_item_binding(WidgetTools& ui,
                                      float x,
                                      float y,
                                      float w,
                                      float h,
                                      bool account_bound,
                                      bool character_bound);

        /** Draws a full diagonal light band sweeping from the icon's top left to bottom right.
         * @param engine UI renderer.
         * @param x Left edge.
         * @param y Top edge.
         * @param w Icon width.
         * @param h Icon height.
         * @param seconds Monotonic animation time; one sweep repeats every two seconds.
         */
        static void draw_enchant_protection(
            engine::IEngine& engine, float x, float y, float w, float h, double seconds);

        /** Selects which part of a document's widgets is rendered. */
        enum class WidgetDrawPass {
            /** Draw every widget in JSON order. */
            All,
            /** Draw every widget except the reserved chrome overlays. */
            Content,
            /** Draw only the reserved chrome overlays (title, emblem, handle and close). */
            ChromeTitle
        };

        /**
         * Builds the 9-slice style from a v1 document.
         * @param doc Document.
         * @return Frame style.
         */
        [[nodiscard]] static WindowFrameStyle frame_style(const UiDocument& doc);

        /** Returns the cached v1 document until the layout is reloaded or destroyed.
         * @param layout Layout, or nullptr for an empty document.
         * @return Immutable parsed document shared by readers of this layout.
         */
        [[nodiscard]] static const UiDocument& document(const UiLayout* layout);

        /** Copies the cached v1 document from a bound layout root.
         * @param doc Destination.
         * @param layout Layout, or nullptr.
         */
        static void reload(UiDocument& doc, const UiLayout* layout);

        /**
         * Screen rect for a widget. Resolves native BMP size when w/h are 0.
         * @param w Widget.
         * @param ui Optional helpers for texture size.
         * @param client_x Client box left.
         * @param client_y Client box top.
         * @param client_w Client box width.
         * @param client_h Client box height.
         * @param scale UI scale.
         * @param sx Out X.
         * @param sy Out Y.
         * @param sw Out W.
         * @param sh Out H.
         */
        static void widget_rect(const UiWidget& w,
                                WidgetTools* ui,
                                float client_x,
                                float client_y,
                                float client_w,
                                float client_h,
                                float scale,
                                float& sx,
                                float& sy,
                                float& sw,
                                float& sh);

        /**
         * Draws chrome (9-slice, HUD bitmap, or nothing) plus close glyph.
         * @param ui Helpers.
         * @param doc Document.
         * @param client_x Client left.
         * @param client_y Client top.
         * @param client_w Client width.
         * @param client_h Client height.
         * @param scale UI scale.
         * @param fill_client True to paint a dark client rect before the frame.
         */
        static void draw_chrome(WidgetTools& ui,
                                const UiDocument& doc,
                                float client_x,
                                float client_y,
                                float client_w,
                                float client_h,
                                float scale,
                                bool fill_client = true);

        /**
         * Draws widgets selected by a rendering pass.
         * @param engine Engine.
         * @param ui Helpers.
         * @param doc Document.
         * @param client_x Client left.
         * @param client_y Client top.
         * @param client_w Client width.
         * @param client_h Client height.
         * @param scale UI scale.
         * @param vars Interpolation catalog.
         * @param body Body face.
         * @param title Title face (falls back to body).
         * @param host Optional bind host.
         * @param pass Rendering pass. `Content` / `ChromeTitle` split the text
         * widgets `title` and `dialog_icon`, and Dragon close controls, around a foreground frame.
         */
        static void draw_widgets(engine::IEngine& engine,
                                 WidgetTools& ui,
                                 const UiDocument& doc,
                                 float client_x,
                                 float client_y,
                                 float client_w,
                                 float client_h,
                                 float scale,
                                 const UiVarMap& vars,
                                 const FreetypeFont& body,
                                 const FreetypeFont& title,
                                 const UiViewHost* host = nullptr,
                                 WidgetDrawPass pass = WidgetDrawPass::All);

        /**
         * Draws content, then chrome, then the reserved chrome overlays. This keeps
         * frame edges above all client content.
         * @param engine Engine.
         * @param ui Helpers.
         * @param doc Document.
         * @param client_x Client left.
         * @param client_y Client top.
         * @param client_w Client width.
         * @param client_h Client height.
         * @param scale UI scale.
         * @param vars Interpolation catalog.
         * @param body Body face.
         * @param title Title face.
         * @param host Optional bind host.
         * @param fill_client Dark fill under the client box.
         */
        static void draw(engine::IEngine& engine,
                         WidgetTools& ui,
                         const UiDocument& doc,
                         float client_x,
                         float client_y,
                         float client_w,
                         float client_h,
                         float scale,
                         const UiVarMap& vars,
                         const FreetypeFont& body,
                         const FreetypeFont& title,
                         const UiViewHost* host = nullptr,
                         bool fill_client = true);

        /**
         * Hit-tests close, widgets (front to back), then the title bar / client.
         * @param doc Document.
         * @param ui Optional helpers for native BMP sizes.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param client_x Client left.
         * @param client_y Client top.
         * @param client_w Client width.
         * @param client_h Client height.
         * @param scale UI scale.
         * @param vars Catalog for visible_when.
         * @param host Optional bind host (grid cell counts).
         * @param framed True to include 9-slice close / title / outer hit.
         * @return Hit; kind None when the pointer missed.
         */
        [[nodiscard]] static UiHit hit(const UiDocument& doc,
                                       WidgetTools* ui,
                                       float mx,
                                       float my,
                                       float client_x,
                                       float client_y,
                                       float client_w,
                                       float client_h,
                                       float scale,
                                       const UiVarMap& vars,
                                       const UiViewHost* host = nullptr,
                                       bool framed = true);
    };

}  // namespace openkal::client
