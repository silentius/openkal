/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "UiCanvas.h"

#include "openkal/client/TextWrap.h"
#include "openkal/platform/File.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <ft2build.h>
#include <string>
#include <unordered_map>
#include FT_FREETYPE_H

namespace {

    QColor hex_color(const std::string& text, QColor fallback) {
        float r = 1, g = 1, b = 1;
        if (openkal::client::ui_parse_hex_color(text, r, g, b)) {
            return QColor(static_cast<int>(r * 255.0f),
                          static_cast<int>(g * 255.0f),
                          static_cast<int>(b * 255.0f));
        }
        return fallback;
    }

    void draw_pixmap_scaled(
        QPainter& p, const QPixmap& px, float x, float y, float w, float h, float alpha = 1.0f) {
        if (px.isNull() || w <= 0.5f || h <= 0.5f) {
            return;
        }
        p.setOpacity(static_cast<qreal>(std::clamp(alpha, 0.0f, 1.0f)));
        p.drawPixmap(QRectF(static_cast<qreal>(x),
                            static_cast<qreal>(y),
                            static_cast<qreal>(w),
                            static_cast<qreal>(h)),
                     px,
                     QRectF(0, 0, px.width(), px.height()));
        p.setOpacity(1.0);
    }

    [[nodiscard]] QRectF chrome_qrect(const openkal::client::UiChromeRect& r) {
        return {static_cast<qreal>(r.x()),
                static_cast<qreal>(r.y()),
                static_cast<qreal>(r.w()),
                static_cast<qreal>(r.h())};
    }

    constexpr float kChromeGutter = 96.0f;

    /** Same search list as FreetypeFont::find_system_sans. */
    std::string ui_sans_regular_path() {
        static const char* kCandidates[] = {
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        };
        for (const char* path : kCandidates) {
            if (openkal::platform::file_exists(path)) {
                return path;
            }
        }
        return {};
    }

    /** Same search list as FreetypeFont::find_system_sans_bold. */
    std::string ui_sans_bold_path() {
        static const char* kCandidates[] = {
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
        };
        for (const char* path : kCandidates) {
            if (openkal::platform::file_exists(path)) {
                return path;
            }
        }
        return {};
    }

    QString font_family_for_path(const std::string& path) {
        static std::unordered_map<std::string, QString> cache;
        if (path.empty()) {
            return {};
        }
        const auto it = cache.find(path);
        if (it != cache.end()) {
            return it->second;
        }
        const int id = QFontDatabase::addApplicationFont(QString::fromStdString(path));
        QString family;
        if (id >= 0) {
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()) {
                family = families.front();
            }
        }
        cache.emplace(path, family);
        return family;
    }

    /** FreeType cap inset (ascender - H bearing) at a pixel size, matching FreetypeFont. */
    float ft_cap_inset(int pixel_size) {
        static FT_Library library = nullptr;
        static FT_Face face = nullptr;
        static std::string loaded_path;
        static int loaded_px = -1;
        static float cached = 2.0f;
        if (pixel_size < 1) {
            return cached;
        }
        const std::string path = ui_sans_regular_path();
        if (path.empty()) {
            return cached;
        }
        if (library == nullptr && FT_Init_FreeType(&library) != 0) {
            library = nullptr;
            return cached;
        }
        if (face == nullptr || loaded_path != path) {
            if (face != nullptr) {
                FT_Done_Face(face);
                face = nullptr;
            }
            if (FT_New_Face(library, path.c_str(), 0, &face) != 0) {
                face = nullptr;
                return cached;
            }
            loaded_path = path;
            loaded_px = -1;
        }
        if (loaded_px != pixel_size) {
            if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size)) != 0) {
                return cached;
            }
            loaded_px = pixel_size;
            const float ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
            float bearing = ascender;
            if (FT_Load_Char(face, static_cast<FT_ULong>('H'), FT_LOAD_RENDER) == 0) {
                bearing = static_cast<float>(face->glyph->bitmap_top);
            }
            cached = std::max(0.0f, ascender - bearing);
        }
        return cached;
    }

    /**
     * Draws a line with (x, y) as FreeType's line-box top.
     * QPainter::drawText(point) uses the baseline; Qt's ascent is tighter than
     * FreeType's (caps sit ~3px below y in-game, ~1px in Qt). Extra inset matches
     * FreetypeFont::draw so JSON y does not need an editor-only nudge.
     */
    void draw_line_top_left(QPainter& p, const QPointF& top_left, const QString& text) {
        const QFontMetricsF fm(p.font());
        const int px = std::max(1, p.font().pixelSize());
        const qreal qt_inset = fm.ascent() + fm.tightBoundingRect(QStringLiteral("H")).y();
        const qreal extra = static_cast<qreal>(ft_cap_inset(px)) - qt_inset;
        p.drawText(QPointF(top_left.x(), top_left.y() + fm.ascent() + extra), text);
    }

}  // namespace

UiCanvas::UiCanvas(BmpStore& bmps, QWidget* parent) : QWidget(parent), m_bmps(bmps) {
    setMinimumSize(480, 480);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundRole(QPalette::Dark);
}

void UiCanvas::set_document(openkal::client::UiDocument* doc) {
    m_doc = doc;
    update();
}

void UiCanvas::set_vars(openkal::client::UiVarMap vars) {
    m_vars = std::move(vars);
    update();
}

void UiCanvas::set_selected_index(int index) {
    if (m_selected == index) {
        return;
    }
    m_selected = index;
    update();
}

void UiCanvas::set_snap(int snap) {
    m_snap = std::max(1, snap);
}

void UiCanvas::set_show_grid(bool on) {
    m_show_grid = on;
    update();
}

void UiCanvas::set_locked(int index, bool locked) {
    if (locked) {
        m_locked.insert(index);
    } else {
        m_locked.erase(index);
    }
}

bool UiCanvas::locked(int index) const {
    return m_locked.count(index) > 0;
}

QPointF UiCanvas::client_origin() const {
    if (!m_doc || m_doc->kind() == openkal::client::UiKind::Hud) {
        return {m_margin, m_margin};
    }
    const float s = view_scale();
    return {m_margin + kChromeGutter * s, m_margin + kChromeGutter * s};
}

std::string UiCanvas::upright_file() const {
    if (!m_doc) {
        return {};
    }
    if (m_doc->chrome_preset() == "info_menu" && !m_doc->chrome_close_button()) {
        return "info-menu-top right - not close.bmp";
    }
    return m_doc->chrome_files().upright();
}

