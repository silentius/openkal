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

namespace openkal::engine {
    struct FrameInput;
}

namespace openkal::client {

    /** Removes gameplay controls when a UI control owns keyboard focus.
     * @param input Frame input to mute in place.
     */
    void mute_gameplay_input(engine::FrameInput& input);

    /** Removes pointer and remaining UI controls when an overlay owns the frame.
     * @param input Frame input to mute in place.
     */
    void mute_ui_input(engine::FrameInput& input);

    /** Retains window lifecycle, hover and inspection shortcuts for a read-only view.
     * Camera look is handled separately from the unfiltered frame.
     * @param input Frame input to restrict in place.
     */
    void restrict_read_only_input(engine::FrameInput& input);

}  // namespace openkal::client
