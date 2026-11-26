/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/UiView.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
    using namespace openkal;
    using namespace openkal::client;
    using test::expect;

    /** Captured glyph identity, placement, dimensions, tint, alpha and atlas rectangle. */
    struct GlyphQuad {
        std::uint32_t texture = 0;
        std::array<float, 12> values{};
    };

    class MultilineEngine final : public openkal::engine::IEngine {
      public:
        bool init(const char*, int, int) override {
            return true;
        }

        void shutdown() override {}

        bool poll(openkal::engine::FrameInput&) override {
            return false;
        }

        void maximize_window() override {}

        bool set_window_mode(openkal::engine::WindowMode, int, int) override {
            return true;
        }

        void set_cursor_mode(openkal::engine::CursorMode) override {}

        openkal::engine::RenderCapabilities render_capabilities() const override {
            return {};
        }

        void set_bloom_strength(float) override {}

        void begin_frame(float, float, float) override {}

        void begin_ui_pass() override {}

        [[nodiscard]] openkal::engine::FrameDrawStats frame_stats() const override {
            return {};
        }

        void set_camera(const openkal::engine::Mat4&, const openkal::engine::Mat4&) override {}

        void set_polygon_offset(float, float) override {}

        void set_depth_less_equal(bool) override {}

        openkal::engine::TextureHandle
        create_texture_rgba8(const std::uint8_t*, int, int, bool, bool) override {
            return openkal::engine::TextureHandle{++m_next_texture};
        }

        openkal::engine::TextureHandle create_texture_r32f(const float*, int, int) override {
            return openkal::engine::TextureHandle{++m_next_texture};
        }

        void destroy_texture(openkal::engine::TextureHandle) override {}

        openkal::engine::MeshHandle create_mesh(const openkal::engine::VertexP3T2*,
                                                std::size_t,
                                                const std::uint32_t*,
                                                std::size_t) override {
            return {};
        }

        openkal::engine::MeshHandle create_mesh_lm(const openkal::engine::VertexP3T2T2*,
                                                   std::size_t,
                                                   const std::uint32_t*,
                                                   std::size_t) override {
            return {};
        }

        bool update_mesh(openkal::engine::MeshHandle,
                         const openkal::engine::VertexP3T2*,
                         std::size_t) override {
            return false;
        }

        void destroy_mesh(openkal::engine::MeshHandle) override {}

        void draw_mesh(openkal::engine::MeshHandle, openkal::engine::TextureHandle) override {}

        void draw_mesh_textured(openkal::engine::MeshHandle,
                                openkal::engine::TextureHandle,
                                const openkal::engine::Mat4&,
                                bool,
                                bool,
                                bool,
                                bool,
                                float,
                                float,
                                float,
                                float,
                                float,
                                float,
                                float) override {}

        void draw_mesh_shadow(openkal::engine::MeshHandle,
                              openkal::engine::TextureHandle,
                              const openkal::engine::Mat4&,
                              const openkal::engine::ShadowReceiver&,
                              float,
                              bool,
                              float) override {}

        void draw_mesh_textured_instances(openkal::engine::MeshHandle,
                                          openkal::engine::TextureHandle,
                                          const std::vector<openkal::engine::Mat4>&,
                                          bool,
                                          bool) override {}

        void draw_sprite_batch(const std::vector<openkal::engine::SpriteBatchVertex>& vertices,
                               openkal::engine::TextureHandle,
                               bool,
                               bool) override {
            ++m_sprite_batch_calls;
            m_sprite_vertices += vertices.size();
        }

        void draw_mesh_lightmapped(openkal::engine::MeshHandle,
                                   openkal::engine::TextureHandle,
                                   openkal::engine::TextureHandle,
                                   const openkal::engine::Mat4&,
                                   bool,
                                   bool,
                                   bool,
                                   float,
                                   float,
                                   float,
                                   float,
                                   float,
                                   float) override {}

        void draw_water_plane(openkal::engine::MeshHandle,
                              openkal::engine::TextureHandle,
                              const openkal::engine::Mat4&,
                              const openkal::engine::Vec3&,
                              float) override {}

        void draw_sky_face(openkal::engine::MeshHandle,
                           openkal::engine::TextureHandle,
                           const openkal::engine::Mat4&) override {}

        void draw_terrain_layer(openkal::engine::MeshHandle,
                                const openkal::engine::TerrainLayerDraw&) override {}

        void draw_mesh_lit(openkal::engine::MeshHandle,
                           const openkal::engine::Mat4&,
                           float,
                           float,
                           float) override {}

        void draw_ui_quad(openkal::engine::TextureHandle texture,
                          float x,
                          float y,
                          float w,
                          float h,
                          float r,
                          float g,
                          float b,
                          float alpha,
                          float u0,
                          float v0,
                          float u1,
                          float v1) override {
            m_quads.push_back({texture.id, {x, y, w, h, r, g, b, alpha, u0, v0, u1, v1}});
        }

        /** Returns captured TrueType glyph geometry and clears the recording.
         * @return Submitted glyph quads in draw order.
         */
        std::vector<GlyphQuad> take_quads() {
            std::vector<GlyphQuad> result;
            result.swap(m_quads);
            return result;
        }

        void draw_ui_rect(float, float, float, float, float, float, float, float) override {}

        void draw_ui_triangle(
            float, float, float, float, float, float, float, float, float, float) override {}

        bool begin_offscreen(int, int, float, float, float) override {
            return false;
        }

        bool end_offscreen_rgba(std::vector<std::uint8_t>&) override {
            return false;
        }

        void end_frame() override {}

        [[nodiscard]] std::size_t sprite_batch_calls() const {
            return m_sprite_batch_calls;
        }

        [[nodiscard]] std::size_t sprite_vertices() const {
            return m_sprite_vertices;
        }

      private:
        std::vector<GlyphQuad> m_quads;
        std::uint32_t m_next_texture = 0;
        std::size_t m_sprite_batch_calls = 0;
        std::size_t m_sprite_vertices = 0;
    };

    constexpr std::array<float, 3> kBodyColor{128.0f / 255.0f, 64.0f / 255.0f, 32.0f / 255.0f};

    struct VisualLine {
        std::string text;
        int row = 0;
        std::array<float, 3> color = kBodyColor;
    };

    struct InputCase {
        const char* name = "";
        std::string text;
        float width = 400;
        float height = 100;
        float line_height = 0;
        float scale = 1;
        int pixel_size = 0;
        std::string scroll = "0";
        std::string text_template{};
    };

    UiDocument document(const InputCase& input) {
        // Project-owned synthetic openkal.ui/v1 document; no runtime assets or new schema fields.
        auto root = platform::Json::parse(R"({
            "schema":"openkal.ui/v1", "id":"multiline_coverage", "kind":"window",
            "window":{"w":640,"h":480},
            "theme":{"colors":{"value":"#804020"}},
            "widgets":[{"id":"draft","type":"input","x":7,"y":11,"w":400,"h":100,
                        "bind":"coverage.text","text":"","scroll_bind":"coverage.scroll",
                        "multiline":true,"align":"right","line_height":0,"pixel_size":0}]
        })");
        auto& widget = root.object()["widgets"].array().front().object();
        widget["w"].set_number(input.width);
        widget["h"].set_number(input.height);
        widget["line_height"].set_number(input.line_height);
        widget["pixel_size"].set_number(input.pixel_size);
        widget["text"].string() = input.text_template;
        UiDocument result;
        expect(result.load(root) && result.widgets().size() == 1 &&
                   result.widgets().front().type() == UiWidgetType::Input &&
                   result.widgets().front().multiline(),
               "load a synthetic universal multiline input document");
        return result;
    }

    void verify(MultilineEngine& engine,
                const FreetypeFont& body,
                const InputCase& input,
                const std::vector<VisualLine>& expected_lines,
                float expected_step,
                float expected_text_scale = 1.0f,
                float expected_ui_scale = 1.0f) {
        const auto doc = document(input);
        UiVarMap vars{{"coverage.text", input.text}, {"coverage.amount", input.text}};
        if (!input.scroll.empty()) {
            vars["coverage.scroll"] = input.scroll;
        }
        WidgetTools ui;
        FreetypeFont title;
        (void) engine.take_quads();
        UiView::draw_widgets(engine, ui, doc, 100, 200, 640, 480, input.scale, vars, body, title);
        const auto actual = engine.take_quads();

        // The visual-line specification is independent of the wrapping implementation. Using
        // the same public font leaf isolates the caller's line choice, offsets, scale and tint
        // from differences between installed TrueType faces and FreeType versions.
        const float left = 100.0f + 7.0f * expected_ui_scale;
        const float top = 200.0f + 11.0f * expected_ui_scale;
        for (const auto& line : expected_lines) {
            body.draw(engine,
                      left,
                      top + line.row * expected_step,
                      line.text,
                      line.color[0],
                      line.color[1],
                      line.color[2],
                      expected_text_scale);
        }
        const auto expected = engine.take_quads();
        bool same = actual.size() == expected.size();
        for (std::size_t n = 0; n < actual.size() && n < expected.size(); ++n) {
            same = same && actual[n].texture == expected[n].texture;
            for (std::size_t field = 0; field < actual[n].values.size(); ++field) {
                same =
                    same && std::abs(actual[n].values[field] - expected[n].values[field]) < .001f;
            }
        }
        expect(same, input.name);
    }

    void single_line_input(MultilineEngine& engine, const FreetypeFont& font) {
        InputCase input{"single-line input remains right-aligned and vertically centered",
                        "AB",
                        140,
                        font.line_height() + 12};
        input.scale = 1.5f;
        auto doc = document(input);
        doc.widgets().front().set_multiline(false);
        WidgetTools ui;
        FreetypeFont title;
        const UiVarMap vars{{"coverage.text", "AB"}, {"coverage.scroll", "999"}};
        (void) engine.take_quads();
        UiView::draw_widgets(engine, ui, doc, 100, 200, 640, 480, 1.5f, vars, font, title);
        const auto actual = engine.take_quads();

        // Width 140 at x 7 within client x 100 places the right edge at 320.5 after
        // zooming by 1.5. The 12 native pixels of vertical spare space split equally.
        const float left = 320.5f - font.text_width("AB", 1.5f);
        font.draw(engine, left, 225.5f, "AB", kBodyColor[0], kBodyColor[1], kBodyColor[2], 1.5f);
        const auto expected = engine.take_quads();
        bool same = actual.size() == expected.size() && !actual.empty();
        for (std::size_t n = 0; n < actual.size() && n < expected.size(); ++n) {
            same = same && actual[n].texture == expected[n].texture;
            for (std::size_t field = 0; field < actual[n].values.size(); ++field) {
                same =
                    same && std::abs(actual[n].values[field] - expected[n].values[field]) < .001f;
            }
        }
        expect(same, input.name);
    }

    void multiline_inputs(MultilineEngine& engine, const FreetypeFont& font) {
        const float step = font.line_height();
        const float pair_width = font.text_width("WW");
        verify(engine,
               font,
               {"multiline caller wraps at exact measured width", "WWWW", pair_width, 2 * step},
               {{"WW", 0}, {"WW", 1}},
               step);
        verify(engine,
               font,
               {"wider input retains a complete unwrapped line",
                "WWWW",
                font.text_width("WWWW") + 1,
                3 * step},
               {{"WWWW", 0}},
               step);
        verify(engine,
               font,
               {"wrapping preserves the space and the following word",
                "AA BB",
                font.text_width("AA "),
                2 * step},
               {{"AA ", 0}, {"BB", 1}},
               step);
        verify(engine,
               font,
               {"paragraph blanks retain their row positions", "A\n\nB\n", 400, 4 * step},
               {{"A", 0}, {"B", 2}},
               step);
        verify(engine,
               font,
               {"only complete lines fitting the input height are drawn", "A\nB\nC", 400, step},
               {{"A", 0}},
               step);
        verify(engine,
               font,
               {"a partial first line is not drawn", "A\nB", 400, step - .25f},
               {},
               step);
        verify(engine, font, {"empty multiline text emits no glyphs", "", 400, 3 * step}, {}, step);
        verify(
            engine, font, {"newline-only text emits no glyphs", "\n\n", 400, 3 * step}, {}, step);

        InputCase scrolling{
            "positive scroll skips wrapped rows and starts at the top", "A\nB\nC", 400, 2 * step};
        scrolling.scroll = "1";
        verify(engine, font, scrolling, {{"B", 0}, {"C", 1}}, step);
        scrolling.name = "negative scroll clamps to the first row";
        scrolling.scroll = "-4";
        verify(engine, font, scrolling, {{"A", 0}, {"B", 1}}, step);
        scrolling.name = "scroll beyond all lines emits no glyphs";
        scrolling.scroll = "999";
        verify(engine, font, scrolling, {}, step);
        scrolling.name = "missing scroll binding defaults to the first row";
        scrolling.scroll.clear();
        verify(engine, font, scrolling, {{"A", 0}, {"B", 1}}, step);
        scrolling.name = "nonnumeric scroll binding defaults to the first row";
        scrolling.scroll = "invalid";
        verify(engine, font, scrolling, {{"A", 0}, {"B", 1}}, step);

        const float roomy = step + 7;
        InputCase spacing{
            "requested row spacing controls baseline placement", "A\nB", 400, 2 * roomy, roomy};
        verify(engine, font, spacing, {{"A", 0}, {"B", 1}}, roomy);
        spacing.name = "UI zoom scales both row spacing and glyph geometry";
        spacing.scale = 1.5f;
        verify(engine, font, spacing, {{"A", 0}, {"B", 1}}, roomy * 1.5f, 1.5f, 1.5f);
        spacing.name = "small UI scale is clamped to one quarter";
        spacing.scale = .1f;
        verify(engine, font, spacing, {{"A", 0}, {"B", 1}}, roomy * .25f, .25f, .25f);
        spacing.name = "widget pixel size scales glyphs and the minimum row height";
        spacing.scale = 1;
        spacing.pixel_size = font.pixel_size() * 2;
        spacing.height = 4 * step;
        verify(engine, font, spacing, {{"A", 0}, {"B", 1}}, 2 * step, 2);
        verify(engine,
               font,
               {"UTF-8 fallback glyphs retain explicit paragraph positions",
                u8"é\n😀\n—",
                400,
                3 * step},
               {{u8"é", 0}, {u8"😀", 1}, {u8"—", 2}},
               step);

        InputCase colored{"color span carries through a wrapped Geon value",
                          "111111",
                          font.text_width("111,"),
                          2 * step};
        colored.text_template = "${coverage.amount:geons}";
        verify(engine, font, colored, {{"111,", 0, {0, 1, 0}}, {"111", 1, {0, 1, 0}}}, step);
        colored.name = "scrolling into a color span keeps its input color";
        colored.scroll = "1";
        verify(engine, font, colored, {{"111", 0, {0, 1, 0}}}, step);
        colored.name = "paragraph text before and after a span keeps the base tint";
        colored.scroll = "0";
        colored.height = 4 * step;
        colored.text_template = "X\n${coverage.amount:geons}\nY";
        verify(engine,
               font,
               colored,
               {{"X", 0}, {"111,", 1, {0, 1, 0}}, {"111", 2, {0, 1, 0}}, {"Y", 3}},
               step);
        FreetypeFont missing;
        verify(engine,
               missing,
               {"unavailable TrueType face emits no multiline glyphs", "A\nB", 400, 100, 20},
               {},
               20);
    }

    void repeated_input_layout(MultilineEngine& engine, const std::string& path) {
        FreetypeFont font;
        if (!font.load(engine, path, 18)) {
            expect(false, "load the face for repeated multiline layouts");
            return;
        }
        const float step = font.line_height();
        const float width = font.text_width("WW");
        InputCase input{"the first layout wraps a repeated value", "WWWW", width, 5 * step};
        verify(engine, font, input, {{"WW", 0}, {"WW", 1}}, step);
        input.name = "a repeated layout scrolls without changing its wrap";
        input.scroll = "1";
        verify(engine, font, input, {{"WW", 0}}, step);
        input.name = "changing the input text replaces its prior layout";
        input.scroll.clear();
        input.text = "A\nB\nC";
        verify(engine, font, input, {{"A", 0}, {"B", 1}, {"C", 2}}, step);
        input.name = "a narrower box wraps the same text into shorter lines";
        input.text = "WWWW";
        input.width = font.text_width("W");
        verify(engine, font, input, {{"W", 0}, {"W", 1}, {"W", 2}, {"W", 3}}, step);
        input.name = "a larger pixel size invalidates a layout at the same width";
        input.width = width;
        input.height = 10 * step;
        input.pixel_size = 36;
        verify(engine, font, input, {{"W", 0}, {"W", 1}, {"W", 2}, {"W", 3}}, 2 * step, 2);
        input.name = "restore the original layout before reloading the face";
        input.pixel_size = 0;
        verify(engine, font, input, {{"WW", 0}, {"WW", 1}}, step);
        if (font.load(engine, path, 36)) {
            input.name = "reloading the same font object discards the old glyph metrics";
            input.height = 5 * font.line_height();
            verify(
                engine, font, input, {{"W", 0}, {"W", 1}, {"W", 2}, {"W", 3}}, font.line_height());
        } else {
            expect(false, "reload the face at a different size");
        }
        font.destroy(engine);
    }

    void benchmark_multiline(MultilineEngine& engine, const FreetypeFont& font) {
        std::cout << "# Actual UiView multiline drawing, including layout and captured glyph "
                     "quads; document/font setup and quad validation excluded.\n"
                  << "characters,samples,p50_us,p95_us,p99_us,glyph_quads\n";
        for (int count : {256, 1024, 4096}) {
            const std::string text(static_cast<std::size_t>(count), 'W');
            InputCase input{
                "multiline benchmark", text, font.text_width(text) + 1, 2 * font.line_height()};
            const auto doc = document(input);
            const UiVarMap vars{{"coverage.text", text}, {"coverage.scroll", "0"}};
            WidgetTools ui;
            FreetypeFont title;
            std::vector<double> samples;
            bool complete = true;
            for (int sample = 0; sample < 25; ++sample) {
                (void) engine.take_quads();
                const auto start = std::chrono::steady_clock::now();
                UiView::draw_widgets(
                    engine, ui, doc, 100, 200, input.width + 20, 480, 1, vars, font, title);
                const auto end = std::chrono::steady_clock::now();
                const auto quads = engine.take_quads();
                complete = complete && quads.size() == static_cast<std::size_t>(count);
                samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
            }
            expect(complete, "each multiline benchmark frame draws every glyph exactly once");
            std::sort(samples.begin(), samples.end());
            std::cout << count << ",25," << std::fixed << std::setprecision(3) << samples[12] << ','
                      << samples[23] << ',' << samples[24] << ',' << count << '\n';
        }
    }
}  // namespace

int main(int argc, char** argv) {
    MultilineEngine engine;
    FreetypeFont font;
    const auto path = FreetypeFont::find_system_sans();
    expect(!path.empty() && font.load(engine, path, 18),
           "load system TrueType for UiView integration");
    if (font.ready()) {
        if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
            benchmark_multiline(engine, font);
        } else {
            multiline_inputs(engine, font);
            single_line_input(engine, font);
            repeated_input_layout(engine, path);
        }
    }
    font.destroy(engine);
    return test::g_failures == 0 ? 0 : 1;
}
