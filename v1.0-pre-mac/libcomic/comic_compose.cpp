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
    for (int y = 0; y < h; ++y) {
        int cy = destY + y;
        if (cy < 0 || cy >= canvasH) continue;
        for (int x = 0; x < w; ++x) {
            int cx = destX + x;
            if (cx < 0 || cx >= canvasW) continue;

            bool opaque;
            if (mask && mask->valid() && x < mask->width() && y < mask->height()) {
                u8 m = mask->indexAt(x, y);
                bool high = (m != 0);
                opaque = maskInsideIsHigh ? high : !high;
            } else {
                opaque = drawing.indexAt(x, y) != bgIndex;
            }
            if (!opaque) continue;

            const RGBA& c = drawing.paletteEntry(drawing.indexAt(x, y));
            size_t o = (static_cast<size_t>(cy) * canvasW + cx) * 4;
            canvas[o + 0] = c.r;
            canvas[o + 1] = c.g;
            canvas[o + 2] = c.b;
            canvas[o + 3] = 255;
        }
    }
}

} // namespace comic
