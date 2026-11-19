/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/LatinText.h"
#include "openkal/client/TextWrap.h"
#include "test_util.h"

// Keep internal stateless category helpers testable without changing the product API.
// This translation unit replaces Inventory.cpp in this target's source list.
#include "../client/src/ui/Inventory.cpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#ifdef OPENKAL_TEST_WRAP_NEW
namespace {
    bool g_track_projection_allocations = false;
    std::size_t g_projection_allocation_count = 0;
    std::size_t g_projection_allocation_bytes = 0;
}  // namespace

extern "C" void* __real__Znwm(std::size_t size);
extern "C" void* __wrap__Znwm(std::size_t size) {
    if (g_track_projection_allocations) {
        ++g_projection_allocation_count;
        g_projection_allocation_bytes += size;
    }
    return __real__Znwm(size);
}
#endif

namespace {
    using namespace openkal;
    using namespace openkal::client;
    using test::expect;

    float scalar_width(std::string_view text) {
        float width = 0;
        for (unsigned char byte : text) {
            if ((byte & 0xc0) != 0x80) {
                ++width;
            }
        }
        return width;
    }

    void expect_lines(std::string_view text,
                      float width,
                      const std::function<float(std::string_view)>& measure,
                      const std::vector<std::string>& expected) {
        const auto lines = layout_text_lines(text, width, measure);
        bool same = lines.size() == expected.size();
        for (std::size_t i = 0; i < std::min(lines.size(), expected.size()); ++i) {
            same = same && text.substr(lines[i].start, lines[i].length) == expected[i];
        }
        expect(same, "layout retains exact source spans, spaces and explicit blank lines");
    }

    void text_layout() {
        expect_lines("", 1, scalar_width, {""});
        expect_lines("\n\n", 1, scalar_width, {"", "", ""});
        expect_lines("one\n\ntwo\n", 100, scalar_width, {"one", "", "two", ""});
        expect_lines("  a  b ", 3, scalar_width, {"  ", "a  ", "b "});
        expect_lines("ab cd", 4, scalar_width, {"ab ", "cd"});
        expect_lines("abcd", 2, scalar_width, {"ab", "cd"});
        expect_lines("abcd", 0.001f, scalar_width, {"a", "b", "c", "d"});
        expect_lines("a", 0.001f, scalar_width, {"a"});
        expect_lines("ab cd\n", 0, scalar_width, {"ab cd", ""});
        expect_lines("ab cd", -1, scalar_width, {"ab cd"});
        expect_lines("ab cd", 1, {}, {"ab cd"});
        expect_lines(u8"ä😀x", 1, scalar_width, {u8"ä", u8"😀", "x"});
        expect_lines(u8"ä 😀x", 2, scalar_width, {u8"ä ", u8"😀x"});
        expect_lines(u8"—é…", 2, scalar_width, {u8"—é", u8"…"});
        // A measure callback is opaque: even a nonadditive width must retain its
        // current first-overflow behavior in the generic layout path.
        expect_lines("abcd",
                     3,
                     [](std::string_view text) { return text.size() == 2 ? 10.0f : 1.0f; },
                     {"a", "b", "c", "d"});
        const std::string malformed = std::string("A") + '\x80' + '\x81' + "B" + '\xc2';
        expect_lines(malformed,
                     1,
                     scalar_width,
                     {std::string("A") + '\x80' + '\x81', "B", std::string(1, '\xc2')});

        int measured = 0;
        bool propagated = false;
        try {
            (void) layout_text_lines("abcd", 1, [&](std::string_view text) {
                if (++measured == 2) {
                    throw std::runtime_error("measurement failed");
                }
                return scalar_width(text);
            });
        } catch (const std::runtime_error& error) {
            propagated = std::string_view(error.what()) == "measurement failed";
        }
        expect(propagated && measured == 2,
               "generic layout propagates callback failure after creating an earlier visual line");

        std::mt19937 random(0x12345);
        const std::array<std::string, 9> alphabet{
            "a", "b", " ", "\n", u8"é", u8"😀", u8"—", u8"…", "x"};
        bool conserved = true;
        bool bounded = true;
        for (int sample = 0; sample < 300; ++sample) {
            std::string text;
            for (unsigned count = random() % 150; count > 0; --count) {
                text += alphabet[random() % alphabet.size()];
            }
            const float width = static_cast<float>(1 + random() % 20);
            const auto lines = layout_text_lines(text, width, scalar_width);
            std::string joined;
            for (const auto& line : lines) {
                const auto part = std::string_view(text).substr(line.start, line.length);
                joined += part;
                conserved =
                    conserved &&
                    (part.empty() || (static_cast<unsigned char>(part.front()) & 0xc0) != 0x80);
                bounded = bounded && scalar_width(part) <= width;
            }
            std::string without_newlines;
            for (char byte : text) {
                if (byte != '\n') {
                    without_newlines += byte;
                }
            }
            conserved = conserved && joined == without_newlines;
        }
        expect(conserved && bounded,
               "seeded UTF-8 layout conserves every non-newline byte and respects scalar "
               "boundaries and width");
    }

