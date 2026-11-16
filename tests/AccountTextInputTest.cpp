/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/AccountTextInput.h"

#include "openkal/server/AccountPassword.h"
#include "test_util.h"

int main() {
    using openkal::client::AccountTextInput;
    using openkal::server::valid_account_password;
    using openkal::test::expect;

    AccountTextInput field;
    expect(field.insert("short", 128) && !valid_account_password(field.value()),
           "short passwords remain editable while failing live server validation");
    field.select_all();
    expect(field.selected() && field.insert("A pasted password", 128) &&
               field.value() == "A pasted password" && !field.selected() &&
               valid_account_password(field.value()),
           "paste replaces select-all and uses the server password policy");
    const auto original = field.value();
    expect(!field.insert(std::string(129, 'x'), 128) && field.value() == original,
           "oversized clipboard text is rejected without truncation");
    field.select_all();
    for (const auto& invalid : {std::string("password\nwith newline"),
                                std::string("password\twith tab"),
                                std::string("password\0with null", 18),
                                std::string("password\x7fwith delete")}) {
        expect(!field.insert(invalid, 128) && field.value() == original && field.selected(),
               "invalid paste preserves both text and selection");
    }
    expect(field.insert("", 128) && field.value() == original && field.selected(),
           "an empty clipboard does not delete selected text");
    field.backspace();
    expect(field.value().empty() && !field.selected(), "deleting select-all clears the field");

    expect(field.insert(std::string(11, 'a'), 128) && !valid_account_password(field.value()) &&
               field.insert("b", 128) && valid_account_password(field.value()),
           "validation updates at the exact minimum boundary");
    field.select_all();
    expect(field.insert(std::string(128, 'x'), 128) && valid_account_password(field.value()),
           "maximum password length is accepted");
    expect(!field.insert("x", 128) && field.value().size() == 128,
           "typing at the byte limit leaves the accepted password intact");
    field.select_all();
    expect(field.insert("replacement", 128) && field.value() == "replacement",
           "select-all replacement is bounded by the replacement size");
    field.select_all();
    field.deselect();
    field.backspace();
    expect(field.value() == "replacemen", "deselection restores append/backspace editing");

    field.clear();
    expect(field.insert(u8"A password ä", 128) && valid_account_password(field.value()),
           "pasted UTF-8 uses the same byte policy as the server");
    field.backspace();
    expect(field.value() == "A password ", "backspace deletes the whole UTF-8 character");
    field.clear();
    expect(field.insert(std::string(127, 'x'), 128) && !field.insert(u8"ä", 128) &&
               field.value().size() == 127,
           "multibyte clipboard text is never split at the byte limit");
    return openkal::test::g_failures ? 1 : 0;
}
