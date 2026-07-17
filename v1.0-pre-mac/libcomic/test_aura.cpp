// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// test_aura — assertions for compositeAura (the nimbus/aura glow path). Builds
// synthetic 1bpp aura DIBs in memory (matching the .avb art: index 0 = black
// ink, index 1 = white), decodes them through Dib, and checks that DARK aura
// pixels paint opaque white into the canvas (the MERGEPAINT glow intent) while
// WHITE aura pixels leave the canvas untouched.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "comic_compose.h"
#include "comic_dib.h"

using namespace comic;

namespace {

// Little-endian appenders.
void put16(std::vector<u8>& v, u16 x) { v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF); }
void put32(std::vector<u8>& v, u32 x) {
    v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF);
    v.push_back((x >> 16) & 0xFF); v.push_back((x >> 24) & 0xFF);
}

// Build a minimal 1bpp BMP (bottom-up, DWORD-padded) with palette 0=black,
// 1=white. `allDark` true => every pixel is index 0; false => every pixel is
// index 1. width x height small. Returns the BMP bytes.
std::vector<u8> makeAuraBmp(int width, int height, bool allDark) {
    const int rowBytes = ((width * 1 + 31) / 32) * 4;
    const u32 pixOff = 14 + 40 + 2 * 4;
    const u32 pixSize = static_cast<u32>(rowBytes) * height;
    std::vector<u8> bmp;
    // BITMAPFILEHEADER
    bmp.push_back('B'); bmp.push_back('M');
    put32(bmp, pixOff + pixSize); // bfSize
    put16(bmp, 0); put16(bmp, 0); // reserved
    put32(bmp, pixOff);           // bfOffBits
    // BITMAPINFOHEADER
    put32(bmp, 40);                       // biSize
    put32(bmp, static_cast<u32>(width));  // biWidth
    put32(bmp, static_cast<u32>(height)); // biHeight (bottom-up)
    put16(bmp, 1);                        // biPlanes
    put16(bmp, 1);                        // biBitCount
    put32(bmp, 0);                        // biCompression = BI_RGB
    put32(bmp, pixSize);                  // biSizeImage
    put32(bmp, 0); put32(bmp, 0);         // pels/meter
    put32(bmp, 2);                        // biClrUsed
    put32(bmp, 2);                        // biClrImportant
    // Palette (RGBQUAD: B,G,R,reserved): index0 = black, index1 = white.
    bmp.push_back(0); bmp.push_back(0); bmp.push_back(0); bmp.push_back(0);
    bmp.push_back(255); bmp.push_back(255); bmp.push_back(255); bmp.push_back(0);
    // Pixels: allDark => all bits 0 (index 0); else all bits 1 (index 1).
    u8 fill = allDark ? 0x00 : 0xFF;
    for (int y = 0; y < height; ++y)
        for (int b = 0; b < rowBytes; ++b) bmp.push_back(fill);
    return bmp;
}

Dib loadBmp(const std::vector<u8>& bmp) {
    // Write to a temp file and load through the real Dib decoder.
    char tmpl[] = "build/aura_test_XXXXXX";
    int fd = mkstemp(tmpl);
    assert(fd >= 0);
    std::FILE* fp = fdopen(fd, "wb+");
    assert(fp);
    assert(std::fwrite(bmp.data(), 1, bmp.size(), fp) == bmp.size());
    std::rewind(fp);
    Dib d;
    bool ok = d.loadFromFile(fp);
    std::fclose(fp);
    std::remove(tmpl);
    assert(ok);
    return d;
}

} // namespace

int main() {
    const int W = 5, H = 4;

    // 1) Invalid aura is a no-op.
    {
        std::vector<u8> canvas(static_cast<size_t>(W) * H * 4, 100);
        Dib empty;
        compositeAura(canvas, W, H, 0, 0, empty);
        for (u8 v : canvas) assert(v == 100);
    }

    // 2) All-DARK aura paints opaque white everywhere it covers.
    {
        Dib aura = loadBmp(makeAuraBmp(W, H, /*allDark=*/true));
        assert(aura.valid() && aura.width() == W && aura.height() == H);
        std::vector<u8> canvas(static_cast<size_t>(W) * H * 4, 40); // gray, a=40
        compositeAura(canvas, W, H, 0, 0, aura);
        for (int i = 0; i < W * H; ++i) {
            assert(canvas[i * 4 + 0] == 255);
            assert(canvas[i * 4 + 1] == 255);
            assert(canvas[i * 4 + 2] == 255);
            assert(canvas[i * 4 + 3] == 255);
        }
    }

    // 3) All-WHITE aura leaves the canvas untouched.
    {
        Dib aura = loadBmp(makeAuraBmp(W, H, /*allDark=*/false));
        assert(aura.valid());
        std::vector<u8> canvas(static_cast<size_t>(W) * H * 4, 40);
        compositeAura(canvas, W, H, 0, 0, aura);
        for (u8 v : canvas) assert(v == 40);
    }

    // 4) Dest offset + canvas clipping: aura at (2,1) on a larger canvas only
    //    touches the covered region; out-of-bounds writes are skipped safely.
    {
        const int CW = 8, CH = 6;
        Dib aura = loadBmp(makeAuraBmp(W, H, /*allDark=*/true));
        std::vector<u8> canvas(static_cast<size_t>(CW) * CH * 4, 40);
        compositeAura(canvas, CW, CH, 2, 1, aura);
        long white = 0;
        for (int y = 0; y < CH; ++y)
            for (int x = 0; x < CW; ++x) {
                size_t o = (static_cast<size_t>(y) * CW + x) * 4;
                bool isWhite = canvas[o] == 255 && canvas[o + 1] == 255 &&
                               canvas[o + 2] == 255 && canvas[o + 3] == 255;
                bool inRect = (x >= 2 && x < 2 + W && y >= 1 && y < 1 + H);
                assert(isWhite == inRect); // exactly the aura rect turned white
                if (isWhite) ++white;
            }
        assert(white == static_cast<long>(W) * H);
    }

    std::printf("test_aura OK\n");
    return 0;
}
