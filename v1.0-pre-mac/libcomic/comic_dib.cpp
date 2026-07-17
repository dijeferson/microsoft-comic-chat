// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Portable port of dib.cpp's load + ConvertToNonRLE, with palette->RGBA added.
// Reads BMP fields explicitly little-endian (the art originated on x86; we do
// not rely on struct layout / host endianness).

#include "comic_dib.h"

#include <cstring>

namespace comic {

namespace {

// BMP compression constants (from wingdi.h).
constexpr u32 kBI_RGB = 0;
constexpr u32 kBI_RLE8 = 1;
constexpr u32 kBI_RLE4 = 2;

// Little-endian readers over a byte buffer with bounds checks.
bool rd16(const u8* p, size_t n, size_t off, u16& out) {
    if (off + 2 > n) return false;
    out = static_cast<u16>(p[off] | (p[off + 1] << 8));
    return true;
}
bool rd32(const u8* p, size_t n, size_t off, u32& out) {
    if (off + 4 > n) return false;
    out = static_cast<u32>(p[off]) | (static_cast<u32>(p[off + 1]) << 8) |
          (static_cast<u32>(p[off + 2]) << 16) | (static_cast<u32>(p[off + 3]) << 24);
    return true;
}

// DWORD-aligned row width in bytes for a given pixel width at `bpp`.
int rowBytes(int widthPx, int bpp) {
    return ((widthPx * bpp + 31) / 32) * 4;
}

} // namespace

u8 Dib::indexAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
    return pixels_[static_cast<size_t>(y) * width_ + x];
}

// BITMAPFILEHEADER is 14 bytes; BITMAPINFOHEADER is 40. We read the whole file
// tail into memory from the BMP start, then parse offsets ourselves — this
// mirrors dib.cpp which used bfSize/bfOffBits to bound the pixel data.
bool Dib::loadFromFile(std::FILE* fp) {
    long bmpStart = std::ftell(fp);
    if (bmpStart < 0) return false;

    u8 fileHdr[14];
    if (std::fread(fileHdr, 1, 14, fp) != 14) return false;
    // 'BM'
    if (fileHdr[0] != 0x42 || fileHdr[1] != 0x4D) return false;
    u32 bfSize, bfOffBits;
    if (!rd32(fileHdr, 14, 2, bfSize)) return false;
    if (!rd32(fileHdr, 14, 10, bfOffBits)) return false;

    u8 infoHdr[40];
    if (std::fread(infoHdr, 1, 40, fp) != 40) return false;
    u32 biSize, biCompression, biClrUsed;
    i32 biWidth, biHeight;
    u16 biBitCount;
    if (!rd32(infoHdr, 40, 0, biSize)) return false;
    if (biSize != 40) return false; // only Windows BITMAPINFOHEADER DIBs in .avb
    u32 w, h;
    if (!rd32(infoHdr, 40, 4, w) || !rd32(infoHdr, 40, 8, h)) return false;
    biWidth = static_cast<i32>(w);
    biHeight = static_cast<i32>(h);
    if (!rd16(infoHdr, 40, 14, biBitCount)) return false;
    if (!rd32(infoHdr, 40, 16, biCompression)) return false;
    if (!rd32(infoHdr, 40, 32, biClrUsed)) return false;

    if (biWidth <= 0 || biHeight == 0) return false;
    bitCount_ = biBitCount;
    width_ = biWidth;
    height_ = biHeight < 0 ? -biHeight : biHeight; // tolerate top-down source
    bool srcTopDown = biHeight < 0;

    // Color table: count derived from bit depth, capped by biClrUsed.
    int paletteCount = 0;
    if (biBitCount == 1) paletteCount = 2;
    else if (biBitCount == 4) paletteCount = 16;
    else if (biBitCount == 8) paletteCount = 256;
    if (biClrUsed != 0 && static_cast<int>(biClrUsed) < paletteCount)
        paletteCount = static_cast<int>(biClrUsed);
    if (paletteCount == 0) { width_ = height_ = 0; return false; } // MVP: indexed only

    // Palette entries are RGBQUAD: B,G,R,reserved.
    std::vector<u8> palBytes(static_cast<size_t>(paletteCount) * 4);
    if (std::fread(palBytes.data(), 1, palBytes.size(), fp) != palBytes.size()) {
        width_ = height_ = 0; return false;
    }
    for (int i = 0; i < 256; ++i) palette_[i] = RGBA{0, 0, 0, 255};
    for (int i = 0; i < paletteCount; ++i) {
        palette_[i].b = palBytes[i * 4 + 0];
        palette_[i].g = palBytes[i * 4 + 1];
        palette_[i].r = palBytes[i * 4 + 2];
        palette_[i].a = 255;
    }

    // Pixel data: from bmpStart + bfOffBits, length bfSize - bfOffBits.
    if (bfOffBits < 14 + biSize) { width_ = height_ = 0; return false; }
    long rawLen = static_cast<long>(bfSize) - static_cast<long>(bfOffBits);
    if (rawLen <= 0) { width_ = height_ = 0; return false; }
    if (std::fseek(fp, bmpStart + static_cast<long>(bfOffBits), SEEK_SET) != 0) {
        width_ = height_ = 0; return false;
    }
    std::vector<u8> raw(static_cast<size_t>(rawLen));
    if (std::fread(raw.data(), 1, raw.size(), fp) != raw.size()) {
        width_ = height_ = 0; return false;
    }

    pixels_.assign(static_cast<size_t>(width_) * height_, 0);

    if (biCompression == kBI_RGB && (biBitCount == 1 || biBitCount == 4 || biBitCount == 8)) {
        // Comic Chat pose drawings are 1bpp line art; icons/others may be 4/8bpp.
        decodePacked(raw, rowBytes(width_, biBitCount), biBitCount);
    } else if (biCompression == kBI_RLE8 && biBitCount == 8) {
        decodeRle8(raw);
    } else if (biCompression == kBI_RLE4 && biBitCount == 4) {
        decodeRle4(raw);
    } else {
        width_ = height_ = 0;
        return false;
    }

    // If source was top-down we filled top-down already but decoders assume
    // bottom-up source; flip back for the top-down case.
    if (srcTopDown) {
        for (int y = 0; y < height_ / 2; ++y) {
            for (int x = 0; x < width_; ++x)
                std::swap(pixels_[static_cast<size_t>(y) * width_ + x],
                          pixels_[static_cast<size_t>(height_ - 1 - y) * width_ + x]);
        }
    }
    return true;
}