openkal::client::UiNineSliceLayout UiCanvas::nine_slice_view() const {
    openkal::client::UiNineSliceLayout lay;
    if (!m_doc || m_doc->kind() == openkal::client::UiKind::Hud) {
        return lay;
    }
    const float s = view_scale();
    const QPointF origin = client_origin();
    const auto& f = m_doc->chrome_files();
    const QPixmap tl = m_bmps.pixmap(f.upleft());
    const QPixmap tc = m_bmps.pixmap(f.upcenter());
    const QPixmap tr = m_bmps.pixmap(upright_file());
    const QPixmap l = m_bmps.pixmap(f.left());
    const QPixmap r = m_bmps.pixmap(f.right());
    const QPixmap bl = m_bmps.pixmap(f.downleft());
    const QPixmap bc = m_bmps.pixmap(f.downcenter());
    const QPixmap br = m_bmps.pixmap(f.downright());
    auto native = [](int v, float fallback) { return v > 0 ? static_cast<float>(v) : fallback; };
    return openkal::client::ui_compute_nine_slice(static_cast<float>(origin.x()),
                                                  static_cast<float>(origin.y()),
                                                  m_doc->window_w() * s,
                                                  m_doc->window_h() * s,
                                                  s,
                                                  m_doc->pad_left(),
                                                  m_doc->pad_top(),
                                                  m_doc->pad_right(),
                                                  m_doc->pad_bottom(),
                                                  native(tl.width(), 35.0f),
                                                  native(tl.height(), 53.0f),
                                                  native(tc.width(), 100.0f),
                                                  native(tc.height(), 47.0f),
                                                  native(tr.width(), 38.0f),
                                                  native(tr.height(), 53.0f),
                                                  native(l.width(), 18.0f),
                                                  native(l.height(), 30.0f),
                                                  native(r.width(), 18.0f),
                                                  native(r.height(), 30.0f),
                                                  native(bl.width(), 23.0f),
                                                  native(bl.height(), 52.0f),
                                                  native(bc.width(), 64.0f),
                                                  native(bc.height(), 16.0f),
                                                  native(br.width(), 23.0f),
                                                  native(br.height(), 52.0f),
                                                  m_doc->chrome_edges() ==
                                                      openkal::client::UiChromeEdges::Tile);
}

QRectF UiCanvas::client_view_rect() const {
    if (!m_doc) {
        return {};
    }
    const QPointF o = client_origin();
    const float s = view_scale();
    return {o.x(),
            o.y(),
            static_cast<qreal>(m_doc->window_w() * s),
            static_cast<qreal>(m_doc->window_h() * s)};
}

QRectF UiCanvas::outer_view_rect() const {
    if (!m_doc) {
        return {};
    }
    const QPointF tl = to_view(-m_doc->pad_left(), -m_doc->pad_top());
    const QPointF br =
        to_view(m_doc->window_w() + m_doc->pad_right(), m_doc->window_h() + m_doc->pad_bottom());
    return QRectF(tl, br).normalized();
}

UiCanvas::DragKind UiCanvas::hit_frame(QPointF pos) const {
    if (!m_doc || m_doc->kind() == openkal::client::UiKind::Hud) {
        const QRectF client = client_view_rect();
        if (QRectF(client.right() - 6, client.top(), 12, client.height()).contains(pos)) {
            return DragKind::ResizeW;
        }
        if (QRectF(client.left(), client.bottom() - 6, client.width(), 12).contains(pos)) {
            return DragKind::ResizeH;
        }
        return DragKind::None;
    }
    const QRectF outer = outer_view_rect();
    const QRectF client = client_view_rect();
    constexpr qreal k = 8.0;
    if (chrome_qrect(nine_slice_view().tl()).contains(pos)) {
        return DragKind::PadCornerTl;
    }
    if (QRectF(outer.left(), outer.top(), outer.width(), k).contains(pos)) {
        return DragKind::PadTop;
    }
    if (QRectF(outer.left(), outer.bottom() - k, outer.width(), k).contains(pos)) {
        return DragKind::PadBottom;
    }
    if (QRectF(outer.left(), outer.top(), k, outer.height()).contains(pos)) {
        return DragKind::PadLeft;
    }
    if (QRectF(outer.right() - k, outer.top(), k, outer.height()).contains(pos)) {
        return DragKind::PadRight;
    }
    if (QRectF(client.right() - 6, client.top(), 12, client.height()).contains(pos)) {
        return DragKind::ResizeW;
    }
    if (QRectF(client.left(), client.bottom() - 6, client.width(), 12).contains(pos)) {
        return DragKind::ResizeH;
    }
    return DragKind::None;
}

void UiCanvas::update_hover_cursor(QPointF pos) {
    switch (hit_frame(pos)) {
    case DragKind::PadLeft:
    case DragKind::PadRight:
    case DragKind::ResizeW:
        setCursor(Qt::SizeHorCursor);
        break;
    case DragKind::PadTop:
    case DragKind::PadBottom:
    case DragKind::ResizeH:
        setCursor(Qt::SizeVerCursor);
        break;
    case DragKind::PadCornerTl:
        setCursor(Qt::SizeFDiagCursor);
        break;
    default:
        unsetCursor();
        break;
    }
}

QPointF UiCanvas::to_view(float nx, float ny) const {
    const QPointF o = client_origin();
    const float s = view_scale();
    return {o.x() + static_cast<qreal>(nx * s), o.y() + static_cast<qreal>(ny * s)};
}

void UiCanvas::from_view(QPointF p, float& nx, float& ny) const {
    const QPointF o = client_origin();
    const float s = view_scale();
    nx = static_cast<float>((p.x() - o.x()) / static_cast<qreal>(s));
    ny = static_cast<float>((p.y() - o.y()) / static_cast<qreal>(s));
}

void UiCanvas::native_size(const openkal::client::UiWidget& w, float& nw, float& nh) const {
    nw = w.w();
    nh = w.h();
    std::string file = w.file_default();
    if (file.empty()) {
        file = w.file();
    }
    if ((nw <= 0.0f || nh <= 0.0f) && !file.empty()) {
        if (nw <= 0.0f) {
            nw = static_cast<float>(m_bmps.width(file));
        }
        if (nh <= 0.0f) {
            nh = static_cast<float>(m_bmps.height(file));
        }
    }
    if (nw <= 0.0f) {
        nw = 16.0f;
    }
    if (nh <= 0.0f) {
        nh = 16.0f;
    }
}

