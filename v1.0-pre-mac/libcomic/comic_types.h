// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_types.h — portable replacements for the Win32/MFC types the original
// Comic Chat engine used. No OS headers. See the macOS port design + source-map
// docs under docs/superpowers/specs/.

#ifndef COMIC_TYPES_H
#define COMIC_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace comic {

// --- Fixed-width scalar aliases (were Win32 typedefs) ---------------------
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using i16 = std::int16_t;
using i32 = std::int32_t;

// --- Geometry -------------------------------------------------------------
// NOTE (risk #1, Y-up/TWIP): the engine uses integer POINT/RECT in its own
// coordinate space. We deliberately do NOT map these to CGPoint/CGRect in the
// core; the renderer applies the TWIP->points + Y-flip transform at the seam.
struct Point {
    long x = 0;
    long y = 0;

    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return !(*this == o); }
};

struct Rect {
    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;

    long width() const { return right - left; }
    long height() const { return bottom - top; }
};

struct Size {
    int cx = 0;
    int cy = 0;
};

// Straight RGBA, 8 bits/channel, NON-premultiplied at this layer. The renderer
// decides premultiplication. Alpha folds in the DIB palette-index transparency.
struct RGBA {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    u8 a = 255;
};

} // namespace comic

#endif // COMIC_TYPES_H