    bool same_lines(const std::vector<TextLine>& first, const std::vector<TextLine>& second) {
        if (first.size() != second.size()) {
            return false;
        }
        for (std::size_t i = 0; i < first.size(); ++i) {
            if (first[i].start != second[i].start || first[i].length != second[i].length) {
                return false;
            }
        }
        return true;
    }

    void freetype_layout(const FreetypeFont& font, bool random_cases) {
        std::vector<std::string> inputs{"",
                                        "\n\n",
                                        "one\n\ntwo\n",
                                        "  a  b ",
                                        "ab cd",
                                        "WWWW",
                                        u8"W…aWW",
                                        u8"ä 😀 — …",
                                        "\tA\rB",
                                        "A\x80\x81"
                                        "B",
                                        "\x80\x81"
                                        "A",
                                        "\xc2",
                                        "\xe2\x80",
                                        "\xf0\x9f",
                                        "\xc0\xaf",
                                        "\xed\xa0\x80",
                                        "\x91\x92\xa0"};
        if (random_cases) {
            std::mt19937 random(0x6f726163);
            const std::array<std::string, 16> tokens{"a",
                                                     "W",
                                                     " ",
                                                     "\n",
                                                     u8"é",
                                                     u8"😀",
                                                     u8"—",
                                                     u8"…",
                                                     "\x80",
                                                     "\x91",
                                                     "\xc2",
                                                     "\xe2\x80",
                                                     "\xc0\xaf",
                                                     "\t",
                                                     "\r",
                                                     std::string(1, '\0')};
            for (int sample = 0; sample < 120; ++sample) {
                std::string text;
                for (unsigned left = random() % 80; left > 0; --left) {
                    text += tokens[random() % tokens.size()];
                }
                inputs.push_back(std::move(text));
            }
        }
        bool equal = true;
        for (const auto& text : inputs) {
            for (float scale : {-1.f, 0.f, .01f, .33333334f, .73f, .75f, 1.f, 1.5f}) {
                const auto measure = [&](std::string_view part) {
                    return font.text_width(part, scale);
                };
                const float edge = measure("WW");
                const float folded_edge = measure(u8"W…a");
                for (float width :
                     {-1.f,
                      0.f,
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::min(),
                      1.f,
                      edge,
                      std::nextafter(edge, 0.f),
                      std::nextafter(edge, std::numeric_limits<float>::infinity()),
                      folded_edge,
                      std::nextafter(folded_edge, 0.f),
                      std::nextafter(folded_edge, std::numeric_limits<float>::infinity()),
                      measure(text),
                      measure(text) + 1.f}) {
                    const auto expected = layout_text_lines(text, width, measure);
                    const auto actual = layout_text_lines(font, text, width, scale);
                    const bool same = same_lines(expected, actual);
                    if (!same && equal) {
                        std::cerr << "FreeType span mismatch: bytes=" << text.size()
                                  << " width=" << std::hexfloat << width << " scale=" << scale
                                  << std::defaultfloat << '\n';
                    }
                    equal = equal && same;
                }
            }
        }
        expect(equal,
               "incremental FreeType layout matches legacy exact byte spans across UTF-8, "
               "malformed clusters, ellipsis, scales and neighboring float widths");
    }

    void font_measurement(engine::IEngine& engine) {
        FreetypeFont font;
        expect(font.text_width("unloaded") == 0 && font.text_width("") == 0,
               "an uninitialized font has zero advances including its glyph fallback");
        freetype_layout(font, false);
        const auto path = FreetypeFont::find_system_sans();
        expect(!path.empty() && font.load(engine, path, 18),
               "load a system TrueType font for width checks");
        if (!font.ready()) {
            return;
        }
        freetype_layout(font, true);
        expect(font.text_width("A") > 0 && font.text_width("") == 0,
               "loaded ASCII advances are positive and empty text stays zero");
        const std::string source = u8"Aé😀 ‘quote’—…";
        expect(std::abs(font.text_width(source) - font.text_width("A?? 'quote'-...")) < .001f,
               "TrueType measurement folds UTF-8 punctuation and unsupported glyphs identically");
        const std::string malformed = std::string(1, '\xc2');
        expect(std::abs(font.text_width(malformed) - font.text_width("?")) < .001f,
               "truncated UTF-8 uses the existing fallback advance");
        for (float scale : {-1.0f, 0.0f, 0.001f, .01f, .75f, 1.0f, 1.5f}) {
            const float expected = font.text_width("Abc") * std::max(.01f, scale);
            expect(std::abs(font.text_width("Abc", scale) - expected) < .001f,
                   "font measurement clamps scale and scales additive advances");
            float sum = 0;
            LatinText::for_each_ascii(
                source, [&](char byte) { sum += font.text_width(std::string(1, byte), scale); });
            expect(std::abs(font.text_width(source, scale) - sum) < .001f,
                   "whole-string width equals the sum of folded glyph widths");
            const auto measure = [&](std::string_view text) {
                return font.text_width(text, scale);
            };
            expect_lines("ab cd", measure("ab c"), measure, {"ab ", "cd"});
        }
        font.destroy(engine);
        freetype_layout(font, false);
        expect(font.text_width("after reload reset") == 0, "destroy resets the width state");
        expect(font.load(engine, path, 24) && font.text_width("A") > 0,
               "font reload restores independent new glyph advances");
        freetype_layout(font, true);
        font.destroy(engine);
    }