// Uncompressed 1/4/8bpp: BMP rows are bottom-up and DWORD padded. Each pixel is
// an index into the palette (packed high-bit-first within a byte). Write
// top-down, one index byte per pixel.
void Dib::decodePacked(const std::vector<u8>& raw, int srcRowBytes, int bpp) {
    for (int y = 0; y < height_; ++y) {
        int srcRow = height_ - 1 - y; // bottom-up -> top-down
        size_t base = static_cast<size_t>(srcRow) * srcRowBytes;
        for (int x = 0; x < width_; ++x) {
            u8 idx = 0;
            if (bpp == 8) {
                size_t si = base + x;
                if (si < raw.size()) idx = raw[si];
            } else if (bpp == 4) {
                size_t si = base + (x >> 1);
                if (si < raw.size()) idx = (x & 1) ? (raw[si] & 0x0F) : (raw[si] >> 4);
            } else { // bpp == 1
                size_t si = base + (x >> 3);
                if (si < raw.size()) idx = (raw[si] >> (7 - (x & 7))) & 0x01;
            }
            pixels_[static_cast<size_t>(y) * width_ + x] = idx;
        }
    }
}

// RLE8 -> indexed, mirroring CDIB::Convert8ToNonRLE. The original decoded into
// a bottom-up DWORD-padded buffer; here we decode directly into a top-down
// tight buffer, tracking the destination cursor (dx,dy). Absolute-mode delta=255
// fill from the original is reproduced as index 255 (its transparent fill).
void Dib::decodeRle8(const std::vector<u8>& raw) {
    const u8* p = raw.data();
    const u8* end = p + raw.size();
    // Destination cursor in bottom-up space (row 0 = bottom), matching BMP.
    int dx = 0, dyBottom = 0;

    auto put = [&](u8 v) {
        int y = height_ - 1 - dyBottom; // convert to top-down
        if (dx >= 0 && dx < width_ && y >= 0 && y < height_)
            pixels_[static_cast<size_t>(y) * width_ + dx] = v;
        ++dx;
    };

    while (p < end) {
        u8 count = *p++;
        if (count > 0) {
            if (p >= end) break;
            u8 val = *p++;
            for (int i = 0; i < count; ++i) put(val);
        } else {
            if (p >= end) break;
            u8 code = *p++;
            if (code == 0) {            // end of line
                dx = 0;
                ++dyBottom;
            } else if (code == 1) {     // end of bitmap
                break;
            } else if (code == 2) {     // delta
                if (p + 2 > end) break;
                u8 hx = *p++;
                u8 hy = *p++;
                dx += hx;
                dyBottom += hy;
            } else {                    // absolute run of `code` literals
                for (int i = 0; i < code; ++i) {
                    if (p >= end) break;
                    put(*p++);
                }
                if ((code & 1) != 0 && p < end) ++p; // pad to word boundary
            }
        }
    }
}

