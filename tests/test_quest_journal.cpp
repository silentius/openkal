/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/QuestJournalWindow.h"
#include "openkal/client/TextWrap.h"
#include "test_util.h"

#include <string_view>

int main() {
    using openkal::client::QuestJournalWindow;
    using openkal::test::expect;

    expect(QuestJournalWindow::format_item_progress("Nou-Woong's Money", 0, 1) ==
               "Nou-Woong's Money 0 / 1",
           "empty progress line");
    expect(QuestJournalWindow::format_item_progress("Nou-Woong's Money", 1, 1) ==
               "Nou-Woong's Money 1 / 1",
           "complete progress line");
    expect(!QuestJournalWindow::item_requirement_met(0, 1), "0/1 is incomplete");
    expect(QuestJournalWindow::item_requirement_met(1, 1), "1/1 is complete");
    expect(QuestJournalWindow::item_requirement_met(2, 1), "over-count is complete");
    expect(!QuestJournalWindow::item_requirement_met(1, 0), "no requirement is not complete");

    {
        auto measure = [](std::string_view s) { return static_cast<float>(s.size()) * 10.0f; };
        const auto words = openkal::client::wrap_text("Bring the old stirrup back", 80.0f, measure);
        expect(words.size() >= 3, "wraps a long quest line on word boundaries");
        expect(words[0] == "Bring", "first wrapped chunk");
        const auto hard = openkal::client::wrap_text("ABCDEFGHIJ", 30.0f, measure);
        expect(hard.size() == 4, "splits an oversized word");
        expect(hard[0] == "ABC" && hard[3] == "J", "hard-wrap pieces");
        const auto kept = openkal::client::wrap_text("one\n\ntwo", 1000.0f, measure);
        expect(kept.size() == 3 && kept[1].empty() && kept[2] == "two", "keeps blank lines");
        const std::string source = "one  two\n\nü😀\n";
        const auto spans = openkal::client::layout_text_lines(source, 50, measure);
        std::string recovered;
        for (std::size_t i = 0; i < spans.size(); ++i) {
            recovered += source.substr(spans[i].start, spans[i].length);
            const auto end = spans[i].start + spans[i].length;
            if (end < source.size() && source[end] == '\n') {
                recovered += '\n';
            }
            expect(end == source.size() || (static_cast<unsigned char>(source[end]) & 0xC0) != 0x80,
                   "multiline layout does not split UTF-8 codepoints");
        }
        expect(recovered == source && spans.back().length == 0,
               "editable layout preserves spaces, blank lines, UTF-8 and a trailing newline");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
