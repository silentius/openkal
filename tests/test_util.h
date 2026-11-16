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

#include <cstdio>
#include <cstdlib>

namespace openkal::test {

    inline int g_failures = 0;

    inline void expect(bool cond, const char* name) {
        if (cond) {
            std::printf("PASS %s\n", name);
        } else {
            std::fprintf(stderr, "FAIL %s\n", name);
            ++g_failures;
        }
    }

}  // namespace openkal::test
