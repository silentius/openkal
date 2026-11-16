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
#include "test_util.h"

int main() {
    using openkal::client::LatinText;
    using openkal::test::expect;

    expect(LatinText::fold("Escort's") == "Escort's", "ASCII apostrophe stays ASCII");
    expect(LatinText::fold("Escort\xE2\x80\x99s") == "Escort's",
           "U+2019 curly apostrophe becomes ASCII apostrophe");
    expect(LatinText::fold("Soldier\xE2\x80\x99s Head") == "Soldier's Head",
           "item name Escort Soldier's Head folds");
    expect(LatinText::fold("\xE2\x80\x9Cquoted\xE2\x80\x9D") == "\"quoted\"",
           "curly double quotes become ASCII");
    expect(LatinText::fold("wait\xE2\x80\xA6") == "wait...", "ellipsis becomes three dots");
    expect(LatinText::fold("a\x92s") == "a's", "lone CP1252 0x92 folds to apostrophe");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