void UiCanvas::text_native_rect(
    const openkal::client::UiWidget& w, float& nx, float& ny, float& nw, float& nh) const {
    if (w.type() == openkal::client::UiWidgetType::Input && w.multiline()) {
        nx = m_doc && w.x_from_right() >= 0 ? m_doc->window_w() - w.x_from_right() : w.x();
        ny = m_doc && w.y_from_bottom() >= 0 ? m_doc->window_h() - w.y_from_bottom() : w.y();
        native_size(w, nw, nh);
        return;
    }
    nx = m_doc && w.x_from_right() >= 0 ? m_doc->window_w() - w.x_from_right() : w.x();
    ny = w.y();
    if (m_doc && w.y_from_bottom() >= 0.0f) {
        ny = m_doc->window_h() - w.y_from_bottom();
    }
    const std::string raw = w.text().empty() ? w.label() : w.text();
    const std::string shown = openkal::client::ui_interpolate(raw, m_vars);
    const int px = w.pixel_size() > 0 ? w.pixel_size() : (m_doc ? m_doc->font_pixel_size() : 10);
    const float s = view_scale();
    QFont font;
    const std::string ttf = w.bold() ? ui_sans_bold_path() : ui_sans_regular_path();
    const QString family = font_family_for_path(ttf);
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    font.setPixelSize(std::max(8, static_cast<int>(std::lround(static_cast<float>(px) * s))));
    font.setBold(w.bold());
    font.setHintingPreference(QFont::PreferFullHinting);
    const QFontMetricsF fm(font);
    const QString qshown = QString::fromStdString(shown);
    nw = shown.empty() ? 16.0f : static_cast<float>(fm.horizontalAdvance(qshown) / s);
    nh = static_cast<float>(fm.height() / s);
    if (w.align() == "right") {
        nx -= nw;
    } else if (w.align() == "center") {
        nx -= nw * 0.5f;
    }
}

float UiCanvas::snap_v(float v) const {
    const float step = static_cast<float>(m_snap);
    return std::round(v / step) * step;
}

float UiCanvas::snap_pad(float v) const {
    return std::round(v * 10.0f) / 10.0f;
}

bool UiCanvas::is_pad_drag(DragKind kind) {
    return kind == DragKind::PadLeft || kind == DragKind::PadTop || kind == DragKind::PadRight ||
           kind == DragKind::PadBottom || kind == DragKind::PadCornerTl;
}

void UiCanvas::tile_h(
    QPainter& p, const QPixmap& px, float x, float y, float dest_w, float dest_h) const {
    if (px.isNull() || dest_w <= 1.0f || dest_h <= 1.0f) {
        return;
    }
    const float tile_w = static_cast<float>(px.width()) * view_scale();
    float remain = dest_w;
    float cx = x;
    while (remain > 0.5f) {
        const float dw = std::min(tile_w, remain);
        const float src_w = dw / view_scale();
        p.drawPixmap(QRectF(static_cast<qreal>(cx),
                            static_cast<qreal>(y),
                            static_cast<qreal>(dw),
                            static_cast<qreal>(dest_h)),
                     px,
                     QRectF(0, 0, static_cast<qreal>(src_w), px.height()));
        cx += dw;
        remain -= dw;
    }
}

void UiCanvas::tile_v(
    QPainter& p, const QPixmap& px, float x, float y, float dest_w, float dest_h) const {
    if (px.isNull() || dest_w <= 1.0f || dest_h <= 1.0f) {
        return;
    }
    const float tile_h_px = static_cast<float>(px.height()) * view_scale();
    float remain = dest_h;
    float cy = y;
    while (remain > 0.5f) {
        const float dh = std::min(tile_h_px, remain);
        const float src_h = dh / view_scale();
        p.drawPixmap(QRectF(static_cast<qreal>(x),
                            static_cast<qreal>(cy),
                            static_cast<qreal>(dest_w),
                            static_cast<qreal>(dh)),
                     px,
                     QRectF(0, 0, px.width(), static_cast<qreal>(src_h)));
        cy += dh;
        remain -= dh;
    }
}

