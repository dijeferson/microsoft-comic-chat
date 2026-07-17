// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_compose.h — composite a pose's drawing (+ optional 1-bit mask) into an
// RGBA canvas. Alpha comes from the mask silhouette (opaque inside the figure);
// with no mask, non-background pixels are opaque. Pure C++.

#ifndef COMIC_COMPOSE_H
#define COMIC_COMPOSE_H

#include <vector>

#include "comic_dib.h"
#include "comic_types.h"

namespace comic {

// A composed, ready-to-upload body bitmap (top-down RGBA, width*height*4).
struct ComposedBody {
    std::vector<u8> rgba;
    int width = 0;
    int height = 0;
    bool valid() const { return width > 0 && height > 0 && !rgba.empty(); }
};

// Stamp `drawing` into `canvas` (canvasW x canvasH, top-down RGBA) with its
// top-left at (destX,destY). If `mask` is non-null, a pixel is opaque where the
// mask marks the silhouette (see maskInsideIsHigh) and transparent elsewhere;
// if null, a pixel is opaque unless it equals `bgIndex` in the drawing.
// `maskInsideIsHigh`: true if the higher palette index (1) is "inside" the
// figure in the mask. RGB is always taken from the drawing's palette.
void stampPart(std::vector<u8>& canvas, int canvasW, int canvasH,
               int destX, int destY,
               const Dib& drawing, const Dib* mask,
               bool maskInsideIsHigh, int bgIndex);

} // namespace comic

#endif // COMIC_COMPOSE_H
