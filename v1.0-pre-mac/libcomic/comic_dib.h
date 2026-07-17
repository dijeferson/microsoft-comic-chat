// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_dib.h — portable decoder for the Windows DIBs embedded in .avb art.
// Replaces dib.cpp's CDIB (which decoded via GDI structs). Produces an 8bpp
// indexed image + palette, and expands to RGBA. No OS headers.

#ifndef COMIC_DIB_H
#define COMIC_DIB_H

#include <cstdio>
#include <vector>

#include "comic_types.h"

namespace comic {

// A decoded, non-RLE, 8-bit indexed bitmap with its 256-entry palette.
// Scanlines are stored top-down here (already un-flipped from BMP bottom-up),
// width bytes per row (no DWORD padding at this layer — we repack on decode).
class Dib {
public:
    // Load a BMP starting at the current position of `fp`. The .avb stores each
    // pose's drawing/mask/aura as a standalone BMP at a byte offset; caller
    // fseeks first. Returns false on any malformed input (no asserts/crashes).
    bool loadFromFile(std::FILE* fp);

    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return width_ > 0 && height_ > 0; }

    // Palette index at (x,y), top-down. 0 if out of range.
    u8 indexAt(int x, int y) const;

    const RGBA& paletteEntry(int i) const { return palette_[i]; }

    // Expand to a top-down RGBA buffer (row-major, width*height*4 bytes).
    // `transparentIndex` < 0 means fully opaque; otherwise that palette index
    // becomes alpha=0 (matches the DIB's index-based transparency).
    std::vector<u8> toRGBA(int transparentIndex = -1) const;

private:
    int width_ = 0;
    int height_ = 0;
    int bitCount_ = 0;
    RGBA palette_[256];
    std::vector<u8> pixels_; // top-down, one byte (index) per pixel, width_*height_

    // Decoders write into pixels_ (top-down). Source scanlines are the raw BMP
    // rows (bottom-up, DWORD padded).
    void decodePacked(const std::vector<u8>& raw, int srcRowBytes, int bpp);
    void decodeRle8(const std::vector<u8>& raw);
    void decodeRle4(const std::vector<u8>& raw);
};

} // namespace comic

#endif // COMIC_DIB_H