void UiCanvas::draw_chrome(QPainter& p) const {
    if (!m_doc) {
        return;
    }
    const float s = view_scale();
    const QPointF origin = client_origin();
    const float cw = m_doc->window_w() * s;
    const float ch = m_doc->window_h() * s;

    if (m_doc->kind() == openkal::client::UiKind::Hud) {
        const QPixmap hud = m_bmps.pixmap(m_doc->chrome_file());
        draw_pixmap_scaled(
            p, hud, static_cast<float>(origin.x()), static_cast<float>(origin.y()), cw, ch);
        return;
    }

    const auto& f = m_doc->chrome_files();
    const QPixmap tl = m_bmps.pixmap(f.upleft());
    const QPixmap tc = m_bmps.pixmap(f.upcenter());
    const QPixmap tr = m_bmps.pixmap(upright_file());
    const QPixmap l = m_bmps.pixmap(f.left());
    const QPixmap r = m_bmps.pixmap(f.right());
    const QPixmap bl = m_bmps.pixmap(f.downleft());
    const QPixmap bc = m_bmps.pixmap(f.downcenter());
    const QPixmap br = m_bmps.pixmap(f.downright());
    const openkal::client::UiNineSliceLayout lay = nine_slice_view();

    if (m_doc->chrome_edges() != openkal::client::UiChromeEdges::Stretch) {
        tile_h(p, tc, lay.top().x(), lay.top().y(), lay.top().w(), lay.top().h());
        tile_v(p, l, lay.left().x(), lay.left().y(), lay.left().w(), lay.left().h());
        tile_v(p, r, lay.right().x(), lay.right().y(), lay.right().w(), lay.right().h());
        tile_h(p, bc, lay.bot().x(), lay.bot().y(), lay.bot().w(), lay.bot().h());
    } else {
        draw_pixmap_scaled(p, tc, lay.top().x(), lay.top().y(), lay.top().w(), lay.top().h());
        draw_pixmap_scaled(p, l, lay.left().x(), lay.left().y(), lay.left().w(), lay.left().h());
        draw_pixmap_scaled(
            p, r, lay.right().x(), lay.right().y(), lay.right().w(), lay.right().h());
        draw_pixmap_scaled(p, bc, lay.bot().x(), lay.bot().y(), lay.bot().w(), lay.bot().h());
    }
    draw_pixmap_scaled(p, tl, lay.tl().x(), lay.tl().y(), lay.tl().w(), lay.tl().h());
    draw_pixmap_scaled(p, tr, lay.tr().x(), lay.tr().y(), lay.tr().w(), lay.tr().h());
    draw_pixmap_scaled(p, bl, lay.bl().x(), lay.bl().y(), lay.bl().w(), lay.bl().h());
    draw_pixmap_scaled(p, br, lay.br().x(), lay.br().y(), lay.br().w(), lay.br().h());

    if (m_doc->chrome_preset() == "dragon_dialog") {
        const QPixmap source = m_bmps.pixmap("dragon_hud/inscription_panel.bmp");
        if (source.width() >= 32 && source.height() >= 98) {
            const QPixmap inlay =
                source.copy(14, 22, 18, 76).transformed(QTransform(0, 1, 1, 0, 0, 0));
            const QPixmap mirrored = inlay.transformed(QTransform::fromScale(-1, 1));
            const float tile_w = inlay.width() * s;
            // Match the native bottom-left ornament silhouette used by the client.
            constexpr int left_inset[] = {10, 10, 10, 10, 13, 14, 14, 15, 22, 23,
                                          24, 24, 24, 24, 36, 37, 38, 38, 39, 45};
            for (int row = 0; row < 20; ++row) {
                const float dw = 48.0f - left_inset[row];
                p.drawPixmap(QRectF(origin.x() + left_inset[row] * s,
                                    origin.y() + ch + (20.0f + row) * s,
                                    dw * s,
                                    s),
                             mirrored,
                             QRectF(inlay.width() - dw,
                                    row * inlay.height() / 20.0f,
                                    dw,
                                    inlay.height() / 20.0f));
            }
            const float width = std::max(0.0f, cw - 64.0f * s);
            bool flip = false;
            for (float offset = 0; offset < width; offset += tile_w) {
                const float dw = std::min(tile_w, width - offset);
                p.drawPixmap(QRectF(origin.x() + 48.0f * s + offset,
                                    origin.y() + ch + 20.0f * s,
                                    dw,
                                    20.0f * s),
                             flip ? mirrored : inlay,
                             QRectF(0, 0, dw / s, inlay.height()));
                flip = !flip;
            }
        }
    }
    if (m_doc->chrome_preset() == "dragon_dialog") {
        p.save();
        QFont font;
        const QString family = font_family_for_path(ui_sans_bold_path());
        if (!family.isEmpty()) {
            font.setFamily(family);
        }
        font.setPixelSize(std::max(8, static_cast<int>(std::lround(12.0f * s))));
        font.setBold(true);
        font.setHintingPreference(QFont::PreferFullHinting);
        p.setFont(font);
        const QString brand = QStringLiteral("OpenKal");
        const QFontMetricsF metrics(font);
        const QPointF pos(origin.x() + cw - 30.0f * s - metrics.horizontalAdvance(brand),
                          origin.y() + ch + 22.0f * s);
        p.setPen(QColor(192, 160, 100));
        draw_line_top_left(p, pos + QPointF(s, s), brand);
        p.setPen(QColor(33, 28, 19));
        draw_line_top_left(p, pos, brand);
        p.restore();
    }

    if (m_doc->chrome_preset() != "info_menu" && m_doc->chrome_close_button()) {
        const QPixmap exit = m_bmps.pixmap(f.exit_file());
        const float cx = static_cast<float>(origin.x()) + cw + m_doc->close_ox() * s;
        const float cy = static_cast<float>(origin.y()) + m_doc->close_oy() * s;
        const float cw_hit = m_doc->close_w() * s;
        const float ch_hit = m_doc->close_h() * s;
        const float ew = (exit.isNull() ? 16.0f : static_cast<float>(exit.width())) * s;
        const float eh = (exit.isNull() ? 16.0f : static_cast<float>(exit.height())) * s;
        draw_pixmap_scaled(p, exit, cx + (cw_hit - ew) * 0.5f, cy + (ch_hit - eh) * 0.5f, ew, eh);
    }
}

