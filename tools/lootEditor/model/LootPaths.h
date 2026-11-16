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

#include <string>

/** Returns the HyperText directory for a client data root.
 * @param data_root Client data root.
 * @return Absolute HyperText path, or empty when absent.
 */
[[nodiscard]] std::string hypertext_dir_for(const std::string& data_root);
