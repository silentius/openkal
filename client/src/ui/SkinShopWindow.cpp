/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/client/SkinShopWindow.h"

#include "openkal/client/TextWrap.h"
#include "openkal/client/WeaponThumbnailCamera.h"
#include "openkal/engine/TransparentMesh.h"
#include "openkal/terrain/GbModel.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace openkal::client {
    namespace {
        struct PreviewPart {
            engine::MeshHandle mesh;
            engine::TextureHandle texture;
            bool alpha_blend = false, alpha_test = false, two_sided = false;
            bool specular = false;
            std::vector<engine::VertexP3T2> vertices;
            std::vector<std::uint32_t> indices;
        };
        struct PreviewParts {
            engine::IEngine& engine;
            std::vector<PreviewPart> parts;
            ~PreviewParts() {
                for (const auto& part : parts) {
                    engine.destroy_mesh(part.mesh);
                }
            }
        };
        engine::TextureHandle
        weapon_thumbnail(engine::IEngine& engine,
                         const std::string& path,
                         protocol::WeaponSkinType weapon_type,
                         std::unordered_map<std::string, engine::TextureHandle>& cache,
                         int width,
                         int height) {
            terrain::GbModel model;
            if (path.empty() || !terrain::load_gb(path, model)) {
                return {};
            }
            PreviewParts uploaded{engine, {}};
            engine::Vec3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
            for (const auto& mesh : model.meshes) {
                if (mesh.name.compare(0, 6, "effect") == 0 ||
                    mesh.name.compare(0, 6, "Effect") == 0 || mesh.pos_uv.empty() ||
                    mesh.indices.empty() || mesh.material_index < 0 ||
                    mesh.material_index >= static_cast<int>(model.materials.size())) {
                    continue;
                }
                const auto& texture_name = model.materials[mesh.material_index].texture;
                const auto key = path + "|" + texture_name;
                auto texture = cache.find(key);
                if (texture == cache.end()) {
                    terrain::ImageRgba pixels;
                    if (!terrain::load_gb_texture(path, texture_name, pixels)) {
                        continue;
                    }
                    texture =
                        cache
                            .emplace(key,
                                     engine.create_texture_rgba8(
                                         pixels.rgba.data(), pixels.width, pixels.height, true))
                            .first;
                }
                PreviewPart part;
                part.texture = texture->second;
                part.alpha_blend = mesh.alpha_blend;
                part.alpha_test = mesh.alpha_test;
                part.two_sided = mesh.two_sided;
                part.specular = mesh.specular_power > 0.0f;
                part.indices = mesh.indices;
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    const float x = mesh.pos_uv[i], y = mesh.pos_uv[i + 1], z = mesh.pos_uv[i + 2];
                    lo.x = std::min(lo.x, x);
                    lo.y = std::min(lo.y, y);
                    lo.z = std::min(lo.z, z);
                    hi.x = std::max(hi.x, x);
                    hi.y = std::max(hi.y, y);
                    hi.z = std::max(hi.z, z);
                    part.vertices.push_back({x, y, z, mesh.pos_uv[i + 3], mesh.pos_uv[i + 4]});
                }
                part.mesh = engine.create_mesh(part.vertices.data(),
                                               part.vertices.size(),
                                               part.indices.data(),
                                               part.indices.size());
                engine.set_mesh_specular(part.mesh, mesh.normals, mesh.specular_power);
                uploaded.parts.push_back(std::move(part));
            }
            if (uploaded.parts.empty() ||
                !engine.begin_offscreen(width, height, .035f, .045f, .045f)) {
                return {};
            }
            const auto camera =
                weapon_thumbnail_camera(lo, hi, weapon_type, static_cast<float>(width) / height);
            engine.set_camera(camera.view, camera.projection);
            for (int pass = 0; pass < 2; ++pass) {
                for (const auto& part : uploaded.parts) {
                    if (part.alpha_blend != (pass == 1)) {
                        continue;
                    }
                    if (part.alpha_blend && part.two_sided && !part.specular) {
                        const auto triangles = engine::build_transparent_mesh_batch(
                            part.vertices, part.indices, engine::Mat4::identity(), camera.view, 1);
                        engine.draw_sprite_batch(triangles, part.texture, true, false);
                    } else {
                        engine.draw_mesh_textured(part.mesh,
                                                  part.texture,
                                                  engine::Mat4::identity(),
                                                  part.alpha_blend,
                                                  part.two_sided,
                                                  part.alpha_test);
                    }
                }
            }
            std::vector<std::uint8_t> pixels;
            return engine.end_offscreen_rgba(pixels)
                       ? engine.create_texture_rgba8(pixels.data(), width, height, true)
                       : engine::TextureHandle{};
        }
        constexpr float kCatalogW = 550;
        constexpr float kCardW = 170, kCardH = 174, kPitch = 180;
        bool two_handed(const ItemDef& item) {
            return item.weapon_skin_type() == protocol::WeaponSkinType::Sword2h;
        }
        std::string group_label(const ItemDef& item, SkinShopCategory category, bool split) {
            if (category == SkinShopCategory::FullBody) {
                return "Full-body skins";
            }
            if (category == SkinShopCategory::Weapons) {
                return split ? (two_handed(item) ? "Two-handed weapons"
                                                 : "One-handed weapons and shields")
                             : "Weapons and shields";
            }
            switch (item.skin_slot()) {
            case protocol::SkinSlot::Helmet:
                return "Head";
            case protocol::SkinSlot::Upper:
                return "Chest";
            case protocol::SkinSlot::Pants:
                return "Legs";
            case protocol::SkinSlot::Boots:
                return "Feet";
            case protocol::SkinSlot::Gauntlet:
                return "Hands";
            case protocol::SkinSlot::Face:
                return "Face";
            default:
                return {};
            }
        }
    }  // namespace

    float SkinShopWindow::scale() const {
        return std::max(.25f, m_scale * m_fit);
    }
    WindowFrameStyle SkinShopWindow::frame_style() const {
        return UiView::frame_style(m_document);
    }
    void SkinShopWindow::sync_from_layout() {
        UiView::reload(m_document, m_layout);
        if (m_document.window_w() > 0) {
            m_w = m_document.window_w();
        }
        if (m_document.window_h() > 0) {
            m_h = m_document.window_h();
        }
        if (const auto* panel = m_document.find_widget("catalog")) {
            m_catalog_x = panel->x();
            m_catalog_y = panel->y();
            m_catalog_w = std::max(1.f, panel->w());
            m_catalog_h = std::max(1.f, panel->h());
        }
    }
    float SkinShopWindow::catalog_height() const {
        return m_catalog_h * kCatalogW / m_catalog_w;
    }
    void SkinShopWindow::center_on_screen(float width, float height) {
        sync_from_layout();
        m_fit = std::min(
            {1.f, (width - 24) / ((m_w + 80) * m_scale), (height - 24) / ((m_h + 140) * m_scale)});
        m_x = std::max(40.f * scale(), (width - m_w * scale()) * .5f);
        m_y = std::max(84.f * scale(), (height - m_h * scale()) * .5f);
    }
    void SkinShopWindow::close() {
        m_open = false;
        m_dragging = false;
    }
    void SkinShopWindow::destroy(engine::IEngine& engine) {
        for (const auto& [index, texture] : m_thumbnails) {
            (void) index;
            if (texture.id) {
                engine.destroy_texture(texture);
            }
        }
        m_thumbnails.clear();
        if (m_mirror.id) {
            engine.destroy_texture(m_mirror);
        }
        m_mirror = {};
        m_mirror_key.clear();
    }
    void SkinShopWindow::show(const protocol::ShopItemList& list,
                              SkinShopCategory category,
                              Class clazz,
                              const ItemDb& catalog) {
        sync_from_layout();
        m_npc_id = list.npc_id;
        m_currency = list.currency_index;
        m_catalog = &catalog;
        m_category = category;
        m_clazz = clazz;
        m_offers.clear();
        for (const auto& offer : list.offers) {
            const auto* item = catalog.find(offer.index);
            if (item && item->is_skin() && item->class_limit() == static_cast<int>(clazz) &&
                offer.buy > 0) {
                m_offers.push_back(offer);
            }
        }
        m_selected = {};
        m_scroll = 0;
        m_yaw = 0;
        m_mirror_key.clear();
        rebuild(catalog);
        m_open = true;
    }
    void SkinShopWindow::rebuild(const ItemDb& catalog) {
        m_groups.clear();
        m_cards.clear();
        m_blocks.clear();
        bool one = false, two = false;
        for (const auto& offer : m_offers) {
            const auto& item = *catalog.find(offer.index);
            if (item.skin_slot() == protocol::SkinSlot::Weapon ||
                item.skin_slot() == protocol::SkinSlot::Shield) {
                two = two || two_handed(item);
                one = one || !two_handed(item);
            }
        }
        const bool split = one && two;
        const std::vector<std::string> order =
            m_category == SkinShopCategory::Weapons
                ? (split ? std::vector<std::string>{"One-handed weapons and shields",
                                                    "Two-handed weapons"}
                         : std::vector<std::string>{"Weapons and shields"})
            : m_category == SkinShopCategory::FullBody
                ? std::vector<std::string>{"Full-body skins"}
                : std::vector<std::string>{"Head", "Chest", "Hands", "Legs", "Feet", "Face"};
        float y = 0;
        for (const auto& label : order) {
            SkinShopGroup group{label, {}};
            for (const auto& offer : m_offers) {
                const auto& item = *catalog.find(offer.index);
                const bool weapon = item.skin_slot() == protocol::SkinSlot::Weapon ||
                                    item.skin_slot() == protocol::SkinSlot::Shield;
                if ((m_category == SkinShopCategory::Weapons && !weapon) ||
                    (m_category == SkinShopCategory::FullBody && !item.full_body_skin()) ||
                    (m_category == SkinShopCategory::Parts && (weapon || item.full_body_skin()))) {
                    continue;
                }
                if (group_label(item, m_category, split) == label) {
                    group.offers.push_back(offer);
                }
            }
            if (group.offers.empty()) {
                continue;
            }
            std::stable_sort(
                group.offers.begin(), group.offers.end(), [&](const auto& a, const auto& b) {
                    return catalog.find(a.index)->name() < catalog.find(b.index)->name();
                });
            m_blocks.push_back({label, y});
            y += 28;
            for (std::size_t i = 0; i < group.offers.size(); ++i) {
                m_cards.push_back({group.offers[i],
                                   static_cast<float>(i % 3) * kPitch,
                                   y + static_cast<float>(i / 3) * kPitch});
            }
            y += std::ceil(group.offers.size() / 3.f) * kPitch + 14;
            m_groups.push_back(std::move(group));
        }
        m_content_height = y;
        m_scroll = std::clamp(m_scroll, 0.f, std::max(0.f, y - catalog_height()));
    }
    bool SkinShopWindow::rect_hit(float mx, float my, float x, float y, float w, float h) const {
        const float s = scale();
        return mx >= m_x + x * s && my >= m_y + y * s && mx < m_x + (x + w) * s &&
               my < m_y + (y + h) * s;
    }
    bool SkinShopWindow::contains(float mx, float my) const {
        return m_open &&
               WidgetTools::window_outer_hit(
                   mx, my, m_x, m_y, m_w * scale(), m_h * scale(), scale(), frame_style());
    }
    void SkinShopWindow::update_pointer(const engine::FrameInput& input) {
        if (!m_open) {
            return;
        }
        sync_from_layout();
        if (!input.mouse_left) {
            m_dragging = false;
        }
        if (m_dragging) {
            m_x = input.mouse_x - m_drag_dx;
            m_y = input.mouse_y - m_drag_dy;
        }
        if (rect_hit(
                input.mouse_x, input.mouse_y, m_catalog_x, m_catalog_y, m_catalog_w, m_catalog_h)) {
            m_scroll = std::clamp(m_scroll - input.mouse_scroll * 65.f,
                                  0.f,
                                  std::max(0.f, m_content_height - catalog_height()));
        }
    }
    UiVarMap SkinShopWindow::variables(const Bag& bag, const ItemDb* catalog) const {
        const auto* skin = catalog ? catalog->find(m_selected.index) : nullptr;
        return {{"skin_shop.title", "Skin Artisan"},
                {"skin_shop.weapons", "Weapons"},
                {"skin_shop.full", "Full skins"},
                {"skin_shop.parts", "Skin pieces"},
                {"skin_shop.rotate_left", "<"},
                {"skin_shop.rotate_right", ">"},
                {"skin_shop.rotate_hint", "Rotate preview"},
                {"skin_shop.tickets",
                 "Tickets available: " + std::to_string(bag.count_item(m_currency))},
                {"skin_shop.cost",
                 "Cost: " + (m_selected.index ? std::to_string(m_selected.buy) + " tickets"
                                              : std::string("--"))},
                {"skin_shop.selected", skin ? skin->name() : "Select a skin to try it on"},
                {"skin_shop.redeem", "Redeem"},
                {"skin_shop.close", "Close"},
                {"skin_shop.hint", "Preview is free. Choose your look."},
                {"skin_shop.category",
                 m_category == SkinShopCategory::Weapons    ? "Weapon skins"
                 : m_category == SkinShopCategory::FullBody ? "Full-body skins"
                                                            : "Individual pieces"}};
    }
    bool SkinShopWindow::button_enabled(std::string_view action) const {
        return action != "skin_shop.redeem" || m_can_redeem;
    }
    SkinShopClickResult
    SkinShopWindow::handle_click(float mx, float my, const Bag& bag, const ItemDb& catalog) {
        if (!contains(mx, my)) {
            return SkinShopClickResult::Ignored;
        }
        m_can_redeem =
            m_selected.index && bag.count_item(m_currency) >= static_cast<int>(m_selected.buy);
        const auto hit = UiView::hit(m_document,
                                     nullptr,
                                     mx,
                                     my,
                                     m_x,
                                     m_y,
                                     m_w * scale(),
                                     m_h * scale(),
                                     scale(),
                                     variables(bag, &catalog),
                                     this);
        if (hit.kind() == UiHitKind::Close || hit.action() == "skin_shop.close") {
            close();
            return SkinShopClickResult::Close;
        }
        if (hit.action() == "skin_shop.redeem" && m_can_redeem) {
            return SkinShopClickResult::Redeem;
        }
        if (hit.action() == "skin_shop.left" || hit.action() == "skin_shop.right") {
            m_yaw += hit.action() == "skin_shop.left" ? -.35f : .35f;
            m_mirror_key.clear();
        }
        if (hit.action() == "skin_shop.weapons" || hit.action() == "skin_shop.full" ||
            hit.action() == "skin_shop.parts") {
            m_category = hit.action() == "skin_shop.weapons" ? SkinShopCategory::Weapons
                         : hit.action() == "skin_shop.full"  ? SkinShopCategory::FullBody
                                                             : SkinShopCategory::Parts;
            m_scroll = 0;
            m_selected = {};
            m_mirror_key.clear();
            rebuild(catalog);
        }
        if (rect_hit(mx, my, m_catalog_x, m_catalog_y, m_catalog_w, m_catalog_h)) {
            for (const auto& card : m_cards) {
                const float y = card.y - m_scroll;
                const float ratio = m_catalog_w / kCatalogW;
                if (y + kCardH > 0 && y < catalog_height() &&
                    rect_hit(mx,
                             my,
                             m_catalog_x + card.x * ratio,
                             m_catalog_y + y * ratio,
                             kCardW * ratio,
                             kCardH * ratio)) {
                    m_selected = card.offer;
                    m_mirror_key.clear();
                    break;
                }
            }
        }
        if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
        }
        return SkinShopClickResult::Consumed;
    }

    Wear
    SkinShopWindow::preview_wear(const Wear& worn, const ItemDef& skin, const ItemDb& catalog) {
        Wear result = worn;
        if (skin.skin_slot() == protocol::SkinSlot::Weapon) {
            const auto* actual = catalog.find(result.weapon_index());
            if (!actual || !skin.skin_matches_weapon(*actual)) {
                const ItemDef* proxy = nullptr;
                for (const auto& [index, candidate] : catalog.by_index()) {
                    if (candidate.is_weapon() && skin.skin_matches_weapon(candidate) &&
                        (candidate.class_limit() < 0 ||
                         candidate.class_limit() == skin.class_limit()) &&
                        (!proxy || index < proxy->index())) {
                        proxy = &candidate;
                    }
                }
                if (proxy) {
                    result.set_weapon_index(proxy->index());
                }
            }
            if (two_handed(skin)) {
                result.unequip_index(result.skin_index(protocol::SkinSlot::Shield));
                result.set_shield_index(0);
            }
        }
        if (skin.skin_slot() == protocol::SkinSlot::Shield) {
            const auto* weapon = catalog.find(result.weapon_index());
            if (weapon && weapon->class_small() == "sword2h") {
                result.unequip_index(result.skin_index(protocol::SkinSlot::Weapon));
                const ItemDef* proxy = nullptr;
                for (const auto& [index, candidate] : catalog.by_index()) {
                    if (candidate.is_weapon() && candidate.class_small() == "sword" &&
                        (candidate.class_limit() < 0 ||
                         candidate.class_limit() == skin.class_limit()) &&
                        (!proxy || index < proxy->index())) {
                        proxy = &candidate;
                    }
                }
                result.set_weapon_index(proxy ? proxy->index() : 0);
            }
        }
        if (skin.skin_slot() == protocol::SkinSlot::Shield && !result.has_shield()) {
            const ItemDef* proxy = nullptr;
            for (const auto& [index, candidate] : catalog.by_index()) {
                if (candidate.is_defense() && candidate.class_small() == "shield" &&
                    (candidate.class_limit() < 0 ||
                     candidate.class_limit() == skin.class_limit()) &&
                    (!proxy || index < proxy->index())) {
                    proxy = &candidate;
                }
            }
            if (proxy) {
                result.set_shield_index(proxy->index());
            }
        }
        result.apply_item(skin);
        return result;
    }

    engine::TextureHandle SkinShopWindow::render_preview(
        engine::IEngine& engine,
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
        bool thumbnail) {
        if (skin && EquipmentPaths::skin_gb_path(data_root, m_clazz, *skin).empty()) {
            return {};
        }
        if (thumbnail && skin &&
            (skin->skin_slot() == protocol::SkinSlot::Weapon ||
             skin->skin_slot() == protocol::SkinSlot::Shield)) {
            return weapon_thumbnail(engine,
                                    EquipmentPaths::skin_gb_path(data_root, m_clazz, *skin),
                                    skin->skin_slot() == protocol::SkinSlot::Weapon
                                        ? skin->weapon_skin_type()
                                        : protocol::WeaponSkinType::None,
                                    texture_cache,
                                    width,
                                    height);
        }
        const Wear outfit = skin ? preview_wear(worn, *skin, catalog) : worn;
        AnimatedActor actor;
        actor.set_appearance(face, hair);
        if (!actor.load(engine, data_root, m_clazz, texture_cache, &outfit, &catalog)) {
            actor.destroy(engine);
            return {};
        }
        actor.update(engine, 0.f, false, false);
        if (!engine.begin_offscreen(width, height, .035f, .045f, .045f)) {
            actor.destroy(engine);
            return {};
        }
        const auto model =
            engine::Mat4::from_quaternion(0, std::sin(yaw * .5f), 0, std::cos(yaw * .5f));
        engine::Vec3 center{0, 10.5f, 0};
        float target_y = 10.5f, distance = 47.f;
        if (!thumbnail) {
            engine::Vec3 lo{-5, 0, -4}, hi{5, 22, 4};
            for (const auto slot : {protocol::SkinSlot::Weapon, protocol::SkinSlot::Shield}) {
                const auto* equipped =
                    catalog.find(slot == protocol::SkinSlot::Weapon ? outfit.weapon_index()
                                                                    : outfit.shield_index());
                if (!equipped) {
                    continue;
                }
                const auto* appearance = catalog.find(outfit.skin_index(slot));
                std::string path =
                    appearance && (slot == protocol::SkinSlot::Shield ||
                                   appearance->skin_matches_weapon(*equipped))
                        ? EquipmentPaths::skin_gb_path(data_root, m_clazz, *appearance)
                        : std::string{};
                if (path.empty()) {
                    path = slot == protocol::SkinSlot::Weapon
                               ? EquipmentPaths::weapon_gb_path(data_root,
                                                                m_clazz,
                                                                equipped->action_class(),
                                                                equipped->action_index())
                               : EquipmentPaths::shield_gb_path(data_root,
                                                                m_clazz,
                                                                equipped->action_class(),
                                                                equipped->action_index());
                }
                engine::Mat4 attachment;
                const bool attached =
                    slot == protocol::SkinSlot::Weapon
                        ? actor.weapon_world_matrix(model, attachment)
                        : actor.bone_world_matrix(
                              model, EquipmentPaths::shield_bone_index(m_clazz), attachment);
                terrain::GbModel geometry;
                if (!attached || path.empty() || !terrain::load_gb(path, geometry)) {
                    continue;
                }
                for (const auto& mesh : geometry.meshes) {
                    if (mesh.name.compare(0, 6, "effect") == 0 ||
                        mesh.name.compare(0, 6, "Effect") == 0) {
                        continue;
                    }
                    for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                        const auto point = engine::Mat4::transform_point(
                            attachment, mesh.pos_uv[i], mesh.pos_uv[i + 1], mesh.pos_uv[i + 2]);
                        lo.x = std::min(lo.x, point.x);
                        lo.y = std::min(lo.y, point.y);
                        lo.z = std::min(lo.z, point.z);
                        hi.x = std::max(hi.x, point.x);
                        hi.y = std::max(hi.y, point.y);
                        hi.z = std::max(hi.z, point.z);
                    }
                }
            }
            center = {(lo.x + hi.x) * .5f, (lo.y + hi.y) * .5f, (lo.z + hi.z) * .5f};
            const float aspect = static_cast<float>(width) / height;
            distance = (std::max((hi.y - lo.y) * .5f, (hi.x - lo.x) * .5f / aspect) /
                            std::tan(.55f * .5f) +
                        (hi.z - lo.z) * .5f) *
                       1.12f;
            target_y = center.y;
        }

        if (thumbnail && skin && !skin->full_body_skin()) {
            switch (skin->skin_slot()) {
            case protocol::SkinSlot::Helmet:
            case protocol::SkinSlot::Face:
                target_y = 18.5f;
                distance = 16;
                break;
            case protocol::SkinSlot::Upper:
                target_y = 13.5f;
                distance = 24;
                break;
            case protocol::SkinSlot::Boots:
                target_y = 2;
                distance = 15;
                break;
            case protocol::SkinSlot::Pants:
                target_y = 7;
                distance = 22;
                break;
            case protocol::SkinSlot::Gauntlet:
                target_y = 11;
                distance = 27;
                break;
            default:
                distance = 51;
                break;
            }
        }
        const engine::Vec3 target{center.x, target_y, center.z},
            eye{center.x, target_y + 1, center.z - distance};
        const auto view = engine::Mat4::multiply(engine::Mat4::scaling(-1, 1, 1),
                                                 engine::Mat4::look_at(eye, target, {0, 1, 0}));
        engine.set_camera(
            view, engine::Mat4::perspective(.55f, static_cast<float>(width) / height, .1f, 150.f));

        actor.draw(engine, model, view, false);
        actor.draw(engine, model, view, true);
        std::vector<std::uint8_t> pixels;
        const bool captured = engine.end_offscreen_rgba(pixels);
        actor.destroy(engine);
        return captured ? engine.create_texture_rgba8(pixels.data(), width, height, true)
                        : engine::TextureHandle{};
    }

    void SkinShopWindow::draw(engine::IEngine& engine,
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
                              const engine::Mat4& world_projection) {
        if (!m_open) {
            return;
        }
        sync_from_layout();
        const float s = scale();
        m_can_redeem =
            m_selected.index && bag.count_item(m_currency) >= static_cast<int>(m_selected.buy);
        std::ostringstream key;
        key << m_selected.index << ':' << face << ':' << hair << ':' << worn.weapon_index() << ':'
            << worn.shield_index();
        for (int i = 0; i < static_cast<int>(ClothPart::Count); ++i) {
            key << ':' << worn.cloth_index(static_cast<ClothPart>(i));
        }
        for (int i = 0; i < static_cast<int>(protocol::kSkinSlots); ++i) {
            key << ':' << worn.skin_index(static_cast<protocol::SkinSlot>(i));
        }
        if (key.str() != m_mirror_key) {
            if (m_mirror.id) {
                engine.destroy_texture(m_mirror);
            }
            m_mirror = render_preview(engine,
                                      catalog,
                                      worn,
                                      catalog.find(m_selected.index),
                                      data_root,
                                      face,
                                      hair,
                                      texture_cache,
                                      320,
                                      480,
                                      m_yaw,
                                      false);
            m_mirror_key = key.str();
        }
        // One thumbnail per frame avoids loading the entire class catalog on opening.
        for (const auto& card : m_cards) {
            if (card.y - m_scroll + kCardH < 0 || card.y - m_scroll > catalog_height() ||
                m_thumbnails.count(card.offer.index)) {
                continue;
            }
            Wear base;
            m_thumbnails[card.offer.index] = render_preview(engine,
                                                            catalog,
                                                            base,
                                                            catalog.find(card.offer.index),
                                                            data_root,
                                                            face,
                                                            hair,
                                                            texture_cache,
                                                            170,
                                                            128,
                                                            0,
                                                            true);
            break;
        }
        engine.set_camera(world_view, world_projection);
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     m_w * s,
                     m_h * s,
                     s,
                     variables(bag, &catalog),
                     font,
                     font,
                     this,
                     true);
    }

    void SkinShopWindow::draw_canvas(engine::IEngine& engine,
                                     WidgetTools& ui,
                                     const UiWidget& widget,
                                     float sx,
                                     float sy,
                                     float sw,
                                     float sh,
                                     float widget_scale,
                                     const UiVarMap& vars,
                                     const FreetypeFont& font) const {
        (void) ui;
        (void) vars;
        if (widget.bind() == "skin_shop.mirror") {
            if (m_mirror.id) {
                engine.draw_ui_quad(m_mirror, sx, sy, sw, sh);
            } else {
                font.draw_centered(engine,
                                   sx + sw * .5f,
                                   sy + sh * .5f,
                                   "Preview unavailable",
                                   .8f,
                                   .7f,
                                   .6f,
                                   widget_scale);
            }
            return;
        }
        if (widget.bind() != "skin_shop.catalog") {
            return;
        }
        const float left = sx, top = sy;
        const float s = sw / kCatalogW, viewport = sh / s;
        for (const auto& block : m_blocks) {
            const float y = block.y - m_scroll;
            if (y >= 0 && y + 24 <= viewport) {
                font.draw(engine, left + 4 * s, top + y * s, block.label, .90f, .81f, .62f, s);
            }
        }
        for (const auto& card : m_cards) {
            float y = card.y - m_scroll;
            if (y + kCardH <= 0 || y >= viewport) {
                continue;
            }
            const float visible_top = std::max(0.f, y),
                        visible_bottom = std::min(viewport, y + kCardH);
            const bool selected = card.offer.index == m_selected.index;
            engine.draw_ui_rect(left + card.x * s,
                                top + visible_top * s,
                                kCardW * s,
                                (visible_bottom - visible_top) * s,
                                selected ? .25f : .07f,
                                selected ? .24f : .08f,
                                selected ? .15f : .08f,
                                1);
            const auto image = m_thumbnails.find(card.offer.index);
            const float image_top = std::max(0.f, y), image_bottom = std::min(viewport, y + 128);
            if (image != m_thumbnails.end() && image->second.id && image_bottom > image_top) {
                engine.draw_ui_quad(image->second,
                                    left + card.x * s,
                                    top + image_top * s,
                                    kCardW * s,
                                    (image_bottom - image_top) * s,
                                    1,
                                    1,
                                    1,
                                    1,
                                    0,
                                    (image_top - y) / 128,
                                    1,
                                    (image_bottom - y) / 128);
            }
            const auto* item = m_catalog ? m_catalog->find(card.offer.index) : nullptr;
            if (item && y + 130 >= 0 && y + 170 <= viewport) {
                auto name = item->name();
                const auto suffix = name.rfind(" (");
                if (suffix != std::string::npos) {
                    name.resize(suffix);
                }
                const auto lines = layout_text_lines(font, name, kCardW - 10);
                for (std::size_t n = 0; n < std::min<std::size_t>(2, lines.size()); ++n) {
                    font.draw(engine,
                              left + (card.x + 5) * s,
                              top + (y + 130 + n * 18) * s,
                              name.substr(lines[n].start, lines[n].length),
                              .91f,
                              .88f,
                              .81f,
                              s);
                }
            }
        }
        if (m_cards.empty()) {
            font.draw(engine,
                      left + 12 * s,
                      top + 40 * s,
                      "No skins in this category.",
                      .8f,
                      .75f,
                      .65f,
                      s);
        }
        if (m_content_height > viewport) {
            const float thumb = std::max(20.f, viewport * viewport / m_content_height);
            const float offset =
                (viewport - thumb) * m_scroll / (m_content_height - catalog_height());
            engine.draw_ui_rect(left + (kCatalogW - 5) * s,
                                top + offset * s,
                                4 * s,
                                thumb * s,
                                .65f,
                                .55f,
                                .34f,
                                1);
        }
    }
}  // namespace openkal::client
