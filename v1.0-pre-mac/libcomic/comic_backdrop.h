// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_backdrop.h — portable loader for comic BACKDROP art (the scenery drawn
// behind the character + balloon in a panel). Ports backdrop.cpp's CBackDrop /
// CBackDropArt: a backdrop is a standalone .bmp (4bpp RLE4 in the shipped art)
// loaded into a Dib and drawn, scaled, to fill the panel's drawing area.
//
// The original's CBackDrop::m_bbox and CBackDropArt::m_worldCoords are BOTH the
// default 4860-TWIP square (see backdrop.h / SetBackDropAux), so the source rect
// is the whole bitmap and the destination is the whole panel — i.e. the backdrop
// simply fills the panel, scaled. We reproduce that "fill the panel" intent here
// and leave the scaling to the renderer's drawImage (dest = panel interior).

#ifndef COMIC_BACKDROP_H
#define COMIC_BACKDROP_H

#include <string>
#include <vector>

#include "comic_dib.h"
#include "comic_types.h"

namespace comic {

// A decoded backdrop: an opaque RGBA buffer plus its natural pixel size. The
// app turns rgba() into a backend image (CGImage) and hands the handle to Panel.
class Backdrop {
public:
    // Load "<backdropDir>/<name>.bmp" into a Dib and expand to opaque RGBA.
    // Returns nullptr on any failure (missing file, malformed BMP, empty image).
    // `name` is the base file name without extension (e.g. "field").
    static Backdrop* load(const std::string& backdropDir, const std::string& name);

    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return width_ > 0 && height_ > 0 && !rgba_.empty(); }

    // Top-down RGBA (row-major, width*height*4). Fully opaque — a backdrop is a
    // solid background, so we expand with transparentIndex = -1.
    const std::vector<u8>& rgba() const { return rgba_; }

    // The shipped backdrop names (mirrors comicart/backdrop/*.bmp). "(none)" is
    // NOT included; the app prepends its own no-backdrop entry.
    static std::vector<std::string> availableNames(const std::string& backdropDir);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<u8> rgba_;
};

} // namespace comic

#endif // COMIC_BACKDROP_H
