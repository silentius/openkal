/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SkinPreview.h"
#include "openkal/platform/Json.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char** argv) {
    using openkal::platform::Json;
    if (argc != 2) {
        std::cerr << "Usage: skin_preview_renderer MANIFEST.json\n";
        return 2;
    }
    try {
        std::ifstream input(argv[1]);
        if (!input) {
            throw std::runtime_error("Cannot read preview manifest");
        }
        const std::string text{std::istreambuf_iterator<char>(input), {}};
        const auto manifest = Json::parse(text);
        const int width = manifest.at("width").as_int(384);
        const int height = manifest.at("height").as_int(512);
        if (!manifest.at("jobs").is_array()) {
            throw std::runtime_error("Preview jobs must be an array");
        }
        openkal::tools::SkinPreview preview;
        if (!preview.initialize()) {
            throw std::runtime_error(preview.error());
        }
        Json results;
        results.set_type(Json::Type::Array);
        for (const auto& job : manifest.at("jobs").array()) {
            std::vector<std::string> paths;
            for (const auto& path : job.at("paths").array()) {
                paths.push_back(path.as_string());
            }
            openkal::tools::SkinPreviewPose pose;
            pose.skeleton = job.at("skeleton").as_string();
            pose.motion = job.at("motion").as_string();
            pose.time_ms = job.at("time_ms").as_float();
            for (const auto& bone : job.at("attachments").array()) {
                pose.attachments.push_back(bone.as_int(-1));
            }
            const bool ok = preview.load(paths, pose) &&
                            preview.capture(job.at("front").as_string(), false, width, height) &&
                            preview.capture(job.at("back").as_string(), true, width, height);
            Json result;
            result.set_type(Json::Type::Object);
            result.object()["id"] = job.at("id");
            result.object()["rendered"].set_boolean(ok);
            result.object()["error"].set_type(Json::Type::String);
            result.object()["error"].string() = ok ? "" : preview.error();
            results.array().push_back(std::move(result));
        }
        std::cout << Json::stringify(results, 0) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
    return 0;
}
