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

#include "openkal/client/AnimatedActor.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/Inventory.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/Shop.h"

#include <map>
#include <string>
#include <vector>

namespace openkal::client {
    /** Cosmetic catalog family selected in the NPC conversation or window tabs. */
    enum class SkinShopCategory { Weapons, FullBody, Parts };

    /** Result of a pointer action in the cosmetic exchange. */
    enum class SkinShopClickResult { Ignored, Consumed, Close, Redeem };

    /** One vertically arranged catalog block. */
    struct SkinShopGroup {
        std::string label;
        std::vector<protocol::ShopOffer> offers;
    };

    /** Ticket exchange with a local character mirror and cached model thumbnails. */
    class SkinShopWindow final : private UiViewHost {
      public:
        /** @return True while this window is visible. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** Closes the window without changing player equipment. */
        void close();

        /** Releases preview GPU resources.
         * @param engine Engine owning the resources.
         */
        void destroy(engine::IEngine& engine);

        /** Binds the universal UI document.
         * @param layout Layout, which must outlive the window.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** Updates layout geometry. */
        void sync_from_layout();

        /** Sets UI scale.
         * @param scale Scale factor.
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /** Centers and fits the window to the screen.
         * @param width Screen width.
         * @param height Screen height.
         */
        void center_on_screen(float width, float height);

        /** Opens authoritative offers filtered to the player's class.
         * @param list Server offers and ticket currency.
         * @param category Initial category from the NPC conversation.
         * @param clazz Player class.
         * @param catalog Loaded item definitions.
         */
        void show(const protocol::ShopItemList& list,
                  SkinShopCategory category,
                  Class clazz,
                  const ItemDb& catalog);

        /** @return Currently visible catalog blocks in display order. */
        [[nodiscard]] const std::vector<SkinShopGroup>& groups() const {
            return m_groups;
        }

        /** @return Runtime NPC id for a redemption. */
        [[nodiscard]] std::uint32_t npc_id() const {
            return m_npc_id;
        }

        /** @return Selected skin definition, or zero. */
        [[nodiscard]] std::uint16_t selected_index() const {
            return m_selected.index;
        }

        /** @return True while the title is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /** Tests the frame and ornaments.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True inside the visible window.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /** Handles scrolling and title dragging.
         * @param input Current frame input.
         */
        void update_pointer(const engine::FrameInput& input);

        /** Handles catalog selection, rotation, category changes and redemption.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param bag Current bag used to enable redemption.
         * @param catalog Item definitions.
         * @return Pointer result; Redeem always means one selected skin.
         */
        [[nodiscard]] SkinShopClickResult
        handle_click(float mx, float my, const Bag& bag, const ItemDb& catalog);

        /** Builds a detached outfit for the preview, resolving conflicting skins.
         * @param worn Real player's current outfit; never changed.
         * @param skin Selected cosmetic definition.
         * @param catalog Item definitions used for compatible display weapons.
         * @return Preview-only equipment snapshot.
         */
        [[nodiscard]] static Wear
        preview_wear(const Wear& worn, const ItemDef& skin, const ItemDb& catalog);

        /** Draws the shop, thumbnails and the character mirror with TrueType text.
         * @param engine Rendering engine.
         * @param ui Widget helpers.
         * @param font TrueType body font.
         * @param bag Current bag.
         * @param catalog Item definitions.
         * @param worn Current real equipment.
         * @param data_root Optional local model root.
         * @param face Natural face.
         * @param hair Natural hair.
         * @param texture_cache Shared texture resources owned by the client session.
         * @param world_view Camera to restore after offscreen rendering.
         * @param world_projection Projection to restore after offscreen rendering.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const Bag& bag,
                  const ItemDb& catalog,
                  const Wear& worn,
                  const std::string& data_root,
                  int face,
                  int hair,
                  std::unordered_map<std::string, engine::TextureHandle>& texture_cache,
                  const engine::Mat4& world_view,
                  const engine::Mat4& world_projection);

      private:
        struct Card {
            protocol::ShopOffer offer;
            float x = 0, y = 0;
        };
        struct Block {
            std::string label;
            float y = 0;
        };
        void rebuild(const ItemDb& catalog);
        [[nodiscard]] float scale() const;
        [[nodiscard]] bool rect_hit(float mx, float my, float x, float y, float w, float h) const;
        [[nodiscard]] UiVarMap variables(const Bag& bag, const ItemDb* catalog = nullptr) const;
        [[nodiscard]] WindowFrameStyle frame_style() const;
        [[nodiscard]] bool button_enabled(std::string_view action) const override;
        void draw_canvas(engine::IEngine& engine,
                         WidgetTools& ui,
                         const UiWidget& widget,
                         float sx,
                         float sy,
                         float sw,
                         float sh,
                         float scale,
                         const UiVarMap& vars,
                         const FreetypeFont& font) const override;
        [[nodiscard]] float catalog_height() const;

        [[nodiscard]] engine::TextureHandle
        render_preview(engine::IEngine& engine,
                       const ItemDb& catalog,
                       const Wear& worn,
                       const ItemDef* skin,
                       const std::string& data_root,
                       int face,
                       int hair,
                       std::unordered_map<std::string, engine::TextureHandle>& texture_cache,
                       int width,
                       int height,
                       float yaw,
                       bool thumbnail);

        bool m_open = false, m_dragging = false;
        float m_x = 80, m_y = 80, m_w = 880, m_h = 620, m_scale = 1, m_fit = 1;
        float m_drag_dx = 0, m_drag_dy = 0, m_scroll = 0, m_content_height = 0, m_yaw = 0;
        float m_catalog_x = 310, m_catalog_y = 70, m_catalog_w = 550, m_catalog_h = 460;
        const ItemDb* m_catalog = nullptr;
        Class m_clazz = Class::Knight;
        SkinShopCategory m_category = SkinShopCategory::Weapons;
        std::uint32_t m_npc_id = 0;
        std::uint16_t m_currency = 0;
        protocol::ShopOffer m_selected;
        std::vector<protocol::ShopOffer> m_offers;
        std::vector<SkinShopGroup> m_groups;
        std::vector<Card> m_cards;
        std::vector<Block> m_blocks;
        const UiLayout* m_layout = nullptr;
        mutable UiDocument m_document;
        mutable bool m_can_redeem = false;
        std::map<int, engine::TextureHandle> m_thumbnails;
        engine::TextureHandle m_mirror{};
        std::string m_mirror_key;
    };
}  // namespace openkal::client