void UiCanvas::paintEvent(QPaintEvent* event) {
    (void) event;
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.fillRect(rect(), QColor(32, 32, 36));
    const int cell = 16;
    for (int y = 0; y < height(); y += cell) {
        for (int x = 0; x < width(); x += cell) {
            if (((x / cell) + (y / cell)) % 2 == 0) {
                p.fillRect(x, y, cell, cell, QColor(40, 40, 44));
            }
        }
    }
    if (!m_doc) {
        p.setPen(QColor(180, 180, 180));
        p.drawText(rect(), Qt::AlignCenter, tr("Open an openkal.ui/v1 JSON file"));
        return;
    }

    const float s = view_scale();
    const QPointF origin = client_origin();
    const float cw = m_doc->window_w() * s;
    const float ch = m_doc->window_h() * s;
    const bool frame_foreground = m_doc->kind() == openkal::client::UiKind::Window &&
                                  m_doc->chrome_kind() == openkal::client::UiChromeKind::NineSlice;
    p.fillRect(QRectF(origin.x(), origin.y(), static_cast<qreal>(cw), static_cast<qreal>(ch)),
               QColor(20, 18, 16, 230));
    if (m_doc->chrome_preset() == "dragon_dialog") {
        p.drawPixmap(QRectF(origin.x(), origin.y(), cw, ch),
                     m_bmps.pixmap("dragon_dialog/panel_background.bmp"),
                     QRectF(0, 0, 128, 128));
    }
    if (!frame_foreground) {
        draw_chrome(p);
    }
    if (m_show_grid) {
        p.setPen(QPen(QColor(255, 255, 255, 28), 1, Qt::DotLine));
        for (int x = 0; x <= static_cast<int>(m_doc->window_w()); x += 32) {
            const QPointF a = to_view(static_cast<float>(x), 0.0f);
            const QPointF b = to_view(static_cast<float>(x), m_doc->window_h());
            p.drawLine(a, b);
        }
        for (int y = 0; y <= static_cast<int>(m_doc->window_h()); y += 32) {
            const QPointF a = to_view(0.0f, static_cast<float>(y));
            const QPointF b = to_view(m_doc->window_w(), static_cast<float>(y));
            p.drawLine(a, b);
        }
    }

    p.setPen(QPen(QColor(220, 180, 80, m_selected == -1 ? 230 : 90), 1, Qt::SolidLine));
    p.drawRect(outer_view_rect());
    if (m_selected == -1 && m_doc->kind() != openkal::client::UiKind::Hud) {
        p.setPen(QPen(QColor(255, 220, 80), 2));
        p.drawRect(chrome_qrect(nine_slice_view().tl()));
    }
    p.setPen(QPen(QColor(120, 180, 255, m_selected == -1 ? 160 : 80), 1, Qt::DashLine));
    p.drawRect(QRectF(origin.x(), origin.y(), static_cast<qreal>(cw), static_cast<qreal>(ch)));
    if (m_selected == -1 && m_doc->kind() != openkal::client::UiKind::Hud) {
        const QRectF outer = outer_view_rect();
        p.setBrush(QColor(220, 180, 80, 200));
        p.setPen(Qt::NoPen);
        constexpr qreal hs = 6.0;
        p.drawRect(QRectF(outer.center().x() - hs * 0.5, outer.top() - 1.0, hs, hs));
        p.drawRect(QRectF(outer.center().x() - hs * 0.5, outer.bottom() - hs + 1.0, hs, hs));
        p.drawRect(QRectF(outer.left() - 1.0, outer.center().y() - hs * 0.5, hs, hs));
        p.drawRect(QRectF(outer.right() - hs + 1.0, outer.center().y() - hs * 0.5, hs, hs));
        p.setBrush(Qt::NoBrush);
    }

    enum class WidgetDrawPass { All, Content, ChromeTitle };
    auto draw_widgets = [&](WidgetDrawPass pass) {
        int index = 0;
        for (const auto& source : m_doc->widgets()) {
            const auto w = openkal::client::ui_scrolled_widget(source, *m_doc, m_vars);
            const bool chrome_title =
                w.id() == "title" || w.id() == "dialog_icon" || w.id() == "frame_handle" ||
                (m_doc->chrome_preset() == "dragon_dialog" && w.action() == "window.close");
            if ((pass == WidgetDrawPass::Content && chrome_title) ||
                (pass == WidgetDrawPass::ChromeTitle && !chrome_title)) {
                ++index;
                continue;
            }
            const bool vis =
                w.visible() && openkal::client::ui_eval_condition(w.visible_when(), m_vars);
            float nw = 0, nh = 0;
            float ox = w.x_from_right() >= 0 ? m_doc->window_w() - w.x_from_right() : w.x();
            float oy = w.y_from_bottom() >= 0 ? m_doc->window_h() - w.y_from_bottom() : w.y();
            if (w.type() == openkal::client::UiWidgetType::Text ||
                w.type() == openkal::client::UiWidgetType::Input) {
                text_native_rect(w, ox, oy, nw, nh);
            } else {
                native_size(w, nw, nh);
            }
            const QPointF tl = to_view(ox, oy);
            const QRectF box(
                tl.x(), tl.y(), static_cast<qreal>(nw * s), static_cast<qreal>(nh * s));
            if (vis) {
                if (w.type() == openkal::client::UiWidgetType::Image && !w.file().empty()) {
                    const QPixmap image = m_bmps.pixmap(w.file());
                    const QRectF bounds(0, 0, image.width(), image.height());
                    const bool cropped = w.source().w > 0 && w.source().h > 0;
                    const QRectF source =
                        cropped ? QRectF(w.source().x, w.source().y, w.source().w, w.source().h)
                                      .intersected(bounds)
                                : bounds;
                    if (!source.isEmpty()) {
                        p.setOpacity(w.alpha());
                        if (!w.repeat()) {
                            p.drawPixmap(box, image, source);
                        } else {
                            for (qreal y = 0; y < box.height(); y += source.height() * s) {
                                const qreal h = std::min(source.height() * s, box.height() - y);
                                for (qreal x = 0; x < box.width(); x += source.width() * s) {
                                    const qreal width =
                                        std::min(source.width() * s, box.width() - x);
                                    p.drawPixmap(QRectF(box.x() + x, box.y() + y, width, h),
                                                 image,
                                                 QRectF(source.x(), source.y(), width / s, h / s));
                                }
                            }
                        }
                        p.setOpacity(1);
                    }
                } else if (w.type() == openkal::client::UiWidgetType::Progress &&
                           w.file().empty()) {
                    const auto it = m_vars.find(w.bind());
                    bool ok = false;
                    const double raw =
                        it == m_vars.end() ? 0 : QString::fromStdString(it->second).toDouble(&ok);
                    const double ratio = ok && std::isfinite(raw) ? std::clamp(raw, 0.0, 1.0) : 0;
                    const auto color = openkal::client::ui_interpolate(w.color(), m_vars);
                    QColor fill(QString::fromStdString(color.empty() ? "#CC3333" : color));
                    fill.setAlphaF(w.alpha());
                    p.fillRect(QRectF(box.x(), box.y(), box.width() * ratio, box.height()), fill);
                } else if (w.type() == openkal::client::UiWidgetType::Progress &&
                           w.fill_mode() == "crop") {
                    const auto it = m_vars.find(w.bind());
                    bool ok = false;
                    const double raw =
                        it == m_vars.end() ? 0 : QString::fromStdString(it->second).toDouble(&ok);
                    const double ratio = ok && std::isfinite(raw) ? std::clamp(raw, 0.0, 1.0) : 0;
                    const QPixmap image = m_bmps.pixmap(w.file());
                    if (!image.isNull() && ratio > 0) {
                        QRectF target = box;
                        QRectF source(0, 0, image.width(), image.height());
                        if (w.direction() == "bottom_to_top") {
                            target.setTop(box.bottom() - box.height() * ratio);
                            source.setTop(image.height() * (1 - ratio));
                        } else {
                            target.setWidth(box.width() * ratio);
                            source.setWidth(image.width() * ratio);
                        }
                        p.save();
                        p.setOpacity(w.alpha());
                        p.drawPixmap(target, image, source);
                        p.restore();
                    }
                } else if (w.type() == openkal::client::UiWidgetType::Button &&
                           !w.file_default().empty()) {
                    draw_pixmap_scaled(p,
                                       m_bmps.pixmap(w.file_default()),
                                       static_cast<float>(box.x()),
                                       static_cast<float>(box.y()),
                                       static_cast<float>(box.width()),
                                       static_cast<float>(box.height()));
                } else if (w.type() == openkal::client::UiWidgetType::Tabs) {
                    for (const auto& tab : w.tabs()) {
                        const auto it = m_vars.find(w.bind().empty() ? "apply_item.tab" : w.bind());
                        const bool on = it != m_vars.end() && tab.id() == it->second;
                        const std::string file = on ? tab.file_on() : tab.file_off();
                        float tw = tab.w();
                        float th = w.h() > 0.0f ? w.h() : 18.0f;
                        if (tw <= 0.0f) {
                            tw = 40.0f;
                        }
                        const QPointF tpos = to_view(tab.x(), w.y());
                        draw_pixmap_scaled(p,
                                           m_bmps.pixmap(file),
                                           static_cast<float>(tpos.x()),
                                           static_cast<float>(tpos.y()),
                                           tw * s,
                                           th * s);
                        QFont font = p.font();
                        const QString family = font_family_for_path(ui_sans_regular_path());
                        if (!family.isEmpty()) {
                            font.setFamily(family);
                        }
                        font.setPixelSize(std::max(
                            8, static_cast<int>(std::lround(m_doc->font_pixel_size() * s))));
                        font.setBold(false);
                        p.setFont(font);
                        const QString label = QString::fromStdString(
                            openkal::client::ui_interpolate(tab.label(), m_vars));
                        p.setPen(hex_color(openkal::client::ui_interpolate(
                                               on ? w.color_on() : w.color_off(), m_vars),
                                           QColor(234, 227, 212)));
                        QPointF label_pos = tpos + QPointF(4 * s, 4 * s);
                        if (w.align() == "center") {
                            const QFontMetricsF metrics(font);
                            label_pos =
                                tpos + QPointF((tw * s - metrics.horizontalAdvance(label)) * 0.5,
                                               (th * s - metrics.height()) * 0.5);
                        }
                        draw_line_top_left(p, label_pos, label);
                    }
                } else if (w.type() == openkal::client::UiWidgetType::Canvas &&
                           w.canvas_kind() == "minimap") {
                    p.save();
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(24, 34, 30));
                    if (w.shape() == "circle") {
                        const qreal diameter = std::min(box.width(), box.height());
                        p.drawEllipse(box.center(), diameter * 0.5, diameter * 0.5);
                    } else {
                        p.drawRect(box);
                    }
                    if (w.bind() == "powerbook.map" || w.bind() == "snitch.map") {
                        p.setBrush(QColor(255, 214, 31, 76));
                        p.setPen(QPen(QColor(255, 224, 56), 2));
                        p.drawRect(QRectF(box.x() + box.width() * .34,
                                          box.y() + box.height() * .32,
                                          box.width() * .32,
                                          box.height() * .36));
                    }
                    p.setPen(QPen(QColor(242, 231, 204), 2));
                    const QPointF c = box.center();
                    p.drawLine(c + QPointF(-4 * s, 4 * s), c + QPointF(0, -5 * s));
                    p.drawLine(c + QPointF(0, -5 * s), c + QPointF(4 * s, 4 * s));
                    p.restore();
                } else if (w.type() == openkal::client::UiWidgetType::Fill) {
                    const std::string c = openkal::client::ui_interpolate(w.color(), m_vars);
                    QColor fill = hex_color(c, QColor(0, 0, 0));
                    fill.setAlphaF(static_cast<qreal>(w.alpha()));
                    p.fillRect(box, fill);
                } else if (w.type() == openkal::client::UiWidgetType::IconSlot ||
                           w.type() == openkal::client::UiWidgetType::IconGrid) {
                    const bool grid = w.type() == openkal::client::UiWidgetType::IconGrid;
                    const int cols = grid ? std::max(1, w.cols()) : 1;
                    const int rows = grid ? std::max(1, w.rows()) : 1;
                    const float cell_w = grid ? (w.cell_w() > 0 ? w.cell_w() : 32) : nw;
                    const float cell_h = grid ? (w.cell_h() > 0 ? w.cell_h() : 32) : nh;
                    for (int row = 0; row < rows; ++row) {
                        for (int col = 0; col < cols; ++col) {
                            const float x = static_cast<float>(box.x()) + col * w.pitch_x() * s;
                            const float y = static_cast<float>(box.y()) + row * w.pitch_y() * s;
                            if (!w.frame_file().empty()) {
                                const float fw = w.frame_w() > 0 ? w.frame_w() : cell_w;
                                const float fh = w.frame_h() > 0 ? w.frame_h() : cell_h;
                                draw_pixmap_scaled(p,
                                                   m_bmps.pixmap(w.frame_file()),
                                                   x + (cell_w - fw) * s * 0.5f,
                                                   y + (cell_h - fh) * s * 0.5f,
                                                   fw * s,
                                                   fh * s);
                            }
                            if (!w.empty_file().empty()) {
                                draw_pixmap_scaled(
                                    p, m_bmps.pixmap(w.empty_file()), x, y, cell_w * s, cell_h * s);
                            }
                        }
                    }
                }
                if (w.type() == openkal::client::UiWidgetType::Text || !w.label().empty() ||
                    !w.text().empty()) {
                    const std::string raw = w.text().empty() ? w.label() : w.text();
                    std::vector<openkal::client::UiTextColorRun> colors;
                    const std::string shown = openkal::client::ui_interpolate(raw, m_vars, &colors);
                    std::string color_raw = openkal::client::ui_interpolate(w.color(), m_vars);
                    QColor col = hex_color(color_raw, QColor(255, 250, 235));
                    if (!w.pending_when().empty() &&
                        openkal::client::ui_eval_condition(w.pending_when(), m_vars)) {
                        col = hex_color(m_doc->color_pending(), QColor(200, 224, 144));
                        colors.clear();
                    }
                    QFont font = p.font();
                    const int px = w.pixel_size() > 0 ? w.pixel_size() : m_doc->font_pixel_size();
                    const std::string ttf = w.bold() ? ui_sans_bold_path() : ui_sans_regular_path();
                    const QString family = font_family_for_path(ttf);
                    if (!family.isEmpty()) {
                        font.setFamily(family);
                    }
                    font.setPixelSize(std::max(8, static_cast<int>(std::lround(px * s))));
                    font.setBold(w.bold());
                    font.setHintingPreference(QFont::PreferFullHinting);
                    p.setFont(font);
                    p.setPen(col);
                    const auto draw_text = [&](const QPointF& origin,
                                               std::size_t offset,
                                               std::size_t length) {
                        const QFontMetricsF metrics(font);
                        const auto draw = [&](std::size_t begin, std::size_t end, const QColor& c) {
                            if (begin >= end) {
                                return;
                            }
                            p.setPen(c);
                            const auto before = QString::fromStdString(shown.substr(offset, begin));
                            draw_line_top_left(
                                p,
                                origin + QPointF(metrics.horizontalAdvance(before), 0),
                                QString::fromStdString(shown.substr(offset + begin, end - begin)));
                        };
                        std::size_t start = 0;
                        for (const auto& run : colors) {
                            if (run.start + run.length <= offset || run.start >= offset + length) {
                                continue;
                            }
                            const auto begin = std::max(run.start, offset) - offset;
                            const auto end =
                                std::min(run.start + run.length, offset + length) - offset;
                            draw(start, begin, col);
                            draw(begin, end, hex_color(run.color, col));
                            start = end;
                        }
                        draw(start, length, col);
                    };
                    QPointF tp = to_view(ox, oy);
                    if (w.type() == openkal::client::UiWidgetType::Button) {
                        tp = QPointF(box.x() + static_cast<qreal>(w.label_ox() * s),
                                     box.y() + static_cast<qreal>(w.label_oy() * s));
                        if (w.align() == "center" || w.align() == "right") {
                            const QFontMetricsF metrics(font);
                            const qreal remaining =
                                box.width() -
                                metrics.horizontalAdvance(QString::fromStdString(shown));
                            tp.rx() += w.align() == "center" ? remaining * 0.5 : remaining;
                            tp.ry() += (box.height() - metrics.height()) * 0.5;
                        }
                    }
                    if (w.type() == openkal::client::UiWidgetType::Input && w.multiline()) {
                        const QFontMetricsF metrics(font);
                        const auto lines = openkal::client::layout_text_lines(
                            shown, box.width(), [&](std::string_view line) {
                                return static_cast<float>(metrics.horizontalAdvance(
                                    QString::fromUtf8(line.data(), static_cast<int>(line.size()))));
                            });
                        const auto scroll = m_vars.find(w.scroll_bind());
                        const auto offset = static_cast<std::size_t>(std::max(
                            0, scroll == m_vars.end() ? 0 : std::atoi(scroll->second.c_str())));
                        const qreal step =
                            std::max(static_cast<qreal>(w.line_height() * s), metrics.height());
                        p.save();
                        p.setClipRect(box);
                        for (std::size_t i = offset;
                             i < lines.size() && (i - offset + 1) * step <= box.height();
                             ++i) {
                            draw_text(box.topLeft() + QPointF(0, (i - offset) * step),
                                      lines[i].start,
                                      lines[i].length);
                        }
                        p.restore();
                    } else {
                        draw_text(tp, 0, shown.size());
                    }
                }
            }
            if (index == m_selected) {
                p.setPen(QPen(QColor(80, 220, 120), 1));
                p.drawRect(box);
            } else if (!vis) {
                p.setPen(QPen(QColor(180, 80, 80, 120), 1, Qt::DotLine));
                p.drawRect(box);
            }
            ++index;
        }
    };
    draw_widgets(frame_foreground ? WidgetDrawPass::Content : WidgetDrawPass::All);
    if (frame_foreground) {
        draw_chrome(p);
        draw_widgets(WidgetDrawPass::ChromeTitle);
    }
}

