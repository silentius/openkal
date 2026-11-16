/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientInput.h"

#include "openkal/engine/engine.h"

namespace openkal::client {

    void restrict_read_only_input(engine::FrameInput& input) {
        const auto original = input;
        input = {};
        input.quit = original.quit;
        input.width = original.width;
        input.height = original.height;
        input.dt = original.dt;
        input.mouse_x = original.mouse_x;
        input.mouse_y = original.mouse_y;
        input.mouse_scroll = original.mouse_scroll;
        input.key_escape = original.key_escape;
        input.key_escape_pressed = original.key_escape_pressed;
        input.key_i_pressed = original.key_i_pressed;
        input.key_c_pressed = original.key_c_pressed;
        input.key_k_pressed = original.key_k_pressed;
        input.key_q_pressed = original.key_q_pressed;
        input.key_m_pressed = original.key_m_pressed;
        input.key_p_pressed = original.key_p_pressed;
    }

    void mute_gameplay_input(engine::FrameInput& input) {
        input.key_w = false;
        input.key_a = false;
        input.key_s = false;
        input.key_d = false;
        input.key_space = false;
        input.key_space_pressed = false;
        input.key_f = false;
        input.key_f_pressed = false;
        input.key_c = false;
        input.key_c_pressed = false;
        input.key_i = false;
        input.key_i_pressed = false;
        input.key_k = false;
        input.key_k_pressed = false;
        input.key_p = false;
        input.key_p_pressed = false;
        input.key_o = false;
        input.key_o_pressed = false;
        input.key_g = false;
        input.key_g_pressed = false;
        input.key_q = false;
        input.key_q_pressed = false;
        input.key_z = false;
        input.key_z_pressed = false;
        input.key_x = false;
        input.key_x_pressed = false;
        input.key_e = false;
        input.key_e_pressed = false;
        input.key_h = false;
        input.key_h_pressed = false;
        input.key_v = false;
        input.key_v_pressed = false;
        input.key_l = false;
        input.key_l_pressed = false;
        input.key_m = false;
        input.key_m_pressed = false;
        input.key_r = false;
        input.key_r_pressed = false;
        input.key_s_pressed = false;
        input.key_left = false;
        input.key_up = false;
        input.key_down = false;
        input.key_right = false;
        for (int digit = 0; digit < 10; ++digit) {
            input.key_digit[digit] = false;
            input.key_digit_pressed[digit] = false;
        }
    }

    void mute_ui_input(engine::FrameInput& input) {
        mute_gameplay_input(input);
        input.mouse_left = false;
        input.mouse_right = false;
        input.mouse_middle = false;
        input.mouse_left_clicked = false;
        input.mouse_right_clicked = false;
        input.mouse_middle_clicked = false;
        input.mouse_dx = 0.0f;
        input.mouse_dy = 0.0f;
        input.mouse_scroll = 0.0f;
        input.key_escape = false;
        input.key_escape_pressed = false;
        input.key_enter = false;
        input.key_enter_pressed = false;
        input.text_char_count = 0;
    }

}  // namespace openkal::client
