/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MixVisual.h"

#include <cstdio>

namespace openkal::client {

    int MixVisual::stone_type(int mix_type) {
        return mix_type & 0x0F;
    }

    int MixVisual::mix_level(int mix_type) {
        return (mix_type >> 4) & 0x03;
    }

    int MixVisual::magic_stone(int mix_type) {
        const int type = stone_type(mix_type);
        const int level = mix_level(mix_type);
        if (type < 1 || type > 11) {
            return 0;
        }
        const int base = 1 << (type - 1);
        if (level >= 2) {
            return base + 10;
        }
        return base;
    }

    std::string MixVisual::particle_name(char job_letter, int magic) {
        if (magic <= 0) {
            return {};
        }
        char buf[16];
        std::snprintf(buf, sizeof(buf), "e%c_%04d", job_letter, magic);
        return buf;
    }

    std::string MixVisual::thief_fx_name(int stone) {
        switch (stone) {
        case 1:
            return "davi_sword_dark";
        case 2:
            return "davi_sword_holy";
        case 3:
            return "davi_sword_red";
        case 4:
            return "davi_sword_ice";
        case 5:
            return "davi_sword_lightning";
        case 6:
            return "davi_sword_poison";
        case 7:
            return "davi_sword_yellow";
        case 8:
            return "davi_sword_strenght";
        case 9:
            return "davi_sword_technique";
        case 10:
            return "davi_sword_mystery";
        case 11:
            return "davi_sword_blood";
        default:
            return {};
        }
    }

}  // namespace openkal::client