void UiCanvas::mousePressEvent(QMouseEvent* event) {
    if (!m_doc || event->button() != Qt::LeftButton) {
        return;
    }
    setFocus(Qt::MouseFocusReason);
    const QPointF pos = event->position();
    float nx = 0, ny = 0;
    from_view(pos, nx, ny);

    const DragKind frame = hit_frame(pos);
    if (is_pad_drag(frame)) {
        m_drag = frame;
        m_grab_nx = static_cast<float>(pos.x());
        m_grab_ny = static_cast<float>(pos.y());
        switch (frame) {
        case DragKind::PadLeft:
            m_grab_pad = m_doc->pad_left();
            break;
        case DragKind::PadTop:
            m_grab_pad = m_doc->pad_top();
            break;
        case DragKind::PadRight:
            m_grab_pad = m_doc->pad_right();
            break;
        case DragKind::PadBottom:
            m_grab_pad = m_doc->pad_bottom();
            break;
        case DragKind::PadCornerTl:
            m_grab_pad = m_doc->pad_left();
            m_grab_pad2 = m_doc->pad_top();
            break;
        default:
            break;
        }
        set_selected_index(-1);
        emit selection_changed(-1);
        return;
    }

    int hit = -2;
    for (int i = static_cast<int>(m_doc->widgets().size()) - 1; i >= 0; --i) {
        const auto w = openkal::client::ui_scrolled_widget(
            m_doc->widgets()[static_cast<std::size_t>(i)], *m_doc, m_vars);
        if (!w.visible() || !openkal::client::ui_eval_condition(w.visible_when(), m_vars)) {
            continue;
        }
        float nw = 0, nh = 0;
        float ox = w.x_from_right() >= 0 ? m_doc->window_w() - w.x_from_right() : w.x();
        float oy = w.y_from_bottom() >= 0 ? m_doc->window_h() - w.y_from_bottom() : w.y();
        if (w.type() == openkal::client::UiWidgetType::Text ||
            w.type() == openkal::client::UiWidgetType::Input) {
            text_native_rect(w, ox, oy, nw, nh);
        } else {
            native_size(w, nw, nh);
        }
        if (nx >= ox && ny >= oy && nx < ox + nw && ny < oy + nh) {
            hit = i;
            break;
        }
    }
    if (hit < 0) {
        if (frame == DragKind::ResizeW || frame == DragKind::ResizeH) {
            m_drag = frame;
            m_grab_nx = static_cast<float>(pos.x());
            m_grab_ny = static_cast<float>(pos.y());
            return;
        }
        const QRectF outer = outer_view_rect();
        if (outer.contains(pos) || client_view_rect().contains(pos)) {
            hit = -1;
        }
    }
    set_selected_index(hit);
    emit selection_changed(hit);
    if (hit >= 0 && !locked(hit)) {
        m_drag = DragKind::Widget;
        m_drag_index = hit;
        const auto& w = m_doc->widgets()[static_cast<std::size_t>(hit)];
        m_grab_nx = nx - (w.x_from_right() >= 0 ? m_doc->window_w() - w.x_from_right() : w.x());
        m_grab_ny = ny - (w.y_from_bottom() >= 0 ? m_doc->window_h() - w.y_from_bottom() : w.y());
    }
}

void UiCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!m_doc) {
        return;
    }
    if (m_drag == DragKind::None) {
        update_hover_cursor(event->position());
        return;
    }
    float nx = 0, ny = 0;
    from_view(event->position(), nx, ny);
    const float s = view_scale();
    auto clamp_pad = [](float v) { return std::clamp(v, 0.0f, 512.0f); };
    if (m_drag == DragKind::ResizeW) {
        m_doc->set_window_w(std::max(32.0f, snap_v(nx)));
        emit geometry_changed();
        update();
        return;
    }
    if (m_drag == DragKind::ResizeH) {
        m_doc->set_window_h(std::max(32.0f, snap_v(ny)));
        emit geometry_changed();
        update();
        return;
    }
    if (m_drag == DragKind::PadTop) {
        const float d = (static_cast<float>(event->position().y()) - m_grab_ny) / s;
        m_doc->set_pad_top(clamp_pad(snap_pad(m_grab_pad - d)));
        emit geometry_changed();
        update();
        return;
    }
    if (m_drag == DragKind::PadBottom) {
        const float d = (static_cast<float>(event->position().y()) - m_grab_ny) / s;
        m_doc->set_pad_bottom(clamp_pad(snap_pad(m_grab_pad + d)));
        emit geometry_changed();
        update();
        return;
    }
    if (m_drag == DragKind::PadLeft) {
        const float d = (static_cast<float>(event->position().x()) - m_grab_nx) / s;
        m_doc->set_pad_left(clamp_pad(snap_pad(m_grab_pad - d)));
        emit geometry_changed();
        update();
        return;
    }
    if (m_drag == DragKind::PadRight) {
        const float d = (static_cast<float>(event->position().x()) - m_grab_nx) / s;
        m_doc->set_pad_right(clamp_pad(snap_pad(m_grab_pad + d)));
        emit geometry_changed();
        update();
        return;
    }
    if (m_drag == DragKind::PadCornerTl) {
        const float dx = (static_cast<float>(event->position().x()) - m_grab_nx) / s;
        const float dy = (static_cast<float>(event->position().y()) - m_grab_ny) / s;
        m_doc->set_pad_left(clamp_pad(snap_pad(m_grab_pad - dx)));
        m_doc->set_pad_top(clamp_pad(snap_pad(m_grab_pad2 - dy)));
        emit geometry_changed();
        update();
        return;
    }
    if (m_drag == DragKind::Widget && m_drag_index >= 0 &&
        m_drag_index < static_cast<int>(m_doc->widgets().size())) {
        auto& w = m_doc->widgets()[static_cast<std::size_t>(m_drag_index)];
        w.set_x(snap_v(nx - m_grab_nx));
        w.set_y(snap_v(ny - m_grab_ny));
        if (w.x_from_right() >= 0) {
            w.set_x_from_right(m_doc->window_w() - w.x());
        }
        if (w.y_from_bottom() >= 0) {
            w.set_y_from_bottom(m_doc->window_h() - w.y());
        }
        emit geometry_changed();
        update();
    }
}

