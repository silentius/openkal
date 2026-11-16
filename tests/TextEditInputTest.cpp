/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/engine/engine.h"
#include "test_util.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <memory>

int main() {
    using openkal::test::expect;
    std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
    if (!engine->init("Text edit input verification", 320, 180)) {
        return 77;
    }
    auto* window = glfwGetCurrentContext();
    const auto callback = glfwSetKeyCallback(window, nullptr);
    glfwSetKeyCallback(window, callback);
    expect(callback != nullptr, "engine registers native text-edit key events");
    if (!callback) {
        engine->shutdown();
        return 1;
    }
    openkal::engine::FrameInput frame;
    (void) engine->poll(frame);
    callback(window, GLFW_KEY_BACKSPACE, 0, GLFW_PRESS, 0);
    expect(engine->poll(frame) && frame.key_backspace_pressed,
           "initial backspace press acts immediately");
    for (int i = 0; i < 5; ++i) {
        expect(engine->poll(frame) && !frame.key_backspace_pressed,
               "polling alone never deletes again before the OS repeat delay");
    }
    for (int i = 0; i < 3; ++i) {
        callback(window, GLFW_KEY_BACKSPACE, 0, GLFW_REPEAT, 0);
        expect(engine->poll(frame) && frame.key_backspace_pressed,
               "each OS repeat produces a text-edit action");
        expect(engine->poll(frame) && !frame.key_backspace_pressed,
               "repeat is consumed exactly once and never deletes every frame");
    }
    callback(window, GLFW_KEY_BACKSPACE, 0, GLFW_RELEASE, 0);
    expect(engine->poll(frame) && !frame.key_backspace_pressed, "release stops deletion");
    callback(window, GLFW_KEY_BACKSPACE, 0, GLFW_PRESS, 0);
    callback(window, GLFW_KEY_BACKSPACE, 0, GLFW_RELEASE, 0);
    expect(engine->poll(frame) && frame.key_backspace_pressed,
           "a quick press and release between frames still deletes once");
    callback(window, GLFW_KEY_O, 0, GLFW_REPEAT, 0);
    expect(engine->poll(frame) && !frame.key_backspace_pressed && !frame.key_o_pressed,
           "text editing repeats do not retrigger gameplay hotkeys");
    engine->shutdown();
    return openkal::test::g_failures ? 1 : 0;
}
