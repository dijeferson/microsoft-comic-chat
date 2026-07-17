// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Portable port of backdrop.cpp's backdrop load path (BackDropArtFromBackID +
// CBackDrop::Draw's "fill the panel, scaled" intent). Loading is Dib::load +
// toRGBA(-1) for a fully-opaque background. Scaling happens at draw time when
// the renderer blits rgba() into the panel-interior dest rect.

#include "comic_backdrop.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace comic {

Backdrop* Backdrop::load(const std::string& backdropDir, const std::string& name) {
    std::string path = backdropDir + "/" + name + ".bmp";
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return nullptr;

    Dib dib;
    bool ok = dib.loadFromFile(fp);
    std::fclose(fp);
    if (!ok || !dib.valid()) return nullptr;

    auto* bd = new Backdrop();
    bd->width_ = dib.width();
    bd->height_ = dib.height();
    bd->rgba_ = dib.toRGBA(-1); // opaque: a backdrop is a solid background
    if (!bd->valid()) {
        delete bd;
        return nullptr;
    }
    return bd;
}

std::vector<std::string> Backdrop::availableNames(const std::string& backdropDir) {
    std::vector<std::string> names;
    std::error_code ec;
    std::filesystem::directory_iterator it(backdropDir, ec), end;
    if (!ec) {
        for (; it != end; it.increment(ec)) {
            if (ec) break;
            const auto& p = it->path();
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (ext == ".bmp") names.push_back(p.stem().string());
        }
    }
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        // Fall back to the shipped set if the directory couldn't be scanned.
        names = {"field", "pastoral", "room8bs"};
    }
    return names;
}

} // namespace comic