void UiCanvas::mouseReleaseEvent(QMouseEvent* event) {
    (void) event;
    m_drag = DragKind::None;
    m_drag_index = -1;
}

void UiCanvas::keyPressEvent(QKeyEvent* event) {
    if (!m_doc || m_selected != -1 || m_doc->kind() == openkal::client::UiKind::Hud) {
        QWidget::keyPressEvent(event);
        return;
    }
    const float step = event->modifiers() & Qt::ShiftModifier ? 0.1f : 1.0f;
    auto nudge = [this, step](float cur, int sign) {
        return std::clamp(snap_pad(cur + step * static_cast<float>(sign)), 0.0f, 512.0f);
    };
    switch (event->key()) {
    case Qt::Key_Up:
        m_doc->set_pad_top(nudge(m_doc->pad_top(), 1));
        break;
    case Qt::Key_Down:
        m_doc->set_pad_top(nudge(m_doc->pad_top(), -1));
        break;
    case Qt::Key_Left:
        m_doc->set_pad_left(nudge(m_doc->pad_left(), 1));
        break;
    case Qt::Key_Right:
        m_doc->set_pad_left(nudge(m_doc->pad_left(), -1));
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    emit geometry_changed();
    update();
}

void UiCanvas::wheelEvent(QWheelEvent* event) {
    if (!(event->modifiers() & Qt::ControlModifier)) {
        if (m_doc) {
            float x, y;
            from_view(event->position(), x, y);
            for (const auto& source : m_doc->widgets()) {
                const auto* viewport = m_doc->find_widget(source.viewport());
                if (!viewport || source.scroll_bind().empty() ||
                    !openkal::client::ui_eval_condition(source.visible_when(), m_vars) ||
                    x < viewport->x() || x >= viewport->x() + viewport->w() || y < viewport->y() ||
                    y >= viewport->y() + viewport->h()) {
                    continue;
                }
                float bottom = viewport->y() + viewport->h();
                float step = 20;
                for (const auto& content : m_doc->widgets()) {
                    if (content.viewport() != source.viewport() ||
                        content.scroll_bind() != source.scroll_bind() ||
                        !openkal::client::ui_eval_condition(content.visible_when(), m_vars)) {
                        continue;
                    }
                    float height = content.h();
                    if (content.type() == openkal::client::UiWidgetType::IconGrid) {
                        step = std::max(1.0f, content.pitch_y());
                        height = (content.rows() - 1) * step + content.cell_h() +
                                 std::max(0.0f, (content.frame_h() - content.cell_h()) * .5f);
                    }
                    bottom = std::max(bottom, content.y() + height);
                }
                auto& value = m_vars[source.scroll_bind()];
                const float offset = QString::fromStdString(value).toFloat();
                const float maximum =
                    std::ceil((bottom - viewport->y() - viewport->h()) / step) * step;
                value = std::to_string(std::clamp(
                    offset + (event->angleDelta().y() > 0 ? -step : step), 0.0f, maximum));
                update();
                event->accept();
                return;
            }
        }
        QWidget::wheelEvent(event);
        return;
    }
    const float factor = event->angleDelta().y() > 0 ? 1.1f : 1.0f / 1.1f;
    m_view_scale = std::clamp(m_view_scale * factor, 0.5f, 4.0f);
    update();
}
