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

// Common lifecycle interface for engine backends.
// Game code depends only on these headers ? never on a concrete backend.

namespace openkal::engine {

    class IEngine {
      public:
        virtual ~IEngine() = default;
        virtual bool init() = 0;
        virtual void shutdown() = 0;
        virtual void begin_frame() = 0;
        virtual void end_frame() = 0;
    };

}  // namespace openkal::engine