    void layout_work_probe(engine::IEngine& engine, int count, std::string_view mode) {
        FreetypeFont font;
        expect(font.load(engine, FreetypeFont::find_system_sans(), 18),
               "load TrueType for an isolated layout work probe");
        if (!font.ready()) {
            return;
        }
        std::string text;
        if (mode == "mixed") {
            const std::array<std::string_view, 5> tokens{u8"…", "W", " ", u8"😀", "a"};
            for (int i = 0; i < count; ++i) {
                text += tokens[static_cast<std::size_t>(i) % tokens.size()];
            }
        } else {
            text.assign(static_cast<std::size_t>(count), 'W');
            if (mode == "rewind") {
                for (std::size_t i = 16; i < text.size(); i += 17) {
                    text[i] = ' ';
                }
            }
        }
        std::size_t glyphs = 0;
        LatinText::for_each_ascii(text, [&](char) { ++glyphs; });
        const auto lines = layout_text_lines(font, text, mode == "rewind" ? 100.f : 1e9f);
        std::size_t bytes = 0;
        for (const auto& line : lines) {
            bytes += line.length;
        }
        expect(bytes == text.size(), "isolated layout probe conserves every source byte");
        std::cout << "text_bytes,folded_glyphs,lines\n"
                  << text.size() << ',' << glyphs << ',' << lines.size() << '\n';
        font.destroy(engine);
    }

    void categories() {
        expect(inventory_filter_category(nullptr) == 5, "unknown item definitions belong to Other");
        for (const auto& row : std::vector<std::pair<std::string, int>>{{"skin", 4},
                                                                        {"pet", 4},
                                                                        {"limitedpet", 4},
                                                                        {"transform", 4},
                                                                        {"ride", 4},
                                                                        {"weapon", 1},
                                                                        {"defense", 2},
                                                                        {"ornament", 2},
                                                                        {"general", 5}}) {
            ItemDef def;
            def.set_index(91000);
            def.set_class_big(row.first);
            def.set_class_small(row.first == "skin" ? "helmet" : "normal");
            expect(inventory_filter_category(&def) == row.second,
                   "each supported class reaches its inventory category");
        }
        for (const char* part : {"face", "mask"}) {
            ItemDef def;
            def.set_class_small(part);
            expect(inventory_filter_category(&def) == 4,
                   "face and mask use the cosmetic category before defense");
        }
        for (int variant = 0; variant < 3; ++variant) {
            ItemDef def;
            if (variant == 0) {
                def.set_has_changeprefix(true);
            }
            if (variant == 1) {
                def.set_charming("attack");
            }
            if (variant == 2) {
                def.set_bof(true);
            }
            expect(inventory_filter_category(&def) == 3,
                   "each metadata-defined upgrade uses Enchantment");
        }
        // These are existing semantic IDs, exercised as in-memory values only.
        // No new production item definitions or allocations are introduced.
        for (int index : {23017, 23018, 23019, 23020, 23021, 23022, 23026, 24003, 24057, 24062}) {
            ItemDef def;
            def.set_index(index);
            expect(inventory_filter_category(&def) == 3,
                   "each special upgrade family uses Enchantment");
        }
        ItemDef medicine;
        medicine.set_refresh_hp(1);
        expect(inventory_filter_category(&medicine) == 0,
               "consumables use their distinct category");
        medicine.set_class_big("weapon");
        expect(inventory_filter_category(&medicine) == 1,
               "specific equipment classification precedes consumable metadata");
        expect(filter_button_index("unknown") == -1, "unknown filter button has no category");
        for (int i = 0; i < 6; ++i) {
            expect(filter_button_index(std::string("filter.") + kInventoryFilterIds[i]) == i,
                   "every filter button maps to the documented category order");
        }
    }

    struct CatalogFixture {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
                                     ("openkal_optimization_ui_" + std::to_string(getpid()));
        ItemDb catalog;
        CatalogFixture() {
            std::filesystem::create_directories(root / "items");
            const std::array<const char*, 6> definitions{
                R"({"index":91040,"name":"Owned medicine","class":["general","refresh"],"use":1,"specialty":{"refresh":{"stat":"hp","amount":5}}})",
                R"({"index":91041,"name":"Owned sword","class":["weapon","sword"],"wear":1})",
                R"({"index":91042,"name":"Owned armor","class":["defense","upperarmor"],"wear":1})",
                R"({"index":91043,"name":"Owned upgrade","class":["general","etc"],"specialty":{"charming":"xattack"}})",
                R"({"index":91044,"name":"Owned pet","class":["pet","normal"],"wear":1})",
                R"({"index":91045,"name":"Owned remainder","class":["general","etc"]})"};
            for (int i = 0; i < 6; ++i) {
                std::ofstream(root / "items" / (std::to_string(91040 + i) + ".json"))
                    << definitions[i];
            }
            expect(catalog.load(root.string(), root.string()),
                   "load only project-authored synthetic inventory items");
        }
        ~CatalogFixture() {
            std::filesystem::remove_all(root);
        }
    };

