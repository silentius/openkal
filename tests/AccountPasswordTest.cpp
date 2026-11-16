/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/AccountPassword.h"

#include "test_util.h"

int main() {
    using namespace openkal::server;
    using openkal::test::expect;
    expect(normalize_account_name("Tester_123") == "tester_123", "normalize account username");
    expect(normalize_account_name("x").empty() && normalize_account_name("name\n").empty(),
           "reject invalid usernames");
    expect(normalize_account_email("Tester+Dev@Example.Test") == "tester+dev@example.test",
           "canonical email policy");
    for (const char* value : {"missing-at",
                              "@example.test",
                              "a@-example.test",
                              "a@example..test",
                              "a..b@example.test",
                              "a@example.test\n"}) {
        expect(normalize_account_email(value).empty(), "reject malformed email");
    }
    expect(!valid_account_password("short") && !valid_account_password(std::string(129, 'x')) &&
               !valid_account_password("bad\npassword is long"),
           "enforce password bounds");
    const std::string password = "A synthetic test password";
    const auto first = hash_account_password(password), second = hash_account_password(password);
    expect(!first.empty() && first != second, "generate unique random salts");
    expect(verify_account_password(password, first), "verify salted password");
    expect(!verify_account_password("A different test password", first),
           "reject incorrect password");
    expect(!verify_account_password(password, password) &&
               !verify_account_password(password, first.substr(1)),
           "reject plaintext and malformed hash formats");
    return openkal::test::g_failures ? 1 : 0;
}
