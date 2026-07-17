// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "png_writer.h"

#include <cstdio>
#include <cstring>

#include <zlib.h>

namespace comic {

namespace {

void put32be(std::vector<u8>& v, u32 x) {
    v.push_back(u8(x >> 24)); v.push_back(u8(x >> 16));
    v.push_back(u8(x >> 8)); v.push_back(u8(x));
}

void writeChunk(std::vector<u8>& out, const char* type, const std::vector<u8>& data) {
    put32be(out, static_cast<u32>(data.size()));
    size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, out.data() + crcStart, static_cast<uInt>(4 + data.size()));
    put32be(out, static_cast<u32>(crc));
}

} // namespace

bool writePng(const std::string& path, const std::vector<u8>& rgba, int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (rgba.size() < static_cast<size_t>(width) * height * 4) return false;

    // Raw image with per-scanline filter byte (0 = none).
    std::vector<u8> raw;
    raw.reserve(static_cast<size_t>(height) * (1 + width * 4));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const u8* row = rgba.data() + static_cast<size_t>(y) * width * 4;
        raw.insert(raw.end(), row, row + static_cast<size_t>(width) * 4);
    }

    uLongf compLen = compressBound(static_cast<uLong>(raw.size()));
    std::vector<u8> comp(compLen);
    if (compress2(comp.data(), &compLen, raw.data(), static_cast<uLong>(raw.size()),
                  Z_BEST_COMPRESSION) != Z_OK)
        return false;
    comp.resize(compLen);

    std::vector<u8> out;
    const u8 sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);

    std::vector<u8> ihdr;
    put32be(ihdr, static_cast<u32>(width));
    put32be(ihdr, static_cast<u32>(height));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // color type RGBA
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    writeChunk(out, "IHDR", ihdr);
    writeChunk(out, "IDAT", comp);
    writeChunk(out, "IEND", {});

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    bool ok = std::fwrite(out.data(), 1, out.size(), fp) == out.size();
    std::fclose(fp);
    return ok;
}

} // namespace comic