    ItemStack stack(int category, int slot) {
        ItemStack item;
        item.index = 91040 + category;
        item.count = 1 + slot % 17;
        item.iid = 1000 + slot;
        item.prefix = 3 + slot % 7;
        item.account_bound = slot % 2 != 0;
        return item;
    }

    std::pair<float, float>
    control(const Window& window, const UiDocument& document, const std::string& id) {
        const auto* widget = document.find_widget(id);
        expect(widget != nullptr, "inventory control exists in the owned layout");
        if (!widget) {
            return {};
        }
        float x, y, w, h;
        UiView::widget_rect(*widget,
                            nullptr,
                            window.x(),
                            window.y(),
                            window.w(),
                            window.h(),
                            window.scale(),
                            x,
                            y,
                            w,
                            h);
        return {x + w / 2, y + h / 2};
    }

    void select(Window& window, const UiDocument& document, Bag& bag, Wear& wear, unsigned mask) {
        window.set_open(true);
        window.set_drag_slot(-1);
        for (int i = 0; i < 6; ++i) {
            if (window.filter_active(i) != ((mask & (1u << i)) != 0)) {
                const auto point =
                    control(window, document, std::string("filter.") + kInventoryFilterIds[i]);
                expect(window.handle_click(point.first, point.second, bag, wear) ==
                           ClickResult::Handled,
                       "filter click changes one category without mutating bag items");
            }
        }
        window.update_pointer({}, bag);
    }

    std::array<int, kBagSlots> expected_projection(const Bag& bag,
                                                   unsigned filters,
                                                   const std::array<bool, kMaxRegisters>& enabled) {
        std::array<int, kBagSlots> projection;
        projection.fill(-1);
        if (!filters) {
            for (int i = 0; i < kBagSlots; ++i) {
                projection[i] = i;
            }
            return projection;
        }
        // Independent specification: sort each storage domain stably by priority:
        // selected occupied slots, genuinely empty slots, then excluded slots.
        std::vector<std::vector<int>> domains(1);
        for (int page = 0; page < kMaxRegisters; ++page) {
            std::vector<int> domain;
            for (int i = 0; i < kSlotsPerPage; ++i) {
                domain.push_back(page * kSlotsPerPage + i);
            }
            if (enabled[page]) {
                domains[0].insert(domains[0].end(), domain.begin(), domain.end());
            } else {
                domains.push_back(std::move(domain));
            }
        }
        const auto priority = [&](int source) {
            if (source >= static_cast<int>(bag.normal.size())) {
                return 2;
            }
            const auto& item = bag.normal[source];
            if (item.index == 0 || item.count <= 0) {
                return 1;
            }
            const int category =
                item.index >= 91040 && item.index <= 91045 ? item.index - 91040 : 5;
            return filters & (1u << category) ? 0 : 2;
        };
        for (const auto& domain : domains) {
            auto ordered = domain;
            std::stable_sort(ordered.begin(), ordered.end(), [&](int a, int b) {
                return priority(a) < priority(b);
            });
            for (std::size_t i = 0; i < ordered.size() && priority(ordered[i]) < 2; ++i) {
                projection[domain[i]] = ordered[i];
            }
        }
        return projection;
    }

    struct ProjectionFixture {
        CatalogFixture fixture;
        UiLayout layout;
        UiDocument document;
        Window window;
        Bag bag;
        Wear wear;
        UiAssets ui;
        std::array<bool, kMaxRegisters> enabled{};
        static constexpr unsigned filter_mask = 21;

        ProjectionFixture() {
            expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/inventory.json"),
                   "load the owned projection probe layout");
            expect(document.load(layout.root()), "parse the owned projection probe document");
            window.set_layout(&layout);
            window.set_catalog(&fixture.catalog);
            window.set_wear(&wear);
            window.center_on_screen(1920, 1080);
            reset();
            select(window, document, bag, wear, filter_mask);
            window.set_open(false);
        }

        void reset() {
            bag.clear();
            for (int cell = 0; cell < kBagSlots; ++cell) {
                if (cell % 9 < 6) {
                    bag.normal[cell] = stack(cell % 6, cell);
                } else if (cell % 9 == 6) {
                    bag.normal[cell] = stack(1, cell);
                    bag.normal[cell].count = 0;
                } else if (cell % 9 == 7) {
                    bag.normal[cell].count = 7;
                }
            }
            enabled = {true, true, false, true, false, false};
            window.set_register_enabled(enabled);
            window.set_open(false);
        }

