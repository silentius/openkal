/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SkinAssetInspection.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char** argv) {
    using openkal::platform::Json;
    if (argc != 2) {
        std::cerr << "Usage: skin_asset_probe MANIFEST.json\n";
        return 2;
    }
    try {
        std::ifstream input(argv[1]);
        if (!input) {
            throw std::runtime_error("Cannot read probe manifest");
        }
        const std::string text{std::istreambuf_iterator<char>(input), {}};
        const auto manifest = Json::parse(text);
        if (!manifest.is_array()) {
            throw std::runtime_error("Probe manifest must be an array");
        }
        Json output;
        output.set_type(Json::Type::Array);
        for (const auto& request : manifest.array()) {
            const auto path = request.at("path").as_string();
            openkal::terrain::GbModel model;
            const bool loaded = !path.empty() && openkal::terrain::load_gb(path, model);
            auto result = openkal::tools::inspect_skin_asset(model, path);
            result.object()["key"] = request.at("key");
            result.object()["loaded"].set_boolean(loaded);
            output.array().push_back(std::move(result));
        }
        std::cout << Json::stringify(output, 0) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
    return 0;
}