// RLE4 -> indexed, mirroring decodeRle8 but with 4-bit nibble pixel data
// (packed two-per-byte, high nibble first). Encoded runs alternate the two
// nibbles of the value byte (high first). Absolute runs read ceil(count/2)
// bytes and pad the read position to a 16-bit WORD boundary. As in decodeRle8,
// each emitted 4-bit value becomes its own full index byte (no re-packing).
void Dib::decodeRle4(const std::vector<u8>& raw) {
    const u8* p = raw.data();
    const u8* end = p + raw.size();
    // Destination cursor in bottom-up space (row 0 = bottom), matching BMP.
    int dx = 0, dyBottom = 0;

    auto put = [&](u8 v) {
        int y = height_ - 1 - dyBottom; // convert to top-down
        if (dx >= 0 && dx < width_ && y >= 0 && y < height_)
            pixels_[static_cast<size_t>(y) * width_ + dx] = v;
        ++dx;
    };

    while (p < end) {
        u8 count = *p++;
        if (count > 0) {
            if (p >= end) break;
            u8 val = *p++;
            u8 hi = (val >> 4) & 0x0F;
            u8 lo = val & 0x0F;
            for (int i = 0; i < count; ++i) put((i & 1) ? lo : hi);
        } else {
            if (p >= end) break;
            u8 code = *p++;
            if (code == 0) {            // end of line
                dx = 0;
                ++dyBottom;
            } else if (code == 1) {     // end of bitmap
                break;
            } else if (code == 2) {     // delta
                if (p + 2 > end) break;
                u8 hx = *p++;
                u8 hy = *p++;
                dx += hx;
                dyBottom += hy;
            } else {                    // absolute run of `code` nibble literals
                int dataBytes = (code + 1) / 2; // ceil(code/2)
                for (int i = 0; i < code; ++i) {
                    if ((i & 1) == 0) {         // start of a new source byte
                        if (p >= end) break;
                        u8 b = *p++;
                        put((b >> 4) & 0x0F);   // high nibble first
                    } else {
                        put(p[-1] & 0x0F);      // low nibble of the byte just read
                    }
                }
                if ((dataBytes & 1) != 0 && p < end) ++p; // pad to word boundary
            }
        }
    }
}

std::vector<u8> Dib::toRGBA(int transparentIndex) const {
    std::vector<u8> out(static_cast<size_t>(width_) * height_ * 4, 0);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            u8 idx = pixels_[static_cast<size_t>(y) * width_ + x];
            const RGBA& c = palette_[idx];
            size_t o = (static_cast<size_t>(y) * width_ + x) * 4;
            out[o + 0] = c.r;
            out[o + 1] = c.g;
            out[o + 2] = c.b;
            out[o + 3] = (transparentIndex >= 0 && idx == transparentIndex) ? 0 : 255;
        }
    }
    return out;
}

} // namespace comic