        void mutate(int iteration) {
            auto& item = bag.normal[(iteration * 17) % kBagSlots];
            item.index = 91040 + iteration % 6;
            item.count = iteration % 4 == 0 ? 0 : 1 + iteration % 7;
            item.iid = 5000 + iteration;
            for (int page = 0; page < kMaxRegisters; ++page) {
                enabled[page] = ((iteration >> page) & 1) != 0;
            }
            window.set_register_enabled(enabled);
        }

        void verify() {
            const auto expected = expected_projection(bag, filter_mask, enabled);
            bool equal = !window.open();
            for (int display = 0; display < kBagSlots; ++display) {
                equal = equal && window.general_source_slot(display) == expected[display];
            }
            expect(equal, "the closed-window probe retains every oracle-defined source mapping");
        }
    };

    std::uint64_t exercise_projection(ProjectionFixture& fixture,
                                      engine::IEngine& engine,
                                      bool hidden_draw,
                                      bool mutate,
                                      int iterations = 1000) {
        std::uint64_t checksum = 0;
        for (int iteration = 0; iteration < iterations; ++iteration) {
            if (mutate) {
                fixture.mutate(iteration);
            }
            if (hidden_draw) {
                fixture.window.draw(fixture.ui, engine, fixture.bag);
            } else {
                fixture.window.update_pointer({}, fixture.bag);
            }
            checksum += static_cast<std::uint64_t>(
                fixture.window.general_source_slot((iteration * 17) % kBagSlots) + 2);
            checksum += static_cast<std::uint64_t>(
                fixture.window.general_source_slot((iteration * 29 + 3) % kBagSlots) + 2);
        }
        return checksum;
    }

    struct ProjectionAllocations {
        std::size_t count = 0, bytes = 0;
        std::uint64_t checksum = 0;
    };

    ProjectionAllocations count_projection_allocations(ProjectionFixture& fixture,
                                                       engine::IEngine& engine,
                                                       bool hidden_draw,
                                                       bool mutate,
                                                       int iterations = 1000) {
        ProjectionAllocations result;
#ifdef OPENKAL_TEST_WRAP_NEW
        g_projection_allocation_count = g_projection_allocation_bytes = 0;
        g_track_projection_allocations = true;
#endif
        result.checksum = exercise_projection(fixture, engine, hidden_draw, mutate, iterations);
#ifdef OPENKAL_TEST_WRAP_NEW
        g_track_projection_allocations = false;
        result.count = g_projection_allocation_count;
        result.bytes = g_projection_allocation_bytes;
#endif
        return result;
    }

    void
    projection_work_probe(engine::IEngine& engine, std::string_view mode, bool require_no_alloc) {
        ProjectionFixture fixture;
        const bool valid = mode == "setup" || mode == "pointer" || mode == "draw" ||
                           mode == "mutate_pointer" || mode == "mutate_draw";
        expect(valid, "known projection work-probe mode");
        if (!valid) {
            return;
        }
        const auto* address = fixture.bag.normal.data();
        const auto allocations =
            count_projection_allocations(fixture,
                                         engine,
                                         mode == "draw" || mode == "mutate_draw",
                                         mode == "mutate_pointer" || mode == "mutate_draw",
                                         mode == "setup" ? 0 : 1000);
        fixture.verify();
        expect(fixture.bag.normal.data() == address, "probe mutations retain the same bag address");
#ifdef OPENKAL_TEST_WRAP_NEW
        if (require_no_alloc) {
            expect(allocations.count == 0 && allocations.bytes == 0,
                   "projection refresh performs no scalar-new allocation requests");
        }
#else
        expect(!require_no_alloc, "the allocation assertion requires GNU scalar-new wrapping");
#endif
        std::cout << "projection_work_probe mode=" << mode
                  << " iterations=" << (mode == "setup" ? 0 : 1000)
                  << " scalar_new_requests=" << allocations.count
                  << " scalar_new_requested_bytes=" << allocations.bytes
#ifdef OPENKAL_TEST_WRAP_NEW
                  << " scalar_new_wrapped=1"
#else
                  << " scalar_new_wrapped=0"
#endif
                  << " checksum=" << allocations.checksum << '\n';
    }

    void benchmark_projection(engine::IEngine& engine) {
        ProjectionFixture fixture;
        std::cout << "projection benchmark:25 samples,1000 calls per sample; windows closed; "
                     "mutating cases include same-address edits; no timing threshold\n"
                     "GNU scalar-new requests/requested bytes are counted separately from timing; "
                     "malloc, new[], aligned allocation and retained heap are not measured\n"
                     "path,mutating,samples,calls,median_us,p95_us,scalar_new_requests,scalar_new_"
                     "requested_bytes,checksum\n";
        for (bool hidden_draw : {false, true}) {
            for (bool mutate : {false, true}) {
                std::vector<double> samples;
                std::uint64_t checksum = 0;
                for (int sample = 0; sample < 25; ++sample) {
                    fixture.reset();
                    const auto start = std::chrono::steady_clock::now();
                    checksum += exercise_projection(fixture, engine, hidden_draw, mutate);
                    samples.push_back(std::chrono::duration<double, std::micro>(
                                          std::chrono::steady_clock::now() - start)
                                          .count());
                    fixture.verify();
                }
                fixture.reset();
                const auto* address = fixture.bag.normal.data();
                const auto allocations =
                    count_projection_allocations(fixture, engine, hidden_draw, mutate);
                fixture.verify();
                expect(fixture.bag.normal.data() == address,
                       "allocation measurement retains the same bag address");
                expect(checksum == allocations.checksum * 25,
                       "timed and counted refresh intervals produce the same observable mappings");
                std::sort(samples.begin(), samples.end());
                std::cout << (hidden_draw ? "hidden_draw" : "closed_pointer") << ',' << mutate
                          << ",25,1000," << std::fixed << std::setprecision(3) << samples[12] << ','
                          << samples[23] << ',' << allocations.count << ',' << allocations.bytes
                          << ',' << checksum << '\n';
            }
        }
    }

    void projection_and_interactions(engine::IEngine& engine) {
        CatalogFixture fixture;
        UiLayout layout;
        expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/inventory.json"),
               "load the redistributable project inventory layout");
        UiDocument document;
        expect(document.load(layout.root()), "parse the owned inventory document");
        Window window;
        window.set_layout(&layout);
        window.set_catalog(&fixture.catalog);
        window.center_on_screen(1920, 1080);
        Wear wear;
        window.set_wear(&wear);
        Bag bag;
        bag.clear();
        for (int i = 0; i < kBagSlots; ++i) {
            if (i % 9 < 6) {
                bag.normal[i] = stack(i % 6, i);
            } else if (i % 9 == 6) {
                bag.normal[i] = stack(1, i);
                bag.normal[i].count = 0;
            } else if (i % 9 == 7) {
                bag.normal[i].count = 7;
            }
        }
        const auto original = bag.normal;
        bool all_mappings = true;
        bool preserved = true;
        for (float scale : {.75f, 1.f, 1.5f}) {
            window.set_scale(scale);
            window.center_on_screen(1920, 1080);
            for (unsigned mask = 0; mask < 64; ++mask) {
                select(window, document, bag, wear, mask);
                window.set_open(false);
                for (unsigned writable = 0; writable < 64; ++writable) {
                    std::array<bool, kMaxRegisters> enabled;
                    for (int page = 0; page < kMaxRegisters; ++page) {
                        enabled[page] = (writable & (1u << page)) != 0;
                    }
                    window.set_register_enabled(enabled);
                    window.update_pointer({}, bag);
                    const auto expected = expected_projection(bag, mask, enabled);
                    for (int display = 0; display < kBagSlots; ++display) {
                        all_mappings = all_mappings &&
                                       window.general_source_slot(display) == expected[display];
                    }
                }
                for (int i = 0; i < kBagSlots; ++i) {
                    preserved = preserved && bag.normal[i].index == original[i].index &&
                                bag.normal[i].iid == original[i].iid &&
                                bag.normal[i].count == original[i].count &&
                                bag.normal[i].prefix == original[i].prefix &&
                                bag.normal[i].account_bound == original[i].account_bound;
                }
            }
        }
        expect(all_mappings,
               "all 216 cells match the domain oracle for all 64 filters, 64 writable masks and "
               "three scales");
        expect(preserved,
               "all projection combinations preserve instance identity, counts and metadata");
        expect(window.general_source_slot(-1) == -1 && window.general_source_slot(216) == -1 &&
                   !window.filter_active(-1) && !window.filter_active(6),
               "projection and filter queries reject both out-of-range boundaries");
        expect(!window.register_writable(-1) && !window.register_writable(6) &&
                   !window.register_page_has_items(bag, -1) &&
                   !window.register_page_has_items(bag, 6),
               "page queries reject both out-of-range boundaries");
        window.set_register_enabled(2, false);
        expect(window.register_viewable(bag, 2), "an expired occupied page remains viewable");
        window.set_tab(Tab::Quest);
        expect(!window.register_page_has_items(bag, 0) && !window.register_viewable(bag, 2),
               "empty Other pages respect their own storage and writability");
        window.set_register_enabled(2, true);
        expect(window.register_viewable(bag, 2), "writable pages stay viewable without items");
        window.set_tab(Tab::Normal);

        select(window, document, bag, wear, 2);
        window.set_open(false);
        for (int length : {0, 1, 35, 36, 37, 72, 73, 215, 216}) {
            bag.normal.assign(static_cast<std::size_t>(length), {});
            if (length > 0) {
                bag.normal.back() = stack(1, length - 1);
            }
            window.update_pointer({}, bag);
            const auto expected = expected_projection(bag, 2, window.register_enabled());
            bool same = true;
            for (int i = 0; i < kBagSlots; ++i) {
                same = same && window.general_source_slot(i) == expected[i];
            }
            expect(same,
                   "short and empty externally mutable bag vectors never produce nonexistent drop "
                   "targets");
            expect(window.register_page_has_items(bag, 0) == (length > 0 && length <= 36),
                   "page occupancy respects the external bag vector boundary");
        }
        bag.clear();
        bag.normal[0] = stack(1, 0);
        bag.normal[0].count = 0;
        expect(!window.register_page_has_items(bag, 0),
               "a nonzero item definition with zero count cannot make a page occupied");
        bag.normal[10] = stack(1, 10);
        bag.normal[79] = stack(1, 79);
        window.set_register_unlock_expire_ms({});
        window.update_pointer({}, bag);
        expect(window.general_source_slot(0) == 10 && window.general_source_slot(72) == 79,
               "expired pages keep selected items isolated from enabled storage");
        std::array<std::uint64_t, kMaxRegisters> expiry{};
        expiry[2] = std::numeric_limits<std::uint64_t>::max();
        window.set_register_unlock_expire_ms(expiry);
        window.update_pointer({}, bag);
        expect(window.general_source_slot(1) == 79,
               "unlock merges the newly writable page into General order");
        window.set_register_unlock_expire_ms({});
        window.update_pointer({}, bag);
        expect(window.general_source_slot(72) == 79,
               "expiry immediately restores take-out-only page isolation");

        bag.normal[10].index = 91040;
        window.update_pointer({}, bag);
        expect(window.general_source_slot(0) != 10,
               "same-address category edit invalidates the filtered projection");
        bag.normal[10] = stack(1, 10);
        bag.normal[10].count = 0;
        window.update_pointer({}, bag);
        expect(window.general_source_slot(0) == 0,
               "count zero changes occupancy without a filter toggle");
        bag.normal[10] = stack(1, 10);
        window.set_catalog(nullptr);
        window.update_pointer({}, bag);
        expect(window.general_source_slot(0) != 10,
               "removing the catalog reclassifies unknown definitions immediately");
        window.set_catalog(&fixture.catalog);
        window.update_pointer({}, bag);
        expect(window.general_source_slot(0) == 10,
               "restoring the catalog immediately restores classification");
        ItemDb replacement;
        const auto replacement_dir = fixture.root / "replacement";
        std::filesystem::create_directories(replacement_dir);
        std::ofstream(replacement_dir / "91041.json")
            << R"({"index":91041,"class":["general","etc"]})";
        expect(replacement.load(
                   fixture.root.string(), fixture.root.string(), replacement_dir.string()),
               "load an independent replacement catalog");
        window.set_catalog(&replacement);
        window.update_pointer({}, bag);
        expect(window.general_source_slot(0) != 10,
               "replacement catalog category changes invalidate projection");
        window.set_catalog(&fixture.catalog);
        expect(fixture.catalog.load(
                   fixture.root.string(), fixture.root.string(), replacement_dir.string()),
               "reload the current catalog object with changed category data");
        window.update_pointer({}, bag);
        expect(window.general_source_slot(0) != 10,
               "same-address catalog reload invalidates category classification");
        expect(fixture.catalog.load(
                   fixture.root.string(), fixture.root.string(), (fixture.root / "items").string()),
               "restore project-owned synthetic item categories");

        UiAssets ui;
        bag.clear();
        bag.normal[10] = stack(1, 10);
        window.draw(ui, engine, bag);
        expect(window.general_source_slot(0) == 10,
               "closed-window draw still refreshes the projection");
        bag.normal[10] = {};
        bag.normal[20] = stack(1, 20);
        window.set_scale(1);
        window.center_on_screen(1920, 1080);
        window.set_open(true);
        float x, y, w, h;
        // Interactions refresh independently of a draw, including authoritative edits
        // between the previous draw and the click.
        const auto* grid = document.find_widget("bag.0");
        const float target_x = window.x() + (grid->x() + 16) * window.scale();
        const float target_y = window.y() + (grid->y() + 16) * window.scale();
        (void) window.handle_click(target_x, target_y, bag, wear);
        expect(window.general_source_slot(0) == 20 && window.selected() == 20,
               "click after authoritative content replacement resolves the new source instance");
        (void) window.handle_release(target_x, target_y, bag, wear);
        select(window, document, bag, wear, 1);
        bag.normal[20] = stack(0, 20);
        window.update_pointer({}, bag);
        expect(window.grid_slot_rect(20, x, y, w, h),
               "filtered consumable has a visible source cell");
        expect(window.handle_right_click(x + w / 2, y + h / 2, bag) == ClickResult::Consumed &&
                   window.last_consumed_slot() == 20 && bag.normal[20].iid == 1020,
               "filtered right-click addresses the exact instance without optimistic mutation");
        const auto toggle = control(window, document, "filter.weapons");
        window.set_drag_slot(20);
        (void) window.handle_click(toggle.first, toggle.second, bag, wear);
        expect(!window.filter_active(1),
               "captured drags cannot change projection through filter toggles");
        window.set_drag_slot(-1);
        // A blank filtered display cell must target genuinely empty storage.
        bag.normal[21] = stack(1, 21);
        window.update_pointer({}, bag);
        const int blank_source = window.general_source_slot(1);
        expect(blank_source == 0, "excluded occupied weapons do not become writable blank cells");
        expect(window.grid_slot_rect(blank_source, x, y, w, h), "filtered empty target is visible");
        window.set_drag_slot(20);
        (void) window.handle_release(x + w / 2, y + h / 2, bag, wear);
        expect(bag.normal[0].iid == 1020 && bag.normal[21].iid == 1021,
               "filtered drag moves the selected instance without overwriting excluded gear");
        window.set_drag_slot(0);
        const auto trash = control(window, document, "destroy");
        expect(window.handle_release(trash.first, trash.second, bag, wear) ==
                       ClickResult::Destroyed &&
                   window.last_destroyed_iid() == 1020,
               "filtered destroy targets the existing instance identity");
        bag.clear();
        bag.normal[10] = stack(1, 10);
        bag.normal[79] = stack(1, 79);
        select(window, document, bag, wear, 2);
        window.set_register_unlock_expire_ms({});
        window.update_pointer({}, bag);
        const auto* viewport = document.find_widget("general_viewport");
        engine::FrameInput scroll;
        scroll.mouse_x = window.x() + (viewport->x() + 30) * window.scale();
        scroll.mouse_y = window.y() + (viewport->y() + 30) * window.scale();
        scroll.mouse_scroll = -12;
        window.update_pointer(scroll, bag);
        expect(window.grid_slot_rect(79, x, y, w, h),
               "expired selected instance can scroll into view");
        window.set_drag_slot(10);
        (void) window.handle_release(x + w / 2, y + h / 2, bag, wear);
        expect(bag.normal[10].iid == 1010 && bag.normal[79].iid == 1079,
               "filtered interactions cannot deposit into expired storage");
        scroll.mouse_scroll = 12;
        window.update_pointer(scroll, bag);
        expect(window.grid_slot_rect(10, x, y, w, h), "filtered source returns after scrolling up");
        (void) window.handle_click(x + w / 2, y + h / 2, bag, wear);
        (void) window.handle_release(x + w / 2, y + h / 2, bag, wear);
        (void) window.handle_click(x + w / 2, y + h / 2, bag, wear);
        expect(
            window.handle_release(x + w / 2, y + h / 2, bag, wear) == ClickResult::EquipChanged &&
                window.last_equip_iid() == 1010 && bag.normal[10].iid == 1010,
            "filtered double click requests the exact source instance without optimistic mutation");
        const auto sort = control(window, document, "sort");
        expect(window.handle_click(sort.first, sort.second, bag, wear) ==
                       ClickResult::SortRequested &&
                   bag.normal[10].iid == 1010 && bag.normal[0].iid == 0,
               "filtered sorting waits for the authoritative server acknowledgement");
        window.complete_sort(bag, false);
        expect(bag.normal[0].iid == 1010 && bag.normal[79].iid == 1079 &&
                   window.general_source_slot(0) == 0 && window.general_source_slot(72) == 79,
               "sorting refreshes filtered source identities and preserves expired storage");
        window.set_tab(Tab::Quest);
        (void) window.handle_click(toggle.first, toggle.second, bag, wear);
        expect(window.filter_active(1), "Other tab interactions cannot change General filters");
        window.set_tab(Tab::Normal);
        select(window, document, bag, wear, 0);
        expect(window.general_source_slot(215) == 215,
               "disabling the last filter restores direct source identity");
        window.set_wear(nullptr);
    }
}  // namespace

int main(int argc, char** argv) {
    std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
    if (!engine || !engine->init("Optimization coverage", 1920, 1080)) {
        return 1;
    }
    if (argc > 1 && std::string_view(argv[1]) == "--projection-benchmark") {
        benchmark_projection(*engine);
        engine->shutdown();
        return openkal::test::g_failures ? 1 : 0;
    }
    if (argc >= 3 && std::string_view(argv[1]) == "--projection-work-probe") {
        projection_work_probe(
            *engine, argv[2], argc > 3 && std::string_view(argv[3]) == "--require-no-alloc");
        engine->shutdown();
        return openkal::test::g_failures ? 1 : 0;
    }
    if (argc >= 3 && std::string_view(argv[1]) == "--layout-work-probe") {
        const int count = std::atoi(argv[2]);
        expect(count == 256 || count == 1024 || count == 4096, "work probe uses a bounded size");
        if (count == 256 || count == 1024 || count == 4096) {
            layout_work_probe(*engine, count, argc > 3 ? argv[3] : "ascii");
        }
        engine->shutdown();
        return openkal::test::g_failures ? 1 : 0;
    }
    text_layout();
    font_measurement(*engine);
    categories();
    projection_and_interactions(*engine);
    engine->shutdown();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
