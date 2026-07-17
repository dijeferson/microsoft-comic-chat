// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "comic_compose.h"

namespace comic {

void stampPart(std::vector<u8>& canvas, int canvasW, int canvasH,
               int destX, int destY,
               const Dib& drawing, const Dib* mask,
               bool maskInsideIsHigh, int bgIndex) {
    if (!drawing.valid()) return;
    int w = drawing.width();
    int h = drawing.height();

    // The background color the no-mask path treats as transparent. The original
    // CBodySingle::DrawBody blits the pose with SRCAND over a WHITE panel, so
    // pure-white source pixels vanish. We key on that color rather than a fixed
    // palette index because the 1-bpp line-art poses store white at index 0 for
    // some characters (connor, tux) and at index 1 for others (rainbow,
    // pedagog); a plain `idx != bgIndex` test inverts the silhouette for the
    // latter and blanks the whole body. If the drawing's background index is
    // not pure white we fall back to that index's color (keeps the path general
    // for any future non-white background).
    const RGBA& bg = drawing.paletteEntry(bgIndex);
    RGBA bgColor = (bg.r == 255 && bg.g == 255 && bg.b == 255) ? bg : RGBA{255, 255, 255, 255};
    for (int y = 0; y < h; ++y) {
        int cy = destY + y;
        if (cy < 0 || cy >= canvasH) continue;
        for (int x = 0; x < w; ++x) {
            int cx = destX + x;
            if (cx < 0 || cx >= canvasW) continue;

            u8 idx = drawing.indexAt(x, y);
            const RGBA& c = drawing.paletteEntry(idx);

            bool opaque;
            if (mask && mask->valid() && x < mask->width() && y < mask->height()) {
                u8 m = mask->indexAt(x, y);
                bool high = (m != 0);
                opaque = maskInsideIsHigh ? high : !high;
            } else {
                // No mask: a pixel is transparent iff it matches the panel
                // background color the original SRCAND blit would let through.
                opaque = !(c.r == bgColor.r && c.g == bgColor.g && c.b == bgColor.b);
            }
            if (!opaque) continue;

            size_t o = (static_cast<size_t>(cy) * canvasW + cx) * 4;
            canvas[o + 0] = c.r;
            canvas[o + 1] = c.g;
            canvas[o + 2] = c.b;
            canvas[o + 3] = 255;
        }
    }
}

void compositeAura(std::vector<u8>& canvas, int canvasW, int canvasH,
                   int destX, int destY, const Dib& aura) {
    if (!aura.valid()) return;
    int w = aura.width();
    int h = aura.height();
    for (int y = 0; y < h; ++y) {
        int cy = destY + y;
        if (cy < 0 || cy >= canvasH) continue;
        for (int x = 0; x < w; ++x) {
            int cx = destX + x;
            if (cx < 0 || cx >= canvasW) continue;

            // The aura is a 1-bpp black/white blob. Under MERGEPAINT the DARK
            // pixels invert to white and OR into the destination (the glow);
            // WHITE pixels leave the destination untouched. Key on the palette
            // color rather than a fixed index because characters store the dark
            // ink at index 0 (connor) or index 1 (rainbow) — an `idx != 0` test
            // would invert the glow for half of them.
            const RGBA& c = aura.paletteEntry(aura.indexAt(x, y));
            bool dark = (c.r < 128 && c.g < 128 && c.b < 128);
            if (!dark) continue;

            size_t o = (static_cast<size_t>(cy) * canvasW + cx) * 4;
            canvas[o + 0] = 255;
            canvas[o + 1] = 255;
            canvas[o + 2] = 255;
            canvas[o + 3] = 255;
        }
    }
}

} // namespace comic
